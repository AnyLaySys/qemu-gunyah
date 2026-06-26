
#ifndef PLUGIN_MEMORY_H
#define PLUGIN_MEMORY_H

#include "exec/hwaddr.h"

struct qemu_plugin_hwaddr {
    bool is_io;
    bool is_store;
    hwaddr phys_addr;
    MemoryRegion *mr;
};

bool tlb_plugin_lookup(CPUState *cpu, vaddr addr, int mmu_idx,
                       bool is_store, struct qemu_plugin_hwaddr *data);

#endif /* PLUGIN_MEMORY_H */
