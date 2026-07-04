

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "hw/intc/arm_gicv3.h"
#include "gicv3_internal.h"

static bool irqbetter(GICv3CPUState *cs, int irq, uint8_t prio, bool nmi)
{
    if (prio != cs->hppi.prio) {
        return prio < cs->hppi.prio;
    }

    if (nmi != cs->hppi.nmi) {
        return nmi;
    }

    if (irq <= cs->hppi.irq) {
        return true;
    }
    return false;
}

static uint32_t gicd_int_pending(GICv3State *s, int irq)
{
    uint32_t pend, grpmask;
    uint32_t pending = *gic_bmp_ptr32(s->pending, irq);
    uint32_t edge_trigger = *gic_bmp_ptr32(s->edge_trigger, irq);
    uint32_t level = *gic_bmp_ptr32(s->level, irq);
    uint32_t group = *gic_bmp_ptr32(s->group, irq);
    uint32_t grpmod = *gic_bmp_ptr32(s->grpmod, irq);
    uint32_t enable = *gic_bmp_ptr32(s->enabled, irq);
    uint32_t active = *gic_bmp_ptr32(s->active, irq);

    pend = pending | (~edge_trigger & level);
    pend &= enable;
    pend &= ~active;

    if (s->gicd_ctlr & GICD_CTLR_DS) {
        grpmod = 0;
    }

    grpmask = 0;
    if (s->gicd_ctlr & GICD_CTLR_EN_GRP1NS) {
        grpmask |= group;
    }
    if (s->gicd_ctlr & GICD_CTLR_EN_GRP1S) {
        grpmask |= (~group & grpmod);
    }
    if (s->gicd_ctlr & GICD_CTLR_EN_GRP0) {
        grpmask |= (~group & ~grpmod);
    }
    pend &= grpmask;

    return pend;
}

static uint32_t gicr_int_pending(GICv3CPUState *cs)
{
    uint32_t pend, grpmask, grpmod;

    pend = cs->gicr_ipendr0 | (~cs->edge_trigger & cs->level);
    pend &= cs->gicr_ienabler0;
    pend &= ~cs->gicr_iactiver0;

    if (cs->gic->gicd_ctlr & GICD_CTLR_DS) {
        grpmod = 0;
    } else {
        grpmod = cs->gicr_igrpmodr0;
    }

    grpmask = 0;
    if (cs->gic->gicd_ctlr & GICD_CTLR_EN_GRP1NS) {
        grpmask |= cs->gicr_igroupr0;
    }
    if (cs->gic->gicd_ctlr & GICD_CTLR_EN_GRP1S) {
        grpmask |= (~cs->gicr_igroupr0 & grpmod);
    }
    if (cs->gic->gicd_ctlr & GICD_CTLR_EN_GRP0) {
        grpmask |= (~cs->gicr_igroupr0 & ~grpmod);
    }
    pend &= grpmask;

    return pend;
}

static bool gicv3_get_priority(GICv3CPUState *cs, bool is_redist, int irq,
                               uint8_t *prio)
{
    uint32_t nmi = 0x0;

    if (is_redist) {
        nmi = extract32(cs->gicr_inmir0, irq, 1);
    } else {
        nmi = *gic_bmp_ptr32(cs->gic->nmi, irq);
        nmi = nmi & (1 << (irq & 0x1f));
    }

    if (nmi) {
        if (!(cs->gic->gicd_ctlr & GICD_CTLR_DS) &&
            ((is_redist && extract32(cs->gicr_igroupr0, irq, 1)) ||
             (!is_redist && gicv3_gicd_group_test(cs->gic, irq)))) {
            *prio = 0x80;
        } else {
            *prio = 0x0;
        }

        return true;
    }

    if (is_redist) {
        *prio = cs->gicr_ipriorityr[irq];
    } else {
        *prio = cs->gic->gicd_ipriority[irq];
    }

    return false;
}

static void gicv3_redist_update_noirqset(GICv3CPUState *cs)
{
    bool seenbetter = false;
    uint8_t prio;
    int i;
    uint32_t pend;
    bool nmi = false;

    pend = gicr_int_pending(cs);

    if (pend) {
        for (i = 0; i < GIC_INTERNAL; i++) {
            if (!(pend & (1 << i))) {
                continue;
            }
            nmi = gicv3_get_priority(cs, true, i, &prio);
            if (irqbetter(cs, i, prio, nmi)) {
                cs->hppi.irq = i;
                cs->hppi.prio = prio;
                cs->hppi.nmi = nmi;
                seenbetter = true;
            }
        }
    }

    if (seenbetter) {
        cs->hppi.grp = gicv3_irq_group(cs->gic, cs, cs->hppi.irq);
    }

    if ((cs->gicr_ctlr & GICR_CTLR_ENABLE_LPIS) && cs->gic->lpi_enable &&
        (cs->gic->gicd_ctlr & GICD_CTLR_EN_GRP1NS) &&
        (cs->hpplpi.prio != 0xff)) {
        if (irqbetter(cs, cs->hpplpi.irq, cs->hpplpi.prio, cs->hpplpi.nmi)) {
            cs->hppi.irq = cs->hpplpi.irq;
            cs->hppi.prio = cs->hpplpi.prio;
            cs->hppi.nmi = cs->hpplpi.nmi;
            cs->hppi.grp = cs->hpplpi.grp;
            seenbetter = true;
        }
    }

    if (!seenbetter && cs->hppi.prio != 0xff &&
        (cs->hppi.irq < GIC_INTERNAL ||
         cs->hppi.irq >= GICV3_LPI_INTID_START)) {
        gicv3_full_update_noirqset(cs->gic);
    }
}

void gicv3_redist_update(GICv3CPUState *cs)
{
    gicv3_redist_update_noirqset(cs);
    gicv3_cpuif_update(cs);
}

static void gicv3_update_noirqset(GICv3State *s, int start, int len)
{
    int i;
    uint8_t prio;
    uint32_t pend = 0;
    bool nmi = false;

    assert(start >= GIC_INTERNAL);
    assert(len > 0);

    for (i = 0; i < s->num_cpu; i++) {
        s->cpu[i].seenbetter = false;
    }

    for (i = start; i < start + len; i++) {
        GICv3CPUState *cs;

        if (i == start || (i & 0x1f) == 0) {
            pend = gicd_int_pending(s, i & ~0x1f);
        }

        if (!(pend & (1 << (i & 0x1f)))) {
            continue;
        }
        cs = s->gicd_irouter_target[i];
        if (!cs) {
            continue;
        }
        nmi = gicv3_get_priority(cs, false, i, &prio);
        if (irqbetter(cs, i, prio, nmi)) {
            cs->hppi.irq = i;
            cs->hppi.prio = prio;
            cs->hppi.nmi = nmi;
            cs->seenbetter = true;
        }
    }

    for (i = 0; i < s->num_cpu; i++) {
        GICv3CPUState *cs = &s->cpu[i];

        if (cs->seenbetter) {
            cs->hppi.grp = gicv3_irq_group(cs->gic, cs, cs->hppi.irq);
        }

        if (!cs->seenbetter && cs->hppi.prio != 0xff &&
            cs->hppi.irq >= start && cs->hppi.irq < start + len) {
            gicv3_full_update_noirqset(s);
            break;
        }
    }
}

void gicv3_update(GICv3State *s, int start, int len)
{
    int i;

    gicv3_update_noirqset(s, start, len);
    for (i = 0; i < s->num_cpu; i++) {
        gicv3_cpuif_update(&s->cpu[i]);
    }
}

void gicv3_full_update_noirqset(GICv3State *s)
{
    int i;

    for (i = 0; i < s->num_cpu; i++) {
        s->cpu[i].hppi.prio = 0xff;
        s->cpu[i].hppi.nmi = false;
    }

    gicv3_update_noirqset(s, GIC_INTERNAL, s->num_irq - GIC_INTERNAL);

    for (i = 0; i < s->num_cpu; i++) {
        gicv3_redist_update_noirqset(&s->cpu[i]);
    }
}

void gicv3_full_update(GICv3State *s)
{
    int i;

    gicv3_full_update_noirqset(s);
    for (i = 0; i < s->num_cpu; i++) {
        gicv3_cpuif_update(&s->cpu[i]);
    }
}

static void gicv3_set_irq(void *opaque, int irq, int level)
{
    GICv3State *s = opaque;

    if (irq < (s->num_irq - GIC_INTERNAL)) {
        gicv3_dist_set_irq(s, irq + GIC_INTERNAL, level);
    } else {
        int cpu;

        irq -= (s->num_irq - GIC_INTERNAL);
        cpu = irq / GIC_INTERNAL;
        irq %= GIC_INTERNAL;
        assert(cpu < s->num_cpu);
        assert(irq >= GIC_NR_SGIS);
        gicv3_redist_set_irq(&s->cpu[cpu], irq, level);
    }
}

static void arm_gicv3_post_load(GICv3State *s)
{
    int i;
    for (i = 0; i < s->num_cpu; i++) {
        gicv3_redist_update_lpi_only(&s->cpu[i]);
    }
    gicv3_full_update_noirqset(s);
    gicv3_cache_all_target_cpustates(s);
}

static const MemoryRegionOps gic_ops[] = {
    {
        .read_with_attrs = gicv3_dist_read,
        .write_with_attrs = gicv3_dist_write,
        .endianness = DEVICE_NATIVE_ENDIAN,
        .valid.min_access_size = 1,
        .valid.max_access_size = 8,
        .impl.min_access_size = 1,
        .impl.max_access_size = 8,
    },
    {
        .read_with_attrs = gicv3_redist_read,
        .write_with_attrs = gicv3_redist_write,
        .endianness = DEVICE_NATIVE_ENDIAN,
        .valid.min_access_size = 1,
        .valid.max_access_size = 8,
        .impl.min_access_size = 1,
        .impl.max_access_size = 8,
    }
};

static void arm_gic_realize(DeviceState *dev, Error **errp)
{
    GICv3State *s = ARM_GICV3(dev);
    ARMGICv3Class *agc = ARM_GICV3_GET_CLASS(s);
    Error *local_err = NULL;

    agc->parent_realize(dev, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        return;
    }

    gicv3_init_irqs_and_mmio(s, gicv3_set_irq, gic_ops);

    gicv3_init_cpuif(s);
}

static void arm_gicv3_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ARMGICv3CommonClass *agcc = ARM_GICV3_COMMON_CLASS(klass);
    ARMGICv3Class *agc = ARM_GICV3_CLASS(klass);

    agcc->post_load = arm_gicv3_post_load;
    device_class_set_parent_realize(dc, arm_gic_realize, &agc->parent_realize);
}

static const TypeInfo arm_gicv3_info = {
    .name = TYPE_ARM_GICV3,
    .parent = TYPE_ARM_GICV3_COMMON,
    .instance_size = sizeof(GICv3State),
    .class_init = arm_gicv3_class_init,
    .class_size = sizeof(ARMGICv3Class),
};

static void arm_gicv3_register_types(void)
{
    type_register_static(&arm_gicv3_info);
}

type_init(arm_gicv3_register_types)
