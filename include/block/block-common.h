#ifndef BLOCK_COMMON_H
#define BLOCK_COMMON_H

#include "qapi/qapi-types-block-core.h"
#include "qemu/queue.h"

#define co_wrapper                     no_coroutine_fn
#define co_wrapper_mixed               no_coroutine_fn coroutine_mixed_fn
#define co_wrapper_bdrv_rdlock         no_coroutine_fn
#define co_wrapper_mixed_bdrv_rdlock   no_coroutine_fn coroutine_mixed_fn

#define no_co_wrapper
#define no_co_wrapper_bdrv_rdlock
#define no_co_wrapper_bdrv_wrlock

#include "block/blockjob.h"

typedef struct BlockDriver BlockDriver;
typedef struct BdrvChild BdrvChild;
typedef struct BdrvChildClass BdrvChildClass;

typedef enum BlockZoneOp {
    BLK_ZO_OPEN,
    BLK_ZO_CLOSE,
    BLK_ZO_FINISH,
    BLK_ZO_RESET,
} BlockZoneOp;

typedef enum BlockZoneModel {
    BLK_Z_NONE = 0x0, /* Regular block device */
    BLK_Z_HM = 0x1, /* Host-managed zoned block device */
    BLK_Z_HA = 0x2, /* Host-aware zoned block device */
} BlockZoneModel;

typedef enum BlockZoneState {
    BLK_ZS_NOT_WP = 0x0,
    BLK_ZS_EMPTY = 0x1,
    BLK_ZS_IOPEN = 0x2,
    BLK_ZS_EOPEN = 0x3,
    BLK_ZS_CLOSED = 0x4,
    BLK_ZS_RDONLY = 0xD,
    BLK_ZS_FULL = 0xE,
    BLK_ZS_OFFLINE = 0xF,
} BlockZoneState;

typedef enum BlockZoneType {
    BLK_ZT_CONV = 0x1, /* Conventional random writes supported */
    BLK_ZT_SWR = 0x2, /* Sequential writes required */
    BLK_ZT_SWP = 0x3, /* Sequential writes preferred */
} BlockZoneType;

typedef struct BlockZoneDescriptor {
    uint64_t start;
    uint64_t length;
    uint64_t cap;
    uint64_t wp;
    BlockZoneType type;
    BlockZoneState state;
} BlockZoneDescriptor;

typedef struct BlockZoneWps {
    CoMutex colock;
    uint64_t wp[];
} BlockZoneWps;

typedef struct BlockDriverInfo {
    int cluster_size;
    int subcluster_size;
    int64_t vm_state_offset;
    bool is_dirty;
    bool needs_compressed_writes;
} BlockDriverInfo;

typedef struct BlockFragInfo {
    uint64_t allocated_clusters;
    uint64_t total_clusters;
    uint64_t fragmented_clusters;
    uint64_t compressed_clusters;
} BlockFragInfo;

typedef enum {
    BDRV_REQ_COPY_ON_READ       = 0x1,
    BDRV_REQ_ZERO_WRITE         = 0x2,

    BDRV_REQ_MAY_UNMAP          = 0x4,

    BDRV_REQ_REGISTERED_BUF     = 0x8,

    BDRV_REQ_FUA                = 0x10,
    BDRV_REQ_WRITE_COMPRESSED   = 0x20,

    BDRV_REQ_WRITE_UNCHANGED    = 0x40,

    BDRV_REQ_SERIALISING        = 0x80,

    BDRV_REQ_NO_FALLBACK        = 0x100,

    BDRV_REQ_PREFETCH  = 0x200,

    BDRV_REQ_NO_WAIT = 0x400,

    BDRV_REQ_MASK               = 0x7ff,
} BdrvRequestFlags;

#define BDRV_O_NO_SHARE    0x0001 /* don't share permissions */
#define BDRV_O_RDWR        0x0002
#define BDRV_O_RESIZE      0x0004 /* request permission for resizing the node */
#define BDRV_O_SNAPSHOT    0x0008 /* open the file read only and save
                                     writes in a snapshot */
#define BDRV_O_TEMPORARY   0x0010 /* delete the file after use */
#define BDRV_O_NOCACHE     0x0020 /* do not use the host page cache */
#define BDRV_O_NATIVE_AIO  0x0080 /* use native AIO instead of the
                                     thread pool */
#define BDRV_O_NO_BACKING  0x0100 /* don't open the backing file */
#define BDRV_O_NO_FLUSH    0x0200 /* disable flushing on this disk */
#define BDRV_O_COPY_ON_READ 0x0400 /* copy read backing sectors into image */
#define BDRV_O_INACTIVE    0x0800  /* consistency hint for migration handoff */
#define BDRV_O_CHECK       0x1000  /* open solely for consistency check */
#define BDRV_O_ALLOW_RDWR  0x2000  /* allow reopen to change from r/o to r/w */
#define BDRV_O_UNMAP       0x4000  /* execute guest UNMAP/TRIM operations */
#define BDRV_O_PROTOCOL    0x8000  /* if no block driver is explicitly given:
                                      select an appropriate protocol driver,
                                      ignoring the format layer */
#define BDRV_O_NO_IO       0x10000 /* don't initialize for I/O */
#define BDRV_O_AUTO_RDONLY 0x20000 /* degrade to read-only if opening
                                      read-write fails */
#define BDRV_O_IO_URING    0x40000 /* use io_uring instead of the thread pool */

#define BDRV_O_CBW_DISCARD_SOURCE 0x80000 /* for copy-before-write filter */

#define BDRV_O_CACHE_MASK  (BDRV_O_NOCACHE | BDRV_O_NO_FLUSH)



#define BDRV_OPT_CACHE_WB       "cache.writeback"
#define BDRV_OPT_CACHE_DIRECT   "cache.direct"
#define BDRV_OPT_CACHE_NO_FLUSH "cache.no-flush"
#define BDRV_OPT_READ_ONLY      "read-only"
#define BDRV_OPT_AUTO_READ_ONLY "auto-read-only"
#define BDRV_OPT_DISCARD        "discard"
#define BDRV_OPT_FORCE_SHARE    "force-share"
#define BDRV_OPT_ACTIVE         "active"


#define BDRV_SECTOR_BITS   9
#define BDRV_SECTOR_SIZE   (1ULL << BDRV_SECTOR_BITS)

#define BDRV_ZT_IS_CONV(wp)    (wp & (1ULL << 63))

#define BDRV_REQUEST_MAX_SECTORS MIN_CONST(SIZE_MAX >> BDRV_SECTOR_BITS, \
                                           INT_MAX >> BDRV_SECTOR_BITS)
#define BDRV_REQUEST_MAX_BYTES (BDRV_REQUEST_MAX_SECTORS << BDRV_SECTOR_BITS)

#define BDRV_MAX_ALIGNMENT (1L << 30)
#define BDRV_MAX_LENGTH (QEMU_ALIGN_DOWN(INT64_MAX, BDRV_MAX_ALIGNMENT))

#define BDRV_BLOCK_DATA         0x01
#define BDRV_BLOCK_ZERO         0x02
#define BDRV_BLOCK_OFFSET_VALID 0x04
#define BDRV_BLOCK_RAW          0x08
#define BDRV_BLOCK_ALLOCATED    0x10
#define BDRV_BLOCK_EOF          0x20
#define BDRV_BLOCK_RECURSE      0x40
#define BDRV_BLOCK_COMPRESSED   0x80

typedef QTAILQ_HEAD(BlockReopenQueue, BlockReopenQueueEntry) BlockReopenQueue;

typedef struct BDRVReopenState {
    BlockDriverState *bs;
    int flags;
    BlockdevDetectZeroesOptions detect_zeroes;
    bool backing_missing;
    BlockDriverState *old_backing_bs; /* keep pointer for permissions update */
    BlockDriverState *old_file_bs; /* keep pointer for permissions update */
    QDict *options;
    QDict *explicit_options;
    void *opaque;
} BDRVReopenState;

typedef enum BlockOpType {
    BLOCK_OP_TYPE_BACKUP_SOURCE,
    BLOCK_OP_TYPE_BACKUP_TARGET,
    BLOCK_OP_TYPE_CHANGE,
    BLOCK_OP_TYPE_COMMIT_SOURCE,
    BLOCK_OP_TYPE_COMMIT_TARGET,
    BLOCK_OP_TYPE_DRIVE_DEL,
    BLOCK_OP_TYPE_EJECT,
    BLOCK_OP_TYPE_EXTERNAL_SNAPSHOT,
    BLOCK_OP_TYPE_INTERNAL_SNAPSHOT,
    BLOCK_OP_TYPE_INTERNAL_SNAPSHOT_DELETE,
    BLOCK_OP_TYPE_MIRROR_SOURCE,
    BLOCK_OP_TYPE_MIRROR_TARGET,
    BLOCK_OP_TYPE_RESIZE,
    BLOCK_OP_TYPE_STREAM,
    BLOCK_OP_TYPE_REPLACE,
    BLOCK_OP_TYPE_MAX,
} BlockOpType;

enum {
    BLK_PERM_CONSISTENT_READ    = 0x01,

    BLK_PERM_WRITE              = 0x02,

    BLK_PERM_WRITE_UNCHANGED    = 0x04,

    BLK_PERM_RESIZE             = 0x08,


    BLK_PERM_ALL                = 0x0f,

    DEFAULT_PERM_PASSTHROUGH    = BLK_PERM_CONSISTENT_READ
                                 | BLK_PERM_WRITE
                                 | BLK_PERM_WRITE_UNCHANGED
                                 | BLK_PERM_RESIZE,

    DEFAULT_PERM_UNCHANGED      = BLK_PERM_ALL & ~DEFAULT_PERM_PASSTHROUGH,
};

enum BdrvChildRoleBits {
    BDRV_CHILD_DATA         = (1 << 0),

    BDRV_CHILD_METADATA     = (1 << 1),

    BDRV_CHILD_FILTERED     = (1 << 2),

    BDRV_CHILD_COW          = (1 << 3),

    BDRV_CHILD_PRIMARY      = (1 << 4),

    BDRV_CHILD_IMAGE        = BDRV_CHILD_DATA
                              | BDRV_CHILD_METADATA
                              | BDRV_CHILD_PRIMARY,
};

typedef unsigned int BdrvChildRole;

typedef struct BdrvCheckResult {
    int corruptions;
    int leaks;
    int check_errors;
    int corruptions_fixed;
    int leaks_fixed;
    int64_t image_end_offset;
    BlockFragInfo bfi;
} BdrvCheckResult;

typedef enum {
    BDRV_FIX_LEAKS    = 1,
    BDRV_FIX_ERRORS   = 2,
} BdrvCheckMode;

typedef struct BlockSizes {
    uint32_t phys;
    uint32_t log;
} BlockSizes;

typedef struct HDGeometry {
    uint32_t heads;
    uint32_t sectors;
    uint32_t cylinders;
} HDGeometry;


char *bdrv_perm_names(uint64_t perm);
uint64_t bdrv_qapi_perm_to_blk_perm(BlockPermission qapi_perm);

void bdrv_init_with_whitelist(void);
bool bdrv_uses_whitelist(void);
int bdrv_is_whitelisted(BlockDriver *drv, bool read_only);

int bdrv_parse_aio(const char *mode, int *flags);
int bdrv_parse_cache_mode(const char *mode, int *flags, bool *writethrough);
int bdrv_parse_discard_flags(const char *mode, int *flags);

int path_has_protocol(const char *path);
int path_is_absolute(const char *path);
char *path_combine(const char *base_path, const char *filename);

char *bdrv_get_full_backing_filename_from_filename(const char *backed,
                                                   const char *backing,
                                                   Error **errp);

#endif /* BLOCK_COMMON_H */
