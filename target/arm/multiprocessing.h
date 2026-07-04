
#ifndef TARGET_ARM_MULTIPROCESSING_H
#define TARGET_ARM_MULTIPROCESSING_H

#include "target/arm/cpu-qom.h"

uint64_t arm_cpu_mp_affinity(ARMCPU *cpu);

#endif
