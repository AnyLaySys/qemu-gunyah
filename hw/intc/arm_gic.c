

#include "qemu/osdep.h"
#include "hw/irq.h"
#include "hw/sysbus.h"
#include "gic_internal.h"
#include "qapi/error.h"
#include "hw/core/cpu.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "trace.h"


#ifdef DEBUG_GIC
#define DEBUG_GIC_GATE 1
#else
#define DEBUG_GIC_GATE 0
#endif

#define DPRINTF(fmt, ...) do {                                          \
        if (DEBUG_GIC_GATE) {                                           \
            fprintf(stderr, "%s: " fmt, __func__, ## __VA_ARGS__);      \
        }                                                               \
    } while (0)

static const uint8_t gic_id_11mpcore[] = {
    0x00, 0x00, 0x00, 0x00, 0x90, 0x13, 0x04, 0x00, 0x0d, 0xf0, 0x05, 0xb1
};

static const uint8_t gic_id_gicv1[] = {
    0x04, 0x00, 0x00, 0x00, 0x90, 0xb3, 0x1b, 0x00, 0x0d, 0xf0, 0x05, 0xb1
};

static const uint8_t gic_id_gicv2[] = {
    0x04, 0x00, 0x00, 0x00, 0x90, 0xb4, 0x2b, 0x00, 0x0d, 0xf0, 0x05, 0xb1
};

static inline int gic_get_current_cpu(GICState *s)
{
    if (s->num_cpu > 1) {
        return current_cpu->cpu_index;
    }
    return 0;
}

static inline int gic_get_current_vcpu(GICState *s)
{
    return gic_get_current_cpu(s) + GIC_NCPU;
}

static inline bool gic_has_groups(GICState *s)
{
    return s->revision == 2 || s->security_extn;
}

static inline bool gic_cpu_ns_access(GICState *s, int cpu, MemTxAttrs attrs)
{
    return !gic_is_vcpu(cpu) && s->security_extn && !attrs.secure;
}

static inline void gic_get_best_irq(GICState *s, int cpu,
                                    int *best_irq, int *best_prio, int *group)
{
    int irq;
    int cm = 1 << cpu;

    *best_irq = 1023;
    *best_prio = 0x100;

    for (irq = 0; irq < s->num_irq; irq++) {
        if (GIC_DIST_TEST_ENABLED(irq, cm) && gic_test_pending(s, irq, cm) &&
            (!GIC_DIST_TEST_ACTIVE(irq, cm)) &&
            (irq < GIC_INTERNAL || GIC_DIST_TARGET(irq) & cm)) {
            if (GIC_DIST_GET_PRIORITY(irq, cpu) < *best_prio) {
                *best_prio = GIC_DIST_GET_PRIORITY(irq, cpu);
                *best_irq = irq;
            }
        }
    }

    if (*best_irq < 1023) {
        *group = GIC_DIST_TEST_GROUP(*best_irq, cm);
    }
}

static inline void gic_get_best_virq(GICState *s, int cpu,
                                     int *best_irq, int *best_prio, int *group)
{
    int lr_idx = 0;

    *best_irq = 1023;
    *best_prio = 0x100;

    for (lr_idx = 0; lr_idx < s->num_lrs; lr_idx++) {
        uint32_t lr_entry = s->h_lr[lr_idx][cpu];
        int state = GICH_LR_STATE(lr_entry);

        if (state == GICH_LR_STATE_PENDING) {
            int prio = GICH_LR_PRIORITY(lr_entry);

            if (prio < *best_prio) {
                *best_prio = prio;
                *best_irq = GICH_LR_VIRT_ID(lr_entry);
                *group = GICH_LR_GROUP(lr_entry);
            }
        }
    }
}

static inline bool gic_irq_signaling_enabled(GICState *s, int cpu, bool virt,
                                    int group_mask)
{
    int cpu_iface = virt ? (cpu + GIC_NCPU) : cpu;

    if (!virt && !(s->ctlr & group_mask)) {
        return false;
    }

    if (virt && !(s->h_hcr[cpu] & R_GICH_HCR_EN_MASK)) {
        return false;
    }

    if (!(s->cpu_ctlr[cpu_iface] & group_mask)) {
        return false;
    }

    return true;
}

static inline void gic_update_internal(GICState *s, bool virt)
{
    int best_irq;
    int best_prio;
    int irq_level, fiq_level;
    int cpu, cpu_iface;
    int group = 0;
    qemu_irq *irq_lines = virt ? s->parent_virq : s->parent_irq;
    qemu_irq *fiq_lines = virt ? s->parent_vfiq : s->parent_fiq;

    for (cpu = 0; cpu < s->num_cpu; cpu++) {
        cpu_iface = virt ? (cpu + GIC_NCPU) : cpu;

        s->current_pending[cpu_iface] = 1023;
        if (!gic_irq_signaling_enabled(s, cpu, virt,
                                       GICD_CTLR_EN_GRP0 | GICD_CTLR_EN_GRP1)) {
            qemu_irq_lower(irq_lines[cpu]);
            qemu_irq_lower(fiq_lines[cpu]);
            continue;
        }

        if (virt) {
            gic_get_best_virq(s, cpu, &best_irq, &best_prio, &group);
        } else {
            gic_get_best_irq(s, cpu, &best_irq, &best_prio, &group);
        }

        if (best_irq != 1023) {
            trace_gic_update_bestirq(virt ? "vcpu" : "cpu", cpu,
                                     best_irq, best_prio,
                                     s->priority_mask[cpu_iface],
                                     s->running_priority[cpu_iface]);
        }

        irq_level = fiq_level = 0;

        if (best_prio < s->priority_mask[cpu_iface]) {
            s->current_pending[cpu_iface] = best_irq;
            if (best_prio < s->running_priority[cpu_iface]) {
                if (gic_irq_signaling_enabled(s, cpu, virt, 1 << group)) {
                    if (group == 0 &&
                        s->cpu_ctlr[cpu_iface] & GICC_CTLR_FIQ_EN) {
                        DPRINTF("Raised pending FIQ %d (cpu %d)\n",
                                best_irq, cpu_iface);
                        fiq_level = 1;
                        trace_gic_update_set_irq(cpu, virt ? "vfiq" : "fiq",
                                                 fiq_level);
                    } else {
                        DPRINTF("Raised pending IRQ %d (cpu %d)\n",
                                best_irq, cpu_iface);
                        irq_level = 1;
                        trace_gic_update_set_irq(cpu, virt ? "virq" : "irq",
                                                 irq_level);
                    }
                }
            }
        }

        qemu_set_irq(irq_lines[cpu], irq_level);
        qemu_set_irq(fiq_lines[cpu], fiq_level);
    }
}

static void gic_update(GICState *s)
{
    gic_update_internal(s, false);
}

static inline bool gic_lr_entry_is_free(uint32_t entry)
{
    return (GICH_LR_STATE(entry) == GICH_LR_STATE_INVALID)
        && (GICH_LR_HW(entry) || !GICH_LR_EOI(entry));
}

static inline bool gic_lr_entry_is_eoi(uint32_t entry)
{
    return (GICH_LR_STATE(entry) == GICH_LR_STATE_INVALID)
        && !GICH_LR_HW(entry) && GICH_LR_EOI(entry);
}

static inline void gic_extract_lr_info(GICState *s, int cpu,
                                int *num_eoi, int *num_valid, int *num_pending)
{
    int lr_idx;

    *num_eoi = 0;
    *num_valid = 0;
    *num_pending = 0;

    for (lr_idx = 0; lr_idx < s->num_lrs; lr_idx++) {
        uint32_t *entry = &s->h_lr[lr_idx][cpu];

        if (gic_lr_entry_is_eoi(*entry)) {
            (*num_eoi)++;
        }

        if (GICH_LR_STATE(*entry) != GICH_LR_STATE_INVALID) {
            (*num_valid)++;
        }

        if (GICH_LR_STATE(*entry) == GICH_LR_STATE_PENDING) {
            (*num_pending)++;
        }
    }
}

static void gic_compute_misr(GICState *s, int cpu)
{
    uint32_t value = 0;
    int vcpu = cpu + GIC_NCPU;

    int num_eoi, num_valid, num_pending;

    gic_extract_lr_info(s, cpu, &num_eoi, &num_valid, &num_pending);

    if (num_eoi) {
        value |= R_GICH_MISR_EOI_MASK;
    }

    if ((s->h_hcr[cpu] & R_GICH_HCR_UIE_MASK) && (num_valid < 2)) {
        value |= R_GICH_MISR_U_MASK;
    }

    if ((s->h_hcr[cpu] & R_GICH_HCR_LRENPIE_MASK) &&
        ((s->h_hcr[cpu] & R_GICH_HCR_EOICount_MASK) != 0)) {
        value |= R_GICH_MISR_LRENP_MASK;
    }

    if ((s->h_hcr[cpu] & R_GICH_HCR_NPIE_MASK) && (num_pending == 0)) {
        value |= R_GICH_MISR_NP_MASK;
    }

    if ((s->h_hcr[cpu] & R_GICH_HCR_VGRP0EIE_MASK) &&
        (s->cpu_ctlr[vcpu] & GICC_CTLR_EN_GRP0)) {
        value |= R_GICH_MISR_VGrp0E_MASK;
    }

    if ((s->h_hcr[cpu] & R_GICH_HCR_VGRP0DIE_MASK) &&
        !(s->cpu_ctlr[vcpu] & GICC_CTLR_EN_GRP0)) {
        value |= R_GICH_MISR_VGrp0D_MASK;
    }

    if ((s->h_hcr[cpu] & R_GICH_HCR_VGRP1EIE_MASK) &&
        (s->cpu_ctlr[vcpu] & GICC_CTLR_EN_GRP1)) {
        value |= R_GICH_MISR_VGrp1E_MASK;
    }

    if ((s->h_hcr[cpu] & R_GICH_HCR_VGRP1DIE_MASK) &&
        !(s->cpu_ctlr[vcpu] & GICC_CTLR_EN_GRP1)) {
        value |= R_GICH_MISR_VGrp1D_MASK;
    }

    s->h_misr[cpu] = value;
}

static void gic_update_maintenance(GICState *s)
{
    int cpu = 0;
    int maint_level;

    for (cpu = 0; cpu < s->num_cpu; cpu++) {
        gic_compute_misr(s, cpu);
        maint_level = (s->h_hcr[cpu] & R_GICH_HCR_EN_MASK) && s->h_misr[cpu];

        trace_gic_update_maintenance_irq(cpu, maint_level);
        qemu_set_irq(s->maintenance_irq[cpu], maint_level);
    }
}

static void gic_update_virt(GICState *s)
{
    gic_update_internal(s, true);
    gic_update_maintenance(s);
}

static void gic_set_irq_11mpcore(GICState *s, int irq, int level,
                                 int cm, int target)
{
    if (level) {
        GIC_DIST_SET_LEVEL(irq, cm);
        if (GIC_DIST_TEST_EDGE_TRIGGER(irq) || GIC_DIST_TEST_ENABLED(irq, cm)) {
            DPRINTF("Set %d pending mask %x\n", irq, target);
            GIC_DIST_SET_PENDING(irq, target);
        }
    } else {
        GIC_DIST_CLEAR_LEVEL(irq, cm);
    }
}

static void gic_set_irq_generic(GICState *s, int irq, int level,
                                int cm, int target)
{
    if (level) {
        GIC_DIST_SET_LEVEL(irq, cm);
        DPRINTF("Set %d pending mask %x\n", irq, target);
        if (GIC_DIST_TEST_EDGE_TRIGGER(irq)) {
            GIC_DIST_SET_PENDING(irq, target);
        }
    } else {
        GIC_DIST_CLEAR_LEVEL(irq, cm);
    }
}

static void gic_set_irq(void *opaque, int irq, int level)
{
    GICState *s = (GICState *)opaque;
    int cm, target;
    if (irq < (s->num_irq - GIC_INTERNAL)) {
        cm = ALL_CPU_MASK;
        irq += GIC_INTERNAL;
        target = GIC_DIST_TARGET(irq);
    } else {
        int cpu;
        irq -= (s->num_irq - GIC_INTERNAL);
        cpu = irq / GIC_INTERNAL;
        irq %= GIC_INTERNAL;
        cm = 1 << cpu;
        target = cm;
    }

    assert(irq >= GIC_NR_SGIS);

    if (level == GIC_DIST_TEST_LEVEL(irq, cm)) {
        return;
    }

    if (s->revision == REV_11MPCORE) {
        gic_set_irq_11mpcore(s, irq, level, cm, target);
    } else {
        gic_set_irq_generic(s, irq, level, cm, target);
    }
    trace_gic_set_irq(irq, level, cm, target);

    gic_update(s);
}

static uint16_t gic_get_current_pending_irq(GICState *s, int cpu,
                                            MemTxAttrs attrs)
{
    uint16_t pending_irq = s->current_pending[cpu];

    if (pending_irq < GIC_MAXIRQ && gic_has_groups(s)) {
        int group = gic_test_group(s, pending_irq, cpu);

        bool secure = !gic_cpu_ns_access(s, cpu, attrs);

        if (group == 0 && !secure) {
            return 1023;
        }
        if (group == 1 && secure && !(s->cpu_ctlr[cpu] & GICC_CTLR_ACK_CTL)) {
            return 1022;
        }
    }
    return pending_irq;
}

static int gic_get_group_priority(GICState *s, int cpu, int irq)
{
    int bpr;
    uint32_t mask;

    if (gic_has_groups(s) &&
        !(s->cpu_ctlr[cpu] & GICC_CTLR_CBPR) &&
        gic_test_group(s, irq, cpu)) {
        bpr = s->abpr[cpu] - 1;
        assert(bpr >= 0);
    } else {
        bpr = s->bpr[cpu];
    }

    mask = ~0U << ((bpr & 7) + 1);

    return gic_get_priority(s, irq, cpu) & mask;
}

static void gic_activate_irq(GICState *s, int cpu, int irq)
{
    int prio = gic_get_group_priority(s, cpu, irq);
    int min_bpr = gic_is_vcpu(cpu) ? GIC_VIRT_MIN_BPR : GIC_MIN_BPR;
    int preemption_level = prio >> (min_bpr + 1);
    int regno = preemption_level / 32;
    int bitno = preemption_level % 32;
    uint32_t *papr = NULL;

    if (gic_is_vcpu(cpu)) {
        assert(regno == 0);
        papr = &s->h_apr[gic_get_vcpu_real_id(cpu)];
    } else if (gic_has_groups(s) && gic_test_group(s, irq, cpu)) {
        papr = &s->nsapr[regno][cpu];
    } else {
        papr = &s->apr[regno][cpu];
    }

    *papr |= (1 << bitno);

    s->running_priority[cpu] = prio;
    gic_set_active(s, irq, cpu);
}

static int gic_get_prio_from_apr_bits(GICState *s, int cpu)
{
    int i;

    if (gic_is_vcpu(cpu)) {
        uint32_t apr = s->h_apr[gic_get_vcpu_real_id(cpu)];
        if (apr) {
            return ctz32(apr) << (GIC_VIRT_MIN_BPR + 1);
        } else {
            return 0x100;
        }
    }

    for (i = 0; i < GIC_NR_APRS; i++) {
        uint32_t apr = s->apr[i][cpu] | s->nsapr[i][cpu];
        if (!apr) {
            continue;
        }
        return (i * 32 + ctz32(apr)) << (GIC_MIN_BPR + 1);
    }
    return 0x100;
}

static void gic_drop_prio(GICState *s, int cpu, int group)
{
    if (gic_is_vcpu(cpu)) {
        int rcpu = gic_get_vcpu_real_id(cpu);

        if (s->h_apr[rcpu]) {
            s->h_apr[rcpu] &= s->h_apr[rcpu] - 1;
        }
    } else {
        int i;

        for (i = 0; i < GIC_NR_APRS; i++) {
            uint32_t *papr = group ? &s->nsapr[i][cpu] : &s->apr[i][cpu];
            if (!*papr) {
                continue;
            }
            *papr &= *papr - 1;
            break;
        }
    }

    s->running_priority[cpu] = gic_get_prio_from_apr_bits(s, cpu);
}

static inline uint32_t gic_clear_pending_sgi(GICState *s, int irq, int cpu)
{
    int src;
    uint32_t ret;

    if (!gic_is_vcpu(cpu)) {
        assert(s->sgi_pending[irq][cpu] != 0);
        src = ctz32(s->sgi_pending[irq][cpu]);
        s->sgi_pending[irq][cpu] &= ~(1 << src);
        if (s->sgi_pending[irq][cpu] == 0) {
            gic_clear_pending(s, irq, cpu);
        }
        ret = irq | ((src & 0x7) << 10);
    } else {
        uint32_t *lr_entry = gic_get_lr_entry(s, irq, cpu);
        src = GICH_LR_CPUID(*lr_entry);

        gic_clear_pending(s, irq, cpu);
        ret = irq | (src << 10);
    }

    return ret;
}

uint32_t gic_acknowledge_irq(GICState *s, int cpu, MemTxAttrs attrs)
{
    int ret, irq;

    irq = gic_get_current_pending_irq(s, cpu, attrs);
    trace_gic_acknowledge_irq(gic_is_vcpu(cpu) ? "vcpu" : "cpu",
                              gic_get_vcpu_real_id(cpu), irq);

    if (irq >= GIC_MAXIRQ) {
        DPRINTF("ACK, no pending interrupt or it is hidden: %d\n", irq);
        return irq;
    }

    if (gic_get_priority(s, irq, cpu) >= s->running_priority[cpu]) {
        DPRINTF("ACK, pending interrupt (%d) has insufficient priority\n", irq);
        return 1023;
    }

    gic_activate_irq(s, cpu, irq);

    if (s->revision == REV_11MPCORE) {
        gic_clear_pending(s, irq, cpu);
        ret = irq;
    } else {
        if (irq < GIC_NR_SGIS) {
            ret = gic_clear_pending_sgi(s, irq, cpu);
        } else {
            gic_clear_pending(s, irq, cpu);
            ret = irq;
        }
    }

    if (gic_is_vcpu(cpu)) {
        gic_update_virt(s);
    } else {
        gic_update(s);
    }
    DPRINTF("ACK %d\n", irq);
    return ret;
}

static uint32_t gic_fullprio_mask(GICState *s, int cpu)
{
    int priBits;

    if (gic_is_vcpu(cpu)) {
        priBits = GIC_VIRT_MAX_GROUP_PRIO_BITS;
    } else {
        priBits = s->n_prio_bits;
    }
    return ~0U << (8 - priBits);
}

void gic_dist_set_priority(GICState *s, int cpu, int irq, uint8_t val,
                      MemTxAttrs attrs)
{
    if (s->security_extn && !attrs.secure) {
        if (!GIC_DIST_TEST_GROUP(irq, (1 << cpu))) {
            return; /* Ignore Non-secure access of Group0 IRQ */
        }
        val = 0x80 | (val >> 1); /* Non-secure view */
    }

    val &= gic_fullprio_mask(s, cpu);

    if (irq < GIC_INTERNAL) {
        s->priority1[irq][cpu] = val;
    } else {
        s->priority2[(irq) - GIC_INTERNAL] = val;
    }
}

static uint32_t gic_dist_get_priority(GICState *s, int cpu, int irq,
                                 MemTxAttrs attrs)
{
    uint32_t prio = GIC_DIST_GET_PRIORITY(irq, cpu);

    if (s->security_extn && !attrs.secure) {
        if (!GIC_DIST_TEST_GROUP(irq, (1 << cpu))) {
            return 0; /* Non-secure access cannot read priority of Group0 IRQ */
        }
        prio = (prio << 1) & 0xff; /* Non-secure view */
    }
    return prio & gic_fullprio_mask(s, cpu);
}

static void gic_set_priority_mask(GICState *s, int cpu, uint8_t pmask,
                                  MemTxAttrs attrs)
{
    if (gic_cpu_ns_access(s, cpu, attrs)) {
        if (s->priority_mask[cpu] & 0x80) {
            pmask = 0x80 | (pmask >> 1);
        } else {
            return;
        }
    }
    s->priority_mask[cpu] = pmask & gic_fullprio_mask(s, cpu);
}

static uint32_t gic_get_priority_mask(GICState *s, int cpu, MemTxAttrs attrs)
{
    uint32_t pmask = s->priority_mask[cpu];

    if (gic_cpu_ns_access(s, cpu, attrs)) {
        if (pmask & 0x80) {
            pmask = (pmask << 1) & 0xff;
        } else {
            pmask = 0;
        }
    }
    return pmask;
}

static uint32_t gic_get_cpu_control(GICState *s, int cpu, MemTxAttrs attrs)
{
    uint32_t ret = s->cpu_ctlr[cpu];

    if (gic_cpu_ns_access(s, cpu, attrs)) {
        ret = (ret & (GICC_CTLR_EN_GRP1 | GICC_CTLR_EOIMODE_NS)) >> 1;
    }
    return ret;
}

static void gic_set_cpu_control(GICState *s, int cpu, uint32_t value,
                                MemTxAttrs attrs)
{
    uint32_t mask;

    if (gic_cpu_ns_access(s, cpu, attrs)) {
        mask = GICC_CTLR_EN_GRP1;
        if (s->revision == 2) {
            mask |= GICC_CTLR_EOIMODE_NS;
        }
        s->cpu_ctlr[cpu] &= ~mask;
        s->cpu_ctlr[cpu] |= (value << 1) & mask;
    } else {
        if (s->revision == 2) {
            mask = s->security_extn ? GICC_CTLR_V2_S_MASK : GICC_CTLR_V2_MASK;
        } else {
            mask = s->security_extn ? GICC_CTLR_V1_S_MASK : GICC_CTLR_V1_MASK;
        }
        s->cpu_ctlr[cpu] = value & mask;
    }
    DPRINTF("CPU Interface %d: Group0 Interrupts %sabled, "
            "Group1 Interrupts %sabled\n", cpu,
            (s->cpu_ctlr[cpu] & GICC_CTLR_EN_GRP0) ? "En" : "Dis",
            (s->cpu_ctlr[cpu] & GICC_CTLR_EN_GRP1) ? "En" : "Dis");
}

static uint8_t gic_get_running_priority(GICState *s, int cpu, MemTxAttrs attrs)
{
    if ((s->revision != REV_11MPCORE) && (s->running_priority[cpu] > 0xff)) {
        return 0xff;
    }

    if (gic_cpu_ns_access(s, cpu, attrs)) {
        if (s->running_priority[cpu] & 0x80) {
            return s->running_priority[cpu] << 1;
        } else {
            return 0;
        }
    } else {
        return s->running_priority[cpu];
    }
}

static bool gic_eoi_split(GICState *s, int cpu, MemTxAttrs attrs)
{
    if (s->revision != 2) {
        return false;
    }
    if (gic_cpu_ns_access(s, cpu, attrs)) {
        return s->cpu_ctlr[cpu] & GICC_CTLR_EOIMODE_NS;
    }
    return s->cpu_ctlr[cpu] & GICC_CTLR_EOIMODE;
}

static void gic_deactivate_irq(GICState *s, int cpu, int irq, MemTxAttrs attrs)
{
    int group;

    if (irq >= GIC_MAXIRQ || (!gic_is_vcpu(cpu) && irq >= s->num_irq)) {
        return;
    }

    if (!gic_eoi_split(s, cpu, attrs)) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "gic_deactivate_irq: GICC_DIR write when EOIMode clear");
        return;
    }

    if (gic_is_vcpu(cpu) && !gic_virq_is_valid(s, irq, cpu)) {
        int rcpu = gic_get_vcpu_real_id(cpu);
        s->h_hcr[rcpu] += 1 << R_GICH_HCR_EOICount_SHIFT;

        gic_update_virt(s);
        return;
    }

    group = gic_has_groups(s) && gic_test_group(s, irq, cpu);

    if (gic_cpu_ns_access(s, cpu, attrs) && !group) {
        DPRINTF("Non-secure DI for Group0 interrupt %d ignored\n", irq);
        return;
    }

    gic_clear_active(s, irq, cpu);
}

static void gic_complete_irq(GICState *s, int cpu, int irq, MemTxAttrs attrs)
{
    int cm = 1 << cpu;
    int group;

    DPRINTF("EOI %d\n", irq);
    if (gic_is_vcpu(cpu)) {
        bool prio_drop = s->running_priority[cpu] < 0x100;

        if (irq >= GIC_MAXIRQ) {
            return;
        }

        gic_drop_prio(s, cpu, 0);

        if (!gic_eoi_split(s, cpu, attrs)) {
            bool valid = gic_virq_is_valid(s, irq, cpu);
            if (prio_drop && !valid) {
                int rcpu = gic_get_vcpu_real_id(cpu);
                s->h_hcr[rcpu] += 1 << R_GICH_HCR_EOICount_SHIFT;
            } else if (valid) {
                gic_clear_active(s, irq, cpu);
            }
        }

        gic_update_virt(s);
        return;
    }

    if (irq >= s->num_irq) {
        return;
    }
    if (s->running_priority[cpu] == 0x100) {
        return; /* No active IRQ.  */
    }

    if (s->revision == REV_11MPCORE) {
        if (!GIC_DIST_TEST_EDGE_TRIGGER(irq) && GIC_DIST_TEST_ENABLED(irq, cm)
            && GIC_DIST_TEST_LEVEL(irq, cm)
            && (GIC_DIST_TARGET(irq) & cm) != 0) {
            DPRINTF("Set %d pending mask %x\n", irq, cm);
            GIC_DIST_SET_PENDING(irq, cm);
        }
    }

    group = gic_has_groups(s) && gic_test_group(s, irq, cpu);

    if (gic_cpu_ns_access(s, cpu, attrs) && !group) {
        DPRINTF("Non-secure EOI for Group0 interrupt %d ignored\n", irq);
        return;
    }


    gic_drop_prio(s, cpu, group);

    if (!gic_eoi_split(s, cpu, attrs)) {
        gic_clear_active(s, irq, cpu);
    }
    gic_update(s);
}

static uint8_t gic_dist_readb(void *opaque, hwaddr offset, MemTxAttrs attrs)
{
    GICState *s = (GICState *)opaque;
    uint32_t res;
    int irq;
    int i;
    int cpu;
    int cm;
    int mask;

    cpu = gic_get_current_cpu(s);
    cm = 1 << cpu;
    if (offset < 0x100) {
        if (offset == 0) {      /* GICD_CTLR */
            if (s->security_extn && !attrs.secure) {
                return extract32(s->ctlr, 1, 1);
            } else {
                return s->ctlr;
            }
        }
        if (offset == 4) {
            return ((s->num_irq / 32) - 1) | ((s->num_cpu - 1) << 5);
        }
        if (offset == 5) {
            return (s->security_extn << 2);
        }
        if (offset == 8) {
            return 0x3b; /* Arm JEP106 identity */
        }
        if (offset == 9) {
            return 0x04; /* Arm JEP106 identity */
        }
        if (offset < 0x0c) {
            return 0;
        }
        if (offset >= 0x80) {
            res = 0;
            if (!(s->security_extn && !attrs.secure) && gic_has_groups(s)) {
                irq = (offset - 0x080) * 8;
                if (irq >= s->num_irq) {
                    goto bad_reg;
                }
                for (i = 0; i < 8; i++) {
                    if (GIC_DIST_TEST_GROUP(irq + i, cm)) {
                        res |= (1 << i);
                    }
                }
            }
            return res;
        }
        goto bad_reg;
    } else if (offset < 0x200) {
        if (offset < 0x180)
            irq = (offset - 0x100) * 8;
        else
            irq = (offset - 0x180) * 8;
        if (irq >= s->num_irq)
            goto bad_reg;
        res = 0;
        for (i = 0; i < 8; i++) {
            if (s->security_extn && !attrs.secure &&
                !GIC_DIST_TEST_GROUP(irq + i, 1 << cpu)) {
                continue; /* Ignore Non-secure access of Group0 IRQ */
            }

            if (GIC_DIST_TEST_ENABLED(irq + i, cm)) {
                res |= (1 << i);
            }
        }
    } else if (offset < 0x300) {
        if (offset < 0x280)
            irq = (offset - 0x200) * 8;
        else
            irq = (offset - 0x280) * 8;
        if (irq >= s->num_irq)
            goto bad_reg;
        res = 0;
        mask = (irq < GIC_INTERNAL) ?  cm : ALL_CPU_MASK;
        for (i = 0; i < 8; i++) {
            if (s->security_extn && !attrs.secure &&
                !GIC_DIST_TEST_GROUP(irq + i, 1 << cpu)) {
                continue; /* Ignore Non-secure access of Group0 IRQ */
            }

            if (gic_test_pending(s, irq + i, mask)) {
                res |= (1 << i);
            }
        }
    } else if (offset < 0x400) {
        if (offset < 0x380) {
            irq = (offset - 0x300) * 8;
        } else if (s->revision == 2) {
            irq = (offset - 0x380) * 8;
        } else {
            goto bad_reg;
        }

        if (irq >= s->num_irq)
            goto bad_reg;
        res = 0;
        mask = (irq < GIC_INTERNAL) ?  cm : ALL_CPU_MASK;
        for (i = 0; i < 8; i++) {
            if (s->security_extn && !attrs.secure &&
                !GIC_DIST_TEST_GROUP(irq + i, 1 << cpu)) {
                continue; /* Ignore Non-secure access of Group0 IRQ */
            }

            if (GIC_DIST_TEST_ACTIVE(irq + i, mask)) {
                res |= (1 << i);
            }
        }
    } else if (offset < 0x800) {
        irq = (offset - 0x400);
        if (irq >= s->num_irq)
            goto bad_reg;
        res = gic_dist_get_priority(s, cpu, irq, attrs);
    } else if (offset < 0xc00) {
        if (s->num_cpu == 1 && s->revision != REV_11MPCORE) {
            res = 0;
        } else {
            irq = (offset - 0x800);
            if (irq >= s->num_irq) {
                goto bad_reg;
            }
            if (irq < 29 && s->revision == REV_11MPCORE) {
                res = 0;
            } else if (irq < GIC_INTERNAL) {
                res = cm;
            } else {
                res = GIC_DIST_TARGET(irq);
            }
        }
    } else if (offset < 0xf00) {
        irq = (offset - 0xc00) * 4;
        if (irq >= s->num_irq)
            goto bad_reg;
        res = 0;
        for (i = 0; i < 4; i++) {
            if (s->security_extn && !attrs.secure &&
                !GIC_DIST_TEST_GROUP(irq + i, 1 << cpu)) {
                continue; /* Ignore Non-secure access of Group0 IRQ */
            }

            if (GIC_DIST_TEST_MODEL(irq + i)) {
                res |= (1 << (i * 2));
            }
            if (GIC_DIST_TEST_EDGE_TRIGGER(irq + i)) {
                res |= (2 << (i * 2));
            }
        }
    } else if (offset < 0xf10) {
        goto bad_reg;
    } else if (offset < 0xf30) {
        if (s->revision == REV_11MPCORE) {
            goto bad_reg;
        }

        if (offset < 0xf20) {
            irq = (offset - 0xf10);
        } else {
            irq = (offset - 0xf20);
        }

        if (s->security_extn && !attrs.secure &&
            !GIC_DIST_TEST_GROUP(irq, 1 << cpu)) {
            res = 0; /* Ignore Non-secure access of Group0 IRQ */
        } else {
            res = s->sgi_pending[irq][cpu];
        }
    } else if (offset < 0xfd0) {
        goto bad_reg;
    } else if (offset < 0x1000) {
        if (offset & 3) {
            res = 0;
        } else {
            switch (s->revision) {
            case REV_11MPCORE:
                res = gic_id_11mpcore[(offset - 0xfd0) >> 2];
                break;
            case 1:
                res = gic_id_gicv1[(offset - 0xfd0) >> 2];
                break;
            case 2:
                res = gic_id_gicv2[(offset - 0xfd0) >> 2];
                break;
            default:
                res = 0;
            }
        }
    } else {
        g_assert_not_reached();
    }
    return res;
bad_reg:
    qemu_log_mask(LOG_GUEST_ERROR,
                  "gic_dist_readb: Bad offset %x\n", (int)offset);
    return 0;
}

static MemTxResult gic_dist_read(void *opaque, hwaddr offset, uint64_t *data,
                                 unsigned size, MemTxAttrs attrs)
{
    switch (size) {
    case 1:
        *data = gic_dist_readb(opaque, offset, attrs);
        break;
    case 2:
        *data = gic_dist_readb(opaque, offset, attrs);
        *data |= gic_dist_readb(opaque, offset + 1, attrs) << 8;
        break;
    case 4:
        *data = gic_dist_readb(opaque, offset, attrs);
        *data |= gic_dist_readb(opaque, offset + 1, attrs) << 8;
        *data |= gic_dist_readb(opaque, offset + 2, attrs) << 16;
        *data |= gic_dist_readb(opaque, offset + 3, attrs) << 24;
        break;
    default:
        return MEMTX_ERROR;
    }

    trace_gic_dist_read(offset, size, *data);
    return MEMTX_OK;
}

static void gic_dist_writeb(void *opaque, hwaddr offset,
                            uint32_t value, MemTxAttrs attrs)
{
    GICState *s = (GICState *)opaque;
    int irq;
    int i;
    int cpu;

    cpu = gic_get_current_cpu(s);
    if (offset < 0x100) {
        if (offset == 0) {
            if (s->security_extn && !attrs.secure) {
                s->ctlr = deposit32(s->ctlr, 1, 1, value);
            } else if (gic_has_groups(s)) {
                s->ctlr = value & (GICD_CTLR_EN_GRP0 | GICD_CTLR_EN_GRP1);
            } else {
                s->ctlr = value & GICD_CTLR_EN_GRP0;
            }
            DPRINTF("Distributor: Group0 %sabled; Group 1 %sabled\n",
                    s->ctlr & GICD_CTLR_EN_GRP0 ? "En" : "Dis",
                    s->ctlr & GICD_CTLR_EN_GRP1 ? "En" : "Dis");
        } else if (offset < 4) {
        } else if (offset >= 0x80) {
            if (!(s->security_extn && !attrs.secure) && gic_has_groups(s)) {
                irq = (offset - 0x80) * 8;
                if (irq >= s->num_irq) {
                    goto bad_reg;
                }
                for (i = 0; i < 8; i++) {
                    int cm = (irq < GIC_INTERNAL) ? (1 << cpu) : ALL_CPU_MASK;
                    if (value & (1 << i)) {
                        GIC_DIST_SET_GROUP(irq + i, cm);
                    } else {
                        GIC_DIST_CLEAR_GROUP(irq + i, cm);
                    }
                }
            }
        } else {
            goto bad_reg;
        }
    } else if (offset < 0x180) {
        irq = (offset - 0x100) * 8;
        if (irq >= s->num_irq)
            goto bad_reg;
        if (irq < GIC_NR_SGIS) {
            value = 0xff;
        }

        for (i = 0; i < 8; i++) {
            if (value & (1 << i)) {
                int mask =
                    (irq < GIC_INTERNAL) ? (1 << cpu)
                                         : GIC_DIST_TARGET(irq + i);
                int cm = (irq < GIC_INTERNAL) ? (1 << cpu) : ALL_CPU_MASK;

                if (s->security_extn && !attrs.secure &&
                    !GIC_DIST_TEST_GROUP(irq + i, 1 << cpu)) {
                    continue; /* Ignore Non-secure access of Group0 IRQ */
                }

                if (!GIC_DIST_TEST_ENABLED(irq + i, cm)) {
                    DPRINTF("Enabled IRQ %d\n", irq + i);
                    trace_gic_enable_irq(irq + i);
                }
                GIC_DIST_SET_ENABLED(irq + i, cm);
                if (s->revision == REV_11MPCORE
                        && GIC_DIST_TEST_LEVEL(irq + i, mask)
                        && !GIC_DIST_TEST_EDGE_TRIGGER(irq + i)) {
                    DPRINTF("Set %d pending mask %x\n", irq + i, mask);
                    GIC_DIST_SET_PENDING(irq + i, mask);
                }
            }
        }
    } else if (offset < 0x200) {
        irq = (offset - 0x180) * 8;
        if (irq >= s->num_irq)
            goto bad_reg;
        if (irq < GIC_NR_SGIS) {
            value = 0;
        }

        for (i = 0; i < 8; i++) {
            if (value & (1 << i)) {
                int cm = (irq < GIC_INTERNAL) ? (1 << cpu) : ALL_CPU_MASK;

                if (s->security_extn && !attrs.secure &&
                    !GIC_DIST_TEST_GROUP(irq + i, 1 << cpu)) {
                    continue; /* Ignore Non-secure access of Group0 IRQ */
                }

                if (GIC_DIST_TEST_ENABLED(irq + i, cm)) {
                    DPRINTF("Disabled IRQ %d\n", irq + i);
                    trace_gic_disable_irq(irq + i);
                }
                GIC_DIST_CLEAR_ENABLED(irq + i, cm);
            }
        }
    } else if (offset < 0x280) {
        irq = (offset - 0x200) * 8;
        if (irq >= s->num_irq)
            goto bad_reg;
        if (irq < GIC_NR_SGIS) {
            value = 0;
        }

        for (i = 0; i < 8; i++) {
            if (value & (1 << i)) {
                int mask = (irq < GIC_INTERNAL) ? (1 << cpu)
                                                : GIC_DIST_TARGET(irq + i);

                if (s->security_extn && !attrs.secure &&
                    !GIC_DIST_TEST_GROUP(irq + i, 1 << cpu)) {
                    continue; /* Ignore Non-secure access of Group0 IRQ */
                }

                GIC_DIST_SET_PENDING(irq + i, mask);
            }
        }
    } else if (offset < 0x300) {
        irq = (offset - 0x280) * 8;
        if (irq >= s->num_irq)
            goto bad_reg;
        if (irq < GIC_NR_SGIS) {
            value = 0;
        }

        for (i = 0; i < 8; i++) {
            if (s->security_extn && !attrs.secure &&
                !GIC_DIST_TEST_GROUP(irq + i, 1 << cpu)) {
                continue; /* Ignore Non-secure access of Group0 IRQ */
            }

            if (value & (1 << i)) {
                GIC_DIST_CLEAR_PENDING(irq + i, ALL_CPU_MASK);
            }
        }
    } else if (offset < 0x380) {
        if (s->revision != 2) {
            goto bad_reg;
        }

        irq = (offset - 0x300) * 8;
        if (irq >= s->num_irq) {
            goto bad_reg;
        }

        int cm = irq < GIC_INTERNAL ? (1 << cpu) : ALL_CPU_MASK;

        for (i = 0; i < 8; i++) {
            if (s->security_extn && !attrs.secure &&
                !GIC_DIST_TEST_GROUP(irq + i, 1 << cpu)) {
                continue; /* Ignore Non-secure access of Group0 IRQ */
            }

            if (value & (1 << i)) {
                GIC_DIST_SET_ACTIVE(irq + i, cm);
            }
        }
    } else if (offset < 0x400) {
        if (s->revision != 2) {
            goto bad_reg;
        }

        irq = (offset - 0x380) * 8;
        if (irq >= s->num_irq) {
            goto bad_reg;
        }

        int cm = irq < GIC_INTERNAL ? (1 << cpu) : ALL_CPU_MASK;

        for (i = 0; i < 8; i++) {
            if (s->security_extn && !attrs.secure &&
                !GIC_DIST_TEST_GROUP(irq + i, 1 << cpu)) {
                continue; /* Ignore Non-secure access of Group0 IRQ */
            }

            if (value & (1 << i)) {
                GIC_DIST_CLEAR_ACTIVE(irq + i, cm);
            }
        }
    } else if (offset < 0x800) {
        irq = (offset - 0x400);
        if (irq >= s->num_irq)
            goto bad_reg;
        gic_dist_set_priority(s, cpu, irq, value, attrs);
    } else if (offset < 0xc00) {
        if (s->num_cpu != 1 || s->revision == REV_11MPCORE) {
            irq = (offset - 0x800);
            if (irq >= s->num_irq) {
                goto bad_reg;
            }
            if (irq < 29 && s->revision == REV_11MPCORE) {
                value = 0;
            } else if (irq < GIC_INTERNAL) {
                value = ALL_CPU_MASK;
            }
            s->irq_target[irq] = value & ALL_CPU_MASK;
            if (irq >= GIC_INTERNAL && s->irq_state[irq].pending) {
                s->irq_state[irq].pending = value & ALL_CPU_MASK;
            }
        }
    } else if (offset < 0xf00) {
        irq = (offset - 0xc00) * 4;
        if (irq >= s->num_irq)
            goto bad_reg;
        if (irq < GIC_NR_SGIS)
            value |= 0xaa;
        for (i = 0; i < 4; i++) {
            if (s->security_extn && !attrs.secure &&
                !GIC_DIST_TEST_GROUP(irq + i, 1 << cpu)) {
                continue; /* Ignore Non-secure access of Group0 IRQ */
            }

            if (s->revision == REV_11MPCORE) {
                if (value & (1 << (i * 2))) {
                    GIC_DIST_SET_MODEL(irq + i);
                } else {
                    GIC_DIST_CLEAR_MODEL(irq + i);
                }
            }
            if (value & (2 << (i * 2))) {
                GIC_DIST_SET_EDGE_TRIGGER(irq + i);
            } else {
                GIC_DIST_CLEAR_EDGE_TRIGGER(irq + i);
            }
        }
    } else if (offset < 0xf10) {
        goto bad_reg;
    } else if (offset < 0xf20) {
        if (s->revision == REV_11MPCORE) {
            goto bad_reg;
        }
        irq = (offset - 0xf10);

        if (!s->security_extn || attrs.secure ||
            GIC_DIST_TEST_GROUP(irq, 1 << cpu)) {
            s->sgi_pending[irq][cpu] &= ~value;
            if (s->sgi_pending[irq][cpu] == 0) {
                GIC_DIST_CLEAR_PENDING(irq, 1 << cpu);
            }
        }
    } else if (offset < 0xf30) {
        if (s->revision == REV_11MPCORE) {
            goto bad_reg;
        }
        irq = (offset - 0xf20);

        if (!s->security_extn || attrs.secure ||
            GIC_DIST_TEST_GROUP(irq, 1 << cpu)) {
            GIC_DIST_SET_PENDING(irq, 1 << cpu);
            s->sgi_pending[irq][cpu] |= value;
        }
    } else {
        goto bad_reg;
    }
    gic_update(s);
    return;
bad_reg:
    qemu_log_mask(LOG_GUEST_ERROR,
                  "gic_dist_writeb: Bad offset %x\n", (int)offset);
}

static void gic_dist_writew(void *opaque, hwaddr offset,
                            uint32_t value, MemTxAttrs attrs)
{
    gic_dist_writeb(opaque, offset, value & 0xff, attrs);
    gic_dist_writeb(opaque, offset + 1, value >> 8, attrs);
}

static void gic_dist_writel(void *opaque, hwaddr offset,
                            uint32_t value, MemTxAttrs attrs)
{
    GICState *s = (GICState *)opaque;
    if (offset == 0xf00) {
        int cpu;
        int irq;
        int mask;
        int target_cpu;

        cpu = gic_get_current_cpu(s);
        irq = value & 0xf;
        switch ((value >> 24) & 3) {
        case 0:
            mask = (value >> 16) & ALL_CPU_MASK;
            break;
        case 1:
            mask = ALL_CPU_MASK ^ (1 << cpu);
            break;
        case 2:
            mask = 1 << cpu;
            break;
        default:
            DPRINTF("Bad Soft Int target filter\n");
            mask = ALL_CPU_MASK;
            break;
        }
        GIC_DIST_SET_PENDING(irq, mask);
        target_cpu = ctz32(mask);
        while (target_cpu < GIC_NCPU) {
            s->sgi_pending[irq][target_cpu] |= (1 << cpu);
            mask &= ~(1 << target_cpu);
            target_cpu = ctz32(mask);
        }
        gic_update(s);
        return;
    }
    gic_dist_writew(opaque, offset, value & 0xffff, attrs);
    gic_dist_writew(opaque, offset + 2, value >> 16, attrs);
}

static MemTxResult gic_dist_write(void *opaque, hwaddr offset, uint64_t data,
                                  unsigned size, MemTxAttrs attrs)
{
    trace_gic_dist_write(offset, size, data);

    switch (size) {
    case 1:
        gic_dist_writeb(opaque, offset, data, attrs);
        return MEMTX_OK;
    case 2:
        gic_dist_writew(opaque, offset, data, attrs);
        return MEMTX_OK;
    case 4:
        gic_dist_writel(opaque, offset, data, attrs);
        return MEMTX_OK;
    default:
        return MEMTX_ERROR;
    }
}

static inline uint32_t gic_apr_ns_view(GICState *s, int cpu, int regno)
{
    switch (GIC_MIN_BPR) {
    case 0:
        if (regno < 2) {
            return s->nsapr[regno + 2][cpu];
        }
        break;
    case 1:
        if (regno == 0) {
            return s->nsapr[regno + 1][cpu];
        }
        break;
    case 2:
        if (regno == 0) {
            return extract32(s->nsapr[0][cpu], 16, 16);
        }
        break;
    case 3:
        if (regno == 0) {
            return extract32(s->nsapr[0][cpu], 8, 8);
        }
        break;
    default:
        g_assert_not_reached();
    }
    return 0;
}

static inline void gic_apr_write_ns_view(GICState *s, int cpu, int regno,
                                         uint32_t value)
{
    switch (GIC_MIN_BPR) {
    case 0:
        if (regno < 2) {
            s->nsapr[regno + 2][cpu] = value;
        }
        break;
    case 1:
        if (regno == 0) {
            s->nsapr[regno + 1][cpu] = value;
        }
        break;
    case 2:
        if (regno == 0) {
            s->nsapr[0][cpu] = deposit32(s->nsapr[0][cpu], 16, 16, value);
        }
        break;
    case 3:
        if (regno == 0) {
            s->nsapr[0][cpu] = deposit32(s->nsapr[0][cpu], 8, 8, value);
        }
        break;
    default:
        g_assert_not_reached();
    }
}

static MemTxResult gic_cpu_read(GICState *s, int cpu, int offset,
                                uint64_t *data, MemTxAttrs attrs)
{
    switch (offset) {
    case 0x00: /* Control */
        *data = gic_get_cpu_control(s, cpu, attrs);
        break;
    case 0x04: /* Priority mask */
        *data = gic_get_priority_mask(s, cpu, attrs);
        break;
    case 0x08: /* Binary Point */
        if (gic_cpu_ns_access(s, cpu, attrs)) {
            if (s->cpu_ctlr[cpu] & GICC_CTLR_CBPR) {
                *data = MIN(s->bpr[cpu] + 1, 7);
            } else {
                *data = s->abpr[cpu];
            }
        } else {
            *data = s->bpr[cpu];
        }
        break;
    case 0x0c: /* Acknowledge */
        *data = gic_acknowledge_irq(s, cpu, attrs);
        break;
    case 0x14: /* Running Priority */
        *data = gic_get_running_priority(s, cpu, attrs);
        break;
    case 0x18: /* Highest Pending Interrupt */
        *data = gic_get_current_pending_irq(s, cpu, attrs);
        break;
    case 0x1c: /* Aliased Binary Point */
        if (!gic_has_groups(s) || (gic_cpu_ns_access(s, cpu, attrs))) {
            *data = 0;
        } else {
            *data = s->abpr[cpu];
        }
        break;
    case 0xd0: case 0xd4: case 0xd8: case 0xdc:
    {
        int regno = (offset - 0xd0) / 4;
        int nr_aprs = gic_is_vcpu(cpu) ? GIC_VIRT_NR_APRS : GIC_NR_APRS;

        if (regno >= nr_aprs || s->revision != 2) {
            *data = 0;
        } else if (gic_is_vcpu(cpu)) {
            *data = s->h_apr[gic_get_vcpu_real_id(cpu)];
        } else if (gic_cpu_ns_access(s, cpu, attrs)) {
            *data = gic_apr_ns_view(s, cpu, regno);
        } else {
            *data = s->apr[regno][cpu];
        }
        break;
    }
    case 0xe0: case 0xe4: case 0xe8: case 0xec:
    {
        int regno = (offset - 0xe0) / 4;

        if (regno >= GIC_NR_APRS || s->revision != 2 || !gic_has_groups(s) ||
            gic_cpu_ns_access(s, cpu, attrs) || gic_is_vcpu(cpu)) {
            *data = 0;
        } else {
            *data = s->nsapr[regno][cpu];
        }
        break;
    }
    case 0xfc:
        if (s->revision == REV_11MPCORE) {
            *data = 0;
        } else {
            *data = (s->revision << 16) | 0x43b;
        }
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "gic_cpu_read: Bad offset %x\n", (int)offset);
        *data = 0;
        break;
    }

    trace_gic_cpu_read(gic_is_vcpu(cpu) ? "vcpu" : "cpu",
                       gic_get_vcpu_real_id(cpu), offset, *data);
    return MEMTX_OK;
}

static MemTxResult gic_cpu_write(GICState *s, int cpu, int offset,
                                 uint32_t value, MemTxAttrs attrs)
{
    trace_gic_cpu_write(gic_is_vcpu(cpu) ? "vcpu" : "cpu",
                        gic_get_vcpu_real_id(cpu), offset, value);

    switch (offset) {
    case 0x00: /* Control */
        gic_set_cpu_control(s, cpu, value, attrs);
        break;
    case 0x04: /* Priority mask */
        gic_set_priority_mask(s, cpu, value, attrs);
        break;
    case 0x08: /* Binary Point */
        if (gic_cpu_ns_access(s, cpu, attrs)) {
            if (s->cpu_ctlr[cpu] & GICC_CTLR_CBPR) {
                return MEMTX_OK;
            } else {
                s->abpr[cpu] = MAX(value & 0x7, GIC_MIN_ABPR);
            }
        } else {
            int min_bpr = gic_is_vcpu(cpu) ? GIC_VIRT_MIN_BPR : GIC_MIN_BPR;
            s->bpr[cpu] = MAX(value & 0x7, min_bpr);
        }
        break;
    case 0x10: /* End Of Interrupt */
        gic_complete_irq(s, cpu, value & 0x3ff, attrs);
        return MEMTX_OK;
    case 0x1c: /* Aliased Binary Point */
        if (!gic_has_groups(s) || (gic_cpu_ns_access(s, cpu, attrs))) {
            return MEMTX_OK;
        } else {
            s->abpr[cpu] = MAX(value & 0x7, GIC_MIN_ABPR);
        }
        break;
    case 0xd0: case 0xd4: case 0xd8: case 0xdc:
    {
        int regno = (offset - 0xd0) / 4;
        int nr_aprs = gic_is_vcpu(cpu) ? GIC_VIRT_NR_APRS : GIC_NR_APRS;

        if (regno >= nr_aprs || s->revision != 2) {
            return MEMTX_OK;
        }
        if (gic_is_vcpu(cpu)) {
            s->h_apr[gic_get_vcpu_real_id(cpu)] = value;
        } else if (gic_cpu_ns_access(s, cpu, attrs)) {
            gic_apr_write_ns_view(s, cpu, regno, value);
        } else {
            s->apr[regno][cpu] = value;
        }
        s->running_priority[cpu] = gic_get_prio_from_apr_bits(s, cpu);
        break;
    }
    case 0xe0: case 0xe4: case 0xe8: case 0xec:
    {
        int regno = (offset - 0xe0) / 4;

        if (regno >= GIC_NR_APRS || s->revision != 2) {
            return MEMTX_OK;
        }
        if (gic_is_vcpu(cpu)) {
            return MEMTX_OK;
        }
        if (!gic_has_groups(s) || (gic_cpu_ns_access(s, cpu, attrs))) {
            return MEMTX_OK;
        }
        s->nsapr[regno][cpu] = value;
        s->running_priority[cpu] = gic_get_prio_from_apr_bits(s, cpu);
        break;
    }
    case 0x1000:
        gic_deactivate_irq(s, cpu, value & 0x3ff, attrs);
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "gic_cpu_write: Bad offset %x\n", (int)offset);
        return MEMTX_OK;
    }

    if (gic_is_vcpu(cpu)) {
        gic_update_virt(s);
    } else {
        gic_update(s);
    }

    return MEMTX_OK;
}

static MemTxResult gic_thiscpu_read(void *opaque, hwaddr addr, uint64_t *data,
                                    unsigned size, MemTxAttrs attrs)
{
    GICState *s = (GICState *)opaque;
    return gic_cpu_read(s, gic_get_current_cpu(s), addr, data, attrs);
}

static MemTxResult gic_thiscpu_write(void *opaque, hwaddr addr,
                                     uint64_t value, unsigned size,
                                     MemTxAttrs attrs)
{
    GICState *s = (GICState *)opaque;
    return gic_cpu_write(s, gic_get_current_cpu(s), addr, value, attrs);
}

static MemTxResult gic_do_cpu_read(void *opaque, hwaddr addr, uint64_t *data,
                                   unsigned size, MemTxAttrs attrs)
{
    GICState **backref = (GICState **)opaque;
    GICState *s = *backref;
    int id = (backref - s->backref);
    return gic_cpu_read(s, id, addr, data, attrs);
}

static MemTxResult gic_do_cpu_write(void *opaque, hwaddr addr,
                                    uint64_t value, unsigned size,
                                    MemTxAttrs attrs)
{
    GICState **backref = (GICState **)opaque;
    GICState *s = *backref;
    int id = (backref - s->backref);
    return gic_cpu_write(s, id, addr, value, attrs);
}

static MemTxResult gic_thisvcpu_read(void *opaque, hwaddr addr, uint64_t *data,
                                    unsigned size, MemTxAttrs attrs)
{
    GICState *s = (GICState *)opaque;

    return gic_cpu_read(s, gic_get_current_vcpu(s), addr, data, attrs);
}

static MemTxResult gic_thisvcpu_write(void *opaque, hwaddr addr,
                                     uint64_t value, unsigned size,
                                     MemTxAttrs attrs)
{
    GICState *s = (GICState *)opaque;

    return gic_cpu_write(s, gic_get_current_vcpu(s), addr, value, attrs);
}

static uint32_t gic_compute_eisr(GICState *s, int cpu, int lr_start)
{
    int lr_idx;
    uint32_t ret = 0;

    for (lr_idx = lr_start; lr_idx < s->num_lrs; lr_idx++) {
        uint32_t *entry = &s->h_lr[lr_idx][cpu];
        ret = deposit32(ret, lr_idx - lr_start, 1,
                        gic_lr_entry_is_eoi(*entry));
    }

    return ret;
}

static uint32_t gic_compute_elrsr(GICState *s, int cpu, int lr_start)
{
    int lr_idx;
    uint32_t ret = 0;

    for (lr_idx = lr_start; lr_idx < s->num_lrs; lr_idx++) {
        uint32_t *entry = &s->h_lr[lr_idx][cpu];
        ret = deposit32(ret, lr_idx - lr_start, 1,
                        gic_lr_entry_is_free(*entry));
    }

    return ret;
}

static void gic_vmcr_write(GICState *s, uint32_t value, MemTxAttrs attrs)
{
    int vcpu = gic_get_current_vcpu(s);
    uint32_t ctlr;
    uint32_t abpr;
    uint32_t bpr;
    uint32_t prio_mask;

    ctlr = FIELD_EX32(value, GICH_VMCR, VMCCtlr);
    abpr = FIELD_EX32(value, GICH_VMCR, VMABP);
    bpr = FIELD_EX32(value, GICH_VMCR, VMBP);
    prio_mask = FIELD_EX32(value, GICH_VMCR, VMPriMask) << 3;

    gic_set_cpu_control(s, vcpu, ctlr, attrs);
    s->abpr[vcpu] = MAX(abpr, GIC_VIRT_MIN_ABPR);
    s->bpr[vcpu] = MAX(bpr, GIC_VIRT_MIN_BPR);
    gic_set_priority_mask(s, vcpu, prio_mask, attrs);
}

static MemTxResult gic_hyp_read(void *opaque, int cpu, hwaddr addr,
                                uint64_t *data, MemTxAttrs attrs)
{
    GICState *s = ARM_GIC(opaque);
    int vcpu = cpu + GIC_NCPU;

    switch (addr) {
    case A_GICH_HCR: /* Hypervisor Control */
        *data = s->h_hcr[cpu];
        break;

    case A_GICH_VTR: /* VGIC Type */
        *data = FIELD_DP32(0, GICH_VTR, ListRegs, s->num_lrs - 1);
        *data = FIELD_DP32(*data, GICH_VTR, PREbits,
                           GIC_VIRT_MAX_GROUP_PRIO_BITS - 1);
        *data = FIELD_DP32(*data, GICH_VTR, PRIbits,
                           (7 - GIC_VIRT_MIN_BPR) - 1);
        break;

    case A_GICH_VMCR: /* Virtual Machine Control */
        *data = FIELD_DP32(0, GICH_VMCR, VMCCtlr,
                           extract32(s->cpu_ctlr[vcpu], 0, 10));
        *data = FIELD_DP32(*data, GICH_VMCR, VMABP, s->abpr[vcpu]);
        *data = FIELD_DP32(*data, GICH_VMCR, VMBP, s->bpr[vcpu]);
        *data = FIELD_DP32(*data, GICH_VMCR, VMPriMask,
                           extract32(s->priority_mask[vcpu], 3, 5));
        break;

    case A_GICH_MISR: /* Maintenance Interrupt Status */
        *data = s->h_misr[cpu];
        break;

    case A_GICH_EISR0: /* End of Interrupt Status 0 and 1 */
    case A_GICH_EISR1:
        *data = gic_compute_eisr(s, cpu, (addr - A_GICH_EISR0) * 8);
        break;

    case A_GICH_ELRSR0: /* Empty List Status 0 and 1 */
    case A_GICH_ELRSR1:
        *data = gic_compute_elrsr(s, cpu, (addr - A_GICH_ELRSR0) * 8);
        break;

    case A_GICH_APR: /* Active Priorities */
        *data = s->h_apr[cpu];
        break;

    case A_GICH_LR0 ... A_GICH_LR63: /* List Registers */
    {
        int lr_idx = (addr - A_GICH_LR0) / 4;

        if (lr_idx > s->num_lrs) {
            *data = 0;
        } else {
            *data = s->h_lr[lr_idx][cpu];
        }
        break;
    }

    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "gic_hyp_read: Bad offset %" HWADDR_PRIx "\n", addr);
        return MEMTX_OK;
    }

    trace_gic_hyp_read(addr, *data);
    return MEMTX_OK;
}

static MemTxResult gic_hyp_write(void *opaque, int cpu, hwaddr addr,
                                 uint64_t value, MemTxAttrs attrs)
{
    GICState *s = ARM_GIC(opaque);
    int vcpu = cpu + GIC_NCPU;

    trace_gic_hyp_write(addr, value);

    switch (addr) {
    case A_GICH_HCR: /* Hypervisor Control */
        s->h_hcr[cpu] = value & GICH_HCR_MASK;
        break;

    case A_GICH_VMCR: /* Virtual Machine Control */
        gic_vmcr_write(s, value, attrs);
        break;

    case A_GICH_APR: /* Active Priorities */
        s->h_apr[cpu] = value;
        s->running_priority[vcpu] = gic_get_prio_from_apr_bits(s, vcpu);
        break;

    case A_GICH_LR0 ... A_GICH_LR63: /* List Registers */
    {
        int lr_idx = (addr - A_GICH_LR0) / 4;

        if (lr_idx > s->num_lrs) {
            return MEMTX_OK;
        }

        s->h_lr[lr_idx][cpu] = value & GICH_LR_MASK;
        trace_gic_lr_entry(cpu, lr_idx, s->h_lr[lr_idx][cpu]);
        break;
    }

    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "gic_hyp_write: Bad offset %" HWADDR_PRIx "\n", addr);
        return MEMTX_OK;
    }

    gic_update_virt(s);
    return MEMTX_OK;
}

static MemTxResult gic_thiscpu_hyp_read(void *opaque, hwaddr addr, uint64_t *data,
                                    unsigned size, MemTxAttrs attrs)
{
    GICState *s = (GICState *)opaque;

    return gic_hyp_read(s, gic_get_current_cpu(s), addr, data, attrs);
}

static MemTxResult gic_thiscpu_hyp_write(void *opaque, hwaddr addr,
                                     uint64_t value, unsigned size,
                                     MemTxAttrs attrs)
{
    GICState *s = (GICState *)opaque;

    return gic_hyp_write(s, gic_get_current_cpu(s), addr, value, attrs);
}

static MemTxResult gic_do_hyp_read(void *opaque, hwaddr addr, uint64_t *data,
                                    unsigned size, MemTxAttrs attrs)
{
    GICState **backref = (GICState **)opaque;
    GICState *s = *backref;
    int id = (backref - s->backref);

    return gic_hyp_read(s, id, addr, data, attrs);
}

static MemTxResult gic_do_hyp_write(void *opaque, hwaddr addr,
                                     uint64_t value, unsigned size,
                                     MemTxAttrs attrs)
{
    GICState **backref = (GICState **)opaque;
    GICState *s = *backref;
    int id = (backref - s->backref);

    return gic_hyp_write(s, id + GIC_NCPU, addr, value, attrs);

}

static const MemoryRegionOps gic_ops[2] = {
    {
        .read_with_attrs = gic_dist_read,
        .write_with_attrs = gic_dist_write,
        .endianness = DEVICE_NATIVE_ENDIAN,
    },
    {
        .read_with_attrs = gic_thiscpu_read,
        .write_with_attrs = gic_thiscpu_write,
        .endianness = DEVICE_NATIVE_ENDIAN,
    }
};

static const MemoryRegionOps gic_cpu_ops = {
    .read_with_attrs = gic_do_cpu_read,
    .write_with_attrs = gic_do_cpu_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static const MemoryRegionOps gic_virt_ops[2] = {
    {
        .read_with_attrs = gic_thiscpu_hyp_read,
        .write_with_attrs = gic_thiscpu_hyp_write,
        .endianness = DEVICE_NATIVE_ENDIAN,
    },
    {
        .read_with_attrs = gic_thisvcpu_read,
        .write_with_attrs = gic_thisvcpu_write,
        .endianness = DEVICE_NATIVE_ENDIAN,
    }
};

static const MemoryRegionOps gic_viface_ops = {
    .read_with_attrs = gic_do_hyp_read,
    .write_with_attrs = gic_do_hyp_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void arm_gic_realize(DeviceState *dev, Error **errp)
{
    int i;
    GICState *s = ARM_GIC(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    ARMGICClass *agc = ARM_GIC_GET_CLASS(s);
    Error *local_err = NULL;

    agc->parent_realize(dev, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        return;
    }

    if (s->n_prio_bits > GIC_MAX_PRIORITY_BITS ||
       (s->virt_extn ? s->n_prio_bits < GIC_VIRT_MAX_GROUP_PRIO_BITS :
        s->n_prio_bits < GIC_MIN_PRIORITY_BITS)) {
        error_setg(errp, "num-priority-bits cannot be greater than %d"
                   " or less than %d", GIC_MAX_PRIORITY_BITS,
                   s->virt_extn ? GIC_VIRT_MAX_GROUP_PRIO_BITS :
                   GIC_MIN_PRIORITY_BITS);
        return;
    }

    gic_init_irqs_and_mmio(s, gic_set_irq, gic_ops, gic_virt_ops);

    for (i = 0; i < s->num_cpu; i++) {
        s->backref[i] = s;
        memory_region_init_io(&s->cpuiomem[i+1], OBJECT(s), &gic_cpu_ops,
                              &s->backref[i], "gic_cpu", 0x100);
        sysbus_init_mmio(sbd, &s->cpuiomem[i+1]);
    }

    if (s->virt_extn) {
        for (i = 0; i < s->num_cpu; i++) {
            memory_region_init_io(&s->vifaceiomem[i + 1], OBJECT(s),
                                  &gic_viface_ops, &s->backref[i],
                                  "gic_viface", 0x200);
            sysbus_init_mmio(sbd, &s->vifaceiomem[i + 1]);
        }
    }

}

static void arm_gic_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ARMGICClass *agc = ARM_GIC_CLASS(klass);

    device_class_set_parent_realize(dc, arm_gic_realize, &agc->parent_realize);
}

static const TypeInfo arm_gic_info = {
    .name = TYPE_ARM_GIC,
    .parent = TYPE_ARM_GIC_COMMON,
    .instance_size = sizeof(GICState),
    .class_init = arm_gic_class_init,
    .class_size = sizeof(ARMGICClass),
};

static void arm_gic_register_types(void)
{
    type_register_static(&arm_gic_info);
}

type_init(arm_gic_register_types)
