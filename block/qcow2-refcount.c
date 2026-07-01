
#include "qemu/osdep.h"
#include "block/block-io.h"
#include "qapi/error.h"
#include "qcow2.h"
#include "qemu/range.h"
#include "qemu/bswap.h"
#include "qemu/cutils.h"
#include "qemu/memalign.h"
#include "trace.h"

static int64_t alloc_clusters_noref(BlockDriverState *bs, uint64_t size,
                                    uint64_t max);

G_GNUC_WARN_UNUSED_RESULT
static int update_refcount(BlockDriverState *bs,
                           int64_t offset, int64_t length, uint64_t addend,
                           bool decrease, enum qcow2_discard_type type);

static uint64_t get_refcount_ro0(const void *refcount_array, uint64_t index);
static uint64_t get_refcount_ro1(const void *refcount_array, uint64_t index);
static uint64_t get_refcount_ro2(const void *refcount_array, uint64_t index);
static uint64_t get_refcount_ro3(const void *refcount_array, uint64_t index);
static uint64_t get_refcount_ro4(const void *refcount_array, uint64_t index);
static uint64_t get_refcount_ro5(const void *refcount_array, uint64_t index);
static uint64_t get_refcount_ro6(const void *refcount_array, uint64_t index);

static void set_refcount_ro0(void *refcount_array, uint64_t index,
                             uint64_t value);
static void set_refcount_ro1(void *refcount_array, uint64_t index,
                             uint64_t value);
static void set_refcount_ro2(void *refcount_array, uint64_t index,
                             uint64_t value);
static void set_refcount_ro3(void *refcount_array, uint64_t index,
                             uint64_t value);
static void set_refcount_ro4(void *refcount_array, uint64_t index,
                             uint64_t value);
static void set_refcount_ro5(void *refcount_array, uint64_t index,
                             uint64_t value);
static void set_refcount_ro6(void *refcount_array, uint64_t index,
                             uint64_t value);


static Qcow2GetRefcountFunc *const get_refcount_funcs[] = {
    &get_refcount_ro0,
    &get_refcount_ro1,
    &get_refcount_ro2,
    &get_refcount_ro3,
    &get_refcount_ro4,
    &get_refcount_ro5,
    &get_refcount_ro6
};

static Qcow2SetRefcountFunc *const set_refcount_funcs[] = {
    &set_refcount_ro0,
    &set_refcount_ro1,
    &set_refcount_ro2,
    &set_refcount_ro3,
    &set_refcount_ro4,
    &set_refcount_ro5,
    &set_refcount_ro6
};



static void update_max_refcount_table_index(BDRVQcow2State *s)
{
    unsigned i = s->refcount_table_size - 1;
    while (i > 0 && (s->refcount_table[i] & REFT_OFFSET_MASK) == 0) {
        i--;
    }
    s->max_refcount_table_index = i;
}

int coroutine_fn qcow2_refcount_init(BlockDriverState *bs)
{
    BDRVQcow2State *s = bs->opaque;
    unsigned int refcount_table_size2, i;
    int ret;

    assert(s->refcount_order >= 0 && s->refcount_order <= 6);

    s->get_refcount = get_refcount_funcs[s->refcount_order];
    s->set_refcount = set_refcount_funcs[s->refcount_order];

    assert(s->refcount_table_size <= INT_MAX / REFTABLE_ENTRY_SIZE);
    refcount_table_size2 = s->refcount_table_size * REFTABLE_ENTRY_SIZE;
    s->refcount_table = g_try_malloc(refcount_table_size2);

    if (s->refcount_table_size > 0) {
        if (s->refcount_table == NULL) {
            ret = -ENOMEM;
            goto fail;
        }
        BLKDBG_CO_EVENT(bs->file, BLKDBG_REFTABLE_LOAD);
        ret = bdrv_co_pread(bs->file, s->refcount_table_offset,
                            refcount_table_size2, s->refcount_table, 0);
        if (ret < 0) {
            goto fail;
        }
        for(i = 0; i < s->refcount_table_size; i++)
            be64_to_cpus(&s->refcount_table[i]);
        update_max_refcount_table_index(s);
    }
    return 0;
 fail:
    return ret;
}

void qcow2_refcount_close(BlockDriverState *bs)
{
    BDRVQcow2State *s = bs->opaque;
    g_free(s->refcount_table);
}


static uint64_t get_refcount_ro0(const void *refcount_array, uint64_t index)
{
    return (((const uint8_t *)refcount_array)[index / 8] >> (index % 8)) & 0x1;
}

static void set_refcount_ro0(void *refcount_array, uint64_t index,
                             uint64_t value)
{
    assert(!(value >> 1));
    ((uint8_t *)refcount_array)[index / 8] &= ~(0x1 << (index % 8));
    ((uint8_t *)refcount_array)[index / 8] |= value << (index % 8);
}

static uint64_t get_refcount_ro1(const void *refcount_array, uint64_t index)
{
    return (((const uint8_t *)refcount_array)[index / 4] >> (2 * (index % 4)))
           & 0x3;
}

static void set_refcount_ro1(void *refcount_array, uint64_t index,
                             uint64_t value)
{
    assert(!(value >> 2));
    ((uint8_t *)refcount_array)[index / 4] &= ~(0x3 << (2 * (index % 4)));
    ((uint8_t *)refcount_array)[index / 4] |= value << (2 * (index % 4));
}

static uint64_t get_refcount_ro2(const void *refcount_array, uint64_t index)
{
    return (((const uint8_t *)refcount_array)[index / 2] >> (4 * (index % 2)))
           & 0xf;
}

static void set_refcount_ro2(void *refcount_array, uint64_t index,
                             uint64_t value)
{
    assert(!(value >> 4));
    ((uint8_t *)refcount_array)[index / 2] &= ~(0xf << (4 * (index % 2)));
    ((uint8_t *)refcount_array)[index / 2] |= value << (4 * (index % 2));
}

static uint64_t get_refcount_ro3(const void *refcount_array, uint64_t index)
{
    return ((const uint8_t *)refcount_array)[index];
}

static void set_refcount_ro3(void *refcount_array, uint64_t index,
                             uint64_t value)
{
    assert(!(value >> 8));
    ((uint8_t *)refcount_array)[index] = value;
}

static uint64_t get_refcount_ro4(const void *refcount_array, uint64_t index)
{
    return be16_to_cpu(((const uint16_t *)refcount_array)[index]);
}

static void set_refcount_ro4(void *refcount_array, uint64_t index,
                             uint64_t value)
{
    assert(!(value >> 16));
    ((uint16_t *)refcount_array)[index] = cpu_to_be16(value);
}

static uint64_t get_refcount_ro5(const void *refcount_array, uint64_t index)
{
    return be32_to_cpu(((const uint32_t *)refcount_array)[index]);
}

static void set_refcount_ro5(void *refcount_array, uint64_t index,
                             uint64_t value)
{
    assert(!(value >> 32));
    ((uint32_t *)refcount_array)[index] = cpu_to_be32(value);
}

static uint64_t get_refcount_ro6(const void *refcount_array, uint64_t index)
{
    return be64_to_cpu(((const uint64_t *)refcount_array)[index]);
}

static void set_refcount_ro6(void *refcount_array, uint64_t index,
                             uint64_t value)
{
    ((uint64_t *)refcount_array)[index] = cpu_to_be64(value);
}


static int GRAPH_RDLOCK
load_refcount_block(BlockDriverState *bs, int64_t refcount_block_offset,
                    void **refcount_block)
{
    BDRVQcow2State *s = bs->opaque;

    BLKDBG_EVENT(bs->file, BLKDBG_REFBLOCK_LOAD);
    return qcow2_cache_get(bs, s->refcount_block_cache, refcount_block_offset,
                           refcount_block);
}

int qcow2_get_refcount(BlockDriverState *bs, int64_t cluster_index,
                       uint64_t *refcount)
{
    BDRVQcow2State *s = bs->opaque;
    uint64_t refcount_table_index, block_index;
    int64_t refcount_block_offset;
    int ret;
    void *refcount_block;

    refcount_table_index = cluster_index >> s->refcount_block_bits;
    if (refcount_table_index >= s->refcount_table_size) {
        *refcount = 0;
        return 0;
    }
    refcount_block_offset =
        s->refcount_table[refcount_table_index] & REFT_OFFSET_MASK;
    if (!refcount_block_offset) {
        *refcount = 0;
        return 0;
    }

    if (offset_into_cluster(s, refcount_block_offset)) {
        qcow2_signal_corruption(bs, true, -1, -1, "Refblock offset %#" PRIx64
                                " unaligned (reftable index: %#" PRIx64 ")",
                                refcount_block_offset, refcount_table_index);
        return -EIO;
    }

    ret = qcow2_cache_get(bs, s->refcount_block_cache, refcount_block_offset,
                          &refcount_block);
    if (ret < 0) {
        return ret;
    }

    block_index = cluster_index & (s->refcount_block_size - 1);
    *refcount = s->get_refcount(refcount_block, block_index);

    qcow2_cache_put(s->refcount_block_cache, &refcount_block);

    return 0;
}

static int in_same_refcount_block(BDRVQcow2State *s, uint64_t offset_a,
    uint64_t offset_b)
{
    uint64_t block_a = offset_a >> (s->cluster_bits + s->refcount_block_bits);
    uint64_t block_b = offset_b >> (s->cluster_bits + s->refcount_block_bits);

    return (block_a == block_b);
}

static int GRAPH_RDLOCK
alloc_refcount_block(BlockDriverState *bs, int64_t cluster_index,
                     void **refcount_block)
{
    BDRVQcow2State *s = bs->opaque;
    unsigned int refcount_table_index;
    int64_t ret;

    BLKDBG_EVENT(bs->file, BLKDBG_REFBLOCK_ALLOC);

    refcount_table_index = cluster_index >> s->refcount_block_bits;

    if (refcount_table_index < s->refcount_table_size) {

        uint64_t refcount_block_offset =
            s->refcount_table[refcount_table_index] & REFT_OFFSET_MASK;

        if (refcount_block_offset) {
            if (offset_into_cluster(s, refcount_block_offset)) {
                qcow2_signal_corruption(bs, true, -1, -1, "Refblock offset %#"
                                        PRIx64 " unaligned (reftable index: "
                                        "%#x)", refcount_block_offset,
                                        refcount_table_index);
                return -EIO;
            }

             return load_refcount_block(bs, refcount_block_offset,
                                        refcount_block);
        }
    }


    *refcount_block = NULL;

    ret = qcow2_cache_flush(bs, s->l2_table_cache);
    if (ret < 0) {
        return ret;
    }

    int64_t new_block = alloc_clusters_noref(bs, s->cluster_size, INT64_MAX);
    if (new_block < 0) {
        return new_block;
    }

    assert((new_block & REFT_OFFSET_MASK) == new_block);

    if (new_block == 0) {
        qcow2_signal_corruption(bs, true, -1, -1, "Preventing invalid "
                                "allocation of refcount block at offset 0");
        return -EIO;
    }

#ifdef DEBUG_ALLOC2
    fprintf(stderr, "qcow2: Allocate refcount block %d for %" PRIx64
        " at %" PRIx64 "\n",
        refcount_table_index, cluster_index << s->cluster_bits, new_block);
#endif

    if (in_same_refcount_block(s, new_block, cluster_index << s->cluster_bits)) {
        ret = qcow2_cache_get_empty(bs, s->refcount_block_cache, new_block,
                                    refcount_block);
        if (ret < 0) {
            goto fail;
        }

        memset(*refcount_block, 0, s->cluster_size);

        int block_index = (new_block >> s->cluster_bits) &
            (s->refcount_block_size - 1);
        s->set_refcount(*refcount_block, block_index, 1);
    } else {
        ret = update_refcount(bs, new_block, s->cluster_size, 1, false,
                              QCOW2_DISCARD_NEVER);
        if (ret < 0) {
            goto fail;
        }

        ret = qcow2_cache_flush(bs, s->refcount_block_cache);
        if (ret < 0) {
            goto fail;
        }

        ret = qcow2_cache_get_empty(bs, s->refcount_block_cache, new_block,
                                    refcount_block);
        if (ret < 0) {
            goto fail;
        }

        memset(*refcount_block, 0, s->cluster_size);
    }

    BLKDBG_EVENT(bs->file, BLKDBG_REFBLOCK_ALLOC_WRITE);
    qcow2_cache_entry_mark_dirty(s->refcount_block_cache, *refcount_block);
    ret = qcow2_cache_flush(bs, s->refcount_block_cache);
    if (ret < 0) {
        goto fail;
    }

    if (refcount_table_index < s->refcount_table_size) {
        uint64_t data64 = cpu_to_be64(new_block);
        BLKDBG_EVENT(bs->file, BLKDBG_REFBLOCK_ALLOC_HOOKUP);
        ret = bdrv_pwrite_sync(bs->file, s->refcount_table_offset +
                               refcount_table_index * REFTABLE_ENTRY_SIZE,
            sizeof(data64), &data64, 0);
        if (ret < 0) {
            goto fail;
        }

        s->refcount_table[refcount_table_index] = new_block;
        s->max_refcount_table_index =
            MAX(s->max_refcount_table_index, refcount_table_index);

        return -EAGAIN;
    }

    qcow2_cache_put(s->refcount_block_cache, refcount_block);

    BLKDBG_EVENT(bs->file, BLKDBG_REFTABLE_GROW);

    uint64_t blocks_used = DIV_ROUND_UP(MAX(cluster_index + 1,
                                            (new_block >> s->cluster_bits) + 1),
                                        s->refcount_block_size);

    uint64_t meta_offset = (blocks_used * s->refcount_block_size) *
        s->cluster_size;

    ret = qcow2_refcount_area(bs, meta_offset, 0, false,
                              refcount_table_index, new_block);
    if (ret < 0) {
        return ret;
    }

    ret = load_refcount_block(bs, new_block, refcount_block);
    if (ret < 0) {
        return ret;
    }

    return -EAGAIN;

fail:
    if (*refcount_block != NULL) {
        qcow2_cache_put(s->refcount_block_cache, refcount_block);
    }
    return ret;
}

int64_t qcow2_refcount_area(BlockDriverState *bs, uint64_t start_offset,
                            uint64_t additional_clusters, bool exact_size,
                            int new_refblock_index,
                            uint64_t new_refblock_offset)
{
    BDRVQcow2State *s = bs->opaque;
    uint64_t total_refblock_count_u64, additional_refblock_count;
    int total_refblock_count, table_size, area_reftable_index, table_clusters;
    int i;
    uint64_t table_offset, block_offset, end_offset;
    int ret;
    uint64_t *new_table;

    assert(!(start_offset % s->cluster_size));

    qcow2_refcount_metadata_size(start_offset / s->cluster_size +
                                 additional_clusters,
                                 s->cluster_size, s->refcount_order,
                                 !exact_size, &total_refblock_count_u64);
    if (total_refblock_count_u64 > QCOW_MAX_REFTABLE_SIZE) {
        return -EFBIG;
    }
    total_refblock_count = total_refblock_count_u64;

    area_reftable_index = (start_offset / s->cluster_size) /
                          s->refcount_block_size;

    if (exact_size) {
        table_size = total_refblock_count;
    } else {
        table_size = total_refblock_count +
                     DIV_ROUND_UP(total_refblock_count, 2);
    }
    table_size = ROUND_UP(table_size, s->cluster_size / REFTABLE_ENTRY_SIZE);
    table_clusters = (table_size * REFTABLE_ENTRY_SIZE) / s->cluster_size;

    if (table_size > QCOW_MAX_REFTABLE_SIZE) {
        return -EFBIG;
    }

    new_table = g_try_new0(uint64_t, table_size);

    assert(table_size > 0);
    if (new_table == NULL) {
        ret = -ENOMEM;
        goto fail;
    }

    if (table_size > s->max_refcount_table_index) {
        memcpy(new_table, s->refcount_table,
               (s->max_refcount_table_index + 1) * REFTABLE_ENTRY_SIZE);
    } else {
        memcpy(new_table, s->refcount_table, table_size * REFTABLE_ENTRY_SIZE);
    }

    if (new_refblock_offset) {
        assert(new_refblock_index < total_refblock_count);
        new_table[new_refblock_index] = new_refblock_offset;
    }

    additional_refblock_count = 0;
    for (i = area_reftable_index; i < total_refblock_count; i++) {
        if (!new_table[i]) {
            additional_refblock_count++;
        }
    }

    table_offset = start_offset + additional_refblock_count * s->cluster_size;
    end_offset = table_offset + table_clusters * s->cluster_size;

    block_offset = start_offset;
    for (i = area_reftable_index; i < total_refblock_count; i++) {
        void *refblock_data;
        uint64_t first_offset_covered;

        if (new_table[i]) {
            ret = qcow2_cache_get(bs, s->refcount_block_cache, new_table[i],
                                  &refblock_data);
            if (ret < 0) {
                goto fail;
            }
        } else {
            ret = qcow2_cache_get_empty(bs, s->refcount_block_cache,
                                        block_offset, &refblock_data);
            if (ret < 0) {
                goto fail;
            }
            memset(refblock_data, 0, s->cluster_size);
            qcow2_cache_entry_mark_dirty(s->refcount_block_cache,
                                         refblock_data);

            new_table[i] = block_offset;
            block_offset += s->cluster_size;
        }

        first_offset_covered = (uint64_t)i * s->refcount_block_size *
                               s->cluster_size;
        if (first_offset_covered < end_offset) {
            int j, end_index;


            if (first_offset_covered < start_offset) {
                assert(i == area_reftable_index);
                j = (start_offset - first_offset_covered) / s->cluster_size;
                assert(j < s->refcount_block_size);
            } else {
                j = 0;
            }

            end_index = MIN((end_offset - first_offset_covered) /
                            s->cluster_size,
                            s->refcount_block_size);

            for (; j < end_index; j++) {
                assert(s->get_refcount(refblock_data, j) == 0);
                s->set_refcount(refblock_data, j, 1);
            }

            qcow2_cache_entry_mark_dirty(s->refcount_block_cache,
                                         refblock_data);
        }

        qcow2_cache_put(s->refcount_block_cache, &refblock_data);
    }

    assert(block_offset == table_offset);

    BLKDBG_EVENT(bs->file, BLKDBG_REFBLOCK_ALLOC_WRITE_BLOCKS);
    ret = qcow2_cache_flush(bs, s->refcount_block_cache);
    if (ret < 0) {
        goto fail;
    }

    for (i = 0; i < total_refblock_count; i++) {
        cpu_to_be64s(&new_table[i]);
    }

    BLKDBG_EVENT(bs->file, BLKDBG_REFBLOCK_ALLOC_WRITE_TABLE);
    ret = bdrv_pwrite_sync(bs->file, table_offset,
                           table_size * REFTABLE_ENTRY_SIZE, new_table, 0);
    if (ret < 0) {
        goto fail;
    }

    for (i = 0; i < total_refblock_count; i++) {
        be64_to_cpus(&new_table[i]);
    }

    struct QEMU_PACKED {
        uint64_t d64;
        uint32_t d32;
    } data;
    data.d64 = cpu_to_be64(table_offset);
    data.d32 = cpu_to_be32(table_clusters);
    BLKDBG_EVENT(bs->file, BLKDBG_REFBLOCK_ALLOC_SWITCH_TABLE);
    ret = bdrv_pwrite_sync(bs->file,
                           offsetof(QCowHeader, refcount_table_offset),
                           sizeof(data), &data, 0);
    if (ret < 0) {
        goto fail;
    }

    uint64_t old_table_offset = s->refcount_table_offset;
    uint64_t old_table_size = s->refcount_table_size;

    g_free(s->refcount_table);
    s->refcount_table = new_table;
    s->refcount_table_size = table_size;
    s->refcount_table_offset = table_offset;
    update_max_refcount_table_index(s);

    qcow2_free_clusters(bs, old_table_offset,
                        old_table_size * REFTABLE_ENTRY_SIZE,
                        QCOW2_DISCARD_OTHER);

    return end_offset;

fail:
    g_free(new_table);
    return ret;
}

void qcow2_process_discards(BlockDriverState *bs, int ret)
{
    BDRVQcow2State *s = bs->opaque;
    Qcow2DiscardRegion *d, *next;

    QTAILQ_FOREACH_SAFE(d, &s->discards, next, next) {
        QTAILQ_REMOVE(&s->discards, d, next);

        if (ret >= 0) {
            int r2 = bdrv_pdiscard(bs->file, d->offset, d->bytes);
            if (r2 < 0) {
                trace_qcow2_process_discards_failed_region(d->offset, d->bytes,
                                                           r2);
            }
        }

        g_free(d);
    }
}

static void update_refcount_discard(BlockDriverState *bs,
                                    uint64_t offset, uint64_t length)
{
    BDRVQcow2State *s = bs->opaque;
    Qcow2DiscardRegion *d, *p, *next;

    QTAILQ_FOREACH(d, &s->discards, next) {
        uint64_t new_start = MIN(offset, d->offset);
        uint64_t new_end = MAX(offset + length, d->offset + d->bytes);

        if (new_end - new_start <= length + d->bytes) {
            assert(d->bytes + length == new_end - new_start);
            d->offset = new_start;
            d->bytes = new_end - new_start;
            goto found;
        }
    }

    d = g_malloc(sizeof(*d));
    *d = (Qcow2DiscardRegion) {
        .bs     = bs,
        .offset = offset,
        .bytes  = length,
    };
    QTAILQ_INSERT_TAIL(&s->discards, d, next);

found:
    QTAILQ_FOREACH_SAFE(p, &s->discards, next, next) {
        if (p == d
            || p->offset > d->offset + d->bytes
            || d->offset > p->offset + p->bytes)
        {
            continue;
        }

        assert(p->offset == d->offset + d->bytes
            || d->offset == p->offset + p->bytes);

        QTAILQ_REMOVE(&s->discards, p, next);
        d->offset = MIN(d->offset, p->offset);
        d->bytes += p->bytes;
        g_free(p);
    }
}

static int GRAPH_RDLOCK
update_refcount(BlockDriverState *bs, int64_t offset, int64_t length,
                uint64_t addend, bool decrease, enum qcow2_discard_type type)
{
    BDRVQcow2State *s = bs->opaque;
    int64_t start, last, cluster_offset;
    void *refcount_block = NULL;
    int64_t old_table_index = -1;
    int ret;

#ifdef DEBUG_ALLOC2
    fprintf(stderr, "update_refcount: offset=%" PRId64 " size=%" PRId64
            " addend=%s%" PRIu64 "\n", offset, length, decrease ? "-" : "",
            addend);
#endif
    if (length < 0) {
        return -EINVAL;
    } else if (length == 0) {
        return 0;
    }

    if (decrease) {
        qcow2_cache_set_dependency(bs, s->refcount_block_cache,
            s->l2_table_cache);
    }

    start = start_of_cluster(s, offset);
    last = start_of_cluster(s, offset + length - 1);
    for(cluster_offset = start; cluster_offset <= last;
        cluster_offset += s->cluster_size)
    {
        int block_index;
        uint64_t refcount;
        int64_t cluster_index = cluster_offset >> s->cluster_bits;
        int64_t table_index = cluster_index >> s->refcount_block_bits;

        if (table_index != old_table_index) {
            if (refcount_block) {
                qcow2_cache_put(s->refcount_block_cache, &refcount_block);
            }
            ret = alloc_refcount_block(bs, cluster_index, &refcount_block);
            if (ret == -EAGAIN) {
                if (s->free_cluster_index > (start >> s->cluster_bits)) {
                    s->free_cluster_index = (start >> s->cluster_bits);
                }
            }
            if (ret < 0) {
                goto fail;
            }
        }
        old_table_index = table_index;

        qcow2_cache_entry_mark_dirty(s->refcount_block_cache, refcount_block);

        block_index = cluster_index & (s->refcount_block_size - 1);

        refcount = s->get_refcount(refcount_block, block_index);
        if (decrease ? (refcount - addend > refcount)
                     : (refcount + addend < refcount ||
                        refcount + addend > s->refcount_max))
        {
            ret = -EINVAL;
            goto fail;
        }
        if (decrease) {
            refcount -= addend;
        } else {
            refcount += addend;
        }
        if (refcount == 0 && cluster_index < s->free_cluster_index) {
            s->free_cluster_index = cluster_index;
        }
        s->set_refcount(refcount_block, block_index, refcount);

        if (refcount == 0) {
            void *table;

            table = qcow2_cache_is_table_offset(s->refcount_block_cache,
                                                offset);
            if (table != NULL) {
                qcow2_cache_put(s->refcount_block_cache, &refcount_block);
                old_table_index = -1;
                qcow2_cache_discard(s->refcount_block_cache, table);
            }

            table = qcow2_cache_is_table_offset(s->l2_table_cache, offset);
            if (table != NULL) {
                qcow2_cache_discard(s->l2_table_cache, table);
            }

            if (s->discard_passthrough[type]) {
                update_refcount_discard(bs, cluster_offset, s->cluster_size);
            }
        }
    }

    ret = 0;
fail:
    if (!s->cache_discards) {
        qcow2_process_discards(bs, ret);
    }

    if (refcount_block) {
        qcow2_cache_put(s->refcount_block_cache, &refcount_block);
    }

    if (ret < 0) {
        int dummy;
        dummy = update_refcount(bs, offset, cluster_offset - offset, addend,
                                !decrease, QCOW2_DISCARD_NEVER);
        (void)dummy;
    }

    return ret;
}

int qcow2_update_cluster_refcount(BlockDriverState *bs,
                                  int64_t cluster_index,
                                  uint64_t addend, bool decrease,
                                  enum qcow2_discard_type type)
{
    BDRVQcow2State *s = bs->opaque;
    int ret;

    ret = update_refcount(bs, cluster_index << s->cluster_bits, 1, addend,
                          decrease, type);
    if (ret < 0) {
        return ret;
    }

    return 0;
}






static int64_t GRAPH_RDLOCK
alloc_clusters_noref(BlockDriverState *bs, uint64_t size, uint64_t max)
{
    BDRVQcow2State *s = bs->opaque;
    uint64_t i, nb_clusters, refcount;
    int ret;

    if (s->cache_discards) {
        qcow2_process_discards(bs, 0);
    }

    nb_clusters = size_to_clusters(s, size);
retry:
    for(i = 0; i < nb_clusters; i++) {
        uint64_t next_cluster_index = s->free_cluster_index++;
        ret = qcow2_get_refcount(bs, next_cluster_index, &refcount);

        if (ret < 0) {
            return ret;
        } else if (refcount != 0) {
            goto retry;
        }
    }

    if (s->free_cluster_index > 0 &&
        s->free_cluster_index - 1 > (max >> s->cluster_bits))
    {
        return -EFBIG;
    }

#ifdef DEBUG_ALLOC2
    fprintf(stderr, "alloc_clusters: size=%" PRId64 " -> %" PRId64 "\n",
            size,
            (s->free_cluster_index - nb_clusters) << s->cluster_bits);
#endif
    return (s->free_cluster_index - nb_clusters) << s->cluster_bits;
}

int64_t qcow2_alloc_clusters(BlockDriverState *bs, uint64_t size)
{
    int64_t offset;
    int ret;

    BLKDBG_EVENT(bs->file, BLKDBG_CLUSTER_ALLOC);
    do {
        offset = alloc_clusters_noref(bs, size, QCOW_MAX_CLUSTER_OFFSET);
        if (offset < 0) {
            return offset;
        }

        ret = update_refcount(bs, offset, size, 1, false, QCOW2_DISCARD_NEVER);
    } while (ret == -EAGAIN);

    if (ret < 0) {
        return ret;
    }

    return offset;
}

int64_t coroutine_fn qcow2_alloc_clusters_at(BlockDriverState *bs, uint64_t offset,
                                             int64_t nb_clusters)
{
    BDRVQcow2State *s = bs->opaque;
    uint64_t cluster_index, refcount;
    uint64_t i;
    int ret;

    assert(nb_clusters >= 0);
    if (nb_clusters == 0) {
        return 0;
    }

    do {
        cluster_index = offset >> s->cluster_bits;
        for(i = 0; i < nb_clusters; i++) {
            ret = qcow2_get_refcount(bs, cluster_index++, &refcount);
            if (ret < 0) {
                return ret;
            } else if (refcount != 0) {
                break;
            }
        }

        ret = update_refcount(bs, offset, i << s->cluster_bits, 1, false,
                              QCOW2_DISCARD_NEVER);
    } while (ret == -EAGAIN);

    if (ret < 0) {
        return ret;
    }

    return i;
}

int64_t coroutine_fn GRAPH_RDLOCK qcow2_alloc_bytes(BlockDriverState *bs, int size)
{
    BDRVQcow2State *s = bs->opaque;
    int64_t offset;
    size_t free_in_cluster;
    int ret;

    BLKDBG_CO_EVENT(bs->file, BLKDBG_CLUSTER_ALLOC_BYTES);
    assert(size > 0 && size <= s->cluster_size);
    assert(!s->free_byte_offset || offset_into_cluster(s, s->free_byte_offset));

    offset = s->free_byte_offset;

    if (offset) {
        uint64_t refcount;
        ret = qcow2_get_refcount(bs, offset >> s->cluster_bits, &refcount);
        if (ret < 0) {
            return ret;
        }

        if (refcount == s->refcount_max) {
            offset = 0;
        }
    }

    free_in_cluster = s->cluster_size - offset_into_cluster(s, offset);
    do {
        if (!offset || free_in_cluster < size) {
            int64_t new_cluster;

            new_cluster = alloc_clusters_noref(bs, s->cluster_size,
                                               MIN(s->cluster_offset_mask,
                                                   QCOW_MAX_CLUSTER_OFFSET));
            if (new_cluster < 0) {
                return new_cluster;
            }

            if (new_cluster == 0) {
                qcow2_signal_corruption(bs, true, -1, -1, "Preventing invalid "
                                        "allocation of compressed cluster "
                                        "at offset 0");
                return -EIO;
            }

            if (!offset || ROUND_UP(offset, s->cluster_size) != new_cluster) {
                offset = new_cluster;
                free_in_cluster = s->cluster_size;
            } else {
                free_in_cluster += s->cluster_size;
            }
        }

        assert(offset);
        ret = update_refcount(bs, offset, size, 1, false, QCOW2_DISCARD_NEVER);
        if (ret < 0) {
            offset = 0;
        }
    } while (ret == -EAGAIN);
    if (ret < 0) {
        return ret;
    }

    qcow2_cache_set_dependency(bs, s->l2_table_cache, s->refcount_block_cache);

    s->free_byte_offset = offset + size;
    if (!offset_into_cluster(s, s->free_byte_offset)) {
        s->free_byte_offset = 0;
    }

    return offset;
}

void qcow2_free_clusters(BlockDriverState *bs,
                          int64_t offset, int64_t size,
                          enum qcow2_discard_type type)
{
    int ret;

    BLKDBG_EVENT(bs->file, BLKDBG_CLUSTER_FREE);
    ret = update_refcount(bs, offset, size, 1, true, type);
    if (ret < 0) {
        fprintf(stderr, "qcow2_free_clusters failed: %s\n", strerror(-ret));
    }
}

void qcow2_free_any_cluster(BlockDriverState *bs, uint64_t l2_entry,
                            enum qcow2_discard_type type)
{
    BDRVQcow2State *s = bs->opaque;
    QCow2ClusterType ctype = qcow2_get_cluster_type(bs, l2_entry);

    if (has_data_file(bs)) {
        if (s->discard_passthrough[type] &&
            (ctype == QCOW2_CLUSTER_NORMAL ||
             ctype == QCOW2_CLUSTER_ZERO_ALLOC))
        {
            bdrv_pdiscard(s->data_file, l2_entry & L2E_OFFSET_MASK,
                          s->cluster_size);
        }
        return;
    }

    switch (ctype) {
    case QCOW2_CLUSTER_COMPRESSED:
        {
            uint64_t coffset;
            int csize;

            qcow2_parse_compressed_l2_entry(bs, l2_entry, &coffset, &csize);
            qcow2_free_clusters(bs, coffset, csize, type);
        }
        break;
    case QCOW2_CLUSTER_NORMAL:
    case QCOW2_CLUSTER_ZERO_ALLOC:
        if (offset_into_cluster(s, l2_entry & L2E_OFFSET_MASK)) {
            qcow2_signal_corruption(bs, false, -1, -1,
                                    "Cannot free unaligned cluster %#llx",
                                    l2_entry & L2E_OFFSET_MASK);
        } else {
            qcow2_free_clusters(bs, l2_entry & L2E_OFFSET_MASK,
                                s->cluster_size, type);
        }
        break;
    case QCOW2_CLUSTER_ZERO_PLAIN:
    case QCOW2_CLUSTER_UNALLOCATED:
        break;
    default:
        abort();
    }
}

int qcow2_write_caches(BlockDriverState *bs)
{
    BDRVQcow2State *s = bs->opaque;
    int ret;

    ret = qcow2_cache_write(bs, s->l2_table_cache);
    if (ret < 0) {
        return ret;
    }

    if (qcow2_need_accurate_refcounts(s)) {
        ret = qcow2_cache_write(bs, s->refcount_block_cache);
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}

int qcow2_flush_caches(BlockDriverState *bs)
{
    int ret = qcow2_write_caches(bs);
    if (ret < 0) {
        return ret;
    }

    return bdrv_flush(bs->file->bs);
}




static uint64_t refcount_array_byte_size(BDRVQcow2State *s, uint64_t entries)
{
    assert(entries < (UINT64_C(1) << (64 - 9)));

    return DIV_ROUND_UP(entries << s->refcount_order, 8);
}

static int realloc_refcount_array(BDRVQcow2State *s, void **array,
                                  int64_t *size, int64_t new_size)
{
    int64_t old_byte_size, new_byte_size;
    void *new_ptr;

    old_byte_size = size_to_clusters(s, refcount_array_byte_size(s, *size))
                    * s->cluster_size;
    new_byte_size = size_to_clusters(s, refcount_array_byte_size(s, new_size))
                    * s->cluster_size;

    if (new_byte_size == old_byte_size) {
        *size = new_size;
        return 0;
    }

    assert(new_byte_size > 0);

    if (new_byte_size > SIZE_MAX) {
        return -ENOMEM;
    }

    new_ptr = g_try_realloc(*array, new_byte_size);
    if (!new_ptr) {
        return -ENOMEM;
    }

    if (new_byte_size > old_byte_size) {
        memset((char *)new_ptr + old_byte_size, 0,
               new_byte_size - old_byte_size);
    }

    *array = new_ptr;
    *size  = new_size;

    return 0;
}

int coroutine_fn GRAPH_RDLOCK
qcow2_inc_refcounts_imrt(BlockDriverState *bs, BdrvCheckResult *res,
                         void **refcount_table,
                         int64_t *refcount_table_size,
                         int64_t offset, int64_t size)
{
    BDRVQcow2State *s = bs->opaque;
    uint64_t start, last, cluster_offset, k, refcount;
    int64_t file_len;
    int ret;

    if (size <= 0) {
        return 0;
    }

    file_len = bdrv_co_getlength(bs->file->bs);
    if (file_len < 0) {
        return file_len;
    }

    if (offset + size - file_len >= s->cluster_size) {
        fprintf(stderr, "ERROR: counting reference for region exceeding the "
                "end of the file by one cluster or more: offset 0x%" PRIx64
                " size 0x%" PRIx64 "\n", offset, size);
        res->corruptions++;
        return 0;
    }

    start = start_of_cluster(s, offset);
    last = start_of_cluster(s, offset + size - 1);
    for(cluster_offset = start; cluster_offset <= last;
        cluster_offset += s->cluster_size) {
        k = cluster_offset >> s->cluster_bits;
        if (k >= *refcount_table_size) {
            ret = realloc_refcount_array(s, refcount_table,
                                         refcount_table_size, k + 1);
            if (ret < 0) {
                res->check_errors++;
                return ret;
            }
        }

        refcount = s->get_refcount(*refcount_table, k);
        if (refcount == s->refcount_max) {
            fprintf(stderr, "ERROR: overflow cluster offset=0x%" PRIx64
                    "\n", cluster_offset);
            fprintf(stderr, "Increase the refcount entry width or create a "
                    "clean copy if the image cannot be opened for writing\n");
            res->corruptions++;
            continue;
        }
        s->set_refcount(*refcount_table, k, refcount + 1);
    }

    return 0;
}

enum {
    CHECK_FRAG_INFO = 0x2,      /* update BlockFragInfo counters */
};

static int coroutine_fn GRAPH_RDLOCK
fix_l2_entry_by_zero(BlockDriverState *bs, BdrvCheckResult *res,
                     uint64_t l2_offset, uint64_t *l2_table,
                     int l2_index, bool active,
                     bool *metadata_overlap)
{
    BDRVQcow2State *s = bs->opaque;
    int ret;
    int idx = l2_index * (l2_entry_size(s) / sizeof(uint64_t));
    uint64_t l2e_offset = l2_offset + (uint64_t)l2_index * l2_entry_size(s);
    int ign = active ? QCOW2_OL_ACTIVE_L2 : QCOW2_OL_INACTIVE_L2;

    if (has_subclusters(s)) {
        uint64_t l2_bitmap = get_l2_bitmap(s, l2_table, l2_index);

        l2_bitmap |= l2_bitmap << 32;
        l2_bitmap &= QCOW_L2_BITMAP_ALL_ZEROES;

        set_l2_bitmap(s, l2_table, l2_index, l2_bitmap);
        set_l2_entry(s, l2_table, l2_index, 0);
    } else {
        set_l2_entry(s, l2_table, l2_index, QCOW_OFLAG_ZERO);
    }

    ret = qcow2_pre_write_overlap_check(bs, ign, l2e_offset, l2_entry_size(s),
                                        false);
    if (metadata_overlap) {
        *metadata_overlap = ret < 0;
    }
    if (ret < 0) {
        fprintf(stderr, "ERROR: Overlap check failed\n");
        goto fail;
    }

    ret = bdrv_co_pwrite_sync(bs->file, l2e_offset, l2_entry_size(s),
                              &l2_table[idx], 0);
    if (ret < 0) {
        fprintf(stderr, "ERROR: Failed to overwrite L2 "
                "table entry: %s\n", strerror(-ret));
        goto fail;
    }

    res->corruptions--;
    res->corruptions_fixed++;
    return 0;

fail:
    res->check_errors++;
    return ret;
}

static int coroutine_fn GRAPH_RDLOCK
check_refcounts_l2(BlockDriverState *bs, BdrvCheckResult *res,
                   void **refcount_table,
                   int64_t *refcount_table_size, int64_t l2_offset,
                   int flags, BdrvCheckMode fix, bool active)
{
    BDRVQcow2State *s = bs->opaque;
    uint64_t l2_entry, l2_bitmap;
    uint64_t next_contiguous_offset = 0;
    int i, ret;
    size_t l2_size_bytes = s->l2_size * l2_entry_size(s);
    g_autofree uint64_t *l2_table = g_malloc(l2_size_bytes);
    bool metadata_overlap;

    ret = bdrv_co_pread(bs->file, l2_offset, l2_size_bytes, l2_table, 0);
    if (ret < 0) {
        fprintf(stderr, "ERROR: I/O error in check_refcounts_l2\n");
        res->check_errors++;
        return ret;
    }

    for (i = 0; i < s->l2_size; i++) {
        uint64_t coffset;
        int csize;
        QCow2ClusterType type;

        l2_entry = get_l2_entry(s, l2_table, i);
        l2_bitmap = get_l2_bitmap(s, l2_table, i);
        type = qcow2_get_cluster_type(bs, l2_entry);

        if (type != QCOW2_CLUSTER_COMPRESSED) {
            if (l2_entry & L2E_STD_RESERVED_MASK) {
                fprintf(stderr, "ERROR found l2 entry with reserved bits set: "
                        "%" PRIx64 "\n", l2_entry);
                res->corruptions++;
            }
        }

        switch (type) {
        case QCOW2_CLUSTER_COMPRESSED:
            if (l2_entry & QCOW_OFLAG_COPIED) {
                fprintf(stderr, "ERROR: coffset=0x%" PRIx64 ": "
                    "copied flag must never be set for compressed "
                    "clusters\n", l2_entry & s->cluster_offset_mask);
                l2_entry &= ~QCOW_OFLAG_COPIED;
                res->corruptions++;
            }

            if (has_data_file(bs)) {
                fprintf(stderr, "ERROR compressed cluster %d with data file, "
                        "entry=0x%" PRIx64 "\n", i, l2_entry);
                res->corruptions++;
                break;
            }

            if (l2_bitmap) {
                fprintf(stderr, "ERROR compressed cluster %d with non-zero "
                        "subcluster allocation bitmap, entry=0x%" PRIx64 "\n",
                        i, l2_entry);
                res->corruptions++;
                break;
            }

            qcow2_parse_compressed_l2_entry(bs, l2_entry, &coffset, &csize);
            ret = qcow2_inc_refcounts_imrt(
                bs, res, refcount_table, refcount_table_size, coffset, csize);
            if (ret < 0) {
                return ret;
            }

            if (flags & CHECK_FRAG_INFO) {
                res->bfi.allocated_clusters++;
                res->bfi.compressed_clusters++;

                res->bfi.fragmented_clusters++;
            }
            break;

        case QCOW2_CLUSTER_ZERO_ALLOC:
        case QCOW2_CLUSTER_NORMAL:
        {
            uint64_t offset = l2_entry & L2E_OFFSET_MASK;

            if ((l2_bitmap >> 32) & l2_bitmap) {
                res->corruptions++;
                fprintf(stderr, "ERROR offset=%" PRIx64 ": Allocated "
                        "cluster has corrupted subcluster allocation bitmap\n",
                        offset);
            }

            if (offset_into_cluster(s, offset)) {
                bool contains_data;
                res->corruptions++;

                if (has_subclusters(s)) {
                    contains_data = (l2_bitmap & QCOW_L2_BITMAP_ALL_ALLOC);
                } else {
                    contains_data = !(l2_entry & QCOW_OFLAG_ZERO);
                }

                if (!contains_data) {
                    fprintf(stderr, "%s offset=%" PRIx64 ": Preallocated "
                            "cluster is not properly aligned; L2 entry "
                            "corrupted.\n",
                            fix & BDRV_FIX_ERRORS ? "Repairing" : "ERROR",
                            offset);
                    if (fix & BDRV_FIX_ERRORS) {
                        ret = fix_l2_entry_by_zero(bs, res, l2_offset,
                                                   l2_table, i, active,
                                                   &metadata_overlap);
                        if (metadata_overlap) {
                            return ret;
                        }

                        if (ret == 0) {
                            continue;
                        }

                    }
                } else {
                    fprintf(stderr, "ERROR offset=%" PRIx64 ": Data cluster is "
                        "not properly aligned; L2 entry corrupted.\n", offset);
                }
            }

            if (flags & CHECK_FRAG_INFO) {
                res->bfi.allocated_clusters++;
                if (next_contiguous_offset &&
                    offset != next_contiguous_offset) {
                    res->bfi.fragmented_clusters++;
                }
                next_contiguous_offset = offset + s->cluster_size;
            }

            if (!has_data_file(bs)) {
                ret = qcow2_inc_refcounts_imrt(bs, res, refcount_table,
                                               refcount_table_size,
                                               offset, s->cluster_size);
                if (ret < 0) {
                    return ret;
                }
            }
            break;
        }

        case QCOW2_CLUSTER_ZERO_PLAIN:
            assert(!l2_bitmap);
            break;

        case QCOW2_CLUSTER_UNALLOCATED:
            if (l2_bitmap & QCOW_L2_BITMAP_ALL_ALLOC) {
                res->corruptions++;
                fprintf(stderr, "ERROR: Unallocated "
                        "cluster has non-zero subcluster allocation map\n");
            }
            break;

        default:
            abort();
        }
    }

    return 0;
}

static int coroutine_fn GRAPH_RDLOCK
check_refcounts_l1(BlockDriverState *bs, BdrvCheckResult *res,
                   void **refcount_table, int64_t *refcount_table_size,
                   int64_t l1_table_offset, int l1_size,
                   int flags, BdrvCheckMode fix, bool active)
{
    BDRVQcow2State *s = bs->opaque;
    size_t l1_size_bytes = l1_size * L1E_SIZE;
    g_autofree uint64_t *l1_table = NULL;
    uint64_t l2_offset;
    int i, ret;

    if (!l1_size) {
        return 0;
    }

    ret = qcow2_inc_refcounts_imrt(bs, res, refcount_table, refcount_table_size,
                                   l1_table_offset, l1_size_bytes);
    if (ret < 0) {
        return ret;
    }

    l1_table = g_try_malloc(l1_size_bytes);
    if (l1_table == NULL) {
        res->check_errors++;
        return -ENOMEM;
    }

    ret = bdrv_co_pread(bs->file, l1_table_offset, l1_size_bytes, l1_table, 0);
    if (ret < 0) {
        fprintf(stderr, "ERROR: I/O error in check_refcounts_l1\n");
        res->check_errors++;
        return ret;
    }

    for (i = 0; i < l1_size; i++) {
        be64_to_cpus(&l1_table[i]);
    }

    for (i = 0; i < l1_size; i++) {
        if (!l1_table[i]) {
            continue;
        }

        if (l1_table[i] & L1E_RESERVED_MASK) {
            fprintf(stderr, "ERROR found L1 entry with reserved bits set: "
                    "%" PRIx64 "\n", l1_table[i]);
            res->corruptions++;
        }

        l2_offset = l1_table[i] & L1E_OFFSET_MASK;

        ret = qcow2_inc_refcounts_imrt(bs, res,
                                       refcount_table, refcount_table_size,
                                       l2_offset, s->cluster_size);
        if (ret < 0) {
            return ret;
        }

        if (offset_into_cluster(s, l2_offset)) {
            fprintf(stderr, "ERROR l2_offset=%" PRIx64 ": Table is not "
                "cluster aligned; L1 entry corrupted\n", l2_offset);
            res->corruptions++;
        }

        ret = check_refcounts_l2(bs, res, refcount_table,
                                 refcount_table_size, l2_offset, flags,
                                 fix, active);
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}

static int coroutine_fn GRAPH_RDLOCK
check_oflag_copied(BlockDriverState *bs, BdrvCheckResult *res, BdrvCheckMode fix)
{
    BDRVQcow2State *s = bs->opaque;
    uint64_t *l2_table = qemu_blockalign(bs, s->cluster_size);
    int ret;
    uint64_t refcount;
    int i, j;
    bool repair;

    if (fix & BDRV_FIX_ERRORS) {
        repair = true;
    } else if (fix & BDRV_FIX_LEAKS) {
        repair = !res->check_errors && !res->corruptions && !res->leaks;
    } else {
        repair = false;
    }

    for (i = 0; i < s->l1_size; i++) {
        uint64_t l1_entry = s->l1_table[i];
        uint64_t l2_offset = l1_entry & L1E_OFFSET_MASK;
        int l2_dirty = 0;

        if (!l2_offset) {
            continue;
        }

        ret = qcow2_get_refcount(bs, l2_offset >> s->cluster_bits,
                                 &refcount);
        if (ret < 0) {
            continue;
        }
        if ((refcount == 1) != ((l1_entry & QCOW_OFLAG_COPIED) != 0)) {
            res->corruptions++;
            fprintf(stderr, "%s OFLAG_COPIED L2 cluster: l1_index=%d "
                    "l1_entry=%" PRIx64 " refcount=%" PRIu64 "\n",
                    repair ? "Repairing" : "ERROR", i, l1_entry, refcount);
            if (repair) {
                s->l1_table[i] = refcount == 1
                               ? l1_entry |  QCOW_OFLAG_COPIED
                               : l1_entry & ~QCOW_OFLAG_COPIED;
                ret = qcow2_write_l1_entry(bs, i);
                if (ret < 0) {
                    res->check_errors++;
                    goto fail;
                }
                res->corruptions--;
                res->corruptions_fixed++;
            }
        }

        ret = bdrv_co_pread(bs->file, l2_offset, s->l2_size * l2_entry_size(s),
                            l2_table, 0);
        if (ret < 0) {
            fprintf(stderr, "ERROR: Could not read L2 table: %s\n",
                    strerror(-ret));
            res->check_errors++;
            goto fail;
        }

        for (j = 0; j < s->l2_size; j++) {
            uint64_t l2_entry = get_l2_entry(s, l2_table, j);
            uint64_t data_offset = l2_entry & L2E_OFFSET_MASK;
            QCow2ClusterType cluster_type = qcow2_get_cluster_type(bs, l2_entry);

            if (cluster_type == QCOW2_CLUSTER_NORMAL ||
                cluster_type == QCOW2_CLUSTER_ZERO_ALLOC) {
                if (has_data_file(bs)) {
                    refcount = 1;
                } else {
                    ret = qcow2_get_refcount(bs,
                                             data_offset >> s->cluster_bits,
                                             &refcount);
                    if (ret < 0) {
                        continue;
                    }
                }
                if ((refcount == 1) != ((l2_entry & QCOW_OFLAG_COPIED) != 0)) {
                    res->corruptions++;
                    fprintf(stderr, "%s OFLAG_COPIED data cluster: "
                            "l2_entry=%" PRIx64 " refcount=%" PRIu64 "\n",
                            repair ? "Repairing" : "ERROR", l2_entry, refcount);
                    if (repair) {
                        set_l2_entry(s, l2_table, j,
                                     refcount == 1 ?
                                     l2_entry |  QCOW_OFLAG_COPIED :
                                     l2_entry & ~QCOW_OFLAG_COPIED);
                        l2_dirty++;
                    }
                }
            }
        }

        if (l2_dirty > 0) {
            ret = qcow2_pre_write_overlap_check(bs, QCOW2_OL_ACTIVE_L2,
                                                l2_offset, s->cluster_size,
                                                false);
            if (ret < 0) {
                fprintf(stderr, "ERROR: Could not write L2 table; metadata "
                        "overlap check failed: %s\n", strerror(-ret));
                res->check_errors++;
                goto fail;
            }

            ret = bdrv_co_pwrite(bs->file, l2_offset, s->cluster_size, l2_table, 0);
            if (ret < 0) {
                fprintf(stderr, "ERROR: Could not write L2 table: %s\n",
                        strerror(-ret));
                res->check_errors++;
                goto fail;
            }
            res->corruptions -= l2_dirty;
            res->corruptions_fixed += l2_dirty;
        }
    }

    ret = 0;

fail:
    qemu_vfree(l2_table);
    return ret;
}

static int coroutine_fn GRAPH_RDLOCK
check_refblocks(BlockDriverState *bs, BdrvCheckResult *res,
                BdrvCheckMode fix, bool *rebuild,
                void **refcount_table, int64_t *nb_clusters)
{
    BDRVQcow2State *s = bs->opaque;
    int64_t i, size;
    int ret;

    for(i = 0; i < s->refcount_table_size; i++) {
        uint64_t offset, cluster;
        offset = s->refcount_table[i] & REFT_OFFSET_MASK;
        cluster = offset >> s->cluster_bits;

        if (s->refcount_table[i] & REFT_RESERVED_MASK) {
            fprintf(stderr, "ERROR refcount table entry %" PRId64 " has "
                    "reserved bits set\n", i);
            res->corruptions++;
            *rebuild = true;
            continue;
        }

        if (offset_into_cluster(s, offset)) {
            fprintf(stderr, "ERROR refcount block %" PRId64 " is not "
                "cluster aligned; refcount table entry corrupted\n", i);
            res->corruptions++;
            *rebuild = true;
            continue;
        }

        if (cluster >= *nb_clusters) {
            res->corruptions++;
            fprintf(stderr, "%s refcount block %" PRId64 " is outside image\n",
                    fix & BDRV_FIX_ERRORS ? "Repairing" : "ERROR", i);

            if (fix & BDRV_FIX_ERRORS) {
                int64_t new_nb_clusters;
                Error *local_err = NULL;

                if (offset > INT64_MAX - s->cluster_size) {
                    ret = -EINVAL;
                    goto resize_fail;
                }

                ret = bdrv_co_truncate(bs->file, offset + s->cluster_size, false,
                                       PREALLOC_MODE_OFF, 0, &local_err);
                if (ret < 0) {
                    error_report_err(local_err);
                    goto resize_fail;
                }
                size = bdrv_co_getlength(bs->file->bs);
                if (size < 0) {
                    ret = size;
                    goto resize_fail;
                }

                new_nb_clusters = size_to_clusters(s, size);
                assert(new_nb_clusters >= *nb_clusters);

                ret = realloc_refcount_array(s, refcount_table,
                                             nb_clusters, new_nb_clusters);
                if (ret < 0) {
                    res->check_errors++;
                    return ret;
                }

                if (cluster >= *nb_clusters) {
                    ret = -EINVAL;
                    goto resize_fail;
                }

                res->corruptions--;
                res->corruptions_fixed++;
                ret = qcow2_inc_refcounts_imrt(bs, res,
                                               refcount_table, nb_clusters,
                                               offset, s->cluster_size);
                if (ret < 0) {
                    return ret;
                }
                continue;

resize_fail:
                *rebuild = true;
                fprintf(stderr, "ERROR could not resize image: %s\n",
                        strerror(-ret));
            }
            continue;
        }

        if (offset != 0) {
            ret = qcow2_inc_refcounts_imrt(bs, res, refcount_table, nb_clusters,
                                           offset, s->cluster_size);
            if (ret < 0) {
                return ret;
            }
            if (s->get_refcount(*refcount_table, cluster) != 1) {
                fprintf(stderr, "ERROR refcount block %" PRId64
                        " refcount=%" PRIu64 "\n", i,
                        s->get_refcount(*refcount_table, cluster));
                res->corruptions++;
                *rebuild = true;
            }
        }
    }

    return 0;
}

static int coroutine_fn GRAPH_RDLOCK
calculate_refcounts(BlockDriverState *bs, BdrvCheckResult *res,
                    BdrvCheckMode fix, bool *rebuild,
                    void **refcount_table, int64_t *nb_clusters)
{
    BDRVQcow2State *s = bs->opaque;
    int ret;

    if (!*refcount_table) {
        int64_t old_size = 0;
        ret = realloc_refcount_array(s, refcount_table,
                                     &old_size, *nb_clusters);
        if (ret < 0) {
            res->check_errors++;
            return ret;
        }
    }

    ret = qcow2_inc_refcounts_imrt(bs, res, refcount_table, nb_clusters,
                                   0, s->cluster_size);
    if (ret < 0) {
        return ret;
    }

    ret = check_refcounts_l1(bs, res, refcount_table, nb_clusters,
                             s->l1_table_offset, s->l1_size, CHECK_FRAG_INFO,
                             fix, true);
    if (ret < 0) {
        return ret;
    }

    ret = qcow2_inc_refcounts_imrt(bs, res, refcount_table, nb_clusters,
                                   s->refcount_table_offset,
                                   s->refcount_table_size *
                                   REFTABLE_ENTRY_SIZE);
    if (ret < 0) {
        return ret;
    }

    return check_refblocks(bs, res, fix, rebuild, refcount_table, nb_clusters);
}

static void coroutine_fn GRAPH_RDLOCK
compare_refcounts(BlockDriverState *bs, BdrvCheckResult *res,
                  BdrvCheckMode fix, bool *rebuild,
                  int64_t *highest_cluster,
                  void *refcount_table, int64_t nb_clusters)
{
    BDRVQcow2State *s = bs->opaque;
    int64_t i;
    uint64_t refcount1, refcount2;
    int ret;

    for (i = 0, *highest_cluster = 0; i < nb_clusters; i++) {
        ret = qcow2_get_refcount(bs, i, &refcount1);
        if (ret < 0) {
            fprintf(stderr, "Can't get refcount for cluster %" PRId64 ": %s\n",
                    i, strerror(-ret));
            res->check_errors++;
            continue;
        }

        refcount2 = s->get_refcount(refcount_table, i);

        if (refcount1 > 0 || refcount2 > 0) {
            *highest_cluster = i;
        }

        if (refcount1 != refcount2) {
            int *num_fixed = NULL;
            if (refcount1 == 0) {
                *rebuild = true;
            } else if (refcount1 > refcount2 && (fix & BDRV_FIX_LEAKS)) {
                num_fixed = &res->leaks_fixed;
            } else if (refcount1 < refcount2 && (fix & BDRV_FIX_ERRORS)) {
                num_fixed = &res->corruptions_fixed;
            }

            fprintf(stderr, "%s cluster %" PRId64 " refcount=%" PRIu64
                    " reference=%" PRIu64 "\n",
                   num_fixed != NULL     ? "Repairing" :
                   refcount1 < refcount2 ? "ERROR" :
                                           "Leaked",
                   i, refcount1, refcount2);

            if (num_fixed) {
                ret = update_refcount(bs, i << s->cluster_bits, 1,
                                      refcount_diff(refcount1, refcount2),
                                      refcount1 > refcount2,
                                      QCOW2_DISCARD_ALWAYS);
                if (ret >= 0) {
                    (*num_fixed)++;
                    continue;
                }
            }

            if (refcount1 < refcount2) {
                res->corruptions++;
            } else {
                res->leaks++;
            }
        }
    }
}

static int64_t alloc_clusters_imrt(BlockDriverState *bs,
                                   int cluster_count,
                                   void **refcount_table,
                                   int64_t *imrt_nb_clusters,
                                   int64_t *first_free_cluster)
{
    BDRVQcow2State *s = bs->opaque;
    int64_t cluster = *first_free_cluster, i;
    bool first_gap = true;
    int contiguous_free_clusters;
    int ret;

    for (contiguous_free_clusters = 0;
         cluster < *imrt_nb_clusters &&
         contiguous_free_clusters < cluster_count;
         cluster++)
    {
        if (!s->get_refcount(*refcount_table, cluster)) {
            contiguous_free_clusters++;
            if (first_gap) {
                *first_free_cluster = cluster;
                first_gap = false;
            }
        } else if (contiguous_free_clusters) {
            contiguous_free_clusters = 0;
        }
    }


    if (contiguous_free_clusters < cluster_count) {
        ret = realloc_refcount_array(s, refcount_table, imrt_nb_clusters,
                                     cluster + cluster_count
                                     - contiguous_free_clusters);
        if (ret < 0) {
            return ret;
        }
    }

    cluster -= contiguous_free_clusters;
    for (i = 0; i < cluster_count; i++) {
        s->set_refcount(*refcount_table, cluster + i, 1);
    }

    return cluster << s->cluster_bits;
}

static int coroutine_fn GRAPH_RDLOCK
rebuild_refcounts_write_refblocks(
        BlockDriverState *bs, void **refcount_table, int64_t *nb_clusters,
        int64_t first_cluster, int64_t end_cluster,
        uint64_t **on_disk_reftable_ptr, uint32_t *on_disk_reftable_entries_ptr,
        Error **errp
    )
{
    BDRVQcow2State *s = bs->opaque;
    int64_t cluster;
    int64_t refblock_offset, refblock_start, refblock_index;
    int64_t first_free_cluster = 0;
    uint64_t *on_disk_reftable = *on_disk_reftable_ptr;
    uint32_t on_disk_reftable_entries = *on_disk_reftable_entries_ptr;
    void *on_disk_refblock;
    bool reftable_grown = false;
    int ret;

    for (cluster = first_cluster; cluster < end_cluster; cluster++) {
        if (!s->get_refcount(*refcount_table, cluster)) {
            continue;
        }


        refblock_index = cluster >> s->refcount_block_bits;
        refblock_start = refblock_index << s->refcount_block_bits;

        if (on_disk_reftable_entries > refblock_index &&
            on_disk_reftable[refblock_index])
        {
            refblock_offset = on_disk_reftable[refblock_index];
        } else {
            int64_t refblock_cluster_index;

            if (first_free_cluster < refblock_start) {
                first_free_cluster = refblock_start;
            }
            refblock_offset = alloc_clusters_imrt(bs, 1, refcount_table,
                                                  nb_clusters,
                                                  &first_free_cluster);
            if (refblock_offset < 0) {
                error_setg_errno(errp, -refblock_offset,
                                 "ERROR allocating refblock");
                return refblock_offset;
            }

            refblock_cluster_index = refblock_offset / s->cluster_size;
            if (refblock_cluster_index >= end_cluster) {
                end_cluster = refblock_cluster_index + 1;
            }

            if (on_disk_reftable_entries <= refblock_index) {
                on_disk_reftable_entries =
                    ROUND_UP((refblock_index + 1) * REFTABLE_ENTRY_SIZE,
                             s->cluster_size) / REFTABLE_ENTRY_SIZE;
                on_disk_reftable =
                    g_try_realloc(on_disk_reftable,
                                  on_disk_reftable_entries *
                                  REFTABLE_ENTRY_SIZE);
                if (!on_disk_reftable) {
                    error_setg(errp, "ERROR allocating reftable memory");
                    return -ENOMEM;
                }

                memset(on_disk_reftable + *on_disk_reftable_entries_ptr, 0,
                       (on_disk_reftable_entries -
                        *on_disk_reftable_entries_ptr) *
                       REFTABLE_ENTRY_SIZE);

                *on_disk_reftable_ptr = on_disk_reftable;
                *on_disk_reftable_entries_ptr = on_disk_reftable_entries;

                reftable_grown = true;
            } else {
                assert(on_disk_reftable);
            }
            on_disk_reftable[refblock_index] = refblock_offset;
        }


        ret = qcow2_pre_write_overlap_check(bs, 0, refblock_offset,
                                            s->cluster_size, false);
        if (ret < 0) {
            error_setg_errno(errp, -ret, "ERROR writing refblock");
            return ret;
        }

        on_disk_refblock = (void *)((char *) *refcount_table +
                                    refblock_index * s->cluster_size);

        ret = bdrv_co_pwrite(bs->file, refblock_offset, s->cluster_size,
                             on_disk_refblock, 0);
        if (ret < 0) {
            error_setg_errno(errp, -ret, "ERROR writing refblock");
            return ret;
        }

        cluster = refblock_start + s->refcount_block_size - 1;
    }

    return reftable_grown;
}

static int coroutine_fn GRAPH_RDLOCK
rebuild_refcount_structure(BlockDriverState *bs, BdrvCheckResult *res,
                           void **refcount_table, int64_t *nb_clusters,
                           Error **errp)
{
    BDRVQcow2State *s = bs->opaque;
    int64_t reftable_offset = -1;
    int64_t reftable_length = 0;
    int64_t reftable_clusters;
    int64_t refblock_index;
    uint32_t on_disk_reftable_entries = 0;
    uint64_t *on_disk_reftable = NULL;
    int ret = 0;
    int reftable_size_changed = 0;
    struct {
        uint64_t reftable_offset;
        uint32_t reftable_clusters;
    } QEMU_PACKED reftable_offset_and_clusters;

    qcow2_cache_empty(bs, s->refcount_block_cache);


    reftable_size_changed =
        rebuild_refcounts_write_refblocks(bs, refcount_table, nb_clusters,
                                          0, *nb_clusters,
                                          &on_disk_reftable,
                                          &on_disk_reftable_entries, errp);
    if (reftable_size_changed < 0) {
        res->check_errors++;
        ret = reftable_size_changed;
        goto fail;
    }

    assert(reftable_size_changed);

    do {
        int64_t reftable_start_cluster, reftable_end_cluster;
        int64_t first_free_cluster = 0;

        reftable_length = on_disk_reftable_entries * REFTABLE_ENTRY_SIZE;
        reftable_clusters = size_to_clusters(s, reftable_length);

        reftable_offset = alloc_clusters_imrt(bs, reftable_clusters,
                                              refcount_table, nb_clusters,
                                              &first_free_cluster);
        if (reftable_offset < 0) {
            error_setg_errno(errp, -reftable_offset,
                             "ERROR allocating reftable");
            res->check_errors++;
            ret = reftable_offset;
            goto fail;
        }

        assert(offset_into_cluster(s, reftable_offset) == 0);
        reftable_start_cluster = reftable_offset / s->cluster_size;
        reftable_end_cluster = reftable_start_cluster + reftable_clusters;
        reftable_size_changed =
            rebuild_refcounts_write_refblocks(bs, refcount_table, nb_clusters,
                                              reftable_start_cluster,
                                              reftable_end_cluster,
                                              &on_disk_reftable,
                                              &on_disk_reftable_entries, errp);
        if (reftable_size_changed < 0) {
            res->check_errors++;
            ret = reftable_size_changed;
            goto fail;
        }

    } while (reftable_size_changed);

    assert(reftable_offset >= 0);


    for (refblock_index = 0; refblock_index < on_disk_reftable_entries;
         refblock_index++)
    {
        cpu_to_be64s(&on_disk_reftable[refblock_index]);
    }

    ret = qcow2_pre_write_overlap_check(bs, 0, reftable_offset, reftable_length,
                                        false);
    if (ret < 0) {
        error_setg_errno(errp, -ret, "ERROR writing reftable");
        goto fail;
    }

    assert(reftable_length < INT_MAX);
    ret = bdrv_co_pwrite(bs->file, reftable_offset, reftable_length,
                         on_disk_reftable, 0);
    if (ret < 0) {
        error_setg_errno(errp, -ret, "ERROR writing reftable");
        goto fail;
    }

    reftable_offset_and_clusters.reftable_offset = cpu_to_be64(reftable_offset);
    reftable_offset_and_clusters.reftable_clusters =
        cpu_to_be32(reftable_clusters);
    ret = bdrv_co_pwrite_sync(bs->file,
                              offsetof(QCowHeader, refcount_table_offset),
                              sizeof(reftable_offset_and_clusters),
                              &reftable_offset_and_clusters, 0);
    if (ret < 0) {
        error_setg_errno(errp, -ret, "ERROR setting reftable");
        goto fail;
    }

    for (refblock_index = 0; refblock_index < on_disk_reftable_entries;
         refblock_index++)
    {
        be64_to_cpus(&on_disk_reftable[refblock_index]);
    }
    s->refcount_table = on_disk_reftable;
    s->refcount_table_offset = reftable_offset;
    s->refcount_table_size = on_disk_reftable_entries;
    update_max_refcount_table_index(s);

    return 0;

fail:
    g_free(on_disk_reftable);
    return ret;
}

int coroutine_fn GRAPH_RDLOCK
qcow2_check_refcounts(BlockDriverState *bs, BdrvCheckResult *res, BdrvCheckMode fix)
{
    BDRVQcow2State *s = bs->opaque;
    BdrvCheckResult pre_compare_res;
    int64_t size, highest_cluster, nb_clusters;
    void *refcount_table = NULL;
    bool rebuild = false;
    int ret;

    size = bdrv_co_getlength(bs->file->bs);
    if (size < 0) {
        res->check_errors++;
        return size;
    }

    nb_clusters = size_to_clusters(s, size);
    if (nb_clusters > INT_MAX) {
        res->check_errors++;
        return -EFBIG;
    }

    res->bfi.total_clusters =
        size_to_clusters(s, bs->total_sectors * BDRV_SECTOR_SIZE);

    ret = calculate_refcounts(bs, res, fix, &rebuild, &refcount_table,
                              &nb_clusters);
    if (ret < 0) {
        goto fail;
    }

    pre_compare_res = *res;
    compare_refcounts(bs, res, 0, &rebuild, &highest_cluster, refcount_table,
                      nb_clusters);

    if (rebuild && (fix & BDRV_FIX_ERRORS)) {
        BdrvCheckResult old_res = *res;
        int fresh_leaks = 0;
        Error *local_err = NULL;

        fprintf(stderr, "Rebuilding refcount structure\n");
        ret = rebuild_refcount_structure(bs, res, &refcount_table,
                                         &nb_clusters, &local_err);
        if (ret < 0) {
            error_report_err(local_err);
            goto fail;
        }

        res->corruptions = 0;
        res->leaks = 0;

        rebuild = false;
        memset(refcount_table, 0, refcount_array_byte_size(s, nb_clusters));
        ret = calculate_refcounts(bs, res, 0, &rebuild, &refcount_table,
                                  &nb_clusters);
        if (ret < 0) {
            goto fail;
        }

        if (fix & BDRV_FIX_LEAKS) {
            BdrvCheckResult saved_res = *res;
            *res = (BdrvCheckResult){ 0 };

            compare_refcounts(bs, res, BDRV_FIX_LEAKS, &rebuild,
                              &highest_cluster, refcount_table, nb_clusters);
            if (rebuild) {
                fprintf(stderr, "ERROR rebuilt refcount structure is still "
                        "broken\n");
            }

            fresh_leaks = res->leaks;
            *res = saved_res;
        }

        if (res->corruptions < old_res.corruptions) {
            res->corruptions_fixed += old_res.corruptions - res->corruptions;
        }
        if (res->leaks < old_res.leaks) {
            res->leaks_fixed += old_res.leaks - res->leaks;
        }
        res->leaks += fresh_leaks;
    } else if (fix) {
        if (rebuild) {
            fprintf(stderr, "ERROR need to rebuild refcount structures\n");
            res->check_errors++;
            ret = -EIO;
            goto fail;
        }

        if (res->leaks || res->corruptions) {
            *res = pre_compare_res;
            compare_refcounts(bs, res, fix, &rebuild, &highest_cluster,
                              refcount_table, nb_clusters);
        }
    }

    ret = check_oflag_copied(bs, res, fix);
    if (ret < 0) {
        goto fail;
    }

    res->image_end_offset = (highest_cluster + 1) * s->cluster_size;
    ret = 0;

fail:
    g_free(refcount_table);

    return ret;
}

#define overlaps_with(ofs, sz) \
    ranges_overlap(offset, size, ofs, sz)

int qcow2_check_metadata_overlap(BlockDriverState *bs, int ign, int64_t offset,
                                 int64_t size)
{
    BDRVQcow2State *s = bs->opaque;
    int chk = s->overlap_check & ~ign;
    int i, j;

    if (!size) {
        return 0;
    }

    if (chk & QCOW2_OL_MAIN_HEADER) {
        if (offset < s->cluster_size) {
            return QCOW2_OL_MAIN_HEADER;
        }
    }

    size = ROUND_UP(offset_into_cluster(s, offset) + size, s->cluster_size);
    offset = start_of_cluster(s, offset);

    if ((chk & QCOW2_OL_ACTIVE_L1) && s->l1_size) {
        if (overlaps_with(s->l1_table_offset, s->l1_size * L1E_SIZE)) {
            return QCOW2_OL_ACTIVE_L1;
        }
    }

    if ((chk & QCOW2_OL_REFCOUNT_TABLE) && s->refcount_table_size) {
        if (overlaps_with(s->refcount_table_offset,
            s->refcount_table_size * REFTABLE_ENTRY_SIZE)) {
            return QCOW2_OL_REFCOUNT_TABLE;
        }
    }

    if ((chk & QCOW2_OL_ACTIVE_L2) && s->l1_table) {
        for (i = 0; i < s->l1_size; i++) {
            if ((s->l1_table[i] & L1E_OFFSET_MASK) &&
                overlaps_with(s->l1_table[i] & L1E_OFFSET_MASK,
                s->cluster_size)) {
                return QCOW2_OL_ACTIVE_L2;
            }
        }
    }

    if ((chk & QCOW2_OL_REFCOUNT_BLOCK) && s->refcount_table) {
        unsigned last_entry = s->max_refcount_table_index;
        assert(last_entry < s->refcount_table_size);
        assert(last_entry + 1 == s->refcount_table_size ||
               (s->refcount_table[last_entry + 1] & REFT_OFFSET_MASK) == 0);
        for (i = 0; i <= last_entry; i++) {
            if ((s->refcount_table[i] & REFT_OFFSET_MASK) &&
                overlaps_with(s->refcount_table[i] & REFT_OFFSET_MASK,
                s->cluster_size)) {
                return QCOW2_OL_REFCOUNT_BLOCK;
            }
        }
    }

    if ((chk & QCOW2_OL_BITMAP_DIRECTORY) &&
        (s->autoclear_features & QCOW2_AUTOCLEAR_BITMAPS))
    {
        if (overlaps_with(s->bitmap_directory_offset,
                          s->bitmap_directory_size))
        {
            return QCOW2_OL_BITMAP_DIRECTORY;
        }
    }

    return 0;
}

static const char *metadata_ol_names[] = {
    [QCOW2_OL_MAIN_HEADER_BITNR]        = "qcow2_header",
    [QCOW2_OL_ACTIVE_L1_BITNR]          = "active L1 table",
    [QCOW2_OL_ACTIVE_L2_BITNR]          = "active L2 table",
    [QCOW2_OL_REFCOUNT_TABLE_BITNR]     = "refcount table",
    [QCOW2_OL_REFCOUNT_BLOCK_BITNR]     = "refcount block",
    [QCOW2_OL_UNUSED_METADATA_BITNR]     = "unused metadata",
    [QCOW2_OL_INACTIVE_L1_BITNR]        = "inactive L1 table",
    [QCOW2_OL_INACTIVE_L2_BITNR]        = "inactive L2 table",
    [QCOW2_OL_BITMAP_DIRECTORY_BITNR]   = "bitmap directory",
};
QEMU_BUILD_BUG_ON(QCOW2_OL_MAX_BITNR != ARRAY_SIZE(metadata_ol_names));

int qcow2_pre_write_overlap_check(BlockDriverState *bs, int ign, int64_t offset,
                                  int64_t size, bool data_file)
{
    int ret;

    if (data_file && has_data_file(bs)) {
        return 0;
    }

    ret = qcow2_check_metadata_overlap(bs, ign, offset, size);
    if (ret < 0) {
        return ret;
    } else if (ret > 0) {
        int metadata_ol_bitnr = ctz32(ret);
        assert(metadata_ol_bitnr < QCOW2_OL_MAX_BITNR);

        qcow2_signal_corruption(bs, true, offset, size, "Preventing invalid "
                                "write on metadata (overlaps with %s)",
                                metadata_ol_names[metadata_ol_bitnr]);
        return -EIO;
    }

    return 0;
}

typedef int /* GRAPH_RDLOCK_PTR */
    (RefblockFinishOp)(BlockDriverState *bs, uint64_t **reftable,
                       uint64_t reftable_index, uint64_t *reftable_size,
                       void *refblock, bool refblock_empty,
                       bool *allocated, Error **errp);

static int GRAPH_RDLOCK
alloc_refblock(BlockDriverState *bs, uint64_t **reftable,
               uint64_t reftable_index, uint64_t *reftable_size,
               void *refblock, bool refblock_empty, bool *allocated,
               Error **errp)
{
    BDRVQcow2State *s = bs->opaque;
    int64_t offset;

    if (!refblock_empty && reftable_index >= *reftable_size) {
        uint64_t *new_reftable;
        uint64_t new_reftable_size;

        new_reftable_size = ROUND_UP(reftable_index + 1,
                                     s->cluster_size / REFTABLE_ENTRY_SIZE);
        if (new_reftable_size > QCOW_MAX_REFTABLE_SIZE / REFTABLE_ENTRY_SIZE) {
            error_setg(errp,
                       "This operation would make the refcount table grow "
                       "beyond the maximum size supported by QEMU, aborting");
            return -ENOTSUP;
        }

        new_reftable = g_try_realloc(*reftable, new_reftable_size *
                                                REFTABLE_ENTRY_SIZE);
        if (!new_reftable) {
            error_setg(errp, "Failed to increase reftable buffer size");
            return -ENOMEM;
        }

        memset(new_reftable + *reftable_size, 0,
               (new_reftable_size - *reftable_size) * REFTABLE_ENTRY_SIZE);

        *reftable      = new_reftable;
        *reftable_size = new_reftable_size;
    }

    if (!refblock_empty && !(*reftable)[reftable_index]) {
        offset = qcow2_alloc_clusters(bs, s->cluster_size);
        if (offset < 0) {
            error_setg_errno(errp, -offset, "Failed to allocate refblock");
            return offset;
        }
        (*reftable)[reftable_index] = offset;
        *allocated = true;
    }

    return 0;
}

static int GRAPH_RDLOCK
flush_refblock(BlockDriverState *bs, uint64_t **reftable,
               uint64_t reftable_index, uint64_t *reftable_size,
               void *refblock, bool refblock_empty, bool *allocated,
               Error **errp)
{
    BDRVQcow2State *s = bs->opaque;
    int64_t offset;
    int ret;

    if (reftable_index < *reftable_size && (*reftable)[reftable_index]) {
        offset = (*reftable)[reftable_index];

        ret = qcow2_pre_write_overlap_check(bs, 0, offset, s->cluster_size,
                                            false);
        if (ret < 0) {
            error_setg_errno(errp, -ret, "Overlap check failed");
            return ret;
        }

        ret = bdrv_pwrite(bs->file, offset, s->cluster_size, refblock, 0);
        if (ret < 0) {
            error_setg_errno(errp, -ret, "Failed to write refblock");
            return ret;
        }
    } else {
        assert(refblock_empty);
    }

    return 0;
}

static int GRAPH_RDLOCK
walk_over_reftable(BlockDriverState *bs, uint64_t **new_reftable,
                   uint64_t *new_reftable_index,
                   uint64_t *new_reftable_size,
                   void *new_refblock, int new_refblock_size,
                   int new_refcount_bits,
                   RefblockFinishOp *operation, bool *allocated,
                   Qcow2SetRefcountFunc *new_set_refcount,
                   BlockDriverAmendStatusCB *status_cb,
                   void *cb_opaque, int index, int total,
                   Error **errp)
{
    BDRVQcow2State *s = bs->opaque;
    uint64_t reftable_index;
    bool new_refblock_empty = true;
    int refblock_index;
    int new_refblock_index = 0;
    int ret;

    for (reftable_index = 0; reftable_index < s->refcount_table_size;
         reftable_index++)
    {
        uint64_t refblock_offset = s->refcount_table[reftable_index]
                                 & REFT_OFFSET_MASK;

        status_cb(bs, (uint64_t)index * s->refcount_table_size + reftable_index,
                  (uint64_t)total * s->refcount_table_size, cb_opaque);

        if (refblock_offset) {
            void *refblock;

            if (offset_into_cluster(s, refblock_offset)) {
                qcow2_signal_corruption(bs, true, -1, -1, "Refblock offset %#"
                                        PRIx64 " unaligned (reftable index: %#"
                                        PRIx64 ")", refblock_offset,
                                        reftable_index);
                error_setg(errp,
                           "Image is corrupt (unaligned refblock offset)");
                return -EIO;
            }

            ret = qcow2_cache_get(bs, s->refcount_block_cache, refblock_offset,
                                  &refblock);
            if (ret < 0) {
                error_setg_errno(errp, -ret, "Failed to retrieve refblock");
                return ret;
            }

            for (refblock_index = 0; refblock_index < s->refcount_block_size;
                 refblock_index++)
            {
                uint64_t refcount;

                if (new_refblock_index >= new_refblock_size) {
                    ret = operation(bs, new_reftable, *new_reftable_index,
                                    new_reftable_size, new_refblock,
                                    new_refblock_empty, allocated, errp);
                    if (ret < 0) {
                        qcow2_cache_put(s->refcount_block_cache, &refblock);
                        return ret;
                    }

                    (*new_reftable_index)++;
                    new_refblock_index = 0;
                    new_refblock_empty = true;
                }

                refcount = s->get_refcount(refblock, refblock_index);
                if (new_refcount_bits < 64 && refcount >> new_refcount_bits) {
                    uint64_t offset;

                    qcow2_cache_put(s->refcount_block_cache, &refblock);

                    offset = ((reftable_index << s->refcount_block_bits)
                              + refblock_index) << s->cluster_bits;

                    error_setg(errp, "Cannot decrease refcount entry width to "
                               "%i bits: Cluster at offset %#" PRIx64 " has a "
                               "refcount of %" PRIu64, new_refcount_bits,
                               offset, refcount);
                    return -EINVAL;
                }

                if (new_set_refcount) {
                    new_set_refcount(new_refblock, new_refblock_index++,
                                     refcount);
                } else {
                    new_refblock_index++;
                }
                new_refblock_empty = new_refblock_empty && refcount == 0;
            }

            qcow2_cache_put(s->refcount_block_cache, &refblock);
        } else {
            for (refblock_index = 0; refblock_index < s->refcount_block_size;
                 refblock_index++)
            {
                if (new_refblock_index >= new_refblock_size) {
                    ret = operation(bs, new_reftable, *new_reftable_index,
                                    new_reftable_size, new_refblock,
                                    new_refblock_empty, allocated, errp);
                    if (ret < 0) {
                        return ret;
                    }

                    (*new_reftable_index)++;
                    new_refblock_index = 0;
                    new_refblock_empty = true;
                }

                if (new_set_refcount) {
                    new_set_refcount(new_refblock, new_refblock_index++, 0);
                } else {
                    new_refblock_index++;
                }
            }
        }
    }

    if (new_refblock_index > 0) {
        if (new_set_refcount) {
            for (; new_refblock_index < new_refblock_size;
                 new_refblock_index++)
            {
                new_set_refcount(new_refblock, new_refblock_index, 0);
            }
        }

        ret = operation(bs, new_reftable, *new_reftable_index,
                        new_reftable_size, new_refblock, new_refblock_empty,
                        allocated, errp);
        if (ret < 0) {
            return ret;
        }

        (*new_reftable_index)++;
    }

    status_cb(bs, (uint64_t)(index + 1) * s->refcount_table_size,
              (uint64_t)total * s->refcount_table_size, cb_opaque);

    return 0;
}

int qcow2_change_refcount_order(BlockDriverState *bs, int refcount_order,
                                BlockDriverAmendStatusCB *status_cb,
                                void *cb_opaque, Error **errp)
{
    BDRVQcow2State *s = bs->opaque;
    Qcow2GetRefcountFunc *new_get_refcount;
    Qcow2SetRefcountFunc *new_set_refcount;
    void *new_refblock = qemu_blockalign(bs->file->bs, s->cluster_size);
    uint64_t *new_reftable = NULL, new_reftable_size = 0;
    uint64_t *old_reftable, old_reftable_size, old_reftable_offset;
    uint64_t new_reftable_index = 0;
    uint64_t i;
    int64_t new_reftable_offset = 0, allocated_reftable_size = 0;
    int new_refblock_size, new_refcount_bits = 1 << refcount_order;
    int old_refcount_order;
    int walk_index = 0;
    int ret;
    bool new_allocation;

    assert(s->qcow_version >= 3);
    assert(refcount_order >= 0 && refcount_order <= 6);

    new_refblock_size = 1 << (s->cluster_bits - (refcount_order - 3));

    new_get_refcount = get_refcount_funcs[refcount_order];
    new_set_refcount = set_refcount_funcs[refcount_order];


    do {
        int total_walks;

        new_allocation = false;

        total_walks = MAX(walk_index + 2, 3);

        ret = walk_over_reftable(bs, &new_reftable, &new_reftable_index,
                                 &new_reftable_size, NULL, new_refblock_size,
                                 new_refcount_bits, &alloc_refblock,
                                 &new_allocation, NULL, status_cb, cb_opaque,
                                 walk_index++, total_walks, errp);
        if (ret < 0) {
            goto done;
        }

        new_reftable_index = 0;

        if (new_allocation) {
            if (new_reftable_offset) {
                qcow2_free_clusters(
                    bs, new_reftable_offset,
                    allocated_reftable_size * REFTABLE_ENTRY_SIZE,
                    QCOW2_DISCARD_NEVER);
            }

            new_reftable_offset = qcow2_alloc_clusters(bs, new_reftable_size *
                                                           REFTABLE_ENTRY_SIZE);
            if (new_reftable_offset < 0) {
                error_setg_errno(errp, -new_reftable_offset,
                                 "Failed to allocate the new reftable");
                ret = new_reftable_offset;
                goto done;
            }
            allocated_reftable_size = new_reftable_size;
        }
    } while (new_allocation);

    ret = walk_over_reftable(bs, &new_reftable, &new_reftable_index,
                             &new_reftable_size, new_refblock,
                             new_refblock_size, new_refcount_bits,
                             &flush_refblock, &new_allocation, new_set_refcount,
                             status_cb, cb_opaque, walk_index, walk_index + 1,
                             errp);
    if (ret < 0) {
        goto done;
    }
    assert(!new_allocation);


    ret = qcow2_pre_write_overlap_check(bs, 0, new_reftable_offset,
                                        new_reftable_size * REFTABLE_ENTRY_SIZE,
                                        false);
    if (ret < 0) {
        error_setg_errno(errp, -ret, "Overlap check failed");
        goto done;
    }

    for (i = 0; i < new_reftable_size; i++) {
        cpu_to_be64s(&new_reftable[i]);
    }

    ret = bdrv_pwrite(bs->file, new_reftable_offset,
                      new_reftable_size * REFTABLE_ENTRY_SIZE, new_reftable,
                      0);

    for (i = 0; i < new_reftable_size; i++) {
        be64_to_cpus(&new_reftable[i]);
    }

    if (ret < 0) {
        error_setg_errno(errp, -ret, "Failed to write the new reftable");
        goto done;
    }


    ret = qcow2_cache_flush(bs, s->refcount_block_cache);
    if (ret < 0) {
        error_setg_errno(errp, -ret, "Failed to flush the refblock cache");
        goto done;
    }

    old_refcount_order  = s->refcount_order;
    old_reftable_size   = s->refcount_table_size;
    old_reftable_offset = s->refcount_table_offset;

    s->refcount_order        = refcount_order;
    s->refcount_table_size   = new_reftable_size;
    s->refcount_table_offset = new_reftable_offset;

    ret = qcow2_update_header(bs);
    if (ret < 0) {
        s->refcount_order        = old_refcount_order;
        s->refcount_table_size   = old_reftable_size;
        s->refcount_table_offset = old_reftable_offset;
        error_setg_errno(errp, -ret, "Failed to update the qcow2 header");
        goto done;
    }

    old_reftable = s->refcount_table;
    s->refcount_table = new_reftable;
    update_max_refcount_table_index(s);

    s->refcount_bits = 1 << refcount_order;
    s->refcount_max = UINT64_C(1) << (s->refcount_bits - 1);
    s->refcount_max += s->refcount_max - 1;

    s->refcount_block_bits = s->cluster_bits - (refcount_order - 3);
    s->refcount_block_size = 1 << s->refcount_block_bits;

    s->get_refcount = new_get_refcount;
    s->set_refcount = new_set_refcount;

    new_reftable        = old_reftable;
    new_reftable_size   = old_reftable_size;
    new_reftable_offset = old_reftable_offset;

done:
    if (new_reftable) {
        for (i = 0; i < new_reftable_size; i++) {
            uint64_t offset = new_reftable[i] & REFT_OFFSET_MASK;
            if (offset) {
                qcow2_free_clusters(bs, offset, s->cluster_size,
                                    QCOW2_DISCARD_OTHER);
            }
        }
        g_free(new_reftable);

        if (new_reftable_offset > 0) {
            qcow2_free_clusters(bs, new_reftable_offset,
                                new_reftable_size * REFTABLE_ENTRY_SIZE,
                                QCOW2_DISCARD_OTHER);
        }
    }

    qemu_vfree(new_refblock);
    return ret;
}

static int64_t coroutine_fn GRAPH_RDLOCK
get_refblock_offset(BlockDriverState *bs, uint64_t offset)
{
    BDRVQcow2State *s = bs->opaque;
    uint32_t index = offset_to_reftable_index(s, offset);
    int64_t covering_refblock_offset = 0;

    if (index < s->refcount_table_size) {
        covering_refblock_offset = s->refcount_table[index] & REFT_OFFSET_MASK;
    }
    if (!covering_refblock_offset) {
        qcow2_signal_corruption(bs, true, -1, -1, "Refblock at %#" PRIx64 " is "
                                "not covered by the refcount structures",
                                offset);
        return -EIO;
    }

    return covering_refblock_offset;
}

static int coroutine_fn GRAPH_RDLOCK
qcow2_discard_refcount_block(BlockDriverState *bs, uint64_t discard_block_offs)
{
    BDRVQcow2State *s = bs->opaque;
    int64_t refblock_offs;
    uint64_t cluster_index = discard_block_offs >> s->cluster_bits;
    uint32_t block_index = cluster_index & (s->refcount_block_size - 1);
    void *refblock;
    int ret;

    refblock_offs = get_refblock_offset(bs, discard_block_offs);
    if (refblock_offs < 0) {
        return refblock_offs;
    }

    assert(discard_block_offs != 0);

    ret = qcow2_cache_get(bs, s->refcount_block_cache, refblock_offs,
                          &refblock);
    if (ret < 0) {
        return ret;
    }

    if (s->get_refcount(refblock, block_index) != 1) {
        qcow2_signal_corruption(bs, true, -1, -1, "Invalid refcount:"
                                " refblock offset %#" PRIx64
                                ", reftable index %u"
                                ", block offset %#" PRIx64
                                ", refcount %#" PRIx64,
                                refblock_offs,
                                offset_to_reftable_index(s, discard_block_offs),
                                discard_block_offs,
                                s->get_refcount(refblock, block_index));
        qcow2_cache_put(s->refcount_block_cache, &refblock);
        return -EINVAL;
    }
    s->set_refcount(refblock, block_index, 0);

    qcow2_cache_entry_mark_dirty(s->refcount_block_cache, refblock);

    qcow2_cache_put(s->refcount_block_cache, &refblock);

    if (cluster_index < s->free_cluster_index) {
        s->free_cluster_index = cluster_index;
    }

    refblock = qcow2_cache_is_table_offset(s->refcount_block_cache,
                                           discard_block_offs);
    if (refblock) {
        qcow2_cache_discard(s->refcount_block_cache, refblock);
    }
    update_refcount_discard(bs, discard_block_offs, s->cluster_size);

    return 0;
}

int coroutine_fn qcow2_shrink_reftable(BlockDriverState *bs)
{
    BDRVQcow2State *s = bs->opaque;
    uint64_t *reftable_tmp =
        g_malloc(s->refcount_table_size * REFTABLE_ENTRY_SIZE);
    int i, ret;

    for (i = 0; i < s->refcount_table_size; i++) {
        int64_t refblock_offs = s->refcount_table[i] & REFT_OFFSET_MASK;
        void *refblock;
        bool unused_block;

        if (refblock_offs == 0) {
            reftable_tmp[i] = 0;
            continue;
        }
        ret = qcow2_cache_get(bs, s->refcount_block_cache, refblock_offs,
                              &refblock);
        if (ret < 0) {
            goto out;
        }

        if (i == offset_to_reftable_index(s, refblock_offs)) {
            uint64_t block_index = (refblock_offs >> s->cluster_bits) &
                                   (s->refcount_block_size - 1);
            uint64_t refcount = s->get_refcount(refblock, block_index);

            s->set_refcount(refblock, block_index, 0);

            unused_block = buffer_is_zero(refblock, s->cluster_size);

            s->set_refcount(refblock, block_index, refcount);
        } else {
            unused_block = buffer_is_zero(refblock, s->cluster_size);
        }
        qcow2_cache_put(s->refcount_block_cache, &refblock);

        reftable_tmp[i] = unused_block ? 0 : cpu_to_be64(s->refcount_table[i]);
    }

    ret = bdrv_co_pwrite_sync(bs->file, s->refcount_table_offset,
                              s->refcount_table_size * REFTABLE_ENTRY_SIZE,
                              reftable_tmp, 0);
    for (i = 0; i < s->refcount_table_size; i++) {
        if (s->refcount_table[i] && !reftable_tmp[i]) {
            if (ret == 0) {
                ret = qcow2_discard_refcount_block(bs, s->refcount_table[i] &
                                                       REFT_OFFSET_MASK);
            }
            s->refcount_table[i] = 0;
        }
    }

    if (!s->cache_discards) {
        qcow2_process_discards(bs, ret);
    }

out:
    g_free(reftable_tmp);
    return ret;
}

int64_t coroutine_fn qcow2_get_last_cluster(BlockDriverState *bs, int64_t size)
{
    BDRVQcow2State *s = bs->opaque;
    int64_t i;

    for (i = size_to_clusters(s, size) - 1; i >= 0; i--) {
        uint64_t refcount;
        int ret = qcow2_get_refcount(bs, i, &refcount);
        if (ret < 0) {
            fprintf(stderr, "Can't get refcount for cluster %" PRId64 ": %s\n",
                    i, strerror(-ret));
            return ret;
        }
        if (refcount > 0) {
            return i;
        }
    }
    qcow2_signal_corruption(bs, true, -1, -1,
                            "There are no references in the refcount table.");
    return -EIO;
}

int coroutine_fn GRAPH_RDLOCK
qcow2_detect_metadata_preallocation(BlockDriverState *bs)
{
    BDRVQcow2State *s = bs->opaque;
    int64_t i, end_cluster, cluster_count = 0, threshold;
    int64_t file_length, real_allocation, real_clusters;

    qemu_co_mutex_assert_locked(&s->lock);

    file_length = bdrv_co_getlength(bs->file->bs);
    if (file_length < 0) {
        return file_length;
    }

    real_allocation = bdrv_co_get_allocated_file_size(bs->file->bs);
    if (real_allocation < 0) {
        return real_allocation;
    }

    real_clusters = real_allocation / s->cluster_size;
    threshold = MAX(real_clusters * 10 / 9, real_clusters + 2);

    end_cluster = size_to_clusters(s, file_length);
    for (i = 0; i < end_cluster && cluster_count < threshold; i++) {
        uint64_t refcount;
        int ret = qcow2_get_refcount(bs, i, &refcount);
        if (ret < 0) {
            return ret;
        }
        cluster_count += !!refcount;
    }

    return cluster_count >= threshold;
}
