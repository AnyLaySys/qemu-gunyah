
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "block/aio.h"
#include "block/thread-pool.h"
#include "block/graph-lock.h"
#include "qemu/main-loop.h"
#include "qemu/atomic.h"
#include "qemu/lockcnt.h"
#include "qemu/rcu_queue.h"
#include "block/raw-aio.h"
#include "qemu/coroutine_int.h"
#include "qemu/coroutine-tls.h"
#include "system/cpu-timers.h"
#include "trace.h"


enum {
    BH_PENDING   = (1 << 0),

    BH_SCHEDULED = (1 << 1),

    BH_DELETED   = (1 << 2),

    BH_ONESHOT   = (1 << 3),

    BH_IDLE      = (1 << 4),
};

struct QEMUBH {
    AioContext *ctx;
    const char *name;
    QEMUBHFunc *cb;
    void *opaque;
    QSLIST_ENTRY(QEMUBH) next;
    unsigned flags;
    MemReentrancyGuard *reentrancy_guard;
};

static void aio_bh_enqueue(QEMUBH *bh, unsigned new_flags)
{
    AioContext *ctx = bh->ctx;
    unsigned old_flags;

    old_flags = qatomic_fetch_or(&bh->flags, BH_PENDING | new_flags);

    if (!(old_flags & BH_PENDING)) {
        QSLIST_INSERT_HEAD_ATOMIC(&ctx->bh_list, bh, next);
    }

    aio_notify(ctx);
}

static QEMUBH *aio_bh_dequeue(BHList *head, unsigned *flags)
{
    QEMUBH *bh = QSLIST_FIRST_RCU(head);

    if (!bh) {
        return NULL;
    }

    QSLIST_REMOVE_HEAD(head, next);

    *flags = qatomic_fetch_and(&bh->flags,
                              ~(BH_PENDING | BH_SCHEDULED | BH_IDLE));
    return bh;
}

void aio_bh_schedule_oneshot_full(AioContext *ctx, QEMUBHFunc *cb,
                                  void *opaque, const char *name)
{
    QEMUBH *bh;
    bh = g_new(QEMUBH, 1);
    *bh = (QEMUBH){
        .ctx = ctx,
        .cb = cb,
        .opaque = opaque,
        .name = name,
    };
    aio_bh_enqueue(bh, BH_SCHEDULED | BH_ONESHOT);
}

QEMUBH *aio_bh_new_full(AioContext *ctx, QEMUBHFunc *cb, void *opaque,
                        const char *name, MemReentrancyGuard *reentrancy_guard)
{
    QEMUBH *bh;
    bh = g_new(QEMUBH, 1);
    *bh = (QEMUBH){
        .ctx = ctx,
        .cb = cb,
        .opaque = opaque,
        .name = name,
        .reentrancy_guard = reentrancy_guard,
    };
    return bh;
}

void aio_bh_call(QEMUBH *bh)
{
    bool last_engaged_in_io = false;

    MemReentrancyGuard *reentrancy_guard = bh->reentrancy_guard;
    if (reentrancy_guard) {
        last_engaged_in_io = reentrancy_guard->engaged_in_io;
        if (reentrancy_guard->engaged_in_io) {
            trace_reentrant_aio(bh->ctx, bh->name);
        }
        reentrancy_guard->engaged_in_io = true;
    }

    bh->cb(bh->opaque);

    if (reentrancy_guard) {
        reentrancy_guard->engaged_in_io = last_engaged_in_io;
    }
}

int aio_bh_poll(AioContext *ctx)
{
    BHListSlice slice;
    BHListSlice *s;
    int ret = 0;

    QSLIST_MOVE_ATOMIC(&slice.bh_list, &ctx->bh_list);

#if !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wdangling-pointer="
#endif
    QSIMPLEQ_INSERT_TAIL(&ctx->bh_slice_list, &slice, next);
#if !defined(__clang__)
#pragma GCC diagnostic pop
#endif

    while ((s = QSIMPLEQ_FIRST(&ctx->bh_slice_list))) {
        QEMUBH *bh;
        unsigned flags;

        bh = aio_bh_dequeue(&s->bh_list, &flags);
        if (!bh) {
            QSIMPLEQ_REMOVE_HEAD(&ctx->bh_slice_list, next);
            continue;
        }

        if ((flags & (BH_SCHEDULED | BH_DELETED)) == BH_SCHEDULED) {
            if (!(flags & BH_IDLE)) {
                ret = 1;
            }
            aio_bh_call(bh);
        }
        if (flags & (BH_DELETED | BH_ONESHOT)) {
            g_free(bh);
        }
    }

    return ret;
}

void qemu_bh_schedule_idle(QEMUBH *bh)
{
    aio_bh_enqueue(bh, BH_SCHEDULED | BH_IDLE);
}

void qemu_bh_schedule(QEMUBH *bh)
{
    aio_bh_enqueue(bh, BH_SCHEDULED);
}

void qemu_bh_cancel(QEMUBH *bh)
{
    qatomic_and(&bh->flags, ~BH_SCHEDULED);
}

void qemu_bh_delete(QEMUBH *bh)
{
    aio_bh_enqueue(bh, BH_DELETED);
}

static int64_t aio_compute_bh_timeout(BHList *head, int timeout)
{
    QEMUBH *bh;

    QSLIST_FOREACH_RCU(bh, head, next) {
        if ((bh->flags & (BH_SCHEDULED | BH_DELETED)) == BH_SCHEDULED) {
            if (bh->flags & BH_IDLE) {
                timeout = 10000000;
            } else {
                return 0;
            }
        }
    }

    return timeout;
}

int64_t
aio_compute_timeout(AioContext *ctx)
{
    BHListSlice *s;
    int64_t deadline;
    int timeout = -1;

    timeout = aio_compute_bh_timeout(&ctx->bh_list, timeout);
    if (timeout == 0) {
        return 0;
    }

    QSIMPLEQ_FOREACH(s, &ctx->bh_slice_list, next) {
        timeout = aio_compute_bh_timeout(&s->bh_list, timeout);
        if (timeout == 0) {
            return 0;
        }
    }

    deadline = timerlistgroup_deadline_ns(&ctx->tlg);
    if (deadline == 0) {
        return 0;
    } else {
        return qemu_soonest_timeout(timeout, deadline);
    }
}

static gboolean
aio_ctx_prepare(GSource *source, gint    *timeout)
{
    AioContext *ctx = (AioContext *) source;

    qatomic_set(&ctx->notify_me, qatomic_read(&ctx->notify_me) | 1);

    smp_mb();

    *timeout = qemu_timeout_ns_to_ms(aio_compute_timeout(ctx));

    if (aio_prepare(ctx)) {
        *timeout = 0;
    }

    return *timeout == 0;
}

static gboolean
aio_ctx_check(GSource *source)
{
    AioContext *ctx = (AioContext *) source;
    QEMUBH *bh;
    BHListSlice *s;

    qatomic_store_release(&ctx->notify_me, qatomic_read(&ctx->notify_me) & ~1);
    aio_notify_accept(ctx);

    QSLIST_FOREACH_RCU(bh, &ctx->bh_list, next) {
        if ((bh->flags & (BH_SCHEDULED | BH_DELETED)) == BH_SCHEDULED) {
            return true;
        }
    }

    QSIMPLEQ_FOREACH(s, &ctx->bh_slice_list, next) {
        QSLIST_FOREACH_RCU(bh, &s->bh_list, next) {
            if ((bh->flags & (BH_SCHEDULED | BH_DELETED)) == BH_SCHEDULED) {
                return true;
            }
        }
    }
    return aio_pending(ctx) || (timerlistgroup_deadline_ns(&ctx->tlg) == 0);
}

static gboolean
aio_ctx_dispatch(GSource     *source,
                 GSourceFunc  callback,
                 gpointer     user_data)
{
    AioContext *ctx = (AioContext *) source;

    assert(callback == NULL);
    aio_dispatch(ctx);
    return true;
}

static void
aio_ctx_finalize(GSource     *source)
{
    AioContext *ctx = (AioContext *) source;
    QEMUBH *bh;
    unsigned flags;

    thread_pool_free_aio(ctx->thread_pool);

#ifdef CONFIG_LINUX_IO_URING
    if (ctx->linux_io_uring) {
        luring_detach_aio_context(ctx->linux_io_uring, ctx);
        luring_cleanup(ctx->linux_io_uring);
        ctx->linux_io_uring = NULL;
    }
#endif

    assert(QSLIST_EMPTY(&ctx->scheduled_coroutines));
    qemu_bh_delete(ctx->co_schedule_bh);

    assert(QSIMPLEQ_EMPTY(&ctx->bh_slice_list));

    while ((bh = aio_bh_dequeue(&ctx->bh_list, &flags))) {
        if (unlikely(!(flags & BH_DELETED))) {
            fprintf(stderr, "%s: BH '%s' leaked, aborting...\n",
                    __func__, bh->name);
            abort();
        }

        g_free(bh);
    }

    aio_set_event_notifier(ctx, &ctx->notifier, NULL, NULL, NULL);
    event_notifier_cleanup(&ctx->notifier);
    qemu_rec_mutex_destroy(&ctx->lock);
    qemu_lockcnt_destroy(&ctx->list_lock);
    timerlistgroup_deinit(&ctx->tlg);
    unregister_aiocontext(ctx);
    aio_context_destroy(ctx);
}

static GSourceFuncs aio_source_funcs = {
    aio_ctx_prepare,
    aio_ctx_check,
    aio_ctx_dispatch,
    aio_ctx_finalize
};

GSource *aio_get_g_source(AioContext *ctx)
{
    aio_context_use_g_source(ctx);
    g_source_ref(&ctx->source);
    return &ctx->source;
}

ThreadPoolAio *aio_get_thread_pool(AioContext *ctx)
{
    if (!ctx->thread_pool) {
        ctx->thread_pool = thread_pool_new_aio(ctx);
    }
    return ctx->thread_pool;
}

#ifdef CONFIG_LINUX_IO_URING
LuringState *aio_setup_linux_io_uring(AioContext *ctx, Error **errp)
{
    if (ctx->linux_io_uring) {
        return ctx->linux_io_uring;
    }

    ctx->linux_io_uring = luring_init(errp);
    if (!ctx->linux_io_uring) {
        return NULL;
    }

    luring_attach_aio_context(ctx->linux_io_uring, ctx);
    return ctx->linux_io_uring;
}

LuringState *aio_get_linux_io_uring(AioContext *ctx)
{
    assert(ctx->linux_io_uring);
    return ctx->linux_io_uring;
}
#endif

void aio_notify(AioContext *ctx)
{
    smp_wmb();
    qatomic_set(&ctx->notified, true);

    smp_mb();
    if (qatomic_read(&ctx->notify_me)) {
        event_notifier_set(&ctx->notifier);
    }
}

void aio_notify_accept(AioContext *ctx)
{
    qatomic_set(&ctx->notified, false);

    smp_mb();
}

static void aio_timerlist_notify(void *opaque, QEMUClockType type)
{
    aio_notify(opaque);
}

static void aio_context_notifier_cb(EventNotifier *e)
{
    AioContext *ctx = container_of(e, AioContext, notifier);

    event_notifier_test_and_clear(&ctx->notifier);
}

static bool aio_context_notifier_poll(void *opaque)
{
    EventNotifier *e = opaque;
    AioContext *ctx = container_of(e, AioContext, notifier);

    return qatomic_read(&ctx->notified);
}

static void aio_context_notifier_poll_ready(EventNotifier *e)
{
}

static void co_schedule_bh_cb(void *opaque)
{
    AioContext *ctx = opaque;
    QSLIST_HEAD(, Coroutine) straight, reversed;

    QSLIST_MOVE_ATOMIC(&reversed, &ctx->scheduled_coroutines);
    QSLIST_INIT(&straight);

    while (!QSLIST_EMPTY(&reversed)) {
        Coroutine *co = QSLIST_FIRST(&reversed);
        QSLIST_REMOVE_HEAD(&reversed, co_scheduled_next);
        QSLIST_INSERT_HEAD(&straight, co, co_scheduled_next);
    }

    while (!QSLIST_EMPTY(&straight)) {
        Coroutine *co = QSLIST_FIRST(&straight);
        QSLIST_REMOVE_HEAD(&straight, co_scheduled_next);
        trace_aio_co_schedule_bh_cb(ctx, co);

        qatomic_set(&co->scheduled, NULL);
        qemu_aio_coroutine_enter(ctx, co);
    }
}

AioContext *aio_context_new(Error **errp)
{
    int ret;
    AioContext *ctx;

    ctx = (AioContext *) g_source_new(&aio_source_funcs, sizeof(AioContext));
    QSLIST_INIT(&ctx->bh_list);
    QSIMPLEQ_INIT(&ctx->bh_slice_list);
    aio_context_setup(ctx);

    ret = event_notifier_init(&ctx->notifier, false);
    if (ret < 0) {
        error_setg_errno(errp, -ret, "Failed to initialize event notifier");
        goto fail;
    }
    g_source_set_can_recurse(&ctx->source, true);
    qemu_lockcnt_init(&ctx->list_lock);

    ctx->co_schedule_bh = aio_bh_new(ctx, co_schedule_bh_cb, ctx);
    QSLIST_INIT(&ctx->scheduled_coroutines);

    aio_set_event_notifier(ctx, &ctx->notifier,
                           aio_context_notifier_cb,
                           aio_context_notifier_poll,
                           aio_context_notifier_poll_ready);
#ifdef CONFIG_LINUX_IO_URING
    ctx->linux_io_uring = NULL;
#endif

    ctx->thread_pool = NULL;
    qemu_rec_mutex_init(&ctx->lock);
    timerlistgroup_init(&ctx->tlg, aio_timerlist_notify, ctx);

    ctx->poll_max_ns = 0;
    ctx->poll_grow = 0;
    ctx->poll_shrink = 0;

    ctx->aio_max_batch = 0;

    ctx->thread_pool_min = 0;
    ctx->thread_pool_max = THREAD_POOL_MAX_THREADS_DEFAULT;

    register_aiocontext(ctx);

    return ctx;
fail:
    g_source_destroy(&ctx->source);
    return NULL;
}

void aio_co_schedule(AioContext *ctx, Coroutine *co)
{
    trace_aio_co_schedule(ctx, co);
    const char *scheduled = qatomic_cmpxchg(&co->scheduled, NULL,
                                           __func__);

    if (scheduled) {
        fprintf(stderr,
                "%s: Co-routine was already scheduled in '%s'\n",
                __func__, scheduled);
        abort();
    }

    aio_context_ref(ctx);

    QSLIST_INSERT_HEAD_ATOMIC(&ctx->scheduled_coroutines,
                              co, co_scheduled_next);
    qemu_bh_schedule(ctx->co_schedule_bh);

    aio_context_unref(ctx);
}

typedef struct AioCoRescheduleSelf {
    Coroutine *co;
    AioContext *new_ctx;
} AioCoRescheduleSelf;

static void aio_co_reschedule_self_bh(void *opaque)
{
    AioCoRescheduleSelf *data = opaque;
    aio_co_schedule(data->new_ctx, data->co);
}

void coroutine_fn aio_co_reschedule_self(AioContext *new_ctx)
{
    AioContext *old_ctx = qemu_get_current_aio_context();

    if (old_ctx != new_ctx) {
        AioCoRescheduleSelf data = {
            .co = qemu_coroutine_self(),
            .new_ctx = new_ctx,
        };
        aio_bh_schedule_oneshot(old_ctx, aio_co_reschedule_self_bh, &data);
        qemu_coroutine_yield();
    }
}

void aio_co_wake(Coroutine *co)
{
    AioContext *ctx;

    smp_read_barrier_depends();
    ctx = qatomic_read(&co->ctx);

    aio_co_enter(ctx, co);
}

void aio_co_enter(AioContext *ctx, Coroutine *co)
{
    if (ctx != qemu_get_current_aio_context()) {
        aio_co_schedule(ctx, co);
        return;
    }

    if (qemu_in_coroutine()) {
        Coroutine *self = qemu_coroutine_self();
        assert(self != co);
        QSIMPLEQ_INSERT_TAIL(&self->co_queue_wakeup, co, co_queue_next);
    } else {
        qemu_aio_coroutine_enter(ctx, co);
    }
}

void aio_context_ref(AioContext *ctx)
{
    g_source_ref(&ctx->source);
}

void aio_context_unref(AioContext *ctx)
{
    g_source_unref(&ctx->source);
}

QEMU_DEFINE_STATIC_CO_TLS(AioContext *, my_aiocontext)

AioContext *qemu_get_current_aio_context(void)
{
    AioContext *ctx = get_my_aiocontext();
    if (ctx) {
        return ctx;
    }
    if (bql_locked()) {
        return qemu_get_aio_context();
    }
    return NULL;
}

void qemu_set_current_aio_context(AioContext *ctx)
{
    set_my_aiocontext(ctx);
}

void aio_context_set_thread_pool_params(AioContext *ctx, int64_t min,
                                        int64_t max, Error **errp)
{

    if (min > max || max <= 0 || min < 0 || min > INT_MAX || max > INT_MAX) {
        error_setg(errp, "bad thread-pool-min/thread-pool-max values");
        return;
    }

    ctx->thread_pool_min = min;
    ctx->thread_pool_max = max;

    if (ctx->thread_pool) {
        thread_pool_update_params(ctx->thread_pool, ctx);
    }
}
