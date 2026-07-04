
#ifndef QEMU_HVF_ARM_H
#define QEMU_HVF_ARM_H

#include "cpu.h"

void hvf_arm_init_debug(void);

void hvf_arm_set_cpu_features_from_host(ARMCPU *cpu);

#ifdef CONFIG_HVF

uint32_t hvf_arm_get_default_ipa_bit_size(void);
uint32_t hvf_arm_get_max_ipa_bit_size(void);

#else

static inline uint32_t hvf_arm_get_default_ipa_bit_size(void)
{
    return 0;
}

static inline uint32_t hvf_arm_get_max_ipa_bit_size(void)
{
    return 0;
}

#endif

#endif
