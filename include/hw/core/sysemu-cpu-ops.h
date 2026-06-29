
#ifndef SYSTEM_CPU_OPS_H
#define SYSTEM_CPU_OPS_H

#include "hw/core/cpu.h"

typedef struct SysemuCPUOps {
    bool (*has_work)(CPUState *cpu); /* MANDATORY NON-NULL */
    bool (*get_paging_enabled)(const CPUState *cpu);
    hwaddr (*get_phys_page_debug)(CPUState *cpu, vaddr addr);
    hwaddr (*get_phys_page_attrs_debug)(CPUState *cpu, vaddr addr,
                                        MemTxAttrs *attrs);
    int (*asidx_from_attrs)(CPUState *cpu, MemTxAttrs attrs);
    GuestPanicInformation* (*get_crash_info)(CPUState *cpu);
    bool (*virtio_is_big_endian)(CPUState *cpu);

    const VMStateDescription *legacy_vmsd;

} SysemuCPUOps;

#endif /* SYSTEM_CPU_OPS_H */
