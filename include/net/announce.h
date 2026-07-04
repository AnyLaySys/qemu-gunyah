
#ifndef QEMU_NET_ANNOUNCE_H
#define QEMU_NET_ANNOUNCE_H

#include "qapi/qapi-types-net.h"
#include "qemu/timer.h"

typedef struct AnnounceTimer {
    QEMUTimer *tm;
    AnnounceParameters params;
    QEMUClockType type;
    int round;
} AnnounceTimer;

int64_t qemu_announce_timer_step(AnnounceTimer *timer);

void qemu_announce_timer_del(AnnounceTimer *timer);

void qemu_announce_timer_reset(AnnounceTimer *timer,
                               AnnounceParameters *params,
                               QEMUClockType type,
                               QEMUTimerCB *cb,
                               void *opaque);

#endif
