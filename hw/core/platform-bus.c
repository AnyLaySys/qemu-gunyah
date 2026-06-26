
#include "qemu/osdep.h"
#include "hw/platform-bus.h"
#include "hw/qdev-properties.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "qemu/module.h"


int platform_bus_get_irqn(PlatformBusDevice *pbus, SysBusDevice *sbdev,
                          int n)
{
    qemu_irq sbirq = sysbus_get_connected_irq(sbdev, n);
    int i;

    for (i = 0; i < pbus->num_irqs; i++) {
        if (pbus->irqs[i] == sbirq) {
            return i;
        }
    }

    return -1;
}

hwaddr platform_bus_get_mmio_addr(PlatformBusDevice *pbus, SysBusDevice *sbdev,
                                  int n)
{
    MemoryRegion *pbus_mr = &pbus->mmio;
    MemoryRegion *sbdev_mr = sysbus_mmio_get_region(sbdev, n);
    Object *pbus_mr_obj = OBJECT(pbus_mr);
    Object *parent_mr;

    if (!memory_region_is_mapped(sbdev_mr)) {
        return -1;
    }

    parent_mr = object_property_get_link(OBJECT(sbdev_mr), "container",
                                         &error_abort);
    if (parent_mr != pbus_mr_obj) {
        return -1;
    }

    return object_property_get_uint(OBJECT(sbdev_mr), "addr", NULL);
}

static void platform_bus_count_irqs(SysBusDevice *sbdev, void *opaque)
{
    PlatformBusDevice *pbus = opaque;
    qemu_irq sbirq;
    int n, i;

    for (n = 0; ; n++) {
        if (!sysbus_has_irq(sbdev, n)) {
            break;
        }

        sbirq = sysbus_get_connected_irq(sbdev, n);
        for (i = 0; i < pbus->num_irqs; i++) {
            if (pbus->irqs[i] == sbirq) {
                bitmap_set(pbus->used_irqs, i, 1);
                break;
            }
        }
    }
}

static void plaform_bus_refresh_irqs(PlatformBusDevice *pbus)
{
    bitmap_zero(pbus->used_irqs, pbus->num_irqs);
    foreach_dynamic_sysbus_device(platform_bus_count_irqs, pbus);
}

static void platform_bus_map_irq(PlatformBusDevice *pbus, SysBusDevice *sbdev,
                                 int n)
{
    int max_irqs = pbus->num_irqs;
    int irqn;

    if (sysbus_is_irq_connected(sbdev, n)) {
        return;
    }

    irqn = find_first_zero_bit(pbus->used_irqs, max_irqs);
    if (irqn >= max_irqs) {
        error_report("Platform Bus: Can not fit IRQ line");
        exit(1);
    }

    set_bit(irqn, pbus->used_irqs);
    sysbus_connect_irq(sbdev, n, pbus->irqs[irqn]);
}

static void platform_bus_map_mmio(PlatformBusDevice *pbus, SysBusDevice *sbdev,
                                  int n)
{
    MemoryRegion *sbdev_mr = sysbus_mmio_get_region(sbdev, n);
    uint64_t size = memory_region_size(sbdev_mr);
    uint64_t alignment = (1ULL << (63 - clz64(size + size - 1)));
    uint64_t off;
    bool found_region = false;

    if (memory_region_is_mapped(sbdev_mr)) {
        return;
    }

    for (off = 0; off < pbus->mmio_size; off += alignment) {
        MemoryRegion *mr = memory_region_find(&pbus->mmio, off, size).mr;
        if (!mr) {
            found_region = true;
            break;
        } else {
            memory_region_unref(mr);
        }
    }

    if (!found_region) {
        error_report("Platform Bus: Can not fit MMIO region of size %"PRIx64,
                     size);
        exit(1);
    }

    memory_region_add_subregion(&pbus->mmio, off, sbdev_mr);
}

void platform_bus_link_device(PlatformBusDevice *pbus, SysBusDevice *sbdev)
{
    int i;

    for (i = 0; sysbus_has_irq(sbdev, i); i++) {
        platform_bus_map_irq(pbus, sbdev, i);
    }

    for (i = 0; sysbus_has_mmio(sbdev, i); i++) {
        platform_bus_map_mmio(pbus, sbdev, i);
    }
}

static void platform_bus_realize(DeviceState *dev, Error **errp)
{
    PlatformBusDevice *pbus;
    SysBusDevice *d;
    int i;

    d = SYS_BUS_DEVICE(dev);
    pbus = PLATFORM_BUS_DEVICE(dev);

    memory_region_init(&pbus->mmio, OBJECT(dev), "platform bus",
                       pbus->mmio_size);
    sysbus_init_mmio(d, &pbus->mmio);

    pbus->used_irqs = bitmap_new(pbus->num_irqs);
    pbus->irqs = g_new0(qemu_irq, pbus->num_irqs);
    for (i = 0; i < pbus->num_irqs; i++) {
        sysbus_init_irq(d, &pbus->irqs[i]);
    }

    plaform_bus_refresh_irqs(pbus);
}

static const Property platform_bus_properties[] = {
    DEFINE_PROP_UINT32("num_irqs", PlatformBusDevice, num_irqs, 0),
    DEFINE_PROP_UINT32("mmio_size", PlatformBusDevice, mmio_size, 0),
};

static void platform_bus_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = platform_bus_realize;
    device_class_set_props(dc, platform_bus_properties);
}

static const TypeInfo platform_bus_info = {
    .name          = TYPE_PLATFORM_BUS_DEVICE,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(PlatformBusDevice),
    .class_init    = platform_bus_class_init,
};

static void platform_bus_register_types(void)
{
    type_register_static(&platform_bus_info);
}

type_init(platform_bus_register_types)
