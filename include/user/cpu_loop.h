
#ifndef USER_CPU_LOOP_H
#define USER_CPU_LOOP_H

#include "exec/abi_ptr.h"
#include "exec/mmu-access-type.h"
#include "exec/log.h"
#include "exec/target_long.h"
#include "special-errno.h"

MMUAccessType adjust_signal_pc(uintptr_t *pc, bool is_write);

bool handle_sigsegv_accerr_write(CPUState *cpu, sigset_t *old_set,
                                 uintptr_t host_pc, abi_ptr guest_addr);

G_NORETURN void cpu_loop_exit_sigsegv(CPUState *cpu, target_ulong addr,
                                      MMUAccessType access_type,
                                      bool maperr, uintptr_t ra);

G_NORETURN void cpu_loop_exit_sigbus(CPUState *cpu, target_ulong addr,
                                     MMUAccessType access_type,
                                     uintptr_t ra);

G_NORETURN void cpu_loop(CPUArchState *env);

void target_exception_dump(CPUArchState *env, const char *fmt, int code);
#define EXCP_DUMP(env, fmt, code) \
    target_exception_dump(env, fmt, code)

typedef struct target_pt_regs target_pt_regs;

void target_cpu_copy_regs(CPUArchState *env, target_pt_regs *regs);

#endif
