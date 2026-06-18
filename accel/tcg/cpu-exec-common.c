


















#include "qemu/osdep.h"
#include "exec/log.h"
#include "system/tcg.h"
#include "qemu/plugin.h"
#include "internal-common.h"

bool tcg_allowed;

bool tcg_cflags_has(CPUState *cpu, uint32_t flags)
{
    return cpu->tcg_cflags & flags;
}

void tcg_cflags_set(CPUState *cpu, uint32_t flags)
{
    cpu->tcg_cflags |= flags;
}

uint32_t curr_cflags(CPUState *cpu)
{
    uint32_t cflags = cpu->tcg_cflags;








    if (unlikely(cpu->singlestep_enabled)) {
        cflags |= CF_NO_GOTO_TB | CF_NO_GOTO_PTR | CF_SINGLE_STEP | 1;
    } else if (qatomic_read(&one_insn_per_tb)) {
        cflags |= CF_NO_GOTO_TB | 1;
    } else if (qemu_loglevel_mask(CPU_LOG_TB_NOCHAIN)) {
        cflags |= CF_NO_GOTO_TB;
    }

    return cflags;
}


void cpu_loop_exit_noexc(CPUState *cpu)
{
    cpu->exception_index = -1;
    cpu_loop_exit(cpu);
}

void cpu_loop_exit(CPUState *cpu)
{

    cpu->neg.can_do_io = true;

    qemu_plugin_disable_mem_helpers(cpu);
    siglongjmp(cpu->jmp_env, 1);
}

void cpu_loop_exit_restore(CPUState *cpu, uintptr_t pc)
{
    if (pc) {
        cpu_restore_state(cpu, pc);
    }
    cpu_loop_exit(cpu);
}

void cpu_loop_exit_atomic(CPUState *cpu, uintptr_t pc)
{

    g_assert(!cpu_in_serial_context(cpu));
    cpu->exception_index = EXCP_ATOMIC;
    cpu_loop_exit_restore(cpu, pc);
}
