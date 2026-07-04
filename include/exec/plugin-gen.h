#ifndef QEMU_PLUGIN_GEN_H
#define QEMU_PLUGIN_GEN_H

#include "tcg/tcg.h"

struct DisasContextBase;

#ifdef CONFIG_PLUGIN

bool plugin_gen_tb_start(CPUState *cpu, const struct DisasContextBase *db);
void plugin_gen_tb_end(CPUState *cpu, size_t num_insns);
void plugin_gen_insn_start(CPUState *cpu, const struct DisasContextBase *db);
void plugin_gen_insn_end(void);

void plugin_gen_disable_mem_helpers(void);

#else /* !CONFIG_PLUGIN */

static inline
bool plugin_gen_tb_start(CPUState *cpu, const struct DisasContextBase *db)
{
    return false;
}

static inline
void plugin_gen_insn_start(CPUState *cpu, const struct DisasContextBase *db)
{ }

static inline void plugin_gen_insn_end(void)
{ }

static inline void plugin_gen_tb_end(CPUState *cpu, size_t num_insns)
{ }

static inline void plugin_gen_disable_mem_helpers(void)
{ }

#endif /* CONFIG_PLUGIN */

#endif /* QEMU_PLUGIN_GEN_H */

