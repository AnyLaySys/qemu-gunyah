#ifndef QEMU_ARM_CPU_QOM_H
#define QEMU_ARM_CPU_QOM_H

#include "hw/core/cpu.h"

#define TYPE_ARM_CPU "arm-cpu"

OBJECT_DECLARE_CPU_TYPE(ARMCPU, ARMCPUClass, ARM_CPU)

#define TYPE_ARM_MAX_CPU "max-" TYPE_ARM_CPU

#define TYPE_AARCH64_CPU "aarch64-cpu"
typedef struct AArch64CPUClass AArch64CPUClass;
DECLARE_CLASS_CHECKERS(AArch64CPUClass, AARCH64_CPU,
                       TYPE_AARCH64_CPU)

#define ARM_CPU_TYPE_SUFFIX "-" TYPE_ARM_CPU
#define ARM_CPU_TYPE_NAME(name) (name ARM_CPU_TYPE_SUFFIX)

#define ARM_CPU_IRQ 0
#define ARM_CPU_FIQ 1
#define ARM_CPU_VIRQ 2
#define ARM_CPU_VFIQ 3
#define ARM_CPU_NMI 4
#define ARM_CPU_VINMI 5
#define ARM_CPU_VFNMI 6

enum {
    M_REG_NS = 0,
    M_REG_S = 1,
    M_REG_NUM_BANKS = 2,
};

#endif
