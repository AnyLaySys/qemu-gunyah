#ifndef ACCEL_BLOCKER_H
#define ACCEL_BLOCKER_H

#include "system/cpus.h"

void accel_blocker_init(void);

void accel_ioctl_begin(void);
void accel_ioctl_end(void);
void accel_cpu_ioctl_begin(CPUState *cpu);
void accel_cpu_ioctl_end(CPUState *cpu);

void accel_ioctl_inhibit_begin(void);

void accel_ioctl_inhibit_end(void);

#endif /* ACCEL_BLOCKER_H */
