
#include "qemu/osdep.h"
#include "qemu/coroutine.h"
#include "block/aio.h"

typedef struct QemuCoTimeoutState {
    CoroutineEntry *entry;
    void *opaque;
    QemuCoSleep sleep_state;
    bool marker;
    CleanupFunc *clean;
} QemuCoTimeoutState;

static void coroutine_fn qemu_co_timeout_entry(void *opaque)
{
    QemuCoTimeoutState *s = opaque;

    s->entry(s->opaque);

    if (s->marker) {
        assert(!s->sleep_state.to_wake);
        if (s->clean) {
            s->clean(s->opaque);
        }
        g_free(s);
    } else {
        s->marker = true;
        qemu_co_sleep_wake(&s->sleep_state);
    }
}

int coroutine_fn qemu_co_timeout(CoroutineEntry *entry, void *opaque,
                                 uint64_t timeout_ns, CleanupFunc clean)
{
    QemuCoTimeoutState *s;
    Coroutine *co;

    if (timeout_ns == 0) {
        entry(opaque);
        return 0;
    }

    s = g_new(QemuCoTimeoutState, 1);
    *s = (QemuCoTimeoutState) {
        .entry = entry,
        .opaque = opaque,
        .clean = clean
    };

    co = qemu_coroutine_create(qemu_co_timeout_entry, s);

    aio_co_enter(qemu_get_current_aio_context(), co);
    qemu_co_sleep_ns_wakeable(&s->sleep_state, QEMU_CLOCK_REALTIME, timeout_ns);

    if (s->marker) {
        g_free(s);
        return 0;
    }

    s->marker = true;
    return -ETIMEDOUT;
}
