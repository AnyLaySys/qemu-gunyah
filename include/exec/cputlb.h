
#ifndef CPUTLB_H
#define CPUTLB_H

#include "exec/cpu-common.h"
#include "exec/hwaddr.h"
#include "exec/memattrs.h"
#include "exec/vaddr.h"

void tlb_set_page_full(CPUState *cpu, int mmu_idx, vaddr addr,
                       CPUTLBEntryFull *full);

void tlb_set_page_with_attrs(CPUState *cpu, vaddr addr,
                             hwaddr paddr, MemTxAttrs attrs,
                             int prot, int mmu_idx, vaddr size);

void tlb_set_page(CPUState *cpu, vaddr addr,
                  hwaddr paddr, int prot,
                  int mmu_idx, vaddr size);

static inline void tlb_flush_page(CPUState *cpu, vaddr addr)
{
}
static inline void tlb_flush_page_all_cpus_synced(CPUState *src, vaddr addr)
{
}
static inline void tlb_flush(CPUState *cpu)
{
}
static inline void tlb_flush_all_cpus_synced(CPUState *src_cpu)
{
}
static inline void tlb_flush_page_by_mmuidx(CPUState *cpu,
                                            vaddr addr, uint16_t idxmap)
{
}

static inline void tlb_flush_by_mmuidx(CPUState *cpu, uint16_t idxmap)
{
}
static inline void tlb_flush_page_by_mmuidx_all_cpus_synced(CPUState *cpu,
                                                            vaddr addr,
                                                            uint16_t idxmap)
{
}
static inline void tlb_flush_by_mmuidx_all_cpus_synced(CPUState *cpu,
                                                       uint16_t idxmap)
{
}
static inline void tlb_flush_page_bits_by_mmuidx(CPUState *cpu,
                                                 vaddr addr,
                                                 uint16_t idxmap,
                                                 unsigned bits)
{
}
static inline void
tlb_flush_page_bits_by_mmuidx_all_cpus_synced(CPUState *cpu, vaddr addr,
                                              uint16_t idxmap, unsigned bits)
{
}
static inline void tlb_flush_range_by_mmuidx(CPUState *cpu, vaddr addr,
                                             vaddr len, uint16_t idxmap,
                                             unsigned bits)
{
}
static inline void tlb_flush_range_by_mmuidx_all_cpus_synced(CPUState *cpu,
                                                             vaddr addr,
                                                             vaddr len,
                                                             uint16_t idxmap,
                                                             unsigned bits)
{
}
#endif /* CPUTLB_H */
