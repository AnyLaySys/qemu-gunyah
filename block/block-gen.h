
#ifndef BLOCK_BLOCK_GEN_H
#define BLOCK_BLOCK_GEN_H

#include "block/block_int.h"

typedef struct BdrvPollCo {
    AioContext *ctx;
    bool in_progress;
    Coroutine *co; /* Keep pointer here for debugging */
} BdrvPollCo;

static inline void bdrv_poll_co(BdrvPollCo *s)
{
    assert(!qemu_in_coroutine());

    aio_co_enter(s->ctx, s->co);
    AIO_WAIT_WHILE(s->ctx, s->in_progress);
}

#endif /* BLOCK_BLOCK_GEN_H */
