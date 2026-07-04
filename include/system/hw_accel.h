
#ifndef QEMU_HW_ACCEL_H
#define QEMU_HW_ACCEL_H

#include "hw/core/cpu.h"
#include "system/gunyah.h"
#include "system/hvf.h"
#include "system/whpx.h"
#include "system/nvmm.h"

void cpu_synchronize_state(CPUState *cpu);
void cpu_synchronize_post_reset(CPUState *cpu);
void cpu_synchronize_post_init(CPUState *cpu);
void cpu_synchronize_pre_loadvm(CPUState *cpu);

#endif /* QEMU_HW_ACCEL_H */
