#ifndef BLOCK_INT_COMMON_H
#define BLOCK_INT_COMMON_H

#include "block/aio.h"
#include "block/block-common.h"
#include "block/block-global-state.h"
#include "block/snapshot.h"
#include "qemu/iov.h"
#include "qemu/rcu.h"
#include "qemu/stats64.h"

#define BLOCK_FLAG_LAZY_REFCOUNTS   8

#define BLOCK_OPT_SIZE              "size"
#define BLOCK_OPT_ENCRYPT           "encryption"
#define BLOCK_OPT_ENCRYPT_FORMAT    "encrypt.format"
#define BLOCK_OPT_COMPAT6           "compat6"
#define BLOCK_OPT_HWVERSION         "hwversion"
#define BLOCK_OPT_BACKING_FILE      "backing_file"
#define BLOCK_OPT_BACKING_FMT       "backing_fmt"
#define BLOCK_OPT_CLUSTER_SIZE      "cluster_size"
#define BLOCK_OPT_TABLE_SIZE        "table_size"
#define BLOCK_OPT_PREALLOC          "preallocation"
#define BLOCK_OPT_SUBFMT            "subformat"
#define BLOCK_OPT_COMPAT_LEVEL      "compat"
#define BLOCK_OPT_LAZY_REFCOUNTS    "lazy_refcounts"
#define BLOCK_OPT_ADAPTER_TYPE      "adapter_type"
#define BLOCK_OPT_REDUNDANCY        "redundancy"
#define BLOCK_OPT_NOCOW             "nocow"
#define BLOCK_OPT_EXTENT_SIZE_HINT  "extent_size_hint"
#define BLOCK_OPT_OBJECT_SIZE       "object_size"
#define BLOCK_OPT_REFCOUNT_BITS     "refcount_bits"
#define BLOCK_OPT_DATA_FILE         "data_file"
#define BLOCK_OPT_DATA_FILE_RAW     "data_file_raw"
#define BLOCK_OPT_COMPRESSION_TYPE  "compression_type"
#define BLOCK_OPT_EXTL2             "extended_l2"

#define BLOCK_PROBE_BUF_SIZE        512

enum BdrvTrackedRequestType {
    BDRV_TRACKED_READ,
    BDRV_TRACKED_WRITE,
    BDRV_TRACKED_DISCARD,
    BDRV_TRACKED_TRUNCATE,
};

typedef struct BdrvTrackedRequest {
    BlockDriverState *bs;
    int64_t offset;
    int64_t bytes;
    enum BdrvTrackedRequestType type;

    bool serialising;
    int64_t overlap_offset;
    int64_t overlap_bytes;

    QLIST_ENTRY(BdrvTrackedRequest) list;
    Coroutine *co; /* owner, used for deadlock detection */
    CoQueue wait_queue; /* coroutines blocked on this request */

    struct BdrvTrackedRequest *waiting_for;
} BdrvTrackedRequest;


struct BlockDriver {

    const char *format_name;
    int instance_size;

    bool is_filter;
    bool filtered_child_is_backing;

    bool is_format;

    bool supports_zoned_children;

    bool bdrv_needs_filename;

    bool supports_backing;

    const char *protocol_name;

    QemuOptsList *create_opts;

    QemuOptsList *amend_opts;

    const char *const *mutable_opts;

    const char *const *strong_runtime_opts;



    int GRAPH_RDLOCK_PTR (*bdrv_amend_pre_run)(
        BlockDriverState *bs, Error **errp);
    void GRAPH_RDLOCK_PTR (*bdrv_amend_clean)(BlockDriverState *bs);

    bool GRAPH_RDLOCK_PTR (*bdrv_recurse_can_replace)(
        BlockDriverState *bs, BlockDriverState *to_replace);

    int (*bdrv_probe_device)(const char *filename);

    void (*bdrv_parse_filename)(const char *filename, QDict *options,
                                Error **errp);

    int GRAPH_UNLOCKED_PTR (*bdrv_reopen_prepare)(
        BDRVReopenState *reopen_state, BlockReopenQueue *queue, Error **errp);
    void GRAPH_UNLOCKED_PTR (*bdrv_reopen_commit)(
        BDRVReopenState *reopen_state);
    void GRAPH_UNLOCKED_PTR (*bdrv_reopen_commit_post)(
        BDRVReopenState *reopen_state);
    void GRAPH_UNLOCKED_PTR (*bdrv_reopen_abort)(
        BDRVReopenState *reopen_state);
    void (*bdrv_join_options)(QDict *options, QDict *old_options);

    int GRAPH_UNLOCKED_PTR (*bdrv_open)(
        BlockDriverState *bs, QDict *options, int flags, Error **errp);

    void (*bdrv_close)(BlockDriverState *bs);

    int coroutine_fn GRAPH_UNLOCKED_PTR (*bdrv_co_create)(
        BlockdevCreateOptions *opts, Error **errp);

    int coroutine_fn GRAPH_UNLOCKED_PTR (*bdrv_co_create_opts)(
        BlockDriver *drv, const char *filename, QemuOpts *opts, Error **errp);

    int GRAPH_RDLOCK_PTR (*bdrv_amend_options)(
        BlockDriverState *bs, QemuOpts *opts,
        BlockDriverAmendStatusCB *status_cb, void *cb_opaque,
        bool force, Error **errp);

    int GRAPH_RDLOCK_PTR (*bdrv_make_empty)(BlockDriverState *bs);

    void GRAPH_RDLOCK_PTR (*bdrv_refresh_filename)(BlockDriverState *bs);

    void GRAPH_RDLOCK_PTR (*bdrv_gather_child_options)(
        BlockDriverState *bs, QDict *target, bool backing_overridden);

    char * GRAPH_RDLOCK_PTR (*bdrv_dirname)(BlockDriverState *bs, Error **errp);

    void GRAPH_RDLOCK_PTR (*bdrv_cancel_in_flight)(BlockDriverState *bs);

    int GRAPH_RDLOCK_PTR (*bdrv_inactivate)(BlockDriverState *bs);

    int GRAPH_RDLOCK_PTR (*bdrv_snapshot_create)(
        BlockDriverState *bs, QEMUSnapshotInfo *sn_info);

    int GRAPH_UNLOCKED_PTR (*bdrv_snapshot_goto)(
        BlockDriverState *bs, const char *snapshot_id);

    int GRAPH_RDLOCK_PTR (*bdrv_snapshot_delete)(
        BlockDriverState *bs, const char *snapshot_id, const char *name,
        Error **errp);

    int GRAPH_RDLOCK_PTR (*bdrv_snapshot_list)(
        BlockDriverState *bs, QEMUSnapshotInfo **psn_info);

    int GRAPH_RDLOCK_PTR (*bdrv_snapshot_load_tmp)(
        BlockDriverState *bs, const char *snapshot_id, const char *name,
        Error **errp);

    int coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_change_backing_file)(
        BlockDriverState *bs, const char *backing_file,
        const char *backing_fmt);

    int (*bdrv_debug_breakpoint)(BlockDriverState *bs, const char *event,
        const char *tag);
    int (*bdrv_debug_remove_breakpoint)(BlockDriverState *bs,
        const char *tag);
    int (*bdrv_debug_resume)(BlockDriverState *bs, const char *tag);
    bool (*bdrv_debug_is_suspended)(BlockDriverState *bs, const char *tag);

    void GRAPH_RDLOCK_PTR (*bdrv_refresh_limits)(
        BlockDriverState *bs, Error **errp);

    int GRAPH_RDLOCK_PTR (*bdrv_has_zero_init)(BlockDriverState *bs);

    void (*bdrv_detach_aio_context)(BlockDriverState *bs);

    void (*bdrv_attach_aio_context)(BlockDriverState *bs,
                                    AioContext *new_context);

    void (*bdrv_drain_begin)(BlockDriverState *bs);
    void (*bdrv_drain_end)(BlockDriverState *bs);

    int GRAPH_RDLOCK_PTR (*bdrv_probe_blocksizes)(
        BlockDriverState *bs, BlockSizes *bsz);
    int GRAPH_RDLOCK_PTR (*bdrv_probe_geometry)(
        BlockDriverState *bs, HDGeometry *geo);

    void GRAPH_WRLOCK_PTR (*bdrv_add_child)(
        BlockDriverState *parent, BlockDriverState *child, Error **errp);

    void GRAPH_WRLOCK_PTR (*bdrv_del_child)(
        BlockDriverState *parent, BdrvChild *child, Error **errp);

    int GRAPH_RDLOCK_PTR (*bdrv_check_perm)(BlockDriverState *bs, uint64_t perm,
                                            uint64_t shared, Error **errp);

    void GRAPH_RDLOCK_PTR (*bdrv_set_perm)(
        BlockDriverState *bs, uint64_t perm, uint64_t shared);

    void GRAPH_RDLOCK_PTR (*bdrv_abort_perm_update)(BlockDriverState *bs);

     void GRAPH_RDLOCK_PTR (*bdrv_child_perm)(
        BlockDriverState *bs, BdrvChild *c, BdrvChildRole role,
        BlockReopenQueue *reopen_queue,
        uint64_t parent_perm, uint64_t parent_shared,
        uint64_t *nperm, uint64_t *nshared);

    bool GRAPH_RDLOCK_PTR (*bdrv_register_buf)(
        BlockDriverState *bs, void *host, size_t size, Error **errp);
    void GRAPH_RDLOCK_PTR (*bdrv_unregister_buf)(
        BlockDriverState *bs, void *host, size_t size);

    QLIST_ENTRY(BlockDriver) list;


    int (*bdrv_probe)(const uint8_t *buf, int buf_size, const char *filename);

    int coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_amend)(
        BlockDriverState *bs, BlockdevAmendOptions *opts, bool force,
        Error **errp);

    BlockAIOCB * GRAPH_RDLOCK_PTR (*bdrv_aio_preadv)(BlockDriverState *bs,
        int64_t offset, int64_t bytes, QEMUIOVector *qiov,
        BdrvRequestFlags flags, BlockCompletionFunc *cb, void *opaque);

    BlockAIOCB * GRAPH_RDLOCK_PTR (*bdrv_aio_pwritev)(BlockDriverState *bs,
        int64_t offset, int64_t bytes, QEMUIOVector *qiov,
        BdrvRequestFlags flags, BlockCompletionFunc *cb, void *opaque);

    BlockAIOCB * GRAPH_RDLOCK_PTR (*bdrv_aio_flush)(
        BlockDriverState *bs, BlockCompletionFunc *cb, void *opaque);

    BlockAIOCB * GRAPH_RDLOCK_PTR (*bdrv_aio_pdiscard)(
        BlockDriverState *bs, int64_t offset, int bytes,
        BlockCompletionFunc *cb, void *opaque);

    int coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_readv)(BlockDriverState *bs,
        int64_t sector_num, int nb_sectors, QEMUIOVector *qiov);

    int coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_preadv)(BlockDriverState *bs,
        int64_t offset, int64_t bytes, QEMUIOVector *qiov,
        BdrvRequestFlags flags);

    int coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_preadv_part)(
        BlockDriverState *bs, int64_t offset, int64_t bytes,
        QEMUIOVector *qiov, size_t qiov_offset,
        BdrvRequestFlags flags);

    int coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_writev)(BlockDriverState *bs,
        int64_t sector_num, int nb_sectors, QEMUIOVector *qiov,
        int flags);
    int coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_pwritev)(
        BlockDriverState *bs, int64_t offset, int64_t bytes, QEMUIOVector *qiov,
        BdrvRequestFlags flags);
    int coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_pwritev_part)(
        BlockDriverState *bs, int64_t offset, int64_t bytes, QEMUIOVector *qiov,
        size_t qiov_offset, BdrvRequestFlags flags);

    int coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_pwrite_zeroes)(
        BlockDriverState *bs, int64_t offset, int64_t bytes,
        BdrvRequestFlags flags);

    int coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_pdiscard)(
        BlockDriverState *bs, int64_t offset, int64_t bytes);

    int coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_copy_range_from)(
        BlockDriverState *bs, BdrvChild *src, int64_t offset,
        BdrvChild *dst, int64_t dst_offset, int64_t bytes,
        BdrvRequestFlags read_flags, BdrvRequestFlags write_flags);

    int coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_copy_range_to)(
        BlockDriverState *bs, BdrvChild *src, int64_t src_offset,
        BdrvChild *dst, int64_t dst_offset, int64_t bytes,
        BdrvRequestFlags read_flags, BdrvRequestFlags write_flags);

    int coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_block_status)(
        BlockDriverState *bs,
        bool want_zero, int64_t offset, int64_t bytes, int64_t *pnum,
        int64_t *map, BlockDriverState **file);

    int coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_preadv_snapshot)(
        BlockDriverState *bs, int64_t offset, int64_t bytes,
        QEMUIOVector *qiov, size_t qiov_offset);

    int coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_snapshot_block_status)(
        BlockDriverState *bs, bool want_zero, int64_t offset, int64_t bytes,
        int64_t *pnum, int64_t *map, BlockDriverState **file);

    int coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_pdiscard_snapshot)(
        BlockDriverState *bs, int64_t offset, int64_t bytes);

    void coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_invalidate_cache)(
        BlockDriverState *bs, Error **errp);

    int coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_flush)(BlockDriverState *bs);

    int coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_delete_file)(
        BlockDriverState *bs, Error **errp);

    int coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_flush_to_disk)(
        BlockDriverState *bs);

    int coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_flush_to_os)(
        BlockDriverState *bs);

    int coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_truncate)(
        BlockDriverState *bs, int64_t offset, bool exact,
        PreallocMode prealloc, BdrvRequestFlags flags, Error **errp);

    int64_t coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_getlength)(
        BlockDriverState *bs);

    int64_t coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_get_allocated_file_size)(
        BlockDriverState *bs);

    BlockMeasureInfo *(*bdrv_measure)(QemuOpts *opts, BlockDriverState *in_bs,
                                      Error **errp);

    int coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_pwritev_compressed)(
        BlockDriverState *bs, int64_t offset, int64_t bytes,
        QEMUIOVector *qiov);

    int coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_pwritev_compressed_part)(
        BlockDriverState *bs, int64_t offset, int64_t bytes,
        QEMUIOVector *qiov, size_t qiov_offset);

    int coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_get_info)(
        BlockDriverState *bs, BlockDriverInfo *bdi);

    ImageInfoSpecific * GRAPH_RDLOCK_PTR (*bdrv_get_specific_info)(
        BlockDriverState *bs, Error **errp);
    BlockStatsSpecific *(*bdrv_get_specific_stats)(BlockDriverState *bs);

    int coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_save_vmstate)(
        BlockDriverState *bs, QEMUIOVector *qiov, int64_t pos);

    int coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_load_vmstate)(
        BlockDriverState *bs, QEMUIOVector *qiov, int64_t pos);

    int coroutine_fn (*bdrv_co_zone_report)(BlockDriverState *bs,
            int64_t offset, unsigned int *nr_zones,
            BlockZoneDescriptor *zones);
    int coroutine_fn (*bdrv_co_zone_mgmt)(BlockDriverState *bs, BlockZoneOp op,
            int64_t offset, int64_t len);
    int coroutine_fn (*bdrv_co_zone_append)(BlockDriverState *bs,
            int64_t *offset, QEMUIOVector *qiov,
            BdrvRequestFlags flags);

    bool coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_is_inserted)(
        BlockDriverState *bs);
    void coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_eject)(
        BlockDriverState *bs, bool eject_flag);
    void coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_lock_medium)(
        BlockDriverState *bs, bool locked);

    BlockAIOCB *coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_aio_ioctl)(
        BlockDriverState *bs, unsigned long int req, void *buf,
        BlockCompletionFunc *cb, void *opaque);

    int coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_ioctl)(
        BlockDriverState *bs, unsigned long int req, void *buf);

    int coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_check)(
        BlockDriverState *bs, BdrvCheckResult *result, BdrvCheckMode fix);

    void coroutine_fn GRAPH_RDLOCK_PTR (*bdrv_co_debug_event)(
        BlockDriverState *bs, BlkdebugEvent event);

};

static inline bool TSA_NO_TSA block_driver_can_compress(BlockDriver *drv)
{
    return drv->bdrv_co_pwritev_compressed ||
           drv->bdrv_co_pwritev_compressed_part;
}

typedef struct BlockLimits {
    uint32_t request_alignment;

    int64_t max_pdiscard;

    uint32_t pdiscard_alignment;

    int64_t max_pwrite_zeroes;

    uint32_t pwrite_zeroes_alignment;

    uint32_t opt_transfer;

    uint32_t max_transfer;

    uint64_t max_hw_transfer;

    int max_hw_iov;


    size_t min_mem_alignment;

    size_t opt_mem_alignment;

    int max_iov;

    bool has_variable_length;

    BlockZoneModel zoned;

    uint32_t zone_size;

    uint32_t nr_zones;

    uint32_t max_append_sectors;

    uint32_t max_open_zones;

    uint32_t max_active_zones;

    uint32_t write_granularity;
} BlockLimits;

typedef struct BdrvOpBlocker BdrvOpBlocker;

typedef struct BdrvAioNotifier {
    void (*attached_aio_context)(AioContext *new_context, void *opaque);
    void (*detach_aio_context)(void *opaque);

    void *opaque;
    bool deleted;

    QLIST_ENTRY(BdrvAioNotifier) list;
} BdrvAioNotifier;

struct BdrvChildClass {
    bool stay_at_node;

    bool parent_is_bds;

    void (*inherit_options)(BdrvChildRole role, bool parent_is_format,
                            int *child_flags, QDict *child_options,
                            int parent_flags, QDict *parent_options);
    void (*change_media)(BdrvChild *child, bool load);

    char *(*get_parent_desc)(BdrvChild *child);

    void GRAPH_RDLOCK_PTR (*activate)(BdrvChild *child, Error **errp);
    int GRAPH_RDLOCK_PTR (*inactivate)(BdrvChild *child);

    void GRAPH_WRLOCK_PTR (*attach)(BdrvChild *child);
    void GRAPH_WRLOCK_PTR (*detach)(BdrvChild *child);

    void GRAPH_RDLOCK_PTR (*drained_begin)(BdrvChild *child);
    void GRAPH_RDLOCK_PTR (*drained_end)(BdrvChild *child);

    bool GRAPH_RDLOCK_PTR (*drained_poll)(BdrvChild *child);

    int (*update_filename)(BdrvChild *child, BlockDriverState *new_base,
                           const char *filename,
                           bool backing_mask_protocol,
                           Error **errp);

    bool (*change_aio_ctx)(BdrvChild *child, AioContext *ctx,
                           GHashTable *visited, Transaction *tran,
                           Error **errp);


    void (*resize)(BdrvChild *child);

    const char *(*get_name)(BdrvChild *child);

    AioContext *(*get_parent_aio_context)(BdrvChild *child);
};

extern const BdrvChildClass child_of_bds;

struct BdrvChild {
    BlockDriverState *bs;
    char *name;
    const BdrvChildClass *klass;
    BdrvChildRole role;
    void *opaque;

    uint64_t perm;

    uint64_t shared_perm;

    bool frozen;

    bool quiesced_parent;

    QLIST_ENTRY(BdrvChild GRAPH_RDLOCK_PTR) next;
    QLIST_ENTRY(BdrvChild GRAPH_RDLOCK_PTR) next_parent;
};

typedef struct BdrvBlockStatusCache {
    struct rcu_head rcu;

    bool valid;
    int64_t data_start;
    int64_t data_end;
} BdrvBlockStatusCache;

struct BlockDriverState {
    int open_flags; /* flags used to open the file, re-used for re-open */
    bool encrypted; /* if true, the media is encrypted */
    bool sg;        /* if true, the device is a /dev/sg* */
    bool probed;    /* if true, format was probed rather than specified */
    bool force_share; /* if true, always allow all shared permissions */
    bool implicit;  /* if true, this filter node was automatically inserted */

    BlockDriver *drv; /* NULL means no media */
    void *opaque;

    AioContext *aio_context; /* event loop used for fd handlers, timers, etc */
    QLIST_HEAD(, BdrvAioNotifier) aio_notifiers;
    bool walking_aio_notifiers; /* to make removal during iteration safe */

    char filename[PATH_MAX];
    char backing_file[PATH_MAX];
    char auto_backing_file[PATH_MAX];
    char backing_format[16]; /* if non-zero and backing_file exists */

    QDict *full_open_options;
    char exact_filename[PATH_MAX];

    BlockLimits bl;

    BdrvRequestFlags supported_read_flags;
    BdrvRequestFlags supported_write_flags;
    BdrvRequestFlags supported_zero_flags;
    BdrvRequestFlags supported_truncate_flags;

    char node_name[32];
    QTAILQ_ENTRY(BlockDriverState) node_list;
    QTAILQ_ENTRY(BlockDriverState) bs_list;
    QTAILQ_ENTRY(BlockDriverState) monitor_list;
    int refcnt;

    QLIST_HEAD(, BdrvOpBlocker) op_blockers[BLOCK_OP_TYPE_MAX];

    BlockDriverState *inherits_from;

    QLIST_HEAD(, BdrvChild GRAPH_RDLOCK_PTR) children;
    BdrvChild * GRAPH_RDLOCK_PTR backing;
    BdrvChild * GRAPH_RDLOCK_PTR file;

    QLIST_HEAD(, BdrvChild GRAPH_RDLOCK_PTR) parents;

    QDict *options;
    QDict *explicit_options;
    BlockdevDetectZeroesOptions detect_zeroes;

    Error *backing_blocker;

    int64_t total_sectors;

    Stat64 wr_highest_offset;

    int copy_on_read;

    unsigned int in_flight;
    unsigned int serialising_in_flight;

    int enable_write_cache;

    int quiesce_counter;

    unsigned int write_gen;               /* Current data generation */

    QemuMutex reqs_lock;
    QLIST_HEAD(, BdrvTrackedRequest) tracked_requests;
    CoQueue flush_queue;                  /* Serializing flush queue */
    bool active_flush_req;                /* Flush request in flight? */

    unsigned int flushed_gen;             /* Flushed write generation */

    bool never_freeze;

    CoMutex bsc_modify_lock;
    BdrvBlockStatusCache *block_status_cache;

    BlockZoneWps *wps;
};

struct BlockBackendRootState {
    int open_flags;
    BlockdevDetectZeroesOptions detect_zeroes;
};

typedef enum BlockMirrorBackingMode {
    MIRROR_SOURCE_BACKING_CHAIN,

    MIRROR_OPEN_BACKING_CHAIN,

    MIRROR_LEAVE_BACKING_CHAIN,
} BlockMirrorBackingMode;


extern BlockDriver bdrv_file;
extern BlockDriver bdrv_raw;

extern unsigned int bdrv_drain_all_count;
extern QemuOptsList bdrv_create_opts_simple;


static inline BlockDriverState *child_bs(BdrvChild *child)
{
    return child ? child->bs : NULL;
}

int bdrv_check_request(int64_t offset, int64_t bytes, Error **errp);
char *create_tmp_file(Error **errp);
void bdrv_parse_filename_strip_prefix(const char *filename, const char *prefix,
                                      QDict *options);


int bdrv_check_qiov_request(int64_t offset, int64_t bytes,
                            QEMUIOVector *qiov, size_t qiov_offset,
                            Error **errp);

#ifdef _WIN32
int is_windows_drive(const char *filename);
#endif

#endif /* BLOCK_INT_COMMON_H */
