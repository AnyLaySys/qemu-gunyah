
#ifndef QEMU_RATELIMIT_H
#define QEMU_RATELIMIT_H

#include "qemu/lockable.h"
#include "qemu/timer.h"

typedef struct {
    QemuMutex lock;
    int64_t slice_start_time;
    int64_t slice_end_time;
    uint64_t slice_quota;
    uint64_t slice_ns;
    uint64_t dispatched;
} RateLimit;

static inline int64_t ratelimit_calculate_delay(RateLimit *limit, uint64_t n)
{
    int64_t now = qemu_clock_get_ns(QEMU_CLOCK_REALTIME);
    double delay_slices;

    QEMU_LOCK_GUARD(&limit->lock);
    if (!limit->slice_quota) {
        return 0;
    }
    assert(limit->slice_ns);

    if (limit->slice_end_time < now) {
        limit->slice_start_time = now;
        limit->slice_end_time = now + limit->slice_ns;
        limit->dispatched = 0;
    }

    limit->dispatched += n;
    if (limit->dispatched < limit->slice_quota) {
        return 0;
    }

    delay_slices = (double)limit->dispatched / limit->slice_quota;
    limit->slice_end_time = limit->slice_start_time +
        (uint64_t)(delay_slices * limit->slice_ns);
    return limit->slice_end_time - now;
}

static inline void ratelimit_init(RateLimit *limit)
{
    qemu_mutex_init(&limit->lock);
}

static inline void ratelimit_destroy(RateLimit *limit)
{
    qemu_mutex_destroy(&limit->lock);
}

static inline void ratelimit_set_speed(RateLimit *limit, uint64_t speed,
                                       uint64_t slice_ns)
{
    QEMU_LOCK_GUARD(&limit->lock);
    limit->slice_ns = slice_ns;
    if (speed == 0) {
        limit->slice_quota = 0;
    } else {
        limit->slice_quota = MAX(((double)speed * slice_ns) / 1000000000ULL, 1);
    }
}

#endif
