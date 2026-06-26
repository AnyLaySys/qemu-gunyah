
#ifndef TCG_CPU_OPS_H
#define TCG_CPU_OPS_H

#include "exec/breakpoint.h"
#include "exec/hwaddr.h"
#include "exec/memattrs.h"
#include "exec/memop.h"
#include "exec/mmu-access-type.h"
#include "exec/vaddr.h"

struct TCGCPUOps {
    void (*initialize)(void);
    void (*translate_code)(CPUState *cpu, TranslationBlock *tb,
                           int *max_insns, vaddr pc, void *host_pc);
    void (*synchronize_from_tb)(CPUState *cpu, const TranslationBlock *tb);
    void (*restore_state_to_opc)(CPUState *cpu, const TranslationBlock *tb,
                                 const uint64_t *data);

    void (*cpu_exec_enter)(CPUState *cpu);
    void (*cpu_exec_exit)(CPUState *cpu);
    void (*debug_excp_handler)(CPUState *cpu);

#ifdef CONFIG_USER_ONLY
    void (*fake_user_interrupt)(CPUState *cpu);

    void (*record_sigsegv)(CPUState *cpu, vaddr addr,
                           MMUAccessType access_type,
                           bool maperr, uintptr_t ra);
    void (*record_sigbus)(CPUState *cpu, vaddr addr,
                          MMUAccessType access_type, uintptr_t ra);
#else
    void (*do_interrupt)(CPUState *cpu);
    bool (*cpu_exec_interrupt)(CPUState *cpu, int interrupt_request);
    bool (*cpu_exec_halt)(CPUState *cpu);
    bool (*tlb_fill_align)(CPUState *cpu, CPUTLBEntryFull *out, vaddr addr,
                           MMUAccessType access_type, int mmu_idx,
                           MemOp memop, int size, bool probe, uintptr_t ra);
    bool (*tlb_fill)(CPUState *cpu, vaddr address, int size,
                     MMUAccessType access_type, int mmu_idx,
                     bool probe, uintptr_t retaddr);
    void (*do_transaction_failed)(CPUState *cpu, hwaddr physaddr, vaddr addr,
                                  unsigned size, MMUAccessType access_type,
                                  int mmu_idx, MemTxAttrs attrs,
                                  MemTxResult response, uintptr_t retaddr);
    G_NORETURN void (*do_unaligned_access)(CPUState *cpu, vaddr addr,
                                           MMUAccessType access_type,
                                           int mmu_idx, uintptr_t retaddr);

    vaddr (*adjust_watchpoint_address)(CPUState *cpu, vaddr addr, int len);

    bool (*debug_check_watchpoint)(CPUState *cpu, CPUWatchpoint *wp);

    bool (*debug_check_breakpoint)(CPUState *cpu);

    bool (*io_recompile_replay_branch)(CPUState *cpu,
                                       const TranslationBlock *tb);
    bool (*need_replay_interrupt)(int interrupt_request);
#endif /* !CONFIG_USER_ONLY */
};

#if defined(CONFIG_USER_ONLY)

static inline void cpu_check_watchpoint(CPUState *cpu, vaddr addr, vaddr len,
                                        MemTxAttrs atr, int fl, uintptr_t ra)
{
}

static inline int cpu_watchpoint_address_matches(CPUState *cpu,
                                                 vaddr addr, vaddr len)
{
    return 0;
}

#else

void cpu_check_watchpoint(CPUState *cpu, vaddr addr, vaddr len,
                          MemTxAttrs attrs, int flags, uintptr_t ra);

int cpu_watchpoint_address_matches(CPUState *cpu, vaddr addr, vaddr len);

#endif

#endif /* TCG_CPU_OPS_H */
