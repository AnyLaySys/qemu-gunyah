
#ifndef QEMU_COROUTINE_H
#define QEMU_COROUTINE_H

#include "qemu/coroutine-core.h"
#include "qemu/atomic.h"
#include "qemu/queue.h"
#include "qemu/timer.h"


struct CoWaitRecord;
struct CoMutex {
    unsigned locked;

    AioContext *ctx;

    QSLIST_HEAD(, CoWaitRecord) from_push, to_pop;

    unsigned handoff, sequence;

    Coroutine *holder;
};

static inline coroutine_fn void qemu_co_mutex_assert_locked(CoMutex *mutex)
{
    assert(qatomic_read(&mutex->locked) &&
           mutex->holder == qemu_coroutine_self());
}

#include "qemu/lockable.h"

typedef struct CoQueue {
    QSIMPLEQ_HEAD(, Coroutine) entries;
} CoQueue;

void qemu_co_queue_init(CoQueue *queue);

typedef enum {
    CO_QUEUE_WAIT_FRONT = 0x1,
} CoQueueWaitFlags;

#define qemu_co_queue_wait(queue, lock) \
    qemu_co_queue_wait_impl(queue, QEMU_MAKE_LOCKABLE(lock), 0)
#define qemu_co_queue_wait_flags(queue, lock, flags) \
    qemu_co_queue_wait_impl(queue, QEMU_MAKE_LOCKABLE(lock), (flags))
void coroutine_fn qemu_co_queue_wait_impl(CoQueue *queue, QemuLockable *lock,
                                          CoQueueWaitFlags flags);

bool coroutine_fn qemu_co_queue_next(CoQueue *queue);

void coroutine_fn qemu_co_queue_restart_all(CoQueue *queue);

#define qemu_co_enter_next(queue, lock) \
    qemu_co_enter_next_impl(queue, QEMU_MAKE_LOCKABLE(lock))
bool qemu_co_enter_next_impl(CoQueue *queue, QemuLockable *lock);

#define qemu_co_enter_all(queue, lock) \
    qemu_co_enter_all_impl(queue, QEMU_MAKE_LOCKABLE(lock))
void qemu_co_enter_all_impl(CoQueue *queue, QemuLockable *lock);

bool qemu_co_queue_empty(CoQueue *queue);


typedef struct CoRwTicket CoRwTicket;
typedef struct CoRwlock {
    CoMutex mutex;

    int owners;

    QSIMPLEQ_HEAD(, CoRwTicket) tickets;
} CoRwlock;

void qemu_co_rwlock_init(CoRwlock *lock);

void coroutine_fn qemu_co_rwlock_rdlock(CoRwlock *lock);

void coroutine_fn qemu_co_rwlock_upgrade(CoRwlock *lock);

void coroutine_fn qemu_co_rwlock_downgrade(CoRwlock *lock);

void coroutine_fn qemu_co_rwlock_wrlock(CoRwlock *lock);

void coroutine_fn qemu_co_rwlock_unlock(CoRwlock *lock);

typedef struct QemuCoSleep {
    Coroutine *to_wake;
} QemuCoSleep;

void coroutine_fn qemu_co_sleep_ns_wakeable(QemuCoSleep *w,
                                            QEMUClockType type, int64_t ns);

void coroutine_fn qemu_co_sleep(QemuCoSleep *w);

static inline void coroutine_fn qemu_co_sleep_ns(QEMUClockType type, int64_t ns)
{
    QemuCoSleep w = { 0 };
    qemu_co_sleep_ns_wakeable(&w, type, ns);
}

typedef void CleanupFunc(void *opaque);
int coroutine_fn qemu_co_timeout(CoroutineEntry *entry, void *opaque,
                                 uint64_t timeout_ns, CleanupFunc clean);

void qemu_co_sleep_wake(QemuCoSleep *w);

void coroutine_fn yield_until_fd_readable(int fd);

void qemu_coroutine_inc_pool_size(unsigned int additional_pool_size);

void qemu_coroutine_dec_pool_size(unsigned int additional_pool_size);

ssize_t coroutine_fn qemu_co_sendv_recvv(int sockfd, struct iovec *iov,
                                         unsigned iov_cnt, size_t offset,
                                         size_t bytes, bool do_send);
#define qemu_co_recvv(sockfd, iov, iov_cnt, offset, bytes) \
  qemu_co_sendv_recvv(sockfd, iov, iov_cnt, offset, bytes, false)
#define qemu_co_sendv(sockfd, iov, iov_cnt, offset, bytes) \
  qemu_co_sendv_recvv(sockfd, iov, iov_cnt, offset, bytes, true)

ssize_t coroutine_fn qemu_co_send_recv(int sockfd, void *buf, size_t bytes,
                                       bool do_send);
#define qemu_co_recv(sockfd, buf, bytes) \
  qemu_co_send_recv(sockfd, buf, bytes, false)
#define qemu_co_send(sockfd, buf, bytes) \
  qemu_co_send_recv(sockfd, buf, bytes, true)

#endif /* QEMU_COROUTINE_H */
