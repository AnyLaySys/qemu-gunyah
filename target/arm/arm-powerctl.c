
#include "qemu/osdep.h"
#include "cpu.h"
#include "cpu-qom.h"
#include "internals.h"
#include "arm-powerctl.h"
#include "qemu/log.h"
#include "qemu/main-loop.h"
#include "system/tcg.h"
#include "target/arm/multiprocessing.h"

#ifndef DEBUG_ARM_POWERCTL
#define DEBUG_ARM_POWERCTL 0
#endif

#define DPRINTF(fmt, args...) \
    do { \
        if (DEBUG_ARM_POWERCTL) { \
            fprintf(stderr, "[ARM]%s: " fmt , __func__, ##args); \
        } \
    } while (0)

CPUState *arm_get_cpu_by_id(uint64_t id)
{
    CPUState *cpu;

    DPRINTF("cpu %" PRId64 "\n", id);

    CPU_FOREACH(cpu) {
        ARMCPU *armcpu = ARM_CPU(cpu);

        if (arm_cpu_mp_affinity(armcpu) == id) {
            return cpu;
        }
    }

    qemu_log_mask(LOG_GUEST_ERROR,
                  "[ARM]%s: Requesting unknown CPU %" PRId64 "\n",
                  __func__, id);

    return NULL;
}

struct CpuOnInfo {
    uint64_t entry;
    uint64_t context_id;
    uint32_t target_el;
    bool target_aa64;
};


static void arm_set_cpu_on_async_work(CPUState *target_cpu_state,
                                      run_on_cpu_data data)
{
    ARMCPU *target_cpu = ARM_CPU(target_cpu_state);
    struct CpuOnInfo *info = (struct CpuOnInfo *) data.host_ptr;

    cpu_reset(target_cpu_state);
    arm_emulate_firmware_reset(target_cpu_state, info->target_el);
    target_cpu_state->halted = 0;

    assert(info->target_el == arm_current_el(&target_cpu->env));

    if (info->target_aa64) {
        target_cpu->env.xregs[0] = info->context_id;
    } else {
        target_cpu->env.regs[0] = info->context_id;
    }

    if (tcg_enabled()) {
        arm_rebuild_hflags(&target_cpu->env);
    }

    cpu_set_pc(target_cpu_state, info->entry);

    g_free(info);

    assert(bql_locked());
    target_cpu->power_state = PSCI_ON;
}

int arm_set_cpu_on(uint64_t cpuid, uint64_t entry, uint64_t context_id,
                   uint32_t target_el, bool target_aa64)
{
    CPUState *target_cpu_state;
    ARMCPU *target_cpu;
    struct CpuOnInfo *info;

    assert(bql_locked());

    DPRINTF("cpu %" PRId64 " (EL %d, %s) @ 0x%" PRIx64 " with R0 = 0x%" PRIx64
            "\n", cpuid, target_el, target_aa64 ? "aarch64" : "aarch32", entry,
            context_id);

    assert((target_el > 0) && (target_el < 4));

    if (target_aa64 && (entry & 3)) {
        return QEMU_ARM_POWERCTL_INVALID_PARAM;
    }

    target_cpu_state = arm_get_cpu_by_id(cpuid);
    if (!target_cpu_state) {
        return QEMU_ARM_POWERCTL_INVALID_PARAM;
    }

    target_cpu = ARM_CPU(target_cpu_state);
    if (target_cpu->power_state == PSCI_ON) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "[ARM]%s: CPU %" PRId64 " is already on\n",
                      __func__, cpuid);
        return QEMU_ARM_POWERCTL_ALREADY_ON;
    }


    if (((target_el == 3) && !arm_feature(&target_cpu->env, ARM_FEATURE_EL3)) ||
        ((target_el == 2) && !arm_feature(&target_cpu->env, ARM_FEATURE_EL2))) {
        return QEMU_ARM_POWERCTL_INVALID_PARAM;
    }

    if (!target_aa64 && arm_feature(&target_cpu->env, ARM_FEATURE_AARCH64)) {
        qemu_log_mask(LOG_UNIMP,
                      "[ARM]%s: Starting AArch64 CPU %" PRId64
                      " in AArch32 mode is not supported yet\n",
                      __func__, cpuid);
        return QEMU_ARM_POWERCTL_INVALID_PARAM;
    }

    if (target_cpu->power_state == PSCI_ON_PENDING) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "[ARM]%s: CPU %" PRId64 " is already powering on\n",
                      __func__, cpuid);
        return QEMU_ARM_POWERCTL_ON_PENDING;
    }

    info = g_new(struct CpuOnInfo, 1);
    info->entry = entry;
    info->context_id = context_id;
    info->target_el = target_el;
    info->target_aa64 = target_aa64;

    async_run_on_cpu(target_cpu_state, arm_set_cpu_on_async_work,
                     RUN_ON_CPU_HOST_PTR(info));

    return QEMU_ARM_POWERCTL_RET_SUCCESS;
}

static void arm_set_cpu_on_and_reset_async_work(CPUState *target_cpu_state,
                                                run_on_cpu_data data)
{
    ARMCPU *target_cpu = ARM_CPU(target_cpu_state);

    cpu_reset(target_cpu_state);
    target_cpu_state->halted = 0;

    assert(bql_locked());
    target_cpu->power_state = PSCI_ON;
}

int arm_set_cpu_on_and_reset(uint64_t cpuid)
{
    CPUState *target_cpu_state;
    ARMCPU *target_cpu;

    assert(bql_locked());

    target_cpu_state = arm_get_cpu_by_id(cpuid);
    if (!target_cpu_state) {
        return QEMU_ARM_POWERCTL_INVALID_PARAM;
    }

    target_cpu = ARM_CPU(target_cpu_state);
    if (target_cpu->power_state == PSCI_ON) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "[ARM]%s: CPU %" PRId64 " is already on\n",
                      __func__, cpuid);
        return QEMU_ARM_POWERCTL_ALREADY_ON;
    }

    if (target_cpu->power_state == PSCI_ON_PENDING) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "[ARM]%s: CPU %" PRId64 " is already powering on\n",
                      __func__, cpuid);
        return QEMU_ARM_POWERCTL_ON_PENDING;
    }

    async_run_on_cpu(target_cpu_state, arm_set_cpu_on_and_reset_async_work,
                     RUN_ON_CPU_NULL);

    return QEMU_ARM_POWERCTL_RET_SUCCESS;
}

static void arm_set_cpu_off_async_work(CPUState *target_cpu_state,
                                       run_on_cpu_data data)
{
    ARMCPU *target_cpu = ARM_CPU(target_cpu_state);

    assert(bql_locked());
    target_cpu->power_state = PSCI_OFF;
    target_cpu_state->halted = 1;
    target_cpu_state->exception_index = EXCP_HLT;
}

int arm_set_cpu_off(uint64_t cpuid)
{
    CPUState *target_cpu_state;
    ARMCPU *target_cpu;

    assert(bql_locked());

    DPRINTF("cpu %" PRId64 "\n", cpuid);

    target_cpu_state = arm_get_cpu_by_id(cpuid);
    if (!target_cpu_state) {
        return QEMU_ARM_POWERCTL_INVALID_PARAM;
    }
    target_cpu = ARM_CPU(target_cpu_state);
    if (target_cpu->power_state == PSCI_OFF) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "[ARM]%s: CPU %" PRId64 " is already off\n",
                      __func__, cpuid);
        return QEMU_ARM_POWERCTL_IS_OFF;
    }

    async_run_on_cpu(target_cpu_state, arm_set_cpu_off_async_work,
                     RUN_ON_CPU_NULL);

    return QEMU_ARM_POWERCTL_RET_SUCCESS;
}

static void arm_reset_cpu_async_work(CPUState *target_cpu_state,
                                     run_on_cpu_data data)
{
    cpu_reset(target_cpu_state);
}

int arm_reset_cpu(uint64_t cpuid)
{
    CPUState *target_cpu_state;
    ARMCPU *target_cpu;

    assert(bql_locked());

    DPRINTF("cpu %" PRId64 "\n", cpuid);

    target_cpu_state = arm_get_cpu_by_id(cpuid);
    if (!target_cpu_state) {
        return QEMU_ARM_POWERCTL_INVALID_PARAM;
    }
    target_cpu = ARM_CPU(target_cpu_state);

    if (target_cpu->power_state == PSCI_OFF) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "[ARM]%s: CPU %" PRId64 " is off\n",
                      __func__, cpuid);
        return QEMU_ARM_POWERCTL_IS_OFF;
    }

    async_run_on_cpu(target_cpu_state, arm_reset_cpu_async_work,
                     RUN_ON_CPU_NULL);

    return QEMU_ARM_POWERCTL_RET_SUCCESS;
}
