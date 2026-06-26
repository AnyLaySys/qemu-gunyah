
#include "qemu/osdep.h"
#include "hw/intc/intc.h"
#include "qemu/module.h"

static const TypeInfo intctrl_info = {
    .name = TYPE_INTERRUPT_STATS_PROVIDER,
    .parent = TYPE_INTERFACE,
    .class_size = sizeof(InterruptStatsProviderClass),
};

static void intc_register_types(void)
{
    type_register_static(&intctrl_info);
}

type_init(intc_register_types)

