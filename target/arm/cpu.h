
#ifndef ARM_CPU_H
#define ARM_CPU_H

#include "qemu/cpu-float.h"
#include "hw/registerfields.h"
#include "cpu-qom.h"
#include "exec/cpu-defs.h"
#include "exec/page-protection.h"
#include "qapi/qapi-types-common.h"
#include "target/arm/multiprocessing.h"
#include "target/arm/gtimer.h"

#define EXCP_UDEF            1   /* undefined instruction */
#define EXCP_SWI             2   /* software interrupt */
#define EXCP_PREFETCH_ABORT  3
#define EXCP_DATA_ABORT      4
#define EXCP_IRQ             5
#define EXCP_FIQ             6
#define EXCP_BKPT            7
#define EXCP_EXCEPTION_EXIT  8   /* Return from v7M exception.  */
#define EXCP_KERNEL_TRAP     9   /* Jumped to kernel code page.  */
#define EXCP_HVC            11   /* HyperVisor Call */
#define EXCP_HYP_TRAP       12
#define EXCP_SMC            13   /* Secure Monitor Call */
#define EXCP_VIRQ           14
#define EXCP_VFIQ           15
#define EXCP_NOCP           16   /* v7M NOCP UsageFault */
#define EXCP_INVSTATE       17   /* v7M INVSTATE UsageFault */
#define EXCP_STKOF          18   /* v8M STKOF UsageFault */
#define EXCP_LAZYFP         19   /* v7M fault during lazy FP stacking */
#define EXCP_LSERR          20   /* v8M LSERR SecureFault */
#define EXCP_UNALIGNED      21   /* v7M UNALIGNED UsageFault */
#define EXCP_DIVBYZERO      22   /* v7M DIVBYZERO UsageFault */
#define EXCP_VSERR          23
#define EXCP_GPC            24   /* v9 Granule Protection Check Fault */
#define EXCP_NMI            25
#define EXCP_VINMI          26
#define EXCP_VFNMI          27
#define EXCP_MON_TRAP       28   /* AArch32 trap to Monitor mode */

#define ARMV7M_EXCP_RESET   1
#define ARMV7M_EXCP_NMI     2
#define ARMV7M_EXCP_HARD    3
#define ARMV7M_EXCP_MEM     4
#define ARMV7M_EXCP_BUS     5
#define ARMV7M_EXCP_USAGE   6
#define ARMV7M_EXCP_SECURE  7
#define ARMV7M_EXCP_SVC     11
#define ARMV7M_EXCP_DEBUG   12
#define ARMV7M_EXCP_PENDSV  14
#define ARMV7M_EXCP_SYSTICK 15

#define CPU_INTERRUPT_FIQ   CPU_INTERRUPT_TGT_EXT_1
#define CPU_INTERRUPT_VIRQ  CPU_INTERRUPT_TGT_EXT_2
#define CPU_INTERRUPT_VFIQ  CPU_INTERRUPT_TGT_EXT_3
#define CPU_INTERRUPT_VSERR CPU_INTERRUPT_TGT_INT_0
#define CPU_INTERRUPT_NMI   CPU_INTERRUPT_TGT_EXT_4
#define CPU_INTERRUPT_VINMI CPU_INTERRUPT_TGT_EXT_0
#define CPU_INTERRUPT_VFNMI CPU_INTERRUPT_TGT_INT_1

#if HOST_BIG_ENDIAN
#define offsetoflow32(S, M) (offsetof(S, M) + sizeof(uint32_t))
#define offsetofhigh32(S, M) offsetof(S, M)
#else
#define offsetoflow32(S, M) offsetof(S, M)
#define offsetofhigh32(S, M) (offsetof(S, M) + sizeof(uint32_t))
#endif

#define TARGET_INSN_START_EXTRA_WORDS 2

#define ARM_INSN_START_WORD2_MASK ((1 << 26) - 1)
#define ARM_INSN_START_WORD2_SHIFT 13


typedef struct ARMGenericTimer {
    uint64_t cval; /* Timer CompareValue register */
    uint64_t ctl; /* Timer Control register */
} ARMGenericTimer;


#ifdef TARGET_AARCH64
# define ARM_MAX_VQ    16
#else
# define ARM_MAX_VQ    1
#endif

typedef struct ARMVectorReg {
    uint64_t d[2 * ARM_MAX_VQ] QEMU_ALIGNED(16);
} ARMVectorReg;

#ifdef TARGET_AARCH64
typedef struct ARMPredicateReg {
    uint64_t p[DIV_ROUND_UP(2 * ARM_MAX_VQ, 8)] QEMU_ALIGNED(16);
} ARMPredicateReg;

typedef struct ARMPACKey {
    uint64_t lo, hi;
} ARMPACKey;
#endif

typedef struct CPUARMTBFlags {
    uint32_t flags;
    target_ulong flags2;
} CPUARMTBFlags;

typedef struct ARMMMUFaultInfo ARMMMUFaultInfo;

typedef struct NVICState NVICState;

typedef enum ARMFPStatusFlavour {
    FPST_A32,
    FPST_A64,
    FPST_A32_F16,
    FPST_A64_F16,
    FPST_AH,
    FPST_AH_F16,
    FPST_STD,
    FPST_STD_F16,
} ARMFPStatusFlavour;
#define FPST_COUNT  8

typedef struct CPUArchState {
    uint32_t regs[16];

    uint64_t xregs[32];
    uint64_t pc;
    uint32_t pstate;
    bool aarch64; /* True if CPU is in aarch64 state; inverse of PSTATE.nRW */
    bool thumb;   /* True if CPU is in thumb mode; cpsr[5] */

    CPUARMTBFlags hflags;

    uint32_t uncached_cpsr;
    uint32_t spsr;

    uint64_t banked_spsr[8];
    uint32_t banked_r13[8];
    uint32_t banked_r14[8];

    uint32_t usr_regs[5];
    uint32_t fiq_regs[5];

    uint32_t CF; /* 0 or 1 */
    uint32_t VF; /* V is the bit 31. All other bits are undefined */
    uint32_t NF; /* N is bit 31. All other bits are undefined.  */
    uint32_t ZF; /* Z set if zero.  */
    uint32_t QF; /* 0 or 1 */
    uint32_t GE; /* cpsr[19:16] */
    uint32_t condexec_bits; /* IT bits.  cpsr[15:10,26:25].  */
    uint32_t btype;  /* BTI branch type.  spsr[11:10].  */
    uint64_t daif; /* exception masks, in the bits they are in PSTATE */
    uint64_t svcr; /* PSTATE.{SM,ZA} in the bits they are in SVCR */

    uint64_t elr_el[4]; /* AArch64 exception link regs  */
    uint64_t sp_el[4]; /* AArch64 banked stack pointers */

    struct {
        uint32_t c0_cpuid;
        union { /* Cache size selection */
            struct {
                uint64_t _unused_csselr0;
                uint64_t csselr_ns;
                uint64_t _unused_csselr1;
                uint64_t csselr_s;
            };
            uint64_t csselr_el[4];
        };
        union { /* System control register. */
            struct {
                uint64_t _unused_sctlr;
                uint64_t sctlr_ns;
                uint64_t hsctlr;
                uint64_t sctlr_s;
            };
            uint64_t sctlr_el[4];
        };
        uint64_t vsctlr; /* Virtualization System control register. */
        uint64_t cpacr_el1; /* Architectural feature access control register */
        uint64_t cptr_el[4];  /* ARMv8 feature trap registers */
        uint32_t c1_xscaleauxcr; /* XScale auxiliary control register.  */
        uint64_t sder; /* Secure debug enable register. */
        uint32_t nsacr; /* Non-secure access control register. */
        union { /* MMU translation table base 0. */
            struct {
                uint64_t _unused_ttbr0_0;
                uint64_t ttbr0_ns;
                uint64_t _unused_ttbr0_1;
                uint64_t ttbr0_s;
            };
            uint64_t ttbr0_el[4];
        };
        union { /* MMU translation table base 1. */
            struct {
                uint64_t _unused_ttbr1_0;
                uint64_t ttbr1_ns;
                uint64_t _unused_ttbr1_1;
                uint64_t ttbr1_s;
            };
            uint64_t ttbr1_el[4];
        };
        uint64_t vttbr_el2; /* Virtualization Translation Table Base.  */
        uint64_t vsttbr_el2; /* Secure Virtualization Translation Table. */
        uint64_t tcr_el[4];
        uint64_t vtcr_el2; /* Virtualization Translation Control.  */
        uint64_t vstcr_el2; /* Secure Virtualization Translation Control. */
        uint32_t c2_data; /* MPU data cacheable bits.  */
        uint32_t c2_insn; /* MPU instruction cacheable bits.  */
        union { /* MMU domain access control register
                 * MPU write buffer control.
                 */
            struct {
                uint64_t dacr_ns;
                uint64_t dacr_s;
            };
            struct {
                uint64_t dacr32_el2;
            };
        };
        uint32_t pmsav5_data_ap; /* PMSAv5 MPU data access permissions */
        uint32_t pmsav5_insn_ap; /* PMSAv5 MPU insn access permissions */
        uint64_t hcr_el2; /* Hypervisor configuration register */
        uint64_t hcrx_el2; /* Extended Hypervisor configuration register */
        uint64_t scr_el3; /* Secure configuration register.  */
        union { /* Fault status registers.  */
            struct {
                uint64_t ifsr_ns;
                uint64_t ifsr_s;
            };
            struct {
                uint64_t ifsr32_el2;
            };
        };
        union {
            struct {
                uint64_t _unused_dfsr;
                uint64_t dfsr_ns;
                uint64_t hsr;
                uint64_t dfsr_s;
            };
            uint64_t esr_el[4];
        };
        uint32_t c6_region[8]; /* MPU base/size registers.  */
        union { /* Fault address registers. */
            struct {
                uint64_t _unused_far0;
#if HOST_BIG_ENDIAN
                uint32_t ifar_ns;
                uint32_t dfar_ns;
                uint32_t ifar_s;
                uint32_t dfar_s;
#else
                uint32_t dfar_ns;
                uint32_t ifar_ns;
                uint32_t dfar_s;
                uint32_t ifar_s;
#endif
                uint64_t _unused_far3;
            };
            uint64_t far_el[4];
        };
        uint64_t hpfar_el2;
        uint64_t hstr_el2;
        union { /* Translation result. */
            struct {
                uint64_t _unused_par_0;
                uint64_t par_ns;
                uint64_t _unused_par_1;
                uint64_t par_s;
            };
            uint64_t par_el[4];
        };

        uint32_t c9_insn; /* Cache lockdown registers.  */
        uint32_t c9_data;
        uint64_t c9_pmcr; /* performance monitor control register */
        uint64_t c9_pmcnten; /* perf monitor counter enables */
        uint64_t c9_pmovsr; /* perf monitor overflow status */
        uint64_t c9_pmuserenr; /* perf monitor user enable */
        uint64_t c9_pmselr; /* perf monitor counter selection register */
        uint64_t c9_pminten; /* perf monitor interrupt enables */
        union { /* Memory attribute redirection */
            struct {
#if HOST_BIG_ENDIAN
                uint64_t _unused_mair_0;
                uint32_t mair1_ns;
                uint32_t mair0_ns;
                uint64_t _unused_mair_1;
                uint32_t mair1_s;
                uint32_t mair0_s;
#else
                uint64_t _unused_mair_0;
                uint32_t mair0_ns;
                uint32_t mair1_ns;
                uint64_t _unused_mair_1;
                uint32_t mair0_s;
                uint32_t mair1_s;
#endif
            };
            uint64_t mair_el[4];
        };
        union { /* vector base address register */
            struct {
                uint64_t _unused_vbar;
                uint64_t vbar_ns;
                uint64_t hvbar;
                uint64_t vbar_s;
            };
            uint64_t vbar_el[4];
        };
        uint32_t mvbar; /* (monitor) vector base address register */
        uint64_t rvbar; /* rvbar sampled from rvbar property at reset */
        struct { /* FCSE PID. */
            uint32_t fcseidr_ns;
            uint32_t fcseidr_s;
        };
        union { /* Context ID. */
            struct {
                uint64_t _unused_contextidr_0;
                uint64_t contextidr_ns;
                uint64_t _unused_contextidr_1;
                uint64_t contextidr_s;
            };
            uint64_t contextidr_el[4];
        };
        union { /* User RW Thread register. */
            struct {
                uint64_t tpidrurw_ns;
                uint64_t tpidrprw_ns;
                uint64_t htpidr;
                uint64_t _tpidr_el3;
            };
            uint64_t tpidr_el[4];
        };
        uint64_t tpidr2_el0;
        uint64_t tpidrurw_s;
        uint64_t tpidrprw_s;
        uint64_t tpidruro_s;

        union { /* User RO Thread register. */
            uint64_t tpidruro_ns;
            uint64_t tpidrro_el[1];
        };
        uint64_t c14_cntfrq; /* Counter Frequency register */
        uint64_t c14_cntkctl; /* Timer Control register */
        uint64_t cnthctl_el2; /* Counter/Timer Hyp Control register */
        uint64_t cntvoff_el2; /* Counter Virtual Offset register */
        uint64_t cntpoff_el2; /* Counter Physical Offset register */
        ARMGenericTimer c14_timer[NUM_GTIMERS];
        uint32_t c15_cpar; /* XScale Coprocessor Access Register */
        uint32_t c15_ticonfig; /* TI925T configuration byte.  */
        uint32_t c15_i_max; /* Maximum D-cache dirty line index.  */
        uint32_t c15_i_min; /* Minimum D-cache dirty line index.  */
        uint32_t c15_threadid; /* TI debugger thread-ID.  */
        uint32_t c15_config_base_address; /* SCU base address.  */
        uint32_t c15_diagnostic; /* diagnostic register */
        uint32_t c15_power_diagnostic;
        uint32_t c15_power_control; /* power control */
        uint64_t dbgbvr[16]; /* breakpoint value registers */
        uint64_t dbgbcr[16]; /* breakpoint control registers */
        uint64_t dbgwvr[16]; /* watchpoint value registers */
        uint64_t dbgwcr[16]; /* watchpoint control registers */
        uint64_t dbgclaim;   /* DBGCLAIM bits */
        uint64_t mdscr_el1;
        uint64_t oslsr_el1; /* OS Lock Status */
        uint64_t osdlr_el1; /* OS DoubleLock status */
        uint64_t mdcr_el2;
        uint64_t mdcr_el3;
        uint64_t c15_ccnt;
        uint64_t c15_ccnt_delta;
        uint64_t c14_pmevcntr[31];
        uint64_t c14_pmevcntr_delta[31];
        uint64_t c14_pmevtyper[31];
        uint64_t pmccfiltr_el0; /* Performance Monitor Filter Register */
        uint64_t vpidr_el2; /* Virtualization Processor ID Register */
        uint64_t vmpidr_el2; /* Virtualization Multiprocessor ID Register */
        uint64_t tfsr_el[4]; /* tfsre0_el1 is index 0.  */
        uint64_t gcr_el1;
        uint64_t rgsr_el1;

        uint64_t disr_el1;
        uint64_t vdisr_el2;
        uint64_t vsesr_el2;

        uint64_t fgt_read[2]; /* HFGRTR, HDFGRTR */
        uint64_t fgt_write[2]; /* HFGWTR, HDFGWTR */
        uint64_t fgt_exec[1]; /* HFGITR */

        uint64_t gpccr_el3;
        uint64_t gptbr_el3;
        uint64_t mfar_el3;

        uint64_t vncr_el2;
    } cp15;

    struct {
        uint32_t other_sp;
        uint32_t other_ss_msp;
        uint32_t other_ss_psp;
        uint32_t vecbase[M_REG_NUM_BANKS];
        uint32_t basepri[M_REG_NUM_BANKS];
        uint32_t control[M_REG_NUM_BANKS];
        uint32_t ccr[M_REG_NUM_BANKS]; /* Configuration and Control */
        uint32_t cfsr[M_REG_NUM_BANKS]; /* Configurable Fault Status */
        uint32_t hfsr; /* HardFault Status */
        uint32_t dfsr; /* Debug Fault Status Register */
        uint32_t sfsr; /* Secure Fault Status Register */
        uint32_t mmfar[M_REG_NUM_BANKS]; /* MemManage Fault Address */
        uint32_t bfar; /* BusFault Address */
        uint32_t sfar; /* Secure Fault Address Register */
        unsigned mpu_ctrl[M_REG_NUM_BANKS]; /* MPU_CTRL */
        int exception;
        uint32_t primask[M_REG_NUM_BANKS];
        uint32_t faultmask[M_REG_NUM_BANKS];
        uint32_t aircr; /* only holds r/w state if security extn implemented */
        uint32_t secure; /* Is CPU in Secure state? (not guest visible) */
        uint32_t csselr[M_REG_NUM_BANKS];
        uint32_t scr[M_REG_NUM_BANKS];
        uint32_t msplim[M_REG_NUM_BANKS];
        uint32_t psplim[M_REG_NUM_BANKS];
        uint32_t fpcar[M_REG_NUM_BANKS];
        uint32_t fpccr[M_REG_NUM_BANKS];
        uint32_t fpdscr[M_REG_NUM_BANKS];
        uint32_t cpacr[M_REG_NUM_BANKS];
        uint32_t nsacr;
        uint32_t ltpsize;
        uint32_t vpr;
    } v7m;

    struct {
        uint32_t syndrome; /* AArch64 format syndrome register */
        uint32_t fsr; /* AArch32 format fault status register info */
        uint64_t vaddress; /* virtual addr associated with exception, if any */
        uint32_t target_el; /* EL the exception should be targeted for */
    } exception;

    struct {
        uint8_t pending;
        uint8_t has_esr;
        uint64_t esr;
    } serror;

    uint8_t ext_dabt_raised; /* Tracking/verifying injection of ext DABT */

    uint32_t irq_line_state;

    uint32_t teecr;
    uint32_t teehbr;

    struct {
        ARMVectorReg zregs[32];

#ifdef TARGET_AARCH64
#define FFR_PRED_NUM 16
        ARMPredicateReg pregs[17];
        ARMPredicateReg preg_tmp;
#endif

        uint32_t qc[4] QEMU_ALIGNED(16);
        int vec_len;
        int vec_stride;

        uint64_t fpsr;
        uint64_t fpcr;

        uint32_t xregs[16];

        uint32_t scratch[8];

        float_status fp_status[FPST_COUNT];

        uint64_t zcr_el[4];   /* ZCR_EL[1-3] */
        uint64_t smcr_el[4];  /* SMCR_EL[1-3] */
    } vfp;

    uint64_t exclusive_addr;
    uint64_t exclusive_val;
    uint64_t exclusive_high;

    struct {
        uint64_t regs[16];
        uint64_t val;

        uint32_t cregs[16];
    } iwmmxt;

#ifdef TARGET_AARCH64
    struct {
        ARMPACKey apia;
        ARMPACKey apib;
        ARMPACKey apda;
        ARMPACKey apdb;
        ARMPACKey apga;
    } keys;

    uint64_t scxtnum_el[4];

    ARMVectorReg zarray[ARM_MAX_VQ * 16];
#endif

    struct CPUBreakpoint *cpu_breakpoint[16];
    struct CPUWatchpoint *cpu_watchpoint[16];

    ARMMMUFaultInfo *tlb_fi;

    struct {} end_reset_fields;


    uint64_t features;

    struct {
        uint32_t *drbar;
        uint32_t *drsr;
        uint32_t *dracr;
        uint32_t rnr[M_REG_NUM_BANKS];
    } pmsav7;

    struct {
        uint32_t *rbar[M_REG_NUM_BANKS];
        uint32_t *rlar[M_REG_NUM_BANKS];
        uint32_t *hprbar;
        uint32_t *hprlar;
        uint32_t mair0[M_REG_NUM_BANKS];
        uint32_t mair1[M_REG_NUM_BANKS];
        uint32_t hprselr;
    } pmsav8;

    struct {
        uint32_t *rbar;
        uint32_t *rlar;
        uint32_t rnr;
        uint32_t ctrl;
    } sau;

#if !defined(CONFIG_USER_ONLY)
    NVICState *nvic;
    const struct arm_boot_info *boot_info;
    void *gicv3state;
#else /* CONFIG_USER_ONLY */
    bool eabi;
#endif /* CONFIG_USER_ONLY */

#ifdef TARGET_TAGGED_ADDRESSES
    bool tagged_addr_enable;
#endif
} CPUARMState;

static inline void set_feature(CPUARMState *env, int feature)
{
    env->features |= 1ULL << feature;
}

static inline void unset_feature(CPUARMState *env, int feature)
{
    env->features &= ~(1ULL << feature);
}

typedef void ARMELChangeHookFn(ARMCPU *cpu, void *opaque);
typedef struct ARMELChangeHook ARMELChangeHook;
struct ARMELChangeHook {
    ARMELChangeHookFn *hook;
    void *opaque;
    QLIST_ENTRY(ARMELChangeHook) node;
};

typedef enum ARMPSCIState {
    PSCI_ON = 0,
    PSCI_OFF = 1,
    PSCI_ON_PENDING = 2
} ARMPSCIState;

typedef struct ARMISARegisters ARMISARegisters;

typedef struct {
    uint32_t map, init, supported;
} ARMVQMap;

struct ArchCPU {
    CPUState parent_obj;

    CPUARMState env;

    GHashTable *cp_regs;
    uint64_t *cpreg_indexes;
    uint64_t *cpreg_values;
    int32_t cpreg_array_len;
    uint64_t *cpreg_vmstate_indexes;
    uint64_t *cpreg_vmstate_values;
    int32_t cpreg_vmstate_array_len;

    QEMUTimer *gt_timer[NUM_GTIMERS];
    QEMUTimer *pmu_timer;
    QEMUTimer *wfxt_timer;

    qemu_irq gt_timer_outputs[NUM_GTIMERS];
    qemu_irq gicv3_maintenance_interrupt;
    qemu_irq pmu_interrupt;

    MemoryRegion *secure_memory;

    MemoryRegion *tag_memory;
    MemoryRegion *secure_tag_memory;

    Object *idau;

    const char *dtb_compatible;

    uint32_t psci_version;

    ARMPSCIState power_state;

    bool has_el2;
    bool has_el3;
    bool has_pmu;
    bool has_vfp;
    bool has_vfp_d32;
    bool has_neon;
    bool has_dsp;

    bool has_mpu;
    uint32_t pmsav7_dregion;
    uint32_t pmsav8r_hdregion;
    uint32_t sau_sregion;

    uint32_t psci_conduit;

    uint32_t init_svtor;
    uint32_t init_nsvtor;

    bool mp_is_up;

    bool host_cpu_probe_failed;

    bool backcompat_cntfrq;

    bool backcompat_pauth_default_use_qarma5;

    int32_t core_count;

    struct ARMISARegisters {
        uint32_t id_isar0;
        uint32_t id_isar1;
        uint32_t id_isar2;
        uint32_t id_isar3;
        uint32_t id_isar4;
        uint32_t id_isar5;
        uint32_t id_isar6;
        uint32_t id_mmfr0;
        uint32_t id_mmfr1;
        uint32_t id_mmfr2;
        uint32_t id_mmfr3;
        uint32_t id_mmfr4;
        uint32_t id_mmfr5;
        uint32_t id_pfr0;
        uint32_t id_pfr1;
        uint32_t id_pfr2;
        uint32_t mvfr0;
        uint32_t mvfr1;
        uint32_t mvfr2;
        uint32_t id_dfr0;
        uint32_t id_dfr1;
        uint32_t dbgdidr;
        uint32_t dbgdevid;
        uint32_t dbgdevid1;
        uint64_t id_aa64isar0;
        uint64_t id_aa64isar1;
        uint64_t id_aa64isar2;
        uint64_t id_aa64pfr0;
        uint64_t id_aa64pfr1;
        uint64_t id_aa64mmfr0;
        uint64_t id_aa64mmfr1;
        uint64_t id_aa64mmfr2;
        uint64_t id_aa64mmfr3;
        uint64_t id_aa64dfr0;
        uint64_t id_aa64dfr1;
        uint64_t id_aa64zfr0;
        uint64_t id_aa64smfr0;
        uint64_t reset_pmcr_el0;
    } isar;
    uint64_t midr;
    uint32_t revidr;
    uint32_t reset_fpsid;
    uint64_t ctr;
    uint32_t reset_sctlr;
    uint64_t pmceid0;
    uint64_t pmceid1;
    uint32_t id_afr0;
    uint64_t id_aa64afr0;
    uint64_t id_aa64afr1;
    uint64_t clidr;
    uint64_t mp_affinity; /* MP ID without feature bits */
    uint64_t ccsidr[16];
    uint64_t reset_cbar;
    uint32_t reset_auxcr;
    bool reset_hivecs;
    uint8_t reset_l0gptsz;

    bool prop_pauth;
    bool prop_pauth_impdef;
    bool prop_pauth_qarma3;
    bool prop_pauth_qarma5;
    bool prop_lpa2;

    uint8_t dcz_blocksize;
    uint8_t gm_blocksize;

    uint64_t rvbar_prop; /* Property/input signals.  */

    int gic_num_lrs; /* number of list registers */
    int gic_vpribits; /* number of virtual priority bits */
    int gic_vprebits; /* number of virtual preemption bits */
    int gic_pribits; /* number of physical priority bits */

    bool cfgend;

    QLIST_HEAD(, ARMELChangeHook) pre_el_change_hooks;
    QLIST_HEAD(, ARMELChangeHook) el_change_hooks;

    int32_t node_id; /* NUMA node this CPU belongs to */

    uint8_t device_irq_level;

    uint32_t sve_max_vq;

#ifdef CONFIG_USER_ONLY
    uint32_t sve_default_vq;
    uint32_t sme_default_vq;
#endif

    ARMVQMap sve_vq;
    ARMVQMap sme_vq;

    uint64_t gt_cntfrq_hz;
};

typedef struct ARMCPUInfo {
    const char *name;
    const char *deprecation_note;
    void (*initfn)(Object *obj);
    void (*class_init)(ObjectClass *oc, void *data);
} ARMCPUInfo;

struct ARMCPUClass {
    CPUClass parent_class;

    const ARMCPUInfo *info;
    DeviceRealize parent_realize;
    ResettablePhases parent_phases;
};

struct AArch64CPUClass {
    ARMCPUClass parent_class;
};

void arm_gt_ptimer_cb(void *opaque);
void arm_gt_vtimer_cb(void *opaque);
void arm_gt_htimer_cb(void *opaque);
void arm_gt_stimer_cb(void *opaque);
void arm_gt_hvtimer_cb(void *opaque);
void arm_gt_sel2timer_cb(void *opaque);
void arm_gt_sel2vtimer_cb(void *opaque);

unsigned int gt_cntfrq_period_ns(ARMCPU *cpu);
void gt_rme_post_el_change(ARMCPU *cpu, void *opaque);

void arm_cpu_post_init(Object *obj);

#define ARM_AFF0_SHIFT 0
#define ARM_AFF0_MASK  (0xFFULL << ARM_AFF0_SHIFT)
#define ARM_AFF1_SHIFT 8
#define ARM_AFF1_MASK  (0xFFULL << ARM_AFF1_SHIFT)
#define ARM_AFF2_SHIFT 16
#define ARM_AFF2_MASK  (0xFFULL << ARM_AFF2_SHIFT)
#define ARM_AFF3_SHIFT 32
#define ARM_AFF3_MASK  (0xFFULL << ARM_AFF3_SHIFT)
#define ARM_DEFAULT_CPUS_PER_CLUSTER 8

#define ARM32_AFFINITY_MASK (ARM_AFF0_MASK | ARM_AFF1_MASK | ARM_AFF2_MASK)
#define ARM64_AFFINITY_MASK \
    (ARM_AFF0_MASK | ARM_AFF1_MASK | ARM_AFF2_MASK | ARM_AFF3_MASK)
#define ARM64_AFFINITY_INVALID (~ARM64_AFFINITY_MASK)

uint64_t arm_build_mp_affinity(int idx, uint8_t clustersz);

#ifndef CONFIG_USER_ONLY
extern const VMStateDescription vmstate_arm_cpu;

void arm_cpu_do_interrupt(CPUState *cpu);
void arm_v7m_cpu_do_interrupt(CPUState *cpu);

hwaddr arm_cpu_get_phys_page_attrs_debug(CPUState *cpu, vaddr addr,
                                         MemTxAttrs *attrs);
#endif /* !CONFIG_USER_ONLY */

void arm_emulate_firmware_reset(CPUState *cpustate, int target_el);

#ifdef TARGET_AARCH64
void aarch64_sve_narrow_vq(CPUARMState *env, unsigned vq);
void aarch64_sve_change_el(CPUARMState *env, int old_el,
                           int new_el, bool el0_a64);
void aarch64_set_svcr(CPUARMState *env, uint64_t new, uint64_t mask);

static inline uint64_t *sve_bswap64(uint64_t *dst, uint64_t *src, int nr)
{
#if HOST_BIG_ENDIAN
    int i;

    for (i = 0; i < nr; ++i) {
        dst[i] = bswap64(src[i]);
    }

    return dst;
#else
    return src;
#endif
}

#else
static inline void aarch64_sve_narrow_vq(CPUARMState *env, unsigned vq) { }
static inline void aarch64_sve_change_el(CPUARMState *env, int o,
                                         int n, bool a)
{ }
#endif

void aarch64_sync_32_to_64(CPUARMState *env);
void aarch64_sync_64_to_32(CPUARMState *env);

int fp_exception_el(CPUARMState *env, int cur_el);
int sve_exception_el(CPUARMState *env, int cur_el);
int sme_exception_el(CPUARMState *env, int cur_el);

uint32_t sve_vqm1_for_el_sm(CPUARMState *env, int el, bool sm);

uint32_t sve_vqm1_for_el(CPUARMState *env, int el);

static inline bool is_a64(CPUARMState *env)
{
    return env->aarch64;
}

void pmu_op_start(CPUARMState *env);
void pmu_op_finish(CPUARMState *env);

void arm_pmu_timer_cb(void *opaque);

void pmu_pre_el_change(ARMCPU *cpu, void *ignored);
void pmu_post_el_change(ARMCPU *cpu, void *ignored);

void pmu_init(ARMCPU *cpu);

#define SCTLR_M       (1U << 0)
#define SCTLR_A       (1U << 1)
#define SCTLR_C       (1U << 2)
#define SCTLR_W       (1U << 3) /* up to v6; RAO in v7 */
#define SCTLR_nTLSMD_32 (1U << 3) /* v8.2-LSMAOC, AArch32 only */
#define SCTLR_SA      (1U << 3) /* AArch64 only */
#define SCTLR_P       (1U << 4) /* up to v5; RAO in v6 and v7 */
#define SCTLR_LSMAOE_32 (1U << 4) /* v8.2-LSMAOC, AArch32 only */
#define SCTLR_SA0     (1U << 4) /* v8 onward, AArch64 only */
#define SCTLR_D       (1U << 5) /* up to v5; RAO in v6 */
#define SCTLR_CP15BEN (1U << 5) /* v7 onward */
#define SCTLR_L       (1U << 6) /* up to v5; RAO in v6 and v7; RAZ in v8 */
#define SCTLR_nAA     (1U << 6) /* when FEAT_LSE2 is implemented */
#define SCTLR_B       (1U << 7) /* up to v6; RAZ in v7 */
#define SCTLR_ITD     (1U << 7) /* v8 onward */
#define SCTLR_S       (1U << 8) /* up to v6; RAZ in v7 */
#define SCTLR_SED     (1U << 8) /* v8 onward */
#define SCTLR_R       (1U << 9) /* up to v6; RAZ in v7 */
#define SCTLR_UMA     (1U << 9) /* v8 onward, AArch64 only */
#define SCTLR_F       (1U << 10) /* up to v6 */
#define SCTLR_SW      (1U << 10) /* v7 */
#define SCTLR_EnRCTX  (1U << 10) /* in v8.0-PredInv */
#define SCTLR_Z       (1U << 11) /* in v7, RES1 in v8 */
#define SCTLR_EOS     (1U << 11) /* v8.5-ExS */
#define SCTLR_I       (1U << 12)
#define SCTLR_V       (1U << 13) /* AArch32 only */
#define SCTLR_EnDB    (1U << 13) /* v8.3, AArch64 only */
#define SCTLR_RR      (1U << 14) /* up to v7 */
#define SCTLR_DZE     (1U << 14) /* v8 onward, AArch64 only */
#define SCTLR_L4      (1U << 15) /* up to v6; RAZ in v7 */
#define SCTLR_UCT     (1U << 15) /* v8 onward, AArch64 only */
#define SCTLR_DT      (1U << 16) /* up to ??, RAO in v6 and v7 */
#define SCTLR_nTWI    (1U << 16) /* v8 onward */
#define SCTLR_HA      (1U << 17) /* up to v7, RES0 in v8 */
#define SCTLR_BR      (1U << 17) /* PMSA only */
#define SCTLR_IT      (1U << 18) /* up to ??, RAO in v6 and v7 */
#define SCTLR_nTWE    (1U << 18) /* v8 onward */
#define SCTLR_WXN     (1U << 19)
#define SCTLR_ST      (1U << 20) /* up to ??, RAZ in v6 */
#define SCTLR_UWXN    (1U << 20) /* v7 onward, AArch32 only */
#define SCTLR_TSCXT   (1U << 20) /* FEAT_CSV2_1p2, AArch64 only */
#define SCTLR_FI      (1U << 21) /* up to v7, v8 RES0 */
#define SCTLR_IESB    (1U << 21) /* v8.2-IESB, AArch64 only */
#define SCTLR_U       (1U << 22) /* up to v6, RAO in v7 */
#define SCTLR_EIS     (1U << 22) /* v8.5-ExS */
#define SCTLR_XP      (1U << 23) /* up to v6; v7 onward RAO */
#define SCTLR_SPAN    (1U << 23) /* v8.1-PAN */
#define SCTLR_VE      (1U << 24) /* up to v7 */
#define SCTLR_E0E     (1U << 24) /* v8 onward, AArch64 only */
#define SCTLR_EE      (1U << 25)
#define SCTLR_L2      (1U << 26) /* up to v6, RAZ in v7 */
#define SCTLR_UCI     (1U << 26) /* v8 onward, AArch64 only */
#define SCTLR_NMFI    (1U << 27) /* up to v7, RAZ in v7VE and v8 */
#define SCTLR_EnDA    (1U << 27) /* v8.3, AArch64 only */
#define SCTLR_TRE     (1U << 28) /* AArch32 only */
#define SCTLR_nTLSMD_64 (1U << 28) /* v8.2-LSMAOC, AArch64 only */
#define SCTLR_AFE     (1U << 29) /* AArch32 only */
#define SCTLR_LSMAOE_64 (1U << 29) /* v8.2-LSMAOC, AArch64 only */
#define SCTLR_TE      (1U << 30) /* AArch32 only */
#define SCTLR_EnIB    (1U << 30) /* v8.3, AArch64 only */
#define SCTLR_EnIA    (1U << 31) /* v8.3, AArch64 only */
#define SCTLR_DSSBS_32 (1U << 31) /* v8.5, AArch32 only */
#define SCTLR_CMOW    (1ULL << 32) /* FEAT_CMOW */
#define SCTLR_MSCEN   (1ULL << 33) /* FEAT_MOPS */
#define SCTLR_BT0     (1ULL << 35) /* v8.5-BTI */
#define SCTLR_BT1     (1ULL << 36) /* v8.5-BTI */
#define SCTLR_ITFSB   (1ULL << 37) /* v8.5-MemTag */
#define SCTLR_TCF0    (3ULL << 38) /* v8.5-MemTag */
#define SCTLR_TCF     (3ULL << 40) /* v8.5-MemTag */
#define SCTLR_ATA0    (1ULL << 42) /* v8.5-MemTag */
#define SCTLR_ATA     (1ULL << 43) /* v8.5-MemTag */
#define SCTLR_DSSBS_64 (1ULL << 44) /* v8.5, AArch64 only */
#define SCTLR_TWEDEn  (1ULL << 45)  /* FEAT_TWED */
#define SCTLR_TWEDEL  MAKE_64_MASK(46, 4)  /* FEAT_TWED */
#define SCTLR_TMT0    (1ULL << 50) /* FEAT_TME */
#define SCTLR_TMT     (1ULL << 51) /* FEAT_TME */
#define SCTLR_TME0    (1ULL << 52) /* FEAT_TME */
#define SCTLR_TME     (1ULL << 53) /* FEAT_TME */
#define SCTLR_EnASR   (1ULL << 54) /* FEAT_LS64_V */
#define SCTLR_EnAS0   (1ULL << 55) /* FEAT_LS64_ACCDATA */
#define SCTLR_EnALS   (1ULL << 56) /* FEAT_LS64 */
#define SCTLR_EPAN    (1ULL << 57) /* FEAT_PAN3 */
#define SCTLR_EnTP2   (1ULL << 60) /* FEAT_SME */
#define SCTLR_NMI     (1ULL << 61) /* FEAT_NMI */
#define SCTLR_SPINTMASK (1ULL << 62) /* FEAT_NMI */
#define SCTLR_TIDCP   (1ULL << 63) /* FEAT_TIDCP1 */

#define CPSR_M (0x1fU)
#define CPSR_T (1U << 5)
#define CPSR_F (1U << 6)
#define CPSR_I (1U << 7)
#define CPSR_A (1U << 8)
#define CPSR_E (1U << 9)
#define CPSR_IT_2_7 (0xfc00U)
#define CPSR_GE (0xfU << 16)
#define CPSR_IL (1U << 20)
#define CPSR_DIT (1U << 21)
#define CPSR_PAN (1U << 22)
#define CPSR_SSBS (1U << 23)
#define CPSR_J (1U << 24)
#define CPSR_IT_0_1 (3U << 25)
#define CPSR_Q (1U << 27)
#define CPSR_V (1U << 28)
#define CPSR_C (1U << 29)
#define CPSR_Z (1U << 30)
#define CPSR_N (1U << 31)
#define CPSR_NZCV (CPSR_N | CPSR_Z | CPSR_C | CPSR_V)
#define CPSR_AIF (CPSR_A | CPSR_I | CPSR_F)
#define ISR_FS (1U << 9)
#define ISR_IS (1U << 10)

#define CPSR_IT (CPSR_IT_0_1 | CPSR_IT_2_7)
#define CACHED_CPSR_BITS (CPSR_T | CPSR_AIF | CPSR_GE | CPSR_IT | CPSR_Q \
    | CPSR_NZCV)
#define CPSR_USER (CPSR_NZCV | CPSR_Q | CPSR_GE | CPSR_E)
#define CPSR_EXEC (CPSR_T | CPSR_IT | CPSR_J | CPSR_IL)

#define XPSR_EXCP 0x1ffU
#define XPSR_SPREALIGN (1U << 9) /* Only set in exception stack frames */
#define XPSR_IT_2_7 CPSR_IT_2_7
#define XPSR_GE CPSR_GE
#define XPSR_SFPA (1U << 20) /* Only set in exception stack frames */
#define XPSR_T (1U << 24) /* Not the same as CPSR_T ! */
#define XPSR_IT_0_1 CPSR_IT_0_1
#define XPSR_Q CPSR_Q
#define XPSR_V CPSR_V
#define XPSR_C CPSR_C
#define XPSR_Z CPSR_Z
#define XPSR_N CPSR_N
#define XPSR_NZCV CPSR_NZCV
#define XPSR_IT CPSR_IT

#define PSTATE_SP (1U)
#define PSTATE_M (0xFU)
#define PSTATE_nRW (1U << 4)
#define PSTATE_F (1U << 6)
#define PSTATE_I (1U << 7)
#define PSTATE_A (1U << 8)
#define PSTATE_D (1U << 9)
#define PSTATE_BTYPE (3U << 10)
#define PSTATE_SSBS (1U << 12)
#define PSTATE_ALLINT (1U << 13)
#define PSTATE_IL (1U << 20)
#define PSTATE_SS (1U << 21)
#define PSTATE_PAN (1U << 22)
#define PSTATE_UAO (1U << 23)
#define PSTATE_DIT (1U << 24)
#define PSTATE_TCO (1U << 25)
#define PSTATE_V (1U << 28)
#define PSTATE_C (1U << 29)
#define PSTATE_Z (1U << 30)
#define PSTATE_N (1U << 31)
#define PSTATE_NZCV (PSTATE_N | PSTATE_Z | PSTATE_C | PSTATE_V)
#define PSTATE_DAIF (PSTATE_D | PSTATE_A | PSTATE_I | PSTATE_F)
#define CACHED_PSTATE_BITS (PSTATE_NZCV | PSTATE_DAIF | PSTATE_BTYPE)
#define PSTATE_MODE_EL3h 13
#define PSTATE_MODE_EL3t 12
#define PSTATE_MODE_EL2h 9
#define PSTATE_MODE_EL2t 8
#define PSTATE_MODE_EL1h 5
#define PSTATE_MODE_EL1t 4
#define PSTATE_MODE_EL0t 0

FIELD(SVCR, SM, 0, 1)
FIELD(SVCR, ZA, 1, 1)

FIELD(SMCR, LEN, 0, 4)
FIELD(SMCR, FA64, 31, 1)

void write_v7m_exception(CPUARMState *env, uint32_t new_exc);

static inline unsigned int aarch64_pstate_mode(unsigned int el, bool handler)
{
    return (el << 2) | handler;
}

static inline uint32_t pstate_read(CPUARMState *env)
{
    int ZF;

    ZF = (env->ZF == 0);
    return (env->NF & 0x80000000) | (ZF << 30)
        | (env->CF << 29) | ((env->VF & 0x80000000) >> 3)
        | env->pstate | env->daif | (env->btype << 10);
}

static inline void pstate_write(CPUARMState *env, uint32_t val)
{
    env->ZF = (~val) & PSTATE_Z;
    env->NF = val;
    env->CF = (val >> 29) & 1;
    env->VF = (val << 3) & 0x80000000;
    env->daif = val & PSTATE_DAIF;
    env->btype = (val >> 10) & 3;
    env->pstate = val & ~CACHED_PSTATE_BITS;
}

uint32_t cpsr_read(CPUARMState *env);

typedef enum CPSRWriteType {
    CPSRWriteByInstr = 0,         /* from guest MSR or CPS */
    CPSRWriteExceptionReturn = 1, /* from guest exception return insn */
    CPSRWriteRaw = 2,
} CPSRWriteType;

void cpsr_write(CPUARMState *env, uint32_t val, uint32_t mask,
                CPSRWriteType write_type);

static inline uint32_t xpsr_read(CPUARMState *env)
{
    int ZF;
    ZF = (env->ZF == 0);
    return (env->NF & 0x80000000) | (ZF << 30)
        | (env->CF << 29) | ((env->VF & 0x80000000) >> 3) | (env->QF << 27)
        | (env->thumb << 24) | ((env->condexec_bits & 3) << 25)
        | ((env->condexec_bits & 0xfc) << 8)
        | (env->GE << 16)
        | env->v7m.exception;
}

static inline void xpsr_write(CPUARMState *env, uint32_t val, uint32_t mask)
{
    if (mask & XPSR_NZCV) {
        env->ZF = (~val) & XPSR_Z;
        env->NF = val;
        env->CF = (val >> 29) & 1;
        env->VF = (val << 3) & 0x80000000;
    }
    if (mask & XPSR_Q) {
        env->QF = ((val & XPSR_Q) != 0);
    }
    if (mask & XPSR_GE) {
        env->GE = (val & XPSR_GE) >> 16;
    }
#ifndef CONFIG_USER_ONLY
    if (mask & XPSR_T) {
        env->thumb = ((val & XPSR_T) != 0);
    }
    if (mask & XPSR_IT_0_1) {
        env->condexec_bits &= ~3;
        env->condexec_bits |= (val >> 25) & 3;
    }
    if (mask & XPSR_IT_2_7) {
        env->condexec_bits &= 3;
        env->condexec_bits |= (val >> 8) & 0xfc;
    }
    if (mask & XPSR_EXCP) {
        write_v7m_exception(env, val & XPSR_EXCP);
    }
#endif
}

#define HCR_VM        (1ULL << 0)
#define HCR_SWIO      (1ULL << 1)
#define HCR_PTW       (1ULL << 2)
#define HCR_FMO       (1ULL << 3)
#define HCR_IMO       (1ULL << 4)
#define HCR_AMO       (1ULL << 5)
#define HCR_VF        (1ULL << 6)
#define HCR_VI        (1ULL << 7)
#define HCR_VSE       (1ULL << 8)
#define HCR_FB        (1ULL << 9)
#define HCR_BSU_MASK  (3ULL << 10)
#define HCR_DC        (1ULL << 12)
#define HCR_TWI       (1ULL << 13)
#define HCR_TWE       (1ULL << 14)
#define HCR_TID0      (1ULL << 15)
#define HCR_TID1      (1ULL << 16)
#define HCR_TID2      (1ULL << 17)
#define HCR_TID3      (1ULL << 18)
#define HCR_TSC       (1ULL << 19)
#define HCR_TIDCP     (1ULL << 20)
#define HCR_TACR      (1ULL << 21)
#define HCR_TSW       (1ULL << 22)
#define HCR_TPCP      (1ULL << 23)
#define HCR_TPU       (1ULL << 24)
#define HCR_TTLB      (1ULL << 25)
#define HCR_TVM       (1ULL << 26)
#define HCR_TGE       (1ULL << 27)
#define HCR_TDZ       (1ULL << 28)
#define HCR_HCD       (1ULL << 29)
#define HCR_TRVM      (1ULL << 30)
#define HCR_RW        (1ULL << 31)
#define HCR_CD        (1ULL << 32)
#define HCR_ID        (1ULL << 33)
#define HCR_E2H       (1ULL << 34)
#define HCR_TLOR      (1ULL << 35)
#define HCR_TERR      (1ULL << 36)
#define HCR_TEA       (1ULL << 37)
#define HCR_MIOCNCE   (1ULL << 38)
#define HCR_TME       (1ULL << 39)
#define HCR_APK       (1ULL << 40)
#define HCR_API       (1ULL << 41)
#define HCR_NV        (1ULL << 42)
#define HCR_NV1       (1ULL << 43)
#define HCR_AT        (1ULL << 44)
#define HCR_NV2       (1ULL << 45)
#define HCR_FWB       (1ULL << 46)
#define HCR_FIEN      (1ULL << 47)
#define HCR_GPF       (1ULL << 48)
#define HCR_TID4      (1ULL << 49)
#define HCR_TICAB     (1ULL << 50)
#define HCR_AMVOFFEN  (1ULL << 51)
#define HCR_TOCU      (1ULL << 52)
#define HCR_ENSCXT    (1ULL << 53)
#define HCR_TTLBIS    (1ULL << 54)
#define HCR_TTLBOS    (1ULL << 55)
#define HCR_ATA       (1ULL << 56)
#define HCR_DCT       (1ULL << 57)
#define HCR_TID5      (1ULL << 58)
#define HCR_TWEDEN    (1ULL << 59)
#define HCR_TWEDEL    MAKE_64BIT_MASK(60, 4)

#define SCR_NS                (1ULL << 0)
#define SCR_IRQ               (1ULL << 1)
#define SCR_FIQ               (1ULL << 2)
#define SCR_EA                (1ULL << 3)
#define SCR_FW                (1ULL << 4)
#define SCR_AW                (1ULL << 5)
#define SCR_NET               (1ULL << 6)
#define SCR_SMD               (1ULL << 7)
#define SCR_HCE               (1ULL << 8)
#define SCR_SIF               (1ULL << 9)
#define SCR_RW                (1ULL << 10)
#define SCR_ST                (1ULL << 11)
#define SCR_TWI               (1ULL << 12)
#define SCR_TWE               (1ULL << 13)
#define SCR_TLOR              (1ULL << 14)
#define SCR_TERR              (1ULL << 15)
#define SCR_APK               (1ULL << 16)
#define SCR_API               (1ULL << 17)
#define SCR_EEL2              (1ULL << 18)
#define SCR_EASE              (1ULL << 19)
#define SCR_NMEA              (1ULL << 20)
#define SCR_FIEN              (1ULL << 21)
#define SCR_ENSCXT            (1ULL << 25)
#define SCR_ATA               (1ULL << 26)
#define SCR_FGTEN             (1ULL << 27)
#define SCR_ECVEN             (1ULL << 28)
#define SCR_TWEDEN            (1ULL << 29)
#define SCR_TWEDEL            MAKE_64BIT_MASK(30, 4)
#define SCR_TME               (1ULL << 34)
#define SCR_AMVOFFEN          (1ULL << 35)
#define SCR_ENAS0             (1ULL << 36)
#define SCR_ADEN              (1ULL << 37)
#define SCR_HXEN              (1ULL << 38)
#define SCR_TRNDR             (1ULL << 40)
#define SCR_ENTP2             (1ULL << 41)
#define SCR_GPF               (1ULL << 48)
#define SCR_NSE               (1ULL << 62)

uint32_t vfp_get_fpscr(CPUARMState *env);
void vfp_set_fpscr(CPUARMState *env, uint32_t val);


#define FPCR_FIZ    (1 << 0)    /* Flush Inputs to Zero (FEAT_AFP) */
#define FPCR_AH     (1 << 1)    /* Alternate Handling (FEAT_AFP) */
#define FPCR_NEP    (1 << 2)    /* SIMD scalar ops preserve elts (FEAT_AFP) */
#define FPCR_IOE    (1 << 8)    /* Invalid Operation exception trap enable */
#define FPCR_DZE    (1 << 9)    /* Divide by Zero exception trap enable */
#define FPCR_OFE    (1 << 10)   /* Overflow exception trap enable */
#define FPCR_UFE    (1 << 11)   /* Underflow exception trap enable */
#define FPCR_IXE    (1 << 12)   /* Inexact exception trap enable */
#define FPCR_EBF    (1 << 13)   /* Extended BFloat16 behaviors */
#define FPCR_IDE    (1 << 15)   /* Input Denormal exception trap enable */
#define FPCR_LEN_MASK (7 << 16) /* LEN, A-profile only */
#define FPCR_FZ16   (1 << 19)   /* ARMv8.2+, FP16 flush-to-zero */
#define FPCR_STRIDE_MASK (3 << 20) /* Stride */
#define FPCR_RMODE_MASK (3 << 22) /* Rounding mode */
#define FPCR_FZ     (1 << 24)   /* Flush-to-zero enable bit */
#define FPCR_DN     (1 << 25)   /* Default NaN enable bit */
#define FPCR_AHP    (1 << 26)   /* Alternative half-precision */

#define FPCR_LTPSIZE_SHIFT 16   /* LTPSIZE, M-profile only */
#define FPCR_LTPSIZE_MASK (7 << FPCR_LTPSIZE_SHIFT)
#define FPCR_LTPSIZE_LENGTH 3

#define FPCR_EEXC_MASK (FPCR_IOE | FPCR_DZE | FPCR_OFE | FPCR_UFE | FPCR_IXE | FPCR_IDE)

#define FPSR_IOC    (1 << 0)    /* Invalid Operation cumulative exception */
#define FPSR_DZC    (1 << 1)    /* Divide by Zero cumulative exception */
#define FPSR_OFC    (1 << 2)    /* Overflow cumulative exception */
#define FPSR_UFC    (1 << 3)    /* Underflow cumulative exception */
#define FPSR_IXC    (1 << 4)    /* Inexact cumulative exception */
#define FPSR_IDC    (1 << 7)    /* Input Denormal cumulative exception */
#define FPSR_QC     (1 << 27)   /* Cumulative saturation bit */
#define FPSR_V      (1 << 28)   /* FP overflow flag */
#define FPSR_C      (1 << 29)   /* FP carry flag */
#define FPSR_Z      (1 << 30)   /* FP zero flag */
#define FPSR_N      (1 << 31)   /* FP negative flag */

#define FPSR_CEXC_MASK (FPSR_IOC | FPSR_DZC | FPSR_OFC | FPSR_UFC | FPSR_IXC | FPSR_IDC)

#define FPSR_NZCV_MASK (FPSR_N | FPSR_Z | FPSR_C | FPSR_V)
#define FPSR_NZCVQC_MASK (FPSR_NZCV_MASK | FPSR_QC)

#define FPSCR_FPSR_MASK (FPSR_NZCVQC_MASK | FPSR_CEXC_MASK)
#define FPSCR_FPCR_MASK (FPCR_EEXC_MASK | FPCR_LEN_MASK | FPCR_FZ16 | \
                         FPCR_STRIDE_MASK | FPCR_RMODE_MASK | \
                         FPCR_FZ | FPCR_DN | FPCR_AHP)
QEMU_BUILD_BUG_ON(FPSCR_FPSR_MASK & FPSCR_FPCR_MASK);

uint32_t vfp_get_fpsr(CPUARMState *env);

uint32_t vfp_get_fpcr(CPUARMState *env);

void vfp_set_fpsr(CPUARMState *env, uint32_t value);

void vfp_set_fpcr(CPUARMState *env, uint32_t value);

enum arm_cpu_mode {
  ARM_CPU_MODE_USR = 0x10,
  ARM_CPU_MODE_FIQ = 0x11,
  ARM_CPU_MODE_IRQ = 0x12,
  ARM_CPU_MODE_SVC = 0x13,
  ARM_CPU_MODE_MON = 0x16,
  ARM_CPU_MODE_ABT = 0x17,
  ARM_CPU_MODE_HYP = 0x1a,
  ARM_CPU_MODE_UND = 0x1b,
  ARM_CPU_MODE_SYS = 0x1f
};

#define ARM_VFP_FPSID   0
#define ARM_VFP_FPSCR   1
#define ARM_VFP_MVFR2   5
#define ARM_VFP_MVFR1   6
#define ARM_VFP_MVFR0   7
#define ARM_VFP_FPEXC   8
#define ARM_VFP_FPINST  9
#define ARM_VFP_FPINST2 10
#define ARM_VFP_FPSCR_NZCVQC 2
#define ARM_VFP_VPR 12
#define ARM_VFP_P0 13
#define ARM_VFP_FPCXT_NS 14
#define ARM_VFP_FPCXT_S 15

#define QEMU_VFP_FPSCR_NZCV 0xffff

#define ARM_IWMMXT_wCID  0
#define ARM_IWMMXT_wCon  1
#define ARM_IWMMXT_wCSSF 2
#define ARM_IWMMXT_wCASF 3
#define ARM_IWMMXT_wCGR0 8
#define ARM_IWMMXT_wCGR1 9
#define ARM_IWMMXT_wCGR2 10
#define ARM_IWMMXT_wCGR3 11

FIELD(V7M_CCR, NONBASETHRDENA, 0, 1)
FIELD(V7M_CCR, USERSETMPEND, 1, 1)
FIELD(V7M_CCR, UNALIGN_TRP, 3, 1)
FIELD(V7M_CCR, DIV_0_TRP, 4, 1)
FIELD(V7M_CCR, BFHFNMIGN, 8, 1)
FIELD(V7M_CCR, STKALIGN, 9, 1)
FIELD(V7M_CCR, STKOFHFNMIGN, 10, 1)
FIELD(V7M_CCR, DC, 16, 1)
FIELD(V7M_CCR, IC, 17, 1)
FIELD(V7M_CCR, BP, 18, 1)
FIELD(V7M_CCR, LOB, 19, 1)
FIELD(V7M_CCR, TRD, 20, 1)

FIELD(V7M_SCR, SLEEPONEXIT, 1, 1)
FIELD(V7M_SCR, SLEEPDEEP, 2, 1)
FIELD(V7M_SCR, SLEEPDEEPS, 3, 1)
FIELD(V7M_SCR, SEVONPEND, 4, 1)

FIELD(V7M_AIRCR, VECTRESET, 0, 1)
FIELD(V7M_AIRCR, VECTCLRACTIVE, 1, 1)
FIELD(V7M_AIRCR, SYSRESETREQ, 2, 1)
FIELD(V7M_AIRCR, SYSRESETREQS, 3, 1)
FIELD(V7M_AIRCR, PRIGROUP, 8, 3)
FIELD(V7M_AIRCR, BFHFNMINS, 13, 1)
FIELD(V7M_AIRCR, PRIS, 14, 1)
FIELD(V7M_AIRCR, ENDIANNESS, 15, 1)
FIELD(V7M_AIRCR, VECTKEY, 16, 16)

FIELD(V7M_CFSR, IACCVIOL, 0, 1)
FIELD(V7M_CFSR, DACCVIOL, 1, 1)
FIELD(V7M_CFSR, MUNSTKERR, 3, 1)
FIELD(V7M_CFSR, MSTKERR, 4, 1)
FIELD(V7M_CFSR, MLSPERR, 5, 1)
FIELD(V7M_CFSR, MMARVALID, 7, 1)

FIELD(V7M_CFSR, IBUSERR, 8 + 0, 1)
FIELD(V7M_CFSR, PRECISERR, 8 + 1, 1)
FIELD(V7M_CFSR, IMPRECISERR, 8 + 2, 1)
FIELD(V7M_CFSR, UNSTKERR, 8 + 3, 1)
FIELD(V7M_CFSR, STKERR, 8 + 4, 1)
FIELD(V7M_CFSR, LSPERR, 8 + 5, 1)
FIELD(V7M_CFSR, BFARVALID, 8 + 7, 1)

FIELD(V7M_CFSR, UNDEFINSTR, 16 + 0, 1)
FIELD(V7M_CFSR, INVSTATE, 16 + 1, 1)
FIELD(V7M_CFSR, INVPC, 16 + 2, 1)
FIELD(V7M_CFSR, NOCP, 16 + 3, 1)
FIELD(V7M_CFSR, STKOF, 16 + 4, 1)
FIELD(V7M_CFSR, UNALIGNED, 16 + 8, 1)
FIELD(V7M_CFSR, DIVBYZERO, 16 + 9, 1)

FIELD(V7M_CFSR, MMFSR, 0, 8)
FIELD(V7M_CFSR, BFSR, 8, 8)
FIELD(V7M_CFSR, UFSR, 16, 16)

FIELD(V7M_HFSR, VECTTBL, 1, 1)
FIELD(V7M_HFSR, FORCED, 30, 1)
FIELD(V7M_HFSR, DEBUGEVT, 31, 1)

FIELD(V7M_DFSR, HALTED, 0, 1)
FIELD(V7M_DFSR, BKPT, 1, 1)
FIELD(V7M_DFSR, DWTTRAP, 2, 1)
FIELD(V7M_DFSR, VCATCH, 3, 1)
FIELD(V7M_DFSR, EXTERNAL, 4, 1)

FIELD(V7M_SFSR, INVEP, 0, 1)
FIELD(V7M_SFSR, INVIS, 1, 1)
FIELD(V7M_SFSR, INVER, 2, 1)
FIELD(V7M_SFSR, AUVIOL, 3, 1)
FIELD(V7M_SFSR, INVTRAN, 4, 1)
FIELD(V7M_SFSR, LSPERR, 5, 1)
FIELD(V7M_SFSR, SFARVALID, 6, 1)
FIELD(V7M_SFSR, LSERR, 7, 1)

FIELD(V7M_MPU_CTRL, ENABLE, 0, 1)
FIELD(V7M_MPU_CTRL, HFNMIENA, 1, 1)
FIELD(V7M_MPU_CTRL, PRIVDEFENA, 2, 1)

FIELD(V7M_CLIDR, CTYPE_ALL, 0, 21)
FIELD(V7M_CLIDR, LOUIS, 21, 3)
FIELD(V7M_CLIDR, LOC, 24, 3)
FIELD(V7M_CLIDR, LOUU, 27, 3)
FIELD(V7M_CLIDR, ICB, 30, 2)

FIELD(V7M_CSSELR, IND, 0, 1)
FIELD(V7M_CSSELR, LEVEL, 1, 3)
FIELD(V7M_CSSELR, INDEX, 0, 4)

FIELD(V7M_FPCCR, LSPACT, 0, 1)
FIELD(V7M_FPCCR, USER, 1, 1)
FIELD(V7M_FPCCR, S, 2, 1)
FIELD(V7M_FPCCR, THREAD, 3, 1)
FIELD(V7M_FPCCR, HFRDY, 4, 1)
FIELD(V7M_FPCCR, MMRDY, 5, 1)
FIELD(V7M_FPCCR, BFRDY, 6, 1)
FIELD(V7M_FPCCR, SFRDY, 7, 1)
FIELD(V7M_FPCCR, MONRDY, 8, 1)
FIELD(V7M_FPCCR, SPLIMVIOL, 9, 1)
FIELD(V7M_FPCCR, UFRDY, 10, 1)
FIELD(V7M_FPCCR, RES0, 11, 15)
FIELD(V7M_FPCCR, TS, 26, 1)
FIELD(V7M_FPCCR, CLRONRETS, 27, 1)
FIELD(V7M_FPCCR, CLRONRET, 28, 1)
FIELD(V7M_FPCCR, LSPENS, 29, 1)
FIELD(V7M_FPCCR, LSPEN, 30, 1)
FIELD(V7M_FPCCR, ASPEN, 31, 1)
#define R_V7M_FPCCR_BANKED_MASK                 \
    (R_V7M_FPCCR_LSPACT_MASK |                  \
     R_V7M_FPCCR_USER_MASK |                    \
     R_V7M_FPCCR_THREAD_MASK |                  \
     R_V7M_FPCCR_MMRDY_MASK |                   \
     R_V7M_FPCCR_SPLIMVIOL_MASK |               \
     R_V7M_FPCCR_UFRDY_MASK |                   \
     R_V7M_FPCCR_ASPEN_MASK)

FIELD(V7M_VPR, P0, 0, 16)
FIELD(V7M_VPR, MASK01, 16, 4)
FIELD(V7M_VPR, MASK23, 20, 4)

FIELD(CLIDR_EL1, CTYPE1, 0, 3)
FIELD(CLIDR_EL1, CTYPE2, 3, 3)
FIELD(CLIDR_EL1, CTYPE3, 6, 3)
FIELD(CLIDR_EL1, CTYPE4, 9, 3)
FIELD(CLIDR_EL1, CTYPE5, 12, 3)
FIELD(CLIDR_EL1, CTYPE6, 15, 3)
FIELD(CLIDR_EL1, CTYPE7, 18, 3)
FIELD(CLIDR_EL1, LOUIS, 21, 3)
FIELD(CLIDR_EL1, LOC, 24, 3)
FIELD(CLIDR_EL1, LOUU, 27, 3)
FIELD(CLIDR_EL1, ICB, 30, 3)

FIELD(CCSIDR_EL1, CCIDX_LINESIZE, 0, 3)
FIELD(CCSIDR_EL1, CCIDX_ASSOCIATIVITY, 3, 21)
FIELD(CCSIDR_EL1, CCIDX_NUMSETS, 32, 24)

FIELD(CCSIDR_EL1, LINESIZE, 0, 3)
FIELD(CCSIDR_EL1, ASSOCIATIVITY, 3, 10)
FIELD(CCSIDR_EL1, NUMSETS, 13, 15)

FIELD(CTR_EL0,  IMINLINE, 0, 4)
FIELD(CTR_EL0,  L1IP, 14, 2)
FIELD(CTR_EL0,  DMINLINE, 16, 4)
FIELD(CTR_EL0,  ERG, 20, 4)
FIELD(CTR_EL0,  CWG, 24, 4)
FIELD(CTR_EL0,  IDC, 28, 1)
FIELD(CTR_EL0,  DIC, 29, 1)
FIELD(CTR_EL0,  TMINLINE, 32, 6)

FIELD(MIDR_EL1, REVISION, 0, 4)
FIELD(MIDR_EL1, PARTNUM, 4, 12)
FIELD(MIDR_EL1, ARCHITECTURE, 16, 4)
FIELD(MIDR_EL1, VARIANT, 20, 4)
FIELD(MIDR_EL1, IMPLEMENTER, 24, 8)

FIELD(ID_ISAR0, SWAP, 0, 4)
FIELD(ID_ISAR0, BITCOUNT, 4, 4)
FIELD(ID_ISAR0, BITFIELD, 8, 4)
FIELD(ID_ISAR0, CMPBRANCH, 12, 4)
FIELD(ID_ISAR0, COPROC, 16, 4)
FIELD(ID_ISAR0, DEBUG, 20, 4)
FIELD(ID_ISAR0, DIVIDE, 24, 4)

FIELD(ID_ISAR1, ENDIAN, 0, 4)
FIELD(ID_ISAR1, EXCEPT, 4, 4)
FIELD(ID_ISAR1, EXCEPT_AR, 8, 4)
FIELD(ID_ISAR1, EXTEND, 12, 4)
FIELD(ID_ISAR1, IFTHEN, 16, 4)
FIELD(ID_ISAR1, IMMEDIATE, 20, 4)
FIELD(ID_ISAR1, INTERWORK, 24, 4)
FIELD(ID_ISAR1, JAZELLE, 28, 4)

FIELD(ID_ISAR2, LOADSTORE, 0, 4)
FIELD(ID_ISAR2, MEMHINT, 4, 4)
FIELD(ID_ISAR2, MULTIACCESSINT, 8, 4)
FIELD(ID_ISAR2, MULT, 12, 4)
FIELD(ID_ISAR2, MULTS, 16, 4)
FIELD(ID_ISAR2, MULTU, 20, 4)
FIELD(ID_ISAR2, PSR_AR, 24, 4)
FIELD(ID_ISAR2, REVERSAL, 28, 4)

FIELD(ID_ISAR3, SATURATE, 0, 4)
FIELD(ID_ISAR3, SIMD, 4, 4)
FIELD(ID_ISAR3, SVC, 8, 4)
FIELD(ID_ISAR3, SYNCHPRIM, 12, 4)
FIELD(ID_ISAR3, TABBRANCH, 16, 4)
FIELD(ID_ISAR3, T32COPY, 20, 4)
FIELD(ID_ISAR3, TRUENOP, 24, 4)
FIELD(ID_ISAR3, T32EE, 28, 4)

FIELD(ID_ISAR4, UNPRIV, 0, 4)
FIELD(ID_ISAR4, WITHSHIFTS, 4, 4)
FIELD(ID_ISAR4, WRITEBACK, 8, 4)
FIELD(ID_ISAR4, SMC, 12, 4)
FIELD(ID_ISAR4, BARRIER, 16, 4)
FIELD(ID_ISAR4, SYNCHPRIM_FRAC, 20, 4)
FIELD(ID_ISAR4, PSR_M, 24, 4)
FIELD(ID_ISAR4, SWP_FRAC, 28, 4)

FIELD(ID_ISAR5, SEVL, 0, 4)
FIELD(ID_ISAR5, AES, 4, 4)
FIELD(ID_ISAR5, SHA1, 8, 4)
FIELD(ID_ISAR5, SHA2, 12, 4)
FIELD(ID_ISAR5, CRC32, 16, 4)
FIELD(ID_ISAR5, RDM, 24, 4)
FIELD(ID_ISAR5, VCMA, 28, 4)

FIELD(ID_ISAR6, JSCVT, 0, 4)
FIELD(ID_ISAR6, DP, 4, 4)
FIELD(ID_ISAR6, FHM, 8, 4)
FIELD(ID_ISAR6, SB, 12, 4)
FIELD(ID_ISAR6, SPECRES, 16, 4)
FIELD(ID_ISAR6, BF16, 20, 4)
FIELD(ID_ISAR6, I8MM, 24, 4)

FIELD(ID_MMFR0, VMSA, 0, 4)
FIELD(ID_MMFR0, PMSA, 4, 4)
FIELD(ID_MMFR0, OUTERSHR, 8, 4)
FIELD(ID_MMFR0, SHARELVL, 12, 4)
FIELD(ID_MMFR0, TCM, 16, 4)
FIELD(ID_MMFR0, AUXREG, 20, 4)
FIELD(ID_MMFR0, FCSE, 24, 4)
FIELD(ID_MMFR0, INNERSHR, 28, 4)

FIELD(ID_MMFR1, L1HVDVA, 0, 4)
FIELD(ID_MMFR1, L1UNIVA, 4, 4)
FIELD(ID_MMFR1, L1HVDSW, 8, 4)
FIELD(ID_MMFR1, L1UNISW, 12, 4)
FIELD(ID_MMFR1, L1HVD, 16, 4)
FIELD(ID_MMFR1, L1UNI, 20, 4)
FIELD(ID_MMFR1, L1TSTCLN, 24, 4)
FIELD(ID_MMFR1, BPRED, 28, 4)

FIELD(ID_MMFR2, L1HVDFG, 0, 4)
FIELD(ID_MMFR2, L1HVDBG, 4, 4)
FIELD(ID_MMFR2, L1HVDRNG, 8, 4)
FIELD(ID_MMFR2, HVDTLB, 12, 4)
FIELD(ID_MMFR2, UNITLB, 16, 4)
FIELD(ID_MMFR2, MEMBARR, 20, 4)
FIELD(ID_MMFR2, WFISTALL, 24, 4)
FIELD(ID_MMFR2, HWACCFLG, 28, 4)

FIELD(ID_MMFR3, CMAINTVA, 0, 4)
FIELD(ID_MMFR3, CMAINTSW, 4, 4)
FIELD(ID_MMFR3, BPMAINT, 8, 4)
FIELD(ID_MMFR3, MAINTBCST, 12, 4)
FIELD(ID_MMFR3, PAN, 16, 4)
FIELD(ID_MMFR3, COHWALK, 20, 4)
FIELD(ID_MMFR3, CMEMSZ, 24, 4)
FIELD(ID_MMFR3, SUPERSEC, 28, 4)

FIELD(ID_MMFR4, SPECSEI, 0, 4)
FIELD(ID_MMFR4, AC2, 4, 4)
FIELD(ID_MMFR4, XNX, 8, 4)
FIELD(ID_MMFR4, CNP, 12, 4)
FIELD(ID_MMFR4, HPDS, 16, 4)
FIELD(ID_MMFR4, LSM, 20, 4)
FIELD(ID_MMFR4, CCIDX, 24, 4)
FIELD(ID_MMFR4, EVT, 28, 4)

FIELD(ID_MMFR5, ETS, 0, 4)
FIELD(ID_MMFR5, NTLBPA, 4, 4)

FIELD(ID_PFR0, STATE0, 0, 4)
FIELD(ID_PFR0, STATE1, 4, 4)
FIELD(ID_PFR0, STATE2, 8, 4)
FIELD(ID_PFR0, STATE3, 12, 4)
FIELD(ID_PFR0, CSV2, 16, 4)
FIELD(ID_PFR0, AMU, 20, 4)
FIELD(ID_PFR0, DIT, 24, 4)
FIELD(ID_PFR0, RAS, 28, 4)

FIELD(ID_PFR1, PROGMOD, 0, 4)
FIELD(ID_PFR1, SECURITY, 4, 4)
FIELD(ID_PFR1, MPROGMOD, 8, 4)
FIELD(ID_PFR1, VIRTUALIZATION, 12, 4)
FIELD(ID_PFR1, GENTIMER, 16, 4)
FIELD(ID_PFR1, SEC_FRAC, 20, 4)
FIELD(ID_PFR1, VIRT_FRAC, 24, 4)
FIELD(ID_PFR1, GIC, 28, 4)

FIELD(ID_PFR2, CSV3, 0, 4)
FIELD(ID_PFR2, SSBS, 4, 4)
FIELD(ID_PFR2, RAS_FRAC, 8, 4)

FIELD(ID_AA64ISAR0, AES, 4, 4)
FIELD(ID_AA64ISAR0, SHA1, 8, 4)
FIELD(ID_AA64ISAR0, SHA2, 12, 4)
FIELD(ID_AA64ISAR0, CRC32, 16, 4)
FIELD(ID_AA64ISAR0, ATOMIC, 20, 4)
FIELD(ID_AA64ISAR0, TME, 24, 4)
FIELD(ID_AA64ISAR0, RDM, 28, 4)
FIELD(ID_AA64ISAR0, SHA3, 32, 4)
FIELD(ID_AA64ISAR0, SM3, 36, 4)
FIELD(ID_AA64ISAR0, SM4, 40, 4)
FIELD(ID_AA64ISAR0, DP, 44, 4)
FIELD(ID_AA64ISAR0, FHM, 48, 4)
FIELD(ID_AA64ISAR0, TS, 52, 4)
FIELD(ID_AA64ISAR0, TLB, 56, 4)
FIELD(ID_AA64ISAR0, RNDR, 60, 4)

FIELD(ID_AA64ISAR1, DPB, 0, 4)
FIELD(ID_AA64ISAR1, APA, 4, 4)
FIELD(ID_AA64ISAR1, API, 8, 4)
FIELD(ID_AA64ISAR1, JSCVT, 12, 4)
FIELD(ID_AA64ISAR1, FCMA, 16, 4)
FIELD(ID_AA64ISAR1, LRCPC, 20, 4)
FIELD(ID_AA64ISAR1, GPA, 24, 4)
FIELD(ID_AA64ISAR1, GPI, 28, 4)
FIELD(ID_AA64ISAR1, FRINTTS, 32, 4)
FIELD(ID_AA64ISAR1, SB, 36, 4)
FIELD(ID_AA64ISAR1, SPECRES, 40, 4)
FIELD(ID_AA64ISAR1, BF16, 44, 4)
FIELD(ID_AA64ISAR1, DGH, 48, 4)
FIELD(ID_AA64ISAR1, I8MM, 52, 4)
FIELD(ID_AA64ISAR1, XS, 56, 4)
FIELD(ID_AA64ISAR1, LS64, 60, 4)

FIELD(ID_AA64ISAR2, WFXT, 0, 4)
FIELD(ID_AA64ISAR2, RPRES, 4, 4)
FIELD(ID_AA64ISAR2, GPA3, 8, 4)
FIELD(ID_AA64ISAR2, APA3, 12, 4)
FIELD(ID_AA64ISAR2, MOPS, 16, 4)
FIELD(ID_AA64ISAR2, BC, 20, 4)
FIELD(ID_AA64ISAR2, PAC_FRAC, 24, 4)
FIELD(ID_AA64ISAR2, CLRBHB, 28, 4)
FIELD(ID_AA64ISAR2, SYSREG_128, 32, 4)
FIELD(ID_AA64ISAR2, SYSINSTR_128, 36, 4)
FIELD(ID_AA64ISAR2, PRFMSLC, 40, 4)
FIELD(ID_AA64ISAR2, RPRFM, 48, 4)
FIELD(ID_AA64ISAR2, CSSC, 52, 4)
FIELD(ID_AA64ISAR2, ATS1A, 60, 4)

FIELD(ID_AA64PFR0, EL0, 0, 4)
FIELD(ID_AA64PFR0, EL1, 4, 4)
FIELD(ID_AA64PFR0, EL2, 8, 4)
FIELD(ID_AA64PFR0, EL3, 12, 4)
FIELD(ID_AA64PFR0, FP, 16, 4)
FIELD(ID_AA64PFR0, ADVSIMD, 20, 4)
FIELD(ID_AA64PFR0, GIC, 24, 4)
FIELD(ID_AA64PFR0, RAS, 28, 4)
FIELD(ID_AA64PFR0, SVE, 32, 4)
FIELD(ID_AA64PFR0, SEL2, 36, 4)
FIELD(ID_AA64PFR0, MPAM, 40, 4)
FIELD(ID_AA64PFR0, AMU, 44, 4)
FIELD(ID_AA64PFR0, DIT, 48, 4)
FIELD(ID_AA64PFR0, RME, 52, 4)
FIELD(ID_AA64PFR0, CSV2, 56, 4)
FIELD(ID_AA64PFR0, CSV3, 60, 4)

FIELD(ID_AA64PFR1, BT, 0, 4)
FIELD(ID_AA64PFR1, SSBS, 4, 4)
FIELD(ID_AA64PFR1, MTE, 8, 4)
FIELD(ID_AA64PFR1, RAS_FRAC, 12, 4)
FIELD(ID_AA64PFR1, MPAM_FRAC, 16, 4)
FIELD(ID_AA64PFR1, SME, 24, 4)
FIELD(ID_AA64PFR1, RNDR_TRAP, 28, 4)
FIELD(ID_AA64PFR1, CSV2_FRAC, 32, 4)
FIELD(ID_AA64PFR1, NMI, 36, 4)
FIELD(ID_AA64PFR1, MTE_FRAC, 40, 4)
FIELD(ID_AA64PFR1, GCS, 44, 4)
FIELD(ID_AA64PFR1, THE, 48, 4)
FIELD(ID_AA64PFR1, MTEX, 52, 4)
FIELD(ID_AA64PFR1, DF2, 56, 4)
FIELD(ID_AA64PFR1, PFAR, 60, 4)

FIELD(ID_AA64MMFR0, PARANGE, 0, 4)
FIELD(ID_AA64MMFR0, ASIDBITS, 4, 4)
FIELD(ID_AA64MMFR0, BIGEND, 8, 4)
FIELD(ID_AA64MMFR0, SNSMEM, 12, 4)
FIELD(ID_AA64MMFR0, BIGENDEL0, 16, 4)
FIELD(ID_AA64MMFR0, TGRAN16, 20, 4)
FIELD(ID_AA64MMFR0, TGRAN64, 24, 4)
FIELD(ID_AA64MMFR0, TGRAN4, 28, 4)
FIELD(ID_AA64MMFR0, TGRAN16_2, 32, 4)
FIELD(ID_AA64MMFR0, TGRAN64_2, 36, 4)
FIELD(ID_AA64MMFR0, TGRAN4_2, 40, 4)
FIELD(ID_AA64MMFR0, EXS, 44, 4)
FIELD(ID_AA64MMFR0, FGT, 56, 4)
FIELD(ID_AA64MMFR0, ECV, 60, 4)

FIELD(ID_AA64MMFR1, HAFDBS, 0, 4)
FIELD(ID_AA64MMFR1, VMIDBITS, 4, 4)
FIELD(ID_AA64MMFR1, VH, 8, 4)
FIELD(ID_AA64MMFR1, HPDS, 12, 4)
FIELD(ID_AA64MMFR1, LO, 16, 4)
FIELD(ID_AA64MMFR1, PAN, 20, 4)
FIELD(ID_AA64MMFR1, SPECSEI, 24, 4)
FIELD(ID_AA64MMFR1, XNX, 28, 4)
FIELD(ID_AA64MMFR1, TWED, 32, 4)
FIELD(ID_AA64MMFR1, ETS, 36, 4)
FIELD(ID_AA64MMFR1, HCX, 40, 4)
FIELD(ID_AA64MMFR1, AFP, 44, 4)
FIELD(ID_AA64MMFR1, NTLBPA, 48, 4)
FIELD(ID_AA64MMFR1, TIDCP1, 52, 4)
FIELD(ID_AA64MMFR1, CMOW, 56, 4)
FIELD(ID_AA64MMFR1, ECBHB, 60, 4)

FIELD(ID_AA64MMFR2, CNP, 0, 4)
FIELD(ID_AA64MMFR2, UAO, 4, 4)
FIELD(ID_AA64MMFR2, LSM, 8, 4)
FIELD(ID_AA64MMFR2, IESB, 12, 4)
FIELD(ID_AA64MMFR2, VARANGE, 16, 4)
FIELD(ID_AA64MMFR2, CCIDX, 20, 4)
FIELD(ID_AA64MMFR2, NV, 24, 4)
FIELD(ID_AA64MMFR2, ST, 28, 4)
FIELD(ID_AA64MMFR2, AT, 32, 4)
FIELD(ID_AA64MMFR2, IDS, 36, 4)
FIELD(ID_AA64MMFR2, FWB, 40, 4)
FIELD(ID_AA64MMFR2, TTL, 48, 4)
FIELD(ID_AA64MMFR2, BBM, 52, 4)
FIELD(ID_AA64MMFR2, EVT, 56, 4)
FIELD(ID_AA64MMFR2, E0PD, 60, 4)

FIELD(ID_AA64MMFR3, TCRX, 0, 4)
FIELD(ID_AA64MMFR3, SCTLRX, 4, 4)
FIELD(ID_AA64MMFR3, S1PIE, 8, 4)
FIELD(ID_AA64MMFR3, S2PIE, 12, 4)
FIELD(ID_AA64MMFR3, S1POE, 16, 4)
FIELD(ID_AA64MMFR3, S2POE, 20, 4)
FIELD(ID_AA64MMFR3, AIE, 24, 4)
FIELD(ID_AA64MMFR3, MEC, 28, 4)
FIELD(ID_AA64MMFR3, D128, 32, 4)
FIELD(ID_AA64MMFR3, D128_2, 36, 4)
FIELD(ID_AA64MMFR3, SNERR, 40, 4)
FIELD(ID_AA64MMFR3, ANERR, 44, 4)
FIELD(ID_AA64MMFR3, SDERR, 52, 4)
FIELD(ID_AA64MMFR3, ADERR, 56, 4)
FIELD(ID_AA64MMFR3, SPEC_FPACC, 60, 4)

FIELD(ID_AA64DFR0, DEBUGVER, 0, 4)
FIELD(ID_AA64DFR0, TRACEVER, 4, 4)
FIELD(ID_AA64DFR0, PMUVER, 8, 4)
FIELD(ID_AA64DFR0, BRPS, 12, 4)
FIELD(ID_AA64DFR0, PMSS, 16, 4)
FIELD(ID_AA64DFR0, WRPS, 20, 4)
FIELD(ID_AA64DFR0, SEBEP, 24, 4)
FIELD(ID_AA64DFR0, CTX_CMPS, 28, 4)
FIELD(ID_AA64DFR0, PMSVER, 32, 4)
FIELD(ID_AA64DFR0, DOUBLELOCK, 36, 4)
FIELD(ID_AA64DFR0, TRACEFILT, 40, 4)
FIELD(ID_AA64DFR0, TRACEBUFFER, 44, 4)
FIELD(ID_AA64DFR0, MTPMU, 48, 4)
FIELD(ID_AA64DFR0, BRBE, 52, 4)
FIELD(ID_AA64DFR0, EXTTRCBUFF, 56, 4)
FIELD(ID_AA64DFR0, HPMN0, 60, 4)

FIELD(ID_AA64ZFR0, SVEVER, 0, 4)
FIELD(ID_AA64ZFR0, AES, 4, 4)
FIELD(ID_AA64ZFR0, BITPERM, 16, 4)
FIELD(ID_AA64ZFR0, BFLOAT16, 20, 4)
FIELD(ID_AA64ZFR0, B16B16, 24, 4)
FIELD(ID_AA64ZFR0, SHA3, 32, 4)
FIELD(ID_AA64ZFR0, SM4, 40, 4)
FIELD(ID_AA64ZFR0, I8MM, 44, 4)
FIELD(ID_AA64ZFR0, F32MM, 52, 4)
FIELD(ID_AA64ZFR0, F64MM, 56, 4)

FIELD(ID_AA64SMFR0, F32F32, 32, 1)
FIELD(ID_AA64SMFR0, BI32I32, 33, 1)
FIELD(ID_AA64SMFR0, B16F32, 34, 1)
FIELD(ID_AA64SMFR0, F16F32, 35, 1)
FIELD(ID_AA64SMFR0, I8I32, 36, 4)
FIELD(ID_AA64SMFR0, F16F16, 42, 1)
FIELD(ID_AA64SMFR0, B16B16, 43, 1)
FIELD(ID_AA64SMFR0, I16I32, 44, 4)
FIELD(ID_AA64SMFR0, F64F64, 48, 1)
FIELD(ID_AA64SMFR0, I16I64, 52, 4)
FIELD(ID_AA64SMFR0, SMEVER, 56, 4)
FIELD(ID_AA64SMFR0, FA64, 63, 1)

FIELD(ID_DFR0, COPDBG, 0, 4)
FIELD(ID_DFR0, COPSDBG, 4, 4)
FIELD(ID_DFR0, MMAPDBG, 8, 4)
FIELD(ID_DFR0, COPTRC, 12, 4)
FIELD(ID_DFR0, MMAPTRC, 16, 4)
FIELD(ID_DFR0, MPROFDBG, 20, 4)
FIELD(ID_DFR0, PERFMON, 24, 4)
FIELD(ID_DFR0, TRACEFILT, 28, 4)

FIELD(ID_DFR1, MTPMU, 0, 4)
FIELD(ID_DFR1, HPMN0, 4, 4)

FIELD(DBGDIDR, SE_IMP, 12, 1)
FIELD(DBGDIDR, NSUHD_IMP, 14, 1)
FIELD(DBGDIDR, VERSION, 16, 4)
FIELD(DBGDIDR, CTX_CMPS, 20, 4)
FIELD(DBGDIDR, BRPS, 24, 4)
FIELD(DBGDIDR, WRPS, 28, 4)

FIELD(DBGDEVID, PCSAMPLE, 0, 4)
FIELD(DBGDEVID, WPADDRMASK, 4, 4)
FIELD(DBGDEVID, BPADDRMASK, 8, 4)
FIELD(DBGDEVID, VECTORCATCH, 12, 4)
FIELD(DBGDEVID, VIRTEXTNS, 16, 4)
FIELD(DBGDEVID, DOUBLELOCK, 20, 4)
FIELD(DBGDEVID, AUXREGS, 24, 4)
FIELD(DBGDEVID, CIDMASK, 28, 4)

FIELD(DBGDEVID1, PCSROFFSET, 0, 4)

FIELD(MVFR0, SIMDREG, 0, 4)
FIELD(MVFR0, FPSP, 4, 4)
FIELD(MVFR0, FPDP, 8, 4)
FIELD(MVFR0, FPTRAP, 12, 4)
FIELD(MVFR0, FPDIVIDE, 16, 4)
FIELD(MVFR0, FPSQRT, 20, 4)
FIELD(MVFR0, FPSHVEC, 24, 4)
FIELD(MVFR0, FPROUND, 28, 4)

FIELD(MVFR1, FPFTZ, 0, 4)
FIELD(MVFR1, FPDNAN, 4, 4)
FIELD(MVFR1, SIMDLS, 8, 4) /* A-profile only */
FIELD(MVFR1, SIMDINT, 12, 4) /* A-profile only */
FIELD(MVFR1, SIMDSP, 16, 4) /* A-profile only */
FIELD(MVFR1, SIMDHP, 20, 4) /* A-profile only */
FIELD(MVFR1, MVE, 8, 4) /* M-profile only */
FIELD(MVFR1, FP16, 20, 4) /* M-profile only */
FIELD(MVFR1, FPHP, 24, 4)
FIELD(MVFR1, SIMDFMAC, 28, 4)

FIELD(MVFR2, SIMDMISC, 0, 4)
FIELD(MVFR2, FPMISC, 4, 4)

FIELD(GPCCR, PPS, 0, 3)
FIELD(GPCCR, IRGN, 8, 2)
FIELD(GPCCR, ORGN, 10, 2)
FIELD(GPCCR, SH, 12, 2)
FIELD(GPCCR, PGS, 14, 2)
FIELD(GPCCR, GPC, 16, 1)
FIELD(GPCCR, GPCP, 17, 1)
FIELD(GPCCR, L0GPTSZ, 20, 4)

FIELD(MFAR, FPA, 12, 40)
FIELD(MFAR, NSE, 62, 1)
FIELD(MFAR, NS, 63, 1)

QEMU_BUILD_BUG_ON(ARRAY_SIZE(((ARMCPU *)0)->ccsidr) <= R_V7M_CSSELR_INDEX_MASK);

enum arm_features {
    ARM_FEATURE_AUXCR,  /* ARM1026 Auxiliary control register.  */
    ARM_FEATURE_XSCALE, /* Intel XScale extensions.  */
    ARM_FEATURE_IWMMXT, /* Intel iwMMXt extension.  */
    ARM_FEATURE_V6,
    ARM_FEATURE_V6K,
    ARM_FEATURE_V7,
    ARM_FEATURE_THUMB2,
    ARM_FEATURE_PMSA,   /* no MMU; may have Memory Protection Unit */
    ARM_FEATURE_NEON,
    ARM_FEATURE_M, /* Microcontroller profile.  */
    ARM_FEATURE_OMAPCP, /* OMAP specific CP15 ops handling.  */
    ARM_FEATURE_THUMB2EE,
    ARM_FEATURE_V7MP,    /* v7 Multiprocessing Extensions */
    ARM_FEATURE_V7VE, /* v7 Virtualization Extensions (non-EL2 parts) */
    ARM_FEATURE_V4T,
    ARM_FEATURE_V5,
    ARM_FEATURE_STRONGARM,
    ARM_FEATURE_VAPA, /* cp15 VA to PA lookups */
    ARM_FEATURE_GENERIC_TIMER,
    ARM_FEATURE_MVFR, /* Media and VFP Feature Registers 0 and 1 */
    ARM_FEATURE_DUMMY_C15_REGS, /* RAZ/WI all of cp15 crn=15 */
    ARM_FEATURE_CACHE_TEST_CLEAN, /* 926/1026 style test-and-clean ops */
    ARM_FEATURE_CACHE_DIRTY_REG, /* 1136/1176 cache dirty status register */
    ARM_FEATURE_CACHE_BLOCK_OPS, /* v6 optional cache block operations */
    ARM_FEATURE_MPIDR, /* has cp15 MPIDR */
    ARM_FEATURE_LPAE, /* has Large Physical Address Extension */
    ARM_FEATURE_V8,
    ARM_FEATURE_AARCH64, /* supports 64 bit mode */
    ARM_FEATURE_CBAR, /* has cp15 CBAR */
    ARM_FEATURE_CBAR_RO, /* has cp15 CBAR and it is read-only */
    ARM_FEATURE_EL2, /* has EL2 Virtualization support */
    ARM_FEATURE_EL3, /* has EL3 Secure monitor support */
    ARM_FEATURE_THUMB_DSP, /* DSP insns supported in the Thumb encodings */
    ARM_FEATURE_PMU, /* has PMU support */
    ARM_FEATURE_VBAR, /* has cp15 VBAR */
    ARM_FEATURE_M_SECURITY, /* M profile Security Extension */
    ARM_FEATURE_M_MAIN, /* M profile Main Extension */
    ARM_FEATURE_V8_1M, /* M profile extras only in v8.1M and later */
    ARM_FEATURE_BACKCOMPAT_CNTFRQ, /* 62.5MHz timer default */
};

static inline int arm_feature(CPUARMState *env, int feature)
{
    return (env->features & (1ULL << feature)) != 0;
}

void arm_cpu_finalize_features(ARMCPU *cpu, Error **errp);


typedef enum ARMSecuritySpace {
    ARMSS_Secure     = 0,
    ARMSS_NonSecure  = 1,
    ARMSS_Root       = 2,
    ARMSS_Realm      = 3,
} ARMSecuritySpace;

static inline bool arm_space_is_secure(ARMSecuritySpace space)
{
    return space == ARMSS_Secure || space == ARMSS_Root;
}

static inline ARMSecuritySpace arm_secure_to_space(bool secure)
{
    return secure ? ARMSS_Secure : ARMSS_NonSecure;
}

#if !defined(CONFIG_USER_ONLY)
ARMSecuritySpace arm_security_space_below_el3(CPUARMState *env);

static inline bool arm_is_secure_below_el3(CPUARMState *env)
{
    ARMSecuritySpace ss = arm_security_space_below_el3(env);
    return ss == ARMSS_Secure;
}

static inline bool arm_is_el3_or_mon(CPUARMState *env)
{
    assert(!arm_feature(env, ARM_FEATURE_M));
    if (arm_feature(env, ARM_FEATURE_EL3)) {
        if (is_a64(env) && extract32(env->pstate, 2, 2) == 3) {
            return true;
        } else if (!is_a64(env) &&
                (env->uncached_cpsr & CPSR_M) == ARM_CPU_MODE_MON) {
            return true;
        }
    }
    return false;
}

ARMSecuritySpace arm_security_space(CPUARMState *env);

static inline bool arm_is_secure(CPUARMState *env)
{
    return arm_space_is_secure(arm_security_space(env));
}

static inline bool arm_is_el2_enabled_secstate(CPUARMState *env,
                                               ARMSecuritySpace space)
{
    assert(space != ARMSS_Root);
    return arm_feature(env, ARM_FEATURE_EL2)
           && (space != ARMSS_Secure || (env->cp15.scr_el3 & SCR_EEL2));
}

static inline bool arm_is_el2_enabled(CPUARMState *env)
{
    return arm_is_el2_enabled_secstate(env, arm_security_space_below_el3(env));
}

#else
static inline ARMSecuritySpace arm_security_space_below_el3(CPUARMState *env)
{
    return ARMSS_NonSecure;
}

static inline bool arm_is_secure_below_el3(CPUARMState *env)
{
    return false;
}

static inline bool arm_is_el3_or_mon(CPUARMState *env)
{
    return false;
}

static inline ARMSecuritySpace arm_security_space(CPUARMState *env)
{
    return ARMSS_NonSecure;
}

static inline bool arm_is_secure(CPUARMState *env)
{
    return false;
}

static inline bool arm_is_el2_enabled_secstate(CPUARMState *env,
                                               ARMSecuritySpace space)
{
    return false;
}

static inline bool arm_is_el2_enabled(CPUARMState *env)
{
    return false;
}
#endif

uint64_t arm_hcr_el2_eff_secstate(CPUARMState *env, ARMSecuritySpace space);
uint64_t arm_hcr_el2_eff(CPUARMState *env);
uint64_t arm_hcrx_el2_eff(CPUARMState *env);

bool access_secure_reg(CPUARMState *env);

uint32_t arm_phys_excp_target_el(CPUState *cs, uint32_t excp_idx,
                                 uint32_t cur_el, bool secure);

static inline int arm_highest_el(CPUARMState *env)
{
    if (arm_feature(env, ARM_FEATURE_EL3)) {
        return 3;
    }
    if (arm_feature(env, ARM_FEATURE_EL2)) {
        return 2;
    }
    return 1;
}

static inline bool arm_v7m_is_handler_mode(CPUARMState *env)
{
    return env->v7m.exception != 0;
}

bool write_list_to_cpustate(ARMCPU *cpu);

bool write_cpustate_to_list(ARMCPU *cpu);

#define ARM_CPUID_TI915T      0x54029152
#define ARM_CPUID_TI925T      0x54029252

#define CPU_RESOLVING_TYPE TYPE_ARM_CPU

#define TYPE_ARM_HOST_CPU "host-" TYPE_ARM_CPU

#define ARM_MMU_IDX_A     0x10  /* A profile */
#define ARM_MMU_IDX_NOTLB 0x20  /* does not have a TLB */
#define ARM_MMU_IDX_M     0x40  /* M profile */

#define ARM_MMU_IDX_M_PRIV   0x1
#define ARM_MMU_IDX_M_NEGPRI 0x2
#define ARM_MMU_IDX_M_S      0x4  /* Secure */

#define ARM_MMU_IDX_TYPE_MASK \
    (ARM_MMU_IDX_A | ARM_MMU_IDX_M | ARM_MMU_IDX_NOTLB)
#define ARM_MMU_IDX_COREIDX_MASK 0xf

typedef enum ARMMMUIdx {
    ARMMMUIdx_E10_0     = 0 | ARM_MMU_IDX_A,
    ARMMMUIdx_E20_0     = 1 | ARM_MMU_IDX_A,
    ARMMMUIdx_E10_1     = 2 | ARM_MMU_IDX_A,
    ARMMMUIdx_E20_2     = 3 | ARM_MMU_IDX_A,
    ARMMMUIdx_E10_1_PAN = 4 | ARM_MMU_IDX_A,
    ARMMMUIdx_E20_2_PAN = 5 | ARM_MMU_IDX_A,
    ARMMMUIdx_E2        = 6 | ARM_MMU_IDX_A,
    ARMMMUIdx_E3        = 7 | ARM_MMU_IDX_A,
    ARMMMUIdx_E30_0     = 8 | ARM_MMU_IDX_A,
    ARMMMUIdx_E30_3_PAN = 9 | ARM_MMU_IDX_A,

    ARMMMUIdx_Stage2_S  = 10 | ARM_MMU_IDX_A,
    ARMMMUIdx_Stage2    = 11 | ARM_MMU_IDX_A,

    ARMMMUIdx_Phys_S     = 12 | ARM_MMU_IDX_A,
    ARMMMUIdx_Phys_NS    = 13 | ARM_MMU_IDX_A,
    ARMMMUIdx_Phys_Root  = 14 | ARM_MMU_IDX_A,
    ARMMMUIdx_Phys_Realm = 15 | ARM_MMU_IDX_A,

    ARMMMUIdx_Stage1_E0 = 0 | ARM_MMU_IDX_NOTLB,
    ARMMMUIdx_Stage1_E1 = 1 | ARM_MMU_IDX_NOTLB,
    ARMMMUIdx_Stage1_E1_PAN = 2 | ARM_MMU_IDX_NOTLB,

    ARMMMUIdx_MUser = ARM_MMU_IDX_M,
    ARMMMUIdx_MPriv = ARM_MMU_IDX_M | ARM_MMU_IDX_M_PRIV,
    ARMMMUIdx_MUserNegPri = ARMMMUIdx_MUser | ARM_MMU_IDX_M_NEGPRI,
    ARMMMUIdx_MPrivNegPri = ARMMMUIdx_MPriv | ARM_MMU_IDX_M_NEGPRI,
    ARMMMUIdx_MSUser = ARMMMUIdx_MUser | ARM_MMU_IDX_M_S,
    ARMMMUIdx_MSPriv = ARMMMUIdx_MPriv | ARM_MMU_IDX_M_S,
    ARMMMUIdx_MSUserNegPri = ARMMMUIdx_MUserNegPri | ARM_MMU_IDX_M_S,
    ARMMMUIdx_MSPrivNegPri = ARMMMUIdx_MPrivNegPri | ARM_MMU_IDX_M_S,
} ARMMMUIdx;

#define TO_CORE_BIT(NAME) \
    ARMMMUIdxBit_##NAME = 1 << (ARMMMUIdx_##NAME & ARM_MMU_IDX_COREIDX_MASK)

typedef enum ARMMMUIdxBit {
    TO_CORE_BIT(E10_0),
    TO_CORE_BIT(E20_0),
    TO_CORE_BIT(E10_1),
    TO_CORE_BIT(E10_1_PAN),
    TO_CORE_BIT(E2),
    TO_CORE_BIT(E20_2),
    TO_CORE_BIT(E20_2_PAN),
    TO_CORE_BIT(E3),
    TO_CORE_BIT(E30_0),
    TO_CORE_BIT(E30_3_PAN),
    TO_CORE_BIT(Stage2),
    TO_CORE_BIT(Stage2_S),

    TO_CORE_BIT(MUser),
    TO_CORE_BIT(MPriv),
    TO_CORE_BIT(MUserNegPri),
    TO_CORE_BIT(MPrivNegPri),
    TO_CORE_BIT(MSUser),
    TO_CORE_BIT(MSPriv),
    TO_CORE_BIT(MSUserNegPri),
    TO_CORE_BIT(MSPrivNegPri),
} ARMMMUIdxBit;

#undef TO_CORE_BIT

#define MMU_USER_IDX 0

typedef enum ARMASIdx {
    ARMASIdx_NS = 0,
    ARMASIdx_S = 1,
    ARMASIdx_TagNS = 2,
    ARMASIdx_TagS = 3,
} ARMASIdx;

static inline ARMMMUIdx arm_space_to_phys(ARMSecuritySpace space)
{
    QEMU_BUILD_BUG_ON(ARMSS_Secure != 0);
    QEMU_BUILD_BUG_ON(ARMMMUIdx_Phys_NS != ARMMMUIdx_Phys_S + ARMSS_NonSecure);
    QEMU_BUILD_BUG_ON(ARMMMUIdx_Phys_Root != ARMMMUIdx_Phys_S + ARMSS_Root);
    QEMU_BUILD_BUG_ON(ARMMMUIdx_Phys_Realm != ARMMMUIdx_Phys_S + ARMSS_Realm);

    return ARMMMUIdx_Phys_S + space;
}

static inline ARMSecuritySpace arm_phys_to_space(ARMMMUIdx idx)
{
    assert(idx >= ARMMMUIdx_Phys_S && idx <= ARMMMUIdx_Phys_Realm);
    return idx - ARMMMUIdx_Phys_S;
}

static inline bool arm_v7m_csselr_razwi(ARMCPU *cpu)
{
    return (cpu->clidr & R_V7M_CLIDR_CTYPE_ALL_MASK) != 0;
}

static inline bool arm_sctlr_b(CPUARMState *env)
{
    return
#ifndef CONFIG_USER_ONLY
        !arm_feature(env, ARM_FEATURE_V7) &&
#endif
        (env->cp15.sctlr_el[1] & SCTLR_B) != 0;
}

uint64_t arm_sctlr(CPUARMState *env, int el);

#include "exec/cpu-all.h"

FIELD(TBFLAG_ANY, AARCH64_STATE, 0, 1)
FIELD(TBFLAG_ANY, SS_ACTIVE, 1, 1)
FIELD(TBFLAG_ANY, PSTATE__SS, 2, 1)      /* Not cached. */
FIELD(TBFLAG_ANY, BE_DATA, 3, 1)
FIELD(TBFLAG_ANY, MMUIDX, 4, 4)
FIELD(TBFLAG_ANY, FPEXC_EL, 8, 2)
FIELD(TBFLAG_ANY, ALIGN_MEM, 10, 1)
FIELD(TBFLAG_ANY, PSTATE__IL, 11, 1)
FIELD(TBFLAG_ANY, FGT_ACTIVE, 12, 1)
FIELD(TBFLAG_ANY, FGT_SVC, 13, 1)

FIELD(TBFLAG_AM32, CONDEXEC, 24, 8)      /* Not cached. */
FIELD(TBFLAG_AM32, THUMB, 23, 1)         /* Not cached. */

FIELD(TBFLAG_A32, VECLEN, 0, 3)         /* Not cached. */
FIELD(TBFLAG_A32, VECSTRIDE, 3, 2)     /* Not cached. */
FIELD(TBFLAG_A32, XSCALE_CPAR, 5, 2)
FIELD(TBFLAG_A32, VFPEN, 7, 1)         /* Partially cached, minus FPEXC. */
FIELD(TBFLAG_A32, SCTLR__B, 8, 1)      /* Cannot overlap with SCTLR_B */
FIELD(TBFLAG_A32, HSTR_ACTIVE, 9, 1)
FIELD(TBFLAG_A32, NS, 10, 1)
FIELD(TBFLAG_A32, SME_TRAP_NONSTREAMING, 11, 1)

FIELD(TBFLAG_M32, HANDLER, 0, 1)
FIELD(TBFLAG_M32, STACKCHECK, 1, 1)
FIELD(TBFLAG_M32, LSPACT, 2, 1)                 /* Not cached. */
FIELD(TBFLAG_M32, NEW_FP_CTXT_NEEDED, 3, 1)     /* Not cached. */
FIELD(TBFLAG_M32, FPCCR_S_WRONG, 4, 1)          /* Not cached. */
FIELD(TBFLAG_M32, MVE_NO_PRED, 5, 1)            /* Not cached. */
FIELD(TBFLAG_M32, SECURE, 6, 1)

FIELD(TBFLAG_A64, TBII, 0, 2)
FIELD(TBFLAG_A64, SVEEXC_EL, 2, 2)
FIELD(TBFLAG_A64, VL, 4, 4)
FIELD(TBFLAG_A64, PAUTH_ACTIVE, 8, 1)
FIELD(TBFLAG_A64, BT, 9, 1)
FIELD(TBFLAG_A64, BTYPE, 10, 2)         /* Not cached. */
FIELD(TBFLAG_A64, TBID, 12, 2)
FIELD(TBFLAG_A64, UNPRIV, 14, 1)
FIELD(TBFLAG_A64, ATA, 15, 1)
FIELD(TBFLAG_A64, TCMA, 16, 2)
FIELD(TBFLAG_A64, MTE_ACTIVE, 18, 1)
FIELD(TBFLAG_A64, MTE0_ACTIVE, 19, 1)
FIELD(TBFLAG_A64, SMEEXC_EL, 20, 2)
FIELD(TBFLAG_A64, PSTATE_SM, 22, 1)
FIELD(TBFLAG_A64, PSTATE_ZA, 23, 1)
FIELD(TBFLAG_A64, SVL, 24, 4)
FIELD(TBFLAG_A64, SME_TRAP_NONSTREAMING, 28, 1)
FIELD(TBFLAG_A64, TRAP_ERET, 29, 1)
FIELD(TBFLAG_A64, NAA, 30, 1)
FIELD(TBFLAG_A64, ATA0, 31, 1)
FIELD(TBFLAG_A64, NV, 32, 1)
FIELD(TBFLAG_A64, NV1, 33, 1)
FIELD(TBFLAG_A64, NV2, 34, 1)
FIELD(TBFLAG_A64, NV2_MEM_E20, 35, 1)
FIELD(TBFLAG_A64, NV2_MEM_BE, 36, 1)
FIELD(TBFLAG_A64, AH, 37, 1)   /* FPCR.AH */
FIELD(TBFLAG_A64, NEP, 38, 1)   /* FPCR.NEP */

#define DP_TBFLAG_ANY(DST, WHICH, VAL) \
    (DST.flags = FIELD_DP32(DST.flags, TBFLAG_ANY, WHICH, VAL))
#define DP_TBFLAG_A64(DST, WHICH, VAL) \
    (DST.flags2 = FIELD_DP64(DST.flags2, TBFLAG_A64, WHICH, VAL))
#define DP_TBFLAG_A32(DST, WHICH, VAL) \
    (DST.flags2 = FIELD_DP32(DST.flags2, TBFLAG_A32, WHICH, VAL))
#define DP_TBFLAG_M32(DST, WHICH, VAL) \
    (DST.flags2 = FIELD_DP32(DST.flags2, TBFLAG_M32, WHICH, VAL))
#define DP_TBFLAG_AM32(DST, WHICH, VAL) \
    (DST.flags2 = FIELD_DP32(DST.flags2, TBFLAG_AM32, WHICH, VAL))

#define EX_TBFLAG_ANY(IN, WHICH)   FIELD_EX32(IN.flags, TBFLAG_ANY, WHICH)
#define EX_TBFLAG_A64(IN, WHICH)   FIELD_EX64(IN.flags2, TBFLAG_A64, WHICH)
#define EX_TBFLAG_A32(IN, WHICH)   FIELD_EX32(IN.flags2, TBFLAG_A32, WHICH)
#define EX_TBFLAG_M32(IN, WHICH)   FIELD_EX32(IN.flags2, TBFLAG_M32, WHICH)
#define EX_TBFLAG_AM32(IN, WHICH)  FIELD_EX32(IN.flags2, TBFLAG_AM32, WHICH)

static inline int sve_vq(CPUARMState *env)
{
    return EX_TBFLAG_A64(env->hflags, VL) + 1;
}

static inline int sme_vq(CPUARMState *env)
{
    return EX_TBFLAG_A64(env->hflags, SVL) + 1;
}

static inline bool bswap_code(bool sctlr_b)
{
#ifdef CONFIG_USER_ONLY
    return TARGET_BIG_ENDIAN ^ sctlr_b;
#else
    return 0;
#endif
}

void cpu_get_tb_cpu_state(CPUARMState *env, vaddr *pc,
                          uint64_t *cs_base, uint32_t *flags);

enum {
    QEMU_PSCI_CONDUIT_DISABLED = 0,
    QEMU_PSCI_CONDUIT_SMC = 1,
    QEMU_PSCI_CONDUIT_HVC = 2,
};

#define QEMU_PSCI_VERSION_0_1 0x00001
#define QEMU_PSCI_VERSION_0_2 0x00020000
#define QEMU_PSCI_VERSION_1_0 0x00010000
#define QEMU_PSCI_VERSION_1_1 0x00010001

#define QEMU_PSCI_0_2_64BIT 0x40000000

#define QEMU_PSCI_0_1_FN_CPU_OFF       1
#define QEMU_PSCI_0_1_FN_CPU_SUSPEND   0
#define QEMU_PSCI_0_1_FN_CPU_ON        3
#define QEMU_PSCI_0_1_FN_MIGRATE       5

#define QEMU_PSCI_0_2_FN_PSCI_VERSION      0x84000000
#define QEMU_PSCI_0_2_FN_CPU_SUSPEND       0x84000001
#define QEMU_PSCI_0_2_FN64_CPU_SUSPEND     0xc4000001
#define QEMU_PSCI_0_2_FN_CPU_OFF           0x84000002
#define QEMU_PSCI_0_2_FN_CPU_ON            0x84000003
#define QEMU_PSCI_0_2_FN64_CPU_ON          0xc4000003
#define QEMU_PSCI_0_2_FN_AFFINITY_INFO     0x84000004
#define QEMU_PSCI_0_2_FN64_AFFINITY_INFO   0xc4000004
#define QEMU_PSCI_0_2_FN_MIGRATE_INFO_TYPE 0x84000005
#define QEMU_PSCI_0_2_FN_MIGRATE           0x84000006
#define QEMU_PSCI_0_2_FN64_MIGRATE         0xc4000006
#define QEMU_PSCI_0_2_FN_SYSTEM_RESET      0x84000007
#define QEMU_PSCI_0_2_FN_SYSTEM_OFF         0x84000008

#define QEMU_PSCI_0_2_RET_TOS_MIGRATION_NOT_REQUIRED 0
#define QEMU_PSCI_0_2_RET_TOS_MIGRATION_REQUIRED     1
#define QEMU_PSCI_0_2_RET_TOS_MIGRATION_NOT_SUPPORTED 2

#define QEMU_PSCI_1_0_FN_PSCI_FEATURES 0x8400000a

#ifndef CONFIG_USER_ONLY
static inline int arm_asidx_from_attrs(CPUState *cs, MemTxAttrs attrs)
{
    return attrs.secure ? ARMASIdx_S : ARMASIdx_NS;
}

static inline AddressSpace *arm_addressspace(CPUState *cs, MemTxAttrs attrs)
{
    return cpu_get_address_space(cs, arm_asidx_from_attrs(cs, attrs));
}
#endif

void arm_register_pre_el_change_hook(ARMCPU *cpu, ARMELChangeHookFn *hook,
                                 void *opaque);
void arm_register_el_change_hook(ARMCPU *cpu, ARMELChangeHookFn *hook, void
        *opaque);

void arm_rebuild_hflags(CPUARMState *env);

static inline uint64_t *aa32_vfp_dreg(CPUARMState *env, unsigned regno)
{
    return &env->vfp.zregs[regno >> 1].d[regno & 1];
}

static inline uint64_t *aa32_vfp_qreg(CPUARMState *env, unsigned regno)
{
    return &env->vfp.zregs[regno].d[0];
}

static inline uint64_t *aa64_vfp_qreg(CPUARMState *env, unsigned regno)
{
    return &env->vfp.zregs[regno].d[0];
}

extern const uint64_t pred_esz_masks[5];

#define PAGE_BTI            PAGE_TARGET_1
#define PAGE_MTE            PAGE_TARGET_2
#define PAGE_TARGET_STICKY  PAGE_MTE

#define LOG2_TAG_GRANULE 4
#define TAG_GRANULE      (1 << LOG2_TAG_GRANULE)

#ifdef CONFIG_USER_ONLY

#define TARGET_PAGE_DATA_SIZE (TARGET_PAGE_SIZE >> (LOG2_TAG_GRANULE + 1))

#ifdef TARGET_TAGGED_ADDRESSES
static inline target_ulong cpu_untagged_addr(CPUState *cs, target_ulong x)
{
    CPUARMState *env = cpu_env(cs);
    if (env->tagged_addr_enable) {
        x &= sextract64(x, 0, 56);
    }
    return x;
}
#endif /* TARGET_TAGGED_ADDRESSES */
#endif /* CONFIG_USER_ONLY */

#endif
