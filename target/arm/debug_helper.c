#include "qemu/osdep.h"
#include "qemu/log.h"
#include "cpu.h"
#include "internals.h"
#include "cpu-features.h"
#include "cpregs.h"
#include "exec/exec-all.h"


static CPAccessResult access_tdosa(CPUARMState *env, const ARMCPRegInfo *ri,
                                   bool isread)
{
    int el = arm_current_el(env);
    uint64_t mdcr_el2 = arm_mdcr_el2_eff(env);
    bool mdcr_el2_tdosa = (mdcr_el2 & MDCR_TDOSA) || (mdcr_el2 & MDCR_TDE) ||
        (arm_hcr_el2_eff(env) & HCR_TGE);

    if (el < 2 && mdcr_el2_tdosa) {
        return CP_ACCESS_TRAP_EL2;
    }
    if (el < 3 && (env->cp15.mdcr_el3 & MDCR_TDOSA)) {
        return CP_ACCESS_TRAP_EL3;
    }
    return CP_ACCESS_OK;
}

static CPAccessResult access_tdra(CPUARMState *env, const ARMCPRegInfo *ri,
                                  bool isread)
{
    int el = arm_current_el(env);
    uint64_t mdcr_el2 = arm_mdcr_el2_eff(env);
    bool mdcr_el2_tdra = (mdcr_el2 & MDCR_TDRA) || (mdcr_el2 & MDCR_TDE) ||
        (arm_hcr_el2_eff(env) & HCR_TGE);

    if (el < 2 && mdcr_el2_tdra) {
        return CP_ACCESS_TRAP_EL2;
    }
    if (el < 3 && (env->cp15.mdcr_el3 & MDCR_TDA)) {
        return CP_ACCESS_TRAP_EL3;
    }
    return CP_ACCESS_OK;
}

static CPAccessResult access_tda(CPUARMState *env, const ARMCPRegInfo *ri,
                                  bool isread)
{
    int el = arm_current_el(env);
    uint64_t mdcr_el2 = arm_mdcr_el2_eff(env);
    bool mdcr_el2_tda = (mdcr_el2 & MDCR_TDA) || (mdcr_el2 & MDCR_TDE) ||
        (arm_hcr_el2_eff(env) & HCR_TGE);

    if (el < 2 && mdcr_el2_tda) {
        return CP_ACCESS_TRAP_EL2;
    }
    if (el < 3 && (env->cp15.mdcr_el3 & MDCR_TDA)) {
        return CP_ACCESS_TRAP_EL3;
    }
    return CP_ACCESS_OK;
}

static CPAccessResult access_dbgvcr32(CPUARMState *env, const ARMCPRegInfo *ri,
                                      bool isread)
{
    if (arm_current_el(env) == 2 && (env->cp15.mdcr_el3 & MDCR_TDA)) {
        return CP_ACCESS_TRAP_EL3;
    }
    return CP_ACCESS_OK;
}

static CPAccessResult access_tdcc(CPUARMState *env, const ARMCPRegInfo *ri,
                                  bool isread)
{
    int el = arm_current_el(env);
    uint64_t mdcr_el2 = arm_mdcr_el2_eff(env);
    bool mdscr_el1_tdcc = extract32(env->cp15.mdscr_el1, 12, 1);
    bool mdcr_el2_tda = (mdcr_el2 & MDCR_TDA) || (mdcr_el2 & MDCR_TDE) ||
        (arm_hcr_el2_eff(env) & HCR_TGE);
    bool mdcr_el2_tdcc = cpu_isar_feature(aa64_fgt, env_archcpu(env)) &&
                                          (mdcr_el2 & MDCR_TDCC);
    bool mdcr_el3_tdcc = cpu_isar_feature(aa64_fgt, env_archcpu(env)) &&
                                          (env->cp15.mdcr_el3 & MDCR_TDCC);

    if (el < 1 && mdscr_el1_tdcc) {
        return CP_ACCESS_TRAP_EL1;
    }
    if (el < 2 && (mdcr_el2_tda || mdcr_el2_tdcc)) {
        return CP_ACCESS_TRAP_EL2;
    }
    if (!arm_is_el3_or_mon(env) &&
        ((env->cp15.mdcr_el3 & MDCR_TDA) || mdcr_el3_tdcc)) {
        return CP_ACCESS_TRAP_EL3;
    }
    return CP_ACCESS_OK;
}

static void oslar_write(CPUARMState *env, const ARMCPRegInfo *ri,
                        uint64_t value)
{
    int oslock;

    if (ri->state == ARM_CP_STATE_AA32) {
        oslock = (value == 0xC5ACCE55);
    } else {
        oslock = value & 1;
    }

    env->cp15.oslsr_el1 = deposit32(env->cp15.oslsr_el1, 1, 1, oslock);
}

static void osdlr_write(CPUARMState *env, const ARMCPRegInfo *ri,
                        uint64_t value)
{
    ARMCPU *cpu = env_archcpu(env);
    if(arm_feature(env, ARM_FEATURE_AARCH64)
       ? cpu_isar_feature(aa64_doublelock, cpu)
       : cpu_isar_feature(aa32_doublelock, cpu)) {
        env->cp15.osdlr_el1 = value & 1;
    }
}

static void dbgclaimset_write(CPUARMState *env, const ARMCPRegInfo *ri,
                              uint64_t value)
{
    env->cp15.dbgclaim |= (value & 0xFF);
}

static uint64_t dbgclaimset_read(CPUARMState *env, const ARMCPRegInfo *ri)
{
    return 0xFF;
}

static void dbgclaimclr_write(CPUARMState *env, const ARMCPRegInfo *ri,
                              uint64_t value)
{
    env->cp15.dbgclaim &= ~(value & 0xFF);
}

static const ARMCPRegInfo debug_cp_reginfo[] = {
    { .name = "DBGDRAR", .cp = 14, .crn = 1, .crm = 0, .opc1 = 0, .opc2 = 0,
      .access = PL0_R, .accessfn = access_tdra,
      .type = ARM_CP_CONST, .resetvalue = 0 },
    { .name = "MDRAR_EL1", .state = ARM_CP_STATE_AA64,
      .opc0 = 2, .opc1 = 0, .crn = 1, .crm = 0, .opc2 = 0,
      .access = PL1_R, .accessfn = access_tdra,
      .type = ARM_CP_CONST, .resetvalue = 0 },
    { .name = "DBGDSAR", .cp = 14, .crn = 2, .crm = 0, .opc1 = 0, .opc2 = 0,
      .access = PL0_R, .accessfn = access_tdra,
      .type = ARM_CP_CONST, .resetvalue = 0 },
    { .name = "MDSCR_EL1", .state = ARM_CP_STATE_BOTH,
      .cp = 14, .opc0 = 2, .opc1 = 0, .crn = 0, .crm = 2, .opc2 = 2,
      .access = PL1_RW, .accessfn = access_tda,
      .fgt = FGT_MDSCR_EL1,
      .nv2_redirect_offset = 0x158,
      .fieldoffset = offsetof(CPUARMState, cp15.mdscr_el1),
      .resetvalue = 0 },
    { .name = "MDCCSR_EL0", .state = ARM_CP_STATE_AA64,
      .opc0 = 2, .opc1 = 3, .crn = 0, .crm = 1, .opc2 = 0,
      .access = PL0_R, .accessfn = access_tdcc,
      .type = ARM_CP_CONST, .resetvalue = 0 },
    { .name = "OSDTRRX_EL1", .state = ARM_CP_STATE_BOTH, .cp = 14,
      .opc0 = 2, .opc1 = 0, .crn = 0, .crm = 0, .opc2 = 2,
      .access = PL1_RW, .accessfn = access_tdcc,
      .type = ARM_CP_CONST, .resetvalue = 0 },
    { .name = "OSDTRTX_EL1", .state = ARM_CP_STATE_BOTH, .cp = 14,
      .opc0 = 2, .opc1 = 0, .crn = 0, .crm = 3, .opc2 = 2,
      .access = PL1_RW, .accessfn = access_tdcc,
      .type = ARM_CP_CONST, .resetvalue = 0 },
    { .name = "DBGDTR_EL0", .state = ARM_CP_STATE_BOTH, .cp = 14,
      .opc0 = 2, .opc1 = 3, .crn = 0, .crm = 5, .opc2 = 0,
      .access = PL0_RW, .accessfn = access_tdcc,
      .type = ARM_CP_CONST, .resetvalue = 0 },
    { .name = "OSECCR_EL1", .state = ARM_CP_STATE_BOTH, .cp = 14,
      .opc0 = 2, .opc1 = 0, .crn = 0, .crm = 6, .opc2 = 2,
      .access = PL1_RW, .accessfn = access_tda,
      .fgt = FGT_OSECCR_EL1,
      .type = ARM_CP_CONST, .resetvalue = 0 },
    { .name = "DBGDSCRint", .state = ARM_CP_STATE_AA32,
      .cp = 14, .opc1 = 0, .crn = 0, .crm = 1, .opc2 = 0,
      .type = ARM_CP_ALIAS,
      .access = PL1_R, .accessfn = access_tda,
      .fieldoffset = offsetof(CPUARMState, cp15.mdscr_el1), },
    { .name = "OSLAR_EL1", .state = ARM_CP_STATE_BOTH,
      .cp = 14, .opc0 = 2, .opc1 = 0, .crn = 1, .crm = 0, .opc2 = 4,
      .access = PL1_W, .type = ARM_CP_NO_RAW,
      .accessfn = access_tdosa,
      .fgt = FGT_OSLAR_EL1,
      .writefn = oslar_write },
    { .name = "OSLSR_EL1", .state = ARM_CP_STATE_BOTH,
      .cp = 14, .opc0 = 2, .opc1 = 0, .crn = 1, .crm = 1, .opc2 = 4,
      .access = PL1_R, .resetvalue = 10,
      .accessfn = access_tdosa,
      .fgt = FGT_OSLSR_EL1,
      .fieldoffset = offsetof(CPUARMState, cp15.oslsr_el1) },
    { .name = "OSDLR_EL1", .state = ARM_CP_STATE_BOTH,
      .cp = 14, .opc0 = 2, .opc1 = 0, .crn = 1, .crm = 3, .opc2 = 4,
      .access = PL1_RW, .accessfn = access_tdosa,
      .fgt = FGT_OSDLR_EL1,
      .writefn = osdlr_write,
      .fieldoffset = offsetof(CPUARMState, cp15.osdlr_el1) },
    { .name = "DBGVCR",
      .cp = 14, .opc1 = 0, .crn = 0, .crm = 7, .opc2 = 0,
      .access = PL1_RW, .accessfn = access_tda,
      .type = ARM_CP_CONST, .resetvalue = 0 },
    { .name = "MDCCINT_EL1", .state = ARM_CP_STATE_BOTH,
      .cp = 14, .opc0 = 2, .opc1 = 0, .crn = 0, .crm = 2, .opc2 = 0,
      .access = PL1_RW, .accessfn = access_tdcc,
      .type = ARM_CP_CONST, .resetvalue = 0 },
    { .name = "DBGCLAIMSET_EL1", .state = ARM_CP_STATE_BOTH,
      .cp = 14, .opc0 = 2, .opc1 = 0, .crn = 7, .crm = 8, .opc2 = 6,
      .type = ARM_CP_ALIAS,
      .access = PL1_RW, .accessfn = access_tda,
      .fgt = FGT_DBGCLAIM,
      .writefn = dbgclaimset_write, .readfn = dbgclaimset_read },
    { .name = "DBGCLAIMCLR_EL1", .state = ARM_CP_STATE_BOTH,
      .cp = 14, .opc0 = 2, .opc1 = 0, .crn = 7, .crm = 9, .opc2 = 6,
      .access = PL1_RW, .accessfn = access_tda,
      .fgt = FGT_DBGCLAIM,
      .writefn = dbgclaimclr_write, .raw_writefn = raw_write,
      .fieldoffset = offsetof(CPUARMState, cp15.dbgclaim) },
};

static const ARMCPRegInfo debug_aa32_el1_reginfo[] = {
    { .name = "DBGVCR32_EL2", .state = ARM_CP_STATE_AA64,
      .opc0 = 2, .opc1 = 4, .crn = 0, .crm = 7, .opc2 = 0,
      .access = PL2_RW, .accessfn = access_dbgvcr32,
      .type = ARM_CP_CONST | ARM_CP_EL3_NO_EL2_KEEP,
      .resetvalue = 0 },
};

static const ARMCPRegInfo debug_lpae_cp_reginfo[] = {
    { .name = "DBGDRAR", .cp = 14, .crm = 1, .opc1 = 0,
      .access = PL0_R, .type = ARM_CP_CONST | ARM_CP_64BIT,
      .resetvalue = 0 },
    { .name = "DBGDSAR", .cp = 14, .crm = 2, .opc1 = 0,
      .access = PL0_R, .type = ARM_CP_CONST | ARM_CP_64BIT,
      .resetvalue = 0 },
};

static void dbgwvr_write(CPUARMState *env, const ARMCPRegInfo *ri,
                         uint64_t value)
{
    value &= ~3ULL;

    raw_write(env, ri, value);
}

static void dbgwcr_write(CPUARMState *env, const ARMCPRegInfo *ri,
                         uint64_t value)
{
    raw_write(env, ri, value);
}

static void dbgbvr_write(CPUARMState *env, const ARMCPRegInfo *ri,
                         uint64_t value)
{
    raw_write(env, ri, value);
}

static void dbgbcr_write(CPUARMState *env, const ARMCPRegInfo *ri,
                         uint64_t value)
{
    value = deposit64(value, 6, 1, extract64(value, 5, 1));
    value = deposit64(value, 8, 1, extract64(value, 7, 1));

    raw_write(env, ri, value);
}

void define_debug_regs(ARMCPU *cpu)
{
    int i;
    int wrps, brps, ctx_cmps;

    if (cpu->isar.dbgdidr != 0) {
        ARMCPRegInfo dbgdidr = {
            .name = "DBGDIDR", .cp = 14, .crn = 0, .crm = 0,
            .opc1 = 0, .opc2 = 0,
            .access = PL0_R, .accessfn = access_tda,
            .type = ARM_CP_CONST, .resetvalue = cpu->isar.dbgdidr,
        };
        define_one_arm_cp_reg(cpu, &dbgdidr);
    }

    if (extract32(cpu->isar.dbgdidr, 15, 1)) {
        ARMCPRegInfo dbgdevid = {
            .name = "DBGDEVID",
            .cp = 14, .opc1 = 0, .crn = 7, .opc2 = 2, .crn = 7,
            .access = PL1_R, .accessfn = access_tda,
            .type = ARM_CP_CONST, .resetvalue = cpu->isar.dbgdevid,
        };
        define_one_arm_cp_reg(cpu, &dbgdevid);
    }
    if (cpu_isar_feature(aa32_debugv7p1, cpu)) {
        ARMCPRegInfo dbgdevid12[] = {
            {
                .name = "DBGDEVID1",
                .cp = 14, .opc1 = 0, .crn = 7, .opc2 = 1, .crn = 7,
                .access = PL1_R, .accessfn = access_tda,
                .type = ARM_CP_CONST, .resetvalue = cpu->isar.dbgdevid1,
            }, {
                .name = "DBGDEVID2",
                .cp = 14, .opc1 = 0, .crn = 7, .opc2 = 0, .crn = 7,
                .access = PL1_R, .accessfn = access_tda,
                .type = ARM_CP_CONST, .resetvalue = 0,
            },
        };
        define_arm_cp_regs(cpu, dbgdevid12);
    }

    brps = arm_num_brps(cpu);
    wrps = arm_num_wrps(cpu);
    ctx_cmps = arm_num_ctx_cmps(cpu);

    assert(ctx_cmps <= brps);

    define_arm_cp_regs(cpu, debug_cp_reginfo);
    if (cpu_isar_feature(aa64_aa32_el1, cpu)) {
        define_arm_cp_regs(cpu, debug_aa32_el1_reginfo);
    }

    if (arm_feature(&cpu->env, ARM_FEATURE_LPAE)) {
        define_arm_cp_regs(cpu, debug_lpae_cp_reginfo);
    }

    for (i = 0; i < brps; i++) {
        char *dbgbvr_el1_name = g_strdup_printf("DBGBVR%d_EL1", i);
        char *dbgbcr_el1_name = g_strdup_printf("DBGBCR%d_EL1", i);
        ARMCPRegInfo dbgregs[] = {
            { .name = dbgbvr_el1_name, .state = ARM_CP_STATE_BOTH,
              .cp = 14, .opc0 = 2, .opc1 = 0, .crn = 0, .crm = i, .opc2 = 4,
              .access = PL1_RW, .accessfn = access_tda,
              .fgt = FGT_DBGBVRN_EL1,
              .fieldoffset = offsetof(CPUARMState, cp15.dbgbvr[i]),
              .writefn = dbgbvr_write, .raw_writefn = raw_write
            },
            { .name = dbgbcr_el1_name, .state = ARM_CP_STATE_BOTH,
              .cp = 14, .opc0 = 2, .opc1 = 0, .crn = 0, .crm = i, .opc2 = 5,
              .access = PL1_RW, .accessfn = access_tda,
              .fgt = FGT_DBGBCRN_EL1,
              .fieldoffset = offsetof(CPUARMState, cp15.dbgbcr[i]),
              .writefn = dbgbcr_write, .raw_writefn = raw_write
            },
        };
        define_arm_cp_regs(cpu, dbgregs);
        g_free(dbgbvr_el1_name);
        g_free(dbgbcr_el1_name);
    }

    for (i = 0; i < wrps; i++) {
        char *dbgwvr_el1_name = g_strdup_printf("DBGWVR%d_EL1", i);
        char *dbgwcr_el1_name = g_strdup_printf("DBGWCR%d_EL1", i);
        ARMCPRegInfo dbgregs[] = {
            { .name = dbgwvr_el1_name, .state = ARM_CP_STATE_BOTH,
              .cp = 14, .opc0 = 2, .opc1 = 0, .crn = 0, .crm = i, .opc2 = 6,
              .access = PL1_RW, .accessfn = access_tda,
              .fgt = FGT_DBGWVRN_EL1,
              .fieldoffset = offsetof(CPUARMState, cp15.dbgwvr[i]),
              .writefn = dbgwvr_write, .raw_writefn = raw_write
            },
            { .name = dbgwcr_el1_name, .state = ARM_CP_STATE_BOTH,
              .cp = 14, .opc0 = 2, .opc1 = 0, .crn = 0, .crm = i, .opc2 = 7,
              .access = PL1_RW, .accessfn = access_tda,
              .fgt = FGT_DBGWCRN_EL1,
              .fieldoffset = offsetof(CPUARMState, cp15.dbgwcr[i]),
              .writefn = dbgwcr_write, .raw_writefn = raw_write
            },
        };
        define_arm_cp_regs(cpu, dbgregs);
        g_free(dbgwvr_el1_name);
        g_free(dbgwcr_el1_name);
    }
}
