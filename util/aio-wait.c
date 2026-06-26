
#include "qemu/osdep.h"
#include "qemu/main-loop.h"
#include "block/aio-wait.h"

AioWait global_aio_wait;

static void dummy_bh_cb(void *opaque)
{
}

void aio_wait_kick(void)
{
    smp_mb();

    if (qatomic_read(&global_aio_wait.num_waiters)) {
        aio_bh_schedule_oneshot(qemu_get_aio_context(), dummy_bh_cb, NULL);
    }
}

typedef struct {
    bool done;
    QEMUBHFunc *cb;
    void *opaque;
} AioWaitBHData;

static void aio_wait_bh(void *opaque)
{
    AioWaitBHData *data = opaque;

    data->cb(data->opaque);

    data->done = true;
    aio_wait_kick();
}

void aio_wait_bh_oneshot(AioContext *ctx, QEMUBHFunc *cb, void *opaque)
{
    AioWaitBHData data = {
        .cb = cb,
        .opaque = opaque,
    };

    assert(qemu_get_current_aio_context() == qemu_get_aio_context());

    aio_bh_schedule_oneshot(ctx, aio_wait_bh, &data);
    AIO_WAIT_WHILE_UNLOCKED(NULL, !data.done);
}
