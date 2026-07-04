
#include "qemu/osdep.h"
#include "cpu.h"
#include "internals.h"
#include "cpu-features.h"

uint32_t vfp_get_fpcr(CPUARMState *env)
{
    uint32_t fpcr = env->vfp.fpcr
        | (env->vfp.vec_len << 16)
        | (env->vfp.vec_stride << 20);

    fpcr |= env->v7m.ltpsize << 16;

    return fpcr;
}

uint32_t vfp_get_fpsr(CPUARMState *env)
{
    uint32_t fpsr = env->vfp.fpsr;
    uint32_t i;

    fpsr |= vfp_get_fpsr_from_host(env);

    i = env->vfp.qc[0] | env->vfp.qc[1] | env->vfp.qc[2] | env->vfp.qc[3];
    fpsr |= i ? FPSR_QC : 0;
    return fpsr;
}

uint32_t vfp_get_fpscr(CPUARMState *env)
{
    return (vfp_get_fpcr(env) & FPSCR_FPCR_MASK) |
        (vfp_get_fpsr(env) & FPSCR_FPSR_MASK);
}

void vfp_set_fpsr(CPUARMState *env, uint32_t val)
{
    ARMCPU *cpu = env_archcpu(env);

    if (arm_feature(env, ARM_FEATURE_NEON) ||
        cpu_isar_feature(aa32_mve, cpu)) {
        env->vfp.qc[0] = val & FPSR_QC;
        env->vfp.qc[1] = 0;
        env->vfp.qc[2] = 0;
        env->vfp.qc[3] = 0;
    }

    val &= FPSR_NZCV_MASK | FPSR_CEXC_MASK;
    env->vfp.fpsr = val;
    vfp_clear_float_status_exc_flags(env);
}

static void vfp_set_fpcr_masked(CPUARMState *env, uint32_t val, uint32_t mask)
{
    ARMCPU *cpu = env_archcpu(env);

    if (!cpu_isar_feature(any_fp16, cpu)) {
        val &= ~FPCR_FZ16;
    }
    if (!cpu_isar_feature(aa64_afp, cpu)) {
        val &= ~(FPCR_FIZ | FPCR_AH | FPCR_NEP);
    }

    if (!cpu_isar_feature(aa64_ebf16, cpu)) {
        val &= ~FPCR_EBF;
    }

    vfp_set_fpcr_to_host(env, val, mask);

    if (mask & (FPCR_LEN_MASK | FPCR_STRIDE_MASK)) {
        if (!arm_feature(env, ARM_FEATURE_M)) {
            env->vfp.vec_len = extract32(val, 16, 3);
            env->vfp.vec_stride = extract32(val, 20, 2);
        } else if (cpu_isar_feature(aa32_mve, cpu)) {
            env->v7m.ltpsize = extract32(val, FPCR_LTPSIZE_SHIFT,
                                         FPCR_LTPSIZE_LENGTH);
        }
    }

    val &= FPCR_AHP | FPCR_DN | FPCR_FZ | FPCR_RMODE_MASK | FPCR_FZ16 |
        FPCR_EBF | FPCR_FIZ | FPCR_AH | FPCR_NEP;
    env->vfp.fpcr &= ~mask;
    env->vfp.fpcr |= val;
}

void vfp_set_fpcr(CPUARMState *env, uint32_t val)
{
    vfp_set_fpcr_masked(env, val, MAKE_64BIT_MASK(0, 32));
}

void vfp_set_fpscr(CPUARMState *env, uint32_t val)
{
    vfp_set_fpcr_masked(env, val, FPSCR_FPCR_MASK);
    vfp_set_fpsr(env, val & FPSCR_FPSR_MASK);
}
