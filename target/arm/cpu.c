
#include "qemu/osdep.h"
#include "qemu/qemu-print.h"
#include "qemu/timer.h"
#include "qemu/log.h"
#include "exec/page-vary.h"
#include "target/arm/idau.h"
#include "qemu/module.h"
#include "qapi/error.h"
#include "cpu.h"
#include "internals.h"
#include "cpu-features.h"
#include "exec/exec-all.h"
#include "hw/qdev-properties.h"
#if !defined(CONFIG_USER_ONLY)
#include "hw/loader.h"
#include "hw/boards.h"
#endif /* !CONFIG_USER_ONLY */
#include "system/hw_accel.h"
#include "system/gunyah.h"
#include "disas/capstone.h"
#include "fpu/softfloat.h"
#include "cpregs.h"
#include "target/arm/cpu-qom.h"
#include "target/arm/gtimer.h"

static void arm_cpu_set_pc(CPUState *cs, vaddr value)
{
    ARMCPU *cpu = ARM_CPU(cs);
    CPUARMState *env = &cpu->env;

    if (is_a64(env)) {
        env->pc = value;
        env->thumb = false;
    } else {
        env->regs[15] = value & ~1;
        env->thumb = value & 1;
    }
}

static vaddr arm_cpu_get_pc(CPUState *cs)
{
    ARMCPU *cpu = ARM_CPU(cs);
    CPUARMState *env = &cpu->env;

    if (is_a64(env)) {
        return env->pc;
    } else {
        return env->regs[15];
    }
}


#ifndef CONFIG_USER_ONLY
static bool arm_cpu_has_work(CPUState *cs)
{
    ARMCPU *cpu = ARM_CPU(cs);

    return (cpu->power_state != PSCI_OFF)
        && cs->interrupt_request &
        (CPU_INTERRUPT_FIQ | CPU_INTERRUPT_HARD
         | CPU_INTERRUPT_NMI | CPU_INTERRUPT_VINMI | CPU_INTERRUPT_VFNMI
         | CPU_INTERRUPT_VFIQ | CPU_INTERRUPT_VIRQ | CPU_INTERRUPT_VSERR
         | CPU_INTERRUPT_EXITTB);
}
#endif /* !CONFIG_USER_ONLY */

static int arm_cpu_mmu_index(CPUState *cs, bool ifetch)
{
    return arm_env_mmu_index(cpu_env(cs));
}

void arm_register_pre_el_change_hook(ARMCPU *cpu, ARMELChangeHookFn *hook,
                                 void *opaque)
{
    ARMELChangeHook *entry = g_new0(ARMELChangeHook, 1);

    entry->hook = hook;
    entry->opaque = opaque;

    QLIST_INSERT_HEAD(&cpu->pre_el_change_hooks, entry, node);
}

void arm_register_el_change_hook(ARMCPU *cpu, ARMELChangeHookFn *hook,
                                 void *opaque)
{
    ARMELChangeHook *entry = g_new0(ARMELChangeHook, 1);

    entry->hook = hook;
    entry->opaque = opaque;

    QLIST_INSERT_HEAD(&cpu->el_change_hooks, entry, node);
}

static void cp_reg_reset(gpointer key, gpointer value, gpointer opaque)
{
    ARMCPRegInfo *ri = value;
    ARMCPU *cpu = opaque;

    if (ri->type & (ARM_CP_SPECIAL_MASK | ARM_CP_ALIAS)) {
        return;
    }

    if (ri->resetfn) {
        ri->resetfn(&cpu->env, ri);
        return;
    }

    if (!ri->fieldoffset) {
        return;
    }

    if (cpreg_field_is_64bit(ri)) {
        CPREG_FIELD64(&cpu->env, ri) = ri->resetvalue;
    } else {
        CPREG_FIELD32(&cpu->env, ri) = ri->resetvalue;
    }
}

static void cp_reg_check_reset(gpointer key, gpointer value,  gpointer opaque)
{
    ARMCPRegInfo *ri = value;
    ARMCPU *cpu = opaque;
    uint64_t oldvalue, newvalue;

    if (ri->type & (ARM_CP_SPECIAL_MASK | ARM_CP_ALIAS | ARM_CP_NO_RAW)) {
        return;
    }

    oldvalue = read_raw_cp_reg(&cpu->env, ri);
    cp_reg_reset(key, value, opaque);
    newvalue = read_raw_cp_reg(&cpu->env, ri);
    assert(oldvalue == newvalue);
}

static void arm_cpu_reset_hold(Object *obj, ResetType type)
{
    CPUState *cs = CPU(obj);
    ARMCPU *cpu = ARM_CPU(cs);
    ARMCPUClass *acc = ARM_CPU_GET_CLASS(obj);
    CPUARMState *env = &cpu->env;

    if (acc->parent_phases.hold) {
        acc->parent_phases.hold(obj, type);
    }

    memset(env, 0, offsetof(CPUARMState, end_reset_fields));

    g_hash_table_foreach(cpu->cp_regs, cp_reg_reset, cpu);
    g_hash_table_foreach(cpu->cp_regs, cp_reg_check_reset, cpu);

    env->vfp.xregs[ARM_VFP_FPSID] = cpu->reset_fpsid;
    env->vfp.xregs[ARM_VFP_MVFR0] = cpu->isar.mvfr0;
    env->vfp.xregs[ARM_VFP_MVFR1] = cpu->isar.mvfr1;
    env->vfp.xregs[ARM_VFP_MVFR2] = cpu->isar.mvfr2;

    cpu->power_state = cs->start_powered_off ? PSCI_OFF : PSCI_ON;

    if (arm_feature(env, ARM_FEATURE_IWMMXT)) {
        env->iwmmxt.cregs[ARM_IWMMXT_wCID] = 0x69051000 | 'Q';
    }

    if (arm_feature(env, ARM_FEATURE_AARCH64)) {
        env->aarch64 = true;
#if defined(CONFIG_USER_ONLY)
        env->pstate = PSTATE_MODE_EL0t;
        env->cp15.sctlr_el[1] |= SCTLR_UCT | SCTLR_UCI | SCTLR_DZE;
        env->cp15.sctlr_el[1] |= (SCTLR_EnIA | SCTLR_EnIB |
                                  SCTLR_EnDA | SCTLR_EnDB);
        env->cp15.sctlr_el[1] |= SCTLR_BT0;
        if (cpu_isar_feature(aa64_tidcp1, cpu)) {
            env->cp15.sctlr_el[1] |= SCTLR_TIDCP;
        }
        env->cp15.cpacr_el1 = FIELD_DP64(env->cp15.cpacr_el1,
                                         CPACR_EL1, FPEN, 3);
        if (cpu_isar_feature(aa64_sve, cpu)) {
            env->cp15.cpacr_el1 = FIELD_DP64(env->cp15.cpacr_el1,
                                             CPACR_EL1, ZEN, 3);
            env->vfp.zcr_el[1] = cpu->sve_default_vq - 1;
        }
        if (cpu_isar_feature(aa64_sme, cpu)) {
            env->cp15.sctlr_el[1] |= SCTLR_EnTP2;
            env->cp15.cpacr_el1 = FIELD_DP64(env->cp15.cpacr_el1,
                                             CPACR_EL1, SMEN, 3);
            env->vfp.smcr_el[1] = cpu->sme_default_vq - 1;
            if (cpu_isar_feature(aa64_sme_fa64, cpu)) {
                env->vfp.smcr_el[1] = FIELD_DP64(env->vfp.smcr_el[1],
                                                 SMCR, FA64, 1);
            }
        }
        env->cp15.tcr_el[1] = 5 | (1ULL << 37);

        if (cpu_isar_feature(aa64_mte, cpu)) {
            env->cp15.sctlr_el[1] |= SCTLR_ATA0;
            env->cp15.gcr_el1 = 0x1ffff;
        }
        env->cp15.sctlr_el[1] |= SCTLR_TSCXT;
        env->cp15.mdscr_el1 |= 1 << 12;
        env->cp15.sctlr_el[1] |= SCTLR_MSCEN;
#else
        if (arm_feature(env, ARM_FEATURE_EL3)) {
            env->pstate = PSTATE_MODE_EL3h;
        } else if (arm_feature(env, ARM_FEATURE_EL2)) {
            env->pstate = PSTATE_MODE_EL2h;
        } else {
            env->pstate = PSTATE_MODE_EL1h;
        }

        env->cp15.rvbar = cpu->rvbar_prop;
        env->pc = env->cp15.rvbar;
#endif
    } else {
#if defined(CONFIG_USER_ONLY)
        env->cp15.cpacr_el1 = FIELD_DP64(env->cp15.cpacr_el1,
                                         CPACR, CP10, 3);
        env->cp15.cpacr_el1 = FIELD_DP64(env->cp15.cpacr_el1,
                                         CPACR, CP11, 3);
#endif
        if (arm_feature(env, ARM_FEATURE_V8)) {
            env->cp15.rvbar = cpu->rvbar_prop;
            env->regs[15] = cpu->rvbar_prop;
        }
    }

#if defined(CONFIG_USER_ONLY)
    env->uncached_cpsr = ARM_CPU_MODE_USR;
    env->vfp.xregs[ARM_VFP_FPEXC] = 1 << 30;
    if (arm_feature(env, ARM_FEATURE_IWMMXT)) {
        env->cp15.c15_cpar = 3;
    } else if (arm_feature(env, ARM_FEATURE_XSCALE)) {
        env->cp15.c15_cpar = 1;
    }
#else

    if (arm_feature(env, ARM_FEATURE_EL2) &&
        !arm_feature(env, ARM_FEATURE_EL3)) {
        env->uncached_cpsr = ARM_CPU_MODE_HYP;
    } else {
        env->uncached_cpsr = ARM_CPU_MODE_SVC;
    }
    env->daif = PSTATE_D | PSTATE_A | PSTATE_I | PSTATE_F;

    if (A32_BANKED_CURRENT_REG_GET(env, sctlr) & SCTLR_V) {
        env->regs[15] = 0xFFFF0000;
    }

    env->vfp.xregs[ARM_VFP_FPEXC] = 0;
#endif

    if (arm_feature(env, ARM_FEATURE_M)) {
#ifndef CONFIG_USER_ONLY
        uint32_t initial_msp; /* Loaded from 0x0 */
        uint32_t initial_pc; /* Loaded from 0x4 */
        uint8_t *rom;
        uint32_t vecbase;
#endif

        if (cpu_isar_feature(aa32_lob, cpu)) {
            env->v7m.ltpsize = 4;
            env->v7m.fpdscr[M_REG_NS] = 4 << FPCR_LTPSIZE_SHIFT;
            env->v7m.fpdscr[M_REG_S] = 4 << FPCR_LTPSIZE_SHIFT;
        }

        if (arm_feature(env, ARM_FEATURE_M_SECURITY)) {
            env->v7m.secure = true;
        } else {
            env->v7m.aircr = R_V7M_AIRCR_BFHFNMINS_MASK;
            env->v7m.nsacr = 0xcff;
        }

        env->v7m.ccr[M_REG_NS] = R_V7M_CCR_STKALIGN_MASK;
        env->v7m.ccr[M_REG_S] = R_V7M_CCR_STKALIGN_MASK;
        if (arm_feature(env, ARM_FEATURE_V8)) {
            env->v7m.ccr[M_REG_NS] |= R_V7M_CCR_NONBASETHRDENA_MASK;
            env->v7m.ccr[M_REG_S] |= R_V7M_CCR_NONBASETHRDENA_MASK;
        }
        if (!arm_feature(env, ARM_FEATURE_M_MAIN)) {
            env->v7m.ccr[M_REG_NS] |= R_V7M_CCR_UNALIGN_TRP_MASK;
            env->v7m.ccr[M_REG_S] |= R_V7M_CCR_UNALIGN_TRP_MASK;
        }

        if (cpu_isar_feature(aa32_vfp_simd, cpu)) {
            env->v7m.fpccr[M_REG_NS] = R_V7M_FPCCR_ASPEN_MASK;
            env->v7m.fpccr[M_REG_S] = R_V7M_FPCCR_ASPEN_MASK |
                R_V7M_FPCCR_LSPEN_MASK | R_V7M_FPCCR_S_MASK;
        }

#ifndef CONFIG_USER_ONLY
        env->regs[14] = 0xffffffff;

        env->v7m.vecbase[M_REG_S] = cpu->init_svtor & 0xffffff80;
        env->v7m.vecbase[M_REG_NS] = cpu->init_nsvtor & 0xffffff80;

        vecbase = env->v7m.vecbase[env->v7m.secure];
        rom = rom_ptr_for_as(cs->as, vecbase, 8);
        if (rom) {
            initial_msp = ldl_p(rom);
            initial_pc = ldl_p(rom + 4);
        } else {
            initial_msp = ldl_phys(cs->as, vecbase);
            initial_pc = ldl_phys(cs->as, vecbase + 4);
        }

        env->regs[13] = initial_msp & 0xFFFFFFFC;
        env->regs[15] = initial_pc & ~1;
        env->thumb = initial_pc & 1;
#else
        env->v7m.secure = false;
        env->v7m.nsacr = 0xcff;
        env->v7m.cpacr[M_REG_NS] = 0xf0ffff;
        env->v7m.fpccr[M_REG_S] &=
            ~(R_V7M_FPCCR_LSPEN_MASK | R_V7M_FPCCR_S_MASK);
        env->v7m.control[M_REG_S] |= R_V7M_CONTROL_FPCA_MASK;
#endif
    }

    arm_clear_exclusive(env);

    if (arm_feature(env, ARM_FEATURE_PMSA)) {
        if (cpu->pmsav7_dregion > 0) {
            if (arm_feature(env, ARM_FEATURE_V8)) {
                memset(env->pmsav8.rbar[M_REG_NS], 0,
                       sizeof(*env->pmsav8.rbar[M_REG_NS])
                       * cpu->pmsav7_dregion);
                memset(env->pmsav8.rlar[M_REG_NS], 0,
                       sizeof(*env->pmsav8.rlar[M_REG_NS])
                       * cpu->pmsav7_dregion);
                if (arm_feature(env, ARM_FEATURE_M_SECURITY)) {
                    memset(env->pmsav8.rbar[M_REG_S], 0,
                           sizeof(*env->pmsav8.rbar[M_REG_S])
                           * cpu->pmsav7_dregion);
                    memset(env->pmsav8.rlar[M_REG_S], 0,
                           sizeof(*env->pmsav8.rlar[M_REG_S])
                           * cpu->pmsav7_dregion);
                }
            } else if (arm_feature(env, ARM_FEATURE_V7)) {
                memset(env->pmsav7.drbar, 0,
                       sizeof(*env->pmsav7.drbar) * cpu->pmsav7_dregion);
                memset(env->pmsav7.drsr, 0,
                       sizeof(*env->pmsav7.drsr) * cpu->pmsav7_dregion);
                memset(env->pmsav7.dracr, 0,
                       sizeof(*env->pmsav7.dracr) * cpu->pmsav7_dregion);
            }
        }

        if (cpu->pmsav8r_hdregion > 0) {
            memset(env->pmsav8.hprbar, 0,
                   sizeof(*env->pmsav8.hprbar) * cpu->pmsav8r_hdregion);
            memset(env->pmsav8.hprlar, 0,
                   sizeof(*env->pmsav8.hprlar) * cpu->pmsav8r_hdregion);
        }

        env->pmsav7.rnr[M_REG_NS] = 0;
        env->pmsav7.rnr[M_REG_S] = 0;
        env->pmsav8.mair0[M_REG_NS] = 0;
        env->pmsav8.mair0[M_REG_S] = 0;
        env->pmsav8.mair1[M_REG_NS] = 0;
        env->pmsav8.mair1[M_REG_S] = 0;
    }

    if (arm_feature(env, ARM_FEATURE_M_SECURITY)) {
        if (cpu->sau_sregion > 0) {
            memset(env->sau.rbar, 0, sizeof(*env->sau.rbar) * cpu->sau_sregion);
            memset(env->sau.rlar, 0, sizeof(*env->sau.rlar) * cpu->sau_sregion);
        }
        env->sau.rnr = 0;
        env->sau.ctrl = 0;
    }

    set_flush_to_zero(1, &env->vfp.fp_status[FPST_STD]);
    set_flush_inputs_to_zero(1, &env->vfp.fp_status[FPST_STD]);
    set_default_nan_mode(1, &env->vfp.fp_status[FPST_STD]);
    set_default_nan_mode(1, &env->vfp.fp_status[FPST_STD_F16]);
    set_flush_to_zero(1, &env->vfp.fp_status[FPST_AH]);
    set_flush_inputs_to_zero(1, &env->vfp.fp_status[FPST_AH]);

#ifndef CONFIG_USER_ONLY
#endif

}

void arm_emulate_firmware_reset(CPUState *cpustate, int target_el)
{
    ARMCPU *cpu = ARM_CPU(cpustate);
    CPUARMState *env = &cpu->env;
    bool have_el3 = arm_feature(env, ARM_FEATURE_EL3);
    bool have_el2 = arm_feature(env, ARM_FEATURE_EL2);

    switch (target_el) {
    case 3:
        assert(have_el3);
        return;
    case 2:
        assert(have_el2);
        if (!have_el3) {
            return;
        }
        break;
    case 1:
        if (!have_el3 && !have_el2) {
            return;
        }
        break;
    default:
        g_assert_not_reached();
    }

    if (have_el3) {
        if (env->aarch64) {
            env->cp15.scr_el3 |= SCR_RW;
            if (cpu_isar_feature(aa64_pauth, cpu)) {
                env->cp15.scr_el3 |= SCR_API | SCR_APK;
            }
            if (cpu_isar_feature(aa64_mte, cpu)) {
                env->cp15.scr_el3 |= SCR_ATA;
            }
            if (cpu_isar_feature(aa64_sve, cpu)) {
                env->cp15.cptr_el[3] |= R_CPTR_EL3_EZ_MASK;
                env->vfp.zcr_el[3] = 0xf;
            }
            if (cpu_isar_feature(aa64_sme, cpu)) {
                env->cp15.cptr_el[3] |= R_CPTR_EL3_ESM_MASK;
                env->cp15.scr_el3 |= SCR_ENTP2;
                env->vfp.smcr_el[3] = 0xf;
            }
            if (cpu_isar_feature(aa64_hcx, cpu)) {
                env->cp15.scr_el3 |= SCR_HXEN;
            }
            if (cpu_isar_feature(aa64_fgt, cpu)) {
                env->cp15.scr_el3 |= SCR_FGTEN;
            }
        }

        if (target_el == 2) {
            env->cp15.scr_el3 |= SCR_HCE;
        }

        env->cp15.scr_el3 |= SCR_NS;
        env->cp15.nsacr |= 3 << 10;
    }

    if (have_el2 && target_el < 2) {
        if (env->aarch64) {
            env->cp15.hcr_el2 |= HCR_RW;
        }
    }

    if (env->aarch64) {
        env->pstate = aarch64_pstate_mode(target_el, true);
    } else {
        static const uint32_t mode_for_el[] = {
            0,
            ARM_CPU_MODE_SVC,
            ARM_CPU_MODE_HYP,
            ARM_CPU_MODE_SVC,
        };

        cpsr_write(env, mode_for_el[target_el], CPSR_M, CPSRWriteRaw);
    }
}



void arm_cpu_update_virq(ARMCPU *cpu)
{
    CPUARMState *env = &cpu->env;
    CPUState *cs = CPU(cpu);

    bool new_state = ((arm_hcr_el2_eff(env) & HCR_VI) &&
        !(arm_hcrx_el2_eff(env) & HCRX_VINMI)) ||
        (env->irq_line_state & CPU_INTERRUPT_VIRQ);

    if (new_state != ((cs->interrupt_request & CPU_INTERRUPT_VIRQ) != 0)) {
        if (new_state) {
            cpu_interrupt(cs, CPU_INTERRUPT_VIRQ);
        } else {
            cpu_reset_interrupt(cs, CPU_INTERRUPT_VIRQ);
        }
    }
}

void arm_cpu_update_vfiq(ARMCPU *cpu)
{
    CPUARMState *env = &cpu->env;
    CPUState *cs = CPU(cpu);

    bool new_state = ((arm_hcr_el2_eff(env) & HCR_VF) &&
        !(arm_hcrx_el2_eff(env) & HCRX_VFNMI)) ||
        (env->irq_line_state & CPU_INTERRUPT_VFIQ);

    if (new_state != ((cs->interrupt_request & CPU_INTERRUPT_VFIQ) != 0)) {
        if (new_state) {
            cpu_interrupt(cs, CPU_INTERRUPT_VFIQ);
        } else {
            cpu_reset_interrupt(cs, CPU_INTERRUPT_VFIQ);
        }
    }
}

void arm_cpu_update_vinmi(ARMCPU *cpu)
{
    CPUARMState *env = &cpu->env;
    CPUState *cs = CPU(cpu);

    bool new_state = ((arm_hcr_el2_eff(env) & HCR_VI) &&
                      (arm_hcrx_el2_eff(env) & HCRX_VINMI)) ||
        (env->irq_line_state & CPU_INTERRUPT_VINMI);

    if (new_state != ((cs->interrupt_request & CPU_INTERRUPT_VINMI) != 0)) {
        if (new_state) {
            cpu_interrupt(cs, CPU_INTERRUPT_VINMI);
        } else {
            cpu_reset_interrupt(cs, CPU_INTERRUPT_VINMI);
        }
    }
}

void arm_cpu_update_vfnmi(ARMCPU *cpu)
{
    CPUARMState *env = &cpu->env;
    CPUState *cs = CPU(cpu);

    bool new_state = (arm_hcr_el2_eff(env) & HCR_VF) &&
                      (arm_hcrx_el2_eff(env) & HCRX_VFNMI);

    if (new_state != ((cs->interrupt_request & CPU_INTERRUPT_VFNMI) != 0)) {
        if (new_state) {
            cpu_interrupt(cs, CPU_INTERRUPT_VFNMI);
        } else {
            cpu_reset_interrupt(cs, CPU_INTERRUPT_VFNMI);
        }
    }
}

void arm_cpu_update_vserr(ARMCPU *cpu)
{
    CPUARMState *env = &cpu->env;
    CPUState *cs = CPU(cpu);

    bool new_state = env->cp15.hcr_el2 & HCR_VSE;

    if (new_state != ((cs->interrupt_request & CPU_INTERRUPT_VSERR) != 0)) {
        if (new_state) {
            cpu_interrupt(cs, CPU_INTERRUPT_VSERR);
        } else {
            cpu_reset_interrupt(cs, CPU_INTERRUPT_VSERR);
        }
    }
}

#ifndef CONFIG_USER_ONLY
static void arm_cpu_set_irq(void *opaque, int irq, int level)
{
    ARMCPU *cpu = opaque;
    CPUARMState *env = &cpu->env;
    CPUState *cs = CPU(cpu);
    static const int mask[] = {
        [ARM_CPU_IRQ] = CPU_INTERRUPT_HARD,
        [ARM_CPU_FIQ] = CPU_INTERRUPT_FIQ,
        [ARM_CPU_VIRQ] = CPU_INTERRUPT_VIRQ,
        [ARM_CPU_VFIQ] = CPU_INTERRUPT_VFIQ,
        [ARM_CPU_NMI] = CPU_INTERRUPT_NMI,
        [ARM_CPU_VINMI] = CPU_INTERRUPT_VINMI,
    };

    if (!arm_feature(env, ARM_FEATURE_EL2) &&
        (irq == ARM_CPU_VIRQ || irq == ARM_CPU_VFIQ)) {
        return;
    }

    if (level) {
        env->irq_line_state |= mask[irq];
    } else {
        env->irq_line_state &= ~mask[irq];
    }

    switch (irq) {
    case ARM_CPU_VIRQ:
        arm_cpu_update_virq(cpu);
        break;
    case ARM_CPU_VFIQ:
        arm_cpu_update_vfiq(cpu);
        break;
    case ARM_CPU_VINMI:
        arm_cpu_update_vinmi(cpu);
        break;
    case ARM_CPU_IRQ:
    case ARM_CPU_FIQ:
    case ARM_CPU_NMI:
        if (level) {
            cpu_interrupt(cs, mask[irq]);
        } else {
            cpu_reset_interrupt(cs, mask[irq]);
        }
        break;
    default:
        g_assert_not_reached();
    }
}

#endif

static bool arm_cpu_virtio_is_big_endian(CPUState *cs)
{
    ARMCPU *cpu = ARM_CPU(cs);
    CPUARMState *env = &cpu->env;

    cpu_synchronize_state(cs);
    return arm_cpu_data_is_big_endian(env);
}


static void arm_disas_set_info(CPUState *cpu, disassemble_info *info)
{
    ARMCPU *ac = ARM_CPU(cpu);
    CPUARMState *env = &ac->env;
    bool sctlr_b = arm_sctlr_b(env);

    if (is_a64(env)) {
        info->cap_arch = CS_ARCH_ARM64;
        info->cap_insn_unit = 4;
        info->cap_insn_split = 4;
    } else {
        int cap_mode;
        if (env->thumb) {
            info->cap_insn_unit = 2;
            info->cap_insn_split = 4;
            cap_mode = CS_MODE_THUMB;
        } else {
            info->cap_insn_unit = 4;
            info->cap_insn_split = 4;
            cap_mode = CS_MODE_ARM;
        }
        if (arm_feature(env, ARM_FEATURE_V8)) {
            cap_mode |= CS_MODE_V8;
        }
        if (arm_feature(env, ARM_FEATURE_M)) {
            cap_mode |= CS_MODE_MCLASS;
        }
        info->cap_arch = CS_ARCH_ARM;
        info->cap_mode = cap_mode;
    }

    info->endian = BFD_ENDIAN_LITTLE;
    if (bswap_code(sctlr_b)) {
        info->endian = TARGET_BIG_ENDIAN ? BFD_ENDIAN_LITTLE : BFD_ENDIAN_BIG;
    }
    info->flags &= ~INSN_ARM_BE32;
#ifndef CONFIG_USER_ONLY
    if (sctlr_b) {
        info->flags |= INSN_ARM_BE32;
    }
#endif
}

#ifdef TARGET_AARCH64

static void aarch64_cpu_dump_state(CPUState *cs, FILE *f, int flags)
{
    ARMCPU *cpu = ARM_CPU(cs);
    CPUARMState *env = &cpu->env;
    uint32_t psr = pstate_read(env);
    int i, j;
    int el = arm_current_el(env);
    uint64_t hcr = arm_hcr_el2_eff(env);
    const char *ns_status;
    bool sve;

    qemu_fprintf(f, " PC=%016" PRIx64 " ", env->pc);
    for (i = 0; i < 32; i++) {
        if (i == 31) {
            qemu_fprintf(f, " SP=%016" PRIx64 "\n", env->xregs[i]);
        } else {
            qemu_fprintf(f, "X%02d=%016" PRIx64 "%s", i, env->xregs[i],
                         (i + 2) % 3 ? " " : "\n");
        }
    }

    if (arm_feature(env, ARM_FEATURE_EL3) && el != 3) {
        ns_status = env->cp15.scr_el3 & SCR_NS ? "NS " : "S ";
    } else {
        ns_status = "";
    }
    qemu_fprintf(f, "PSTATE=%08x %c%c%c%c %sEL%d%c",
                 psr,
                 psr & PSTATE_N ? 'N' : '-',
                 psr & PSTATE_Z ? 'Z' : '-',
                 psr & PSTATE_C ? 'C' : '-',
                 psr & PSTATE_V ? 'V' : '-',
                 ns_status,
                 el,
                 psr & PSTATE_SP ? 'h' : 't');

    if (cpu_isar_feature(aa64_sme, cpu)) {
        qemu_fprintf(f, "  SVCR=%08" PRIx64 " %c%c",
                     env->svcr,
                     (FIELD_EX64(env->svcr, SVCR, ZA) ? 'Z' : '-'),
                     (FIELD_EX64(env->svcr, SVCR, SM) ? 'S' : '-'));
    }
    if (cpu_isar_feature(aa64_bti, cpu)) {
        qemu_fprintf(f, "  BTYPE=%d", (psr & PSTATE_BTYPE) >> 10);
    }
    qemu_fprintf(f, "%s%s%s",
                 (hcr & HCR_NV) ? " NV" : "",
                 (hcr & HCR_NV1) ? " NV1" : "",
                 (hcr & HCR_NV2) ? " NV2" : "");
    if (!(flags & CPU_DUMP_FPU)) {
        qemu_fprintf(f, "\n");
        return;
    }
    if (fp_exception_el(env, el) != 0) {
        qemu_fprintf(f, "    FPU disabled\n");
        return;
    }
    qemu_fprintf(f, "     FPCR=%08x FPSR=%08x\n",
                 vfp_get_fpcr(env), vfp_get_fpsr(env));

    if (cpu_isar_feature(aa64_sme, cpu) && FIELD_EX64(env->svcr, SVCR, SM)) {
        sve = sme_exception_el(env, el) == 0;
    } else if (cpu_isar_feature(aa64_sve, cpu)) {
        sve = sve_exception_el(env, el) == 0;
    } else {
        sve = false;
    }

    if (sve) {
        int zcr_len = sve_vqm1_for_el(env, el);

        for (i = 0; i <= FFR_PRED_NUM; i++) {
            bool eol;
            if (i == FFR_PRED_NUM) {
                qemu_fprintf(f, "FFR=");
                eol = true;
            } else {
                qemu_fprintf(f, "P%02d=", i);
                switch (zcr_len) {
                case 0:
                    eol = i % 8 == 7;
                    break;
                case 1:
                    eol = i % 6 == 5;
                    break;
                case 2:
                case 3:
                    eol = i % 3 == 2;
                    break;
                default:
                    eol = true;
                    break;
                }
            }
            for (j = zcr_len / 4; j >= 0; j--) {
                int digits;
                if (j * 4 + 4 <= zcr_len + 1) {
                    digits = 16;
                } else {
                    digits = (zcr_len % 4 + 1) * 4;
                }
                qemu_fprintf(f, "%0*" PRIx64 "%s", digits,
                             env->vfp.pregs[i].p[j],
                             j ? ":" : eol ? "\n" : " ");
            }
        }

        if (zcr_len == 0) {
            for (i = 0; i < 32; i++) {
                qemu_fprintf(f, "Z%02d=%016" PRIx64 ":%016" PRIx64 "%s",
                             i, env->vfp.zregs[i].d[1],
                             env->vfp.zregs[i].d[0], i & 1 ? "\n" : " ");
            }
        } else {
            for (i = 0; i < 32; i++) {
                qemu_fprintf(f, "Z%02d=", i);
                for (j = zcr_len; j >= 0; j--) {
                    qemu_fprintf(f, "%016" PRIx64 ":%016" PRIx64 "%s",
                                 env->vfp.zregs[i].d[j * 2 + 1],
                                 env->vfp.zregs[i].d[j * 2 + 0],
                                 j ? ":" : "\n");
                }
            }
        }
    } else {
        for (i = 0; i < 32; i++) {
            uint64_t *q = aa64_vfp_qreg(env, i);
            qemu_fprintf(f, "Q%02d=%016" PRIx64 ":%016" PRIx64 "%s",
                         i, q[1], q[0], (i & 1 ? "\n" : " "));
        }
    }

    if (cpu_isar_feature(aa64_sme, cpu) &&
        FIELD_EX64(env->svcr, SVCR, ZA) &&
        sme_exception_el(env, el) == 0) {
        int zcr_len = sve_vqm1_for_el_sm(env, el, true);
        int svl = (zcr_len + 1) * 16;
        int svl_lg10 = svl < 100 ? 2 : 3;

        for (i = 0; i < svl; i++) {
            qemu_fprintf(f, "ZA[%0*d]=", svl_lg10, i);
            for (j = zcr_len; j >= 0; --j) {
                qemu_fprintf(f, "%016" PRIx64 ":%016" PRIx64 "%c",
                             env->zarray[i].d[2 * j + 1],
                             env->zarray[i].d[2 * j],
                             j ? ':' : '\n');
            }
        }
    }
}

#else

static inline void aarch64_cpu_dump_state(CPUState *cs, FILE *f, int flags)
{
    g_assert_not_reached();
}

#endif

static void arm_cpu_dump_state(CPUState *cs, FILE *f, int flags)
{
    ARMCPU *cpu = ARM_CPU(cs);
    CPUARMState *env = &cpu->env;
    int i;

    if (is_a64(env)) {
        aarch64_cpu_dump_state(cs, f, flags);
        return;
    }

    for (i = 0; i < 16; i++) {
        qemu_fprintf(f, "R%02d=%08x", i, env->regs[i]);
        if ((i % 4) == 3) {
            qemu_fprintf(f, "\n");
        } else {
            qemu_fprintf(f, " ");
        }
    }

    if (arm_feature(env, ARM_FEATURE_M)) {
        uint32_t xpsr = xpsr_read(env);
        const char *mode;
        const char *ns_status = "";

        if (arm_feature(env, ARM_FEATURE_M_SECURITY)) {
            ns_status = env->v7m.secure ? "S " : "NS ";
        }

        if (xpsr & XPSR_EXCP) {
            mode = "handler";
        } else {
            if (env->v7m.control[env->v7m.secure] & R_V7M_CONTROL_NPRIV_MASK) {
                mode = "unpriv-thread";
            } else {
                mode = "priv-thread";
            }
        }

        qemu_fprintf(f, "XPSR=%08x %c%c%c%c %c %s%s\n",
                     xpsr,
                     xpsr & XPSR_N ? 'N' : '-',
                     xpsr & XPSR_Z ? 'Z' : '-',
                     xpsr & XPSR_C ? 'C' : '-',
                     xpsr & XPSR_V ? 'V' : '-',
                     xpsr & XPSR_T ? 'T' : 'A',
                     ns_status,
                     mode);
    } else {
        uint32_t psr = cpsr_read(env);
        const char *ns_status = "";

        if (arm_feature(env, ARM_FEATURE_EL3) &&
            (psr & CPSR_M) != ARM_CPU_MODE_MON) {
            ns_status = env->cp15.scr_el3 & SCR_NS ? "NS " : "S ";
        }

        qemu_fprintf(f, "PSR=%08x %c%c%c%c %c %s%s%d\n",
                     psr,
                     psr & CPSR_N ? 'N' : '-',
                     psr & CPSR_Z ? 'Z' : '-',
                     psr & CPSR_C ? 'C' : '-',
                     psr & CPSR_V ? 'V' : '-',
                     psr & CPSR_T ? 'T' : 'A',
                     ns_status,
                     aarch32_mode_name(psr), (psr & 0x10) ? 32 : 26);
    }

    if (flags & CPU_DUMP_FPU) {
        int numvfpregs = 0;
        if (cpu_isar_feature(aa32_simd_r32, cpu)) {
            numvfpregs = 32;
        } else if (cpu_isar_feature(aa32_vfp_simd, cpu)) {
            numvfpregs = 16;
        }
        for (i = 0; i < numvfpregs; i++) {
            uint64_t v = *aa32_vfp_dreg(env, i);
            qemu_fprintf(f, "s%02d=%08x s%02d=%08x d%02d=%016" PRIx64 "\n",
                         i * 2, (uint32_t)v,
                         i * 2 + 1, (uint32_t)(v >> 32),
                         i, v);
        }
        qemu_fprintf(f, "FPSCR: %08x\n", vfp_get_fpscr(env));
        if (cpu_isar_feature(aa32_mve, cpu)) {
            qemu_fprintf(f, "VPR: %08x\n", env->v7m.vpr);
        }
    }
}

uint64_t arm_build_mp_affinity(int idx, uint8_t clustersz)
{
    uint32_t Aff1 = idx / clustersz;
    uint32_t Aff0 = idx % clustersz;
    return (Aff1 << ARM_AFF1_SHIFT) | Aff0;
}

uint64_t arm_cpu_mp_affinity(ARMCPU *cpu)
{
    return cpu->mp_affinity;
}

static void arm_cpu_initfn(Object *obj)
{
    ARMCPU *cpu = ARM_CPU(obj);

    cpu->cp_regs = g_hash_table_new_full(g_direct_hash, g_direct_equal,
                                         NULL, g_free);

    QLIST_INIT(&cpu->pre_el_change_hooks);
    QLIST_INIT(&cpu->el_change_hooks);

#ifdef CONFIG_USER_ONLY
# ifdef TARGET_AARCH64
    cpu->sve_default_vq = 4;
    cpu->sme_default_vq = 2;
# endif
#else
    qdev_init_gpio_in(DEVICE(cpu), arm_cpu_set_irq, 6);

    qdev_init_gpio_out(DEVICE(cpu), cpu->gt_timer_outputs,
                       ARRAY_SIZE(cpu->gt_timer_outputs));

    qdev_init_gpio_out_named(DEVICE(cpu), &cpu->gicv3_maintenance_interrupt,
                             "gicv3-maintenance-interrupt", 1);
    qdev_init_gpio_out_named(DEVICE(cpu), &cpu->pmu_interrupt,
                             "pmu-interrupt", 1);
#endif

    cpu->dtb_compatible = "qemu,unknown";
    cpu->psci_version = QEMU_PSCI_VERSION_0_1; /* By default assume PSCI v0.1 */
    

    if (gunyah_enabled()) {
        cpu->psci_version = QEMU_PSCI_VERSION_1_1;
    }
}

static const Property arm_cpu_gt_cntfrq_property =
            DEFINE_PROP_UINT64("cntfrq", ARMCPU, gt_cntfrq_hz, 0);

static const Property arm_cpu_reset_cbar_property =
            DEFINE_PROP_UINT64("reset-cbar", ARMCPU, reset_cbar, 0);

static const Property arm_cpu_reset_hivecs_property =
            DEFINE_PROP_BOOL("reset-hivecs", ARMCPU, reset_hivecs, false);

#ifndef CONFIG_USER_ONLY
static const Property arm_cpu_has_el2_property =
            DEFINE_PROP_BOOL("has_el2", ARMCPU, has_el2, true);

static const Property arm_cpu_has_el3_property =
            DEFINE_PROP_BOOL("has_el3", ARMCPU, has_el3, true);
#endif

static const Property arm_cpu_cfgend_property =
            DEFINE_PROP_BOOL("cfgend", ARMCPU, cfgend, false);

static const Property arm_cpu_has_neon_property =
            DEFINE_PROP_BOOL("neon", ARMCPU, has_neon, true);

static const Property arm_cpu_has_dsp_property =
            DEFINE_PROP_BOOL("dsp", ARMCPU, has_dsp, true);

static const Property arm_cpu_has_mpu_property =
            DEFINE_PROP_BOOL("has-mpu", ARMCPU, has_mpu, true);

static const Property arm_cpu_pmsav7_dregion_property =
            DEFINE_PROP_UNSIGNED_NODEFAULT("pmsav7-dregion", ARMCPU,
                                           pmsav7_dregion,
                                           qdev_prop_uint32, uint32_t);

static bool arm_get_pmu(Object *obj, Error **errp)
{
    ARMCPU *cpu = ARM_CPU(obj);

    return cpu->has_pmu;
}

static void arm_set_pmu(Object *obj, bool value, Error **errp)
{
    ARMCPU *cpu = ARM_CPU(obj);

    if (value) {
        if (!false) {
            error_setg(errp, "'pmu' feature not supported by KVM on this host");
            return;
        }
        set_feature(&cpu->env, ARM_FEATURE_PMU);
    } else {
        unset_feature(&cpu->env, ARM_FEATURE_PMU);
    }
    cpu->has_pmu = value;
}

unsigned int gt_cntfrq_period_ns(ARMCPU *cpu)
{
    return NANOSECONDS_PER_SECOND > cpu->gt_cntfrq_hz ?
      NANOSECONDS_PER_SECOND / cpu->gt_cntfrq_hz : 1;
}

static void arm_cpu_propagate_feature_implications(ARMCPU *cpu)
{
    CPUARMState *env = &cpu->env;
    if (arm_feature(env, ARM_FEATURE_M)) {
        set_feature(env, ARM_FEATURE_PMSA);
    }

    if (arm_feature(env, ARM_FEATURE_V8)) {
        if (arm_feature(env, ARM_FEATURE_M)) {
            set_feature(env, ARM_FEATURE_V7);
        } else {
            set_feature(env, ARM_FEATURE_V7VE);
        }
    }

    if (arm_feature(env, ARM_FEATURE_V7VE)) {
        set_feature(env, ARM_FEATURE_LPAE);
        set_feature(env, ARM_FEATURE_V7);
    }
    if (arm_feature(env, ARM_FEATURE_V7)) {
        set_feature(env, ARM_FEATURE_VAPA);
        set_feature(env, ARM_FEATURE_THUMB2);
        set_feature(env, ARM_FEATURE_MPIDR);
        if (!arm_feature(env, ARM_FEATURE_M)) {
            set_feature(env, ARM_FEATURE_V6K);
        } else {
            set_feature(env, ARM_FEATURE_V6);
        }

        set_feature(env, ARM_FEATURE_VBAR);
    }
    if (arm_feature(env, ARM_FEATURE_V6K)) {
        set_feature(env, ARM_FEATURE_V6);
        set_feature(env, ARM_FEATURE_MVFR);
    }
    if (arm_feature(env, ARM_FEATURE_V6)) {
        set_feature(env, ARM_FEATURE_V5);
        if (!arm_feature(env, ARM_FEATURE_M)) {
            set_feature(env, ARM_FEATURE_AUXCR);
        }
    }
    if (arm_feature(env, ARM_FEATURE_V5)) {
        set_feature(env, ARM_FEATURE_V4T);
    }
    if (arm_feature(env, ARM_FEATURE_LPAE)) {
        set_feature(env, ARM_FEATURE_V7MP);
    }
    if (arm_feature(env, ARM_FEATURE_CBAR_RO)) {
        set_feature(env, ARM_FEATURE_CBAR);
    }
    if (arm_feature(env, ARM_FEATURE_THUMB2) &&
        !arm_feature(env, ARM_FEATURE_M)) {
        set_feature(env, ARM_FEATURE_THUMB_DSP);
    }
}

void arm_cpu_post_init(Object *obj)
{
    ARMCPU *cpu = ARM_CPU(obj);

    arm_cpu_propagate_feature_implications(cpu);

    if (arm_feature(&cpu->env, ARM_FEATURE_CBAR) ||
        arm_feature(&cpu->env, ARM_FEATURE_CBAR_RO)) {
        qdev_property_add_static(DEVICE(obj), &arm_cpu_reset_cbar_property);
    }

    if (!arm_feature(&cpu->env, ARM_FEATURE_M)) {
        qdev_property_add_static(DEVICE(obj), &arm_cpu_reset_hivecs_property);
    }

    if (arm_feature(&cpu->env, ARM_FEATURE_V8)) {
        object_property_add_uint64_ptr(obj, "rvbar",
                                       &cpu->rvbar_prop,
                                       OBJ_PROP_FLAG_READWRITE);
    }

#ifndef CONFIG_USER_ONLY
    if (arm_feature(&cpu->env, ARM_FEATURE_EL3)) {
        qdev_property_add_static(DEVICE(obj), &arm_cpu_has_el3_property);

        object_property_add_link(obj, "secure-memory",
                                 TYPE_MEMORY_REGION,
                                 (Object **)&cpu->secure_memory,
                                 qdev_prop_allow_set_link_before_realize,
                                 OBJ_PROP_LINK_STRONG);
    }

    if (arm_feature(&cpu->env, ARM_FEATURE_EL2)) {
        qdev_property_add_static(DEVICE(obj), &arm_cpu_has_el2_property);
    }
#endif

    if (arm_feature(&cpu->env, ARM_FEATURE_PMU)) {
        cpu->has_pmu = true;
        object_property_add_bool(obj, "pmu", arm_get_pmu, arm_set_pmu);
    }

    if (arm_feature(&cpu->env, ARM_FEATURE_AARCH64)) {
        if (cpu_isar_feature(aa64_fp_simd, cpu)) {
            cpu->has_vfp = true;
            cpu->has_vfp_d32 = true;
        }
    } else if (cpu_isar_feature(aa32_vfp, cpu)) {
        cpu->has_vfp = true;
        if (cpu_isar_feature(aa32_simd_r32, cpu)) {
            cpu->has_vfp_d32 = true;
        }
    }

    if (arm_feature(&cpu->env, ARM_FEATURE_NEON)) {
        cpu->has_neon = true;
        if (!false) {
            qdev_property_add_static(DEVICE(obj), &arm_cpu_has_neon_property);
        }
    }

    if (arm_feature(&cpu->env, ARM_FEATURE_M) &&
        arm_feature(&cpu->env, ARM_FEATURE_THUMB_DSP)) {
        qdev_property_add_static(DEVICE(obj), &arm_cpu_has_dsp_property);
    }

    if (arm_feature(&cpu->env, ARM_FEATURE_PMSA)) {
        qdev_property_add_static(DEVICE(obj), &arm_cpu_has_mpu_property);
        if (arm_feature(&cpu->env, ARM_FEATURE_V7)) {
            qdev_property_add_static(DEVICE(obj),
                                     &arm_cpu_pmsav7_dregion_property);
        }
    }

    if (arm_feature(&cpu->env, ARM_FEATURE_M_SECURITY)) {
        object_property_add_link(obj, "idau", TYPE_IDAU_INTERFACE, &cpu->idau,
                                 qdev_prop_allow_set_link_before_realize,
                                 OBJ_PROP_LINK_STRONG);
        object_property_add_uint32_ptr(obj, "init-svtor",
                                       &cpu->init_svtor,
                                       OBJ_PROP_FLAG_READWRITE);
    }
    if (arm_feature(&cpu->env, ARM_FEATURE_M)) {
        object_property_add_uint32_ptr(obj, "init-nsvtor",
                                       &cpu->init_nsvtor,
                                       OBJ_PROP_FLAG_READWRITE);
    }

    object_property_add_uint32_ptr(obj, "psci-conduit",
                                   &cpu->psci_conduit,
                                   OBJ_PROP_FLAG_READWRITE);

    qdev_property_add_static(DEVICE(obj), &arm_cpu_cfgend_property);

    if (arm_feature(&cpu->env, ARM_FEATURE_GENERIC_TIMER)) {
        qdev_property_add_static(DEVICE(cpu), &arm_cpu_gt_cntfrq_property);
    }

#ifndef CONFIG_USER_ONLY
    if (arm_feature(&cpu->env, ARM_FEATURE_AARCH64) &&
        cpu_isar_feature(aa64_mte, cpu)) {
        object_property_add_link(obj, "tag-memory",
                                 TYPE_MEMORY_REGION,
                                 (Object **)&cpu->tag_memory,
                                 qdev_prop_allow_set_link_before_realize,
                                 OBJ_PROP_LINK_STRONG);

        if (arm_feature(&cpu->env, ARM_FEATURE_EL3)) {
            object_property_add_link(obj, "secure-tag-memory",
                                     TYPE_MEMORY_REGION,
                                     (Object **)&cpu->secure_tag_memory,
                                     qdev_prop_allow_set_link_before_realize,
                                     OBJ_PROP_LINK_STRONG);
        }
    }
#endif
}

static void arm_cpu_finalizefn(Object *obj)
{
    ARMCPU *cpu = ARM_CPU(obj);
    ARMELChangeHook *hook, *next;

    g_hash_table_destroy(cpu->cp_regs);

    QLIST_FOREACH_SAFE(hook, &cpu->pre_el_change_hooks, node, next) {
        QLIST_REMOVE(hook, node);
        g_free(hook);
    }
    QLIST_FOREACH_SAFE(hook, &cpu->el_change_hooks, node, next) {
        QLIST_REMOVE(hook, node);
        g_free(hook);
    }
#ifndef CONFIG_USER_ONLY
    if (cpu->pmu_timer) {
        timer_free(cpu->pmu_timer);
    }
    if (cpu->wfxt_timer) {
        timer_free(cpu->wfxt_timer);
    }
#endif
}

void arm_cpu_finalize_features(ARMCPU *cpu, Error **errp)
{
    Error *local_err = NULL;

#ifdef TARGET_AARCH64
    if (arm_feature(&cpu->env, ARM_FEATURE_AARCH64)) {
        arm_cpu_sve_finalize(cpu, &local_err);
        if (local_err != NULL) {
            error_propagate(errp, local_err);
            return;
        }

        if (cpu_isar_feature(aa64_sme, cpu) && !cpu_isar_feature(aa64_sve, cpu)) {
            object_property_set_bool(OBJECT(cpu), "sme", false, &error_abort);
        }

        arm_cpu_sme_finalize(cpu, &local_err);
        if (local_err != NULL) {
            error_propagate(errp, local_err);
            return;
        }

        arm_cpu_pauth_finalize(cpu, &local_err);
        if (local_err != NULL) {
            error_propagate(errp, local_err);
            return;
        }

        arm_cpu_lpa2_finalize(cpu, &local_err);
        if (local_err != NULL) {
            error_propagate(errp, local_err);
            return;
        }
    }
#endif

}

static void arm_cpu_realizefn(DeviceState *dev, Error **errp)
{
    CPUState *cs = CPU(dev);
    ARMCPU *cpu = ARM_CPU(dev);
    ARMCPUClass *acc = ARM_CPU_GET_CLASS(dev);
    CPUARMState *env = &cpu->env;
    Error *local_err = NULL;

    if (cpu->host_cpu_probe_failed) {
        if (!gunyah_enabled()) {
            error_setg(errp, "The 'host' CPU type can only be used with Gunyah");
        } else {
            error_setg(errp, "Failed to retrieve host CPU features");
        }
        return;
    }

    if (!cpu->gt_cntfrq_hz) {
        if (arm_feature(env, ARM_FEATURE_BACKCOMPAT_CNTFRQ) ||
            cpu->backcompat_cntfrq) {
            cpu->gt_cntfrq_hz = GTIMER_BACKCOMPAT_HZ;
        } else {
            cpu->gt_cntfrq_hz = GTIMER_DEFAULT_HZ;
        }
    }

#ifndef CONFIG_USER_ONLY
    if (arm_feature(env, ARM_FEATURE_M)) {
        if (!env->nvic) {
            error_setg(errp, "This board cannot be used with Cortex-M CPUs");
            return;
        }
    } else {
        if (env->nvic) {
            error_setg(errp, "This board can only be used with Cortex-M CPUs");
            return;
        }
    }

        if (arm_feature(env, ARM_FEATURE_M)) {
            error_setg(errp,
                       "Cannot enable %s when using an M-profile guest CPU",
                       current_accel_name());
            return;
        }
        if (cpu->has_el3) {
            error_setg(errp,
                       "Cannot enable %s when guest CPU has EL3 enabled",
                       current_accel_name());
            return;
        }
        if (cpu->tag_memory) {
            error_setg(errp,
                       "Cannot enable %s when guest CPUs has MTE enabled",
                       current_accel_name());
            return;
        }
    {
        uint64_t scale = gt_cntfrq_period_ns(cpu);

        cpu->gt_timer[GTIMER_PHYS] = timer_new(QEMU_CLOCK_VIRTUAL, scale,
                                               arm_gt_ptimer_cb, cpu);
        cpu->gt_timer[GTIMER_VIRT] = timer_new(QEMU_CLOCK_VIRTUAL, scale,
                                               arm_gt_vtimer_cb, cpu);
        cpu->gt_timer[GTIMER_HYP] = timer_new(QEMU_CLOCK_VIRTUAL, scale,
                                              arm_gt_htimer_cb, cpu);
        cpu->gt_timer[GTIMER_SEC] = timer_new(QEMU_CLOCK_VIRTUAL, scale,
                                              arm_gt_stimer_cb, cpu);
        cpu->gt_timer[GTIMER_HYPVIRT] = timer_new(QEMU_CLOCK_VIRTUAL, scale,
                                                  arm_gt_hvtimer_cb, cpu);
        cpu->gt_timer[GTIMER_S_EL2_PHYS] = timer_new(QEMU_CLOCK_VIRTUAL, scale,
                                                     arm_gt_sel2timer_cb, cpu);
        cpu->gt_timer[GTIMER_S_EL2_VIRT] = timer_new(QEMU_CLOCK_VIRTUAL, scale,
                                                     arm_gt_sel2vtimer_cb, cpu);
    }
#endif

    cpu_exec_realizefn(cs, &local_err);
    if (local_err != NULL) {
        error_propagate(errp, local_err);
        return;
    }

    arm_cpu_finalize_features(cpu, &local_err);
    if (local_err != NULL) {
        error_propagate(errp, local_err);
        return;
    }

#ifdef CONFIG_USER_ONLY
    cpu->ctr = FIELD_DP64(cpu->ctr, CTR_EL0, DIC, 0);
#endif

    if (arm_feature(env, ARM_FEATURE_AARCH64) &&
        cpu->has_vfp != cpu->has_neon) {
        error_setg(errp,
                   "AArch64 CPUs must have both VFP and Neon or neither");
        return;
    }

    if (cpu->has_vfp_d32 != cpu->has_neon) {
        error_setg(errp, "ARM CPUs must have both VFP-D32 and Neon or neither");
        return;
    }

   if (!cpu->has_vfp_d32) {
        uint32_t u;

        u = cpu->isar.mvfr0;
        u = FIELD_DP32(u, MVFR0, SIMDREG, 1); /* 16 registers */
        cpu->isar.mvfr0 = u;
    }

    if (!cpu->has_vfp) {
        uint64_t t;
        uint32_t u;

        t = cpu->isar.id_aa64isar1;
        t = FIELD_DP64(t, ID_AA64ISAR1, JSCVT, 0);
        cpu->isar.id_aa64isar1 = t;

        t = cpu->isar.id_aa64pfr0;
        t = FIELD_DP64(t, ID_AA64PFR0, FP, 0xf);
        cpu->isar.id_aa64pfr0 = t;

        u = cpu->isar.id_isar6;
        u = FIELD_DP32(u, ID_ISAR6, JSCVT, 0);
        u = FIELD_DP32(u, ID_ISAR6, BF16, 0);
        cpu->isar.id_isar6 = u;

        u = cpu->isar.mvfr0;
        u = FIELD_DP32(u, MVFR0, FPSP, 0);
        u = FIELD_DP32(u, MVFR0, FPDP, 0);
        u = FIELD_DP32(u, MVFR0, FPDIVIDE, 0);
        u = FIELD_DP32(u, MVFR0, FPSQRT, 0);
        u = FIELD_DP32(u, MVFR0, FPROUND, 0);
        if (!arm_feature(env, ARM_FEATURE_M)) {
            u = FIELD_DP32(u, MVFR0, FPTRAP, 0);
            u = FIELD_DP32(u, MVFR0, FPSHVEC, 0);
        }
        cpu->isar.mvfr0 = u;

        u = cpu->isar.mvfr1;
        u = FIELD_DP32(u, MVFR1, FPFTZ, 0);
        u = FIELD_DP32(u, MVFR1, FPDNAN, 0);
        u = FIELD_DP32(u, MVFR1, FPHP, 0);
        if (arm_feature(env, ARM_FEATURE_M)) {
            u = FIELD_DP32(u, MVFR1, FP16, 0);
        }
        cpu->isar.mvfr1 = u;

        u = cpu->isar.mvfr2;
        u = FIELD_DP32(u, MVFR2, FPMISC, 0);
        cpu->isar.mvfr2 = u;
    }

    if (!cpu->has_neon) {
        uint64_t t;
        uint32_t u;

        unset_feature(env, ARM_FEATURE_NEON);

        t = cpu->isar.id_aa64isar0;
        t = FIELD_DP64(t, ID_AA64ISAR0, AES, 0);
        t = FIELD_DP64(t, ID_AA64ISAR0, SHA1, 0);
        t = FIELD_DP64(t, ID_AA64ISAR0, SHA2, 0);
        t = FIELD_DP64(t, ID_AA64ISAR0, SHA3, 0);
        t = FIELD_DP64(t, ID_AA64ISAR0, SM3, 0);
        t = FIELD_DP64(t, ID_AA64ISAR0, SM4, 0);
        t = FIELD_DP64(t, ID_AA64ISAR0, DP, 0);
        cpu->isar.id_aa64isar0 = t;

        t = cpu->isar.id_aa64isar1;
        t = FIELD_DP64(t, ID_AA64ISAR1, FCMA, 0);
        t = FIELD_DP64(t, ID_AA64ISAR1, BF16, 0);
        t = FIELD_DP64(t, ID_AA64ISAR1, I8MM, 0);
        cpu->isar.id_aa64isar1 = t;

        t = cpu->isar.id_aa64pfr0;
        t = FIELD_DP64(t, ID_AA64PFR0, ADVSIMD, 0xf);
        cpu->isar.id_aa64pfr0 = t;

        u = cpu->isar.id_isar5;
        u = FIELD_DP32(u, ID_ISAR5, AES, 0);
        u = FIELD_DP32(u, ID_ISAR5, SHA1, 0);
        u = FIELD_DP32(u, ID_ISAR5, SHA2, 0);
        u = FIELD_DP32(u, ID_ISAR5, RDM, 0);
        u = FIELD_DP32(u, ID_ISAR5, VCMA, 0);
        cpu->isar.id_isar5 = u;

        u = cpu->isar.id_isar6;
        u = FIELD_DP32(u, ID_ISAR6, DP, 0);
        u = FIELD_DP32(u, ID_ISAR6, FHM, 0);
        u = FIELD_DP32(u, ID_ISAR6, BF16, 0);
        u = FIELD_DP32(u, ID_ISAR6, I8MM, 0);
        cpu->isar.id_isar6 = u;

        if (!arm_feature(env, ARM_FEATURE_M)) {
            u = cpu->isar.mvfr1;
            u = FIELD_DP32(u, MVFR1, SIMDLS, 0);
            u = FIELD_DP32(u, MVFR1, SIMDINT, 0);
            u = FIELD_DP32(u, MVFR1, SIMDSP, 0);
            u = FIELD_DP32(u, MVFR1, SIMDHP, 0);
            cpu->isar.mvfr1 = u;

            u = cpu->isar.mvfr2;
            u = FIELD_DP32(u, MVFR2, SIMDMISC, 0);
            cpu->isar.mvfr2 = u;
        }
    }

    if (!cpu->has_neon && !cpu->has_vfp) {
        uint64_t t;
        uint32_t u;

        t = cpu->isar.id_aa64isar0;
        t = FIELD_DP64(t, ID_AA64ISAR0, FHM, 0);
        cpu->isar.id_aa64isar0 = t;

        t = cpu->isar.id_aa64isar1;
        t = FIELD_DP64(t, ID_AA64ISAR1, FRINTTS, 0);
        cpu->isar.id_aa64isar1 = t;

        u = cpu->isar.mvfr0;
        u = FIELD_DP32(u, MVFR0, SIMDREG, 0);
        cpu->isar.mvfr0 = u;

        u = cpu->isar.mvfr1;
        u = FIELD_DP32(u, MVFR1, SIMDFMAC, 0);
        cpu->isar.mvfr1 = u;
    }

    if (arm_feature(env, ARM_FEATURE_M) && !cpu->has_dsp) {
        uint32_t u;

        unset_feature(env, ARM_FEATURE_THUMB_DSP);

        u = cpu->isar.id_isar1;
        u = FIELD_DP32(u, ID_ISAR1, EXTEND, 1);
        cpu->isar.id_isar1 = u;

        u = cpu->isar.id_isar2;
        u = FIELD_DP32(u, ID_ISAR2, MULTU, 1);
        u = FIELD_DP32(u, ID_ISAR2, MULTS, 1);
        cpu->isar.id_isar2 = u;

        u = cpu->isar.id_isar3;
        u = FIELD_DP32(u, ID_ISAR3, SIMD, 1);
        u = FIELD_DP32(u, ID_ISAR3, SATURATE, 0);
        cpu->isar.id_isar3 = u;
    }


    assert(arm_feature(env, ARM_FEATURE_AARCH64) ||
           !cpu_isar_feature(aa32_vfp_simd, cpu) ||
           !arm_feature(env, ARM_FEATURE_XSCALE));

#ifndef CONFIG_USER_ONLY
    {
        int pagebits;
        if (arm_feature(env, ARM_FEATURE_V7) &&
            !arm_feature(env, ARM_FEATURE_M) &&
            !arm_feature(env, ARM_FEATURE_PMSA)) {
            pagebits = 12;
        } else {
            pagebits = 10;
        }
        if (!set_preferred_target_page_bits(pagebits)) {
            error_setg(errp, "This CPU requires a smaller page size "
                       "than the system is using");
            return;
        }
    }
#endif

    if (cpu->mp_affinity == ARM64_AFFINITY_INVALID) {
        cpu->mp_affinity = arm_build_mp_affinity(cs->cpu_index,
                                                 ARM_DEFAULT_CPUS_PER_CLUSTER);
    }

    if (cpu->reset_hivecs) {
            cpu->reset_sctlr |= (1 << 13);
    }

    if (cpu->cfgend) {
        if (arm_feature(env, ARM_FEATURE_V7)) {
            cpu->reset_sctlr |= SCTLR_EE;
        } else {
            cpu->reset_sctlr |= SCTLR_B;
        }
    }

    if (!arm_feature(env, ARM_FEATURE_M) && !cpu->has_el3) {
        unset_feature(env, ARM_FEATURE_EL3);

        cpu->isar.id_pfr1 = FIELD_DP32(cpu->isar.id_pfr1, ID_PFR1, SECURITY, 0);
        cpu->isar.id_dfr0 = FIELD_DP32(cpu->isar.id_dfr0, ID_DFR0, COPSDBG, 0);
        cpu->isar.id_aa64pfr0 = FIELD_DP64(cpu->isar.id_aa64pfr0,
                                           ID_AA64PFR0, EL3, 0);

        cpu->isar.id_aa64pfr0 = FIELD_DP64(cpu->isar.id_aa64pfr0,
                                           ID_AA64PFR0, RME, 0);
    }

    if (!cpu->has_el2) {
        unset_feature(env, ARM_FEATURE_EL2);
    }

    if (!cpu->has_pmu) {
        unset_feature(env, ARM_FEATURE_PMU);
    }
    if (arm_feature(env, ARM_FEATURE_PMU)) {
        pmu_init(cpu);

        if (!false) {
            arm_register_pre_el_change_hook(cpu, &pmu_pre_el_change, 0);
            arm_register_el_change_hook(cpu, &pmu_post_el_change, 0);
        }

#ifndef CONFIG_USER_ONLY
        cpu->pmu_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, arm_pmu_timer_cb,
                cpu);
#endif
    } else {
        cpu->isar.id_aa64dfr0 =
            FIELD_DP64(cpu->isar.id_aa64dfr0, ID_AA64DFR0, PMUVER, 0);
        cpu->isar.id_dfr0 = FIELD_DP32(cpu->isar.id_dfr0, ID_DFR0, PERFMON, 0);
        cpu->pmceid0 = 0;
        cpu->pmceid1 = 0;
    }

    if (!arm_feature(env, ARM_FEATURE_EL2)) {
        cpu->isar.id_aa64pfr0 = FIELD_DP64(cpu->isar.id_aa64pfr0,
                                           ID_AA64PFR0, EL2, 0);
        cpu->isar.id_pfr1 = FIELD_DP32(cpu->isar.id_pfr1,
                                       ID_PFR1, VIRTUALIZATION, 0);
    }

    if (cpu_isar_feature(aa64_mte, cpu)) {
#ifndef CONFIG_USER_ONLY
        if (true) {
                FIELD_DP64(cpu->isar.id_aa64pfr1, ID_AA64PFR1, MTE, 0);
        }
#endif
    }

    if (!cpu->has_mpu || cpu->pmsav7_dregion == 0) {
        cpu->has_mpu = false;
        cpu->pmsav7_dregion = 0;
        cpu->pmsav8r_hdregion = 0;
    }

    if (arm_feature(env, ARM_FEATURE_PMSA) &&
        arm_feature(env, ARM_FEATURE_V7)) {
        uint32_t nr = cpu->pmsav7_dregion;

        if (nr > 0xff) {
            error_setg(errp, "PMSAv7 MPU #regions invalid %" PRIu32, nr);
            return;
        }

        if (nr) {
            if (arm_feature(env, ARM_FEATURE_V8)) {
                env->pmsav8.rbar[M_REG_NS] = g_new0(uint32_t, nr);
                env->pmsav8.rlar[M_REG_NS] = g_new0(uint32_t, nr);
                if (arm_feature(env, ARM_FEATURE_M_SECURITY)) {
                    env->pmsav8.rbar[M_REG_S] = g_new0(uint32_t, nr);
                    env->pmsav8.rlar[M_REG_S] = g_new0(uint32_t, nr);
                }
            } else {
                env->pmsav7.drbar = g_new0(uint32_t, nr);
                env->pmsav7.drsr = g_new0(uint32_t, nr);
                env->pmsav7.dracr = g_new0(uint32_t, nr);
            }
        }

        if (cpu->pmsav8r_hdregion > 0xff) {
            error_setg(errp, "PMSAv8 MPU EL2 #regions invalid %" PRIu32,
                              cpu->pmsav8r_hdregion);
            return;
        }

        if (cpu->pmsav8r_hdregion) {
            env->pmsav8.hprbar = g_new0(uint32_t,
                                        cpu->pmsav8r_hdregion);
            env->pmsav8.hprlar = g_new0(uint32_t,
                                        cpu->pmsav8r_hdregion);
        }
    }

    if (arm_feature(env, ARM_FEATURE_M_SECURITY)) {
        uint32_t nr = cpu->sau_sregion;

        if (nr > 0xff) {
            error_setg(errp, "v8M SAU #regions invalid %" PRIu32, nr);
            return;
        }

        if (nr) {
            env->sau.rbar = g_new0(uint32_t, nr);
            env->sau.rlar = g_new0(uint32_t, nr);
        }
    }

    if (arm_feature(env, ARM_FEATURE_EL3)) {
        set_feature(env, ARM_FEATURE_VBAR);
    }

    register_cp_regs_for_features(cpu);

    init_cpreg_list(cpu);

#ifndef CONFIG_USER_ONLY
    MachineState *ms = MACHINE(qdev_get_machine());
    unsigned int smp_cpus = ms->smp.cpus;
    bool has_secure = cpu->has_el3 || arm_feature(env, ARM_FEATURE_M_SECURITY);

    if (cpu->tag_memory != NULL) {
        cs->num_ases = 3 + has_secure;
    } else {
        cs->num_ases = 1 + has_secure;
    }

    if (has_secure) {
        if (!cpu->secure_memory) {
            cpu->secure_memory = cs->memory;
        }
        cpu_address_space_init(cs, ARMASIdx_S, "cpu-secure-memory",
                               cpu->secure_memory);
    }

    if (cpu->tag_memory != NULL) {
        cpu_address_space_init(cs, ARMASIdx_TagNS, "cpu-tag-memory",
                               cpu->tag_memory);
        if (has_secure) {
            cpu_address_space_init(cs, ARMASIdx_TagS, "cpu-tag-memory",
                                   cpu->secure_tag_memory);
        }
    }

    cpu_address_space_init(cs, ARMASIdx_NS, "cpu-memory", cs->memory);

    if (cpu->core_count == -1) {
        cpu->core_count = smp_cpus;
    }
#endif

    qemu_init_vcpu(cs);
    cpu_reset(cs);

    acc->parent_realize(dev, errp);
}

static ObjectClass *arm_cpu_class_by_name(const char *cpu_model)
{
    ObjectClass *oc;
    char *typename;
    char **cpuname;
    const char *cpunamestr;

    cpuname = g_strsplit(cpu_model, ",", 1);
    cpunamestr = cpuname[0];
#ifdef CONFIG_USER_ONLY
    if (!strcmp(cpunamestr, "any")) {
        cpunamestr = "max";
    }
#endif
    typename = g_strdup_printf(ARM_CPU_TYPE_NAME("%s"), cpunamestr);
    oc = object_class_by_name(typename);
    g_strfreev(cpuname);
    g_free(typename);

    return oc;
}

static const Property arm_cpu_properties[] = {
    DEFINE_PROP_UINT64("midr", ARMCPU, midr, 0),
    DEFINE_PROP_UINT64("mp-affinity", ARMCPU,
                        mp_affinity, ARM64_AFFINITY_INVALID),
    DEFINE_PROP_INT32("node-id", ARMCPU, node_id, CPU_UNSET_NUMA_NODE_ID),
    DEFINE_PROP_INT32("core-count", ARMCPU, core_count, -1),
    DEFINE_PROP_BOOL("backcompat-cntfrq", ARMCPU, backcompat_cntfrq, false),
    DEFINE_PROP_BOOL("backcompat-pauth-default-use-qarma5", ARMCPU,
                      backcompat_pauth_default_use_qarma5, false),
};

#ifndef CONFIG_USER_ONLY
#include "hw/core/sysemu-cpu-ops.h"

static const struct SysemuCPUOps arm_sysemu_ops = {
    .has_work = arm_cpu_has_work,
    .get_phys_page_attrs_debug = arm_cpu_get_phys_page_attrs_debug,
    .asidx_from_attrs = arm_asidx_from_attrs,
    .virtio_is_big_endian = arm_cpu_virtio_is_big_endian,
    .legacy_vmsd = &vmstate_arm_cpu,
};
#endif


static void arm_cpu_class_init(ObjectClass *oc, void *data)
{
    ARMCPUClass *acc = ARM_CPU_CLASS(oc);
    CPUClass *cc = CPU_CLASS(acc);
    DeviceClass *dc = DEVICE_CLASS(oc);
    ResettableClass *rc = RESETTABLE_CLASS(oc);

    device_class_set_parent_realize(dc, arm_cpu_realizefn,
                                    &acc->parent_realize);

    device_class_set_props(dc, arm_cpu_properties);

    resettable_class_set_parent_phases(rc, NULL, arm_cpu_reset_hold, NULL,
                                       &acc->parent_phases);

    cc->class_by_name = arm_cpu_class_by_name;
    cc->mmu_index = arm_cpu_mmu_index;
    cc->dump_state = arm_cpu_dump_state;
    cc->set_pc = arm_cpu_set_pc;
    cc->get_pc = arm_cpu_get_pc;
#ifndef CONFIG_USER_ONLY
    cc->sysemu_ops = &arm_sysemu_ops;
#endif
    cc->disas_set_info = arm_disas_set_info;

}

static void arm_cpu_instance_init(Object *obj)
{
    ARMCPUClass *acc = ARM_CPU_GET_CLASS(obj);

    acc->info->initfn(obj);
    arm_cpu_post_init(obj);
}

static void cpu_register_class_init(ObjectClass *oc, void *data)
{
    ARMCPUClass *acc = ARM_CPU_CLASS(oc);
    CPUClass *cc = CPU_CLASS(acc);

    acc->info = data;
    if (acc->info->deprecation_note) {
        cc->deprecation_note = acc->info->deprecation_note;
    }
}

void arm_cpu_register(const ARMCPUInfo *info)
{
    TypeInfo type_info = {
        .parent = TYPE_ARM_CPU,
        .instance_init = arm_cpu_instance_init,
        .class_init = info->class_init ?: cpu_register_class_init,
        .class_data = (void *)info,
    };

    type_info.name = g_strdup_printf("%s-" TYPE_ARM_CPU, info->name);
    type_register_static(&type_info);
    g_free((void *)type_info.name);
}

static const TypeInfo arm_cpu_type_info = {
    .name = TYPE_ARM_CPU,
    .parent = TYPE_CPU,
    .instance_size = sizeof(ARMCPU),
    .instance_align = __alignof__(ARMCPU),
    .instance_init = arm_cpu_initfn,
    .instance_finalize = arm_cpu_finalizefn,
    .abstract = true,
    .class_size = sizeof(ARMCPUClass),
    .class_init = arm_cpu_class_init,
};

static void arm_cpu_register_types(void)
{
    type_register_static(&arm_cpu_type_info);
}

type_init(arm_cpu_register_types)
