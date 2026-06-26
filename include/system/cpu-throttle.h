
#ifndef SYSTEM_CPU_THROTTLE_H
#define SYSTEM_CPU_THROTTLE_H

#include "qemu/timer.h"

void cpu_throttle_init(void);

void cpu_throttle_set(int new_throttle_pct);

void cpu_throttle_stop(void);

bool cpu_throttle_active(void);

int cpu_throttle_get_percentage(void);

void cpu_throttle_dirty_sync_timer_tick(void *opaque);

void cpu_throttle_dirty_sync_timer(bool enable);

#endif /* SYSTEM_CPU_THROTTLE_H */
