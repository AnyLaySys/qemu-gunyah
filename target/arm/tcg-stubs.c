
#include "qemu/osdep.h"
#include "cpu.h"
#include "internals.h"

void write_v7m_exception(CPUARMState *env, uint32_t new_exc)
{
    g_assert_not_reached();
}

void raise_exception_ra(CPUARMState *env, uint32_t excp, uint32_t syndrome,
                        uint32_t target_el, uintptr_t ra)
{
    g_assert_not_reached();
}
void assert_hflags_rebuild_correctly(CPUARMState *env)
{
}

void define_tlb_insn_regs(ARMCPU *cpu)
{
}

void arm_set_default_fp_behaviours(float_status *s)
{
}

void arm_set_ah_fp_behaviours(float_status *s)
{
}

uint32_t vfp_get_fpsr_from_host(CPUARMState *env)
{
    return 0;
}

void vfp_clear_float_status_exc_flags(CPUARMState *env)
{
}

void vfp_set_fpcr_to_host(CPUARMState *env, uint32_t val, uint32_t mask)
{
}
