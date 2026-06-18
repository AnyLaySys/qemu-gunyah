










#ifndef TCG_ACCEL_OPS_H
#define TCG_ACCEL_OPS_H

#include "system/cpus.h"

void tcg_cpu_destroy(CPUState *cpu);
int tcg_cpu_exec(CPUState *cpu);
void tcg_handle_interrupt(CPUState *cpu, int mask);
void tcg_cpu_init_cflags(CPUState *cpu, bool parallel);

#endif
