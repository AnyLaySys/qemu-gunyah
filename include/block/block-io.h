#ifndef BLOCK_IO_H
#define BLOCK_IO_H

#include "block/aio-wait.h"
#include "block/block-common.h"
#include "qemu/coroutine.h"
#include "qemu/iov.h"


int co_wrapper_mixed_bdrv_rdlock
bdrv_pwrite_zeroes(BdrvChild *child, int64_t offset, int64_t bytes,
                   BdrvRequestFlags flags);

int bdrv_make_zero(BdrvChild *child, BdrvRequestFlags flags);

int co_wrapper_mixed_bdrv_rdlock
bdrv_pread(BdrvChild *child, int64_t offset, int64_t bytes, void *buf,
           BdrvRequestFlags flags);

int co_wrapper_mixed_bdrv_rdlock
bdrv_pwrite(BdrvChild *child, int64_t offset,int64_t bytes,
            const void *buf, BdrvRequestFlags flags);

int co_wrapper_mixed_bdrv_rdlock
bdrv_pwrite_sync(BdrvChild *child, int64_t offset, int64_t bytes,
                 const void *buf, BdrvRequestFlags flags);

int coroutine_fn GRAPH_RDLOCK
bdrv_co_pwrite_sync(BdrvChild *child, int64_t offset, int64_t bytes,
                    const void *buf, BdrvRequestFlags flags);

int coroutine_fn GRAPH_RDLOCK
bdrv_co_pwrite_zeroes(BdrvChild *child, int64_t offset, int64_t bytes,
                      BdrvRequestFlags flags);

int coroutine_fn GRAPH_RDLOCK
bdrv_co_truncate(BdrvChild *child, int64_t offset, bool exact,
                 PreallocMode prealloc, BdrvRequestFlags flags, Error **errp);

int64_t coroutine_fn GRAPH_RDLOCK bdrv_co_nb_sectors(BlockDriverState *bs);
int64_t coroutine_mixed_fn bdrv_nb_sectors(BlockDriverState *bs);

int64_t coroutine_fn GRAPH_RDLOCK bdrv_co_getlength(BlockDriverState *bs);
int64_t co_wrapper_mixed_bdrv_rdlock bdrv_getlength(BlockDriverState *bs);

int64_t coroutine_fn GRAPH_RDLOCK
bdrv_co_get_allocated_file_size(BlockDriverState *bs);

int64_t co_wrapper_bdrv_rdlock
bdrv_get_allocated_file_size(BlockDriverState *bs);

BlockMeasureInfo *bdrv_measure(BlockDriver *drv, QemuOpts *opts,
                               BlockDriverState *in_bs, Error **errp);

int coroutine_fn GRAPH_RDLOCK
bdrv_co_delete_file(BlockDriverState *bs, Error **errp);

void coroutine_fn GRAPH_RDLOCK
bdrv_co_delete_file_noerr(BlockDriverState *bs);


void bdrv_aio_cancel_async(BlockAIOCB *acb);

int coroutine_fn GRAPH_RDLOCK
bdrv_co_ioctl(BlockDriverState *bs, int req, void *buf);

int coroutine_fn GRAPH_RDLOCK bdrv_co_flush(BlockDriverState *bs);

int coroutine_fn GRAPH_RDLOCK bdrv_co_pdiscard(BdrvChild *child, int64_t offset,
                                               int64_t bytes);

int coroutine_fn GRAPH_RDLOCK bdrv_co_zone_report(BlockDriverState *bs,
                                                  int64_t offset,
                                                  unsigned int *nr_zones,
                                                  BlockZoneDescriptor *zones);
int coroutine_fn GRAPH_RDLOCK bdrv_co_zone_mgmt(BlockDriverState *bs,
                                                BlockZoneOp op,
                                                int64_t offset, int64_t len);
int coroutine_fn GRAPH_RDLOCK bdrv_co_zone_append(BlockDriverState *bs,
                                                  int64_t *offset,
                                                  QEMUIOVector *qiov,
                                                  BdrvRequestFlags flags);

bool bdrv_can_write_zeroes_with_unmap(BlockDriverState *bs);

int coroutine_fn GRAPH_RDLOCK
bdrv_co_block_status(BlockDriverState *bs, int64_t offset, int64_t bytes,
                     int64_t *pnum, int64_t *map, BlockDriverState **file);
int co_wrapper_mixed_bdrv_rdlock
bdrv_block_status(BlockDriverState *bs, int64_t offset, int64_t bytes,
                  int64_t *pnum, int64_t *map, BlockDriverState **file);

int coroutine_fn GRAPH_RDLOCK
bdrv_co_block_status_above(BlockDriverState *bs, BlockDriverState *base,
                           int64_t offset, int64_t bytes, int64_t *pnum,
                           int64_t *map, BlockDriverState **file);
int co_wrapper_mixed_bdrv_rdlock
bdrv_block_status_above(BlockDriverState *bs, BlockDriverState *base,
                        int64_t offset, int64_t bytes, int64_t *pnum,
                        int64_t *map, BlockDriverState **file);

int coroutine_fn GRAPH_RDLOCK
bdrv_co_is_allocated(BlockDriverState *bs, int64_t offset, int64_t bytes,
                     int64_t *pnum);
int co_wrapper_mixed_bdrv_rdlock
bdrv_is_allocated(BlockDriverState *bs, int64_t offset,
                  int64_t bytes, int64_t *pnum);

int coroutine_fn GRAPH_RDLOCK
bdrv_co_is_allocated_above(BlockDriverState *top, BlockDriverState *base,
                           bool include_base, int64_t offset, int64_t bytes,
                           int64_t *pnum);
int co_wrapper_mixed_bdrv_rdlock
bdrv_is_allocated_above(BlockDriverState *bs, BlockDriverState *base,
                        bool include_base, int64_t offset,
                        int64_t bytes, int64_t *pnum);

int coroutine_fn GRAPH_RDLOCK
bdrv_co_is_zero_fast(BlockDriverState *bs, int64_t offset, int64_t bytes);

int GRAPH_RDLOCK
bdrv_apply_auto_read_only(BlockDriverState *bs, const char *errmsg,
                          Error **errp);

bool bdrv_is_read_only(BlockDriverState *bs);
bool bdrv_is_writable(BlockDriverState *bs);
bool bdrv_is_sg(BlockDriverState *bs);
int bdrv_get_flags(BlockDriverState *bs);

bool coroutine_fn GRAPH_RDLOCK bdrv_co_is_inserted(BlockDriverState *bs);
bool co_wrapper_bdrv_rdlock bdrv_is_inserted(BlockDriverState *bs);

void coroutine_fn GRAPH_RDLOCK
bdrv_co_lock_medium(BlockDriverState *bs, bool locked);

void coroutine_fn GRAPH_RDLOCK
bdrv_co_eject(BlockDriverState *bs, bool eject_flag);

const char *bdrv_get_format_name(BlockDriverState *bs);

bool GRAPH_RDLOCK bdrv_supports_compressed_writes(BlockDriverState *bs);
const char *bdrv_get_node_name(const BlockDriverState *bs);

const char * GRAPH_RDLOCK
bdrv_get_device_name(const BlockDriverState *bs);

const char * GRAPH_RDLOCK
bdrv_get_device_or_node_name(const BlockDriverState *bs);

int coroutine_fn GRAPH_RDLOCK
bdrv_co_get_info(BlockDriverState *bs, BlockDriverInfo *bdi);

int co_wrapper_mixed_bdrv_rdlock
bdrv_get_info(BlockDriverState *bs, BlockDriverInfo *bdi);

ImageInfoSpecific * GRAPH_RDLOCK
bdrv_get_specific_info(BlockDriverState *bs, Error **errp);

BlockStatsSpecific *bdrv_get_specific_stats(BlockDriverState *bs);
void bdrv_round_to_subclusters(BlockDriverState *bs,
                               int64_t offset, int64_t bytes,
                               int64_t *cluster_offset,
                               int64_t *cluster_bytes);

void bdrv_get_backing_filename(BlockDriverState *bs,
                               char *filename, int filename_size);

int coroutine_fn GRAPH_RDLOCK
bdrv_co_change_backing_file(BlockDriverState *bs, const char *backing_file,
                            const char *backing_fmt, bool warn);

int co_wrapper_bdrv_rdlock
bdrv_change_backing_file(BlockDriverState *bs, const char *backing_file,
                         const char *backing_fmt, bool warn);

int bdrv_save_vmstate(BlockDriverState *bs, const uint8_t *buf,
                      int64_t pos, int size);

int bdrv_load_vmstate(BlockDriverState *bs, uint8_t *buf,
                      int64_t pos, int size);

size_t bdrv_min_mem_align(BlockDriverState *bs);
size_t bdrv_opt_mem_align(BlockDriverState *bs);
void *qemu_blockalign(BlockDriverState *bs, size_t size);
void *qemu_blockalign0(BlockDriverState *bs, size_t size);
void *qemu_try_blockalign(BlockDriverState *bs, size_t size);
void *qemu_try_blockalign0(BlockDriverState *bs, size_t size);

void bdrv_enable_copy_on_read(BlockDriverState *bs);
void bdrv_disable_copy_on_read(BlockDriverState *bs);

void coroutine_fn GRAPH_RDLOCK
bdrv_co_debug_event(BlockDriverState *bs, BlkdebugEvent event);

void co_wrapper_mixed_bdrv_rdlock
bdrv_debug_event(BlockDriverState *bs, BlkdebugEvent event);

#define BLKDBG_CO_EVENT(child, evt) \
    do { \
        if (child) { \
            bdrv_co_debug_event(child->bs, evt); \
        } \
    } while (0)

#define BLKDBG_EVENT(child, evt) \
    do { \
        if (child) { \
            bdrv_debug_event(child->bs, evt); \
        } \
    } while (0)

AioContext *bdrv_get_aio_context(BlockDriverState *bs);

AioContext *bdrv_child_get_parent_aio_context(BdrvChild *c);

AioContext *coroutine_fn bdrv_co_enter(BlockDriverState *bs);

void coroutine_fn bdrv_co_leave(BlockDriverState *bs, AioContext *old_ctx);

AioContext *child_of_bds_get_parent_aio_context(BdrvChild *c);

int coroutine_fn GRAPH_RDLOCK
bdrv_co_copy_range(BdrvChild *src, int64_t src_offset,
                   BdrvChild *dst, int64_t dst_offset,
                   int64_t bytes, BdrvRequestFlags read_flags,
                   BdrvRequestFlags write_flags);


#define BDRV_POLL_WHILE(bs, cond) ({                       \
    BlockDriverState *bs_ = (bs);                          \
    IO_OR_GS_CODE();                                       \
    AIO_WAIT_WHILE(bdrv_get_aio_context(bs_),              \
                   cond); })

void bdrv_drain(BlockDriverState *bs);

int co_wrapper_mixed_bdrv_rdlock
bdrv_truncate(BdrvChild *child, int64_t offset, bool exact,
              PreallocMode prealloc, BdrvRequestFlags flags, Error **errp);

int co_wrapper_mixed_bdrv_rdlock
bdrv_check(BlockDriverState *bs, BdrvCheckResult *res, BdrvCheckMode fix);

int co_wrapper_mixed_bdrv_rdlock
bdrv_invalidate_cache(BlockDriverState *bs, Error **errp);

int co_wrapper_mixed_bdrv_rdlock bdrv_flush(BlockDriverState *bs);

int co_wrapper_mixed_bdrv_rdlock
bdrv_pdiscard(BdrvChild *child, int64_t offset, int64_t bytes);

int co_wrapper_mixed_bdrv_rdlock
bdrv_readv_vmstate(BlockDriverState *bs, QEMUIOVector *qiov, int64_t pos);

int co_wrapper_mixed_bdrv_rdlock
bdrv_writev_vmstate(BlockDriverState *bs, QEMUIOVector *qiov, int64_t pos);

void GRAPH_RDLOCK bdrv_parent_drained_begin_single(BdrvChild *c);

bool GRAPH_RDLOCK bdrv_parent_drained_poll_single(BdrvChild *c);

void GRAPH_RDLOCK bdrv_parent_drained_end_single(BdrvChild *c);

bool GRAPH_RDLOCK
bdrv_drain_poll(BlockDriverState *bs, BdrvChild *ignore_parent,
                bool ignore_bds_parents);

void bdrv_drained_begin(BlockDriverState *bs);

void bdrv_do_drained_begin_quiesce(BlockDriverState *bs, BdrvChild *parent);

void bdrv_drained_end(BlockDriverState *bs);

#endif /* BLOCK_IO_H */
