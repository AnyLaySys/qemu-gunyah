

#ifndef HVF_H
#define HVF_H

#include "qemu/accel.h"
#include "qom/object.h"

#ifdef COMPILING_PER_TARGET
#include "cpu.h"

#ifdef CONFIG_HVF
extern bool hvf_allowed;
#define hvf_enabled() (hvf_allowed)
#else /* !CONFIG_HVF */
#define hvf_enabled() 0
#endif /* !CONFIG_HVF */

#endif /* COMPILING_PER_TARGET */

#define TYPE_HVF_ACCEL ACCEL_CLASS_NAME("hvf")

typedef struct HVFState HVFState;
DECLARE_INSTANCE_CHECKER(HVFState, HVF_STATE,
                         TYPE_HVF_ACCEL)

#ifdef COMPILING_PER_TARGET
struct hvf_sw_breakpoint {
    vaddr pc;
    vaddr saved_insn;
    int use_count;
    QTAILQ_ENTRY(hvf_sw_breakpoint) entry;
};

struct hvf_sw_breakpoint *hvf_find_sw_breakpoint(CPUState *cpu,
                                                 vaddr pc);
int hvf_sw_breakpoints_active(CPUState *cpu);

int hvf_arch_insert_sw_breakpoint(CPUState *cpu, struct hvf_sw_breakpoint *bp);
int hvf_arch_remove_sw_breakpoint(CPUState *cpu, struct hvf_sw_breakpoint *bp);
int hvf_arch_insert_hw_breakpoint(vaddr addr, vaddr len, int type);
int hvf_arch_remove_hw_breakpoint(vaddr addr, vaddr len, int type);
void hvf_arch_remove_all_hw_breakpoints(void);

int hvf_update_guest_debug(CPUState *cpu);
void hvf_arch_update_guest_debug(CPUState *cpu);

bool hvf_arch_supports_guest_debug(void);
#endif /* COMPILING_PER_TARGET */

#endif
