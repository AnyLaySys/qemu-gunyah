
#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/range.h"
#include "qemu/main-loop.h"
#include "exec/exec-all.h"
#include "exec/page-protection.h"
#include "cpu.h"
#include "internals.h"
#include "cpu-features.h"
#include "idau.h"

typedef struct S1Translate {
    ARMMMUIdx in_mmu_idx;
    ARMMMUIdx in_ptw_idx;
    ARMSecuritySpace in_space;
    bool in_debug;
    bool in_s1_is_el0;
    bool out_rw;
    bool out_be;
    ARMSecuritySpace out_space;
    hwaddr out_virt;
    hwaddr out_phys;
    void *out_host;
} S1Translate;

static bool get_phys_addr_nogpc(CPUARMState *env, S1Translate *ptw,
                                vaddr address,
                                MMUAccessType access_type, MemOp memop,
                                GetPhysAddrResult *result,
                                ARMMMUFaultInfo *fi);

static bool get_phys_addr_gpc(CPUARMState *env, S1Translate *ptw,
                              vaddr address,
                              MMUAccessType access_type, MemOp memop,
                              GetPhysAddrResult *result,
                              ARMMMUFaultInfo *fi);

static int get_S1prot(CPUARMState *env, ARMMMUIdx mmu_idx, bool is_aa64,
                      int user_rw, int prot_rw, int xn, int pxn,
                      ARMSecuritySpace in_pa, ARMSecuritySpace out_pa);

static const uint8_t pamax_map[] = {
    [0] = 32,
    [1] = 36,
    [2] = 40,
    [3] = 42,
    [4] = 44,
    [5] = 48,
    [6] = 52,
};

uint8_t round_down_to_parange_index(uint8_t bit_size)
{
    for (int i = ARRAY_SIZE(pamax_map) - 1; i >= 0; i--) {
        if (pamax_map[i] <= bit_size) {
            return i;
        }
    }
    g_assert_not_reached();
}

uint8_t round_down_to_parange_bit_size(uint8_t bit_size)
{
    return pamax_map[round_down_to_parange_index(bit_size)];
}

unsigned int arm_pamax(ARMCPU *cpu)
{
    if (arm_feature(&cpu->env, ARM_FEATURE_AARCH64)) {
        unsigned int parange =
            FIELD_EX64(cpu->isar.id_aa64mmfr0, ID_AA64MMFR0, PARANGE);

        assert(parange < ARRAY_SIZE(pamax_map));
        return pamax_map[parange];
    }

    if (arm_feature(&cpu->env, ARM_FEATURE_LPAE)) {
        return 40;
    }
    return 32;
}

ARMMMUIdx stage_1_mmu_idx(ARMMMUIdx mmu_idx)
{
    switch (mmu_idx) {
    case ARMMMUIdx_E10_0:
        return ARMMMUIdx_Stage1_E0;
    case ARMMMUIdx_E10_1:
        return ARMMMUIdx_Stage1_E1;
    case ARMMMUIdx_E10_1_PAN:
        return ARMMMUIdx_Stage1_E1_PAN;
    default:
        return mmu_idx;
    }
}

ARMMMUIdx arm_stage1_mmu_idx(CPUARMState *env)
{
    return stage_1_mmu_idx(arm_mmu_idx(env));
}

static ARMMMUIdx ptw_idx_for_stage_2(CPUARMState *env, ARMMMUIdx stage2idx)
{
    bool s2walk_secure;

    if (!arm_el_is_aa64(env, 3)) {
        return ARMMMUIdx_Phys_NS;
    }

    switch (arm_security_space_below_el3(env)) {
    case ARMSS_NonSecure:
        return ARMMMUIdx_Phys_NS;
    case ARMSS_Realm:
        return ARMMMUIdx_Phys_Realm;
    case ARMSS_Secure:
        if (stage2idx == ARMMMUIdx_Stage2_S) {
            s2walk_secure = !(env->cp15.vstcr_el2 & VSTCR_SW);
        } else {
            s2walk_secure = !(env->cp15.vtcr_el2 & VTCR_NSW);
        }
        return s2walk_secure ? ARMMMUIdx_Phys_S : ARMMMUIdx_Phys_NS;
    default:
        g_assert_not_reached();
    }
}

static bool regime_translation_big_endian(CPUARMState *env, ARMMMUIdx mmu_idx)
{
    return (regime_sctlr(env, mmu_idx) & SCTLR_EE) != 0;
}

static uint64_t regime_ttbr(CPUARMState *env, ARMMMUIdx mmu_idx, int ttbrn)
{
    if (mmu_idx == ARMMMUIdx_Stage2) {
        return env->cp15.vttbr_el2;
    }
    if (mmu_idx == ARMMMUIdx_Stage2_S) {
        return env->cp15.vsttbr_el2;
    }
    if (ttbrn == 0) {
        return env->cp15.ttbr0_el[regime_el(env, mmu_idx)];
    } else {
        return env->cp15.ttbr1_el[regime_el(env, mmu_idx)];
    }
}

static bool regime_translation_disabled(CPUARMState *env, ARMMMUIdx mmu_idx,
                                        ARMSecuritySpace space)
{
    uint64_t hcr_el2;

    if (arm_feature(env, ARM_FEATURE_M)) {
        bool is_secure = arm_space_is_secure(space);
        switch (env->v7m.mpu_ctrl[is_secure] &
                (R_V7M_MPU_CTRL_ENABLE_MASK | R_V7M_MPU_CTRL_HFNMIENA_MASK)) {
        case R_V7M_MPU_CTRL_ENABLE_MASK:
            return mmu_idx & ARM_MMU_IDX_M_NEGPRI;
        case R_V7M_MPU_CTRL_ENABLE_MASK | R_V7M_MPU_CTRL_HFNMIENA_MASK:
            return false;
        case 0:
        default:
            return true;
        }
    }


    switch (mmu_idx) {
    case ARMMMUIdx_Stage2:
    case ARMMMUIdx_Stage2_S:
        hcr_el2 = arm_hcr_el2_eff_secstate(env, space);
        return (hcr_el2 & (HCR_DC | HCR_VM)) == 0;

    case ARMMMUIdx_E10_0:
    case ARMMMUIdx_E10_1:
    case ARMMMUIdx_E10_1_PAN:
        hcr_el2 = arm_hcr_el2_eff_secstate(env, space);
        if (hcr_el2 & HCR_TGE) {
            return true;
        }
        break;

    case ARMMMUIdx_Stage1_E0:
    case ARMMMUIdx_Stage1_E1:
    case ARMMMUIdx_Stage1_E1_PAN:
        hcr_el2 = arm_hcr_el2_eff_secstate(env, space);
        if (hcr_el2 & HCR_DC) {
            return true;
        }
        break;

    case ARMMMUIdx_E20_0:
    case ARMMMUIdx_E20_2:
    case ARMMMUIdx_E20_2_PAN:
    case ARMMMUIdx_E2:
    case ARMMMUIdx_E3:
    case ARMMMUIdx_E30_0:
    case ARMMMUIdx_E30_3_PAN:
        break;

    case ARMMMUIdx_Phys_S:
    case ARMMMUIdx_Phys_NS:
    case ARMMMUIdx_Phys_Root:
    case ARMMMUIdx_Phys_Realm:
        return true;

    default:
        g_assert_not_reached();
    }

    return (regime_sctlr(env, mmu_idx) & SCTLR_M) == 0;
}

static bool granule_protection_check(CPUARMState *env, uint64_t paddress,
                                     ARMSecuritySpace pspace,
                                     ARMMMUFaultInfo *fi)
{
    MemTxAttrs attrs = {
        .secure = true,
        .space = ARMSS_Root,
    };
    ARMCPU *cpu = env_archcpu(env);
    uint64_t gpccr = env->cp15.gpccr_el3;
    unsigned pps, pgs, l0gptsz, level = 0;
    uint64_t tableaddr, pps_mask, align, entry, index;
    AddressSpace *as;
    MemTxResult result;
    int gpi;

    if (!FIELD_EX64(gpccr, GPCCR, GPC)) {
        return true;
    }


    pps = FIELD_EX64(gpccr, GPCCR, PPS);
    if (pps > FIELD_EX64(cpu->isar.id_aa64mmfr0, ID_AA64MMFR0, PARANGE)) {
        goto fault_walk;
    }
    pps = pamax_map[pps];
    pps_mask = MAKE_64BIT_MASK(0, pps);

    switch (FIELD_EX64(gpccr, GPCCR, SH)) {
    case 0b10: /* outer shareable */
        break;
    case 0b00: /* non-shareable */
    case 0b11: /* inner shareable */
        if (FIELD_EX64(gpccr, GPCCR, ORGN) == 0 &&
            FIELD_EX64(gpccr, GPCCR, IRGN) == 0) {
            goto fault_walk;
        }
        break;
    default:   /* reserved */
        goto fault_walk;
    }

    switch (FIELD_EX64(gpccr, GPCCR, PGS)) {
    case 0b00: /* 4KB */
        pgs = 12;
        break;
    case 0b01: /* 64KB */
        pgs = 16;
        break;
    case 0b10: /* 16KB */
        pgs = 14;
        break;
    default: /* reserved */
        goto fault_walk;
    }

    l0gptsz = 30 + FIELD_EX64(gpccr, GPCCR, L0GPTSZ);

    if (paddress & ~pps_mask) {
        if (pspace == ARMSS_NonSecure) {
            return true;
        }
        goto fault_size;
    }

    tableaddr = env->cp15.gptbr_el3 << 12;
    if (tableaddr & ~pps_mask) {
        goto fault_size;
    }

    align = MAX(pps - l0gptsz + 3, 12);
    align = MAKE_64BIT_MASK(0, align);
    tableaddr &= ~align;

    as = arm_addressspace(env_cpu(env), attrs);

    index = extract64(paddress, l0gptsz, pps - l0gptsz);
    tableaddr += index * 8;
    entry = address_space_ldq_le(as, tableaddr, attrs, &result);
    if (result != MEMTX_OK) {
        goto fault_eabt;
    }

    switch (extract32(entry, 0, 4)) {
    case 1: /* block descriptor */
        if (entry >> 8) {
            goto fault_walk; /* RES0 bits not 0 */
        }
        gpi = extract32(entry, 4, 4);
        goto found;
    case 3: /* table descriptor */
        tableaddr = entry & ~0xf;
        align = MAX(l0gptsz - pgs - 1, 12);
        align = MAKE_64BIT_MASK(0, align);
        if (tableaddr & (~pps_mask | align)) {
            goto fault_walk; /* RES0 bits not 0 */
        }
        break;
    default: /* invalid */
        goto fault_walk;
    }

    level = 1;
    index = extract64(paddress, pgs + 4, l0gptsz - pgs - 4);
    tableaddr += index * 8;
    entry = address_space_ldq_le(as, tableaddr, attrs, &result);
    if (result != MEMTX_OK) {
        goto fault_eabt;
    }

    switch (extract32(entry, 0, 4)) {
    case 1: /* contiguous descriptor */
        if (entry >> 10) {
            goto fault_walk; /* RES0 bits not 0 */
        }
        if (extract32(entry, 8, 2) == 0) {
            goto fault_walk; /* reserved contig */
        }
        gpi = extract32(entry, 4, 4);
        break;
    default:
        index = extract64(paddress, pgs, 4);
        gpi = extract64(entry, index * 4, 4);
        break;
    }

 found:
    switch (gpi) {
    case 0b0000: /* no access */
        break;
    case 0b1111: /* all access */
        return true;
    case 0b1000:
    case 0b1001:
    case 0b1010:
    case 0b1011:
        if (pspace == (gpi & 3)) {
            return true;
        }
        break;
    default:
        goto fault_walk; /* reserved */
    }

    fi->gpcf = GPCF_Fail;
    goto fault_common;
 fault_eabt:
    fi->gpcf = GPCF_EABT;
    goto fault_common;
 fault_size:
    fi->gpcf = GPCF_AddressSize;
    goto fault_common;
 fault_walk:
    fi->gpcf = GPCF_Walk;
 fault_common:
    fi->level = level;
    fi->paddr = paddress;
    fi->paddr_space = pspace;
    return false;
}

static bool S1_attrs_are_device(uint8_t attrs)
{
    return (attrs & 0xf0) == 0;
}

static bool S2_attrs_are_device(uint64_t hcr, uint8_t attrs)
{
    if (hcr & HCR_FWB) {
        return (attrs & 0x4) == 0;
    } else {
        return (attrs & 0xc) == 0;
    }
}

static ARMSecuritySpace S2_security_space(ARMSecuritySpace s1_space,
                                          ARMMMUIdx s2_mmu_idx)
{
    if (regime_is_stage2(s2_mmu_idx)) {
        if (s1_space == ARMSS_Secure) {
            return arm_secure_to_space(s2_mmu_idx == ARMMMUIdx_Stage2_S);
        } else {
            assert(s2_mmu_idx != ARMMMUIdx_Stage2_S);
            assert(s1_space != ARMSS_Root);
            return s1_space;
        }
    } else {
        return arm_phys_to_space(s2_mmu_idx);
    }
}

static bool fault_s1ns(ARMSecuritySpace space, ARMMMUIdx s2_mmu_idx)
{
    return space == ARMSS_Secure && regime_is_stage2(s2_mmu_idx)
        && s2_mmu_idx == ARMMMUIdx_Stage2_S;
}

static bool S1_ptw_translate(CPUARMState *env, S1Translate *ptw,
                             hwaddr addr, ARMMMUFaultInfo *fi)
{
    ARMMMUIdx mmu_idx = ptw->in_mmu_idx;
    ARMMMUIdx s2_mmu_idx = ptw->in_ptw_idx;
    uint8_t pte_attrs;

    ptw->out_virt = addr;

    if (unlikely(ptw->in_debug)) {
        ARMSecuritySpace s2_space = S2_security_space(ptw->in_space, s2_mmu_idx);
        S1Translate s2ptw = {
            .in_mmu_idx = s2_mmu_idx,
            .in_ptw_idx = ptw_idx_for_stage_2(env, s2_mmu_idx),
            .in_space = s2_space,
            .in_debug = true,
        };
        GetPhysAddrResult s2 = { };

        if (get_phys_addr_gpc(env, &s2ptw, addr, MMU_DATA_LOAD, 0, &s2, fi)) {
            goto fail;
        }

        ptw->out_phys = s2.f.phys_addr;
        pte_attrs = s2.cacheattrs.attrs;
        ptw->out_host = NULL;
        ptw->out_rw = false;
        ptw->out_space = s2.f.attrs.space;
    } else {
        g_assert_not_reached();
    }

    if (regime_is_stage2(s2_mmu_idx)) {
        uint64_t hcr = arm_hcr_el2_eff_secstate(env, ptw->in_space);

        if ((hcr & HCR_PTW) && S2_attrs_are_device(hcr, pte_attrs)) {
            fi->type = ARMFault_Permission;
            fi->s2addr = addr;
            fi->stage2 = true;
            fi->s1ptw = true;
            fi->s1ns = fault_s1ns(ptw->in_space, s2_mmu_idx);
            return false;
        }
    }

    ptw->out_be = regime_translation_big_endian(env, mmu_idx);
    return true;

 fail:
    assert(fi->type != ARMFault_None);
    if (fi->type == ARMFault_GPCFOnOutput) {
        fi->type = ARMFault_GPCFOnWalk;
    }
    fi->s2addr = addr;
    fi->stage2 = regime_is_stage2(s2_mmu_idx);
    fi->s1ptw = fi->stage2;
    fi->s1ns = fault_s1ns(ptw->in_space, s2_mmu_idx);
    return false;
}

static uint32_t arm_ldl_ptw(CPUARMState *env, S1Translate *ptw,
                            ARMMMUFaultInfo *fi)
{
    CPUState *cs = env_cpu(env);
    void *host = ptw->out_host;
    uint32_t data;

    if (likely(host)) {
        data = qatomic_read((uint32_t *)host);
        if (ptw->out_be) {
            data = be32_to_cpu(data);
        } else {
            data = le32_to_cpu(data);
        }
    } else {
        MemTxAttrs attrs = {
            .space = ptw->out_space,
            .secure = arm_space_is_secure(ptw->out_space),
        };
        AddressSpace *as = arm_addressspace(cs, attrs);
        MemTxResult result = MEMTX_OK;

        if (ptw->out_be) {
            data = address_space_ldl_be(as, ptw->out_phys, attrs, &result);
        } else {
            data = address_space_ldl_le(as, ptw->out_phys, attrs, &result);
        }
        if (unlikely(result != MEMTX_OK)) {
            fi->type = ARMFault_SyncExternalOnWalk;
            fi->ea = arm_extabort_type(result);
            return 0;
        }
    }
    return data;
}

static uint64_t arm_ldq_ptw(CPUARMState *env, S1Translate *ptw,
                            ARMMMUFaultInfo *fi)
{
    CPUState *cs = env_cpu(env);
    void *host = ptw->out_host;
    uint64_t data;

    if (likely(host)) {
#ifdef CONFIG_ATOMIC64
        data = qatomic_read__nocheck((uint64_t *)host);
        if (ptw->out_be) {
            data = be64_to_cpu(data);
        } else {
            data = le64_to_cpu(data);
        }
#else
        if (ptw->out_be) {
            data = ldq_be_p(host);
        } else {
            data = ldq_le_p(host);
        }
#endif
    } else {
        MemTxAttrs attrs = {
            .space = ptw->out_space,
            .secure = arm_space_is_secure(ptw->out_space),
        };
        AddressSpace *as = arm_addressspace(cs, attrs);
        MemTxResult result = MEMTX_OK;

        if (ptw->out_be) {
            data = address_space_ldq_be(as, ptw->out_phys, attrs, &result);
        } else {
            data = address_space_ldq_le(as, ptw->out_phys, attrs, &result);
        }
        if (unlikely(result != MEMTX_OK)) {
            fi->type = ARMFault_SyncExternalOnWalk;
            fi->ea = arm_extabort_type(result);
            return 0;
        }
    }
    return data;
}

static uint64_t arm_casq_ptw(CPUARMState *env, uint64_t old_val,
                             uint64_t new_val, S1Translate *ptw,
                             ARMMMUFaultInfo *fi)
{
    g_assert_not_reached();
}

static bool get_level1_table_address(CPUARMState *env, ARMMMUIdx mmu_idx,
                                     uint32_t *table, uint32_t address)
{
    uint64_t tcr = regime_tcr(env, mmu_idx);
    int maskshift = extract32(tcr, 0, 3);
    uint32_t mask = ~(((uint32_t)0xffffffffu) >> maskshift);
    uint32_t base_mask;

    if (address & mask) {
        if (tcr & TTBCR_PD1) {
            return false;
        }
        *table = regime_ttbr(env, mmu_idx, 1) & 0xffffc000;
    } else {
        if (tcr & TTBCR_PD0) {
            return false;
        }
        base_mask = ~((uint32_t)0x3fffu >> maskshift);
        *table = regime_ttbr(env, mmu_idx, 0) & base_mask;
    }
    *table |= (address >> 18) & 0x3ffc;
    return true;
}

static int ap_to_rw_prot_is_user(CPUARMState *env, ARMMMUIdx mmu_idx,
                         int ap, int domain_prot, bool is_user)
{
    if (domain_prot == 3) {
        return PAGE_READ | PAGE_WRITE;
    }

    switch (ap) {
    case 0:
        if (arm_feature(env, ARM_FEATURE_V7)) {
            return 0;
        }
        switch (regime_sctlr(env, mmu_idx) & (SCTLR_S | SCTLR_R)) {
        case SCTLR_S:
            return is_user ? 0 : PAGE_READ;
        case SCTLR_R:
            return PAGE_READ;
        default:
            return 0;
        }
    case 1:
        return is_user ? 0 : PAGE_READ | PAGE_WRITE;
    case 2:
        if (is_user) {
            return PAGE_READ;
        } else {
            return PAGE_READ | PAGE_WRITE;
        }
    case 3:
        return PAGE_READ | PAGE_WRITE;
    case 4: /* Reserved.  */
        return 0;
    case 5:
        return is_user ? 0 : PAGE_READ;
    case 6:
        return PAGE_READ;
    case 7:
        if (!arm_feature(env, ARM_FEATURE_V6K)) {
            return 0;
        }
        return PAGE_READ;
    default:
        g_assert_not_reached();
    }
}

static int ap_to_rw_prot(CPUARMState *env, ARMMMUIdx mmu_idx,
                         int ap, int domain_prot)
{
   return ap_to_rw_prot_is_user(env, mmu_idx, ap, domain_prot,
                                regime_is_user(env, mmu_idx));
}

static int simple_ap_to_rw_prot_is_user(int ap, bool is_user)
{
    switch (ap) {
    case 0:
        return is_user ? 0 : PAGE_READ | PAGE_WRITE;
    case 1:
        return PAGE_READ | PAGE_WRITE;
    case 2:
        return is_user ? 0 : PAGE_READ;
    case 3:
        return PAGE_READ;
    default:
        g_assert_not_reached();
    }
}

static int simple_ap_to_rw_prot(CPUARMState *env, ARMMMUIdx mmu_idx, int ap)
{
    return simple_ap_to_rw_prot_is_user(ap, regime_is_user(env, mmu_idx));
}

static bool get_phys_addr_v5(CPUARMState *env, S1Translate *ptw,
                             uint32_t address, MMUAccessType access_type,
                             GetPhysAddrResult *result, ARMMMUFaultInfo *fi)
{
    int level = 1;
    uint32_t table;
    uint32_t desc;
    int type;
    int ap;
    int domain = 0;
    int domain_prot;
    hwaddr phys_addr;
    uint32_t dacr;

    if (!get_level1_table_address(env, ptw->in_mmu_idx, &table, address)) {
        fi->type = ARMFault_Translation;
        goto do_fault;
    }
    if (!S1_ptw_translate(env, ptw, table, fi)) {
        goto do_fault;
    }
    desc = arm_ldl_ptw(env, ptw, fi);
    if (fi->type != ARMFault_None) {
        goto do_fault;
    }
    type = (desc & 3);
    domain = (desc >> 5) & 0x0f;
    if (regime_el(env, ptw->in_mmu_idx) == 1) {
        dacr = env->cp15.dacr_ns;
    } else {
        dacr = env->cp15.dacr_s;
    }
    domain_prot = (dacr >> (domain * 2)) & 3;
    if (type == 0) {
        fi->type = ARMFault_Translation;
        goto do_fault;
    }
    if (type != 2) {
        level = 2;
    }
    if (domain_prot == 0 || domain_prot == 2) {
        fi->type = ARMFault_Domain;
        goto do_fault;
    }
    if (type == 2) {
        phys_addr = (desc & 0xfff00000) | (address & 0x000fffff);
        ap = (desc >> 10) & 3;
        result->f.lg_page_size = 20; /* 1MB */
    } else {
        if (type == 1) {
            table = (desc & 0xfffffc00) | ((address >> 10) & 0x3fc);
        } else {
            table = (desc & 0xfffff000) | ((address >> 8) & 0xffc);
        }
        if (!S1_ptw_translate(env, ptw, table, fi)) {
            goto do_fault;
        }
        desc = arm_ldl_ptw(env, ptw, fi);
        if (fi->type != ARMFault_None) {
            goto do_fault;
        }
        switch (desc & 3) {
        case 0: /* Page translation fault.  */
            fi->type = ARMFault_Translation;
            goto do_fault;
        case 1: /* 64k page.  */
            phys_addr = (desc & 0xffff0000) | (address & 0xffff);
            ap = (desc >> (4 + ((address >> 13) & 6))) & 3;
            result->f.lg_page_size = 16;
            break;
        case 2: /* 4k page.  */
            phys_addr = (desc & 0xfffff000) | (address & 0xfff);
            ap = (desc >> (4 + ((address >> 9) & 6))) & 3;
            result->f.lg_page_size = 12;
            break;
        case 3: /* 1k page, or ARMv6/XScale "extended small (4k) page" */
            if (type == 1) {
                if (arm_feature(env, ARM_FEATURE_XSCALE)
                    || arm_feature(env, ARM_FEATURE_V6)) {
                    phys_addr = (desc & 0xfffff000) | (address & 0xfff);
                    result->f.lg_page_size = 12;
                } else {
                    fi->type = ARMFault_Translation;
                    goto do_fault;
                }
            } else {
                phys_addr = (desc & 0xfffffc00) | (address & 0x3ff);
                result->f.lg_page_size = 10;
            }
            ap = (desc >> 4) & 3;
            break;
        default:
            g_assert_not_reached();
        }
    }
    result->f.prot = ap_to_rw_prot(env, ptw->in_mmu_idx, ap, domain_prot);
    result->f.prot |= result->f.prot ? PAGE_EXEC : 0;
    if (!(result->f.prot & (1 << access_type))) {
        fi->type = ARMFault_Permission;
        goto do_fault;
    }
    result->f.phys_addr = phys_addr;
    return false;
do_fault:
    fi->domain = domain;
    fi->level = level;
    return true;
}

static bool get_phys_addr_v6(CPUARMState *env, S1Translate *ptw,
                             uint32_t address, MMUAccessType access_type,
                             GetPhysAddrResult *result, ARMMMUFaultInfo *fi)
{
    ARMCPU *cpu = env_archcpu(env);
    ARMMMUIdx mmu_idx = ptw->in_mmu_idx;
    int level = 1;
    uint32_t table;
    uint32_t desc;
    uint32_t xn;
    uint32_t pxn = 0;
    int type;
    int ap;
    int domain = 0;
    int domain_prot;
    hwaddr phys_addr;
    uint32_t dacr;
    bool ns;
    ARMSecuritySpace out_space;

    if (!get_level1_table_address(env, mmu_idx, &table, address)) {
        fi->type = ARMFault_Translation;
        goto do_fault;
    }
    if (!S1_ptw_translate(env, ptw, table, fi)) {
        goto do_fault;
    }
    desc = arm_ldl_ptw(env, ptw, fi);
    if (fi->type != ARMFault_None) {
        goto do_fault;
    }
    type = (desc & 3);
    if (type == 0 || (type == 3 && !cpu_isar_feature(aa32_pxn, cpu))) {
        fi->type = ARMFault_Translation;
        goto do_fault;
    }
    if ((type == 1) || !(desc & (1 << 18))) {
        domain = (desc >> 5) & 0x0f;
    }
    if (regime_el(env, mmu_idx) == 1) {
        dacr = env->cp15.dacr_ns;
    } else {
        dacr = env->cp15.dacr_s;
    }
    if (type == 1) {
        level = 2;
    }
    domain_prot = (dacr >> (domain * 2)) & 3;
    if (domain_prot == 0 || domain_prot == 2) {
        fi->type = ARMFault_Domain;
        goto do_fault;
    }
    if (type != 1) {
        if (desc & (1 << 18)) {
            phys_addr = (desc & 0xff000000) | (address & 0x00ffffff);
            phys_addr |= (uint64_t)extract32(desc, 20, 4) << 32;
            phys_addr |= (uint64_t)extract32(desc, 5, 4) << 36;
            result->f.lg_page_size = 24;  /* 16MB */
        } else {
            phys_addr = (desc & 0xfff00000) | (address & 0x000fffff);
            result->f.lg_page_size = 20;  /* 1MB */
        }
        ap = ((desc >> 10) & 3) | ((desc >> 13) & 4);
        xn = desc & (1 << 4);
        pxn = desc & 1;
        ns = extract32(desc, 19, 1);
    } else {
        if (cpu_isar_feature(aa32_pxn, cpu)) {
            pxn = (desc >> 2) & 1;
        }
        ns = extract32(desc, 3, 1);
        table = (desc & 0xfffffc00) | ((address >> 10) & 0x3fc);
        if (!S1_ptw_translate(env, ptw, table, fi)) {
            goto do_fault;
        }
        desc = arm_ldl_ptw(env, ptw, fi);
        if (fi->type != ARMFault_None) {
            goto do_fault;
        }
        ap = ((desc >> 4) & 3) | ((desc >> 7) & 4);
        switch (desc & 3) {
        case 0: /* Page translation fault.  */
            fi->type = ARMFault_Translation;
            goto do_fault;
        case 1: /* 64k page.  */
            phys_addr = (desc & 0xffff0000) | (address & 0xffff);
            xn = desc & (1 << 15);
            result->f.lg_page_size = 16;
            break;
        case 2: case 3: /* 4k page.  */
            phys_addr = (desc & 0xfffff000) | (address & 0xfff);
            xn = desc & 1;
            result->f.lg_page_size = 12;
            break;
        default:
            g_assert_not_reached();
        }
    }
    out_space = ptw->in_space;
    if (ns) {
        out_space = ARMSS_NonSecure;
    }
    if (domain_prot == 3) {
        result->f.prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
    } else {
        int user_rw, prot_rw;

        if (arm_feature(env, ARM_FEATURE_V6K) &&
                (regime_sctlr(env, mmu_idx) & SCTLR_AFE)) {
            if ((ap & 1) == 0) {
                fi->type = ARMFault_AccessFlag;
                goto do_fault;
            }
            prot_rw = simple_ap_to_rw_prot(env, mmu_idx, ap >> 1);
            user_rw = simple_ap_to_rw_prot_is_user(ap >> 1, 1);
        } else {
            prot_rw = ap_to_rw_prot(env, mmu_idx, ap, domain_prot);
            user_rw = ap_to_rw_prot_is_user(env, mmu_idx, ap, domain_prot, 1);
        }

        result->f.prot = get_S1prot(env, mmu_idx, false, user_rw, prot_rw,
                                    xn, pxn, result->f.attrs.space, out_space);
        if (!(result->f.prot & (1 << access_type))) {
            fi->type = ARMFault_Permission;
            goto do_fault;
        }
    }
    result->f.attrs.space = out_space;
    result->f.attrs.secure = arm_space_is_secure(out_space);
    result->f.phys_addr = phys_addr;
    return false;
do_fault:
    fi->domain = domain;
    fi->level = level;
    return true;
}

static int get_S2prot_noexecute(int s2ap)
{
    int prot = 0;

    if (s2ap & 1) {
        prot |= PAGE_READ;
    }
    if (s2ap & 2) {
        prot |= PAGE_WRITE;
    }
    return prot;
}

static int get_S2prot(CPUARMState *env, int s2ap, int xn, bool s1_is_el0)
{
    int prot = get_S2prot_noexecute(s2ap);

    if (cpu_isar_feature(any_tts2uxn, env_archcpu(env))) {
        switch (xn) {
        case 0:
            prot |= PAGE_EXEC;
            break;
        case 1:
            if (s1_is_el0) {
                prot |= PAGE_EXEC;
            }
            break;
        case 2:
            break;
        case 3:
            if (!s1_is_el0) {
                prot |= PAGE_EXEC;
            }
            break;
        default:
            g_assert_not_reached();
        }
    } else {
        if (!extract32(xn, 1, 1)) {
            if (arm_el_is_aa64(env, 2) || prot & PAGE_READ) {
                prot |= PAGE_EXEC;
            }
        }
    }
    return prot;
}

static int get_S1prot(CPUARMState *env, ARMMMUIdx mmu_idx, bool is_aa64,
                      int user_rw, int prot_rw, int xn, int pxn,
                      ARMSecuritySpace in_pa, ARMSecuritySpace out_pa)
{
    ARMCPU *cpu = env_archcpu(env);
    bool is_user = regime_is_user(env, mmu_idx);
    bool have_wxn;
    int wxn = 0;

    assert(!regime_is_stage2(mmu_idx));

    if (is_user) {
        prot_rw = user_rw;
    } else {
        if (user_rw && regime_is_pan(env, mmu_idx)) {
            prot_rw = 0;
        } else if (cpu_isar_feature(aa64_pan3, cpu) && is_aa64 &&
                   regime_is_pan(env, mmu_idx) &&
                   (regime_sctlr(env, mmu_idx) & SCTLR_EPAN) && !xn) {
            prot_rw = 0;
        }
    }

    if (in_pa != out_pa) {
        switch (in_pa) {
        case ARMSS_Root:
            return prot_rw;
        case ARMSS_Realm:
            switch (mmu_idx) {
            case ARMMMUIdx_E2:
            case ARMMMUIdx_E20_0:
            case ARMMMUIdx_E20_2:
            case ARMMMUIdx_E20_2_PAN:
                return prot_rw;
            default:
                break;
            }
            break;
        case ARMSS_Secure:
            if (env->cp15.scr_el3 & SCR_SIF) {
                return prot_rw;
            }
            break;
        default:
            g_assert_not_reached();
        }
    }

    have_wxn = arm_feature(env, ARM_FEATURE_LPAE);

    if (have_wxn) {
        wxn = regime_sctlr(env, mmu_idx) & SCTLR_WXN;
    }

    if (is_aa64) {
        if (regime_has_2_ranges(mmu_idx) && !is_user) {
            xn = pxn || (user_rw & PAGE_WRITE);
        }
    } else if (arm_feature(env, ARM_FEATURE_V7)) {
        switch (regime_el(env, mmu_idx)) {
        case 1:
        case 3:
            if (is_user) {
                xn = xn || !(user_rw & PAGE_READ);
            } else {
                int uwxn = 0;
                if (have_wxn) {
                    uwxn = regime_sctlr(env, mmu_idx) & SCTLR_UWXN;
                }
                xn = xn || !(prot_rw & PAGE_READ) || pxn ||
                     (uwxn && (user_rw & PAGE_WRITE));
            }
            break;
        case 2:
            break;
        }
    } else {
        xn = wxn = 0;
    }

    if (xn || (wxn && (prot_rw & PAGE_WRITE))) {
        return prot_rw;
    }
    return prot_rw | PAGE_EXEC;
}

static ARMVAParameters aa32_va_parameters(CPUARMState *env, uint32_t va,
                                          ARMMMUIdx mmu_idx)
{
    uint64_t tcr = regime_tcr(env, mmu_idx);
    uint32_t el = regime_el(env, mmu_idx);
    int select, tsz;
    bool epd, hpd;

    assert(mmu_idx != ARMMMUIdx_Stage2_S);

    if (mmu_idx == ARMMMUIdx_Stage2) {
        bool sext = extract32(tcr, 4, 1);
        bool sign = extract32(tcr, 3, 1);

        if (sign != sext) {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "AArch32: VTCR.S / VTCR.T0SZ[3] mismatch\n");
        }
        tsz = sextract32(tcr, 0, 4) + 8;
        select = 0;
        hpd = false;
        epd = false;
    } else if (el == 2) {
        tsz = extract32(tcr, 0, 3);
        select = 0;
        hpd = extract64(tcr, 24, 1);
        epd = false;
    } else {
        int t0sz = extract32(tcr, 0, 3);
        int t1sz = extract32(tcr, 16, 3);

        if (t1sz == 0) {
            select = va > (0xffffffffu >> t0sz);
        } else {
            select = va >= ~(0xffffffffu >> t1sz);
        }
        if (!select) {
            tsz = t0sz;
            epd = extract32(tcr, 7, 1);
            hpd = extract64(tcr, 41, 1);
        } else {
            tsz = t1sz;
            epd = extract32(tcr, 23, 1);
            hpd = extract64(tcr, 42, 1);
        }
        hpd &= extract32(tcr, 6, 1);
    }

    return (ARMVAParameters) {
        .tsz = tsz,
        .select = select,
        .epd = epd,
        .hpd = hpd,
    };
}

static int check_s2_mmu_setup(ARMCPU *cpu, bool is_aa64, uint64_t tcr,
                              bool ds, int iasize, int stride)
{
    int sl0, sl2, startlevel, granulebits, levels;
    int s1_min_iasize, s1_max_iasize;

    sl0 = extract32(tcr, 6, 2);
    if (is_aa64) {
        switch (stride) {
        case 9: /* 4KB */
            sl2 = extract64(tcr, 33, 1);
            if (ds && sl2) {
                if (sl0 != 0) {
                    goto fail;
                }
                startlevel = -1;
            } else {
                startlevel = 2 - sl0;
                switch (sl0) {
                case 2:
                    if (arm_pamax(cpu) < 44) {
                        goto fail;
                    }
                    break;
                case 3:
                    if (!cpu_isar_feature(aa64_st, cpu)) {
                        goto fail;
                    }
                    startlevel = 3;
                    break;
                }
            }
            break;
        case 11: /* 16KB */
            switch (sl0) {
            case 2:
                if (arm_pamax(cpu) < 42) {
                    goto fail;
                }
                break;
            case 3:
                if (!ds) {
                    goto fail;
                }
                break;
            }
            startlevel = 3 - sl0;
            break;
        case 13: /* 64KB */
            switch (sl0) {
            case 2:
                if (arm_pamax(cpu) < 44) {
                    goto fail;
                }
                break;
            case 3:
                goto fail;
            }
            startlevel = 3 - sl0;
            break;
        default:
            g_assert_not_reached();
        }
    } else {
        assert(stride == 9);
        if (sl0 >= 2) {
            goto fail;
        }
        startlevel = 2 - sl0;
    }

    levels = 3 - startlevel;
    granulebits = stride + 3;

    s1_min_iasize = levels * stride + granulebits + 1;
    s1_max_iasize = s1_min_iasize + (stride - 1) + 4;

    if (iasize >= s1_min_iasize && iasize <= s1_max_iasize) {
        return startlevel;
    }

 fail:
    return INT_MIN;
}

static bool lpae_block_desc_valid(ARMCPU *cpu, bool ds,
                                  ARMGranuleSize gran, int level)
{
    switch (gran) {
    case Gran4K:
        return (level == 0 && ds) || level == 1 || level == 2;
    case Gran16K:
        return (level == 1 && ds) || level == 2;
    case Gran64K:
        return (level == 1 && arm_pamax(cpu) == 52) || level == 2;
    default:
        g_assert_not_reached();
    }
}

static bool nv_nv1_enabled(CPUARMState *env, S1Translate *ptw)
{
    uint64_t hcr = arm_hcr_el2_eff_secstate(env, ptw->in_space);
    return (hcr & (HCR_NV | HCR_NV1)) == (HCR_NV | HCR_NV1);
}

static bool get_phys_addr_lpae(CPUARMState *env, S1Translate *ptw,
                               uint64_t address,
                               MMUAccessType access_type, MemOp memop,
                               GetPhysAddrResult *result, ARMMMUFaultInfo *fi)
{
    ARMCPU *cpu = env_archcpu(env);
    ARMMMUIdx mmu_idx = ptw->in_mmu_idx;
    int32_t level;
    ARMVAParameters param;
    uint64_t ttbr;
    hwaddr descaddr, indexmask, indexmask_grainsize;
    uint32_t tableattrs;
    target_ulong page_size;
    uint64_t attrs;
    int32_t stride;
    int addrsize, inputsize, outputsize;
    uint64_t tcr = regime_tcr(env, mmu_idx);
    int ap, xn, pxn;
    uint32_t el = regime_el(env, mmu_idx);
    uint64_t descaddrmask;
    bool aarch64 = arm_el_is_aa64(env, el);
    uint64_t descriptor, new_descriptor;
    ARMSecuritySpace out_space;
    bool device;

    if (aarch64) {
        int ps;

        param = aa64_va_parameters(env, address, mmu_idx,
                                   access_type != MMU_INST_FETCH,
                                   !arm_el_is_aa64(env, 1));
        level = 0;

        if (param.tsz_oob) {
            goto do_translation_fault;
        }

        addrsize = 64 - 8 * param.tbi;
        inputsize = 64 - param.tsz;

        ps = FIELD_EX64(cpu->isar.id_aa64mmfr0, ID_AA64MMFR0, PARANGE);
        ps = MIN(ps, param.ps);
        assert(ps < ARRAY_SIZE(pamax_map));
        outputsize = pamax_map[ps];

        if (!param.ds && param.gran != Gran64K) {
            outputsize = MIN(outputsize, 48);
        }
    } else {
        param = aa32_va_parameters(env, address, mmu_idx);
        level = 1;
        addrsize = (mmu_idx == ARMMMUIdx_Stage2 ? 40 : 32);
        inputsize = addrsize - param.tsz;
        outputsize = 40;
    }

    if (inputsize < addrsize) {
        target_ulong top_bits = sextract64(address, inputsize,
                                           addrsize - inputsize);
        if (-top_bits != param.select) {
            goto do_translation_fault;
        }
    }

    stride = arm_granule_bits(param.gran) - 3;

    ttbr = regime_ttbr(env, mmu_idx, param.select);


    if (param.epd) {
        goto do_translation_fault;
    }

    if (!regime_is_stage2(mmu_idx)) {
        level = 4 - (inputsize - 4) / stride;
    } else {
        int startlevel = check_s2_mmu_setup(cpu, aarch64, tcr, param.ds,
                                            inputsize, stride);
        if (startlevel == INT_MIN) {
            level = 0;
            goto do_translation_fault;
        }
        level = startlevel;
    }

    indexmask_grainsize = MAKE_64BIT_MASK(0, stride + 3);
    indexmask = MAKE_64BIT_MASK(0, inputsize - (stride * (4 - level)));

    descaddr = extract64(ttbr, 0, 48);

    if (outputsize > 48) {
        descaddr |= extract64(ttbr, 2, 4) << 48;
    } else if (descaddr >> outputsize) {
        level = 0;
        fi->type = ARMFault_AddressSize;
        goto do_fault;
    }

    descaddr &= ~indexmask;

    if (param.ds) {
        descaddrmask = MAKE_64BIT_MASK(0, 50);
    } else if (arm_feature(env, ARM_FEATURE_V8)) {
        descaddrmask = MAKE_64BIT_MASK(0, 48);
    } else {
        descaddrmask = MAKE_64BIT_MASK(0, 40);
    }
    descaddrmask &= ~indexmask_grainsize;
    tableattrs = 0;

 next_level:
    descaddr |= (address >> (stride * (4 - level))) & indexmask;
    descaddr &= ~7ULL;

    if (ptw->in_space == ARMSS_Secure
        && !regime_is_stage2(mmu_idx)
        && extract32(tableattrs, 4, 1)) {
        QEMU_BUILD_BUG_ON(ARMMMUIdx_Phys_S + 1 != ARMMMUIdx_Phys_NS);
        QEMU_BUILD_BUG_ON(ARMMMUIdx_Stage2_S + 1 != ARMMMUIdx_Stage2);
        ptw->in_ptw_idx += 1;
        ptw->in_space = ARMSS_NonSecure;
    }

    if (!S1_ptw_translate(env, ptw, descaddr, fi)) {
        goto do_fault;
    }
    descriptor = arm_ldq_ptw(env, ptw, fi);
    if (fi->type != ARMFault_None) {
        goto do_fault;
    }
    new_descriptor = descriptor;

 restart_atomic_update:
    if (!(descriptor & 1) ||
        (!(descriptor & 2) &&
         !lpae_block_desc_valid(cpu, param.ds, param.gran, level))) {
        goto do_translation_fault;
    }

    descaddr = descriptor & descaddrmask;

    if (outputsize > 48) {
        if (param.ds) {
            descaddr |= extract64(descriptor, 8, 2) << 50;
        } else {
            descaddr |= extract64(descriptor, 12, 4) << 48;
        }
    } else if (descaddr >> outputsize) {
        fi->type = ARMFault_AddressSize;
        goto do_fault;
    }

    if ((descriptor & 2) && (level < 3)) {
        tableattrs |= extract64(descriptor, 59, 5);
        level++;
        indexmask = indexmask_grainsize;
        goto next_level;
    }

    page_size = (1ULL << ((stride * (4 - level)) + 3));
    descaddr &= ~(hwaddr)(page_size - 1);
    descaddr |= (address & (page_size - 1));

    if (likely(!ptw->in_debug)) {
        if (!(descriptor & (1 << 10))) {
            if (param.ha) {
                new_descriptor |= 1 << 10; /* AF */
            } else {
                fi->type = ARMFault_AccessFlag;
                goto do_fault;
            }
        }

        if (param.hd
            && extract64(descriptor, 51, 1)  /* DBM */
            && access_type == MMU_DATA_STORE) {
            if (regime_is_stage2(mmu_idx)) {
                new_descriptor |= 1ull << 7;    /* set S2AP[1] */
            } else {
                new_descriptor &= ~(1ull << 7); /* clear AP[2] */
            }
        }
    }

    attrs = new_descriptor & (MAKE_64BIT_MASK(2, 10) | MAKE_64BIT_MASK(50, 14));
    if (!regime_is_stage2(mmu_idx)) {
        if (!param.hpd) {
            attrs |= extract64(tableattrs, 0, 2) << 53;     /* XN, PXN */
            attrs &= ~(extract64(tableattrs, 2, 1) << 6); /* !APT[0] => AP[1] */
            attrs |= extract32(tableattrs, 3, 1) << 7;    /* APT[1] => AP[2] */
        }
    }

    ap = extract32(attrs, 6, 2);
    out_space = ptw->in_space;
    if (regime_is_stage2(mmu_idx)) {
        if (out_space == ARMSS_Realm && extract64(attrs, 55, 1)) {
            out_space = ARMSS_NonSecure;
            result->f.prot = get_S2prot_noexecute(ap);
        } else {
            xn = extract64(attrs, 53, 2);
            result->f.prot = get_S2prot(env, ap, xn, ptw->in_s1_is_el0);
        }

        result->cacheattrs.is_s2_format = true;
        result->cacheattrs.attrs = extract32(attrs, 2, 4);
        device = S2_attrs_are_device(arm_hcr_el2_eff(env),
                                     result->cacheattrs.attrs);
    } else {
        int nse, ns = extract32(attrs, 5, 1);
        uint8_t attrindx;
        uint64_t mair;
        int user_rw, prot_rw;

        switch (out_space) {
        case ARMSS_Root:
            nse = extract32(attrs, 11, 1);
            out_space = (nse << 1) | ns;
            if (out_space == ARMSS_Secure &&
                !cpu_isar_feature(aa64_sel2, cpu)) {
                out_space = ARMSS_NonSecure;
            }
            break;
        case ARMSS_Secure:
            if (ns) {
                out_space = ARMSS_NonSecure;
            }
            break;
        case ARMSS_Realm:
            switch (mmu_idx) {
            case ARMMMUIdx_Stage1_E0:
            case ARMMMUIdx_Stage1_E1:
            case ARMMMUIdx_Stage1_E1_PAN:
                break;
            case ARMMMUIdx_E2:
            case ARMMMUIdx_E20_0:
            case ARMMMUIdx_E20_2:
            case ARMMMUIdx_E20_2_PAN:
                if (ns) {
                    out_space = ARMSS_NonSecure;
                }
                break;
            default:
                g_assert_not_reached();
            }
            break;
        case ARMSS_NonSecure:
            break;
        default:
            g_assert_not_reached();
        }
        xn = extract64(attrs, 54, 1);
        pxn = extract64(attrs, 53, 1);

        if (el == 1 && nv_nv1_enabled(env, ptw)) {
            pxn = xn;
            xn = 0;
            ap &= ~1;
        }

        user_rw = simple_ap_to_rw_prot_is_user(ap, true);
        prot_rw = simple_ap_to_rw_prot_is_user(ap, false);
        result->f.prot = get_S1prot(env, mmu_idx, aarch64, user_rw, prot_rw,
                                    xn, pxn, result->f.attrs.space, out_space);

        attrindx = extract32(attrs, 2, 3);
        mair = env->cp15.mair_el[regime_el(env, mmu_idx)];
        assert(attrindx <= 7);
        result->cacheattrs.is_s2_format = false;
        result->cacheattrs.attrs = extract64(mair, attrindx * 8, 8);

        if (aarch64 && cpu_isar_feature(aa64_bti, cpu)) {
            result->f.extra.arm.guarded = extract64(attrs, 50, 1); /* GP */
        }
        device = S1_attrs_are_device(result->cacheattrs.attrs);
    }

    if (device) {
        unsigned a_bits = memop_atomicity_bits(memop);
        if (address & ((1 << a_bits) - 1)) {
            fi->type = ARMFault_Alignment;
            goto do_fault;
        }
        result->f.tlb_fill_flags = TLB_CHECK_ALIGNED;
    } else {
        result->f.tlb_fill_flags = 0;
    }

    if (!(result->f.prot & (1 << access_type))) {
        fi->type = ARMFault_Permission;
        goto do_fault;
    }

    if (new_descriptor != descriptor) {
        new_descriptor = arm_casq_ptw(env, descriptor, new_descriptor, ptw, fi);
        if (fi->type != ARMFault_None) {
            goto do_fault;
        }
        if (new_descriptor != descriptor) {
            descriptor = new_descriptor;
            goto restart_atomic_update;
        }
    }

    result->f.attrs.space = out_space;
    result->f.attrs.secure = arm_space_is_secure(out_space);

    if (param.ds) {
        result->cacheattrs.shareability = param.sh;
    } else {
        result->cacheattrs.shareability = extract32(attrs, 8, 2);
    }

    result->f.phys_addr = descaddr;
    result->f.lg_page_size = ctz64(page_size);
    return false;

 do_translation_fault:
    fi->type = ARMFault_Translation;
 do_fault:
    if (fi->s1ptw) {
        assert(fi->stage2);
    } else {
        fi->level = level;
        fi->stage2 = regime_is_stage2(mmu_idx);
    }
    fi->s1ns = fault_s1ns(ptw->in_space, mmu_idx);
    return true;
}

static bool get_phys_addr_pmsav5(CPUARMState *env,
                                 S1Translate *ptw,
                                 uint32_t address,
                                 MMUAccessType access_type,
                                 GetPhysAddrResult *result,
                                 ARMMMUFaultInfo *fi)
{
    int n;
    uint32_t mask;
    uint32_t base;
    ARMMMUIdx mmu_idx = ptw->in_mmu_idx;
    bool is_user = regime_is_user(env, mmu_idx);

    if (regime_translation_disabled(env, mmu_idx, ptw->in_space)) {
        result->f.phys_addr = address;
        result->f.prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
        return false;
    }

    result->f.phys_addr = address;
    for (n = 7; n >= 0; n--) {
        base = env->cp15.c6_region[n];
        if ((base & 1) == 0) {
            continue;
        }
        mask = 1 << ((base >> 1) & 0x1f);
        mask = (mask << 1) - 1;
        if (((base ^ address) & ~mask) == 0) {
            break;
        }
    }
    if (n < 0) {
        fi->type = ARMFault_Background;
        return true;
    }

    if (access_type == MMU_INST_FETCH) {
        mask = env->cp15.pmsav5_insn_ap;
    } else {
        mask = env->cp15.pmsav5_data_ap;
    }
    mask = (mask >> (n * 4)) & 0xf;
    switch (mask) {
    case 0:
        fi->type = ARMFault_Permission;
        fi->level = 1;
        return true;
    case 1:
        if (is_user) {
            fi->type = ARMFault_Permission;
            fi->level = 1;
            return true;
        }
        result->f.prot = PAGE_READ | PAGE_WRITE;
        break;
    case 2:
        result->f.prot = PAGE_READ;
        if (!is_user) {
            result->f.prot |= PAGE_WRITE;
        }
        break;
    case 3:
        result->f.prot = PAGE_READ | PAGE_WRITE;
        break;
    case 5:
        if (is_user) {
            fi->type = ARMFault_Permission;
            fi->level = 1;
            return true;
        }
        result->f.prot = PAGE_READ;
        break;
    case 6:
        result->f.prot = PAGE_READ;
        break;
    default:
        fi->type = ARMFault_Permission;
        fi->level = 1;
        return true;
    }
    result->f.prot |= PAGE_EXEC;
    return false;
}

static void get_phys_addr_pmsav7_default(CPUARMState *env, ARMMMUIdx mmu_idx,
                                         int32_t address, uint8_t *prot)
{
    if (!arm_feature(env, ARM_FEATURE_M)) {
        *prot = PAGE_READ | PAGE_WRITE;
        switch (address) {
        case 0xF0000000 ... 0xFFFFFFFF:
            if (regime_sctlr(env, mmu_idx) & SCTLR_V) {
                *prot |= PAGE_EXEC;
            }
            break;
        case 0x00000000 ... 0x7FFFFFFF:
            *prot |= PAGE_EXEC;
            break;
        }
    } else {
        switch (address) {
        case 0x00000000 ... 0x1fffffff: /* ROM */
        case 0x20000000 ... 0x3fffffff: /* SRAM */
        case 0x60000000 ... 0x7fffffff: /* RAM */
        case 0x80000000 ... 0x9fffffff: /* RAM */
            *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
            break;
        case 0x40000000 ... 0x5fffffff: /* Peripheral */
        case 0xa0000000 ... 0xbfffffff: /* Device */
        case 0xc0000000 ... 0xdfffffff: /* Device */
        case 0xe0000000 ... 0xffffffff: /* System */
            *prot = PAGE_READ | PAGE_WRITE;
            break;
        default:
            g_assert_not_reached();
        }
    }
}

static bool m_is_ppb_region(CPUARMState *env, uint32_t address)
{
    return arm_feature(env, ARM_FEATURE_M) &&
        extract32(address, 20, 12) == 0xe00;
}

static bool m_is_system_region(CPUARMState *env, uint32_t address)
{
    return arm_feature(env, ARM_FEATURE_M) && extract32(address, 29, 3) == 0x7;
}

static bool pmsav7_use_background_region(ARMCPU *cpu, ARMMMUIdx mmu_idx,
                                         bool is_secure, bool is_user)
{
    CPUARMState *env = &cpu->env;

    if (is_user) {
        return false;
    }

    if (arm_feature(env, ARM_FEATURE_M)) {
        return env->v7m.mpu_ctrl[is_secure] & R_V7M_MPU_CTRL_PRIVDEFENA_MASK;
    }

    if (mmu_idx == ARMMMUIdx_Stage2) {
        return false;
    }

    return regime_sctlr(env, mmu_idx) & SCTLR_BR;
}

static bool get_phys_addr_pmsav7(CPUARMState *env,
                                 S1Translate *ptw,
                                 uint32_t address,
                                 MMUAccessType access_type,
                                 GetPhysAddrResult *result,
                                 ARMMMUFaultInfo *fi)
{
    ARMCPU *cpu = env_archcpu(env);
    int n;
    ARMMMUIdx mmu_idx = ptw->in_mmu_idx;
    bool is_user = regime_is_user(env, mmu_idx);
    bool secure = arm_space_is_secure(ptw->in_space);

    result->f.phys_addr = address;
    result->f.lg_page_size = TARGET_PAGE_BITS;
    result->f.prot = 0;

    if (regime_translation_disabled(env, mmu_idx, ptw->in_space) ||
        m_is_ppb_region(env, address)) {
        get_phys_addr_pmsav7_default(env, mmu_idx, address, &result->f.prot);
    } else { /* MPU enabled */
        for (n = (int)cpu->pmsav7_dregion - 1; n >= 0; n--) {
            uint32_t base = env->pmsav7.drbar[n];
            uint32_t rsize = extract32(env->pmsav7.drsr[n], 1, 5);
            uint32_t rmask;
            bool srdis = false;

            if (!(env->pmsav7.drsr[n] & 0x1)) {
                continue;
            }

            if (!rsize) {
                qemu_log_mask(LOG_GUEST_ERROR,
                              "DRSR[%d]: Rsize field cannot be 0\n", n);
                continue;
            }
            rsize++;
            rmask = (1ull << rsize) - 1;

            if (base & rmask) {
                qemu_log_mask(LOG_GUEST_ERROR,
                              "DRBAR[%d]: 0x%" PRIx32 " misaligned "
                              "to DRSR region size, mask = 0x%" PRIx32 "\n",
                              n, base, rmask);
                continue;
            }

            if (address < base || address > base + rmask) {
                if (ranges_overlap(base, rmask,
                                   address & TARGET_PAGE_MASK,
                                   TARGET_PAGE_SIZE)) {
                    result->f.lg_page_size = 0;
                }
                continue;
            }


            if (rsize >= 8) { /* no subregions for regions < 256 bytes */
                int i, snd;
                uint32_t srdis_mask;

                rsize -= 3; /* sub region size (power of 2) */
                snd = ((address - base) >> rsize) & 0x7;
                srdis = extract32(env->pmsav7.drsr[n], snd + 8, 1);

                srdis_mask = srdis ? 0x3 : 0x0;
                for (i = 2; i <= 8 && rsize < TARGET_PAGE_BITS; i *= 2) {
                    int snd_rounded = snd & ~(i - 1);
                    uint32_t srdis_multi = extract32(env->pmsav7.drsr[n],
                                                     snd_rounded + 8, i);
                    if (srdis_mask ^ srdis_multi) {
                        break;
                    }
                    srdis_mask = (srdis_mask << i) | srdis_mask;
                    rsize++;
                }
            }
            if (srdis) {
                continue;
            }
            if (rsize < TARGET_PAGE_BITS) {
                result->f.lg_page_size = rsize;
            }
            break;
        }

        if (n == -1) { /* no hits */
            if (!pmsav7_use_background_region(cpu, mmu_idx, secure, is_user)) {
                fi->type = ARMFault_Background;
                return true;
            }
            get_phys_addr_pmsav7_default(env, mmu_idx, address,
                                         &result->f.prot);
        } else { /* a MPU hit! */
            uint32_t ap = extract32(env->pmsav7.dracr[n], 8, 3);
            uint32_t xn = extract32(env->pmsav7.dracr[n], 12, 1);

            if (m_is_system_region(env, address)) {
                xn = 1;
            }

            if (is_user) { /* User mode AP bit decoding */
                switch (ap) {
                case 0:
                case 1:
                case 5:
                    break; /* no access */
                case 3:
                    result->f.prot |= PAGE_WRITE;
                case 2:
                case 6:
                    result->f.prot |= PAGE_READ | PAGE_EXEC;
                    break;
                case 7:
                    if (arm_feature(env, ARM_FEATURE_M)) {
                        result->f.prot |= PAGE_READ | PAGE_EXEC;
                        break;
                    }
                default:
                    qemu_log_mask(LOG_GUEST_ERROR,
                                  "DRACR[%d]: Bad value for AP bits: 0x%"
                                  PRIx32 "\n", n, ap);
                }
            } else { /* Priv. mode AP bits decoding */
                switch (ap) {
                case 0:
                    break; /* no access */
                case 1:
                case 2:
                case 3:
                    result->f.prot |= PAGE_WRITE;
                case 5:
                case 6:
                    result->f.prot |= PAGE_READ | PAGE_EXEC;
                    break;
                case 7:
                    if (arm_feature(env, ARM_FEATURE_M)) {
                        result->f.prot |= PAGE_READ | PAGE_EXEC;
                        break;
                    }
                default:
                    qemu_log_mask(LOG_GUEST_ERROR,
                                  "DRACR[%d]: Bad value for AP bits: 0x%"
                                  PRIx32 "\n", n, ap);
                }
            }

            if (xn) {
                result->f.prot &= ~PAGE_EXEC;
            }
        }
    }

    fi->type = ARMFault_Permission;
    fi->level = 1;
    return !(result->f.prot & (1 << access_type));
}

static uint32_t *regime_rbar(CPUARMState *env, ARMMMUIdx mmu_idx,
                             uint32_t secure)
{
    if (regime_el(env, mmu_idx) == 2) {
        return env->pmsav8.hprbar;
    } else {
        return env->pmsav8.rbar[secure];
    }
}

static uint32_t *regime_rlar(CPUARMState *env, ARMMMUIdx mmu_idx,
                             uint32_t secure)
{
    if (regime_el(env, mmu_idx) == 2) {
        return env->pmsav8.hprlar;
    } else {
        return env->pmsav8.rlar[secure];
    }
}

bool pmsav8_mpu_lookup(CPUARMState *env, uint32_t address,
                       MMUAccessType access_type, ARMMMUIdx mmu_idx,
                       bool secure, GetPhysAddrResult *result,
                       ARMMMUFaultInfo *fi, uint32_t *mregion)
{
    ARMCPU *cpu = env_archcpu(env);
    bool is_user = regime_is_user(env, mmu_idx);
    int n;
    int matchregion = -1;
    bool hit = false;
    uint32_t addr_page_base = address & TARGET_PAGE_MASK;
    uint32_t addr_page_limit = addr_page_base + (TARGET_PAGE_SIZE - 1);
    int region_counter;

    if (regime_el(env, mmu_idx) == 2) {
        region_counter = cpu->pmsav8r_hdregion;
    } else {
        region_counter = cpu->pmsav7_dregion;
    }

    result->f.lg_page_size = TARGET_PAGE_BITS;
    result->f.phys_addr = address;
    result->f.prot = 0;
    if (mregion) {
        *mregion = -1;
    }

    if (mmu_idx == ARMMMUIdx_Stage2) {
        fi->stage2 = true;
    }

    if (regime_translation_disabled(env, mmu_idx, arm_secure_to_space(secure))) {
        hit = true;
    } else if (m_is_ppb_region(env, address)) {
        hit = true;
    } else {
        if (pmsav7_use_background_region(cpu, mmu_idx, secure, is_user)) {
            hit = true;
        }

        uint32_t bitmask;
        if (arm_feature(env, ARM_FEATURE_M)) {
            bitmask = 0x1f;
        } else {
            bitmask = 0x3f;
            fi->level = 0;
        }

        for (n = region_counter - 1; n >= 0; n--) {
            uint32_t base = regime_rbar(env, mmu_idx, secure)[n] & ~bitmask;
            uint32_t limit = regime_rlar(env, mmu_idx, secure)[n] | bitmask;

            if (!(regime_rlar(env, mmu_idx, secure)[n] & 0x1)) {
                continue;
            }

            if (address < base || address > limit) {
                if (limit >= base &&
                    ranges_overlap(base, limit - base + 1,
                                   addr_page_base,
                                   TARGET_PAGE_SIZE)) {
                    result->f.lg_page_size = 0;
                }
                continue;
            }

            if (base > addr_page_base || limit < addr_page_limit) {
                result->f.lg_page_size = 0;
            }

            if (matchregion != -1) {
                fi->type = ARMFault_Permission;
                if (arm_feature(env, ARM_FEATURE_M)) {
                    fi->level = 1;
                }
                return true;
            }

            matchregion = n;
            hit = true;
        }
    }

    if (!hit) {
        if (arm_feature(env, ARM_FEATURE_M)) {
            fi->type = ARMFault_Background;
        } else {
            fi->type = ARMFault_Permission;
        }
        return true;
    }

    if (matchregion == -1) {
        get_phys_addr_pmsav7_default(env, mmu_idx, address, &result->f.prot);
    } else {
        uint32_t matched_rbar = regime_rbar(env, mmu_idx, secure)[matchregion];
        uint32_t matched_rlar = regime_rlar(env, mmu_idx, secure)[matchregion];
        uint32_t ap = extract32(matched_rbar, 1, 2);
        uint32_t xn = extract32(matched_rbar, 0, 1);
        bool pxn = false;

        if (arm_feature(env, ARM_FEATURE_V8_1M)) {
            pxn = extract32(matched_rlar, 4, 1);
        }

        if (m_is_system_region(env, address)) {
            xn = 1;
        }

        if (regime_el(env, mmu_idx) == 2) {
            result->f.prot = simple_ap_to_rw_prot_is_user(ap,
                                            mmu_idx != ARMMMUIdx_E2);
        } else {
            result->f.prot = simple_ap_to_rw_prot(env, mmu_idx, ap);
        }

        if (!arm_feature(env, ARM_FEATURE_M)) {
            uint8_t attrindx = extract32(matched_rlar, 1, 3);
            uint64_t mair = env->cp15.mair_el[regime_el(env, mmu_idx)];
            uint8_t sh = extract32(matched_rlar, 3, 2);

            if (regime_sctlr(env, mmu_idx) & SCTLR_WXN &&
                result->f.prot & PAGE_WRITE && mmu_idx != ARMMMUIdx_Stage2) {
                xn = 0x1;
            }

            if ((regime_el(env, mmu_idx) == 1) &&
                regime_sctlr(env, mmu_idx) & SCTLR_UWXN && ap == 0x1) {
                pxn = 0x1;
            }

            result->cacheattrs.is_s2_format = false;
            result->cacheattrs.attrs = extract64(mair, attrindx * 8, 8);
            result->cacheattrs.shareability = sh;
        }

        if (result->f.prot && !xn && !(pxn && !is_user)) {
            result->f.prot |= PAGE_EXEC;
        }

        if (mregion) {
            *mregion = matchregion;
        }
    }

    fi->type = ARMFault_Permission;
    if (arm_feature(env, ARM_FEATURE_M)) {
        fi->level = 1;
    }
    return !(result->f.prot & (1 << access_type));
}

static bool v8m_is_sau_exempt(CPUARMState *env,
                              uint32_t address, MMUAccessType access_type)
{
    return
        (access_type == MMU_INST_FETCH && m_is_system_region(env, address)) ||
        (address >= 0xe0000000 && address <= 0xe0002fff) ||
        (address >= 0xe000e000 && address <= 0xe000efff) ||
        (address >= 0xe002e000 && address <= 0xe002efff) ||
        (address >= 0xe0040000 && address <= 0xe0041fff) ||
        (address >= 0xe00ff000 && address <= 0xe00fffff);
}

void v8m_security_lookup(CPUARMState *env, uint32_t address,
                         MMUAccessType access_type, ARMMMUIdx mmu_idx,
                         bool is_secure, V8M_SAttributes *sattrs)
{
    ARMCPU *cpu = env_archcpu(env);
    int r;
    bool idau_exempt = false, idau_ns = true, idau_nsc = true;
    int idau_region = IREGION_NOTVALID;
    uint32_t addr_page_base = address & TARGET_PAGE_MASK;
    uint32_t addr_page_limit = addr_page_base + (TARGET_PAGE_SIZE - 1);

    if (cpu->idau) {
        IDAUInterfaceClass *iic = IDAU_INTERFACE_GET_CLASS(cpu->idau);
        IDAUInterface *ii = IDAU_INTERFACE(cpu->idau);

        iic->check(ii, address, &idau_region, &idau_exempt, &idau_ns,
                   &idau_nsc);
    }

    if (access_type == MMU_INST_FETCH && extract32(address, 28, 4) == 0xf) {
        return;
    }

    if (idau_exempt || v8m_is_sau_exempt(env, address, access_type)) {
        sattrs->ns = !is_secure;
        return;
    }

    if (idau_region != IREGION_NOTVALID) {
        sattrs->irvalid = true;
        sattrs->iregion = idau_region;
    }

    switch (env->sau.ctrl & 3) {
    case 0: /* SAU.ENABLE == 0, SAU.ALLNS == 0 */
        break;
    case 2: /* SAU.ENABLE == 0, SAU.ALLNS == 1 */
        sattrs->ns = true;
        break;
    default: /* SAU.ENABLE == 1 */
        for (r = 0; r < cpu->sau_sregion; r++) {
            if (env->sau.rlar[r] & 1) {
                uint32_t base = env->sau.rbar[r] & ~0x1f;
                uint32_t limit = env->sau.rlar[r] | 0x1f;

                if (base <= address && limit >= address) {
                    if (base > addr_page_base || limit < addr_page_limit) {
                        sattrs->subpage = true;
                    }
                    if (sattrs->srvalid) {
                        sattrs->ns = false;
                        sattrs->nsc = false;
                        sattrs->sregion = 0;
                        sattrs->srvalid = false;
                        break;
                    } else {
                        if (env->sau.rlar[r] & 2) {
                            sattrs->nsc = true;
                        } else {
                            sattrs->ns = true;
                        }
                        sattrs->srvalid = true;
                        sattrs->sregion = r;
                    }
                } else {
                    if (limit >= base &&
                        ranges_overlap(base, limit - base + 1,
                                       addr_page_base,
                                       TARGET_PAGE_SIZE)) {
                        sattrs->subpage = true;
                    }
                }
            }
        }
        break;
    }

    if (!idau_ns) {
        if (sattrs->ns || (!idau_nsc && sattrs->nsc)) {
            sattrs->ns = false;
            sattrs->nsc = idau_nsc;
        }
    }
}

static bool get_phys_addr_pmsav8(CPUARMState *env,
                                 S1Translate *ptw,
                                 uint32_t address,
                                 MMUAccessType access_type,
                                 GetPhysAddrResult *result,
                                 ARMMMUFaultInfo *fi)
{
    V8M_SAttributes sattrs = {};
    ARMMMUIdx mmu_idx = ptw->in_mmu_idx;
    bool secure = arm_space_is_secure(ptw->in_space);
    bool ret;

    if (arm_feature(env, ARM_FEATURE_M_SECURITY)) {
        v8m_security_lookup(env, address, access_type, mmu_idx,
                            secure, &sattrs);
        if (access_type == MMU_INST_FETCH) {
            if (sattrs.ns != !secure) {
                if (sattrs.nsc) {
                    fi->type = ARMFault_QEMU_NSCExec;
                } else {
                    fi->type = ARMFault_QEMU_SFault;
                }
                result->f.lg_page_size = sattrs.subpage ? 0 : TARGET_PAGE_BITS;
                result->f.phys_addr = address;
                result->f.prot = 0;
                return true;
            }
        } else {
            if (sattrs.ns) {
                result->f.attrs.secure = false;
                result->f.attrs.space = ARMSS_NonSecure;
            } else if (!secure) {
                fi->type = ARMFault_QEMU_SFault;
                result->f.lg_page_size = sattrs.subpage ? 0 : TARGET_PAGE_BITS;
                result->f.phys_addr = address;
                result->f.prot = 0;
                return true;
            }
        }
    }

    ret = pmsav8_mpu_lookup(env, address, access_type, mmu_idx, secure,
                            result, fi, NULL);
    if (sattrs.subpage) {
        result->f.lg_page_size = 0;
    }
    return ret;
}

static uint8_t convert_stage2_attrs(uint64_t hcr, uint8_t s2attrs)
{
    uint8_t hiattr = extract32(s2attrs, 2, 2);
    uint8_t loattr = extract32(s2attrs, 0, 2);
    uint8_t hihint = 0, lohint = 0;

    if (hiattr != 0) { /* normal memory */
        if (hcr & HCR_CD) { /* cache disabled */
            hiattr = loattr = 1; /* non-cacheable */
        } else {
            if (hiattr != 1) { /* Write-through or write-back */
                hihint = 3; /* RW allocate */
            }
            if (loattr != 1) { /* Write-through or write-back */
                lohint = 3; /* RW allocate */
            }
        }
    }

    return (hiattr << 6) | (hihint << 4) | (loattr << 2) | lohint;
}

static uint8_t combine_cacheattr_nibble(uint8_t s1, uint8_t s2)
{
    if (s1 == 4 || s2 == 4) {
        return 4;
    } else if (extract32(s1, 2, 2) == 0 || extract32(s1, 2, 2) == 2) {
        return s1;
    } else if (extract32(s2, 2, 2) == 2) {
        return (2 << 2) | extract32(s1, 0, 2);
    } else { /* write-back */
        return s1;
    }
}

static uint8_t combined_attrs_nofwb(uint64_t hcr,
                                    ARMCacheAttrs s1, ARMCacheAttrs s2)
{
    uint8_t s1lo, s2lo, s1hi, s2hi, s2_mair_attrs, ret_attrs;

    if (s2.is_s2_format) {
        s2_mair_attrs = convert_stage2_attrs(hcr, s2.attrs);
    } else {
        s2_mair_attrs = s2.attrs;
    }

    s1lo = extract32(s1.attrs, 0, 4);
    s2lo = extract32(s2_mair_attrs, 0, 4);
    s1hi = extract32(s1.attrs, 4, 4);
    s2hi = extract32(s2_mair_attrs, 4, 4);

    if (s1hi == 0 || s2hi == 0) {
        if (s1lo == 0 || s2lo == 0) {
            ret_attrs = 0;
        } else if (s1lo == 4 || s2lo == 4) {
            ret_attrs = 4;  /* nGnRE */
        } else if (s1lo == 8 || s2lo == 8) {
            ret_attrs = 8;  /* nGRE */
        } else {
            ret_attrs = 0xc; /* GRE */
        }
    } else { /* Normal memory */
        ret_attrs = combine_cacheattr_nibble(s1hi, s2hi) << 4
                  | combine_cacheattr_nibble(s1lo, s2lo);
    }
    return ret_attrs;
}

static uint8_t force_cacheattr_nibble_wb(uint8_t attr)
{
    if (attr == 0 || attr == 4) {
        return 0xf;
    }
    return attr | 4;
}

static uint8_t combined_attrs_fwb(ARMCacheAttrs s1, ARMCacheAttrs s2)
{
    assert(s2.is_s2_format && !s1.is_s2_format);

    switch (s2.attrs) {
    case 7:
        return s1.attrs;
    case 6:
        if ((s1.attrs & 0xf0) == 0) {
            return 0xff;
        }
        return force_cacheattr_nibble_wb(s1.attrs & 0xf) |
            force_cacheattr_nibble_wb(s1.attrs >> 4) << 4;
    case 5:
        if ((s1.attrs & 0xf0) == 0) {
            return s1.attrs;
        }
        return 0x44;
    case 0 ... 3:
        return s2.attrs << 2;
    default:
        return 0;
    }
}

static ARMCacheAttrs combine_cacheattrs(uint64_t hcr,
                                        ARMCacheAttrs s1, ARMCacheAttrs s2)
{
    ARMCacheAttrs ret;
    bool tagged = false;

    assert(!s1.is_s2_format);
    ret.is_s2_format = false;

    if (s1.attrs == 0xf0) {
        tagged = true;
        s1.attrs = 0xff;
    }

    if (s1.shareability == 2 || s2.shareability == 2) {
        ret.shareability = 2;
    } else if (s1.shareability == 3 || s2.shareability == 3) {
        ret.shareability = 3;
    } else {
        ret.shareability = 0;
    }

    if (hcr & HCR_FWB) {
        ret.attrs = combined_attrs_fwb(s1, s2);
    } else {
        ret.attrs = combined_attrs_nofwb(hcr, s1, s2);
    }

    if ((ret.attrs & 0xf0) == 0 || ret.attrs == 0x44) {
        ret.shareability = 2;
    }

    if (tagged && ret.attrs == 0xff) {
        ret.attrs = 0xf0;
    }

    return ret;
}

static bool get_phys_addr_disabled(CPUARMState *env,
                                   S1Translate *ptw,
                                   vaddr address,
                                   MMUAccessType access_type,
                                   GetPhysAddrResult *result,
                                   ARMMMUFaultInfo *fi)
{
    ARMMMUIdx mmu_idx = ptw->in_mmu_idx;
    uint8_t memattr = 0x00;    /* Device nGnRnE */
    uint8_t shareability = 0;  /* non-shareable */
    int r_el;

    switch (mmu_idx) {
    case ARMMMUIdx_Stage2:
    case ARMMMUIdx_Stage2_S:
    case ARMMMUIdx_Phys_S:
    case ARMMMUIdx_Phys_NS:
    case ARMMMUIdx_Phys_Root:
    case ARMMMUIdx_Phys_Realm:
        break;

    default:
        r_el = regime_el(env, mmu_idx);
        if (arm_el_is_aa64(env, r_el)) {
            int pamax = arm_pamax(env_archcpu(env));
            uint64_t tcr = env->cp15.tcr_el[r_el];
            int addrtop, tbi;

            tbi = aa64_va_parameter_tbi(tcr, mmu_idx);
            if (access_type == MMU_INST_FETCH) {
                tbi &= ~aa64_va_parameter_tbid(tcr, mmu_idx);
            }
            tbi = (tbi >> extract64(address, 55, 1)) & 1;
            addrtop = (tbi ? 55 : 63);

            if (extract64(address, pamax, addrtop - pamax + 1) != 0) {
                fi->type = ARMFault_AddressSize;
                fi->level = 0;
                fi->stage2 = false;
                return 1;
            }

            address = extract64(address, 0, 52);
        }

        if (r_el == 1) {
            uint64_t hcr = arm_hcr_el2_eff_secstate(env, ptw->in_space);
            if (hcr & HCR_DC) {
                if (hcr & HCR_DCT) {
                    memattr = 0xf0;  /* Tagged, Normal, WB, RWA */
                } else {
                    memattr = 0xff;  /* Normal, WB, RWA */
                }
            }
        }
        if (memattr == 0) {
            if (access_type == MMU_INST_FETCH) {
                if (regime_sctlr(env, mmu_idx) & SCTLR_I) {
                    memattr = 0xee;  /* Normal, WT, RA, NT */
                } else {
                    memattr = 0x44;  /* Normal, NC, No */
                }
            }
            shareability = 2; /* outer shareable */
        }
        result->cacheattrs.is_s2_format = false;
        break;
    }

    result->f.phys_addr = address;
    result->f.prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
    result->f.lg_page_size = TARGET_PAGE_BITS;
    result->cacheattrs.shareability = shareability;
    result->cacheattrs.attrs = memattr;
    return false;
}

static bool get_phys_addr_twostage(CPUARMState *env, S1Translate *ptw,
                                   vaddr address,
                                   MMUAccessType access_type, MemOp memop,
                                   GetPhysAddrResult *result,
                                   ARMMMUFaultInfo *fi)
{
    hwaddr ipa;
    int s1_prot, s1_lgpgsz;
    ARMSecuritySpace in_space = ptw->in_space;
    bool ret, ipa_secure, s1_guarded;
    ARMCacheAttrs cacheattrs1;
    ARMSecuritySpace ipa_space;
    uint64_t hcr;

    ret = get_phys_addr_nogpc(env, ptw, address, access_type,
                              memop, result, fi);

    if (ret) {
        return ret;
    }

    ipa = result->f.phys_addr;
    ipa_secure = result->f.attrs.secure;
    ipa_space = result->f.attrs.space;

    ptw->in_s1_is_el0 = ptw->in_mmu_idx == ARMMMUIdx_Stage1_E0;
    ptw->in_mmu_idx = ipa_secure ? ARMMMUIdx_Stage2_S : ARMMMUIdx_Stage2;
    ptw->in_space = ipa_space;
    ptw->in_ptw_idx = ptw_idx_for_stage_2(env, ptw->in_mmu_idx);

    s1_prot = result->f.prot;
    s1_lgpgsz = result->f.lg_page_size;
    s1_guarded = result->f.extra.arm.guarded;
    cacheattrs1 = result->cacheattrs;
    memset(result, 0, sizeof(*result));

    ret = get_phys_addr_nogpc(env, ptw, ipa, access_type,
                              memop, result, fi);
    fi->s2addr = ipa;

    result->f.prot &= s1_prot;

    if (ret) {
        return ret;
    }

    if (result->f.lg_page_size < TARGET_PAGE_BITS ||
        s1_lgpgsz < TARGET_PAGE_BITS) {
        result->f.lg_page_size = 0;
    } else if (result->f.lg_page_size < s1_lgpgsz) {
        result->f.lg_page_size = s1_lgpgsz;
    }

    hcr = arm_hcr_el2_eff_secstate(env, in_space);
    if (hcr & HCR_DC) {
        if (cacheattrs1.attrs != 0xf0) {
            cacheattrs1.attrs = 0xff;
        }
        cacheattrs1.shareability = 0;
    }
    result->cacheattrs = combine_cacheattrs(hcr, cacheattrs1,
                                            result->cacheattrs);

    result->f.extra.arm.guarded = s1_guarded;

    if (in_space == ARMSS_Secure) {
        result->f.attrs.secure =
            !(env->cp15.vstcr_el2 & (VSTCR_SA | VSTCR_SW))
            && (ipa_secure
                || !(env->cp15.vtcr_el2 & (VTCR_NSA | VTCR_NSW)));
        result->f.attrs.space = arm_secure_to_space(result->f.attrs.secure);
    }

    return false;
}

static bool get_phys_addr_nogpc(CPUARMState *env, S1Translate *ptw,
                                      vaddr address,
                                      MMUAccessType access_type, MemOp memop,
                                      GetPhysAddrResult *result,
                                      ARMMMUFaultInfo *fi)
{
    ARMMMUIdx mmu_idx = ptw->in_mmu_idx;
    ARMMMUIdx s1_mmu_idx;

    result->f.attrs.space = ptw->in_space;
    result->f.attrs.secure = arm_space_is_secure(ptw->in_space);

    switch (mmu_idx) {
    case ARMMMUIdx_Phys_S:
    case ARMMMUIdx_Phys_NS:
    case ARMMMUIdx_Phys_Root:
    case ARMMMUIdx_Phys_Realm:
        return get_phys_addr_disabled(env, ptw, address, access_type,
                                      result, fi);

    case ARMMMUIdx_Stage1_E0:
    case ARMMMUIdx_Stage1_E1:
    case ARMMMUIdx_Stage1_E1_PAN:
        ptw->in_ptw_idx = (ptw->in_space == ARMSS_Secure) ?
            ARMMMUIdx_Stage2_S : ARMMMUIdx_Stage2;
        break;

    case ARMMMUIdx_Stage2:
    case ARMMMUIdx_Stage2_S:
        ptw->in_ptw_idx = ptw_idx_for_stage_2(env, mmu_idx);
        break;

    case ARMMMUIdx_E10_0:
        s1_mmu_idx = ARMMMUIdx_Stage1_E0;
        goto do_twostage;
    case ARMMMUIdx_E10_1:
        s1_mmu_idx = ARMMMUIdx_Stage1_E1;
        goto do_twostage;
    case ARMMMUIdx_E10_1_PAN:
        s1_mmu_idx = ARMMMUIdx_Stage1_E1_PAN;
    do_twostage:
        ptw->in_mmu_idx = mmu_idx = s1_mmu_idx;
        if (arm_feature(env, ARM_FEATURE_EL2) &&
            !regime_translation_disabled(env, ARMMMUIdx_Stage2, ptw->in_space)) {
            return get_phys_addr_twostage(env, ptw, address, access_type,
                                          memop, result, fi);
        }

    default:
        ptw->in_ptw_idx = arm_space_to_phys(ptw->in_space);
        break;
    }

    result->f.attrs.user = regime_is_user(env, mmu_idx);

    if (address < 0x02000000 && mmu_idx != ARMMMUIdx_Stage2
        && !arm_feature(env, ARM_FEATURE_V8)) {
        if (regime_el(env, mmu_idx) == 3) {
            address += env->cp15.fcseidr_s;
        } else {
            address += env->cp15.fcseidr_ns;
        }
    }

    if (arm_feature(env, ARM_FEATURE_PMSA)) {
        bool ret;
        result->f.lg_page_size = TARGET_PAGE_BITS;

        if (arm_feature(env, ARM_FEATURE_V8)) {
            ret = get_phys_addr_pmsav8(env, ptw, address, access_type,
                                       result, fi);
        } else if (arm_feature(env, ARM_FEATURE_V7)) {
            ret = get_phys_addr_pmsav7(env, ptw, address, access_type,
                                       result, fi);
        } else {
            ret = get_phys_addr_pmsav5(env, ptw, address, access_type,
                                       result, fi);
        }
        qemu_log_mask(CPU_LOG_MMU, "PMSA MPU lookup for %s at 0x%08" PRIx32
                      " mmu_idx %u -> %s (prot %c%c%c)\n",
                      access_type == MMU_DATA_LOAD ? "reading" :
                      (access_type == MMU_DATA_STORE ? "writing" : "execute"),
                      (uint32_t)address, mmu_idx,
                      ret ? "Miss" : "Hit",
                      result->f.prot & PAGE_READ ? 'r' : '-',
                      result->f.prot & PAGE_WRITE ? 'w' : '-',
                      result->f.prot & PAGE_EXEC ? 'x' : '-');

        return ret;
    }


    if (regime_translation_disabled(env, mmu_idx, ptw->in_space)) {
        return get_phys_addr_disabled(env, ptw, address, access_type,
                                      result, fi);
    }

    if (regime_using_lpae_format(env, mmu_idx)) {
        return get_phys_addr_lpae(env, ptw, address, access_type,
                                  memop, result, fi);
    } else if (arm_feature(env, ARM_FEATURE_V7) ||
               regime_sctlr(env, mmu_idx) & SCTLR_XP) {
        return get_phys_addr_v6(env, ptw, address, access_type, result, fi);
    } else {
        return get_phys_addr_v5(env, ptw, address, access_type, result, fi);
    }
}

static bool get_phys_addr_gpc(CPUARMState *env, S1Translate *ptw,
                              vaddr address,
                              MMUAccessType access_type, MemOp memop,
                              GetPhysAddrResult *result,
                              ARMMMUFaultInfo *fi)
{
    if (get_phys_addr_nogpc(env, ptw, address, access_type,
                            memop, result, fi)) {
        return true;
    }
    if (!granule_protection_check(env, result->f.phys_addr,
                                  result->f.attrs.space, fi)) {
        fi->type = ARMFault_GPCFOnOutput;
        return true;
    }
    return false;
}

bool get_phys_addr_with_space_nogpc(CPUARMState *env, vaddr address,
                                    MMUAccessType access_type, MemOp memop,
                                    ARMMMUIdx mmu_idx, ARMSecuritySpace space,
                                    GetPhysAddrResult *result,
                                    ARMMMUFaultInfo *fi)
{
    S1Translate ptw = {
        .in_mmu_idx = mmu_idx,
        .in_space = space,
    };
    return get_phys_addr_nogpc(env, &ptw, address, access_type,
                               memop, result, fi);
}

bool get_phys_addr(CPUARMState *env, vaddr address,
                   MMUAccessType access_type, MemOp memop, ARMMMUIdx mmu_idx,
                   GetPhysAddrResult *result, ARMMMUFaultInfo *fi)
{
    S1Translate ptw = {
        .in_mmu_idx = mmu_idx,
    };
    ARMSecuritySpace ss;

    switch (mmu_idx) {
    case ARMMMUIdx_E10_0:
    case ARMMMUIdx_E10_1:
    case ARMMMUIdx_E10_1_PAN:
    case ARMMMUIdx_E20_0:
    case ARMMMUIdx_E20_2:
    case ARMMMUIdx_E20_2_PAN:
    case ARMMMUIdx_Stage1_E0:
    case ARMMMUIdx_Stage1_E1:
    case ARMMMUIdx_Stage1_E1_PAN:
    case ARMMMUIdx_E2:
        ss = arm_security_space_below_el3(env);
        break;
    case ARMMMUIdx_Stage2:
        ss = arm_security_space_below_el3(env);
        if (ss == ARMSS_Secure) {
            ss = ARMSS_NonSecure;
        }
        break;
    case ARMMMUIdx_Phys_NS:
    case ARMMMUIdx_MPrivNegPri:
    case ARMMMUIdx_MUserNegPri:
    case ARMMMUIdx_MPriv:
    case ARMMMUIdx_MUser:
        ss = ARMSS_NonSecure;
        break;
    case ARMMMUIdx_Stage2_S:
    case ARMMMUIdx_Phys_S:
    case ARMMMUIdx_MSPrivNegPri:
    case ARMMMUIdx_MSUserNegPri:
    case ARMMMUIdx_MSPriv:
    case ARMMMUIdx_MSUser:
        ss = ARMSS_Secure;
        break;
    case ARMMMUIdx_E3:
    case ARMMMUIdx_E30_0:
    case ARMMMUIdx_E30_3_PAN:
        if (arm_feature(env, ARM_FEATURE_AARCH64) &&
            cpu_isar_feature(aa64_rme, env_archcpu(env))) {
            ss = ARMSS_Root;
        } else {
            ss = ARMSS_Secure;
        }
        break;
    case ARMMMUIdx_Phys_Root:
        ss = ARMSS_Root;
        break;
    case ARMMMUIdx_Phys_Realm:
        ss = ARMSS_Realm;
        break;
    default:
        g_assert_not_reached();
    }

    ptw.in_space = ss;
    return get_phys_addr_gpc(env, &ptw, address, access_type,
                             memop, result, fi);
}

hwaddr arm_cpu_get_phys_page_attrs_debug(CPUState *cs, vaddr addr,
                                         MemTxAttrs *attrs)
{
    ARMCPU *cpu = ARM_CPU(cs);
    CPUARMState *env = &cpu->env;
    ARMMMUIdx mmu_idx = arm_mmu_idx(env);
    ARMSecuritySpace ss = arm_security_space(env);
    S1Translate ptw = {
        .in_mmu_idx = mmu_idx,
        .in_space = ss,
        .in_debug = true,
    };
    GetPhysAddrResult res = {};
    ARMMMUFaultInfo fi = {};
    bool ret;

    ret = get_phys_addr_gpc(env, &ptw, addr, MMU_DATA_LOAD, 0, &res, &fi);
    *attrs = res.f.attrs;

    if (ret) {
        return -1;
    }
    return res.f.phys_addr;
}
