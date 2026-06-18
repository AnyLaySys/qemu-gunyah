


























#include "qemu/osdep.h"
#include "system/accel-ops.h"
#include "system/tcg.h"
#include "system/replay.h"
#include "system/cpu-timers.h"
#include "qemu/main-loop.h"
#include "qemu/guest-random.h"
#include "qemu/timer.h"
#include "exec/cputlb.h"
#include "exec/hwaddr.h"
#include "exec/tb-flush.h"
#include "exec/translation-block.h"
#include "gdbstub/enums.h"

#include "hw/core/cpu.h"

#include "tcg-accel-ops.h"
#include "tcg-accel-ops-mttcg.h"
#include "tcg-accel-ops-rr.h"
#include "tcg-accel-ops-icount.h"



void tcg_cpu_init_cflags(CPUState *cpu, bool parallel)
{
    uint32_t cflags;









    cflags = cpu->cluster_index << CF_CLUSTER_SHIFT;

    cflags |= parallel ? CF_PARALLEL : 0;
    cflags |= icount_enabled() ? CF_USE_ICOUNT : 0;
    tcg_cflags_set(cpu, cflags);
}

void tcg_cpu_destroy(CPUState *cpu)
{
    cpu_thread_signal_destroyed(cpu);
}

int tcg_cpu_exec(CPUState *cpu)
{
    int ret;
    assert(tcg_enabled());
    cpu_exec_start(cpu);
    ret = cpu_exec(cpu);
    cpu_exec_end(cpu);
    return ret;
}

static void tcg_cpu_reset_hold(CPUState *cpu)
{
    tcg_flush_jmp_cache(cpu);

    tlb_flush(cpu);
}


void tcg_handle_interrupt(CPUState *cpu, int mask)
{
    g_assert(bql_locked());

    cpu->interrupt_request |= mask;





    if (!qemu_cpu_is_self(cpu)) {
        qemu_cpu_kick(cpu);
    } else {
        qatomic_set(&cpu->neg.icount_decr.u16.high, -1);
    }
}

static bool tcg_supports_guest_debug(void)
{
    return true;
}


static inline int xlat_gdb_type(CPUState *cpu, int gdbtype)
{
    static const int xlat[] = {
        [GDB_WATCHPOINT_WRITE]  = BP_GDB | BP_MEM_WRITE,
        [GDB_WATCHPOINT_READ]   = BP_GDB | BP_MEM_READ,
        [GDB_WATCHPOINT_ACCESS] = BP_GDB | BP_MEM_ACCESS,
    };

    int cputype = xlat[gdbtype];

    if (cpu->cc->gdb_stop_before_watchpoint) {
        cputype |= BP_STOP_BEFORE_ACCESS;
    }
    return cputype;
}

static int tcg_insert_breakpoint(CPUState *cs, int type, vaddr addr, vaddr len)
{
    CPUState *cpu;
    int err = 0;

    switch (type) {
    case GDB_BREAKPOINT_SW:
    case GDB_BREAKPOINT_HW:
        CPU_FOREACH(cpu) {
            err = cpu_breakpoint_insert(cpu, addr, BP_GDB, NULL);
            if (err) {
                break;
            }
        }
        return err;
    case GDB_WATCHPOINT_WRITE:
    case GDB_WATCHPOINT_READ:
    case GDB_WATCHPOINT_ACCESS:
        CPU_FOREACH(cpu) {
            err = cpu_watchpoint_insert(cpu, addr, len,
                                        xlat_gdb_type(cpu, type), NULL);
            if (err) {
                break;
            }
        }
        return err;
    default:
        return -ENOSYS;
    }
}

static int tcg_remove_breakpoint(CPUState *cs, int type, vaddr addr, vaddr len)
{
    CPUState *cpu;
    int err = 0;

    switch (type) {
    case GDB_BREAKPOINT_SW:
    case GDB_BREAKPOINT_HW:
        CPU_FOREACH(cpu) {
            err = cpu_breakpoint_remove(cpu, addr, BP_GDB);
            if (err) {
                break;
            }
        }
        return err;
    case GDB_WATCHPOINT_WRITE:
    case GDB_WATCHPOINT_READ:
    case GDB_WATCHPOINT_ACCESS:
        CPU_FOREACH(cpu) {
            err = cpu_watchpoint_remove(cpu, addr, len,
                                        xlat_gdb_type(cpu, type));
            if (err) {
                break;
            }
        }
        return err;
    default:
        return -ENOSYS;
    }
}

static inline void tcg_remove_all_breakpoints(CPUState *cpu)
{
    cpu_breakpoint_remove_all(cpu, BP_GDB);
    cpu_watchpoint_remove_all(cpu, BP_GDB);
}

static void tcg_accel_ops_init(AccelOpsClass *ops)
{
    if (qemu_tcg_mttcg_enabled()) {
        ops->create_vcpu_thread = mttcg_start_vcpu_thread;
        ops->kick_vcpu_thread = mttcg_kick_vcpu_thread;
        ops->handle_interrupt = tcg_handle_interrupt;
    } else {
        ops->create_vcpu_thread = rr_start_vcpu_thread;
        ops->kick_vcpu_thread = rr_kick_vcpu_thread;

        if (icount_enabled()) {
            ops->handle_interrupt = icount_handle_interrupt;
            ops->get_virtual_clock = icount_get;
            ops->get_elapsed_ticks = icount_get;
        } else {
            ops->handle_interrupt = tcg_handle_interrupt;
        }
    }

    ops->cpu_reset_hold = tcg_cpu_reset_hold;
    ops->supports_guest_debug = tcg_supports_guest_debug;
    ops->insert_breakpoint = tcg_insert_breakpoint;
    ops->remove_breakpoint = tcg_remove_breakpoint;
    ops->remove_all_breakpoints = tcg_remove_all_breakpoints;
}

static void tcg_accel_ops_class_init(ObjectClass *oc, void *data)
{
    AccelOpsClass *ops = ACCEL_OPS_CLASS(oc);

    ops->ops_init = tcg_accel_ops_init;
}

static const TypeInfo tcg_accel_ops_type = {
    .name = ACCEL_OPS_NAME("tcg"),

    .parent = TYPE_ACCEL_OPS,
    .class_init = tcg_accel_ops_class_init,
    .abstract = true,
};
module_obj(ACCEL_OPS_NAME("tcg"));

static void tcg_accel_ops_register_types(void)
{
    type_register_static(&tcg_accel_ops_type);
}
type_init(tcg_accel_ops_register_types);
