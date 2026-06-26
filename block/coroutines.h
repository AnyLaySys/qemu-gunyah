
#ifndef BLOCK_COROUTINES_H
#define BLOCK_COROUTINES_H

#include "block/block_int.h"

#include "system/block-backend.h"


int coroutine_fn GRAPH_RDLOCK
bdrv_co_check(BlockDriverState *bs, BdrvCheckResult *res, BdrvCheckMode fix);

int coroutine_fn GRAPH_RDLOCK
bdrv_co_invalidate_cache(BlockDriverState *bs, Error **errp);

int coroutine_fn GRAPH_RDLOCK
bdrv_co_common_block_status_above(BlockDriverState *bs,
                                  BlockDriverState *base,
                                  bool include_base,
                                  bool want_zero,
                                  int64_t offset,
                                  int64_t bytes,
                                  int64_t *pnum,
                                  int64_t *map,
                                  BlockDriverState **file,
                                  int *depth);

int coroutine_fn GRAPH_RDLOCK
bdrv_co_readv_vmstate(BlockDriverState *bs, QEMUIOVector *qiov, int64_t pos);

int coroutine_fn GRAPH_RDLOCK
bdrv_co_writev_vmstate(BlockDriverState *bs, QEMUIOVector *qiov, int64_t pos);


int co_wrapper_mixed_bdrv_rdlock
bdrv_common_block_status_above(BlockDriverState *bs,
                               BlockDriverState *base,
                               bool include_base,
                               bool want_zero,
                               int64_t offset,
                               int64_t bytes,
                               int64_t *pnum,
                               int64_t *map,
                               BlockDriverState **file,
                               int *depth);

#endif /* BLOCK_COROUTINES_H */
