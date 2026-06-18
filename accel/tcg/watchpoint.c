


















#include "qemu/osdep.h"
#include "qemu/main-loop.h"
#include "exec/breakpoint.h"
#include "exec/cpu-interrupt.h"
#include "exec/page-protection.h"
#include "exec/translation-block.h"
#include "system/tcg.h"
#include "system/replay.h"
#include "accel/tcg/cpu-ops.h"
#include "hw/core/cpu.h"
#include "internal-common.h"







static inline bool watchpoint_address_matches(CPUWatchpoint *wp,
                                              vaddr addr, vaddr len)
{






    vaddr wpend = wp->vaddr + wp->len - 1;
    vaddr addrend = addr + len - 1;

    return !(addr > wpend || wp->vaddr > addrend);
}


int cpu_watchpoint_address_matches(CPUState *cpu, vaddr addr, vaddr len)
{
    CPUWatchpoint *wp;
    int ret = 0;

    QTAILQ_FOREACH(wp, &cpu->watchpoints, entry) {
        if (watchpoint_address_matches(wp, addr, len)) {
            ret |= wp->flags;
        }
    }
    return ret;
}


void cpu_check_watchpoint(CPUState *cpu, vaddr addr, vaddr len,
                          MemTxAttrs attrs, int flags, uintptr_t ra)
{
    CPUWatchpoint *wp;

    assert(tcg_enabled());
    if (cpu->watchpoint_hit) {





        bql_lock();
        cpu_interrupt(cpu, CPU_INTERRUPT_DEBUG);
        bql_unlock();
        return;
    }

    if (cpu->cc->tcg_ops->adjust_watchpoint_address) {

        addr = cpu->cc->tcg_ops->adjust_watchpoint_address(cpu, addr, len);
    }

    assert((flags & ~BP_MEM_ACCESS) == 0);
    QTAILQ_FOREACH(wp, &cpu->watchpoints, entry) {
        int hit_flags = wp->flags & flags;

        if (hit_flags && watchpoint_address_matches(wp, addr, len)) {
            if (replay_running_debug()) {





                if (!cpu->neg.can_do_io) {

                    cpu->cflags_next_tb = 1 | CF_NOIRQ | curr_cflags(cpu);
                    cpu_loop_exit_restore(cpu, ra);
                }




                replay_breakpoint();
                return;
            }

            wp->flags |= hit_flags << BP_HIT_SHIFT;
            wp->hitaddr = MAX(addr, wp->vaddr);
            wp->hitattrs = attrs;

            if (wp->flags & BP_CPU
                && cpu->cc->tcg_ops->debug_check_watchpoint
                && !cpu->cc->tcg_ops->debug_check_watchpoint(cpu, wp)) {
                wp->flags &= ~BP_WATCHPOINT_HIT;
                continue;
            }
            cpu->watchpoint_hit = wp;

            mmap_lock();

            tb_check_watchpoint(cpu, ra);
            if (wp->flags & BP_STOP_BEFORE_ACCESS) {
                cpu->exception_index = EXCP_DEBUG;
                mmap_unlock();
                cpu_loop_exit(cpu);
            } else {

                cpu->cflags_next_tb = 1 | CF_NOIRQ | curr_cflags(cpu);
                mmap_unlock();
                cpu_loop_exit_noexc(cpu);
            }
        } else {
            wp->flags &= ~BP_WATCHPOINT_HIT;
        }
    }
}
