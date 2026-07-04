#include "qemu/osdep.h"
#include "qemu/lockcnt.h"
#include "qemu/thread.h"
#include "qemu/atomic.h"
#include "trace.h"

#ifdef CONFIG_LINUX
#include "qemu/futex.h"


#define QEMU_LOCKCNT_STATE_MASK    3
#define QEMU_LOCKCNT_STATE_FREE    0   /* free, uncontended */
#define QEMU_LOCKCNT_STATE_LOCKED  1   /* locked, uncontended */
#define QEMU_LOCKCNT_STATE_WAITING 2   /* locked, contended */

#define QEMU_LOCKCNT_COUNT_STEP    4
#define QEMU_LOCKCNT_COUNT_SHIFT   2

void qemu_lockcnt_init(QemuLockCnt *lockcnt)
{
    lockcnt->count = 0;
}

void qemu_lockcnt_destroy(QemuLockCnt *lockcnt)
{
}

static bool qemu_lockcnt_cmpxchg_or_wait(QemuLockCnt *lockcnt, int *val,
                                         int new_if_free, bool *waited)
{
    if ((*val & QEMU_LOCKCNT_STATE_MASK) == QEMU_LOCKCNT_STATE_FREE) {
        int expected = *val;

        trace_lockcnt_fast_path_attempt(lockcnt, expected, new_if_free);
        *val = qatomic_cmpxchg(&lockcnt->count, expected, new_if_free);
        if (*val == expected) {
            trace_lockcnt_fast_path_success(lockcnt, expected, new_if_free);
            *val = new_if_free;
            return true;
        }
    }

    while ((*val & QEMU_LOCKCNT_STATE_MASK) != QEMU_LOCKCNT_STATE_FREE) {
        if ((*val & QEMU_LOCKCNT_STATE_MASK) == QEMU_LOCKCNT_STATE_LOCKED) {
            int expected = *val;
            int new = expected - QEMU_LOCKCNT_STATE_LOCKED + QEMU_LOCKCNT_STATE_WAITING;

            trace_lockcnt_futex_wait_prepare(lockcnt, expected, new);
            *val = qatomic_cmpxchg(&lockcnt->count, expected, new);
            if (*val == expected) {
                *val = new;
            }
            continue;
        }

        if ((*val & QEMU_LOCKCNT_STATE_MASK) == QEMU_LOCKCNT_STATE_WAITING) {
            *waited = true;
            trace_lockcnt_futex_wait(lockcnt, *val);
            qemu_futex_wait(&lockcnt->count, *val);
            *val = qatomic_read(&lockcnt->count);
            trace_lockcnt_futex_wait_resume(lockcnt, *val);
            continue;
        }

        abort();
    }
    return false;
}

static void lockcnt_wake(QemuLockCnt *lockcnt)
{
    trace_lockcnt_futex_wake(lockcnt);
    qemu_futex_wake(&lockcnt->count, 1);
}

void qemu_lockcnt_inc(QemuLockCnt *lockcnt)
{
    int val = qatomic_read(&lockcnt->count);
    bool waited = false;

    for (;;) {
        if (val >= QEMU_LOCKCNT_COUNT_STEP) {
            int expected = val;
            val = qatomic_cmpxchg(&lockcnt->count, val,
                                  val + QEMU_LOCKCNT_COUNT_STEP);
            if (val == expected) {
                break;
            }
        } else {
            if (qemu_lockcnt_cmpxchg_or_wait(lockcnt, &val, QEMU_LOCKCNT_COUNT_STEP,
                                             &waited)) {
                break;
            }
        }
    }

    if (waited) {
        lockcnt_wake(lockcnt);
    }
}

void qemu_lockcnt_dec(QemuLockCnt *lockcnt)
{
    qatomic_sub(&lockcnt->count, QEMU_LOCKCNT_COUNT_STEP);
}

bool qemu_lockcnt_dec_and_lock(QemuLockCnt *lockcnt)
{
    int val = qatomic_read(&lockcnt->count);
    int locked_state = QEMU_LOCKCNT_STATE_LOCKED;
    bool waited = false;

    for (;;) {
        if (val >= 2 * QEMU_LOCKCNT_COUNT_STEP) {
            int expected = val;
            val = qatomic_cmpxchg(&lockcnt->count, val,
                                  val - QEMU_LOCKCNT_COUNT_STEP);
            if (val == expected) {
                break;
            }
        } else {
            if (qemu_lockcnt_cmpxchg_or_wait(lockcnt, &val, locked_state, &waited)) {
                return true;
            }

            if (waited) {
                locked_state = QEMU_LOCKCNT_STATE_WAITING;
            }
        }
    }

    if (waited) {
        lockcnt_wake(lockcnt);
    }
    return false;
}

bool qemu_lockcnt_dec_if_lock(QemuLockCnt *lockcnt)
{
    int val = qatomic_read(&lockcnt->count);
    int locked_state = QEMU_LOCKCNT_STATE_LOCKED;
    bool waited = false;

    while (val < 2 * QEMU_LOCKCNT_COUNT_STEP) {
        if (qemu_lockcnt_cmpxchg_or_wait(lockcnt, &val, locked_state, &waited)) {
            return true;
        }

        if (waited) {
            locked_state = QEMU_LOCKCNT_STATE_WAITING;
        }
    }

    if (waited) {
        lockcnt_wake(lockcnt);
    }
    return false;
}

void qemu_lockcnt_lock(QemuLockCnt *lockcnt)
{
    int val = qatomic_read(&lockcnt->count);
    int step = QEMU_LOCKCNT_STATE_LOCKED;
    bool waited = false;

    while (!qemu_lockcnt_cmpxchg_or_wait(lockcnt, &val, val + step, &waited)) {
        if (waited) {
            step = QEMU_LOCKCNT_STATE_WAITING;
        }
    }
}

void qemu_lockcnt_inc_and_unlock(QemuLockCnt *lockcnt)
{
    int expected, new, val;

    val = qatomic_read(&lockcnt->count);
    do {
        expected = val;
        new = (val + QEMU_LOCKCNT_COUNT_STEP) & ~QEMU_LOCKCNT_STATE_MASK;
        trace_lockcnt_unlock_attempt(lockcnt, val, new);
        val = qatomic_cmpxchg(&lockcnt->count, val, new);
    } while (val != expected);

    trace_lockcnt_unlock_success(lockcnt, val, new);
    if (val & QEMU_LOCKCNT_STATE_WAITING) {
        lockcnt_wake(lockcnt);
    }
}

void qemu_lockcnt_unlock(QemuLockCnt *lockcnt)
{
    int expected, new, val;

    val = qatomic_read(&lockcnt->count);
    do {
        expected = val;
        new = val & ~QEMU_LOCKCNT_STATE_MASK;
        trace_lockcnt_unlock_attempt(lockcnt, val, new);
        val = qatomic_cmpxchg(&lockcnt->count, val, new);
    } while (val != expected);

    trace_lockcnt_unlock_success(lockcnt, val, new);
    if (val & QEMU_LOCKCNT_STATE_WAITING) {
        lockcnt_wake(lockcnt);
    }
}

unsigned qemu_lockcnt_count(QemuLockCnt *lockcnt)
{
    return qatomic_read(&lockcnt->count) >> QEMU_LOCKCNT_COUNT_SHIFT;
}
#else
void qemu_lockcnt_init(QemuLockCnt *lockcnt)
{
    qemu_mutex_init(&lockcnt->mutex);
    lockcnt->count = 0;
}

void qemu_lockcnt_destroy(QemuLockCnt *lockcnt)
{
    qemu_mutex_destroy(&lockcnt->mutex);
}

void qemu_lockcnt_inc(QemuLockCnt *lockcnt)
{
    int old;
    for (;;) {
        old = qatomic_read(&lockcnt->count);
        if (old == 0) {
            qemu_lockcnt_lock(lockcnt);
            qemu_lockcnt_inc_and_unlock(lockcnt);
            return;
        } else {
            if (qatomic_cmpxchg(&lockcnt->count, old, old + 1) == old) {
                return;
            }
        }
    }
}

void qemu_lockcnt_dec(QemuLockCnt *lockcnt)
{
    qatomic_dec(&lockcnt->count);
}

bool qemu_lockcnt_dec_and_lock(QemuLockCnt *lockcnt)
{
    int val = qatomic_read(&lockcnt->count);
    while (val > 1) {
        int old = qatomic_cmpxchg(&lockcnt->count, val, val - 1);
        if (old != val) {
            val = old;
            continue;
        }

        return false;
    }

    qemu_lockcnt_lock(lockcnt);
    if (qatomic_fetch_dec(&lockcnt->count) == 1) {
        return true;
    }

    qemu_lockcnt_unlock(lockcnt);
    return false;
}

bool qemu_lockcnt_dec_if_lock(QemuLockCnt *lockcnt)
{
    int val = qatomic_read(&lockcnt->count);
    if (val > 1) {
        return false;
    }

    qemu_lockcnt_lock(lockcnt);
    if (qatomic_fetch_dec(&lockcnt->count) == 1) {
        return true;
    }

    qemu_lockcnt_inc_and_unlock(lockcnt);
    return false;
}

void qemu_lockcnt_lock(QemuLockCnt *lockcnt)
{
    qemu_mutex_lock(&lockcnt->mutex);
}

void qemu_lockcnt_inc_and_unlock(QemuLockCnt *lockcnt)
{
    qatomic_inc(&lockcnt->count);
    qemu_mutex_unlock(&lockcnt->mutex);
}

void qemu_lockcnt_unlock(QemuLockCnt *lockcnt)
{
    qemu_mutex_unlock(&lockcnt->mutex);
}

unsigned qemu_lockcnt_count(QemuLockCnt *lockcnt)
{
    return qatomic_read(&lockcnt->count);
}
#endif
