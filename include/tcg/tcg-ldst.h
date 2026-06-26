
#ifndef TCG_LDST_H
#define TCG_LDST_H

tcg_target_ulong helper_ldub_mmu(CPUArchState *env, uint64_t addr,
                                 MemOpIdx oi, uintptr_t retaddr);
tcg_target_ulong helper_lduw_mmu(CPUArchState *env, uint64_t addr,
                                 MemOpIdx oi, uintptr_t retaddr);
tcg_target_ulong helper_ldul_mmu(CPUArchState *env, uint64_t addr,
                                 MemOpIdx oi, uintptr_t retaddr);
uint64_t helper_ldq_mmu(CPUArchState *env, uint64_t addr,
                        MemOpIdx oi, uintptr_t retaddr);
Int128 helper_ld16_mmu(CPUArchState *env, uint64_t addr,
                       MemOpIdx oi, uintptr_t retaddr);

tcg_target_ulong helper_ldsb_mmu(CPUArchState *env, uint64_t addr,
                                 MemOpIdx oi, uintptr_t retaddr);
tcg_target_ulong helper_ldsw_mmu(CPUArchState *env, uint64_t addr,
                                 MemOpIdx oi, uintptr_t retaddr);
tcg_target_ulong helper_ldsl_mmu(CPUArchState *env, uint64_t addr,
                                 MemOpIdx oi, uintptr_t retaddr);

void helper_stb_mmu(CPUArchState *env, uint64_t addr, uint32_t val,
                    MemOpIdx oi, uintptr_t retaddr);
void helper_stw_mmu(CPUArchState *env, uint64_t addr, uint32_t val,
                    MemOpIdx oi, uintptr_t retaddr);
void helper_stl_mmu(CPUArchState *env, uint64_t addr, uint32_t val,
                    MemOpIdx oi, uintptr_t retaddr);
void helper_stq_mmu(CPUArchState *env, uint64_t addr, uint64_t val,
                    MemOpIdx oi, uintptr_t retaddr);
void helper_st16_mmu(CPUArchState *env, uint64_t addr, Int128 val,
                     MemOpIdx oi, uintptr_t retaddr);

#endif /* TCG_LDST_H */
