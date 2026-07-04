
#include "qemu/osdep.h"
#include "qemu/coroutine_int.h"
#include "qemu/processor.h"
#include "qemu/queue.h"
#include "block/aio.h"
#include "trace.h"

void qemu_co_queue_init(CoQueue *queue)
{
    QSIMPLEQ_INIT(&queue->entries);
}

void coroutine_fn qemu_co_queue_wait_impl(CoQueue *queue, QemuLockable *lock,
                                          CoQueueWaitFlags flags)
{
    Coroutine *self = qemu_coroutine_self();
    if (flags & CO_QUEUE_WAIT_FRONT) {
        QSIMPLEQ_INSERT_HEAD(&queue->entries, self, co_queue_next);
    } else {
        QSIMPLEQ_INSERT_TAIL(&queue->entries, self, co_queue_next);
    }

    if (lock) {
        qemu_lockable_unlock(lock);
    }

    qemu_coroutine_yield();
    assert(qemu_in_coroutine());

    if (lock) {
        qemu_lockable_lock(lock);
    }
}

bool qemu_co_enter_next_impl(CoQueue *queue, QemuLockable *lock)
{
    Coroutine *next;

    next = QSIMPLEQ_FIRST(&queue->entries);
    if (!next) {
        return false;
    }

    QSIMPLEQ_REMOVE_HEAD(&queue->entries, co_queue_next);
    if (lock) {
        qemu_lockable_unlock(lock);
    }
    aio_co_wake(next);
    if (lock) {
        qemu_lockable_lock(lock);
    }
    return true;
}

bool coroutine_fn qemu_co_queue_next(CoQueue *queue)
{
    return qemu_co_enter_next_impl(queue, NULL);
}

void qemu_co_enter_all_impl(CoQueue *queue, QemuLockable *lock)
{
    while (qemu_co_enter_next_impl(queue, lock)) {
    }
}

void coroutine_fn qemu_co_queue_restart_all(CoQueue *queue)
{
    qemu_co_enter_all_impl(queue, NULL);
}

bool qemu_co_queue_empty(CoQueue *queue)
{
    return QSIMPLEQ_FIRST(&queue->entries) == NULL;
}

typedef struct CoWaitRecord {
    Coroutine *co;
    QSLIST_ENTRY(CoWaitRecord) next;
} CoWaitRecord;

static void coroutine_fn push_waiter(CoMutex *mutex, CoWaitRecord *w)
{
    w->co = qemu_coroutine_self();
    QSLIST_INSERT_HEAD_ATOMIC(&mutex->from_push, w, next);
}

static void move_waiters(CoMutex *mutex)
{
    QSLIST_HEAD(, CoWaitRecord) reversed;
    QSLIST_MOVE_ATOMIC(&reversed, &mutex->from_push);
    while (!QSLIST_EMPTY(&reversed)) {
        CoWaitRecord *w = QSLIST_FIRST(&reversed);
        QSLIST_REMOVE_HEAD(&reversed, next);
        QSLIST_INSERT_HEAD(&mutex->to_pop, w, next);
    }
}

static CoWaitRecord *pop_waiter(CoMutex *mutex)
{
    CoWaitRecord *w;

    if (QSLIST_EMPTY(&mutex->to_pop)) {
        move_waiters(mutex);
        if (QSLIST_EMPTY(&mutex->to_pop)) {
            return NULL;
        }
    }
    w = QSLIST_FIRST(&mutex->to_pop);
    QSLIST_REMOVE_HEAD(&mutex->to_pop, next);
    return w;
}

static bool has_waiters(CoMutex *mutex)
{
    return QSLIST_EMPTY(&mutex->to_pop) || QSLIST_EMPTY(&mutex->from_push);
}

void qemu_co_mutex_init(CoMutex *mutex)
{
    memset(mutex, 0, sizeof(*mutex));
}

static void coroutine_fn qemu_co_mutex_wake(CoMutex *mutex, Coroutine *co)
{
    smp_read_barrier_depends();
    mutex->ctx = co->ctx;
    aio_co_wake(co);
}

static void coroutine_fn qemu_co_mutex_lock_slowpath(AioContext *ctx,
                                                     CoMutex *mutex)
{
    Coroutine *self = qemu_coroutine_self();
    CoWaitRecord w;
    unsigned old_handoff;

    trace_qemu_co_mutex_lock_entry(mutex, self);
    push_waiter(mutex, &w);

    smp_mb__after_rmw();

    old_handoff = qatomic_read(&mutex->handoff);
    if (old_handoff &&
        has_waiters(mutex) &&
        qatomic_cmpxchg(&mutex->handoff, old_handoff, 0) == old_handoff) {
        CoWaitRecord *to_wake = pop_waiter(mutex);
        Coroutine *co = to_wake->co;
        if (co == self) {
            assert(to_wake == &w);
            mutex->ctx = ctx;
            return;
        }

        qemu_co_mutex_wake(mutex, co);
    }

    qemu_coroutine_yield();
    trace_qemu_co_mutex_lock_return(mutex, self);
}

void coroutine_fn qemu_co_mutex_lock(CoMutex *mutex)
{
    AioContext *ctx = qemu_get_current_aio_context();
    Coroutine *self = qemu_coroutine_self();
    int waiters, i;

    i = 0;
retry_fast_path:
    waiters = qatomic_cmpxchg(&mutex->locked, 0, 1);
    if (waiters != 0) {
        while (waiters == 1 && ++i < 1000) {
            if (qatomic_read(&mutex->ctx) == ctx) {
                break;
            }
            if (qatomic_read(&mutex->locked) == 0) {
                goto retry_fast_path;
            }
            cpu_relax();
        }
        waiters = qatomic_fetch_inc(&mutex->locked);
    }

    if (waiters == 0) {
        trace_qemu_co_mutex_lock_uncontended(mutex, self);
        mutex->ctx = ctx;
    } else {
        qemu_co_mutex_lock_slowpath(ctx, mutex);
    }
    mutex->holder = self;
    self->locks_held++;
}

void coroutine_fn qemu_co_mutex_unlock(CoMutex *mutex)
{
    Coroutine *self = qemu_coroutine_self();

    trace_qemu_co_mutex_unlock_entry(mutex, self);

    assert(mutex->locked);
    assert(mutex->holder == self);
    assert(qemu_in_coroutine());

    mutex->ctx = NULL;
    mutex->holder = NULL;
    self->locks_held--;
    if (qatomic_fetch_dec(&mutex->locked) == 1) {
        return;
    }

    for (;;) {
        CoWaitRecord *to_wake = pop_waiter(mutex);
        unsigned our_handoff;

        if (to_wake) {
            qemu_co_mutex_wake(mutex, to_wake->co);
            break;
        }

        if (++mutex->sequence == 0) {
            mutex->sequence = 1;
        }

        our_handoff = mutex->sequence;
        qatomic_set_mb(&mutex->handoff, our_handoff);
        if (!has_waiters(mutex)) {
            break;
        }

        if (qatomic_cmpxchg(&mutex->handoff, our_handoff, 0) != our_handoff) {
            break;
        }
    }

    trace_qemu_co_mutex_unlock_return(mutex, self);
}

struct CoRwTicket {
    bool read;
    Coroutine *co;
    QSIMPLEQ_ENTRY(CoRwTicket) next;
};

void qemu_co_rwlock_init(CoRwlock *lock)
{
    qemu_co_mutex_init(&lock->mutex);
    lock->owners = 0;
    QSIMPLEQ_INIT(&lock->tickets);
}

static void coroutine_fn qemu_co_rwlock_maybe_wake_one(CoRwlock *lock)
{
    CoRwTicket *tkt = QSIMPLEQ_FIRST(&lock->tickets);
    Coroutine *co = NULL;


    if (tkt) {
        if (tkt->read) {
            if (lock->owners >= 0) {
                lock->owners++;
                co = tkt->co;
            }
        } else {
            if (lock->owners == 0) {
                lock->owners = -1;
                co = tkt->co;
            }
        }
    }

    if (co) {
        QSIMPLEQ_REMOVE_HEAD(&lock->tickets, next);
        qemu_co_mutex_unlock(&lock->mutex);
        aio_co_wake(co);
    } else {
        qemu_co_mutex_unlock(&lock->mutex);
    }
}

void coroutine_fn qemu_co_rwlock_rdlock(CoRwlock *lock)
{
    Coroutine *self = qemu_coroutine_self();

    qemu_co_mutex_lock(&lock->mutex);
    if (lock->owners == 0 || (lock->owners > 0 && QSIMPLEQ_EMPTY(&lock->tickets))) {
        lock->owners++;
        qemu_co_mutex_unlock(&lock->mutex);
    } else {
        CoRwTicket my_ticket = { true, self };

        QSIMPLEQ_INSERT_TAIL(&lock->tickets, &my_ticket, next);
        qemu_co_mutex_unlock(&lock->mutex);
        qemu_coroutine_yield();
        assert(lock->owners >= 1);

        qemu_co_mutex_lock(&lock->mutex);
        qemu_co_rwlock_maybe_wake_one(lock);
    }

    self->locks_held++;
}

void coroutine_fn qemu_co_rwlock_unlock(CoRwlock *lock)
{
    Coroutine *self = qemu_coroutine_self();

    assert(qemu_in_coroutine());
    self->locks_held--;

    qemu_co_mutex_lock(&lock->mutex);
    if (lock->owners > 0) {
        lock->owners--;
    } else {
        assert(lock->owners == -1);
        lock->owners = 0;
    }

    qemu_co_rwlock_maybe_wake_one(lock);
}

void coroutine_fn qemu_co_rwlock_downgrade(CoRwlock *lock)
{
    qemu_co_mutex_lock(&lock->mutex);
    assert(lock->owners == -1);
    lock->owners = 1;

    qemu_co_rwlock_maybe_wake_one(lock);
}

void coroutine_fn qemu_co_rwlock_wrlock(CoRwlock *lock)
{
    Coroutine *self = qemu_coroutine_self();

    qemu_co_mutex_lock(&lock->mutex);
    if (lock->owners == 0) {
        lock->owners = -1;
        qemu_co_mutex_unlock(&lock->mutex);
    } else {
        CoRwTicket my_ticket = { false, qemu_coroutine_self() };

        QSIMPLEQ_INSERT_TAIL(&lock->tickets, &my_ticket, next);
        qemu_co_mutex_unlock(&lock->mutex);
        qemu_coroutine_yield();
        assert(lock->owners == -1);
    }

    self->locks_held++;
}

void coroutine_fn qemu_co_rwlock_upgrade(CoRwlock *lock)
{
    qemu_co_mutex_lock(&lock->mutex);
    assert(lock->owners > 0);
    if (lock->owners == 1 && QSIMPLEQ_EMPTY(&lock->tickets)) {
        lock->owners = -1;
        qemu_co_mutex_unlock(&lock->mutex);
    } else {
        CoRwTicket my_ticket = { false, qemu_coroutine_self() };

        lock->owners--;
        QSIMPLEQ_INSERT_TAIL(&lock->tickets, &my_ticket, next);
        qemu_co_rwlock_maybe_wake_one(lock);
        qemu_coroutine_yield();
        assert(lock->owners == -1);
    }
}
