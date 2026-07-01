
#include "qemu/osdep.h"
#include <zlib.h>

#include "block/block-io.h"
#include "qapi/error.h"
#include "qcow2.h"
#include "qemu/bswap.h"
#include "qemu/memalign.h"
#include "trace.h"

int coroutine_fn qcow2_shrink_l1_table(BlockDriverState *bs,
                                       uint64_t exact_size)
{
    BDRVQcow2State *s = bs->opaque;
    int new_l1_size, i, ret;

    if (exact_size >= s->l1_size) {
        return 0;
    }

    new_l1_size = exact_size;

#ifdef DEBUG_ALLOC2
    fprintf(stderr, "shrink l1_table from %d to %d\n", s->l1_size, new_l1_size);
#endif

    BLKDBG_CO_EVENT(bs->file, BLKDBG_L1_SHRINK_WRITE_TABLE);
    ret = bdrv_co_pwrite_zeroes(bs->file,
                                s->l1_table_offset + new_l1_size * L1E_SIZE,
                                (s->l1_size - new_l1_size) * L1E_SIZE, 0);
    if (ret < 0) {
        goto fail;
    }

    ret = bdrv_co_flush(bs->file->bs);
    if (ret < 0) {
        goto fail;
    }

    BLKDBG_CO_EVENT(bs->file, BLKDBG_L1_SHRINK_FREE_L2_CLUSTERS);
    for (i = s->l1_size - 1; i > new_l1_size - 1; i--) {
        if ((s->l1_table[i] & L1E_OFFSET_MASK) == 0) {
            continue;
        }
        qcow2_free_clusters(bs, s->l1_table[i] & L1E_OFFSET_MASK,
                            s->cluster_size, QCOW2_DISCARD_ALWAYS);
        s->l1_table[i] = 0;
    }
    return 0;

fail:
    memset(s->l1_table + new_l1_size, 0,
           (s->l1_size - new_l1_size) * L1E_SIZE);
    return ret;
}

int qcow2_grow_l1_table(BlockDriverState *bs, uint64_t min_size,
                        bool exact_size)
{
    BDRVQcow2State *s = bs->opaque;
    int new_l1_size2, ret, i;
    uint64_t *new_l1_table;
    int64_t old_l1_table_offset, old_l1_size;
    int64_t new_l1_table_offset, new_l1_size;
    uint8_t data[12];

    if (min_size <= s->l1_size)
        return 0;

    if (min_size > INT_MAX / L1E_SIZE) {
        return -EFBIG;
    }

    if (exact_size) {
        new_l1_size = min_size;
    } else {
        new_l1_size = s->l1_size;
        if (new_l1_size == 0) {
            new_l1_size = 1;
        }
        while (min_size > new_l1_size) {
            new_l1_size = DIV_ROUND_UP(new_l1_size * 3, 2);
        }
    }

    QEMU_BUILD_BUG_ON(QCOW_MAX_L1_SIZE > INT_MAX);
    if (new_l1_size > QCOW_MAX_L1_SIZE / L1E_SIZE) {
        return -EFBIG;
    }

#ifdef DEBUG_ALLOC2
    fprintf(stderr, "grow l1_table from %d to %" PRId64 "\n",
            s->l1_size, new_l1_size);
#endif

    new_l1_size2 = L1E_SIZE * new_l1_size;
    new_l1_table = qemu_try_blockalign(bs->file->bs, new_l1_size2);
    if (new_l1_table == NULL) {
        return -ENOMEM;
    }
    memset(new_l1_table, 0, new_l1_size2);

    if (s->l1_size) {
        memcpy(new_l1_table, s->l1_table, s->l1_size * L1E_SIZE);
    }

    BLKDBG_EVENT(bs->file, BLKDBG_L1_GROW_ALLOC_TABLE);
    new_l1_table_offset = qcow2_alloc_clusters(bs, new_l1_size2);
    if (new_l1_table_offset < 0) {
        qemu_vfree(new_l1_table);
        return new_l1_table_offset;
    }

    ret = qcow2_cache_flush(bs, s->refcount_block_cache);
    if (ret < 0) {
        goto fail;
    }

    ret = qcow2_pre_write_overlap_check(bs, 0, new_l1_table_offset,
                                        new_l1_size2, false);
    if (ret < 0) {
        goto fail;
    }

    BLKDBG_EVENT(bs->file, BLKDBG_L1_GROW_WRITE_TABLE);
    for(i = 0; i < s->l1_size; i++)
        new_l1_table[i] = cpu_to_be64(new_l1_table[i]);
    ret = bdrv_pwrite_sync(bs->file, new_l1_table_offset, new_l1_size2,
                           new_l1_table, 0);
    if (ret < 0)
        goto fail;
    for(i = 0; i < s->l1_size; i++)
        new_l1_table[i] = be64_to_cpu(new_l1_table[i]);

    BLKDBG_EVENT(bs->file, BLKDBG_L1_GROW_ACTIVATE_TABLE);
    stl_be_p(data, new_l1_size);
    stq_be_p(data + 4, new_l1_table_offset);
    ret = bdrv_pwrite_sync(bs->file, offsetof(QCowHeader, l1_size),
                           sizeof(data), data, 0);
    if (ret < 0) {
        goto fail;
    }
    qemu_vfree(s->l1_table);
    old_l1_table_offset = s->l1_table_offset;
    s->l1_table_offset = new_l1_table_offset;
    s->l1_table = new_l1_table;
    old_l1_size = s->l1_size;
    s->l1_size = new_l1_size;
    qcow2_free_clusters(bs, old_l1_table_offset, old_l1_size * L1E_SIZE,
                        QCOW2_DISCARD_OTHER);
    return 0;
 fail:
    qemu_vfree(new_l1_table);
    qcow2_free_clusters(bs, new_l1_table_offset, new_l1_size2,
                        QCOW2_DISCARD_OTHER);
    return ret;
}

static int GRAPH_RDLOCK
l2_load(BlockDriverState *bs, uint64_t offset,
        uint64_t l2_offset, uint64_t **l2_slice)
{
    BDRVQcow2State *s = bs->opaque;
    int start_of_slice = l2_entry_size(s) *
        (offset_to_l2_index(s, offset) - offset_to_l2_slice_index(s, offset));

    return qcow2_cache_get(bs, s->l2_table_cache, l2_offset + start_of_slice,
                           (void **)l2_slice);
}

int qcow2_write_l1_entry(BlockDriverState *bs, int l1_index)
{
    BDRVQcow2State *s = bs->opaque;
    int l1_start_index;
    int i, ret;
    int bufsize = MAX(L1E_SIZE,
                      MIN(bs->file->bs->bl.request_alignment, s->cluster_size));
    int nentries = bufsize / L1E_SIZE;
    g_autofree uint64_t *buf = g_try_new0(uint64_t, nentries);

    if (buf == NULL) {
        return -ENOMEM;
    }

    l1_start_index = QEMU_ALIGN_DOWN(l1_index, nentries);
    for (i = 0; i < MIN(nentries, s->l1_size - l1_start_index); i++) {
        buf[i] = cpu_to_be64(s->l1_table[l1_start_index + i]);
    }

    ret = qcow2_pre_write_overlap_check(bs, QCOW2_OL_ACTIVE_L1,
            s->l1_table_offset + L1E_SIZE * l1_start_index, bufsize, false);
    if (ret < 0) {
        return ret;
    }

    BLKDBG_EVENT(bs->file, BLKDBG_L1_UPDATE);
    ret = bdrv_pwrite_sync(bs->file,
                           s->l1_table_offset + L1E_SIZE * l1_start_index,
                           bufsize, buf, 0);
    if (ret < 0) {
        return ret;
    }

    return 0;
}


static int GRAPH_RDLOCK l2_allocate(BlockDriverState *bs, int l1_index)
{
    BDRVQcow2State *s = bs->opaque;
    uint64_t old_l2_offset;
    uint64_t *l2_slice = NULL;
    unsigned slice, slice_size2, n_slices;
    int64_t l2_offset;
    int ret;

    old_l2_offset = s->l1_table[l1_index];

    trace_qcow2_l2_allocate(bs, l1_index);


    l2_offset = qcow2_alloc_clusters(bs, s->l2_size * l2_entry_size(s));
    if (l2_offset < 0) {
        ret = l2_offset;
        goto fail;
    }

    assert((l2_offset & L1E_OFFSET_MASK) == l2_offset);

    if (l2_offset == 0) {
        qcow2_signal_corruption(bs, true, -1, -1, "Preventing invalid "
                                "allocation of L2 table at offset 0");
        ret = -EIO;
        goto fail;
    }

    ret = qcow2_cache_flush(bs, s->refcount_block_cache);
    if (ret < 0) {
        goto fail;
    }


    slice_size2 = s->l2_slice_size * l2_entry_size(s);
    n_slices = s->cluster_size / slice_size2;

    trace_qcow2_l2_allocate_get_empty(bs, l1_index);
    for (slice = 0; slice < n_slices; slice++) {
        ret = qcow2_cache_get_empty(bs, s->l2_table_cache,
                                    l2_offset + slice * slice_size2,
                                    (void **) &l2_slice);
        if (ret < 0) {
            goto fail;
        }

        if ((old_l2_offset & L1E_OFFSET_MASK) == 0) {
            memset(l2_slice, 0, slice_size2);
        } else {
            uint64_t *old_slice;
            uint64_t old_l2_slice_offset =
                (old_l2_offset & L1E_OFFSET_MASK) + slice * slice_size2;

            BLKDBG_EVENT(bs->file, BLKDBG_L2_ALLOC_COW_READ);
            ret = qcow2_cache_get(bs, s->l2_table_cache, old_l2_slice_offset,
                                  (void **) &old_slice);
            if (ret < 0) {
                goto fail;
            }

            memcpy(l2_slice, old_slice, slice_size2);

            qcow2_cache_put(s->l2_table_cache, (void **) &old_slice);
        }

        BLKDBG_EVENT(bs->file, BLKDBG_L2_ALLOC_WRITE);

        trace_qcow2_l2_allocate_write_l2(bs, l1_index);
        qcow2_cache_entry_mark_dirty(s->l2_table_cache, l2_slice);
        qcow2_cache_put(s->l2_table_cache, (void **) &l2_slice);
    }

    ret = qcow2_cache_flush(bs, s->l2_table_cache);
    if (ret < 0) {
        goto fail;
    }

    trace_qcow2_l2_allocate_write_l1(bs, l1_index);
    s->l1_table[l1_index] = l2_offset | QCOW_OFLAG_COPIED;
    ret = qcow2_write_l1_entry(bs, l1_index);
    if (ret < 0) {
        goto fail;
    }

    trace_qcow2_l2_allocate_done(bs, l1_index, 0);
    return 0;

fail:
    trace_qcow2_l2_allocate_done(bs, l1_index, ret);
    if (l2_slice != NULL) {
        qcow2_cache_put(s->l2_table_cache, (void **) &l2_slice);
    }
    s->l1_table[l1_index] = old_l2_offset;
    if (l2_offset > 0) {
        qcow2_free_clusters(bs, l2_offset, s->l2_size * l2_entry_size(s),
                            QCOW2_DISCARD_ALWAYS);
    }
    return ret;
}

static int GRAPH_RDLOCK
qcow2_get_subcluster_range_type(BlockDriverState *bs, uint64_t l2_entry,
                                uint64_t l2_bitmap, unsigned sc_from,
                                QCow2SubclusterType *type)
{
    BDRVQcow2State *s = bs->opaque;
    uint32_t val;

    *type = qcow2_get_subcluster_type(bs, l2_entry, l2_bitmap, sc_from);

    if (*type == QCOW2_SUBCLUSTER_INVALID) {
        return -EINVAL;
    } else if (!has_subclusters(s) || *type == QCOW2_SUBCLUSTER_COMPRESSED) {
        return s->subclusters_per_cluster - sc_from;
    }

    switch (*type) {
    case QCOW2_SUBCLUSTER_NORMAL:
        val = l2_bitmap | QCOW_OFLAG_SUB_ALLOC_RANGE(0, sc_from);
        return cto32(val) - sc_from;

    case QCOW2_SUBCLUSTER_ZERO_PLAIN:
    case QCOW2_SUBCLUSTER_ZERO_ALLOC:
        val = (l2_bitmap | QCOW_OFLAG_SUB_ZERO_RANGE(0, sc_from)) >> 32;
        return cto32(val) - sc_from;

    case QCOW2_SUBCLUSTER_UNALLOCATED_PLAIN:
    case QCOW2_SUBCLUSTER_UNALLOCATED_ALLOC:
        val = ((l2_bitmap >> 32) | l2_bitmap)
            & ~QCOW_OFLAG_SUB_ALLOC_RANGE(0, sc_from);
        return ctz32(val) - sc_from;

    default:
        g_assert_not_reached();
    }
}

static int GRAPH_RDLOCK
count_contiguous_subclusters(BlockDriverState *bs, int nb_clusters,
                             unsigned sc_index, uint64_t *l2_slice,
                             unsigned *l2_index)
{
    BDRVQcow2State *s = bs->opaque;
    int i, count = 0;
    bool check_offset = false;
    uint64_t expected_offset = 0;
    QCow2SubclusterType expected_type = QCOW2_SUBCLUSTER_NORMAL, type;

    assert(*l2_index + nb_clusters <= s->l2_slice_size);

    for (i = 0; i < nb_clusters; i++) {
        unsigned first_sc = (i == 0) ? sc_index : 0;
        uint64_t l2_entry = get_l2_entry(s, l2_slice, *l2_index + i);
        uint64_t l2_bitmap = get_l2_bitmap(s, l2_slice, *l2_index + i);
        int ret = qcow2_get_subcluster_range_type(bs, l2_entry, l2_bitmap,
                                                  first_sc, &type);
        if (ret < 0) {
            *l2_index += i; /* Point to the invalid entry */
            return -EIO;
        }
        if (i == 0) {
            if (type == QCOW2_SUBCLUSTER_COMPRESSED) {
                return ret;
            }
            expected_type = type;
            expected_offset = l2_entry & L2E_OFFSET_MASK;
            check_offset = (type == QCOW2_SUBCLUSTER_NORMAL ||
                            type == QCOW2_SUBCLUSTER_ZERO_ALLOC ||
                            type == QCOW2_SUBCLUSTER_UNALLOCATED_ALLOC);
        } else if (type != expected_type) {
            break;
        } else if (check_offset) {
            expected_offset += s->cluster_size;
            if (expected_offset != (l2_entry & L2E_OFFSET_MASK)) {
                break;
            }
        }
        count += ret;
        if (first_sc + ret < s->subclusters_per_cluster) {
            break;
        }
    }

    return count;
}

static int coroutine_fn GRAPH_RDLOCK
do_perform_cow_read(BlockDriverState *bs, uint64_t src_cluster_offset,
                    unsigned offset_in_cluster, QEMUIOVector *qiov)
{
    int ret;

    if (qiov->size == 0) {
        return 0;
    }

    BLKDBG_CO_EVENT(bs->file, BLKDBG_COW_READ);

    if (!bs->drv) {
        return -ENOMEDIUM;
    }

    assert(src_cluster_offset <= INT64_MAX);
    assert(src_cluster_offset + offset_in_cluster <= INT64_MAX);
    assert((uint64_t)qiov->size <= INT64_MAX);
    bdrv_check_qiov_request(src_cluster_offset + offset_in_cluster, qiov->size,
                            qiov, 0, &error_abort);
    ret = bs->drv->bdrv_co_preadv_part(bs,
                                       src_cluster_offset + offset_in_cluster,
                                       qiov->size, qiov, 0, 0);
    if (ret < 0) {
        return ret;
    }

    return 0;
}

static int coroutine_fn GRAPH_RDLOCK
do_perform_cow_write(BlockDriverState *bs, uint64_t cluster_offset,
                     unsigned offset_in_cluster, QEMUIOVector *qiov)
{
    BDRVQcow2State *s = bs->opaque;
    int ret;

    if (qiov->size == 0) {
        return 0;
    }

    ret = qcow2_pre_write_overlap_check(bs, 0,
            cluster_offset + offset_in_cluster, qiov->size, true);
    if (ret < 0) {
        return ret;
    }

    BLKDBG_CO_EVENT(bs->file, BLKDBG_COW_WRITE);
    ret = bdrv_co_pwritev(s->data_file, cluster_offset + offset_in_cluster,
                          qiov->size, qiov, 0);
    if (ret < 0) {
        return ret;
    }

    return 0;
}


int qcow2_get_host_offset(BlockDriverState *bs, uint64_t offset,
                          unsigned int *bytes, uint64_t *host_offset,
                          QCow2SubclusterType *subcluster_type)
{
    BDRVQcow2State *s = bs->opaque;
    unsigned int l2_index, sc_index;
    uint64_t l1_index, l2_offset, *l2_slice, l2_entry, l2_bitmap;
    int sc;
    unsigned int offset_in_cluster;
    uint64_t bytes_available, bytes_needed, nb_clusters;
    QCow2SubclusterType type;
    int ret;

    offset_in_cluster = offset_into_cluster(s, offset);
    bytes_needed = (uint64_t) *bytes + offset_in_cluster;

    bytes_available =
        ((uint64_t) (s->l2_slice_size - offset_to_l2_slice_index(s, offset)))
        << s->cluster_bits;

    if (bytes_needed > bytes_available) {
        bytes_needed = bytes_available;
    }

    *host_offset = 0;


    l1_index = offset_to_l1_index(s, offset);
    if (l1_index >= s->l1_size) {
        type = QCOW2_SUBCLUSTER_UNALLOCATED_PLAIN;
        goto out;
    }

    l2_offset = s->l1_table[l1_index] & L1E_OFFSET_MASK;
    if (!l2_offset) {
        type = QCOW2_SUBCLUSTER_UNALLOCATED_PLAIN;
        goto out;
    }

    if (offset_into_cluster(s, l2_offset)) {
        qcow2_signal_corruption(bs, true, -1, -1, "L2 table offset %#" PRIx64
                                " unaligned (L1 index: %#" PRIx64 ")",
                                l2_offset, l1_index);
        return -EIO;
    }


    ret = l2_load(bs, offset, l2_offset, &l2_slice);
    if (ret < 0) {
        return ret;
    }


    l2_index = offset_to_l2_slice_index(s, offset);
    sc_index = offset_to_sc_index(s, offset);
    l2_entry = get_l2_entry(s, l2_slice, l2_index);
    l2_bitmap = get_l2_bitmap(s, l2_slice, l2_index);

    nb_clusters = size_to_clusters(s, bytes_needed);
    assert(nb_clusters <= INT_MAX);

    type = qcow2_get_subcluster_type(bs, l2_entry, l2_bitmap, sc_index);
    if (s->qcow_version < 3 && (type == QCOW2_SUBCLUSTER_ZERO_PLAIN ||
                                type == QCOW2_SUBCLUSTER_ZERO_ALLOC)) {
        qcow2_signal_corruption(bs, true, -1, -1, "Zero cluster entry found"
                                " in pre-v3 image (L2 offset: %#" PRIx64
                                ", L2 index: %#x)", l2_offset, l2_index);
        ret = -EIO;
        goto fail;
    }
    switch (type) {
    case QCOW2_SUBCLUSTER_INVALID:
        break; /* This is handled by count_contiguous_subclusters() below */
    case QCOW2_SUBCLUSTER_COMPRESSED:
        if (has_data_file(bs)) {
            qcow2_signal_corruption(bs, true, -1, -1, "Compressed cluster "
                                    "entry found in image with external data "
                                    "file (L2 offset: %#" PRIx64 ", L2 index: "
                                    "%#x)", l2_offset, l2_index);
            ret = -EIO;
            goto fail;
        }
        *host_offset = l2_entry;
        break;
    case QCOW2_SUBCLUSTER_ZERO_PLAIN:
    case QCOW2_SUBCLUSTER_UNALLOCATED_PLAIN:
        break;
    case QCOW2_SUBCLUSTER_ZERO_ALLOC:
    case QCOW2_SUBCLUSTER_NORMAL:
    case QCOW2_SUBCLUSTER_UNALLOCATED_ALLOC: {
        uint64_t host_cluster_offset = l2_entry & L2E_OFFSET_MASK;
        *host_offset = host_cluster_offset + offset_in_cluster;
        if (offset_into_cluster(s, host_cluster_offset)) {
            qcow2_signal_corruption(bs, true, -1, -1,
                                    "Cluster allocation offset %#"
                                    PRIx64 " unaligned (L2 offset: %#" PRIx64
                                    ", L2 index: %#x)", host_cluster_offset,
                                    l2_offset, l2_index);
            ret = -EIO;
            goto fail;
        }
        if (has_data_file(bs) && *host_offset != offset) {
            qcow2_signal_corruption(bs, true, -1, -1,
                                    "External data file host cluster offset %#"
                                    PRIx64 " does not match guest cluster "
                                    "offset: %#" PRIx64
                                    ", L2 index: %#x)", host_cluster_offset,
                                    offset - offset_in_cluster, l2_index);
            ret = -EIO;
            goto fail;
        }
        break;
    }
    default:
        abort();
    }

    sc = count_contiguous_subclusters(bs, nb_clusters, sc_index,
                                      l2_slice, &l2_index);
    if (sc < 0) {
        qcow2_signal_corruption(bs, true, -1, -1, "Invalid cluster entry found "
                                " (L2 offset: %#" PRIx64 ", L2 index: %#x)",
                                l2_offset, l2_index);
        ret = -EIO;
        goto fail;
    }
    qcow2_cache_put(s->l2_table_cache, (void **) &l2_slice);

    bytes_available = ((int64_t)sc + sc_index) << s->subcluster_bits;

out:
    if (bytes_available > bytes_needed) {
        bytes_available = bytes_needed;
    }

    assert(bytes_available - offset_in_cluster <= UINT_MAX);
    *bytes = bytes_available - offset_in_cluster;

    *subcluster_type = type;

    return 0;

fail:
    qcow2_cache_put(s->l2_table_cache, (void **)&l2_slice);
    return ret;
}

static int GRAPH_RDLOCK
get_cluster_table(BlockDriverState *bs, uint64_t offset,
                  uint64_t **new_l2_slice, int *new_l2_index)
{
    BDRVQcow2State *s = bs->opaque;
    unsigned int l2_index;
    uint64_t l1_index, l2_offset;
    uint64_t *l2_slice = NULL;
    int ret;


    l1_index = offset_to_l1_index(s, offset);
    if (l1_index >= s->l1_size) {
        ret = qcow2_grow_l1_table(bs, l1_index + 1, false);
        if (ret < 0) {
            return ret;
        }
    }

    assert(l1_index < s->l1_size);
    l2_offset = s->l1_table[l1_index] & L1E_OFFSET_MASK;
    if (offset_into_cluster(s, l2_offset)) {
        qcow2_signal_corruption(bs, true, -1, -1, "L2 table offset %#" PRIx64
                                " unaligned (L1 index: %#" PRIx64 ")",
                                l2_offset, l1_index);
        return -EIO;
    }

    if (!(s->l1_table[l1_index] & QCOW_OFLAG_COPIED)) {
        ret = l2_allocate(bs, l1_index);
        if (ret < 0) {
            return ret;
        }

        if (l2_offset) {
            qcow2_free_clusters(bs, l2_offset, s->l2_size * l2_entry_size(s),
                                QCOW2_DISCARD_OTHER);
        }

        l2_offset = s->l1_table[l1_index] & L1E_OFFSET_MASK;
        assert(offset_into_cluster(s, l2_offset) == 0);
    }

    ret = l2_load(bs, offset, l2_offset, &l2_slice);
    if (ret < 0) {
        return ret;
    }


    l2_index = offset_to_l2_slice_index(s, offset);

    *new_l2_slice = l2_slice;
    *new_l2_index = l2_index;

    return 0;
}

int coroutine_fn GRAPH_RDLOCK
qcow2_alloc_compressed_cluster_offset(BlockDriverState *bs, uint64_t offset,
                                      int compressed_size, uint64_t *host_offset)
{
    BDRVQcow2State *s = bs->opaque;
    int l2_index, ret;
    uint64_t *l2_slice;
    int64_t cluster_offset;
    int nb_csectors;

    if (has_data_file(bs)) {
        return 0;
    }

    ret = get_cluster_table(bs, offset, &l2_slice, &l2_index);
    if (ret < 0) {
        return ret;
    }

    cluster_offset = get_l2_entry(s, l2_slice, l2_index);
    if (cluster_offset & L2E_OFFSET_MASK) {
        qcow2_cache_put(s->l2_table_cache, (void **) &l2_slice);
        return -EIO;
    }

    cluster_offset = qcow2_alloc_bytes(bs, compressed_size);
    if (cluster_offset < 0) {
        qcow2_cache_put(s->l2_table_cache, (void **) &l2_slice);
        return cluster_offset;
    }

    nb_csectors =
        (cluster_offset + compressed_size - 1) / QCOW2_COMPRESSED_SECTOR_SIZE -
        (cluster_offset / QCOW2_COMPRESSED_SECTOR_SIZE);

    assert((cluster_offset & s->cluster_offset_mask) == cluster_offset);
    assert((nb_csectors & s->csize_mask) == nb_csectors);

    cluster_offset |= QCOW_OFLAG_COMPRESSED |
                      ((uint64_t)nb_csectors << s->csize_shift);



    BLKDBG_CO_EVENT(bs->file, BLKDBG_L2_UPDATE_COMPRESSED);
    qcow2_cache_entry_mark_dirty(s->l2_table_cache, l2_slice);
    set_l2_entry(s, l2_slice, l2_index, cluster_offset);
    if (has_subclusters(s)) {
        set_l2_bitmap(s, l2_slice, l2_index, 0);
    }
    qcow2_cache_put(s->l2_table_cache, (void **) &l2_slice);

    *host_offset = cluster_offset & s->cluster_offset_mask;
    return 0;
}

static int coroutine_fn GRAPH_RDLOCK
perform_cow(BlockDriverState *bs, QCowL2Meta *m)
{
    BDRVQcow2State *s = bs->opaque;
    Qcow2COWRegion *start = &m->cow_start;
    Qcow2COWRegion *end = &m->cow_end;
    unsigned buffer_size;
    unsigned data_bytes = end->offset - (start->offset + start->nb_bytes);
    bool merge_reads;
    uint8_t *start_buffer, *end_buffer;
    QEMUIOVector qiov;
    int ret;

    assert(start->nb_bytes <= UINT_MAX - end->nb_bytes);
    assert(start->nb_bytes + end->nb_bytes <= UINT_MAX - data_bytes);
    assert(start->offset + start->nb_bytes <= end->offset);

    if ((start->nb_bytes == 0 && end->nb_bytes == 0) || m->skip_cow) {
        return 0;
    }

    merge_reads = start->nb_bytes && end->nb_bytes && data_bytes <= 16384;
    if (merge_reads) {
        buffer_size = start->nb_bytes + data_bytes + end->nb_bytes;
    } else {
        size_t align = bdrv_opt_mem_align(bs);
        assert(align > 0 && align <= UINT_MAX);
        assert(QEMU_ALIGN_UP(start->nb_bytes, align) <=
               UINT_MAX - end->nb_bytes);
        buffer_size = QEMU_ALIGN_UP(start->nb_bytes, align) + end->nb_bytes;
    }

    start_buffer = qemu_try_blockalign(bs, buffer_size);
    if (start_buffer == NULL) {
        return -ENOMEM;
    }
    end_buffer = start_buffer + buffer_size - end->nb_bytes;

    qemu_iovec_init(&qiov, 2 + (m->data_qiov ?
                                qemu_iovec_subvec_niov(m->data_qiov,
                                                       m->data_qiov_offset,
                                                       data_bytes)
                                : 0));

    qemu_co_mutex_unlock(&s->lock);
    if (merge_reads) {
        qemu_iovec_add(&qiov, start_buffer, buffer_size);
        ret = do_perform_cow_read(bs, m->offset, start->offset, &qiov);
    } else {
        qemu_iovec_add(&qiov, start_buffer, start->nb_bytes);
        ret = do_perform_cow_read(bs, m->offset, start->offset, &qiov);
        if (ret < 0) {
            goto fail;
        }

        qemu_iovec_reset(&qiov);
        qemu_iovec_add(&qiov, end_buffer, end->nb_bytes);
        ret = do_perform_cow_read(bs, m->offset, end->offset, &qiov);
    }
    if (ret < 0) {
        goto fail;
    }

    if (m->data_qiov) {
        qemu_iovec_reset(&qiov);
        if (start->nb_bytes) {
            qemu_iovec_add(&qiov, start_buffer, start->nb_bytes);
        }
        qemu_iovec_concat(&qiov, m->data_qiov, m->data_qiov_offset, data_bytes);
        if (end->nb_bytes) {
            qemu_iovec_add(&qiov, end_buffer, end->nb_bytes);
        }
        BLKDBG_CO_EVENT(bs->file, BLKDBG_WRITE_AIO);
        ret = do_perform_cow_write(bs, m->alloc_offset, start->offset, &qiov);
    } else {
        qemu_iovec_reset(&qiov);
        qemu_iovec_add(&qiov, start_buffer, start->nb_bytes);
        ret = do_perform_cow_write(bs, m->alloc_offset, start->offset, &qiov);
        if (ret < 0) {
            goto fail;
        }

        qemu_iovec_reset(&qiov);
        qemu_iovec_add(&qiov, end_buffer, end->nb_bytes);
        ret = do_perform_cow_write(bs, m->alloc_offset, end->offset, &qiov);
    }

fail:
    qemu_co_mutex_lock(&s->lock);

    if (ret == 0) {
        qcow2_cache_depends_on_flush(s->l2_table_cache);
    }

    qemu_vfree(start_buffer);
    qemu_iovec_destroy(&qiov);
    return ret;
}

int coroutine_fn qcow2_alloc_cluster_link_l2(BlockDriverState *bs,
                                             QCowL2Meta *m)
{
    BDRVQcow2State *s = bs->opaque;
    int i, j = 0, l2_index, ret;
    uint64_t *old_cluster, *l2_slice;
    uint64_t cluster_offset = m->alloc_offset;

    trace_qcow2_cluster_link_l2(qemu_coroutine_self(), m->nb_clusters);
    assert(m->nb_clusters > 0);

    old_cluster = g_try_new(uint64_t, m->nb_clusters);
    if (old_cluster == NULL) {
        ret = -ENOMEM;
        goto err;
    }

    ret = perform_cow(bs, m);
    if (ret < 0) {
        goto err;
    }

    if (s->use_lazy_refcounts) {
        qcow2_mark_dirty(bs);
    }
    if (qcow2_need_accurate_refcounts(s)) {
        qcow2_cache_set_dependency(bs, s->l2_table_cache,
                                   s->refcount_block_cache);
    }

    ret = get_cluster_table(bs, m->offset, &l2_slice, &l2_index);
    if (ret < 0) {
        goto err;
    }
    qcow2_cache_entry_mark_dirty(s->l2_table_cache, l2_slice);

    assert(l2_index + m->nb_clusters <= s->l2_slice_size);
    assert(m->cow_end.offset + m->cow_end.nb_bytes <=
           m->nb_clusters << s->cluster_bits);
    for (i = 0; i < m->nb_clusters; i++) {
        uint64_t offset = cluster_offset + ((uint64_t)i << s->cluster_bits);
        if (get_l2_entry(s, l2_slice, l2_index + i) != 0) {
            old_cluster[j++] = get_l2_entry(s, l2_slice, l2_index + i);
        }

        assert((offset & L2E_OFFSET_MASK) == offset);

        set_l2_entry(s, l2_slice, l2_index + i, offset | QCOW_OFLAG_COPIED);

        if (has_subclusters(s) && !m->prealloc) {
            uint64_t l2_bitmap = get_l2_bitmap(s, l2_slice, l2_index + i);
            unsigned written_from = m->cow_start.offset;
            unsigned written_to = m->cow_end.offset + m->cow_end.nb_bytes;
            int first_sc, last_sc;
            written_from = MAX(written_from, i << s->cluster_bits);
            written_to   = MIN(written_to, (i + 1) << s->cluster_bits);
            assert(written_from < written_to);
            first_sc = offset_to_sc_index(s, written_from);
            last_sc  = offset_to_sc_index(s, written_to - 1);
            l2_bitmap |= QCOW_OFLAG_SUB_ALLOC_RANGE(first_sc, last_sc + 1);
            l2_bitmap &= ~QCOW_OFLAG_SUB_ZERO_RANGE(first_sc, last_sc + 1);
            set_l2_bitmap(s, l2_slice, l2_index + i, l2_bitmap);
        }
     }


    qcow2_cache_put(s->l2_table_cache, (void **) &l2_slice);

    if (!m->keep_old_clusters && j != 0) {
        for (i = 0; i < j; i++) {
            qcow2_free_any_cluster(bs, old_cluster[i], QCOW2_DISCARD_NEVER);
        }
    }

    ret = 0;
err:
    g_free(old_cluster);
    return ret;
 }

void coroutine_fn qcow2_alloc_cluster_abort(BlockDriverState *bs, QCowL2Meta *m)
{
    BDRVQcow2State *s = bs->opaque;
    if (!has_data_file(bs) && !m->keep_old_clusters) {
        qcow2_free_clusters(bs, m->alloc_offset,
                            m->nb_clusters << s->cluster_bits,
                            QCOW2_DISCARD_NEVER);
    }
}

static int coroutine_fn GRAPH_RDLOCK
calculate_l2_meta(BlockDriverState *bs, uint64_t host_cluster_offset,
                  uint64_t guest_offset, unsigned bytes, uint64_t *l2_slice,
                  QCowL2Meta **m, bool keep_old)
{
    BDRVQcow2State *s = bs->opaque;
    int sc_index, l2_index = offset_to_l2_slice_index(s, guest_offset);
    uint64_t l2_entry, l2_bitmap;
    unsigned cow_start_from, cow_end_to;
    unsigned cow_start_to = offset_into_cluster(s, guest_offset);
    unsigned cow_end_from = cow_start_to + bytes;
    unsigned nb_clusters = size_to_clusters(s, cow_end_from);
    QCowL2Meta *old_m = *m;
    QCow2SubclusterType type;
    int i;
    bool skip_cow = keep_old;

    assert(nb_clusters <= s->l2_slice_size - l2_index);

    for (i = 0; i < nb_clusters; i++) {
        l2_entry = get_l2_entry(s, l2_slice, l2_index + i);
        l2_bitmap = get_l2_bitmap(s, l2_slice, l2_index + i);
        if (skip_cow) {
            unsigned write_from = MAX(cow_start_to, i << s->cluster_bits);
            unsigned write_to = MIN(cow_end_from, (i + 1) << s->cluster_bits);
            int first_sc = offset_to_sc_index(s, write_from);
            int last_sc = offset_to_sc_index(s, write_to - 1);
            int cnt = qcow2_get_subcluster_range_type(bs, l2_entry, l2_bitmap,
                                                      first_sc, &type);
            if (type != QCOW2_SUBCLUSTER_NORMAL || first_sc + cnt <= last_sc) {
                skip_cow = false;
            }
        } else {
            type = qcow2_get_subcluster_type(bs, l2_entry, l2_bitmap, 0);
        }
        if (type == QCOW2_SUBCLUSTER_INVALID) {
            int l1_index = offset_to_l1_index(s, guest_offset);
            uint64_t l2_offset = s->l1_table[l1_index] & L1E_OFFSET_MASK;
            qcow2_signal_corruption(bs, true, -1, -1, "Invalid cluster "
                                    "entry found (L2 offset: %#" PRIx64
                                    ", L2 index: %#x)",
                                    l2_offset, l2_index + i);
            return -EIO;
        }
    }

    if (skip_cow) {
        return 0;
    }

    l2_entry = get_l2_entry(s, l2_slice, l2_index);
    l2_bitmap = get_l2_bitmap(s, l2_slice, l2_index);
    sc_index = offset_to_sc_index(s, guest_offset);
    type = qcow2_get_subcluster_type(bs, l2_entry, l2_bitmap, sc_index);

    if (!keep_old) {
        switch (type) {
        case QCOW2_SUBCLUSTER_COMPRESSED:
            cow_start_from = 0;
            break;
        case QCOW2_SUBCLUSTER_NORMAL:
        case QCOW2_SUBCLUSTER_ZERO_ALLOC:
        case QCOW2_SUBCLUSTER_UNALLOCATED_ALLOC:
            if (has_subclusters(s)) {
                uint32_t alloc_bitmap = l2_bitmap & QCOW_L2_BITMAP_ALL_ALLOC;
                cow_start_from =
                    MIN(sc_index, ctz32(alloc_bitmap)) << s->subcluster_bits;
            } else {
                cow_start_from = 0;
            }
            break;
        case QCOW2_SUBCLUSTER_ZERO_PLAIN:
        case QCOW2_SUBCLUSTER_UNALLOCATED_PLAIN:
            cow_start_from = sc_index << s->subcluster_bits;
            break;
        default:
            g_assert_not_reached();
        }
    } else {
        switch (type) {
        case QCOW2_SUBCLUSTER_NORMAL:
            cow_start_from = cow_start_to;
            break;
        case QCOW2_SUBCLUSTER_ZERO_ALLOC:
        case QCOW2_SUBCLUSTER_UNALLOCATED_ALLOC:
            cow_start_from = sc_index << s->subcluster_bits;
            break;
        default:
            g_assert_not_reached();
        }
    }

    l2_index += nb_clusters - 1;
    l2_entry = get_l2_entry(s, l2_slice, l2_index);
    l2_bitmap = get_l2_bitmap(s, l2_slice, l2_index);
    sc_index = offset_to_sc_index(s, guest_offset + bytes - 1);
    type = qcow2_get_subcluster_type(bs, l2_entry, l2_bitmap, sc_index);

    if (!keep_old) {
        switch (type) {
        case QCOW2_SUBCLUSTER_COMPRESSED:
            cow_end_to = ROUND_UP(cow_end_from, s->cluster_size);
            break;
        case QCOW2_SUBCLUSTER_NORMAL:
        case QCOW2_SUBCLUSTER_ZERO_ALLOC:
        case QCOW2_SUBCLUSTER_UNALLOCATED_ALLOC:
            cow_end_to = ROUND_UP(cow_end_from, s->cluster_size);
            if (has_subclusters(s)) {
                uint32_t alloc_bitmap = l2_bitmap & QCOW_L2_BITMAP_ALL_ALLOC;
                cow_end_to -=
                    MIN(s->subclusters_per_cluster - sc_index - 1,
                        clz32(alloc_bitmap)) << s->subcluster_bits;
            }
            break;
        case QCOW2_SUBCLUSTER_ZERO_PLAIN:
        case QCOW2_SUBCLUSTER_UNALLOCATED_PLAIN:
            cow_end_to = ROUND_UP(cow_end_from, s->subcluster_size);
            break;
        default:
            g_assert_not_reached();
        }
    } else {
        switch (type) {
        case QCOW2_SUBCLUSTER_NORMAL:
            cow_end_to = cow_end_from;
            break;
        case QCOW2_SUBCLUSTER_ZERO_ALLOC:
        case QCOW2_SUBCLUSTER_UNALLOCATED_ALLOC:
            cow_end_to = ROUND_UP(cow_end_from, s->subcluster_size);
            break;
        default:
            g_assert_not_reached();
        }
    }

    *m = g_malloc0(sizeof(**m));
    **m = (QCowL2Meta) {
        .next           = old_m,

        .alloc_offset   = host_cluster_offset,
        .offset         = start_of_cluster(s, guest_offset),
        .nb_clusters    = nb_clusters,

        .keep_old_clusters = keep_old,

        .cow_start = {
            .offset     = cow_start_from,
            .nb_bytes   = cow_start_to - cow_start_from,
        },
        .cow_end = {
            .offset     = cow_end_from,
            .nb_bytes   = cow_end_to - cow_end_from,
        },
    };

    qemu_co_queue_init(&(*m)->dependent_requests);
    QLIST_INSERT_HEAD(&s->cluster_allocs, *m, next_in_flight);

    return 0;
}

static bool GRAPH_RDLOCK
cluster_needs_new_alloc(BlockDriverState *bs, uint64_t l2_entry)
{
    switch (qcow2_get_cluster_type(bs, l2_entry)) {
    case QCOW2_CLUSTER_NORMAL:
    case QCOW2_CLUSTER_ZERO_ALLOC:
        if (l2_entry & QCOW_OFLAG_COPIED) {
            return false;
        }
    case QCOW2_CLUSTER_UNALLOCATED:
    case QCOW2_CLUSTER_COMPRESSED:
    case QCOW2_CLUSTER_ZERO_PLAIN:
        return true;
    default:
        abort();
    }
}

static int GRAPH_RDLOCK
count_single_write_clusters(BlockDriverState *bs, int nb_clusters,
                            uint64_t *l2_slice, int l2_index, bool new_alloc)
{
    BDRVQcow2State *s = bs->opaque;
    uint64_t l2_entry = get_l2_entry(s, l2_slice, l2_index);
    uint64_t expected_offset = l2_entry & L2E_OFFSET_MASK;
    int i;

    for (i = 0; i < nb_clusters; i++) {
        l2_entry = get_l2_entry(s, l2_slice, l2_index + i);
        if (cluster_needs_new_alloc(bs, l2_entry) != new_alloc) {
            break;
        }
        if (!new_alloc) {
            if (expected_offset != (l2_entry & L2E_OFFSET_MASK)) {
                break;
            }
            expected_offset += s->cluster_size;
        }
    }

    assert(i <= nb_clusters);
    return i;
}

static int coroutine_fn handle_dependencies(BlockDriverState *bs,
                                            uint64_t guest_offset,
                                            uint64_t *cur_bytes, QCowL2Meta **m)
{
    BDRVQcow2State *s = bs->opaque;
    QCowL2Meta *old_alloc;
    uint64_t bytes = *cur_bytes;

    QLIST_FOREACH(old_alloc, &s->cluster_allocs, next_in_flight) {

        uint64_t start = guest_offset;
        uint64_t end = start + bytes;
        uint64_t old_start = start_of_cluster(s, l2meta_cow_start(old_alloc));
        uint64_t old_end = ROUND_UP(l2meta_cow_end(old_alloc), s->cluster_size);

        if (end <= old_start || start >= old_end) {
            continue;
        }

        if (old_alloc->keep_old_clusters &&
            (end <= l2meta_cow_start(old_alloc) ||
             start >= l2meta_cow_end(old_alloc)))
        {
            continue;
        }


        if (start < old_start) {
            bytes = old_start - start;
        } else {
            bytes = 0;
        }

        if (bytes == 0 && *m) {
            *cur_bytes = 0;
            return 0;
        }

        if (bytes == 0) {
            qemu_co_queue_wait(&old_alloc->dependent_requests, &s->lock);
            return -EAGAIN;
        }
    }

    *cur_bytes = bytes;

    return 0;
}

static int coroutine_fn GRAPH_RDLOCK
handle_copied(BlockDriverState *bs, uint64_t guest_offset,
              uint64_t *host_offset, uint64_t *bytes, QCowL2Meta **m)
{
    BDRVQcow2State *s = bs->opaque;
    int l2_index;
    uint64_t l2_entry, cluster_offset;
    uint64_t *l2_slice;
    uint64_t nb_clusters;
    unsigned int keep_clusters;
    int ret;

    trace_qcow2_handle_copied(qemu_coroutine_self(), guest_offset, *host_offset,
                              *bytes);

    assert(*host_offset == INV_OFFSET || offset_into_cluster(s, guest_offset)
                                      == offset_into_cluster(s, *host_offset));

    nb_clusters =
        size_to_clusters(s, offset_into_cluster(s, guest_offset) + *bytes);

    l2_index = offset_to_l2_slice_index(s, guest_offset);
    nb_clusters = MIN(nb_clusters, s->l2_slice_size - l2_index);
    nb_clusters = MIN(nb_clusters, BDRV_REQUEST_MAX_BYTES >> s->cluster_bits);

    ret = get_cluster_table(bs, guest_offset, &l2_slice, &l2_index);
    if (ret < 0) {
        return ret;
    }

    l2_entry = get_l2_entry(s, l2_slice, l2_index);
    cluster_offset = l2_entry & L2E_OFFSET_MASK;

    if (!cluster_needs_new_alloc(bs, l2_entry)) {
        if (offset_into_cluster(s, cluster_offset)) {
            qcow2_signal_corruption(bs, true, -1, -1, "%s cluster offset "
                                    "%#" PRIx64 " unaligned (guest offset: %#"
                                    PRIx64 ")", l2_entry & QCOW_OFLAG_ZERO ?
                                    "Preallocated zero" : "Data",
                                    cluster_offset, guest_offset);
            ret = -EIO;
            goto out;
        }

        if (*host_offset != INV_OFFSET && cluster_offset != *host_offset) {
            *bytes = 0;
            ret = 0;
            goto out;
        }

        keep_clusters = count_single_write_clusters(bs, nb_clusters, l2_slice,
                                                    l2_index, false);
        assert(keep_clusters <= nb_clusters);

        *bytes = MIN(*bytes,
                 keep_clusters * s->cluster_size
                 - offset_into_cluster(s, guest_offset));
        assert(*bytes != 0);

        ret = calculate_l2_meta(bs, cluster_offset, guest_offset,
                                *bytes, l2_slice, m, true);
        if (ret < 0) {
            goto out;
        }

        ret = 1;
    } else {
        ret = 0;
    }

out:
    qcow2_cache_put(s->l2_table_cache, (void **) &l2_slice);

    if (ret > 0) {
        *host_offset = cluster_offset + offset_into_cluster(s, guest_offset);
    }

    return ret;
}

static int coroutine_fn GRAPH_RDLOCK
do_alloc_cluster_offset(BlockDriverState *bs, uint64_t guest_offset,
                        uint64_t *host_offset, uint64_t *nb_clusters)
{
    BDRVQcow2State *s = bs->opaque;

    trace_qcow2_do_alloc_clusters_offset(qemu_coroutine_self(), guest_offset,
                                         *host_offset, *nb_clusters);

    if (has_data_file(bs)) {
        assert(*host_offset == INV_OFFSET ||
               *host_offset == start_of_cluster(s, guest_offset));
        *host_offset = start_of_cluster(s, guest_offset);
        return 0;
    }

    trace_qcow2_cluster_alloc_phys(qemu_coroutine_self());
    if (*host_offset == INV_OFFSET) {
        int64_t cluster_offset =
            qcow2_alloc_clusters(bs, *nb_clusters * s->cluster_size);
        if (cluster_offset < 0) {
            return cluster_offset;
        }
        *host_offset = cluster_offset;
        return 0;
    } else {
        int64_t ret = qcow2_alloc_clusters_at(bs, *host_offset, *nb_clusters);
        if (ret < 0) {
            return ret;
        }
        *nb_clusters = ret;
        return 0;
    }
}

static int coroutine_fn GRAPH_RDLOCK
handle_alloc(BlockDriverState *bs, uint64_t guest_offset,
             uint64_t *host_offset, uint64_t *bytes, QCowL2Meta **m)
{
    BDRVQcow2State *s = bs->opaque;
    int l2_index;
    uint64_t *l2_slice;
    uint64_t nb_clusters;
    int ret;

    uint64_t alloc_cluster_offset;

    trace_qcow2_handle_alloc(qemu_coroutine_self(), guest_offset, *host_offset,
                             *bytes);
    assert(*bytes > 0);

    nb_clusters =
        size_to_clusters(s, offset_into_cluster(s, guest_offset) + *bytes);

    l2_index = offset_to_l2_slice_index(s, guest_offset);
    nb_clusters = MIN(nb_clusters, s->l2_slice_size - l2_index);
    nb_clusters = MIN(nb_clusters, BDRV_REQUEST_MAX_BYTES >> s->cluster_bits);

    ret = get_cluster_table(bs, guest_offset, &l2_slice, &l2_index);
    if (ret < 0) {
        return ret;
    }

    nb_clusters = count_single_write_clusters(bs, nb_clusters,
                                              l2_slice, l2_index, true);

    assert(nb_clusters > 0);

    alloc_cluster_offset = *host_offset == INV_OFFSET ? INV_OFFSET :
        start_of_cluster(s, *host_offset);
    ret = do_alloc_cluster_offset(bs, guest_offset, &alloc_cluster_offset,
                                  &nb_clusters);
    if (ret < 0) {
        goto out;
    }

    if (nb_clusters == 0) {
        *bytes = 0;
        ret = 0;
        goto out;
    }

    assert(alloc_cluster_offset != INV_OFFSET);

    uint64_t requested_bytes = *bytes + offset_into_cluster(s, guest_offset);
    int avail_bytes = nb_clusters << s->cluster_bits;
    int nb_bytes = MIN(requested_bytes, avail_bytes);

    *host_offset = alloc_cluster_offset + offset_into_cluster(s, guest_offset);
    *bytes = MIN(*bytes, nb_bytes - offset_into_cluster(s, guest_offset));
    assert(*bytes != 0);

    ret = calculate_l2_meta(bs, alloc_cluster_offset, guest_offset, *bytes,
                            l2_slice, m, false);
    if (ret < 0) {
        goto out;
    }

    ret = 1;

out:
    qcow2_cache_put(s->l2_table_cache, (void **) &l2_slice);
    return ret;
}

int coroutine_fn qcow2_alloc_host_offset(BlockDriverState *bs, uint64_t offset,
                                         unsigned int *bytes,
                                         uint64_t *host_offset,
                                         QCowL2Meta **m)
{
    BDRVQcow2State *s = bs->opaque;
    uint64_t start, remaining;
    uint64_t cluster_offset;
    uint64_t cur_bytes;
    int ret;

    trace_qcow2_alloc_clusters_offset(qemu_coroutine_self(), offset, *bytes);

again:
    start = offset;
    remaining = *bytes;
    cluster_offset = INV_OFFSET;
    *host_offset = INV_OFFSET;
    cur_bytes = 0;
    *m = NULL;

    while (true) {

        if (*host_offset == INV_OFFSET && cluster_offset != INV_OFFSET) {
            *host_offset = cluster_offset;
        }

        assert(remaining >= cur_bytes);

        start           += cur_bytes;
        remaining       -= cur_bytes;

        if (cluster_offset != INV_OFFSET) {
            cluster_offset += cur_bytes;
        }

        if (remaining == 0) {
            break;
        }

        cur_bytes = remaining;

        ret = handle_dependencies(bs, start, &cur_bytes, m);
        if (ret == -EAGAIN) {
            assert(*m == NULL);
            goto again;
        } else if (ret < 0) {
            return ret;
        } else if (cur_bytes == 0) {
            break;
        } else {
        }

        ret = handle_copied(bs, start, &cluster_offset, &cur_bytes, m);
        if (ret < 0) {
            return ret;
        } else if (ret) {
            continue;
        } else if (cur_bytes == 0) {
            break;
        }

        ret = handle_alloc(bs, start, &cluster_offset, &cur_bytes, m);
        if (ret < 0) {
            return ret;
        } else if (ret) {
            continue;
        } else {
            assert(cur_bytes == 0);
            break;
        }
    }

    *bytes -= remaining;
    assert(*bytes > 0);
    assert(*host_offset != INV_OFFSET);
    assert(offset_into_cluster(s, *host_offset) ==
           offset_into_cluster(s, offset));

    return 0;
}

static int GRAPH_RDLOCK
discard_in_l2_slice(BlockDriverState *bs, uint64_t offset, uint64_t nb_clusters,
                    enum qcow2_discard_type type, bool full_discard)
{
    BDRVQcow2State *s = bs->opaque;
    uint64_t *l2_slice;
    int l2_index;
    int ret;
    int i;

    ret = get_cluster_table(bs, offset, &l2_slice, &l2_index);
    if (ret < 0) {
        return ret;
    }

    nb_clusters = MIN(nb_clusters, s->l2_slice_size - l2_index);
    assert(nb_clusters <= INT_MAX);

    for (i = 0; i < nb_clusters; i++) {
        uint64_t old_l2_entry = get_l2_entry(s, l2_slice, l2_index + i);
        uint64_t old_l2_bitmap = get_l2_bitmap(s, l2_slice, l2_index + i);
        uint64_t new_l2_entry = old_l2_entry;
        uint64_t new_l2_bitmap = old_l2_bitmap;
        QCow2ClusterType cluster_type =
            qcow2_get_cluster_type(bs, old_l2_entry);
        bool keep_reference = (cluster_type != QCOW2_CLUSTER_COMPRESSED) &&
                              !full_discard &&
                              (s->discard_no_unref &&
                               type == QCOW2_DISCARD_REQUEST);

        if (full_discard) {
            new_l2_entry = new_l2_bitmap = 0;
        } else if (bs->backing || qcow2_cluster_is_allocated(cluster_type)) {
            if (has_subclusters(s)) {
                if (keep_reference) {
                    new_l2_entry = old_l2_entry;
                } else {
                    new_l2_entry = 0;
                }
                new_l2_bitmap = QCOW_L2_BITMAP_ALL_ZEROES;
            } else {
                if (s->qcow_version >= 3) {
                    if (keep_reference) {
                        new_l2_entry |= QCOW_OFLAG_ZERO;
                    } else {
                        new_l2_entry = QCOW_OFLAG_ZERO;
                    }
                } else {
                    new_l2_entry = 0;
                }
            }
        }

        if (old_l2_entry == new_l2_entry && old_l2_bitmap == new_l2_bitmap) {
            continue;
        }

        qcow2_cache_entry_mark_dirty(s->l2_table_cache, l2_slice);
        set_l2_entry(s, l2_slice, l2_index + i, new_l2_entry);
        if (has_subclusters(s)) {
            set_l2_bitmap(s, l2_slice, l2_index + i, new_l2_bitmap);
        }
        if (!keep_reference) {
            qcow2_free_any_cluster(bs, old_l2_entry, type);
        } else if (s->discard_passthrough[type] &&
                   (cluster_type == QCOW2_CLUSTER_NORMAL ||
                    cluster_type == QCOW2_CLUSTER_ZERO_ALLOC)) {
            bdrv_pdiscard(s->data_file, old_l2_entry & L2E_OFFSET_MASK,
                          s->cluster_size);
        }
    }

    qcow2_cache_put(s->l2_table_cache, (void **) &l2_slice);

    return nb_clusters;
}

int qcow2_cluster_discard(BlockDriverState *bs, uint64_t offset,
                          uint64_t bytes, enum qcow2_discard_type type,
                          bool full_discard)
{
    BDRVQcow2State *s = bs->opaque;
    uint64_t end_offset = offset + bytes;
    uint64_t nb_clusters;
    int64_t cleared;
    int ret;

    assert(QEMU_IS_ALIGNED(offset, s->cluster_size));
    assert(QEMU_IS_ALIGNED(end_offset, s->cluster_size) ||
           end_offset == bs->total_sectors << BDRV_SECTOR_BITS);

    nb_clusters = size_to_clusters(s, bytes);

    s->cache_discards = true;

    while (nb_clusters > 0) {
        cleared = discard_in_l2_slice(bs, offset, nb_clusters, type,
                                      full_discard);
        if (cleared < 0) {
            ret = cleared;
            goto fail;
        }

        nb_clusters -= cleared;
        offset += (cleared * s->cluster_size);
    }

    ret = 0;
fail:
    s->cache_discards = false;
    qcow2_process_discards(bs, ret);

    return ret;
}

static int coroutine_fn GRAPH_RDLOCK
zero_in_l2_slice(BlockDriverState *bs, uint64_t offset,
                 uint64_t nb_clusters, int flags)
{
    BDRVQcow2State *s = bs->opaque;
    uint64_t *l2_slice;
    int l2_index;
    int ret;
    int i;

    ret = get_cluster_table(bs, offset, &l2_slice, &l2_index);
    if (ret < 0) {
        return ret;
    }

    nb_clusters = MIN(nb_clusters, s->l2_slice_size - l2_index);
    assert(nb_clusters <= INT_MAX);

    for (i = 0; i < nb_clusters; i++) {
        uint64_t old_l2_entry = get_l2_entry(s, l2_slice, l2_index + i);
        uint64_t old_l2_bitmap = get_l2_bitmap(s, l2_slice, l2_index + i);
        QCow2ClusterType type = qcow2_get_cluster_type(bs, old_l2_entry);
        bool unmap = (type == QCOW2_CLUSTER_COMPRESSED) ||
            ((flags & BDRV_REQ_MAY_UNMAP) && qcow2_cluster_is_allocated(type));
        bool keep_reference =
            (s->discard_no_unref && type != QCOW2_CLUSTER_COMPRESSED);
        uint64_t new_l2_entry = old_l2_entry;
        uint64_t new_l2_bitmap = old_l2_bitmap;

        if (unmap && !keep_reference) {
            new_l2_entry = 0;
        }

        if (has_subclusters(s)) {
            new_l2_bitmap = QCOW_L2_BITMAP_ALL_ZEROES;
        } else {
            new_l2_entry |= QCOW_OFLAG_ZERO;
        }

        if (old_l2_entry == new_l2_entry && old_l2_bitmap == new_l2_bitmap) {
            continue;
        }

        qcow2_cache_entry_mark_dirty(s->l2_table_cache, l2_slice);
        set_l2_entry(s, l2_slice, l2_index + i, new_l2_entry);
        if (has_subclusters(s)) {
            set_l2_bitmap(s, l2_slice, l2_index + i, new_l2_bitmap);
        }

        if (unmap) {
            if (!keep_reference) {
                qcow2_free_any_cluster(bs, old_l2_entry, QCOW2_DISCARD_REQUEST);
            } else if (s->discard_passthrough[QCOW2_DISCARD_REQUEST] &&
                       (type == QCOW2_CLUSTER_NORMAL ||
                        type == QCOW2_CLUSTER_ZERO_ALLOC)) {
                bdrv_pdiscard(s->data_file, old_l2_entry & L2E_OFFSET_MASK,
                            s->cluster_size);
            }
        }
    }

    qcow2_cache_put(s->l2_table_cache, (void **) &l2_slice);

    return nb_clusters;
}

static int coroutine_fn GRAPH_RDLOCK
zero_l2_subclusters(BlockDriverState *bs, uint64_t offset,
                    unsigned nb_subclusters)
{
    BDRVQcow2State *s = bs->opaque;
    uint64_t *l2_slice;
    uint64_t old_l2_bitmap, l2_bitmap;
    int l2_index, ret, sc = offset_to_sc_index(s, offset);

    assert(nb_subclusters > 0 && nb_subclusters < s->subclusters_per_cluster);
    assert(sc + nb_subclusters <= s->subclusters_per_cluster);
    assert(offset_into_subcluster(s, offset) == 0);

    ret = get_cluster_table(bs, offset, &l2_slice, &l2_index);
    if (ret < 0) {
        return ret;
    }

    switch (qcow2_get_cluster_type(bs, get_l2_entry(s, l2_slice, l2_index))) {
    case QCOW2_CLUSTER_COMPRESSED:
        ret = -ENOTSUP; /* We cannot partially zeroize compressed clusters */
        goto out;
    case QCOW2_CLUSTER_NORMAL:
    case QCOW2_CLUSTER_UNALLOCATED:
        break;
    default:
        g_assert_not_reached();
    }

    old_l2_bitmap = l2_bitmap = get_l2_bitmap(s, l2_slice, l2_index);

    l2_bitmap |=  QCOW_OFLAG_SUB_ZERO_RANGE(sc, sc + nb_subclusters);
    l2_bitmap &= ~QCOW_OFLAG_SUB_ALLOC_RANGE(sc, sc + nb_subclusters);

    if (old_l2_bitmap != l2_bitmap) {
        set_l2_bitmap(s, l2_slice, l2_index, l2_bitmap);
        qcow2_cache_entry_mark_dirty(s->l2_table_cache, l2_slice);
    }

    ret = 0;
out:
    qcow2_cache_put(s->l2_table_cache, (void **) &l2_slice);

    return ret;
}

int coroutine_fn qcow2_subcluster_zeroize(BlockDriverState *bs, uint64_t offset,
                                          uint64_t bytes, int flags)
{
    BDRVQcow2State *s = bs->opaque;
    uint64_t end_offset = offset + bytes;
    uint64_t nb_clusters;
    unsigned head, tail;
    int64_t cleared;
    int ret;

    if (data_file_is_raw(bs)) {
        assert(has_data_file(bs));
        ret = bdrv_co_pwrite_zeroes(s->data_file, offset, bytes, flags);
        if (ret < 0) {
            return ret;
        }
    }

    assert(offset_into_subcluster(s, offset) == 0);
    assert(offset_into_subcluster(s, end_offset) == 0 ||
           end_offset >= bs->total_sectors << BDRV_SECTOR_BITS);

    if (s->qcow_version < 3) {
        if (!bs->backing) {
            return qcow2_cluster_discard(bs, offset, bytes,
                                         QCOW2_DISCARD_REQUEST, false);
        }
        return -ENOTSUP;
    }

    head = MIN(end_offset, ROUND_UP(offset, s->cluster_size)) - offset;
    offset += head;

    tail = (end_offset >= bs->total_sectors << BDRV_SECTOR_BITS) ? 0 :
        end_offset - MAX(offset, start_of_cluster(s, end_offset));
    end_offset -= tail;

    s->cache_discards = true;

    if (head) {
        ret = zero_l2_subclusters(bs, offset - head,
                                  size_to_subclusters(s, head));
        if (ret < 0) {
            goto fail;
        }
    }

    nb_clusters = size_to_clusters(s, end_offset - offset);

    while (nb_clusters > 0) {
        cleared = zero_in_l2_slice(bs, offset, nb_clusters, flags);
        if (cleared < 0) {
            ret = cleared;
            goto fail;
        }

        nb_clusters -= cleared;
        offset += (cleared * s->cluster_size);
    }

    if (tail) {
        ret = zero_l2_subclusters(bs, end_offset, size_to_subclusters(s, tail));
        if (ret < 0) {
            goto fail;
        }
    }

    ret = 0;
fail:
    s->cache_discards = false;
    qcow2_process_discards(bs, ret);

    return ret;
}

static int GRAPH_RDLOCK
expand_zero_clusters_in_l1(BlockDriverState *bs, uint64_t *l1_table,
                           int l1_size, int64_t *visited_l1_entries,
                           int64_t l1_entries,
                           BlockDriverAmendStatusCB *status_cb,
                           void *cb_opaque)
{
    BDRVQcow2State *s = bs->opaque;
    bool is_active_l1 = (l1_table == s->l1_table);
    uint64_t *l2_slice = NULL;
    unsigned slice, slice_size2, n_slices;
    int ret;
    int i, j;

    assert(!has_subclusters(s));

    slice_size2 = s->l2_slice_size * l2_entry_size(s);
    n_slices = s->cluster_size / slice_size2;

    if (!is_active_l1) {
        l2_slice = qemu_try_blockalign(bs->file->bs, slice_size2);
        if (l2_slice == NULL) {
            return -ENOMEM;
        }
    }

    for (i = 0; i < l1_size; i++) {
        uint64_t l2_offset = l1_table[i] & L1E_OFFSET_MASK;
        uint64_t l2_refcount;

        if (!l2_offset) {
            (*visited_l1_entries)++;
            if (status_cb) {
                status_cb(bs, *visited_l1_entries, l1_entries, cb_opaque);
            }
            continue;
        }

        if (offset_into_cluster(s, l2_offset)) {
            qcow2_signal_corruption(bs, true, -1, -1, "L2 table offset %#"
                                    PRIx64 " unaligned (L1 index: %#x)",
                                    l2_offset, i);
            ret = -EIO;
            goto fail;
        }

        ret = qcow2_get_refcount(bs, l2_offset >> s->cluster_bits,
                                 &l2_refcount);
        if (ret < 0) {
            goto fail;
        }

        for (slice = 0; slice < n_slices; slice++) {
            uint64_t slice_offset = l2_offset + slice * slice_size2;
            bool l2_dirty = false;
            if (is_active_l1) {
                ret = qcow2_cache_get(bs, s->l2_table_cache, slice_offset,
                                      (void **)&l2_slice);
            } else {
                ret = bdrv_pread(bs->file, slice_offset, slice_size2,
                                 l2_slice, 0);
            }
            if (ret < 0) {
                goto fail;
            }

            for (j = 0; j < s->l2_slice_size; j++) {
                uint64_t l2_entry = get_l2_entry(s, l2_slice, j);
                int64_t offset = l2_entry & L2E_OFFSET_MASK;
                QCow2ClusterType cluster_type =
                    qcow2_get_cluster_type(bs, l2_entry);

                if (cluster_type != QCOW2_CLUSTER_ZERO_PLAIN &&
                    cluster_type != QCOW2_CLUSTER_ZERO_ALLOC) {
                    continue;
                }

                if (cluster_type == QCOW2_CLUSTER_ZERO_PLAIN) {
                    if (!bs->backing) {
                        set_l2_entry(s, l2_slice, j, 0);
                        l2_dirty = true;
                        continue;
                    }

                    offset = qcow2_alloc_clusters(bs, s->cluster_size);
                    if (offset < 0) {
                        ret = offset;
                        goto fail;
                    }

                    assert((offset & L2E_OFFSET_MASK) == offset);

                    if (l2_refcount > 1) {
                        ret = qcow2_update_cluster_refcount(
                            bs, offset >> s->cluster_bits,
                            refcount_diff(1, l2_refcount), false,
                            QCOW2_DISCARD_OTHER);
                        if (ret < 0) {
                            qcow2_free_clusters(bs, offset, s->cluster_size,
                                                QCOW2_DISCARD_OTHER);
                            goto fail;
                        }
                    }
                }

                if (offset_into_cluster(s, offset)) {
                    int l2_index = slice * s->l2_slice_size + j;
                    qcow2_signal_corruption(
                        bs, true, -1, -1,
                        "Cluster allocation offset "
                        "%#" PRIx64 " unaligned (L2 offset: %#"
                        PRIx64 ", L2 index: %#x)", offset,
                        l2_offset, l2_index);
                    if (cluster_type == QCOW2_CLUSTER_ZERO_PLAIN) {
                        qcow2_free_clusters(bs, offset, s->cluster_size,
                                            QCOW2_DISCARD_ALWAYS);
                    }
                    ret = -EIO;
                    goto fail;
                }

                ret = qcow2_pre_write_overlap_check(bs, 0, offset,
                                                    s->cluster_size, true);
                if (ret < 0) {
                    if (cluster_type == QCOW2_CLUSTER_ZERO_PLAIN) {
                        qcow2_free_clusters(bs, offset, s->cluster_size,
                                            QCOW2_DISCARD_ALWAYS);
                    }
                    goto fail;
                }

                ret = bdrv_pwrite_zeroes(s->data_file, offset,
                                         s->cluster_size, 0);
                if (ret < 0) {
                    if (cluster_type == QCOW2_CLUSTER_ZERO_PLAIN) {
                        qcow2_free_clusters(bs, offset, s->cluster_size,
                                            QCOW2_DISCARD_ALWAYS);
                    }
                    goto fail;
                }

                if (l2_refcount == 1) {
                    set_l2_entry(s, l2_slice, j, offset | QCOW_OFLAG_COPIED);
                } else {
                    set_l2_entry(s, l2_slice, j, offset);
                }
                l2_dirty = true;
            }

            if (is_active_l1) {
                if (l2_dirty) {
                    qcow2_cache_entry_mark_dirty(s->l2_table_cache, l2_slice);
                    qcow2_cache_depends_on_flush(s->l2_table_cache);
                }
                qcow2_cache_put(s->l2_table_cache, (void **) &l2_slice);
            } else {
                if (l2_dirty) {
                    ret = qcow2_pre_write_overlap_check(
                        bs, QCOW2_OL_INACTIVE_L2 | QCOW2_OL_ACTIVE_L2,
                        slice_offset, slice_size2, false);
                    if (ret < 0) {
                        goto fail;
                    }

                    ret = bdrv_pwrite(bs->file, slice_offset, slice_size2,
                                      l2_slice, 0);
                    if (ret < 0) {
                        goto fail;
                    }
                }
            }
        }

        (*visited_l1_entries)++;
        if (status_cb) {
            status_cb(bs, *visited_l1_entries, l1_entries, cb_opaque);
        }
    }

    ret = 0;

fail:
    if (l2_slice) {
        if (!is_active_l1) {
            qemu_vfree(l2_slice);
        } else {
            qcow2_cache_put(s->l2_table_cache, (void **) &l2_slice);
        }
    }
    return ret;
}

int qcow2_expand_zero_clusters(BlockDriverState *bs,
                               BlockDriverAmendStatusCB *status_cb,
                               void *cb_opaque)
{
    BDRVQcow2State *s = bs->opaque;
    uint64_t *l1_table = NULL;
    int64_t l1_entries = 0, visited_l1_entries = 0;
    int ret;

    if (status_cb) {
        l1_entries = s->l1_size;
    }

    ret = expand_zero_clusters_in_l1(bs, s->l1_table, s->l1_size,
                                     &visited_l1_entries, l1_entries,
                                     status_cb, cb_opaque);
    if (ret < 0) {
        goto fail;
    }

    ret = qcow2_cache_empty(bs, s->l2_table_cache);
    if (ret < 0) {
        goto fail;
    }

    ret = 0;

fail:
    g_free(l1_table);
    return ret;
}

void qcow2_parse_compressed_l2_entry(BlockDriverState *bs, uint64_t l2_entry,
                                     uint64_t *coffset, int *csize)
{
    BDRVQcow2State *s = bs->opaque;
    int nb_csectors;

    assert(qcow2_get_cluster_type(bs, l2_entry) == QCOW2_CLUSTER_COMPRESSED);

    *coffset = l2_entry & s->cluster_offset_mask;

    nb_csectors = ((l2_entry >> s->csize_shift) & s->csize_mask) + 1;
    *csize = nb_csectors * QCOW2_COMPRESSED_SECTOR_SIZE -
        (*coffset & (QCOW2_COMPRESSED_SECTOR_SIZE - 1));
}
