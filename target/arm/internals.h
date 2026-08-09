
#ifndef TARGET_ARM_INTERNALS_H
#define TARGET_ARM_INTERNALS_H

#include "exec/breakpoint.h"
#include "hw/registerfields.h"
#include "syndrome.h"
#include "cpu-features.h"

#define BANK_USRSYS 0
#define BANK_SVC    1
#define BANK_ABT    2
#define BANK_UND    3
#define BANK_IRQ    4
#define BANK_FIQ    5
#define BANK_HYP    6
#define BANK_MON    7

static inline int arm_env_mmu_index(CPUARMState *env)
{
    return EX_TBFLAG_ANY(env->hflags, MMUIDX);
}

static inline bool excp_is_internal(int excp)
{
    return excp == EXCP_INTERRUPT
        || excp == EXCP_HLT
        || excp == EXCP_DEBUG
        || excp == EXCP_HALTED
        || excp == EXCP_EXCEPTION_EXIT
        || excp == EXCP_KERNEL_TRAP;
}

#define GTIMER_DEFAULT_HZ 1000000000
#define GTIMER_BACKCOMPAT_HZ 62500000

FIELD(V7M_CONTROL, NPRIV, 0, 1)
FIELD(V7M_CONTROL, SPSEL, 1, 1)
FIELD(V7M_CONTROL, FPCA, 2, 1)
FIELD(V7M_CONTROL, SFPA, 3, 1)

FIELD(V7M_EXCRET, ES, 0, 1)
FIELD(V7M_EXCRET, RES0, 1, 1)
FIELD(V7M_EXCRET, SPSEL, 2, 1)
FIELD(V7M_EXCRET, MODE, 3, 1)
FIELD(V7M_EXCRET, FTYPE, 4, 1)
FIELD(V7M_EXCRET, DCRS, 5, 1)
FIELD(V7M_EXCRET, S, 6, 1)
FIELD(V7M_EXCRET, RES1, 7, 25) /* including the must-be-1 prefix */

#define EXC_RETURN_MIN_MAGIC 0xff000000
#define FNC_RETURN_MIN_MAGIC 0xfefffffe

FIELD(DBGWCR, E, 0, 1)
FIELD(DBGWCR, PAC, 1, 2)
FIELD(DBGWCR, LSC, 3, 2)
FIELD(DBGWCR, BAS, 5, 8)
FIELD(DBGWCR, HMC, 13, 1)
FIELD(DBGWCR, SSC, 14, 2)
FIELD(DBGWCR, LBN, 16, 4)
FIELD(DBGWCR, WT, 20, 1)
FIELD(DBGWCR, MASK, 24, 5)
FIELD(DBGWCR, SSCE, 29, 1)

#define VTCR_NSW (1u << 29)
#define VTCR_NSA (1u << 30)
#define VSTCR_SW VTCR_NSW
#define VSTCR_SA VTCR_NSA

FIELD(CPACR, CP10, 20, 2)
FIELD(CPACR, CP11, 22, 2)
FIELD(CPACR, TRCDIS, 28, 1)    /* matches CPACR_EL1.TTA */
FIELD(CPACR, D32DIS, 30, 1)    /* up to v7; RAZ in v8 */
FIELD(CPACR, ASEDIS, 31, 1)

FIELD(CPACR_EL1, ZEN, 16, 2)
FIELD(CPACR_EL1, FPEN, 20, 2)
FIELD(CPACR_EL1, SMEN, 24, 2)
FIELD(CPACR_EL1, TTA, 28, 1)   /* matches CPACR.TRCDIS */

FIELD(HCPTR, TCP10, 10, 1)
FIELD(HCPTR, TCP11, 11, 1)
FIELD(HCPTR, TASE, 15, 1)
FIELD(HCPTR, TTA, 20, 1)
FIELD(HCPTR, TAM, 30, 1)       /* matches CPTR_EL2.TAM */
FIELD(HCPTR, TCPAC, 31, 1)     /* matches CPTR_EL2.TCPAC */

FIELD(CPTR_EL2, TZ, 8, 1)      /* !E2H */
FIELD(CPTR_EL2, TFP, 10, 1)    /* !E2H, matches HCPTR.TCP10 */
FIELD(CPTR_EL2, TSM, 12, 1)    /* !E2H */
FIELD(CPTR_EL2, ZEN, 16, 2)    /* E2H */
FIELD(CPTR_EL2, FPEN, 20, 2)   /* E2H */
FIELD(CPTR_EL2, SMEN, 24, 2)   /* E2H */
FIELD(CPTR_EL2, TTA, 28, 1)
FIELD(CPTR_EL2, TAM, 30, 1)    /* matches HCPTR.TAM */
FIELD(CPTR_EL2, TCPAC, 31, 1)  /* matches HCPTR.TCPAC */

FIELD(CPTR_EL3, EZ, 8, 1)
FIELD(CPTR_EL3, TFP, 10, 1)
FIELD(CPTR_EL3, ESM, 12, 1)
FIELD(CPTR_EL3, TTA, 20, 1)
FIELD(CPTR_EL3, TAM, 30, 1)
FIELD(CPTR_EL3, TCPAC, 31, 1)

#define MDCR_MTPME    (1U << 28)
#define MDCR_TDCC     (1U << 27)
#define MDCR_HLP      (1U << 26)  /* MDCR_EL2 */
#define MDCR_SCCD     (1U << 23)  /* MDCR_EL3 */
#define MDCR_HCCD     (1U << 23)  /* MDCR_EL2 */
#define MDCR_EPMAD    (1U << 21)
#define MDCR_EDAD     (1U << 20)
#define MDCR_TTRF     (1U << 19)
#define MDCR_STE      (1U << 18)  /* MDCR_EL3 */
#define MDCR_SPME     (1U << 17)  /* MDCR_EL3 */
#define MDCR_HPMD     (1U << 17)  /* MDCR_EL2 */
#define MDCR_SDD      (1U << 16)
#define MDCR_SPD      (3U << 14)
#define MDCR_TDRA     (1U << 11)
#define MDCR_TDOSA    (1U << 10)
#define MDCR_TDA      (1U << 9)
#define MDCR_TDE      (1U << 8)
#define MDCR_HPME     (1U << 7)
#define MDCR_TPM      (1U << 6)
#define MDCR_TPMCR    (1U << 5)
#define MDCR_HPMN     (0x1fU)

#define SDCR_VALID_MASK (MDCR_MTPME | MDCR_TDCC | MDCR_SCCD | \
                         MDCR_EPMAD | MDCR_EDAD | MDCR_TTRF | \
                         MDCR_STE | MDCR_SPME | MDCR_SPD)

#define TTBCR_N      (7U << 0) /* TTBCR.EAE==0 */
#define TTBCR_T0SZ   (7U << 0) /* TTBCR.EAE==1 */
#define TTBCR_PD0    (1U << 4)
#define TTBCR_PD1    (1U << 5)
#define TTBCR_EPD0   (1U << 7)
#define TTBCR_IRGN0  (3U << 8)
#define TTBCR_ORGN0  (3U << 10)
#define TTBCR_SH0    (3U << 12)
#define TTBCR_T1SZ   (3U << 16)
#define TTBCR_A1     (1U << 22)
#define TTBCR_EPD1   (1U << 23)
#define TTBCR_IRGN1  (3U << 24)
#define TTBCR_ORGN1  (3U << 26)
#define TTBCR_SH1    (1U << 28)
#define TTBCR_EAE    (1U << 31)

FIELD(VTCR, T0SZ, 0, 6)
FIELD(VTCR, SL0, 6, 2)
FIELD(VTCR, IRGN0, 8, 2)
FIELD(VTCR, ORGN0, 10, 2)
FIELD(VTCR, SH0, 12, 2)
FIELD(VTCR, TG0, 14, 2)
FIELD(VTCR, PS, 16, 3)
FIELD(VTCR, VS, 19, 1)
FIELD(VTCR, HA, 21, 1)
FIELD(VTCR, HD, 22, 1)
FIELD(VTCR, HWU59, 25, 1)
FIELD(VTCR, HWU60, 26, 1)
FIELD(VTCR, HWU61, 27, 1)
FIELD(VTCR, HWU62, 28, 1)
FIELD(VTCR, NSW, 29, 1)
FIELD(VTCR, NSA, 30, 1)
FIELD(VTCR, DS, 32, 1)
FIELD(VTCR, SL2, 33, 1)

#define HCRX_ENAS0    (1ULL << 0)
#define HCRX_ENALS    (1ULL << 1)
#define HCRX_ENASR    (1ULL << 2)
#define HCRX_FNXS     (1ULL << 3)
#define HCRX_FGTNXS   (1ULL << 4)
#define HCRX_SMPME    (1ULL << 5)
#define HCRX_TALLINT  (1ULL << 6)
#define HCRX_VINMI    (1ULL << 7)
#define HCRX_VFNMI    (1ULL << 8)
#define HCRX_CMOW     (1ULL << 9)
#define HCRX_MCE2     (1ULL << 10)
#define HCRX_MSCEN    (1ULL << 11)

#define HPFAR_NS      (1ULL << 63)

#define HSTR_TTEE (1 << 16)
#define HSTR_TJDBX (1 << 17)

FIELD(CNTHCTL, EL0PCTEN_E2H1, 0, 1)
FIELD(CNTHCTL, EL0VCTEN_E2H1, 1, 1)
FIELD(CNTHCTL, EL1PCTEN_E2H0, 0, 1)
FIELD(CNTHCTL, EL1PCEN_E2H0, 1, 1)
FIELD(CNTHCTL, EVNTEN, 2, 1)
FIELD(CNTHCTL, EVNTDIR, 3, 1)
FIELD(CNTHCTL, EVNTI, 4, 4)
FIELD(CNTHCTL, EL0VTEN, 8, 1)
FIELD(CNTHCTL, EL0PTEN, 9, 1)
FIELD(CNTHCTL, EL1PCTEN_E2H1, 10, 1)
FIELD(CNTHCTL, EL1PTEN, 11, 1)
FIELD(CNTHCTL, ECV, 12, 1)
FIELD(CNTHCTL, EL1TVT, 13, 1)
FIELD(CNTHCTL, EL1TVCT, 14, 1)
FIELD(CNTHCTL, EL1NVPCT, 15, 1)
FIELD(CNTHCTL, EL1NVVCT, 16, 1)
FIELD(CNTHCTL, EVNTIS, 17, 1)
FIELD(CNTHCTL, CNTVMASK, 18, 1)
FIELD(CNTHCTL, CNTPMASK, 19, 1)

#define M_FAKE_FSR_NSC_EXEC 0xf /* NS executing in S&NSC memory */
#define M_FAKE_FSR_SFAULT 0xe /* SecureFault INVTRAN, INVEP or AUVIOL */

G_NORETURN void raise_exception(CPUARMState *env, uint32_t excp,
                                uint32_t syndrome, uint32_t target_el);

G_NORETURN void raise_exception_ra(CPUARMState *env, uint32_t excp,
                                      uint32_t syndrome, uint32_t target_el,
                                      uintptr_t ra);

static inline unsigned int aarch64_banked_spsr_index(unsigned int el)
{
    static const unsigned int map[4] = {
        [1] = BANK_SVC, /* EL1.  */
        [2] = BANK_HYP, /* EL2.  */
        [3] = BANK_MON, /* EL3.  */
    };
    assert(el >= 1 && el <= 3);
    return map[el];
}

static inline int bank_number(int mode)
{
    switch (mode) {
    case ARM_CPU_MODE_USR:
    case ARM_CPU_MODE_SYS:
        return BANK_USRSYS;
    case ARM_CPU_MODE_SVC:
        return BANK_SVC;
    case ARM_CPU_MODE_ABT:
        return BANK_ABT;
    case ARM_CPU_MODE_UND:
        return BANK_UND;
    case ARM_CPU_MODE_IRQ:
        return BANK_IRQ;
    case ARM_CPU_MODE_FIQ:
        return BANK_FIQ;
    case ARM_CPU_MODE_HYP:
        return BANK_HYP;
    case ARM_CPU_MODE_MON:
        return BANK_MON;
    }
    g_assert_not_reached();
}

static inline int r14_bank_number(int mode)
{
    return (mode == ARM_CPU_MODE_HYP) ? BANK_USRSYS : bank_number(mode);
}

void arm_cpu_register(const ARMCPUInfo *info);
void aarch64_cpu_register(const ARMCPUInfo *info);

void register_cp_regs_for_features(ARMCPU *cpu);
void init_cpreg_list(ARMCPU *cpu);

typedef enum ARMFPRounding {
    FPROUNDING_TIEEVEN,
    FPROUNDING_POSINF,
    FPROUNDING_NEGINF,
    FPROUNDING_ZERO,
    FPROUNDING_TIEAWAY,
    FPROUNDING_ODD
} ARMFPRounding;

extern const FloatRoundMode arm_rmode_to_sf_map[6];

static inline FloatRoundMode arm_rmode_to_sf(ARMFPRounding rmode)
{
    assert((unsigned)rmode < ARRAY_SIZE(arm_rmode_to_sf_map));
    return arm_rmode_to_sf_map[rmode];
}

static inline bool arm_scr_rw_eff(CPUARMState *env)
{
    ARMCPU *cpu = env_archcpu(env);

    if (env->cp15.scr_el3 & SCR_RW) {
        return true;
    }
    if (env->cp15.scr_el3 & SCR_NS) {
        return arm_feature(env, ARM_FEATURE_EL2) &&
            !cpu_isar_feature(aa64_aa32_el2, cpu);
    } else {
        return env->cp15.scr_el3 & SCR_EEL2;
    }
}

static inline bool arm_el_is_aa64(CPUARMState *env, int el)
{
    assert(el >= 1 && el <= 3);
    bool aa64 = arm_feature(env, ARM_FEATURE_AARCH64);

    if (el == 3) {
        return aa64;
    }

    if (arm_feature(env, ARM_FEATURE_EL3)) {
        aa64 = aa64 && arm_scr_rw_eff(env);
    }

    if (el == 2) {
        return aa64;
    }

    if (arm_is_el2_enabled(env)) {
        aa64 = aa64 && (env->cp15.hcr_el2 & HCR_RW);
    }

    return aa64;
}

static inline int arm_current_el(CPUARMState *env)
{
    if (arm_feature(env, ARM_FEATURE_M)) {
        return arm_v7m_is_handler_mode(env) ||
            !(env->v7m.control[env->v7m.secure] & 1);
    }

    if (is_a64(env)) {
        return extract32(env->pstate, 2, 2);
    }

    switch (env->uncached_cpsr & 0x1f) {
    case ARM_CPU_MODE_USR:
        return 0;
    case ARM_CPU_MODE_HYP:
        return 2;
    case ARM_CPU_MODE_MON:
        return 3;
    default:
        if (arm_is_secure(env) && !arm_el_is_aa64(env, 3)) {
            return 3;
        }

        return 1;
    }
}

static inline bool arm_cpu_data_is_big_endian_a32(CPUARMState *env,
                                                  bool sctlr_b)
{
#ifdef CONFIG_USER_ONLY
    if (sctlr_b) {
        return true;
    }
#endif
    return env->uncached_cpsr & CPSR_E;
}

static inline bool arm_cpu_data_is_big_endian_a64(int el, uint64_t sctlr)
{
    return sctlr & (el ? SCTLR_EE : SCTLR_E0E);
}

static inline bool arm_cpu_data_is_big_endian(CPUARMState *env)
{
    if (!is_a64(env)) {
        return arm_cpu_data_is_big_endian_a32(env, arm_sctlr_b(env));
    } else {
        int cur_el = arm_current_el(env);
        uint64_t sctlr = arm_sctlr(env, cur_el);
        return arm_cpu_data_is_big_endian_a64(cur_el, sctlr);
    }
}

#ifdef CONFIG_USER_ONLY
static inline bool arm_cpu_bswap_data(CPUARMState *env)
{
    return TARGET_BIG_ENDIAN ^ arm_cpu_data_is_big_endian(env);
}
#endif

static inline void aarch64_save_sp(CPUARMState *env, int el)
{
    if (env->pstate & PSTATE_SP) {
        env->sp_el[el] = env->xregs[31];
    } else {
        env->sp_el[0] = env->xregs[31];
    }
}

static inline void aarch64_restore_sp(CPUARMState *env, int el)
{
    if (env->pstate & PSTATE_SP) {
        env->xregs[31] = env->sp_el[el];
    } else {
        env->xregs[31] = env->sp_el[0];
    }
}

static inline void update_spsel(CPUARMState *env, uint32_t imm)
{
    unsigned int cur_el = arm_current_el(env);
    if (!((imm ^ env->pstate) & PSTATE_SP)) {
        return;
    }
    aarch64_save_sp(env, cur_el);
    env->pstate = deposit32(env->pstate, 0, 1, imm);

    assert(cur_el >= 1 && cur_el <= 3);
    aarch64_restore_sp(env, cur_el);
}

unsigned int arm_pamax(ARMCPU *cpu);

uint8_t round_down_to_parange_index(uint8_t bit_size);

uint8_t round_down_to_parange_bit_size(uint8_t bit_size);

static inline bool extended_addresses_enabled(CPUARMState *env)
{
    uint64_t tcr = env->cp15.tcr_el[arm_is_secure(env) ? 3 : 1];
    if (arm_feature(env, ARM_FEATURE_PMSA) &&
        arm_feature(env, ARM_FEATURE_V8)) {
        return true;
    }
    return arm_el_is_aa64(env, 1) ||
           (arm_feature(env, ARM_FEATURE_LPAE) && (tcr & TTBCR_EAE));
}

static inline void arm_clear_exclusive(CPUARMState *env)
{
    env->exclusive_addr = -1;
}

typedef enum ARMFaultType {
    ARMFault_None,
    ARMFault_AccessFlag,
    ARMFault_Alignment,
    ARMFault_Background,
    ARMFault_Domain,
    ARMFault_Permission,
    ARMFault_Translation,
    ARMFault_AddressSize,
    ARMFault_SyncExternal,
    ARMFault_SyncExternalOnWalk,
    ARMFault_SyncParity,
    ARMFault_SyncParityOnWalk,
    ARMFault_AsyncParity,
    ARMFault_AsyncExternal,
    ARMFault_Debug,
    ARMFault_TLBConflict,
    ARMFault_UnsuppAtomicUpdate,
    ARMFault_Lockdown,
    ARMFault_Exclusive,
    ARMFault_ICacheMaint,
    ARMFault_QEMU_NSCExec, /* v8M: NS executing in S&NSC memory */
    ARMFault_QEMU_SFault, /* v8M: SecureFault INVTRAN, INVEP or AUVIOL */
    ARMFault_GPCFOnWalk,
    ARMFault_GPCFOnOutput,
} ARMFaultType;

typedef enum ARMGPCF {
    GPCF_None,
    GPCF_AddressSize,
    GPCF_Walk,
    GPCF_EABT,
    GPCF_Fail,
} ARMGPCF;

typedef struct ARMMMUFaultInfo ARMMMUFaultInfo;
struct ARMMMUFaultInfo {
    ARMFaultType type;
    ARMGPCF gpcf;
    target_ulong s2addr;
    target_ulong paddr;
    ARMSecuritySpace paddr_space;
    int level;
    int domain;
    bool stage2;
    bool s1ptw;
    bool s1ns;
    bool ea;
};

static inline uint32_t arm_fi_to_sfsc(ARMMMUFaultInfo *fi)
{
    uint32_t fsc;

    switch (fi->type) {
    case ARMFault_None:
        return 0;
    case ARMFault_AccessFlag:
        fsc = fi->level == 1 ? 0x3 : 0x6;
        break;
    case ARMFault_Alignment:
        fsc = 0x1;
        break;
    case ARMFault_Permission:
        fsc = fi->level == 1 ? 0xd : 0xf;
        break;
    case ARMFault_Domain:
        fsc = fi->level == 1 ? 0x9 : 0xb;
        break;
    case ARMFault_Translation:
        fsc = fi->level == 1 ? 0x5 : 0x7;
        break;
    case ARMFault_SyncExternal:
        fsc = 0x8 | (fi->ea << 12);
        break;
    case ARMFault_SyncExternalOnWalk:
        fsc = fi->level == 1 ? 0xc : 0xe;
        fsc |= (fi->ea << 12);
        break;
    case ARMFault_SyncParity:
        fsc = 0x409;
        break;
    case ARMFault_SyncParityOnWalk:
        fsc = fi->level == 1 ? 0x40c : 0x40e;
        break;
    case ARMFault_AsyncParity:
        fsc = 0x408;
        break;
    case ARMFault_AsyncExternal:
        fsc = 0x406 | (fi->ea << 12);
        break;
    case ARMFault_Debug:
        fsc = 0x2;
        break;
    case ARMFault_TLBConflict:
        fsc = 0x400;
        break;
    case ARMFault_Lockdown:
        fsc = 0x404;
        break;
    case ARMFault_Exclusive:
        fsc = 0x405;
        break;
    case ARMFault_ICacheMaint:
        fsc = 0x4;
        break;
    case ARMFault_Background:
        fsc = 0x0;
        break;
    case ARMFault_QEMU_NSCExec:
        fsc = M_FAKE_FSR_NSC_EXEC;
        break;
    case ARMFault_QEMU_SFault:
        fsc = M_FAKE_FSR_SFAULT;
        break;
    default:
        g_assert_not_reached();
    }

    fsc |= (fi->domain << 4);
    return fsc;
}

static inline uint32_t arm_fi_to_lfsc(ARMMMUFaultInfo *fi)
{
    uint32_t fsc;

    switch (fi->type) {
    case ARMFault_None:
        return 0;
    case ARMFault_AddressSize:
        assert(fi->level >= -1 && fi->level <= 3);
        if (fi->level < 0) {
            fsc = 0b101001;
        } else {
            fsc = fi->level;
        }
        break;
    case ARMFault_AccessFlag:
        assert(fi->level >= 0 && fi->level <= 3);
        fsc = 0b001000 | fi->level;
        break;
    case ARMFault_Permission:
        assert(fi->level >= 0 && fi->level <= 3);
        fsc = 0b001100 | fi->level;
        break;
    case ARMFault_Translation:
        assert(fi->level >= -1 && fi->level <= 3);
        if (fi->level < 0) {
            fsc = 0b101011;
        } else {
            fsc = 0b000100 | fi->level;
        }
        break;
    case ARMFault_SyncExternal:
        fsc = 0x10 | (fi->ea << 12);
        break;
    case ARMFault_SyncExternalOnWalk:
        assert(fi->level >= -1 && fi->level <= 3);
        if (fi->level < 0) {
            fsc = 0b010011;
        } else {
            fsc = 0b010100 | fi->level;
        }
        fsc |= fi->ea << 12;
        break;
    case ARMFault_SyncParity:
        fsc = 0x18;
        break;
    case ARMFault_SyncParityOnWalk:
        assert(fi->level >= -1 && fi->level <= 3);
        if (fi->level < 0) {
            fsc = 0b011011;
        } else {
            fsc = 0b011100 | fi->level;
        }
        break;
    case ARMFault_AsyncParity:
        fsc = 0x19;
        break;
    case ARMFault_AsyncExternal:
        fsc = 0x11 | (fi->ea << 12);
        break;
    case ARMFault_Alignment:
        fsc = 0x21;
        break;
    case ARMFault_Debug:
        fsc = 0x22;
        break;
    case ARMFault_TLBConflict:
        fsc = 0x30;
        break;
    case ARMFault_UnsuppAtomicUpdate:
        fsc = 0x31;
        break;
    case ARMFault_Lockdown:
        fsc = 0x34;
        break;
    case ARMFault_Exclusive:
        fsc = 0x35;
        break;
    case ARMFault_GPCFOnWalk:
        assert(fi->level >= -1 && fi->level <= 3);
        if (fi->level < 0) {
            fsc = 0b100011;
        } else {
            fsc = 0b100100 | fi->level;
        }
        break;
    case ARMFault_GPCFOnOutput:
        fsc = 0b101000;
        break;
    default:
        g_assert_not_reached();
    }

    fsc |= 1 << 9;
    return fsc;
}

static inline bool arm_extabort_type(MemTxResult result)
{
    return result != MEMTX_DECODE_ERROR;
}

static inline int arm_to_core_mmu_idx(ARMMMUIdx mmu_idx)
{
    return mmu_idx & ARM_MMU_IDX_COREIDX_MASK;
}

static inline ARMMMUIdx core_to_arm_mmu_idx(CPUARMState *env, int mmu_idx)
{
    if (arm_feature(env, ARM_FEATURE_M)) {
        return mmu_idx | ARM_MMU_IDX_M;
    } else {
        return mmu_idx | ARM_MMU_IDX_A;
    }
}

static inline ARMMMUIdx core_to_aa64_mmu_idx(int mmu_idx)
{
    return mmu_idx | ARM_MMU_IDX_A;
}

int arm_mmu_idx_to_el(ARMMMUIdx mmu_idx);

ARMMMUIdx arm_v7m_mmu_idx_for_secstate(CPUARMState *env, bool secstate);

bool arm_s1_regime_using_lpae_format(CPUARMState *env, ARMMMUIdx mmu_idx);

static inline bool regime_has_2_ranges(ARMMMUIdx mmu_idx)
{
    switch (mmu_idx) {
    case ARMMMUIdx_Stage1_E0:
    case ARMMMUIdx_Stage1_E1:
    case ARMMMUIdx_Stage1_E1_PAN:
    case ARMMMUIdx_E10_0:
    case ARMMMUIdx_E10_1:
    case ARMMMUIdx_E10_1_PAN:
    case ARMMMUIdx_E20_0:
    case ARMMMUIdx_E20_2:
    case ARMMMUIdx_E20_2_PAN:
        return true;
    default:
        return false;
    }
}

static inline bool regime_is_pan(CPUARMState *env, ARMMMUIdx mmu_idx)
{
    switch (mmu_idx) {
    case ARMMMUIdx_Stage1_E1_PAN:
    case ARMMMUIdx_E10_1_PAN:
    case ARMMMUIdx_E20_2_PAN:
    case ARMMMUIdx_E30_3_PAN:
        return true;
    default:
        return false;
    }
}

static inline bool regime_is_stage2(ARMMMUIdx mmu_idx)
{
    return mmu_idx == ARMMMUIdx_Stage2 || mmu_idx == ARMMMUIdx_Stage2_S;
}

static inline uint32_t regime_el(CPUARMState *env, ARMMMUIdx mmu_idx)
{
    switch (mmu_idx) {
    case ARMMMUIdx_E20_0:
    case ARMMMUIdx_E20_2:
    case ARMMMUIdx_E20_2_PAN:
    case ARMMMUIdx_Stage2:
    case ARMMMUIdx_Stage2_S:
    case ARMMMUIdx_E2:
        return 2;
    case ARMMMUIdx_E3:
    case ARMMMUIdx_E30_0:
    case ARMMMUIdx_E30_3_PAN:
        return 3;
    case ARMMMUIdx_E10_0:
    case ARMMMUIdx_Stage1_E0:
    case ARMMMUIdx_Stage1_E1:
    case ARMMMUIdx_Stage1_E1_PAN:
    case ARMMMUIdx_E10_1:
    case ARMMMUIdx_E10_1_PAN:
    case ARMMMUIdx_MPrivNegPri:
    case ARMMMUIdx_MUserNegPri:
    case ARMMMUIdx_MPriv:
    case ARMMMUIdx_MUser:
    case ARMMMUIdx_MSPrivNegPri:
    case ARMMMUIdx_MSUserNegPri:
    case ARMMMUIdx_MSPriv:
    case ARMMMUIdx_MSUser:
        return 1;
    default:
        g_assert_not_reached();
    }
}

static inline bool regime_is_user(CPUARMState *env, ARMMMUIdx mmu_idx)
{
    switch (mmu_idx) {
    case ARMMMUIdx_E10_0:
    case ARMMMUIdx_E20_0:
    case ARMMMUIdx_E30_0:
    case ARMMMUIdx_Stage1_E0:
    case ARMMMUIdx_MUser:
    case ARMMMUIdx_MSUser:
    case ARMMMUIdx_MUserNegPri:
    case ARMMMUIdx_MSUserNegPri:
        return true;
    default:
        return false;
    }
}

static inline uint64_t regime_sctlr(CPUARMState *env, ARMMMUIdx mmu_idx)
{
    return env->cp15.sctlr_el[regime_el(env, mmu_idx)];
}

#define VTCR_SHARED_FIELD_MASK \
    (R_VTCR_IRGN0_MASK | R_VTCR_ORGN0_MASK | R_VTCR_SH0_MASK | \
     R_VTCR_PS_MASK | R_VTCR_VS_MASK | R_VTCR_HA_MASK | R_VTCR_HD_MASK | \
     R_VTCR_DS_MASK)

static inline uint64_t regime_tcr(CPUARMState *env, ARMMMUIdx mmu_idx)
{
    if (mmu_idx == ARMMMUIdx_Stage2) {
        return env->cp15.vtcr_el2;
    }
    if (mmu_idx == ARMMMUIdx_Stage2_S) {
        uint64_t v = env->cp15.vstcr_el2 & ~VTCR_SHARED_FIELD_MASK;
        v |= env->cp15.vtcr_el2 & VTCR_SHARED_FIELD_MASK;
        return v;
    }
    return env->cp15.tcr_el[regime_el(env, mmu_idx)];
}

static inline bool regime_using_lpae_format(CPUARMState *env, ARMMMUIdx mmu_idx)
{
    int el = regime_el(env, mmu_idx);
    if (el == 2 || arm_el_is_aa64(env, el)) {
        return true;
    }
    if (arm_feature(env, ARM_FEATURE_PMSA) &&
        arm_feature(env, ARM_FEATURE_V8)) {
        return true;
    }
    if (arm_feature(env, ARM_FEATURE_LPAE)
        && (regime_tcr(env, mmu_idx) & TTBCR_EAE)) {
        return true;
    }
    return false;
}

static inline int arm_num_brps(ARMCPU *cpu)
{
    if (arm_feature(&cpu->env, ARM_FEATURE_AARCH64)) {
        return FIELD_EX64(cpu->isar.id_aa64dfr0, ID_AA64DFR0, BRPS) + 1;
    } else {
        return FIELD_EX32(cpu->isar.dbgdidr, DBGDIDR, BRPS) + 1;
    }
}

static inline int arm_num_wrps(ARMCPU *cpu)
{
    if (arm_feature(&cpu->env, ARM_FEATURE_AARCH64)) {
        return FIELD_EX64(cpu->isar.id_aa64dfr0, ID_AA64DFR0, WRPS) + 1;
    } else {
        return FIELD_EX32(cpu->isar.dbgdidr, DBGDIDR, WRPS) + 1;
    }
}

static inline int arm_num_ctx_cmps(ARMCPU *cpu)
{
    if (arm_feature(&cpu->env, ARM_FEATURE_AARCH64)) {
        return FIELD_EX64(cpu->isar.id_aa64dfr0, ID_AA64DFR0, CTX_CMPS) + 1;
    } else {
        return FIELD_EX32(cpu->isar.dbgdidr, DBGDIDR, CTX_CMPS) + 1;
    }
}

static inline bool v7m_using_psp(CPUARMState *env)
{
    return !arm_v7m_is_handler_mode(env) &&
        env->v7m.control[env->v7m.secure] & R_V7M_CONTROL_SPSEL_MASK;
}

static inline uint32_t v7m_sp_limit(CPUARMState *env)
{
    if (v7m_using_psp(env)) {
        return env->v7m.psplim[env->v7m.secure];
    } else {
        return env->v7m.msplim[env->v7m.secure];
    }
}

static inline bool v7m_cpacr_pass(CPUARMState *env,
                                  bool is_secure, bool is_priv)
{
    switch (extract32(env->v7m.cpacr[is_secure], 20, 2)) {
    case 0:
    case 2: /* UNPREDICTABLE: we treat like 0 */
        return false;
    case 1:
        return is_priv;
    case 3:
        return true;
    default:
        g_assert_not_reached();
    }
}

static inline const char *aarch32_mode_name(uint32_t psr)
{
    static const char cpu_mode_names[16][4] = {
        "usr", "fiq", "irq", "svc", "???", "???", "mon", "abt",
        "???", "???", "hyp", "und", "???", "???", "???", "sys"
    };

    return cpu_mode_names[psr & 0xf];
}

void arm_cpu_update_virq(ARMCPU *cpu);

void arm_cpu_update_vfiq(ARMCPU *cpu);

void arm_cpu_update_vinmi(ARMCPU *cpu);

void arm_cpu_update_vfnmi(ARMCPU *cpu);

void arm_cpu_update_vserr(ARMCPU *cpu);

ARMMMUIdx arm_mmu_idx_el(CPUARMState *env, int el);

ARMMMUIdx arm_mmu_idx(CPUARMState *env);

#ifdef CONFIG_USER_ONLY
static inline ARMMMUIdx stage_1_mmu_idx(ARMMMUIdx mmu_idx)
{
    return ARMMMUIdx_Stage1_E0;
}
static inline ARMMMUIdx arm_stage1_mmu_idx(CPUARMState *env)
{
    return ARMMMUIdx_Stage1_E0;
}
#else
ARMMMUIdx stage_1_mmu_idx(ARMMMUIdx mmu_idx);
ARMMMUIdx arm_stage1_mmu_idx(CPUARMState *env);
#endif

static inline bool arm_mmu_idx_is_stage1_of_2(ARMMMUIdx mmu_idx)
{
    switch (mmu_idx) {
    case ARMMMUIdx_Stage1_E0:
    case ARMMMUIdx_Stage1_E1:
    case ARMMMUIdx_Stage1_E1_PAN:
        return true;
    default:
        return false;
    }
}

static inline uint32_t aarch32_cpsr_valid_mask(uint64_t features,
                                               const ARMISARegisters *id)
{
    uint32_t valid = CPSR_M | CPSR_AIF | CPSR_IL | CPSR_NZCV;

    if ((features >> ARM_FEATURE_V4T) & 1) {
        valid |= CPSR_T;
    }
    if ((features >> ARM_FEATURE_V5) & 1) {
        valid |= CPSR_Q; /* V5TE in reality*/
    }
    if ((features >> ARM_FEATURE_V6) & 1) {
        valid |= CPSR_E | CPSR_GE;
    }
    if ((features >> ARM_FEATURE_THUMB2) & 1) {
        valid |= CPSR_IT;
    }
    if (isar_feature_aa32_jazelle(id)) {
        valid |= CPSR_J;
    }
    if (isar_feature_aa32_pan(id)) {
        valid |= CPSR_PAN;
    }
    if (isar_feature_aa32_dit(id)) {
        valid |= CPSR_DIT;
    }
    if (isar_feature_aa32_ssbs(id)) {
        valid |= CPSR_SSBS;
    }

    return valid;
}

static inline uint32_t aarch64_pstate_valid_mask(const ARMISARegisters *id)
{
    uint32_t valid;

    valid = PSTATE_M | PSTATE_DAIF | PSTATE_IL | PSTATE_SS | PSTATE_NZCV;
    if (isar_feature_aa64_bti(id)) {
        valid |= PSTATE_BTYPE;
    }
    if (isar_feature_aa64_pan(id)) {
        valid |= PSTATE_PAN;
    }
    if (isar_feature_aa64_uao(id)) {
        valid |= PSTATE_UAO;
    }
    if (isar_feature_aa64_dit(id)) {
        valid |= PSTATE_DIT;
    }
    if (isar_feature_aa64_ssbs(id)) {
        valid |= PSTATE_SSBS;
    }
    if (isar_feature_aa64_mte(id)) {
        valid |= PSTATE_TCO;
    }
    if (isar_feature_aa64_nmi(id)) {
        valid |= PSTATE_ALLINT;
    }

    return valid;
}

typedef enum ARMGranuleSize {
    Gran4K,
    Gran64K,
    Gran16K,
    GranInvalid,
} ARMGranuleSize;

static inline int arm_granule_bits(ARMGranuleSize gran)
{
    switch (gran) {
    case Gran64K:
        return 16;
    case Gran16K:
        return 14;
    case Gran4K:
        return 12;
    default:
        g_assert_not_reached();
    }
}

typedef struct ARMVAParameters {
    unsigned tsz    : 8;
    unsigned ps     : 3;
    unsigned sh     : 2;
    unsigned select : 1;
    bool tbi        : 1;
    bool epd        : 1;
    bool hpd        : 1;
    bool tsz_oob    : 1;  /* tsz has been clamped to legal range */
    bool ds         : 1;
    bool ha         : 1;
    bool hd         : 1;
    ARMGranuleSize gran : 2;
} ARMVAParameters;

ARMVAParameters aa64_va_parameters(CPUARMState *env, uint64_t va,
                                   ARMMMUIdx mmu_idx, bool data,
                                   bool el1_is_aa32);

int aa64_va_parameter_tbi(uint64_t tcr, ARMMMUIdx mmu_idx);
int aa64_va_parameter_tbid(uint64_t tcr, ARMMMUIdx mmu_idx);
int aa64_va_parameter_tcma(uint64_t tcr, ARMMMUIdx mmu_idx);

static inline bool allocation_tag_access_enabled(CPUARMState *env, int el,
                                                 uint64_t sctlr)
{
    if (el < 3
        && arm_feature(env, ARM_FEATURE_EL3)
        && !(env->cp15.scr_el3 & SCR_ATA)) {
        return false;
    }
    if (el < 2 && arm_is_el2_enabled(env)) {
        uint64_t hcr = arm_hcr_el2_eff(env);
        if (!(hcr & HCR_ATA) && (!(hcr & HCR_E2H) || !(hcr & HCR_TGE))) {
            return false;
        }
    }
    sctlr &= (el == 0 ? SCTLR_ATA0 : SCTLR_ATA);
    return sctlr != 0;
}

#ifndef CONFIG_USER_ONLY

typedef struct V8M_SAttributes {
    bool subpage; /* true if these attrs don't cover the whole TARGET_PAGE */
    bool ns;
    bool nsc;
    uint8_t sregion;
    bool srvalid;
    uint8_t iregion;
    bool irvalid;
} V8M_SAttributes;

void v8m_security_lookup(CPUARMState *env, uint32_t address,
                         MMUAccessType access_type, ARMMMUIdx mmu_idx,
                         bool secure, V8M_SAttributes *sattrs);

typedef struct ARMCacheAttrs {
    unsigned int attrs:8;
    unsigned int shareability:2; /* as in the SH field of the VMSAv8-64 PTEs */
    bool is_s2_format:1;
} ARMCacheAttrs;

typedef struct GetPhysAddrResult {
    CPUTLBEntryFull f;
    ARMCacheAttrs cacheattrs;
} GetPhysAddrResult;

bool get_phys_addr(CPUARMState *env, vaddr address,
                   MMUAccessType access_type, MemOp memop, ARMMMUIdx mmu_idx,
                   GetPhysAddrResult *result, ARMMMUFaultInfo *fi)
    __attribute__((nonnull));

bool get_phys_addr_with_space_nogpc(CPUARMState *env, vaddr address,
                                    MMUAccessType access_type, MemOp memop,
                                    ARMMMUIdx mmu_idx, ARMSecuritySpace space,
                                    GetPhysAddrResult *result,
                                    ARMMMUFaultInfo *fi)
    __attribute__((nonnull));

bool pmsav8_mpu_lookup(CPUARMState *env, uint32_t address,
                       MMUAccessType access_type, ARMMMUIdx mmu_idx,
                       bool is_secure, GetPhysAddrResult *result,
                       ARMMMUFaultInfo *fi, uint32_t *mregion);

#endif /* !CONFIG_USER_ONLY */

enum MVEECIState {
    ECI_NONE = 0, /* No completed beats */
    ECI_A0 = 1, /* Completed: A0 */
    ECI_A0A1 = 2, /* Completed: A0, A1 */
    ECI_A0A1A2 = 4, /* Completed: A0, A1, A2 */
    ECI_A0A1A2B0 = 5, /* Completed: A0, A1, A2, B0 */
};

#define PMCRN_MASK  0xf800
#define PMCRN_SHIFT 11
#define PMCRLP  0x80
#define PMCRLC  0x40
#define PMCRDP  0x20
#define PMCRX   0x10
#define PMCRD   0x8
#define PMCRC   0x4
#define PMCRP   0x2
#define PMCRE   0x1
#define PMCR_WRITABLE_MASK (PMCRLP | PMCRLC | PMCRDP | PMCRX | PMCRD | PMCRE)

#define PMXEVTYPER_P          0x80000000
#define PMXEVTYPER_U          0x40000000
#define PMXEVTYPER_NSK        0x20000000
#define PMXEVTYPER_NSU        0x10000000
#define PMXEVTYPER_NSH        0x08000000
#define PMXEVTYPER_M          0x04000000
#define PMXEVTYPER_MT         0x02000000
#define PMXEVTYPER_EVTCOUNT   0x0000ffff
#define PMXEVTYPER_MASK       (PMXEVTYPER_P | PMXEVTYPER_U | PMXEVTYPER_NSK | \
                               PMXEVTYPER_NSU | PMXEVTYPER_NSH | \
                               PMXEVTYPER_M | PMXEVTYPER_MT | \
                               PMXEVTYPER_EVTCOUNT)

#define PMCCFILTR             0xf8000000
#define PMCCFILTR_M           PMXEVTYPER_M
#define PMCCFILTR_EL0         (PMCCFILTR | PMCCFILTR_M)

static inline uint32_t pmu_num_counters(CPUARMState *env)
{
    ARMCPU *cpu = env_archcpu(env);

    return (cpu->isar.reset_pmcr_el0 & PMCRN_MASK) >> PMCRN_SHIFT;
}

static inline uint64_t pmu_counter_mask(CPUARMState *env)
{
  return (1ULL << 31) | ((1ULL << pmu_num_counters(env)) - 1);
}

#ifdef TARGET_AARCH64
void arm_cpu_sve_finalize(ARMCPU *cpu, Error **errp);
void arm_cpu_sme_finalize(ARMCPU *cpu, Error **errp);
void arm_cpu_pauth_finalize(ARMCPU *cpu, Error **errp);
void arm_cpu_lpa2_finalize(ARMCPU *cpu, Error **errp);
void aarch64_add_pauth_properties(Object *obj);
void aarch64_add_sve_properties(Object *obj);
void aarch64_add_sme_properties(Object *obj);
#endif

uint32_t arm_v7m_mrs_control(CPUARMState *env, uint32_t secure);

uint32_t *arm_v7m_get_sp_ptr(CPUARMState *env, bool secure,
                             bool threadmode, bool spsel);

bool el_is_in_host(CPUARMState *env, int el);

void aa32_max_features(ARMCPU *cpu);
static inline uint64_t pauth_ptr_mask(ARMVAParameters param)
{
    int bot_pac_bit = 64 - param.tsz;
    int top_pac_bit = 64 - 8 * param.tbi;

    return MAKE_64BIT_MASK(bot_pac_bit, top_pac_bit - bot_pac_bit);
}

void define_debug_regs(ARMCPU *cpu);

static inline uint64_t arm_mdcr_el2_eff(CPUARMState *env)
{
    return arm_is_el2_enabled(env) ? env->cp15.mdcr_el2 : 0;
}

#define SVE_VQ_POW2_MAP                                 \
    ((1 << (1 - 1)) | (1 << (2 - 1)) |                  \
     (1 << (4 - 1)) | (1 << (8 - 1)) | (1 << (16 - 1)))

static inline bool arm_fgt_active(CPUARMState *env, int el)
{
    return cpu_isar_feature(aa64_fgt, env_archcpu(env)) &&
        el < 2 && arm_is_el2_enabled(env) &&
        arm_el_is_aa64(env, 1) &&
        (arm_hcr_el2_eff(env) & (HCR_E2H | HCR_TGE)) != (HCR_E2H | HCR_TGE) &&
        (!arm_feature(env, ARM_FEATURE_EL3) || (env->cp15.scr_el3 & SCR_FGTEN));
}



typedef struct {
    uint64_t bcr;
    uint64_t bvr;
} HWBreakpoint;

typedef struct {
    uint64_t wcr;
    uint64_t wvr;
    CPUWatchpoint details;
} HWWatchpoint;

extern int max_hw_bps, max_hw_wps;
extern GArray *hw_breakpoints, *hw_watchpoints;

#define cur_hw_wps      (hw_watchpoints->len)
#define cur_hw_bps      (hw_breakpoints->len)
#define get_hw_bp(i)    (&g_array_index(hw_breakpoints, HWBreakpoint, i))
#define get_hw_wp(i)    (&g_array_index(hw_watchpoints, HWWatchpoint, i))

bool find_hw_breakpoint(CPUState *cpu, target_ulong pc);
int insert_hw_breakpoint(target_ulong pc);
int delete_hw_breakpoint(target_ulong pc);

bool check_watchpoint_in_range(int i, target_ulong addr);
CPUWatchpoint *find_hw_watchpoint(CPUState *cpu, target_ulong addr);
int insert_hw_watchpoint(target_ulong addr, target_ulong len, int type);
int delete_hw_watchpoint(target_ulong addr, target_ulong len, int type);

uint64_t gt_get_countervalue(CPUARMState *env);
uint64_t gt_direct_access_timer_offset(CPUARMState *env, int timeridx);

int alle1_tlbmask(CPUARMState *env);

#endif
