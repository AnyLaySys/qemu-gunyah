#ifndef BLOCK_INT_IO_H
#define BLOCK_INT_IO_H

#include "block/block_int-common.h"
#include "qemu/hbitmap.h"
#include "qemu/main-loop.h"


int coroutine_fn GRAPH_RDLOCK bdrv_co_preadv_snapshot(BdrvChild *child,
    int64_t offset, int64_t bytes, QEMUIOVector *qiov, size_t qiov_offset);
int coroutine_fn GRAPH_RDLOCK bdrv_co_snapshot_block_status(
    BlockDriverState *bs, bool want_zero, int64_t offset, int64_t bytes,
    int64_t *pnum, int64_t *map, BlockDriverState **file);
int coroutine_fn GRAPH_RDLOCK bdrv_co_pdiscard_snapshot(BlockDriverState *bs,
    int64_t offset, int64_t bytes);


int coroutine_fn GRAPH_RDLOCK bdrv_co_preadv(BdrvChild *child,
    int64_t offset, int64_t bytes, QEMUIOVector *qiov,
    BdrvRequestFlags flags);
int coroutine_fn GRAPH_RDLOCK bdrv_co_preadv_part(BdrvChild *child,
    int64_t offset, int64_t bytes,
    QEMUIOVector *qiov, size_t qiov_offset, BdrvRequestFlags flags);
int coroutine_fn GRAPH_RDLOCK bdrv_co_pwritev(BdrvChild *child,
    int64_t offset, int64_t bytes, QEMUIOVector *qiov,
    BdrvRequestFlags flags);
int coroutine_fn GRAPH_RDLOCK bdrv_co_pwritev_part(BdrvChild *child,
    int64_t offset, int64_t bytes,
    QEMUIOVector *qiov, size_t qiov_offset, BdrvRequestFlags flags);

static inline int coroutine_fn GRAPH_RDLOCK bdrv_co_pread(BdrvChild *child,
    int64_t offset, int64_t bytes, void *buf, BdrvRequestFlags flags)
{
    QEMUIOVector qiov = QEMU_IOVEC_INIT_BUF(qiov, buf, bytes);
    IO_CODE();
    assert_bdrv_graph_readable();

    return bdrv_co_preadv(child, offset, bytes, &qiov, flags);
}

static inline int coroutine_fn GRAPH_RDLOCK bdrv_co_pwrite(BdrvChild *child,
    int64_t offset, int64_t bytes, const void *buf, BdrvRequestFlags flags)
{
    QEMUIOVector qiov = QEMU_IOVEC_INIT_BUF(qiov, buf, bytes);
    IO_CODE();
    assert_bdrv_graph_readable();

    return bdrv_co_pwritev(child, offset, bytes, &qiov, flags);
}

void coroutine_fn bdrv_make_request_serialising(BdrvTrackedRequest *req,
                                                uint64_t align);
BdrvTrackedRequest *coroutine_fn bdrv_co_get_self_request(BlockDriverState *bs);

BlockDriver *bdrv_probe_all(const uint8_t *buf, int buf_size,
                            const char *filename);

void bdrv_wakeup(BlockDriverState *bs);

const char * GRAPH_RDLOCK bdrv_get_parent_name(const BlockDriverState *bs);
bool blk_dev_has_tray(BlockBackend *blk);
bool blk_dev_is_tray_open(BlockBackend *blk);

void bdrv_inc_in_flight(BlockDriverState *bs);
void bdrv_dec_in_flight(BlockDriverState *bs);

int coroutine_fn GRAPH_RDLOCK
bdrv_co_copy_range_from(BdrvChild *src, int64_t src_offset,
                        BdrvChild *dst, int64_t dst_offset,
                        int64_t bytes, BdrvRequestFlags read_flags,
                        BdrvRequestFlags write_flags);
int coroutine_fn GRAPH_RDLOCK
bdrv_co_copy_range_to(BdrvChild *src, int64_t src_offset,
                      BdrvChild *dst, int64_t dst_offset,
                      int64_t bytes, BdrvRequestFlags read_flags,
                      BdrvRequestFlags write_flags);

int coroutine_fn GRAPH_RDLOCK
bdrv_co_refresh_total_sectors(BlockDriverState *bs, int64_t hint);

int co_wrapper_mixed_bdrv_rdlock
bdrv_refresh_total_sectors(BlockDriverState *bs, int64_t hint);

BdrvChild * GRAPH_RDLOCK bdrv_cow_child(BlockDriverState *bs);
BdrvChild * GRAPH_RDLOCK bdrv_filter_child(BlockDriverState *bs);
BdrvChild * GRAPH_RDLOCK bdrv_filter_or_cow_child(BlockDriverState *bs);
BdrvChild * GRAPH_RDLOCK bdrv_primary_child(BlockDriverState *bs);
BlockDriverState * GRAPH_RDLOCK bdrv_skip_filters(BlockDriverState *bs);
BlockDriverState * GRAPH_RDLOCK bdrv_backing_chain_next(BlockDriverState *bs);

static inline BlockDriverState * GRAPH_RDLOCK
bdrv_cow_bs(BlockDriverState *bs)
{
    IO_CODE();
    return child_bs(bdrv_cow_child(bs));
}

static inline BlockDriverState * GRAPH_RDLOCK
bdrv_filter_bs(BlockDriverState *bs)
{
    IO_CODE();
    return child_bs(bdrv_filter_child(bs));
}

static inline BlockDriverState * GRAPH_RDLOCK
bdrv_filter_or_cow_bs(BlockDriverState *bs)
{
    IO_CODE();
    return child_bs(bdrv_filter_or_cow_child(bs));
}

static inline BlockDriverState * GRAPH_RDLOCK
bdrv_primary_bs(BlockDriverState *bs)
{
    IO_CODE();
    return child_bs(bdrv_primary_child(bs));
}

bool bdrv_bsc_is_data(BlockDriverState *bs, int64_t offset, int64_t *pnum);

void bdrv_bsc_invalidate_range(BlockDriverState *bs,
                               int64_t offset, int64_t bytes);

void bdrv_bsc_fill(BlockDriverState *bs, int64_t offset, int64_t bytes);

#endif /* BLOCK_INT_IO_H */
