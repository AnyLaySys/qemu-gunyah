
#ifndef CPUTLB_H
#define CPUTLB_H

#include "exec/cpu-common.h"
#include "exec/hwaddr.h"
#include "exec/memattrs.h"
#include "exec/vaddr.h"

#if defined(CONFIG_TCG) && !defined(CONFIG_USER_ONLY)
void tlb_protect_code(ram_addr_t ram_addr);
void tlb_unprotect_code(ram_addr_t ram_addr);
#endif

#ifndef CONFIG_USER_ONLY
void tlb_reset_dirty(CPUState *cpu, ram_addr_t start1, ram_addr_t length);
void tlb_reset_dirty_range_all(ram_addr_t start, ram_addr_t length);
#endif

void tlb_set_page_full(CPUState *cpu, int mmu_idx, vaddr addr,
                       CPUTLBEntryFull *full);

void tlb_set_page_with_attrs(CPUState *cpu, vaddr addr,
                             hwaddr paddr, MemTxAttrs attrs,
                             int prot, int mmu_idx, vaddr size);

void tlb_set_page(CPUState *cpu, vaddr addr,
                  hwaddr paddr, int prot,
                  int mmu_idx, vaddr size);

#if defined(CONFIG_TCG) && !defined(CONFIG_USER_ONLY)
void tlb_flush_page(CPUState *cpu, vaddr addr);

void tlb_flush_page_all_cpus_synced(CPUState *src, vaddr addr);

void tlb_flush(CPUState *cpu);

void tlb_flush_all_cpus_synced(CPUState *src_cpu);

void tlb_flush_page_by_mmuidx(CPUState *cpu, vaddr addr,
                              uint16_t idxmap);

void tlb_flush_page_by_mmuidx_all_cpus_synced(CPUState *cpu, vaddr addr,
                                              uint16_t idxmap);

void tlb_flush_by_mmuidx(CPUState *cpu, uint16_t idxmap);

void tlb_flush_by_mmuidx_all_cpus_synced(CPUState *cpu, uint16_t idxmap);

void tlb_flush_page_bits_by_mmuidx(CPUState *cpu, vaddr addr,
                                   uint16_t idxmap, unsigned bits);

void tlb_flush_page_bits_by_mmuidx_all_cpus_synced(CPUState *cpu, vaddr addr,
                                                   uint16_t idxmap,
                                                   unsigned bits);

void tlb_flush_range_by_mmuidx(CPUState *cpu, vaddr addr,
                               vaddr len, uint16_t idxmap,
                               unsigned bits);

void tlb_flush_range_by_mmuidx_all_cpus_synced(CPUState *cpu,
                                               vaddr addr,
                                               vaddr len,
                                               uint16_t idxmap,
                                               unsigned bits);
#else
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
#endif /* CONFIG_TCG && !CONFIG_USER_ONLY */
#endif /* CPUTLB_H */
