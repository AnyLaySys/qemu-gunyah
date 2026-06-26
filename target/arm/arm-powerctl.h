
#ifndef QEMU_ARM_POWERCTL_H
#define QEMU_ARM_POWERCTL_H

#define QEMU_PSCI_RET_SUCCESS              0
#define QEMU_PSCI_RET_NOT_SUPPORTED        (-1)
#define QEMU_PSCI_RET_INVALID_PARAMS       (-2)
#define QEMU_PSCI_RET_DENIED               (-3)
#define QEMU_PSCI_RET_ALREADY_ON           (-4)
#define QEMU_PSCI_RET_ON_PENDING           (-5)

#define QEMU_ARM_POWERCTL_RET_SUCCESS QEMU_PSCI_RET_SUCCESS
#define QEMU_ARM_POWERCTL_INVALID_PARAM QEMU_PSCI_RET_INVALID_PARAMS
#define QEMU_ARM_POWERCTL_ALREADY_ON QEMU_PSCI_RET_ALREADY_ON
#define QEMU_ARM_POWERCTL_IS_OFF QEMU_PSCI_RET_DENIED
#define QEMU_ARM_POWERCTL_ON_PENDING QEMU_PSCI_RET_ON_PENDING

CPUState *arm_get_cpu_by_id(uint64_t cpuid);

int arm_set_cpu_on(uint64_t cpuid, uint64_t entry, uint64_t context_id,
                   uint32_t target_el, bool target_aa64);


int arm_set_cpu_off(uint64_t cpuid);

int arm_reset_cpu(uint64_t cpuid);

int arm_set_cpu_on_and_reset(uint64_t cpuid);

#endif
