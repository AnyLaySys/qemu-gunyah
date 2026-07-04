#ifndef SYSTEM_CPU_TIMERS_H
#define SYSTEM_CPU_TIMERS_H

#include "qemu/timer.h"

void cpu_timers_init(void);

#define icount_enabled() 0


void cpu_enable_ticks(void);
void cpu_disable_ticks(void);

int64_t cpu_get_ticks(void);

int64_t cpu_get_clock(void);

void qemu_timer_notify_cb(void *opaque, QEMUClockType type);

int64_t cpus_get_virtual_clock(void);
void cpus_set_virtual_clock(int64_t new_time);
int64_t cpus_get_elapsed_ticks(void);

#endif /* SYSTEM_CPU_TIMERS_H */
