
#ifndef QEMU_AIO_WAIT_H
#define QEMU_AIO_WAIT_H

#include "block/aio.h"
#include "qemu/main-loop.h"

typedef struct {
    unsigned num_waiters;
} AioWait;

extern AioWait global_aio_wait;

#define AIO_WAIT_WHILE_INTERNAL(ctx, cond) ({                      \
    bool waited_ = false;                                          \
    AioWait *wait_ = &global_aio_wait;                             \
    AioContext *ctx_ = (ctx);                                      \
    /* Increment wait_->num_waiters before evaluating cond. */     \
    qatomic_inc(&wait_->num_waiters);                              \
    /* Paired with smp_mb in aio_wait_kick(). */                   \
    smp_mb__after_rmw();                                           \
    if (ctx_ && in_aio_context_home_thread(ctx_)) {                \
        while ((cond)) {                                           \
            aio_poll(ctx_, true);                                  \
            waited_ = true;                                        \
        }                                                          \
    } else {                                                       \
        assert(qemu_get_current_aio_context() ==                   \
               qemu_get_aio_context());                            \
        while ((cond)) {                                           \
            aio_poll(qemu_get_aio_context(), true);                \
            waited_ = true;                                        \
        }                                                          \
    }                                                              \
    qatomic_dec(&wait_->num_waiters);                              \
    waited_; })

#define AIO_WAIT_WHILE(ctx, cond)                                  \
    AIO_WAIT_WHILE_INTERNAL(ctx, cond)

#define AIO_WAIT_WHILE_UNLOCKED(ctx, cond)                         \
    AIO_WAIT_WHILE_INTERNAL(ctx, cond)

void aio_wait_kick(void);

void aio_wait_bh_oneshot(AioContext *ctx, QEMUBHFunc *cb, void *opaque);

static inline bool in_aio_context_home_thread(AioContext *ctx)
{
    if (ctx == qemu_get_current_aio_context()) {
        return true;
    }

    if (ctx == qemu_get_aio_context()) {
        return bql_locked();
    } else {
        return false;
    }
}

#endif /* QEMU_AIO_WAIT_H */
