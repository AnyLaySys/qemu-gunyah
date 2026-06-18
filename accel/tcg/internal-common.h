







#ifndef ACCEL_TCG_INTERNAL_COMMON_H
#define ACCEL_TCG_INTERNAL_COMMON_H

#include "exec/cpu-common.h"
#include "exec/translation-block.h"

extern int64_t max_delay;
extern int64_t max_advance;

extern bool one_insn_per_tb;

extern bool icount_align_option;





static inline bool cpu_in_serial_context(CPUState *cs)
{
    return !tcg_cflags_has(cs, CF_PARALLEL) || cpu_in_exclusive_context(cs);
}









static inline bool cpu_plugin_mem_cbs_enabled(const CPUState *cpu)
{
#ifdef CONFIG_PLUGIN
    return !!cpu->neg.plugin_mem_cbs;
#else
    return false;
#endif
}

TranslationBlock *tb_gen_code(CPUState *cpu, vaddr pc,
                              uint64_t cs_base, uint32_t flags,
                              int cflags);
void page_init(void);
void tb_htable_init(void);
void tb_reset_jump(TranslationBlock *tb, int n);
TranslationBlock *tb_link_page(TranslationBlock *tb);
void cpu_restore_state_from_tb(CPUState *cpu, TranslationBlock *tb,
                               uintptr_t host_pc);





void tlb_init(CPUState *cpu);




void tlb_destroy(CPUState *cpu);

bool tcg_exec_realizefn(CPUState *cpu, Error **errp);
void tcg_exec_unrealizefn(CPUState *cpu);


uint32_t curr_cflags(CPUState *cpu);

void tb_check_watchpoint(CPUState *cpu, uintptr_t retaddr);

#endif
