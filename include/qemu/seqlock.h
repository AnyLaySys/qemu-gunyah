
#ifndef QEMU_SEQLOCK_H
#define QEMU_SEQLOCK_H

#include "qemu/atomic.h"
#include "qemu/thread.h"
#include "qemu/lockable.h"

typedef struct QemuSeqLock QemuSeqLock;

struct QemuSeqLock {
    unsigned sequence;
};

static inline void seqlock_init(QemuSeqLock *sl)
{
    sl->sequence = 0;
}

static inline void seqlock_write_begin(QemuSeqLock *sl)
{
    qatomic_set(&sl->sequence, sl->sequence + 1);

    smp_wmb();
}

static inline void seqlock_write_end(QemuSeqLock *sl)
{
    smp_wmb();

    qatomic_set(&sl->sequence, sl->sequence + 1);
}

static inline void seqlock_write_lock_impl(QemuSeqLock *sl, QemuLockable *lock)
{
    qemu_lockable_lock(lock);
    seqlock_write_begin(sl);
}
#define seqlock_write_lock(sl, lock) \
    seqlock_write_lock_impl(sl, QEMU_MAKE_LOCKABLE(lock))

static inline void seqlock_write_unlock_impl(QemuSeqLock *sl, QemuLockable *lock)
{
    seqlock_write_end(sl);
    qemu_lockable_unlock(lock);
}
#define seqlock_write_unlock(sl, lock) \
    seqlock_write_unlock_impl(sl, QEMU_MAKE_LOCKABLE(lock))


static inline unsigned seqlock_read_begin(const QemuSeqLock *sl)
{
    unsigned ret = qatomic_read(&sl->sequence);

    smp_rmb();
    return ret & ~1;
}

static inline int seqlock_read_retry(const QemuSeqLock *sl, unsigned start)
{
    smp_rmb();
    return unlikely(qatomic_read(&sl->sequence) != start);
}

#endif
