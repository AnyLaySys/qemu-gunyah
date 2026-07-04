
#ifndef CPU_LDST_H
#define CPU_LDST_H

#ifndef CONFIG_TCG
#error Can only include this header with TCG
#endif

#include "exec/memopidx.h"
#include "exec/vaddr.h"
#include "exec/abi_ptr.h"
#include "exec/mmu-access-type.h"
#include "qemu/int128.h"

#if defined(CONFIG_USER_ONLY)
#include "user/guest-host.h"
#endif /* CONFIG_USER_ONLY */

uint32_t cpu_ldub_data(CPUArchState *env, abi_ptr ptr);
int cpu_ldsb_data(CPUArchState *env, abi_ptr ptr);
uint32_t cpu_lduw_be_data(CPUArchState *env, abi_ptr ptr);
int cpu_ldsw_be_data(CPUArchState *env, abi_ptr ptr);
uint32_t cpu_ldl_be_data(CPUArchState *env, abi_ptr ptr);
uint64_t cpu_ldq_be_data(CPUArchState *env, abi_ptr ptr);
uint32_t cpu_lduw_le_data(CPUArchState *env, abi_ptr ptr);
int cpu_ldsw_le_data(CPUArchState *env, abi_ptr ptr);
uint32_t cpu_ldl_le_data(CPUArchState *env, abi_ptr ptr);
uint64_t cpu_ldq_le_data(CPUArchState *env, abi_ptr ptr);

uint32_t cpu_ldub_data_ra(CPUArchState *env, abi_ptr ptr, uintptr_t ra);
int cpu_ldsb_data_ra(CPUArchState *env, abi_ptr ptr, uintptr_t ra);
uint32_t cpu_lduw_be_data_ra(CPUArchState *env, abi_ptr ptr, uintptr_t ra);
int cpu_ldsw_be_data_ra(CPUArchState *env, abi_ptr ptr, uintptr_t ra);
uint32_t cpu_ldl_be_data_ra(CPUArchState *env, abi_ptr ptr, uintptr_t ra);
uint64_t cpu_ldq_be_data_ra(CPUArchState *env, abi_ptr ptr, uintptr_t ra);
uint32_t cpu_lduw_le_data_ra(CPUArchState *env, abi_ptr ptr, uintptr_t ra);
int cpu_ldsw_le_data_ra(CPUArchState *env, abi_ptr ptr, uintptr_t ra);
uint32_t cpu_ldl_le_data_ra(CPUArchState *env, abi_ptr ptr, uintptr_t ra);
uint64_t cpu_ldq_le_data_ra(CPUArchState *env, abi_ptr ptr, uintptr_t ra);

void cpu_stb_data(CPUArchState *env, abi_ptr ptr, uint32_t val);
void cpu_stw_be_data(CPUArchState *env, abi_ptr ptr, uint32_t val);
void cpu_stl_be_data(CPUArchState *env, abi_ptr ptr, uint32_t val);
void cpu_stq_be_data(CPUArchState *env, abi_ptr ptr, uint64_t val);
void cpu_stw_le_data(CPUArchState *env, abi_ptr ptr, uint32_t val);
void cpu_stl_le_data(CPUArchState *env, abi_ptr ptr, uint32_t val);
void cpu_stq_le_data(CPUArchState *env, abi_ptr ptr, uint64_t val);

void cpu_stb_data_ra(CPUArchState *env, abi_ptr ptr,
                     uint32_t val, uintptr_t ra);
void cpu_stw_be_data_ra(CPUArchState *env, abi_ptr ptr,
                        uint32_t val, uintptr_t ra);
void cpu_stl_be_data_ra(CPUArchState *env, abi_ptr ptr,
                        uint32_t val, uintptr_t ra);
void cpu_stq_be_data_ra(CPUArchState *env, abi_ptr ptr,
                        uint64_t val, uintptr_t ra);
void cpu_stw_le_data_ra(CPUArchState *env, abi_ptr ptr,
                        uint32_t val, uintptr_t ra);
void cpu_stl_le_data_ra(CPUArchState *env, abi_ptr ptr,
                        uint32_t val, uintptr_t ra);
void cpu_stq_le_data_ra(CPUArchState *env, abi_ptr ptr,
                        uint64_t val, uintptr_t ra);

uint32_t cpu_ldub_mmuidx_ra(CPUArchState *env, abi_ptr ptr,
                            int mmu_idx, uintptr_t ra);
int cpu_ldsb_mmuidx_ra(CPUArchState *env, abi_ptr ptr,
                       int mmu_idx, uintptr_t ra);
uint32_t cpu_lduw_be_mmuidx_ra(CPUArchState *env, abi_ptr ptr,
                               int mmu_idx, uintptr_t ra);
int cpu_ldsw_be_mmuidx_ra(CPUArchState *env, abi_ptr ptr,
                          int mmu_idx, uintptr_t ra);
uint32_t cpu_ldl_be_mmuidx_ra(CPUArchState *env, abi_ptr ptr,
                              int mmu_idx, uintptr_t ra);
uint64_t cpu_ldq_be_mmuidx_ra(CPUArchState *env, abi_ptr ptr,
                              int mmu_idx, uintptr_t ra);
uint32_t cpu_lduw_le_mmuidx_ra(CPUArchState *env, abi_ptr ptr,
                               int mmu_idx, uintptr_t ra);
int cpu_ldsw_le_mmuidx_ra(CPUArchState *env, abi_ptr ptr,
                          int mmu_idx, uintptr_t ra);
uint32_t cpu_ldl_le_mmuidx_ra(CPUArchState *env, abi_ptr ptr,
                              int mmu_idx, uintptr_t ra);
uint64_t cpu_ldq_le_mmuidx_ra(CPUArchState *env, abi_ptr ptr,
                              int mmu_idx, uintptr_t ra);

void cpu_stb_mmuidx_ra(CPUArchState *env, abi_ptr ptr, uint32_t val,
                       int mmu_idx, uintptr_t ra);
void cpu_stw_be_mmuidx_ra(CPUArchState *env, abi_ptr ptr, uint32_t val,
                          int mmu_idx, uintptr_t ra);
void cpu_stl_be_mmuidx_ra(CPUArchState *env, abi_ptr ptr, uint32_t val,
                          int mmu_idx, uintptr_t ra);
void cpu_stq_be_mmuidx_ra(CPUArchState *env, abi_ptr ptr, uint64_t val,
                          int mmu_idx, uintptr_t ra);
void cpu_stw_le_mmuidx_ra(CPUArchState *env, abi_ptr ptr, uint32_t val,
                          int mmu_idx, uintptr_t ra);
void cpu_stl_le_mmuidx_ra(CPUArchState *env, abi_ptr ptr, uint32_t val,
                          int mmu_idx, uintptr_t ra);
void cpu_stq_le_mmuidx_ra(CPUArchState *env, abi_ptr ptr, uint64_t val,
                          int mmu_idx, uintptr_t ra);

uint8_t cpu_ldb_mmu(CPUArchState *env, abi_ptr ptr, MemOpIdx oi, uintptr_t ra);
uint16_t cpu_ldw_mmu(CPUArchState *env, abi_ptr ptr, MemOpIdx oi, uintptr_t ra);
uint32_t cpu_ldl_mmu(CPUArchState *env, abi_ptr ptr, MemOpIdx oi, uintptr_t ra);
uint64_t cpu_ldq_mmu(CPUArchState *env, abi_ptr ptr, MemOpIdx oi, uintptr_t ra);
Int128 cpu_ld16_mmu(CPUArchState *env, abi_ptr addr, MemOpIdx oi, uintptr_t ra);

void cpu_stb_mmu(CPUArchState *env, abi_ptr ptr, uint8_t val,
                 MemOpIdx oi, uintptr_t ra);
void cpu_stw_mmu(CPUArchState *env, abi_ptr ptr, uint16_t val,
                 MemOpIdx oi, uintptr_t ra);
void cpu_stl_mmu(CPUArchState *env, abi_ptr ptr, uint32_t val,
                 MemOpIdx oi, uintptr_t ra);
void cpu_stq_mmu(CPUArchState *env, abi_ptr ptr, uint64_t val,
                 MemOpIdx oi, uintptr_t ra);
void cpu_st16_mmu(CPUArchState *env, abi_ptr addr, Int128 val,
                  MemOpIdx oi, uintptr_t ra);

uint32_t cpu_atomic_cmpxchgb_mmu(CPUArchState *env, abi_ptr addr,
                                 uint32_t cmpv, uint32_t newv,
                                 MemOpIdx oi, uintptr_t retaddr);
uint32_t cpu_atomic_cmpxchgw_le_mmu(CPUArchState *env, abi_ptr addr,
                                    uint32_t cmpv, uint32_t newv,
                                    MemOpIdx oi, uintptr_t retaddr);
uint32_t cpu_atomic_cmpxchgl_le_mmu(CPUArchState *env, abi_ptr addr,
                                    uint32_t cmpv, uint32_t newv,
                                    MemOpIdx oi, uintptr_t retaddr);
uint64_t cpu_atomic_cmpxchgq_le_mmu(CPUArchState *env, abi_ptr addr,
                                    uint64_t cmpv, uint64_t newv,
                                    MemOpIdx oi, uintptr_t retaddr);
uint32_t cpu_atomic_cmpxchgw_be_mmu(CPUArchState *env, abi_ptr addr,
                                    uint32_t cmpv, uint32_t newv,
                                    MemOpIdx oi, uintptr_t retaddr);
uint32_t cpu_atomic_cmpxchgl_be_mmu(CPUArchState *env, abi_ptr addr,
                                    uint32_t cmpv, uint32_t newv,
                                    MemOpIdx oi, uintptr_t retaddr);
uint64_t cpu_atomic_cmpxchgq_be_mmu(CPUArchState *env, abi_ptr addr,
                                    uint64_t cmpv, uint64_t newv,
                                    MemOpIdx oi, uintptr_t retaddr);

#define GEN_ATOMIC_HELPER(NAME, TYPE, SUFFIX)   \
TYPE cpu_atomic_ ## NAME ## SUFFIX ## _mmu      \
    (CPUArchState *env, abi_ptr addr, TYPE val, \
     MemOpIdx oi, uintptr_t retaddr);

#ifdef CONFIG_ATOMIC64
#define GEN_ATOMIC_HELPER_ALL(NAME)          \
    GEN_ATOMIC_HELPER(NAME, uint32_t, b)     \
    GEN_ATOMIC_HELPER(NAME, uint32_t, w_le)  \
    GEN_ATOMIC_HELPER(NAME, uint32_t, w_be)  \
    GEN_ATOMIC_HELPER(NAME, uint32_t, l_le)  \
    GEN_ATOMIC_HELPER(NAME, uint32_t, l_be)  \
    GEN_ATOMIC_HELPER(NAME, uint64_t, q_le)  \
    GEN_ATOMIC_HELPER(NAME, uint64_t, q_be)
#else
#define GEN_ATOMIC_HELPER_ALL(NAME)          \
    GEN_ATOMIC_HELPER(NAME, uint32_t, b)     \
    GEN_ATOMIC_HELPER(NAME, uint32_t, w_le)  \
    GEN_ATOMIC_HELPER(NAME, uint32_t, w_be)  \
    GEN_ATOMIC_HELPER(NAME, uint32_t, l_le)  \
    GEN_ATOMIC_HELPER(NAME, uint32_t, l_be)
#endif

GEN_ATOMIC_HELPER_ALL(fetch_add)
GEN_ATOMIC_HELPER_ALL(fetch_sub)
GEN_ATOMIC_HELPER_ALL(fetch_and)
GEN_ATOMIC_HELPER_ALL(fetch_or)
GEN_ATOMIC_HELPER_ALL(fetch_xor)
GEN_ATOMIC_HELPER_ALL(fetch_smin)
GEN_ATOMIC_HELPER_ALL(fetch_umin)
GEN_ATOMIC_HELPER_ALL(fetch_smax)
GEN_ATOMIC_HELPER_ALL(fetch_umax)

GEN_ATOMIC_HELPER_ALL(add_fetch)
GEN_ATOMIC_HELPER_ALL(sub_fetch)
GEN_ATOMIC_HELPER_ALL(and_fetch)
GEN_ATOMIC_HELPER_ALL(or_fetch)
GEN_ATOMIC_HELPER_ALL(xor_fetch)
GEN_ATOMIC_HELPER_ALL(smin_fetch)
GEN_ATOMIC_HELPER_ALL(umin_fetch)
GEN_ATOMIC_HELPER_ALL(smax_fetch)
GEN_ATOMIC_HELPER_ALL(umax_fetch)

GEN_ATOMIC_HELPER_ALL(xchg)

#undef GEN_ATOMIC_HELPER_ALL
#undef GEN_ATOMIC_HELPER

Int128 cpu_atomic_cmpxchgo_le_mmu(CPUArchState *env, abi_ptr addr,
                                  Int128 cmpv, Int128 newv,
                                  MemOpIdx oi, uintptr_t retaddr);
Int128 cpu_atomic_cmpxchgo_be_mmu(CPUArchState *env, abi_ptr addr,
                                  Int128 cmpv, Int128 newv,
                                  MemOpIdx oi, uintptr_t retaddr);

#if TARGET_BIG_ENDIAN
# define cpu_lduw_data        cpu_lduw_be_data
# define cpu_ldsw_data        cpu_ldsw_be_data
# define cpu_ldl_data         cpu_ldl_be_data
# define cpu_ldq_data         cpu_ldq_be_data
# define cpu_lduw_data_ra     cpu_lduw_be_data_ra
# define cpu_ldsw_data_ra     cpu_ldsw_be_data_ra
# define cpu_ldl_data_ra      cpu_ldl_be_data_ra
# define cpu_ldq_data_ra      cpu_ldq_be_data_ra
# define cpu_lduw_mmuidx_ra   cpu_lduw_be_mmuidx_ra
# define cpu_ldsw_mmuidx_ra   cpu_ldsw_be_mmuidx_ra
# define cpu_ldl_mmuidx_ra    cpu_ldl_be_mmuidx_ra
# define cpu_ldq_mmuidx_ra    cpu_ldq_be_mmuidx_ra
# define cpu_stw_data         cpu_stw_be_data
# define cpu_stl_data         cpu_stl_be_data
# define cpu_stq_data         cpu_stq_be_data
# define cpu_stw_data_ra      cpu_stw_be_data_ra
# define cpu_stl_data_ra      cpu_stl_be_data_ra
# define cpu_stq_data_ra      cpu_stq_be_data_ra
# define cpu_stw_mmuidx_ra    cpu_stw_be_mmuidx_ra
# define cpu_stl_mmuidx_ra    cpu_stl_be_mmuidx_ra
# define cpu_stq_mmuidx_ra    cpu_stq_be_mmuidx_ra
#else
# define cpu_lduw_data        cpu_lduw_le_data
# define cpu_ldsw_data        cpu_ldsw_le_data
# define cpu_ldl_data         cpu_ldl_le_data
# define cpu_ldq_data         cpu_ldq_le_data
# define cpu_lduw_data_ra     cpu_lduw_le_data_ra
# define cpu_ldsw_data_ra     cpu_ldsw_le_data_ra
# define cpu_ldl_data_ra      cpu_ldl_le_data_ra
# define cpu_ldq_data_ra      cpu_ldq_le_data_ra
# define cpu_lduw_mmuidx_ra   cpu_lduw_le_mmuidx_ra
# define cpu_ldsw_mmuidx_ra   cpu_ldsw_le_mmuidx_ra
# define cpu_ldl_mmuidx_ra    cpu_ldl_le_mmuidx_ra
# define cpu_ldq_mmuidx_ra    cpu_ldq_le_mmuidx_ra
# define cpu_stw_data         cpu_stw_le_data
# define cpu_stl_data         cpu_stl_le_data
# define cpu_stq_data         cpu_stq_le_data
# define cpu_stw_data_ra      cpu_stw_le_data_ra
# define cpu_stl_data_ra      cpu_stl_le_data_ra
# define cpu_stq_data_ra      cpu_stq_le_data_ra
# define cpu_stw_mmuidx_ra    cpu_stw_le_mmuidx_ra
# define cpu_stl_mmuidx_ra    cpu_stl_le_mmuidx_ra
# define cpu_stq_mmuidx_ra    cpu_stq_le_mmuidx_ra
#endif

uint8_t cpu_ldb_code_mmu(CPUArchState *env, abi_ptr addr,
                         MemOpIdx oi, uintptr_t ra);
uint16_t cpu_ldw_code_mmu(CPUArchState *env, abi_ptr addr,
                          MemOpIdx oi, uintptr_t ra);
uint32_t cpu_ldl_code_mmu(CPUArchState *env, abi_ptr addr,
                          MemOpIdx oi, uintptr_t ra);
uint64_t cpu_ldq_code_mmu(CPUArchState *env, abi_ptr addr,
                          MemOpIdx oi, uintptr_t ra);

uint32_t cpu_ldub_code(CPUArchState *env, abi_ptr addr);
uint32_t cpu_lduw_code(CPUArchState *env, abi_ptr addr);
uint32_t cpu_ldl_code(CPUArchState *env, abi_ptr addr);
uint64_t cpu_ldq_code(CPUArchState *env, abi_ptr addr);

#ifdef CONFIG_USER_ONLY
static inline void *tlb_vaddr_to_host(CPUArchState *env, abi_ptr addr,
                                      MMUAccessType access_type, int mmu_idx)
{
    return g2h(env_cpu(env), addr);
}
#else
void *tlb_vaddr_to_host(CPUArchState *env, vaddr addr,
                        MMUAccessType access_type, int mmu_idx);
#endif

#ifdef CONFIG_USER_ONLY
extern __thread uintptr_t helper_retaddr;

static inline void set_helper_retaddr(uintptr_t ra)
{
    helper_retaddr = ra;
    signal_barrier();
}

static inline void clear_helper_retaddr(void)
{
    signal_barrier();
    helper_retaddr = 0;
}
#else
#define set_helper_retaddr(ra)   do { } while (0)
#define clear_helper_retaddr()   do { } while (0)
#endif

#endif /* CPU_LDST_H */
