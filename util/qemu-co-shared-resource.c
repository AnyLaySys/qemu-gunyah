
#include "qemu/osdep.h"
#include "qemu/coroutine.h"
#include "qemu/co-shared-resource.h"

struct SharedResource {
    uint64_t total; /* Set in shres_create() and not changed anymore */

    uint64_t available;
    CoQueue queue;

    QemuMutex lock;
};

SharedResource *shres_create(uint64_t total)
{
    SharedResource *s = g_new0(SharedResource, 1);

    s->total = s->available = total;
    qemu_co_queue_init(&s->queue);
    qemu_mutex_init(&s->lock);

    return s;
}

void shres_destroy(SharedResource *s)
{
    assert(s->available == s->total);
    qemu_mutex_destroy(&s->lock);
    g_free(s);
}

static bool co_try_get_from_shres_locked(SharedResource *s, uint64_t n)
{
    if (s->available >= n) {
        s->available -= n;
        return true;
    }

    return false;
}

void coroutine_fn co_get_from_shres(SharedResource *s, uint64_t n)
{
    assert(n <= s->total);
    QEMU_LOCK_GUARD(&s->lock);
    while (!co_try_get_from_shres_locked(s, n)) {
        qemu_co_queue_wait(&s->queue, &s->lock);
    }
}

void coroutine_fn co_put_to_shres(SharedResource *s, uint64_t n)
{
    QEMU_LOCK_GUARD(&s->lock);
    assert(s->total - s->available >= n);
    s->available += n;
    qemu_co_queue_restart_all(&s->queue);
}
