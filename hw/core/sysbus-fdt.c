
#include "qemu/osdep.h"
#include "qapi/error.h"
#include <libfdt.h>
#include "hw/core/sysbus-fdt.h"
#include "qemu/error-report.h"
#include "system/device_tree.h"
#include "hw/platform-bus.h"
#include "hw/arm/fdt.h"

typedef struct PlatformBusFDTData {
    void *fdt; /* device tree handle */
    int irq_start; /* index of the first IRQ usable by platform bus devices */
    const char *pbus_node_name; /* name of the platform bus node */
    PlatformBusDevice *pbus;
} PlatformBusFDTData;

typedef struct BindingEntry {
    const char *typename;
    const char *compat;
    int  (*add_fn)(SysBusDevice *sbdev, void *opaque);
    bool (*match_fn)(SysBusDevice *sbdev, const struct BindingEntry *combo);
} BindingEntry;

static bool type_match(SysBusDevice *sbdev, const BindingEntry *entry)
{
    return !strcmp(object_get_typename(OBJECT(sbdev)), entry->typename);
}

#define TYPE_BINDING(type, add_fn) {(type), NULL, (add_fn), NULL}

static const BindingEntry bindings[] = {
    TYPE_BINDING("", NULL), /* last element */
};


static void add_fdt_node(SysBusDevice *sbdev, void *opaque)
{
    int i, ret;

    for (i = 0; i < ARRAY_SIZE(bindings); i++) {
        const BindingEntry *iter = &bindings[i];

        if (type_match(sbdev, iter)) {
            if (!iter->match_fn || iter->match_fn(sbdev, iter)) {
                ret = iter->add_fn(sbdev, opaque);
                assert(!ret);
                return;
            }
        }
    }
    error_report("Device %s can not be dynamically instantiated",
                     qdev_fw_name(DEVICE(sbdev)));
    exit(1);
}

void platform_bus_add_all_fdt_nodes(void *fdt, const char *intc, hwaddr addr,
                                    hwaddr bus_size, int irq_start)
{
    const char platcomp[] = "qemu,platform\0simple-bus";
    PlatformBusDevice *pbus;
    DeviceState *dev;
    gchar *node;

    assert(fdt);

    node = g_strdup_printf("/platform-bus@%"PRIx64, addr);

    qemu_fdt_add_subnode(fdt, node);
    qemu_fdt_setprop(fdt, node, "compatible", platcomp, sizeof(platcomp));

    qemu_fdt_setprop_cells(fdt, node, "#size-cells", 1);
    qemu_fdt_setprop_cells(fdt, node, "#address-cells", 1);
    qemu_fdt_setprop_cells(fdt, node, "ranges", 0, addr >> 32, addr, bus_size);

    qemu_fdt_setprop_phandle(fdt, node, "interrupt-parent", intc);

    dev = qdev_find_recursive(sysbus_get_default(), TYPE_PLATFORM_BUS_DEVICE);
    pbus = PLATFORM_BUS_DEVICE(dev);

    PlatformBusFDTData data = {
        .fdt = fdt,
        .irq_start = irq_start,
        .pbus_node_name = node,
        .pbus = pbus,
    };

    foreach_dynamic_sysbus_device(add_fdt_node, &data);

    g_free(node);
}
