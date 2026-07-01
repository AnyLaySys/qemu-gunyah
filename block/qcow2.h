
#ifndef BLOCK_QCOW2_H
#define BLOCK_QCOW2_H

#include "qemu/coroutine.h"
#include "qemu/units.h"
#include "block/block_int.h"


#define QCOW_MAGIC (('Q' << 24) | ('F' << 16) | ('I' << 8) | 0xfb)

#define QCOW_METHOD_NONE 0

#define QCOW_MAX_CLUSTER_OFFSET ((1ULL << 56) - 1)

#define QCOW_MAX_REFTABLE_SIZE (8 * MiB)

#define QCOW_MAX_L1_SIZE (32 * MiB)

#define QCOW2_MAX_BITMAPS 65535
#define QCOW2_MAX_BITMAP_DIRECTORY_SIZE (1024 * QCOW2_MAX_BITMAPS)

#define QCOW2_MAX_WORKERS 8

#define QCOW_OFLAG_COPIED     (1ULL << 63)
#define QCOW_OFLAG_COMPRESSED (1ULL << 62)
#define QCOW_OFLAG_ZERO (1ULL << 0)

#define QCOW_EXTL2_SUBCLUSTERS_PER_CLUSTER 32

#define QCOW_OFLAG_SUB_ALLOC(X)   (1ULL << (X))
#define QCOW_OFLAG_SUB_ZERO(X)    (QCOW_OFLAG_SUB_ALLOC(X) << 32)
#define QCOW_OFLAG_SUB_ALLOC_RANGE(X, Y) \
    (QCOW_OFLAG_SUB_ALLOC(Y) - QCOW_OFLAG_SUB_ALLOC(X))
#define QCOW_OFLAG_SUB_ZERO_RANGE(X, Y) \
    (QCOW_OFLAG_SUB_ALLOC_RANGE(X, Y) << 32)
#define QCOW_L2_BITMAP_ALL_ALLOC  (QCOW_OFLAG_SUB_ALLOC_RANGE(0, 32))
#define QCOW_L2_BITMAP_ALL_ZEROES (QCOW_OFLAG_SUB_ZERO_RANGE(0, 32))

#define L2E_SIZE_NORMAL   (sizeof(uint64_t))
#define L2E_SIZE_EXTENDED (sizeof(uint64_t) * 2)

#define L1E_SIZE (sizeof(uint64_t))

#define REFTABLE_ENTRY_SIZE (sizeof(uint64_t))

#define MIN_CLUSTER_BITS 9
#define MAX_CLUSTER_BITS 21

#define QCOW2_COMPRESSED_SECTOR_SIZE 512U

#define MIN_L2_CACHE_SIZE 2 /* cache entries */

#define MIN_REFCOUNT_CACHE_SIZE 4 /* clusters */

#ifdef CONFIG_LINUX
#define DEFAULT_L2_CACHE_MAX_SIZE (32 * MiB)
#define DEFAULT_CACHE_CLEAN_INTERVAL 600  /* seconds */
#else
#define DEFAULT_L2_CACHE_MAX_SIZE (8 * MiB)
#define DEFAULT_CACHE_CLEAN_INTERVAL 0
#endif

#define DEFAULT_CLUSTER_SIZE 65536

#define QCOW2_OPT_DATA_FILE "data-file"
#define QCOW2_OPT_LAZY_REFCOUNTS "lazy-refcounts"
#define QCOW2_OPT_DISCARD_REQUEST "pass-discard-request"
#define QCOW2_OPT_DISCARD_UNUSED "pass-discard-unused"
#define QCOW2_OPT_DISCARD_OTHER "pass-discard-other"
#define QCOW2_OPT_DISCARD_NO_UNREF "discard-no-unref"
#define QCOW2_OPT_OVERLAP "overlap-check"
#define QCOW2_OPT_OVERLAP_TEMPLATE "overlap-check.template"
#define QCOW2_OPT_OVERLAP_MAIN_HEADER "overlap-check.main-header"
#define QCOW2_OPT_OVERLAP_ACTIVE_L1 "overlap-check.active-l1"
#define QCOW2_OPT_OVERLAP_ACTIVE_L2 "overlap-check.active-l2"
#define QCOW2_OPT_OVERLAP_REFCOUNT_TABLE "overlap-check.refcount-table"
#define QCOW2_OPT_OVERLAP_REFCOUNT_BLOCK "overlap-check.refcount-block"
#define QCOW2_OPT_OVERLAP_UNUSED_METADATA "overlap-check.unused-table"
#define QCOW2_OPT_OVERLAP_INACTIVE_L1 "overlap-check.inactive-l1"
#define QCOW2_OPT_OVERLAP_INACTIVE_L2 "overlap-check.inactive-l2"
#define QCOW2_OPT_OVERLAP_BITMAP_DIRECTORY "overlap-check.bitmap-directory"
#define QCOW2_OPT_CACHE_SIZE "cache-size"
#define QCOW2_OPT_L2_CACHE_SIZE "l2-cache-size"
#define QCOW2_OPT_L2_CACHE_ENTRY_SIZE "l2-cache-entry-size"
#define QCOW2_OPT_REFCOUNT_CACHE_SIZE "refcount-cache-size"
#define QCOW2_OPT_CACHE_CLEAN_INTERVAL "cache-clean-interval"

typedef struct QCowHeader {
    uint32_t magic;
    uint32_t version;
    uint64_t base_name_offset;
    uint32_t base_name_size;
    uint32_t cluster_bits;
    uint64_t size; /* in bytes */
    uint32_t image_method;
    uint32_t l1_size; /* XXX: save number of clusters instead ? */
    uint64_t l1_table_offset;
    uint64_t refcount_table_offset;
    uint32_t refcount_table_clusters;
    uint32_t legacy_count;
    uint64_t legacy_offset;

    uint64_t incompatible_features;
    uint64_t compatible_features;
    uint64_t autoclear_features;

    uint32_t refcount_order;
    uint32_t header_length;

    uint8_t compression_type;

    uint8_t padding[7];
} QEMU_PACKED QCowHeader;

QEMU_BUILD_BUG_ON(!QEMU_IS_ALIGNED(sizeof(QCowHeader), 8));

struct Qcow2Cache;
typedef struct Qcow2Cache Qcow2Cache;

typedef struct Qcow2UnknownHeaderExtension {
    uint32_t magic;
    uint32_t len;
    QLIST_ENTRY(Qcow2UnknownHeaderExtension) next;
    uint8_t data[];
} Qcow2UnknownHeaderExtension;

enum {
    QCOW2_FEAT_TYPE_INCOMPATIBLE    = 0,
    QCOW2_FEAT_TYPE_COMPATIBLE      = 1,
    QCOW2_FEAT_TYPE_AUTOCLEAR       = 2,
};

enum {
    QCOW2_INCOMPAT_DIRTY_BITNR      = 0,
    QCOW2_INCOMPAT_CORRUPT_BITNR    = 1,
    QCOW2_INCOMPAT_DATA_FILE_BITNR  = 2,
    QCOW2_INCOMPAT_COMPRESSION_BITNR = 3,
    QCOW2_INCOMPAT_EXTL2_BITNR      = 4,
    QCOW2_INCOMPAT_DIRTY            = 1 << QCOW2_INCOMPAT_DIRTY_BITNR,
    QCOW2_INCOMPAT_CORRUPT          = 1 << QCOW2_INCOMPAT_CORRUPT_BITNR,
    QCOW2_INCOMPAT_DATA_FILE        = 1 << QCOW2_INCOMPAT_DATA_FILE_BITNR,
    QCOW2_INCOMPAT_COMPRESSION      = 1 << QCOW2_INCOMPAT_COMPRESSION_BITNR,
    QCOW2_INCOMPAT_EXTL2            = 1 << QCOW2_INCOMPAT_EXTL2_BITNR,

    QCOW2_INCOMPAT_MASK             = QCOW2_INCOMPAT_DIRTY
                                    | QCOW2_INCOMPAT_CORRUPT
                                    | QCOW2_INCOMPAT_DATA_FILE
                                    | QCOW2_INCOMPAT_COMPRESSION
                                    | QCOW2_INCOMPAT_EXTL2,
};

enum {
    QCOW2_COMPAT_LAZY_REFCOUNTS_BITNR = 0,
    QCOW2_COMPAT_LAZY_REFCOUNTS       = 1 << QCOW2_COMPAT_LAZY_REFCOUNTS_BITNR,

    QCOW2_COMPAT_FEAT_MASK            = QCOW2_COMPAT_LAZY_REFCOUNTS,
};

enum {
    QCOW2_AUTOCLEAR_BITMAPS_BITNR       = 0,
    QCOW2_AUTOCLEAR_DATA_FILE_RAW_BITNR = 1,
    QCOW2_AUTOCLEAR_BITMAPS             = 1 << QCOW2_AUTOCLEAR_BITMAPS_BITNR,
    QCOW2_AUTOCLEAR_DATA_FILE_RAW       = 1 << QCOW2_AUTOCLEAR_DATA_FILE_RAW_BITNR,

    QCOW2_AUTOCLEAR_MASK                = QCOW2_AUTOCLEAR_BITMAPS
                                        | QCOW2_AUTOCLEAR_DATA_FILE_RAW,
};

enum qcow2_discard_type {
    QCOW2_DISCARD_NEVER = 0,
    QCOW2_DISCARD_ALWAYS,
    QCOW2_DISCARD_REQUEST,
    QCOW2_DISCARD_UNUSED,
    QCOW2_DISCARD_OTHER,
    QCOW2_DISCARD_MAX
};

typedef struct Qcow2Feature {
    uint8_t type;
    uint8_t bit;
    char    name[46];
} QEMU_PACKED Qcow2Feature;

typedef struct Qcow2DiscardRegion {
    BlockDriverState *bs;
    uint64_t offset;
    uint64_t bytes;
    QTAILQ_ENTRY(Qcow2DiscardRegion) next;
} Qcow2DiscardRegion;

typedef uint64_t Qcow2GetRefcountFunc(const void *refcount_array,
                                      uint64_t index);
typedef void Qcow2SetRefcountFunc(void *refcount_array,
                                  uint64_t index, uint64_t value);

typedef struct Qcow2BitmapHeaderExt {
    uint32_t nb_bitmaps;
    uint32_t reserved32;
    uint64_t bitmap_directory_size;
    uint64_t bitmap_directory_offset;
} QEMU_PACKED Qcow2BitmapHeaderExt;

#define QCOW2_MAX_THREADS 4

typedef struct BDRVQcow2State {
    int cluster_bits;
    int cluster_size;
    int l2_slice_size;
    int subcluster_bits;
    int subcluster_size;
    int subclusters_per_cluster;
    int l2_bits;
    int l2_size;
    int l1_size;
    int l1_vm_state_index;
    int refcount_block_bits;
    int refcount_block_size;
    int csize_shift;
    int csize_mask;
    uint64_t cluster_offset_mask;
    uint64_t l1_table_offset;
    uint64_t *l1_table;

    Qcow2Cache *l2_table_cache;
    Qcow2Cache *refcount_block_cache;
    QEMUTimer *cache_clean_timer;
    unsigned cache_clean_interval;

    QLIST_HEAD(, QCowL2Meta) cluster_allocs;

    uint64_t *refcount_table;
    uint64_t refcount_table_offset;
    uint32_t refcount_table_size;
    uint32_t max_refcount_table_index; /* Last used entry in refcount_table */
    uint64_t free_cluster_index;
    uint64_t free_byte_offset;

    CoMutex lock;

    uint32_t nb_bitmaps;
    uint64_t bitmap_directory_size;
    uint64_t bitmap_directory_offset;

    int flags;
    int qcow_version;
    bool use_lazy_refcounts;
    int refcount_order;
    int refcount_bits;
    uint64_t refcount_max;

    Qcow2GetRefcountFunc *get_refcount;
    Qcow2SetRefcountFunc *set_refcount;

    bool discard_passthrough[QCOW2_DISCARD_MAX];

    bool discard_no_unref;

    int overlap_check; /* bitmask of Qcow2MetadataOverlap values */
    bool signaled_corruption;

    uint64_t incompatible_features;
    uint64_t compatible_features;
    uint64_t autoclear_features;

    size_t unknown_header_fields_size;
    void *unknown_header_fields;
    QLIST_HEAD(, Qcow2UnknownHeaderExtension) unknown_header_ext;
    QTAILQ_HEAD (, Qcow2DiscardRegion) discards;
    bool cache_discards;

    char *image_data_file;

    CoQueue thread_task_queue;
    int nb_threads;

    BdrvChild *data_file;

    bool metadata_preallocation_checked;
    bool metadata_preallocation;
    Qcow2CompressionType compression_type;
} BDRVQcow2State;

typedef struct Qcow2COWRegion {
    unsigned    offset;

    unsigned    nb_bytes;
} Qcow2COWRegion;

typedef struct QCowL2Meta
{
    uint64_t offset;

    uint64_t alloc_offset;

    int nb_clusters;

    bool keep_old_clusters;

    CoQueue dependent_requests;

    Qcow2COWRegion cow_start;

    Qcow2COWRegion cow_end;

    bool skip_cow;

    bool prealloc;

    QEMUIOVector *data_qiov;
    size_t data_qiov_offset;

    struct QCowL2Meta *next;

    QLIST_ENTRY(QCowL2Meta) next_in_flight;
} QCowL2Meta;


typedef enum QCow2ClusterType {
    QCOW2_CLUSTER_UNALLOCATED,
    QCOW2_CLUSTER_ZERO_PLAIN,
    QCOW2_CLUSTER_ZERO_ALLOC,
    QCOW2_CLUSTER_NORMAL,
    QCOW2_CLUSTER_COMPRESSED,
} QCow2ClusterType;

typedef enum QCow2SubclusterType {
    QCOW2_SUBCLUSTER_UNALLOCATED_PLAIN,
    QCOW2_SUBCLUSTER_UNALLOCATED_ALLOC,
    QCOW2_SUBCLUSTER_ZERO_PLAIN,
    QCOW2_SUBCLUSTER_ZERO_ALLOC,
    QCOW2_SUBCLUSTER_NORMAL,
    QCOW2_SUBCLUSTER_COMPRESSED,
    QCOW2_SUBCLUSTER_INVALID,
} QCow2SubclusterType;

typedef enum QCow2MetadataOverlap {
    QCOW2_OL_MAIN_HEADER_BITNR      = 0,
    QCOW2_OL_ACTIVE_L1_BITNR        = 1,
    QCOW2_OL_ACTIVE_L2_BITNR        = 2,
    QCOW2_OL_REFCOUNT_TABLE_BITNR   = 3,
    QCOW2_OL_REFCOUNT_BLOCK_BITNR   = 4,
    QCOW2_OL_UNUSED_METADATA_BITNR   = 5,
    QCOW2_OL_INACTIVE_L1_BITNR      = 6,
    QCOW2_OL_INACTIVE_L2_BITNR      = 7,
    QCOW2_OL_BITMAP_DIRECTORY_BITNR = 8,

    QCOW2_OL_MAX_BITNR              = 9,

    QCOW2_OL_NONE             = 0,
    QCOW2_OL_MAIN_HEADER      = (1 << QCOW2_OL_MAIN_HEADER_BITNR),
    QCOW2_OL_ACTIVE_L1        = (1 << QCOW2_OL_ACTIVE_L1_BITNR),
    QCOW2_OL_ACTIVE_L2        = (1 << QCOW2_OL_ACTIVE_L2_BITNR),
    QCOW2_OL_REFCOUNT_TABLE   = (1 << QCOW2_OL_REFCOUNT_TABLE_BITNR),
    QCOW2_OL_REFCOUNT_BLOCK   = (1 << QCOW2_OL_REFCOUNT_BLOCK_BITNR),
    QCOW2_OL_UNUSED_METADATA   = (1 << QCOW2_OL_UNUSED_METADATA_BITNR),
    QCOW2_OL_INACTIVE_L1      = (1 << QCOW2_OL_INACTIVE_L1_BITNR),
    QCOW2_OL_INACTIVE_L2      = (1 << QCOW2_OL_INACTIVE_L2_BITNR),
    QCOW2_OL_BITMAP_DIRECTORY = (1 << QCOW2_OL_BITMAP_DIRECTORY_BITNR),
} QCow2MetadataOverlap;

#define QCOW2_OL_CONSTANT \
    (QCOW2_OL_MAIN_HEADER | QCOW2_OL_ACTIVE_L1 | QCOW2_OL_REFCOUNT_TABLE | \
     QCOW2_OL_UNUSED_METADATA | QCOW2_OL_BITMAP_DIRECTORY)

#define QCOW2_OL_CACHED \
    (QCOW2_OL_CONSTANT | QCOW2_OL_ACTIVE_L2 | QCOW2_OL_REFCOUNT_BLOCK | \
     QCOW2_OL_INACTIVE_L1)

#define QCOW2_OL_ALL \
    (QCOW2_OL_CACHED | QCOW2_OL_INACTIVE_L2)

#define L1E_OFFSET_MASK 0x00fffffffffffe00ULL
#define L1E_RESERVED_MASK 0x7f000000000001ffULL
#define L2E_OFFSET_MASK 0x00fffffffffffe00ULL
#define L2E_STD_RESERVED_MASK 0x3f000000000001feULL

#define REFT_OFFSET_MASK 0xfffffffffffffe00ULL
#define REFT_RESERVED_MASK 0x1ffULL

#define INV_OFFSET (-1ULL)

static inline bool has_subclusters(BDRVQcow2State *s)
{
    return s->incompatible_features & QCOW2_INCOMPAT_EXTL2;
}

static inline size_t l2_entry_size(BDRVQcow2State *s)
{
    return has_subclusters(s) ? L2E_SIZE_EXTENDED : L2E_SIZE_NORMAL;
}

static inline uint64_t get_l2_entry(BDRVQcow2State *s, uint64_t *l2_slice,
                                    int idx)
{
    idx *= l2_entry_size(s) / sizeof(uint64_t);
    return be64_to_cpu(l2_slice[idx]);
}

static inline uint64_t get_l2_bitmap(BDRVQcow2State *s, uint64_t *l2_slice,
                                     int idx)
{
    if (has_subclusters(s)) {
        idx *= l2_entry_size(s) / sizeof(uint64_t);
        return be64_to_cpu(l2_slice[idx + 1]);
    } else {
        return 0; /* For convenience only; this value has no meaning. */
    }
}

static inline void set_l2_entry(BDRVQcow2State *s, uint64_t *l2_slice,
                                int idx, uint64_t entry)
{
    idx *= l2_entry_size(s) / sizeof(uint64_t);
    l2_slice[idx] = cpu_to_be64(entry);
}

static inline void set_l2_bitmap(BDRVQcow2State *s, uint64_t *l2_slice,
                                 int idx, uint64_t bitmap)
{
    assert(has_subclusters(s));
    idx *= l2_entry_size(s) / sizeof(uint64_t);
    l2_slice[idx + 1] = cpu_to_be64(bitmap);
}

static inline bool GRAPH_RDLOCK has_data_file(BlockDriverState *bs)
{
    BDRVQcow2State *s = bs->opaque;
    return (s->data_file != bs->file);
}

static inline bool data_file_is_raw(BlockDriverState *bs)
{
    BDRVQcow2State *s = bs->opaque;
    return !!(s->autoclear_features & QCOW2_AUTOCLEAR_DATA_FILE_RAW);
}

static inline int64_t start_of_cluster(BDRVQcow2State *s, int64_t offset)
{
    return offset & ~(s->cluster_size - 1);
}

static inline int64_t offset_into_cluster(BDRVQcow2State *s, int64_t offset)
{
    return offset & (s->cluster_size - 1);
}

static inline int64_t offset_into_subcluster(BDRVQcow2State *s, int64_t offset)
{
    return offset & (s->subcluster_size - 1);
}

static inline uint64_t size_to_clusters(BDRVQcow2State *s, uint64_t size)
{
    return (size + (s->cluster_size - 1)) >> s->cluster_bits;
}

static inline uint64_t size_to_subclusters(BDRVQcow2State *s, uint64_t size)
{
    return (size + (s->subcluster_size - 1)) >> s->subcluster_bits;
}

static inline int64_t size_to_l1(BDRVQcow2State *s, int64_t size)
{
    int shift = s->cluster_bits + s->l2_bits;
    return (size + (1ULL << shift) - 1) >> shift;
}

static inline int offset_to_l1_index(BDRVQcow2State *s, uint64_t offset)
{
    return offset >> (s->l2_bits + s->cluster_bits);
}

static inline int offset_to_l2_index(BDRVQcow2State *s, int64_t offset)
{
    return (offset >> s->cluster_bits) & (s->l2_size - 1);
}

static inline int offset_to_l2_slice_index(BDRVQcow2State *s, int64_t offset)
{
    return (offset >> s->cluster_bits) & (s->l2_slice_size - 1);
}

static inline int offset_to_sc_index(BDRVQcow2State *s, int64_t offset)
{
    return (offset >> s->subcluster_bits) & (s->subclusters_per_cluster - 1);
}

static inline int64_t qcow2_vm_state_offset(BDRVQcow2State *s)
{
    return (int64_t)s->l1_vm_state_index << (s->cluster_bits + s->l2_bits);
}

static inline QCow2ClusterType GRAPH_RDLOCK
qcow2_get_cluster_type(BlockDriverState *bs, uint64_t l2_entry)
{
    BDRVQcow2State *s = bs->opaque;

    if (l2_entry & QCOW_OFLAG_COMPRESSED) {
        return QCOW2_CLUSTER_COMPRESSED;
    } else if ((l2_entry & QCOW_OFLAG_ZERO) && !has_subclusters(s)) {
        if (l2_entry & L2E_OFFSET_MASK) {
            return QCOW2_CLUSTER_ZERO_ALLOC;
        }
        return QCOW2_CLUSTER_ZERO_PLAIN;
    } else if (!(l2_entry & L2E_OFFSET_MASK)) {
        if (has_data_file(bs) && (l2_entry & QCOW_OFLAG_COPIED)) {
            return QCOW2_CLUSTER_NORMAL;
        } else {
            return QCOW2_CLUSTER_UNALLOCATED;
        }
    } else {
        return QCOW2_CLUSTER_NORMAL;
    }
}

static inline GRAPH_RDLOCK
QCow2SubclusterType qcow2_get_subcluster_type(BlockDriverState *bs,
                                              uint64_t l2_entry,
                                              uint64_t l2_bitmap,
                                              unsigned sc_index)
{
    BDRVQcow2State *s = bs->opaque;
    QCow2ClusterType type = qcow2_get_cluster_type(bs, l2_entry);
    assert(sc_index < s->subclusters_per_cluster);

    if (has_subclusters(s)) {
        switch (type) {
        case QCOW2_CLUSTER_COMPRESSED:
            return QCOW2_SUBCLUSTER_COMPRESSED;
        case QCOW2_CLUSTER_NORMAL:
            if ((l2_bitmap >> 32) & l2_bitmap) {
                return QCOW2_SUBCLUSTER_INVALID;
            } else if (l2_bitmap & QCOW_OFLAG_SUB_ZERO(sc_index)) {
                return QCOW2_SUBCLUSTER_ZERO_ALLOC;
            } else if (l2_bitmap & QCOW_OFLAG_SUB_ALLOC(sc_index)) {
                return QCOW2_SUBCLUSTER_NORMAL;
            } else {
                return QCOW2_SUBCLUSTER_UNALLOCATED_ALLOC;
            }
        case QCOW2_CLUSTER_UNALLOCATED:
            if (l2_bitmap & QCOW_L2_BITMAP_ALL_ALLOC) {
                return QCOW2_SUBCLUSTER_INVALID;
            } else if (l2_bitmap & QCOW_OFLAG_SUB_ZERO(sc_index)) {
                return QCOW2_SUBCLUSTER_ZERO_PLAIN;
            } else {
                return QCOW2_SUBCLUSTER_UNALLOCATED_PLAIN;
            }
        default:
            g_assert_not_reached();
        }
    } else {
        switch (type) {
        case QCOW2_CLUSTER_COMPRESSED:
            return QCOW2_SUBCLUSTER_COMPRESSED;
        case QCOW2_CLUSTER_ZERO_PLAIN:
            return QCOW2_SUBCLUSTER_ZERO_PLAIN;
        case QCOW2_CLUSTER_ZERO_ALLOC:
            return QCOW2_SUBCLUSTER_ZERO_ALLOC;
        case QCOW2_CLUSTER_NORMAL:
            return QCOW2_SUBCLUSTER_NORMAL;
        case QCOW2_CLUSTER_UNALLOCATED:
            return QCOW2_SUBCLUSTER_UNALLOCATED_PLAIN;
        default:
            g_assert_not_reached();
        }
    }
}

static inline bool qcow2_cluster_is_allocated(QCow2ClusterType type)
{
    return (type == QCOW2_CLUSTER_COMPRESSED || type == QCOW2_CLUSTER_NORMAL ||
            type == QCOW2_CLUSTER_ZERO_ALLOC);
}

static inline bool qcow2_need_accurate_refcounts(BDRVQcow2State *s)
{
    return !(s->incompatible_features & QCOW2_INCOMPAT_DIRTY);
}

static inline uint64_t l2meta_cow_start(QCowL2Meta *m)
{
    return m->offset + m->cow_start.offset;
}

static inline uint64_t l2meta_cow_end(QCowL2Meta *m)
{
    return m->offset + m->cow_end.offset + m->cow_end.nb_bytes;
}

static inline uint64_t refcount_diff(uint64_t r1, uint64_t r2)
{
    return r1 > r2 ? r1 - r2 : r2 - r1;
}

static inline
uint32_t offset_to_reftable_index(BDRVQcow2State *s, uint64_t offset)
{
    return offset >> (s->refcount_block_bits + s->cluster_bits);
}

int64_t qcow2_refcount_metadata_size(int64_t clusters, size_t cluster_size,
                                     int refcount_order, bool generous_increase,
                                     uint64_t *refblock_count);

int GRAPH_RDLOCK qcow2_mark_dirty(BlockDriverState *bs);
int GRAPH_RDLOCK qcow2_mark_corrupt(BlockDriverState *bs);
int GRAPH_RDLOCK qcow2_update_header(BlockDriverState *bs);

void GRAPH_RDLOCK
qcow2_signal_corruption(BlockDriverState *bs, bool fatal, int64_t offset,
                        int64_t size, const char *message_format, ...)
                        G_GNUC_PRINTF(5, 6);

int qcow2_validate_table(BlockDriverState *bs, uint64_t offset,
                         uint64_t entries, size_t entry_len,
                         int64_t max_size_bytes, const char *table_name,
                         Error **errp);

int coroutine_fn GRAPH_RDLOCK qcow2_refcount_init(BlockDriverState *bs);
void qcow2_refcount_close(BlockDriverState *bs);

int GRAPH_RDLOCK qcow2_get_refcount(BlockDriverState *bs, int64_t cluster_index,
                                    uint64_t *refcount);

int GRAPH_RDLOCK
qcow2_update_cluster_refcount(BlockDriverState *bs, int64_t cluster_index,
                              uint64_t addend, bool decrease,
                              enum qcow2_discard_type type);

int64_t GRAPH_RDLOCK
qcow2_refcount_area(BlockDriverState *bs, uint64_t offset,
                    uint64_t additional_clusters, bool exact_size,
                    int new_refblock_index,
                    uint64_t new_refblock_offset);

int64_t GRAPH_RDLOCK
qcow2_alloc_clusters(BlockDriverState *bs, uint64_t size);

int64_t GRAPH_RDLOCK coroutine_fn
qcow2_alloc_clusters_at(BlockDriverState *bs, uint64_t offset,
                        int64_t nb_clusters);

int64_t coroutine_fn GRAPH_RDLOCK qcow2_alloc_bytes(BlockDriverState *bs, int size);
void GRAPH_RDLOCK qcow2_free_clusters(BlockDriverState *bs,
                                      int64_t offset, int64_t size,
                                      enum qcow2_discard_type type);
void GRAPH_RDLOCK
qcow2_free_any_cluster(BlockDriverState *bs, uint64_t l2_entry,
                       enum qcow2_discard_type type);

int GRAPH_RDLOCK qcow2_flush_caches(BlockDriverState *bs);
int GRAPH_RDLOCK qcow2_write_caches(BlockDriverState *bs);
int coroutine_fn qcow2_check_refcounts(BlockDriverState *bs, BdrvCheckResult *res,
                                       BdrvCheckMode fix);

void GRAPH_RDLOCK qcow2_process_discards(BlockDriverState *bs, int ret);

int GRAPH_RDLOCK
qcow2_check_metadata_overlap(BlockDriverState *bs, int ign, int64_t offset,
                             int64_t size);
int GRAPH_RDLOCK
qcow2_pre_write_overlap_check(BlockDriverState *bs, int ign, int64_t offset,
                              int64_t size, bool data_file);

int coroutine_fn qcow2_inc_refcounts_imrt(BlockDriverState *bs, BdrvCheckResult *res,
                                          void **refcount_table,
                                          int64_t *refcount_table_size,
                                          int64_t offset, int64_t size);

int GRAPH_RDLOCK
qcow2_change_refcount_order(BlockDriverState *bs, int refcount_order,
                            BlockDriverAmendStatusCB *status_cb,
                            void *cb_opaque, Error **errp);
int coroutine_fn GRAPH_RDLOCK qcow2_shrink_reftable(BlockDriverState *bs);

int64_t coroutine_fn GRAPH_RDLOCK
qcow2_get_last_cluster(BlockDriverState *bs, int64_t size);

int coroutine_fn GRAPH_RDLOCK
qcow2_detect_metadata_preallocation(BlockDriverState *bs);

int GRAPH_RDLOCK
qcow2_grow_l1_table(BlockDriverState *bs, uint64_t min_size, bool exact_size);

int coroutine_fn GRAPH_RDLOCK
qcow2_shrink_l1_table(BlockDriverState *bs, uint64_t max_size);

int GRAPH_RDLOCK qcow2_write_l1_entry(BlockDriverState *bs, int l1_index);
int GRAPH_RDLOCK
qcow2_get_host_offset(BlockDriverState *bs, uint64_t offset,
                      unsigned int *bytes, uint64_t *host_offset,
                      QCow2SubclusterType *subcluster_type);

int coroutine_fn GRAPH_RDLOCK
qcow2_alloc_host_offset(BlockDriverState *bs, uint64_t offset,
                        unsigned int *bytes, uint64_t *host_offset,
                        QCowL2Meta **m);

int coroutine_fn GRAPH_RDLOCK
qcow2_alloc_compressed_cluster_offset(BlockDriverState *bs, uint64_t offset,
                                      int compressed_size, uint64_t *host_offset);
void GRAPH_RDLOCK
qcow2_parse_compressed_l2_entry(BlockDriverState *bs, uint64_t l2_entry,
                                uint64_t *coffset, int *csize);

int coroutine_fn GRAPH_RDLOCK
qcow2_alloc_cluster_link_l2(BlockDriverState *bs, QCowL2Meta *m);

void coroutine_fn GRAPH_RDLOCK
qcow2_alloc_cluster_abort(BlockDriverState *bs, QCowL2Meta *m);

int GRAPH_RDLOCK
qcow2_cluster_discard(BlockDriverState *bs, uint64_t offset, uint64_t bytes,
                      enum qcow2_discard_type type, bool full_discard);

int coroutine_fn GRAPH_RDLOCK
qcow2_subcluster_zeroize(BlockDriverState *bs, uint64_t offset, uint64_t bytes,
                         int flags);

int GRAPH_RDLOCK
qcow2_expand_zero_clusters(BlockDriverState *bs,
                           BlockDriverAmendStatusCB *status_cb,
                           void *cb_opaque);

Qcow2Cache * GRAPH_RDLOCK
qcow2_cache_create(BlockDriverState *bs, int num_tables, unsigned table_size);

int qcow2_cache_destroy(Qcow2Cache *c);

void qcow2_cache_entry_mark_dirty(Qcow2Cache *c, void *table);
int GRAPH_RDLOCK qcow2_cache_flush(BlockDriverState *bs, Qcow2Cache *c);
int GRAPH_RDLOCK qcow2_cache_write(BlockDriverState *bs, Qcow2Cache *c);
int GRAPH_RDLOCK qcow2_cache_set_dependency(BlockDriverState *bs, Qcow2Cache *c,
                                            Qcow2Cache *dependency);
void qcow2_cache_depends_on_flush(Qcow2Cache *c);

void qcow2_cache_clean_unused(Qcow2Cache *c);
int GRAPH_RDLOCK qcow2_cache_empty(BlockDriverState *bs, Qcow2Cache *c);

int GRAPH_RDLOCK
qcow2_cache_get(BlockDriverState *bs, Qcow2Cache *c, uint64_t offset,
                void **table);

int GRAPH_RDLOCK
qcow2_cache_get_empty(BlockDriverState *bs, Qcow2Cache *c, uint64_t offset,
                      void **table);

void qcow2_cache_put(Qcow2Cache *c, void **table);
void *qcow2_cache_is_table_offset(Qcow2Cache *c, uint64_t offset);
void qcow2_cache_discard(Qcow2Cache *c, void *table);

ssize_t coroutine_fn
qcow2_co_compress(BlockDriverState *bs, void *dest, size_t dest_size,
                  const void *src, size_t src_size);
ssize_t coroutine_fn
qcow2_co_decompress(BlockDriverState *bs, void *dest, size_t dest_size,
                    const void *src, size_t src_size);
#endif
