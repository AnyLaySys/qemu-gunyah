#ifndef CPU_ALL_H
#define CPU_ALL_H

#include "exec/page-protection.h"
#include "exec/cpu-common.h"
#include "exec/cpu-interrupt.h"
#include "exec/memory.h"
#include "exec/tswap.h"
#include "hw/core/cpu.h"

#if TARGET_BIG_ENDIAN
#define lduw_p(p) lduw_be_p(p)
#define ldsw_p(p) ldsw_be_p(p)
#define ldl_p(p) ldl_be_p(p)
#define ldq_p(p) ldq_be_p(p)
#define stw_p(p, v) stw_be_p(p, v)
#define stl_p(p, v) stl_be_p(p, v)
#define stq_p(p, v) stq_be_p(p, v)
#define ldn_p(p, sz) ldn_be_p(p, sz)
#define stn_p(p, sz, v) stn_be_p(p, sz, v)
#else
#define lduw_p(p) lduw_le_p(p)
#define ldsw_p(p) ldsw_le_p(p)
#define ldl_p(p) ldl_le_p(p)
#define ldq_p(p) ldq_le_p(p)
#define stw_p(p, v) stw_le_p(p, v)
#define stl_p(p, v) stl_le_p(p, v)
#define stq_p(p, v) stq_le_p(p, v)
#define ldn_p(p, sz) ldn_le_p(p, sz)
#define stn_p(p, sz, v) stn_le_p(p, sz, v)
#endif


#if !defined(CONFIG_USER_ONLY)

#include "exec/hwaddr.h"

#define SUFFIX
#define ARG1         as
#define ARG1_DECL    AddressSpace *as
#define TARGET_ENDIANNESS
#include "exec/memory_ldst.h.inc"

#define SUFFIX       _cached_slow
#define ARG1         cache
#define ARG1_DECL    MemoryRegionCache *cache
#define TARGET_ENDIANNESS
#include "exec/memory_ldst.h.inc"

static inline void stl_phys_notdirty(AddressSpace *as, hwaddr addr, uint32_t val)
{
    address_space_stl_notdirty(as, addr, val,
                               MEMTXATTRS_UNSPECIFIED, NULL);
}

#define SUFFIX
#define ARG1         as
#define ARG1_DECL    AddressSpace *as
#define TARGET_ENDIANNESS
#include "exec/memory_ldst_phys.h.inc"

#define ENDIANNESS
#include "exec/memory_ldst_cached.h.inc"

#define SUFFIX       _cached
#define ARG1         cache
#define ARG1_DECL    MemoryRegionCache *cache
#define TARGET_ENDIANNESS
#include "exec/memory_ldst_phys.h.inc"
#endif

#include "exec/cpu-defs.h"
#include "exec/target_page.h"

CPUArchState *cpu_copy(CPUArchState *env);

#include "cpu.h"

#ifdef CONFIG_USER_ONLY

static inline int cpu_mmu_index(CPUState *cs, bool ifetch);

#define TLB_INVALID_MASK    (1 << (TARGET_PAGE_BITS_MIN - 1))
#define TLB_MMIO            (1 << (TARGET_PAGE_BITS_MIN - 2))
#define TLB_WATCHPOINT      0

static inline int cpu_mmu_index(CPUState *cs, bool ifetch)
{
    return MMU_USER_IDX;
}
#else

#define TLB_INVALID_MASK    (1 << (TARGET_PAGE_BITS_MIN - 1))
#define TLB_NOTDIRTY        (1 << (TARGET_PAGE_BITS_MIN - 2))
#define TLB_MMIO            (1 << (TARGET_PAGE_BITS_MIN - 3))
#define TLB_DISCARD_WRITE   (1 << (TARGET_PAGE_BITS_MIN - 4))
#define TLB_FORCE_SLOW      (1 << (TARGET_PAGE_BITS_MIN - 5))

#define TLB_FLAGS_MASK \
    (TLB_INVALID_MASK | TLB_NOTDIRTY | TLB_MMIO \
    | TLB_FORCE_SLOW | TLB_DISCARD_WRITE)

#define TLB_BSWAP            (1 << 0)
#define TLB_WATCHPOINT       (1 << 1)
#define TLB_CHECK_ALIGNED    (1 << 2)

#define TLB_SLOW_FLAGS_MASK  (TLB_BSWAP | TLB_WATCHPOINT | TLB_CHECK_ALIGNED)

QEMU_BUILD_BUG_ON(TLB_FLAGS_MASK & TLB_SLOW_FLAGS_MASK);

#endif /* !CONFIG_USER_ONLY */

QEMU_BUILD_BUG_ON(offsetof(ArchCPU, parent_obj) != 0);
QEMU_BUILD_BUG_ON(offsetof(ArchCPU, env) != sizeof(CPUState));

#endif /* CPU_ALL_H */
