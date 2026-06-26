#ifndef QEMU_EXEC_LOG_H
#define QEMU_EXEC_LOG_H

#include "qemu/log.h"
#include "hw/core/cpu.h"
#include "disas/disas.h"

static inline void log_cpu_state(CPUState *cpu, int flags)
{
    FILE *f = qemu_log_trylock();
    if (f) {
        cpu_dump_state(cpu, f, flags);
        qemu_log_unlock(f);
    }
}

static inline void log_cpu_state_mask(int mask, CPUState *cpu, int flags)
{
    if (qemu_loglevel & mask) {
        log_cpu_state(cpu, flags);
    }
}

#endif
