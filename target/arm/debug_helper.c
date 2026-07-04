#include "qemu/osdep.h"
#include "qemu/log.h"
#include "cpu.h"
#include "internals.h"
#include "cpu-features.h"
#include "cpregs.h"
#include "exec/exec-all.h"
#include "exec/helper-proto.h"
#include "system/tcg.h"

#ifdef CONFIG_TCG
static int arm_debug_target_el(CPUARMState *env)
{
    bool secure = arm_is_secure(env);
    bool route_to_el2 = false;

    if (arm_feature(env, ARM_FEATURE_M)) {
        return 1;
    }

    if (arm_is_el2_enabled(env)) {
        route_to_el2 = env->cp15.hcr_el2 & HCR_TGE ||
                       env->cp15.mdcr_el2 & MDCR_TDE;
    }

    if (route_to_el2) {
        return 2;
    } else if (arm_feature(env, ARM_FEATURE_EL3) &&
               !arm_el_is_aa64(env, 3) && secure) {
        return 3;
    } else {
        return 1;
    }
}

G_NORETURN static void
raise_exception_debug(CPUARMState *env, uint32_t excp, uint32_t syndrome)
{
    int debug_el = arm_debug_target_el(env);
    int cur_el = arm_current_el(env);

    assert(debug_el >= cur_el);
    syndrome |= (debug_el == cur_el) << ARM_EL_EC_SHIFT;
    raise_exception(env, excp, syndrome, debug_el);
}

static bool aa64_generate_debug_exceptions(CPUARMState *env)
{
    int cur_el = arm_current_el(env);
    int debug_el;

    if (cur_el == 3) {
        return false;
    }

    if (arm_is_secure_below_el3(env)
        && extract32(env->cp15.mdcr_el3, 16, 1)) {
        return false;
    }

    debug_el = arm_debug_target_el(env);

    if (cur_el == debug_el) {
        return extract32(env->cp15.mdscr_el1, 13, 1)
            && !(env->daif & PSTATE_D);
    }

    return debug_el > cur_el;
}

static bool aa32_generate_debug_exceptions(CPUARMState *env)
{
    int el = arm_current_el(env);

    if (el == 0 && arm_el_is_aa64(env, 1)) {
        return aa64_generate_debug_exceptions(env);
    }

    if (arm_is_secure(env)) {
        int spd;

        if (el == 0 && (env->cp15.sder & 1)) {
            return true;
        }

        spd = extract32(env->cp15.mdcr_el3, 14, 2);
        switch (spd) {
        case 1:
        case 0:
            return true;
        case 2:
            return false;
        case 3:
            return true;
        }
    }

    return el != 2;
}

bool arm_generate_debug_exceptions(CPUARMState *env)
{
    if ((env->cp15.oslsr_el1 & 1) || (env->cp15.osdlr_el1 & 1)) {
        return false;
    }
    if (is_a64(env)) {
        return aa64_generate_debug_exceptions(env);
    } else {
        return aa32_generate_debug_exceptions(env);
    }
}

bool arm_singlestep_active(CPUARMState *env)
{
    return extract32(env->cp15.mdscr_el1, 0, 1)
        && arm_el_is_aa64(env, arm_debug_target_el(env))
        && arm_generate_debug_exceptions(env);
}

static bool linked_bp_matches(ARMCPU *cpu, int lbn)
{
    CPUARMState *env = &cpu->env;
    uint64_t bcr = env->cp15.dbgbcr[lbn];
    int brps = arm_num_brps(cpu);
    int ctx_cmps = arm_num_ctx_cmps(cpu);
    int bt;
    uint32_t contextidr;
    uint64_t hcr_el2;

    if (lbn >= brps || lbn < (brps - ctx_cmps)) {
        return false;
    }

    bcr = env->cp15.dbgbcr[lbn];

    if (extract64(bcr, 0, 1) == 0) {
        return false;
    }

    bt = extract64(bcr, 20, 4);
    hcr_el2 = arm_hcr_el2_eff(env);

    switch (bt) {
    case 3: /* linked context ID match */
        switch (arm_current_el(env)) {
        default:
            return false;
        case 2:
            if (!(hcr_el2 & HCR_E2H)) {
                return false;
            }
            contextidr = env->cp15.contextidr_el[2];
            break;
        case 1:
            contextidr = env->cp15.contextidr_el[1];
            break;
        case 0:
            if ((hcr_el2 & (HCR_E2H | HCR_TGE)) == (HCR_E2H | HCR_TGE)) {
                contextidr = env->cp15.contextidr_el[2];
            } else {
                contextidr = env->cp15.contextidr_el[1];
            }
            break;
        }
        break;

    case 7:  /* linked contextidr_el1 match */
        contextidr = env->cp15.contextidr_el[1];
        break;
    case 13: /* linked contextidr_el2 match */
        contextidr = env->cp15.contextidr_el[2];
        break;

    case 9: /* linked VMID match (reserved if no EL2) */
    case 11: /* linked context ID and VMID match (reserved if no EL2) */
    case 15: /* linked full context ID match */
    default:
        return false;
    }

    return contextidr == (uint32_t)env->cp15.dbgbvr[lbn];
}

static bool bp_wp_matches(ARMCPU *cpu, int n, bool is_wp)
{
    CPUARMState *env = &cpu->env;
    uint64_t cr;
    int pac, hmc, ssc, wt, lbn;
    bool is_secure = arm_is_secure(env);
    int access_el = arm_current_el(env);

    if (is_wp) {
        CPUWatchpoint *wp = env->cpu_watchpoint[n];

        if (!wp || !(wp->flags & BP_WATCHPOINT_HIT)) {
            return false;
        }
        cr = env->cp15.dbgwcr[n];
        if (wp->hitattrs.user) {
            access_el = 0;
        }
    } else {
        uint64_t pc = is_a64(env) ? env->pc : env->regs[15];

        if (!env->cpu_breakpoint[n] || env->cpu_breakpoint[n]->pc != pc) {
            return false;
        }
        cr = env->cp15.dbgbcr[n];
    }
    pac = FIELD_EX64(cr, DBGWCR, PAC);
    hmc = FIELD_EX64(cr, DBGWCR, HMC);
    ssc = FIELD_EX64(cr, DBGWCR, SSC);

    switch (ssc) {
    case 0:
        break;
    case 1:
    case 3:
        if (is_secure) {
            return false;
        }
        break;
    case 2:
        if (!is_secure) {
            return false;
        }
        break;
    }

    switch (access_el) {
    case 3:
    case 2:
        if (!hmc) {
            return false;
        }
        break;
    case 1:
        if (extract32(pac, 0, 1) == 0) {
            return false;
        }
        break;
    case 0:
        if (extract32(pac, 1, 1) == 0) {
            return false;
        }
        break;
    default:
        g_assert_not_reached();
    }

    wt = FIELD_EX64(cr, DBGWCR, WT);
    lbn = FIELD_EX64(cr, DBGWCR, LBN);

    if (wt && !linked_bp_matches(cpu, lbn)) {
        return false;
    }

    return true;
}

static bool check_watchpoints(ARMCPU *cpu)
{
    CPUARMState *env = &cpu->env;
    int n;

    if (extract32(env->cp15.mdscr_el1, 15, 1) == 0
        || !arm_generate_debug_exceptions(env)) {
        return false;
    }

    for (n = 0; n < ARRAY_SIZE(env->cpu_watchpoint); n++) {
        if (bp_wp_matches(cpu, n, true)) {
            return true;
        }
    }
    return false;
}

bool arm_debug_check_breakpoint(CPUState *cs)
{
    ARMCPU *cpu = ARM_CPU(cs);
    CPUARMState *env = &cpu->env;
    target_ulong pc;
    int n;

    if (extract32(env->cp15.mdscr_el1, 15, 1) == 0
        || !arm_generate_debug_exceptions(env)) {
        return false;
    }

    if (arm_singlestep_active(env) && !(env->pstate & PSTATE_SS)) {
        return false;
    }

    pc = is_a64(env) ? env->pc : env->regs[15];
    if ((is_a64(env) || !env->thumb) && (pc & 3) != 0) {
        return false;
    }


    for (n = 0; n < ARRAY_SIZE(env->cpu_breakpoint); n++) {
        if (bp_wp_matches(cpu, n, false)) {
            return true;
        }
    }
    return false;
}

bool arm_debug_check_watchpoint(CPUState *cs, CPUWatchpoint *wp)
{
    ARMCPU *cpu = ARM_CPU(cs);

    return check_watchpoints(cpu);
}

static uint32_t arm_debug_exception_fsr(CPUARMState *env)
{
    ARMMMUFaultInfo fi = { .type = ARMFault_Debug };
    int target_el = arm_debug_target_el(env);
    bool using_lpae;

    if (arm_feature(env, ARM_FEATURE_M)) {
        using_lpae = false;
    } else if (target_el == 2 || arm_el_is_aa64(env, target_el)) {
        using_lpae = true;
    } else if (arm_feature(env, ARM_FEATURE_PMSA) &&
               arm_feature(env, ARM_FEATURE_V8)) {
        using_lpae = true;
    } else if (arm_feature(env, ARM_FEATURE_LPAE) &&
               (env->cp15.tcr_el[target_el] & TTBCR_EAE)) {
        using_lpae = true;
    } else {
        using_lpae = false;
    }

    if (using_lpae) {
        return arm_fi_to_lfsc(&fi);
    } else {
        return arm_fi_to_sfsc(&fi);
    }
}

void arm_debug_excp_handler(CPUState *cs)
{
    ARMCPU *cpu = ARM_CPU(cs);
    CPUARMState *env = &cpu->env;
    CPUWatchpoint *wp_hit = cs->watchpoint_hit;

    if (wp_hit) {
        if (wp_hit->flags & BP_CPU) {
            bool wnr = (wp_hit->flags & BP_WATCHPOINT_HIT_WRITE) != 0;

            cs->watchpoint_hit = NULL;

            env->exception.fsr = arm_debug_exception_fsr(env);
            env->exception.vaddress = wp_hit->hitaddr;
            raise_exception_debug(env, EXCP_DATA_ABORT,
                                  syn_watchpoint(0, 0, wnr));
        }
    } else {
        uint64_t pc = is_a64(env) ? env->pc : env->regs[15];

        if (!cpu_breakpoint_test(cs, pc, BP_CPU)) {
            return;
        }

        env->exception.fsr = arm_debug_exception_fsr(env);
        env->exception.vaddress = 0;
        raise_exception_debug(env, EXCP_PREFETCH_ABORT, syn_breakpoint(0));
    }
}

void HELPER(exception_bkpt_insn)(CPUARMState *env, uint32_t syndrome)
{
    int debug_el = arm_debug_target_el(env);
    int cur_el = arm_current_el(env);

    env->exception.fsr = arm_debug_exception_fsr(env);
    env->exception.vaddress = 0;
    if (debug_el < cur_el) {
        debug_el = cur_el;
    }
    raise_exception(env, EXCP_BKPT, syndrome, debug_el);
}

void HELPER(exception_swstep)(CPUARMState *env, uint32_t syndrome)
{
    raise_exception_debug(env, EXCP_UDEF, syndrome);
}

void hw_watchpoint_update(ARMCPU *cpu, int n)
{
    CPUARMState *env = &cpu->env;
    vaddr len = 0;
    vaddr wvr = env->cp15.dbgwvr[n];
    uint64_t wcr = env->cp15.dbgwcr[n];
    int mask;
    int flags = BP_CPU | BP_STOP_BEFORE_ACCESS;

    if (env->cpu_watchpoint[n]) {
        cpu_watchpoint_remove_by_ref(CPU(cpu), env->cpu_watchpoint[n]);
        env->cpu_watchpoint[n] = NULL;
    }

    if (!FIELD_EX64(wcr, DBGWCR, E)) {
        return;
    }

    switch (FIELD_EX64(wcr, DBGWCR, LSC)) {
    case 0:
        return;
    case 1:
        flags |= BP_MEM_READ;
        break;
    case 2:
        flags |= BP_MEM_WRITE;
        break;
    case 3:
        flags |= BP_MEM_ACCESS;
        break;
    }

    mask = FIELD_EX64(wcr, DBGWCR, MASK);
    if (mask == 1 || mask == 2) {
        return;
    } else if (mask) {
        len = 1ULL << mask;
        wvr &= ~(len - 1);
    } else {
        int bas = FIELD_EX64(wcr, DBGWCR, BAS);
        int basstart;

        if (extract64(wvr, 2, 1)) {
            bas &= 0xf;
        }

        if (bas == 0) {
            return;
        }

        basstart = ctz32(bas);
        len = cto32(bas >> basstart);
        wvr += basstart;
    }

    cpu_watchpoint_insert(CPU(cpu), wvr, len, flags,
                          &env->cpu_watchpoint[n]);
}

void hw_watchpoint_update_all(ARMCPU *cpu)
{
    int i;
    CPUARMState *env = &cpu->env;

    cpu_watchpoint_remove_all(CPU(cpu), BP_CPU);
    memset(env->cpu_watchpoint, 0, sizeof(env->cpu_watchpoint));

    for (i = 0; i < ARRAY_SIZE(cpu->env.cpu_watchpoint); i++) {
        hw_watchpoint_update(cpu, i);
    }
}

void hw_breakpoint_update(ARMCPU *cpu, int n)
{
    CPUARMState *env = &cpu->env;
    uint64_t bvr = env->cp15.dbgbvr[n];
    uint64_t bcr = env->cp15.dbgbcr[n];
    vaddr addr;
    int bt;
    int flags = BP_CPU;

    if (env->cpu_breakpoint[n]) {
        cpu_breakpoint_remove_by_ref(CPU(cpu), env->cpu_breakpoint[n]);
        env->cpu_breakpoint[n] = NULL;
    }

    if (!extract64(bcr, 0, 1)) {
        return;
    }

    bt = extract64(bcr, 20, 4);

    switch (bt) {
    case 4: /* unlinked address mismatch (reserved if AArch64) */
    case 5: /* linked address mismatch (reserved if AArch64) */
        qemu_log_mask(LOG_UNIMP,
                      "arm: address mismatch breakpoint types not implemented\n");
        return;
    case 0: /* unlinked address match */
    case 1: /* linked address match */
    {
        int bas = extract64(bcr, 5, 4);
        addr = bvr & ~3ULL;
        if (bas == 0) {
            return;
        }
        if (bas == 0xc) {
            addr += 2;
        }
        break;
    }
    case 2: /* unlinked context ID match */
    case 8: /* unlinked VMID match (reserved if no EL2) */
    case 10: /* unlinked context ID and VMID match (reserved if no EL2) */
        qemu_log_mask(LOG_UNIMP,
                      "arm: unlinked context breakpoint types not implemented\n");
        return;
    case 9: /* linked VMID match (reserved if no EL2) */
    case 11: /* linked context ID and VMID match (reserved if no EL2) */
    case 3: /* linked context ID match */
    default:
        return;
    }

    cpu_breakpoint_insert(CPU(cpu), addr, flags, &env->cpu_breakpoint[n]);
}

void hw_breakpoint_update_all(ARMCPU *cpu)
{
    int i;
    CPUARMState *env = &cpu->env;

    cpu_breakpoint_remove_all(CPU(cpu), BP_CPU);
    memset(env->cpu_breakpoint, 0, sizeof(env->cpu_breakpoint));

    for (i = 0; i < ARRAY_SIZE(cpu->env.cpu_breakpoint); i++) {
        hw_breakpoint_update(cpu, i);
    }
}

#if !defined(CONFIG_USER_ONLY)

vaddr arm_adjust_watchpoint_address(CPUState *cs, vaddr addr, int len)
{
    ARMCPU *cpu = ARM_CPU(cs);
    CPUARMState *env = &cpu->env;

    if (arm_sctlr_b(env)) {
        if (len == 1) {
            addr ^= 3;
        } else if (len == 2) {
            addr ^= 2;
        }
    }

    return addr;
}

#endif /* !CONFIG_USER_ONLY */
#endif /* CONFIG_TCG */

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
    ARMCPU *cpu = env_archcpu(env);
    int i = ri->crm;

    value &= ~3ULL;

    raw_write(env, ri, value);
    if (tcg_enabled()) {
        hw_watchpoint_update(cpu, i);
    }
}

static void dbgwcr_write(CPUARMState *env, const ARMCPRegInfo *ri,
                         uint64_t value)
{
    ARMCPU *cpu = env_archcpu(env);
    int i = ri->crm;

    raw_write(env, ri, value);
    if (tcg_enabled()) {
        hw_watchpoint_update(cpu, i);
    }
}

static void dbgbvr_write(CPUARMState *env, const ARMCPRegInfo *ri,
                         uint64_t value)
{
    ARMCPU *cpu = env_archcpu(env);
    int i = ri->crm;

    raw_write(env, ri, value);
    if (tcg_enabled()) {
        hw_breakpoint_update(cpu, i);
    }
}

static void dbgbcr_write(CPUARMState *env, const ARMCPRegInfo *ri,
                         uint64_t value)
{
    ARMCPU *cpu = env_archcpu(env);
    int i = ri->crm;

    value = deposit64(value, 6, 1, extract64(value, 5, 1));
    value = deposit64(value, 8, 1, extract64(value, 7, 1));

    raw_write(env, ri, value);
    if (tcg_enabled()) {
        hw_breakpoint_update(cpu, i);
    }
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
