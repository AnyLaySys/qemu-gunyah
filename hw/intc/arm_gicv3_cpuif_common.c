
#include "qemu/osdep.h"
#include "gicv3_internal.h"
#include "cpu.h"

void gicv3_set_gicv3state(CPUState *cpu, GICv3CPUState *s)
{
    ARMCPU *arm_cpu = ARM_CPU(cpu);
    CPUARMState *env = &arm_cpu->env;

    env->gicv3state = (void *)s;
};
