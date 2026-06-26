
#ifndef BLOCK_INT_GLOBAL_STATE_H
#define BLOCK_INT_GLOBAL_STATE_H

#include "block/blockjob.h"
#include "block/block_int-common.h"
#include "qemu/hbitmap.h"
#include "qemu/main-loop.h"


void stream_start(const char *job_id, BlockDriverState *bs,
                  BlockDriverState *base, const char *backing_file_str,
                  bool backing_mask_protocol,
                  BlockDriverState *bottom,
                  int creation_flags, int64_t speed,
                  BlockdevOnError on_error,
                  const char *filter_node_name,
                  Error **errp);

void commit_start(const char *job_id, BlockDriverState *bs,
                  BlockDriverState *base, BlockDriverState *top,
                  int creation_flags, int64_t speed,
                  BlockdevOnError on_error, const char *backing_file_str,
                  bool backing_mask_protocol,
                  const char *filter_node_name, Error **errp);
BlockJob *commit_active_start(const char *job_id, BlockDriverState *bs,
                              BlockDriverState *base, int creation_flags,
                              int64_t speed, BlockdevOnError on_error,
                              const char *filter_node_name,
                              BlockCompletionFunc *cb, void *opaque,
                              bool auto_complete, Error **errp);
void mirror_start(const char *job_id, BlockDriverState *bs,
                  BlockDriverState *target, const char *replaces,
                  int creation_flags, int64_t speed,
                  uint32_t granularity, int64_t buf_size,
                  MirrorSyncMode mode, BlockMirrorBackingMode backing_mode,
                  bool zero_target,
                  BlockdevOnError on_source_error,
                  BlockdevOnError on_target_error,
                  bool unmap, const char *filter_node_name,
                  MirrorCopyMode copy_mode, Error **errp);

BlockJob *backup_job_create(const char *job_id, BlockDriverState *bs,
                            BlockDriverState *target, int64_t speed,
                            MirrorSyncMode sync_mode,
                            BdrvDirtyBitmap *sync_bitmap,
                            BitmapSyncMode bitmap_mode,
                            bool compress, bool discard_source,
                            const char *filter_node_name,
                            BackupPerf *perf,
                            BlockdevOnError on_source_error,
                            BlockdevOnError on_target_error,
                            int creation_flags,
                            BlockCompletionFunc *cb, void *opaque,
                            JobTxn *txn, Error **errp);

BdrvChild * GRAPH_WRLOCK
bdrv_root_attach_child(BlockDriverState *child_bs, const char *child_name,
                       const BdrvChildClass *child_class,
                       BdrvChildRole child_role,
                       uint64_t perm, uint64_t shared_perm,
                       void *opaque, Error **errp);

void GRAPH_WRLOCK bdrv_root_unref_child(BdrvChild *child);

void GRAPH_RDLOCK bdrv_get_cumulative_perm(BlockDriverState *bs, uint64_t *perm,
                                           uint64_t *shared_perm);

int GRAPH_RDLOCK
bdrv_child_try_set_perm(BdrvChild *c, uint64_t perm, uint64_t shared,
                        Error **errp);

int GRAPH_RDLOCK
bdrv_child_refresh_perms(BlockDriverState *bs, BdrvChild *c, Error **errp);

bool GRAPH_RDLOCK bdrv_recurse_can_replace(BlockDriverState *bs,
                                           BlockDriverState *to_replace);

void bdrv_default_perms(BlockDriverState *bs, BdrvChild *c,
                        BdrvChildRole role, BlockReopenQueue *reopen_queue,
                        uint64_t perm, uint64_t shared,
                        uint64_t *nperm, uint64_t *nshared);

void blk_dev_change_media_cb(BlockBackend *blk, bool load, Error **errp);
bool blk_dev_has_removable_media(BlockBackend *blk);
void blk_dev_eject_request(BlockBackend *blk, bool force);
bool blk_dev_is_medium_locked(BlockBackend *blk);

void bdrv_restore_dirty_bitmap(BdrvDirtyBitmap *bitmap, HBitmap *backup);

void bdrv_set_monitor_owned(BlockDriverState *bs);

void blockdev_close_all_bdrv_states(void);

BlockDriverState *bds_tree_init(QDict *bs_opts, Error **errp);

int coroutine_fn bdrv_co_create_opts_simple(BlockDriver *drv,
                                            const char *filename,
                                            QemuOpts *opts,
                                            Error **errp);

BdrvDirtyBitmap *block_dirty_bitmap_lookup(const char *node,
                                           const char *name,
                                           BlockDriverState **pbs,
                                           Error **errp);
BdrvDirtyBitmap *block_dirty_bitmap_merge(const char *node, const char *target,
                                          BlockDirtyBitmapOrStrList *bms,
                                          HBitmap **backup, Error **errp);
BdrvDirtyBitmap *block_dirty_bitmap_remove(const char *node, const char *name,
                                           bool release,
                                           BlockDriverState **bitmap_bs,
                                           Error **errp);


BlockDriverState * GRAPH_RDLOCK
bdrv_skip_implicit_filters(BlockDriverState *bs);

void bdrv_add_aio_context_notifier(BlockDriverState *bs,
        void (*attached_aio_context)(AioContext *new_context, void *opaque),
        void (*detach_aio_context)(void *opaque), void *opaque);

void bdrv_remove_aio_context_notifier(BlockDriverState *bs,
                                      void (*aio_context_attached)(AioContext *,
                                                                   void *),
                                      void (*aio_context_detached)(void *),
                                      void *opaque);

void bdrv_drain_all_end_quiesce(BlockDriverState *bs);

#endif /* BLOCK_INT_GLOBAL_STATE_H */
