
#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "exec/cputlb.h"
#include "exec/target_page.h"
#include "hw/core/cpu.h"

int cpu_watchpoint_insert(CPUState *cpu, vaddr addr, vaddr len,
                          int flags, CPUWatchpoint **watchpoint)
{
    CPUWatchpoint *wp;
    vaddr in_page;

    if (len == 0 || (addr + len - 1) < addr) {
        error_report("tried to set invalid watchpoint at %"
                     VADDR_PRIx ", len=%" VADDR_PRIu, addr, len);
        return -EINVAL;
    }
    wp = g_malloc(sizeof(*wp));

    wp->vaddr = addr;
    wp->len = len;
    wp->flags = flags;

    QTAILQ_INSERT_TAIL(&cpu->watchpoints, wp, entry);

    in_page = -(addr | TARGET_PAGE_MASK);
    if (len <= in_page) {
        tlb_flush_page(cpu, addr);
    } else {
        tlb_flush(cpu);
    }

    if (watchpoint) {
        *watchpoint = wp;
    }
    return 0;
}

int cpu_watchpoint_remove(CPUState *cpu, vaddr addr, vaddr len,
                          int flags)
{
    CPUWatchpoint *wp;

    QTAILQ_FOREACH(wp, &cpu->watchpoints, entry) {
        if (addr == wp->vaddr && len == wp->len
                && flags == (wp->flags & ~BP_WATCHPOINT_HIT)) {
            cpu_watchpoint_remove_by_ref(cpu, wp);
            return 0;
        }
    }
    return -ENOENT;
}

void cpu_watchpoint_remove_by_ref(CPUState *cpu, CPUWatchpoint *watchpoint)
{
    QTAILQ_REMOVE(&cpu->watchpoints, watchpoint, entry);

    tlb_flush_page(cpu, watchpoint->vaddr);

    g_free(watchpoint);
}

void cpu_watchpoint_remove_all(CPUState *cpu, int mask)
{
    CPUWatchpoint *wp, *next;

    QTAILQ_FOREACH_SAFE(wp, &cpu->watchpoints, entry, next) {
        if (wp->flags & mask) {
            cpu_watchpoint_remove_by_ref(cpu, wp);
        }
    }
}
