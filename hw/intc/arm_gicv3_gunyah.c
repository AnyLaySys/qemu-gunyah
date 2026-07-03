
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "cpu.h"
#include "hw/intc/arm_gicv3_common.h"
#include "qemu/error-report.h"
#include "qemu/module.h"
#include "system/gunyah.h"
#include "system/gunyah_int.h"
#include "system/runstate.h"
#include "gicv3_internal.h"
#include "vgic_common.h"
#include "qom/object.h"
#include "target/arm/cpregs.h"
#include "qemu/event_notifier.h"

struct GUNYAHARMGICv3Class {
    ARMGICv3CommonClass parent_class;
    DeviceRealize parent_realize;
    ResettablePhases parent_phases;
};

#define TYPE_GUNYAH_ARM_GICV3 "gunyah-arm-gicv3"
typedef struct GUNYAHARMGICv3Class GUNYAHARMGICv3Class;

DECLARE_OBJ_CHECKERS(GICv3State, GUNYAHARMGICv3Class,
                     GUNYAH_ARM_GICV3, TYPE_GUNYAH_ARM_GICV3)

static EventNotifier *irq_notify;
static int irq_notify_count;

void gunyah_gic_register_irq_notifiers(EventNotifier *notifiers,
                                        int count, int base_spi)
{
    int total = base_spi + count;
    int i;

    if (!irq_notify || irq_notify_count < total) {
        EventNotifier *old = irq_notify;
        int old_count = irq_notify_count;

        irq_notify = g_new0(EventNotifier, total);
        if (old && old_count > 0) {
            memcpy(irq_notify, old, old_count * sizeof(EventNotifier));
            g_free(old);
        }
        irq_notify_count = total;
    }

    for (i = 0; i < count; i++) {
        irq_notify[base_spi + i] = notifiers[i];
    }

    gh_report("GIC registered %d IRQ notifiers at SPI %d-%d "
              "(irq_notify_count=%d)",
              count, base_spi, base_spi + count - 1, irq_notify_count);
}

static void gunyah_arm_gicv3_set_irq(void *opaque, int irq, int level)
{
    GICv3State *s = (GICv3State *)opaque;

    if (gunyah_enabled()) {
        static int gh_gic_irq_count;
        if (gh_gic_irq_count < 512) {
            gh_report("GHDBG gic set-irq spi=%d level=%d "
                      "notify_count=%d guest_spi_limit=%d has_notifier=%d",
                      irq, level, irq_notify_count,
                      s->num_irq - GIC_INTERNAL,
                      irq_notify && irq < irq_notify_count);
            gh_gic_irq_count++;
        }
    }

    if (!irq_notify || irq_notify_count == 0) {
        return;
    }

    if (level && irq < irq_notify_count && irq < s->num_irq - GIC_INTERNAL) {
        event_notifier_set(&irq_notify[irq]);
    }
}

static void gunyah_arm_gicv3_realize(DeviceState *dev, Error **errp)
{
    GICv3State *s = GUNYAH_ARM_GICV3(dev);
    GUNYAHARMGICv3Class *ggc = GUNYAH_ARM_GICV3_GET_CLASS(s);
    Error *local_err = NULL;
    GUNYAHState *state = get_gunyah_state();

    ggc->parent_realize(dev, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        return;
    }

    if (s->revision != 3) {
        error_setg(errp, "unsupported GIC revision %d for in-kernel GIC",
                   s->revision);
        return;
    }

    gicv3_init_irqs_and_mmio(s, gunyah_arm_gicv3_set_irq, NULL);

    irq_notify = NULL;
    irq_notify_count = 0;
    gh_report("GIC realized - IRQs from QEMU devices will be ignored "
              "(guest uses Gunyah doorbells, total SPIs=%d)",
              s->num_irq - GIC_INTERNAL);

    state->nr_irqs = 0;
}

static void gunyah_arm_gicv3_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);
    GUNYAHARMGICv3Class *ggc = GUNYAH_ARM_GICV3_CLASS(klass);

    device_class_set_parent_realize(dc, gunyah_arm_gicv3_realize,
                                    &ggc->parent_realize);
    resettable_class_set_parent_phases(rc, NULL, NULL, NULL,
                                       &ggc->parent_phases);
}

static const TypeInfo gunyah_arm_gicv3_info = {
    .name = TYPE_GUNYAH_ARM_GICV3,
    .parent = TYPE_ARM_GICV3_COMMON,
    .instance_size = sizeof(GICv3State),
    .class_init = gunyah_arm_gicv3_class_init,
    .class_size = sizeof(GUNYAHARMGICv3Class),
};

static void gunyah_arm_gicv3_register_types(void)
{
    type_register_static(&gunyah_arm_gicv3_info);
}

type_init(gunyah_arm_gicv3_register_types)
