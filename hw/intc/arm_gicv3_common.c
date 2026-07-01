

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "qemu/error-report.h"
#include "hw/core/cpu.h"
#include "hw/intc/arm_gicv3_common.h"
#include "hw/qdev-properties.h"
#include "state/vmstate.h"
#include "gicv3_internal.h"
#include "hw/arm/linux-boot-if.h"
#include "system/gunyah.h"


static void gicv3_gicd_state_shift_bug_post_load(GICv3State *cs)
{
    if (cs->gicd_state_shift_bug) {
        return;
    }

    
    memmove(cs->group, (uint8_t *)cs->group + GIC_INTERNAL / 8,
            sizeof(cs->group) - GIC_INTERNAL / 8);
    memmove(cs->grpmod, (uint8_t *)cs->grpmod + GIC_INTERNAL / 8,
            sizeof(cs->grpmod) - GIC_INTERNAL / 8);
    memmove(cs->enabled, (uint8_t *)cs->enabled + GIC_INTERNAL / 8,
            sizeof(cs->enabled) - GIC_INTERNAL / 8);
    memmove(cs->pending, (uint8_t *)cs->pending + GIC_INTERNAL / 8,
            sizeof(cs->pending) - GIC_INTERNAL / 8);
    memmove(cs->active, (uint8_t *)cs->active + GIC_INTERNAL / 8,
            sizeof(cs->active) - GIC_INTERNAL / 8);
    memmove(cs->edge_trigger, (uint8_t *)cs->edge_trigger + GIC_INTERNAL / 8,
            sizeof(cs->edge_trigger) - GIC_INTERNAL / 8);

    
    cs->gicd_state_shift_bug = true;
}

static int gicv3_pre_save(void *opaque)
{
    GICv3State *s = (GICv3State *)opaque;
    ARMGICv3CommonClass *c = ARM_GICV3_COMMON_GET_CLASS(s);

    if (c->pre_save) {
        c->pre_save(s);
    }

    return 0;
}

static int gicv3_post_load(void *opaque, int version_id)
{
    GICv3State *s = (GICv3State *)opaque;
    ARMGICv3CommonClass *c = ARM_GICV3_COMMON_GET_CLASS(s);

    gicv3_gicd_state_shift_bug_post_load(s);

    if (c->post_load) {
        c->post_load(s);
    }
    return 0;
}

static bool virt_state_needed(void *opaque)
{
    GICv3CPUState *cs = opaque;

    return cs->num_list_regs != 0;
}

static const VMStateDescription vmstate_gicv3_cpu_virt = {
    .name = "arm_gicv3_cpu/virt",
    .version_id = 1,
    .minimum_version_id = 1,
    .needed = virt_state_needed,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT64_2DARRAY(ich_apr, GICv3CPUState, 3, 4),
        VMSTATE_UINT64(ich_hcr_el2, GICv3CPUState),
        VMSTATE_UINT64_ARRAY(ich_lr_el2, GICv3CPUState, GICV3_LR_MAX),
        VMSTATE_UINT64(ich_vmcr_el2, GICv3CPUState),
        VMSTATE_END_OF_LIST()
    }
};

static int vmstate_gicv3_cpu_pre_load(void *opaque)
{
    GICv3CPUState *cs = opaque;

   
    cs->icc_sre_el1 = 0x7;
    return 0;
}

static bool icc_sre_el1_reg_needed(void *opaque)
{
    GICv3CPUState *cs = opaque;

    return cs->icc_sre_el1 != 7;
}

const VMStateDescription vmstate_gicv3_cpu_sre_el1 = {
    .name = "arm_gicv3_cpu/sre_el1",
    .version_id = 1,
    .minimum_version_id = 1,
    .needed = icc_sre_el1_reg_needed,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT64(icc_sre_el1, GICv3CPUState),
        VMSTATE_END_OF_LIST()
    }
};

static bool gicv4_needed(void *opaque)
{
    GICv3CPUState *cs = opaque;

    return cs->gic->revision > 3;
}

const VMStateDescription vmstate_gicv3_gicv4 = {
    .name = "arm_gicv3_cpu/gicv4",
    .version_id = 1,
    .minimum_version_id = 1,
    .needed = gicv4_needed,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT64(gicr_vpropbaser, GICv3CPUState),
        VMSTATE_UINT64(gicr_vpendbaser, GICv3CPUState),
        VMSTATE_END_OF_LIST()
    }
};

static bool gicv3_cpu_nmi_needed(void *opaque)
{
    GICv3CPUState *cs = opaque;

    return cs->gic->nmi_support;
}

static const VMStateDescription vmstate_gicv3_cpu_nmi = {
    .name = "arm_gicv3_cpu/nmi",
    .version_id = 1,
    .minimum_version_id = 1,
    .needed = gicv3_cpu_nmi_needed,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(gicr_inmir0, GICv3CPUState),
        VMSTATE_END_OF_LIST()
    }
};

static const VMStateDescription vmstate_gicv3_cpu = {
    .name = "arm_gicv3_cpu",
    .version_id = 1,
    .minimum_version_id = 1,
    .pre_load = vmstate_gicv3_cpu_pre_load,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(level, GICv3CPUState),
        VMSTATE_UINT32(gicr_ctlr, GICv3CPUState),
        VMSTATE_UINT32_ARRAY(gicr_statusr, GICv3CPUState, 2),
        VMSTATE_UINT32(gicr_waker, GICv3CPUState),
        VMSTATE_UINT64(gicr_propbaser, GICv3CPUState),
        VMSTATE_UINT64(gicr_pendbaser, GICv3CPUState),
        VMSTATE_UINT32(gicr_igroupr0, GICv3CPUState),
        VMSTATE_UINT32(gicr_ienabler0, GICv3CPUState),
        VMSTATE_UINT32(gicr_ipendr0, GICv3CPUState),
        VMSTATE_UINT32(gicr_iactiver0, GICv3CPUState),
        VMSTATE_UINT32(edge_trigger, GICv3CPUState),
        VMSTATE_UINT32(gicr_igrpmodr0, GICv3CPUState),
        VMSTATE_UINT32(gicr_nsacr, GICv3CPUState),
        VMSTATE_UINT8_ARRAY(gicr_ipriorityr, GICv3CPUState, GIC_INTERNAL),
        VMSTATE_UINT64_ARRAY(icc_ctlr_el1, GICv3CPUState, 2),
        VMSTATE_UINT64(icc_pmr_el1, GICv3CPUState),
        VMSTATE_UINT64_ARRAY(icc_bpr, GICv3CPUState, 3),
        VMSTATE_UINT64_2DARRAY(icc_apr, GICv3CPUState, 3, 4),
        VMSTATE_UINT64_ARRAY(icc_igrpen, GICv3CPUState, 3),
        VMSTATE_UINT64(icc_ctlr_el3, GICv3CPUState),
        VMSTATE_END_OF_LIST()
    },
    .subsections = (const VMStateDescription * const []) {
        &vmstate_gicv3_cpu_virt,
        &vmstate_gicv3_cpu_sre_el1,
        &vmstate_gicv3_gicv4,
        &vmstate_gicv3_cpu_nmi,
        NULL
    }
};

static int gicv3_pre_load(void *opaque)
{
   
    

    return 0;
}

static bool needed_always(void *opaque)
{
    return true;
}

const VMStateDescription vmstate_gicv3_gicd_state_shift_bug = {
    .name = "arm_gicv3/gicd_state_shift_bug",
    .version_id = 1,
    .minimum_version_id = 1,
    .needed = needed_always,
    .fields = (const VMStateField[]) {
        VMSTATE_BOOL(gicd_state_shift_bug, GICv3State),
        VMSTATE_END_OF_LIST()
    }
};

static bool gicv3_nmi_needed(void *opaque)
{
    GICv3State *cs = opaque;

    return cs->nmi_support;
}

const VMStateDescription vmstate_gicv3_gicd_nmi = {
    .name = "arm_gicv3/gicd_nmi",
    .version_id = 1,
    .minimum_version_id = 1,
    .needed = gicv3_nmi_needed,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32_ARRAY(nmi, GICv3State, GICV3_BMP_SIZE),
        VMSTATE_END_OF_LIST()
    }
};

static const VMStateDescription vmstate_gicv3 = {
    .name = "arm_gicv3",
    .version_id = 1,
    .minimum_version_id = 1,
    .pre_load = gicv3_pre_load,
    .pre_save = gicv3_pre_save,
    .post_load = gicv3_post_load,
    .priority = MIG_PRI_GICV3,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(gicd_ctlr, GICv3State),
        VMSTATE_UINT32_ARRAY(gicd_statusr, GICv3State, 2),
        VMSTATE_UINT32_ARRAY(group, GICv3State, GICV3_BMP_SIZE),
        VMSTATE_UINT32_ARRAY(grpmod, GICv3State, GICV3_BMP_SIZE),
        VMSTATE_UINT32_ARRAY(enabled, GICv3State, GICV3_BMP_SIZE),
        VMSTATE_UINT32_ARRAY(pending, GICv3State, GICV3_BMP_SIZE),
        VMSTATE_UINT32_ARRAY(active, GICv3State, GICV3_BMP_SIZE),
        VMSTATE_UINT32_ARRAY(level, GICv3State, GICV3_BMP_SIZE),
        VMSTATE_UINT32_ARRAY(edge_trigger, GICv3State, GICV3_BMP_SIZE),
        VMSTATE_UINT8_ARRAY(gicd_ipriority, GICv3State, GICV3_MAXIRQ),
        VMSTATE_UINT64_ARRAY(gicd_irouter, GICv3State, GICV3_MAXIRQ),
        VMSTATE_UINT32_ARRAY(gicd_nsacr, GICv3State,
                             DIV_ROUND_UP(GICV3_MAXIRQ, 16)),
        VMSTATE_STRUCT_VARRAY_POINTER_UINT32(cpu, GICv3State, num_cpu,
                                             vmstate_gicv3_cpu, GICv3CPUState),
        VMSTATE_END_OF_LIST()
    },
    .subsections = (const VMStateDescription * const []) {
        &vmstate_gicv3_gicd_state_shift_bug,
        &vmstate_gicv3_gicd_nmi,
        NULL
    }
};

void gicv3_init_irqs_and_mmio(GICv3State *s, qemu_irq_handler handler,
                              const MemoryRegionOps *ops)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(s);
    int i;
    int cpuidx;

    
    i = s->num_irq - GIC_INTERNAL + GIC_INTERNAL * s->num_cpu;
    qdev_init_gpio_in(DEVICE(s), handler, i);

    for (i = 0; i < s->num_cpu; i++) {
        sysbus_init_irq(sbd, &s->cpu[i].parent_irq);
    }
    for (i = 0; i < s->num_cpu; i++) {
        sysbus_init_irq(sbd, &s->cpu[i].parent_fiq);
    }
    for (i = 0; i < s->num_cpu; i++) {
        sysbus_init_irq(sbd, &s->cpu[i].parent_virq);
    }
    for (i = 0; i < s->num_cpu; i++) {
        sysbus_init_irq(sbd, &s->cpu[i].parent_vfiq);
    }
    for (i = 0; i < s->num_cpu; i++) {
        sysbus_init_irq(sbd, &s->cpu[i].parent_nmi);
    }
    for (i = 0; i < s->num_cpu; i++) {
        sysbus_init_irq(sbd, &s->cpu[i].parent_vnmi);
    }

    memory_region_init_io(&s->iomem_dist, OBJECT(s), ops, s,
                          "gicv3_dist", 0x10000);
    sysbus_init_mmio(sbd, &s->iomem_dist);

    s->redist_regions = g_new0(GICv3RedistRegion, s->nb_redist_regions);
    cpuidx = 0;
    for (i = 0; i < s->nb_redist_regions; i++) {
        char *name = g_strdup_printf("gicv3_redist_region[%d]", i);
        GICv3RedistRegion *region = &s->redist_regions[i];

        region->gic = s;
        region->cpuidx = cpuidx;
        cpuidx += s->redist_region_count[i];

        memory_region_init_io(&region->iomem, OBJECT(s),
                              ops ? &ops[1] : NULL, region, name,
                              s->redist_region_count[i] * gicv3_redist_size(s));
        sysbus_init_mmio(sbd, &region->iomem);
        g_free(name);
    }
}

static void arm_gicv3_common_realize(DeviceState *dev, Error **errp)
{
    GICv3State *s = ARM_GICV3_COMMON(dev);
    int i, rdist_capacity, cpuidx;

    
    if (s->revision != 3 && s->revision != 4) {
        error_setg(errp, "unsupported GIC revision %d", s->revision);
        return;
    }

    if (s->num_irq > GICV3_MAXIRQ) {
        error_setg(errp,
                   "requested %u interrupt lines exceeds GIC maximum %d",
                   s->num_irq, GICV3_MAXIRQ);
        return;
    }
    if (s->num_irq < GIC_INTERNAL) {
        error_setg(errp,
                   "requested %u interrupt lines is below GIC minimum %d",
                   s->num_irq, GIC_INTERNAL);
        return;
    }
    if (s->num_cpu == 0) {
        error_setg(errp, "num-cpu must be at least 1");
        return;
    }

    
    if (s->num_irq % 32) {
        error_setg(errp,
                   "%d interrupt lines unsupported: not divisible by 32",
                   s->num_irq);
        return;
    }

    if (s->lpi_enable && !s->dma) {
        error_setg(errp, "Redist-ITS: Guest 'sysmem' reference link not set");
        return;
    }

    rdist_capacity = 0;
    for (i = 0; i < s->nb_redist_regions; i++) {
        rdist_capacity += s->redist_region_count[i];
    }
    if (rdist_capacity != s->num_cpu) {
        error_setg(errp, "Capacity of the redist regions(%d) "
                   "does not match the number of vcpus(%d)",
                   rdist_capacity, s->num_cpu);
        return;
    }

    if (s->lpi_enable) {
        address_space_init(&s->dma_as, s->dma,
                           "gicv3-its-sysmem");
    }

    s->cpu = g_new0(GICv3CPUState, s->num_cpu);

    for (i = 0; i < s->num_cpu; i++) {
        CPUState *cpu = qemu_get_cpu(i);
        uint64_t cpu_affid;

        s->cpu[i].cpu = cpu;
        s->cpu[i].gic = s;
        
        gicv3_set_gicv3state(cpu, &s->cpu[i]);

        
        cpu_affid = object_property_get_uint(OBJECT(cpu), "mp-affinity", NULL);

        
        cpu_affid = ((cpu_affid & 0xFF00000000ULL) >> 8) |
                     (cpu_affid & 0xFFFFFF);
        s->cpu[i].gicr_typer = (cpu_affid << 32) |
            (1 << 24) |
            (i << 8);

        if (s->lpi_enable) {
            s->cpu[i].gicr_typer |= GICR_TYPER_PLPIS;
            if (s->revision > 3) {
                s->cpu[i].gicr_typer |= GICR_TYPER_VLPIS;
            }
        }
    }

    
    cpuidx = 0;
    for (i = 0; i < s->nb_redist_regions; i++) {
        cpuidx += s->redist_region_count[i];
        s->cpu[cpuidx - 1].gicr_typer |= GICR_TYPER_LAST;
    }

    s->itslist = g_ptr_array_new();
}

static void arm_gicv3_finalize(Object *obj)
{
    GICv3State *s = ARM_GICV3_COMMON(obj);

    g_free(s->redist_region_count);
}

static void arm_gicv3_common_reset_hold(Object *obj, ResetType type)
{
    GICv3State *s = ARM_GICV3_COMMON(obj);
    int i;

    for (i = 0; i < s->num_cpu; i++) {
        GICv3CPUState *cs = &s->cpu[i];

        cs->level = 0;
        cs->gicr_ctlr = 0;
        if (s->lpi_enable) {
            
            cs->gicr_ctlr |= GICR_CTLR_CES;
        }
        cs->gicr_statusr[GICV3_S] = 0;
        cs->gicr_statusr[GICV3_NS] = 0;
        cs->gicr_waker = GICR_WAKER_ProcessorSleep | GICR_WAKER_ChildrenAsleep;
        cs->gicr_propbaser = 0;
        cs->gicr_pendbaser = 0;
        cs->gicr_vpropbaser = 0;
        cs->gicr_vpendbaser = 0;
        
        if (s->irq_reset_nonsecure) {
            cs->gicr_igroupr0 = 0xffffffff;
        } else {
            cs->gicr_igroupr0 = 0;
        }

        cs->gicr_ienabler0 = 0;
        cs->gicr_ipendr0 = 0;
        cs->gicr_iactiver0 = 0;
        cs->edge_trigger = 0xffff;
        cs->gicr_igrpmodr0 = 0;
        cs->gicr_nsacr = 0;
        memset(cs->gicr_ipriorityr, 0, sizeof(cs->gicr_ipriorityr));

        cs->hppi.prio = 0xff;
        cs->hppi.nmi = false;
        cs->hpplpi.prio = 0xff;
        cs->hpplpi.nmi = false;
        cs->hppvlpi.prio = 0xff;
        cs->hppvlpi.nmi = false;

        
    }

    
    if (s->security_extn) {
        s->gicd_ctlr = GICD_CTLR_ARE_S | GICD_CTLR_ARE_NS;
    } else {
        s->gicd_ctlr = GICD_CTLR_DS | GICD_CTLR_ARE;
    }

    s->gicd_statusr[GICV3_S] = 0;
    s->gicd_statusr[GICV3_NS] = 0;

    memset(s->group, 0, sizeof(s->group));
    memset(s->grpmod, 0, sizeof(s->grpmod));
    memset(s->enabled, 0, sizeof(s->enabled));
    memset(s->pending, 0, sizeof(s->pending));
    memset(s->active, 0, sizeof(s->active));
    memset(s->level, 0, sizeof(s->level));
    memset(s->edge_trigger, 0, sizeof(s->edge_trigger));
    memset(s->gicd_ipriority, 0, sizeof(s->gicd_ipriority));
    memset(s->gicd_irouter, 0, sizeof(s->gicd_irouter));
    memset(s->gicd_nsacr, 0, sizeof(s->gicd_nsacr));
    
    gicv3_cache_all_target_cpustates(s);

    if (s->irq_reset_nonsecure) {
        
        for (i = GIC_INTERNAL; i < s->num_irq; i++) {
            gicv3_gicd_group_set(s, i);
        }
    }
    s->gicd_state_shift_bug = true;
}

static void arm_gic_common_linux_init(ARMLinuxBootIf *obj,
                                      bool secure_boot)
{
    GICv3State *s = ARM_GICV3_COMMON(obj);

    if (s->security_extn && !secure_boot) {
        
        s->irq_reset_nonsecure = true;
    }
}

static const Property arm_gicv3_common_properties[] = {
    DEFINE_PROP_UINT32("num-cpu", GICv3State, num_cpu, 1),
    DEFINE_PROP_UINT32("num-irq", GICv3State, num_irq, 32),
    DEFINE_PROP_UINT32("revision", GICv3State, revision, 3),
    DEFINE_PROP_BOOL("has-lpi", GICv3State, lpi_enable, 0),
    DEFINE_PROP_BOOL("has-nmi", GICv3State, nmi_support, 0),
    DEFINE_PROP_BOOL("has-security-extensions", GICv3State, security_extn, 0),
    
    DEFINE_PROP_BOOL("force-8-bit-prio", GICv3State, force_8bit_prio, 0),
    DEFINE_PROP_ARRAY("redist-region-count", GICv3State, nb_redist_regions,
                      redist_region_count, qdev_prop_uint32, uint32_t),
    DEFINE_PROP_LINK("sysmem", GICv3State, dma, TYPE_MEMORY_REGION,
                     MemoryRegion *),
};

static void arm_gicv3_common_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);
    ARMLinuxBootIfClass *albifc = ARM_LINUX_BOOT_IF_CLASS(klass);

    rc->phases.hold = arm_gicv3_common_reset_hold;
    dc->realize = arm_gicv3_common_realize;
    device_class_set_props(dc, arm_gicv3_common_properties);
    dc->vmsd = &vmstate_gicv3;
    albifc->arm_linux_init = arm_gic_common_linux_init;
}

static const TypeInfo arm_gicv3_common_type = {
    .name = TYPE_ARM_GICV3_COMMON,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(GICv3State),
    .class_size = sizeof(ARMGICv3CommonClass),
    .class_init = arm_gicv3_common_class_init,
    .instance_finalize = arm_gicv3_finalize,
    .abstract = true,
    .interfaces = (InterfaceInfo []) {
        { TYPE_ARM_LINUX_BOOT_IF },
        { },
    },
};

static void register_types(void)
{
    type_register_static(&arm_gicv3_common_type);
}

type_init(register_types)

const char *gicv3_class_name(void)
{
    if (false) {
        return "kvm-arm-gicv3";
    } else if (gunyah_enabled()) {
        return "gunyah-arm-gicv3";
    } else {
        
        return "arm-gicv3";
    }
}
