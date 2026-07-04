#include "qemu/osdep.h"
#include "exec/tb-flush.h"
#include "exec/exec-all.h"
G_NORETURN void cpu_loop_exit(CPUState*cpu){g_assert_not_reached();}G_NORETURN void cpu_loop_exit_restore(CPUState*cpu,uintptr_t pc){g_assert_not_reached();}
