#ifndef HW_PLATFORM_BUS_H
#define HW_PLATFORM_BUS_H


#include "hw/sysbus.h"
#include "qom/object.h"


#define TYPE_PLATFORM_BUS_DEVICE "platform-bus-device"
OBJECT_DECLARE_SIMPLE_TYPE(PlatformBusDevice, PLATFORM_BUS_DEVICE)

struct PlatformBusDevice {
    SysBusDevice parent_obj;

    uint32_t mmio_size;
    MemoryRegion mmio;

    uint32_t num_irqs;
    qemu_irq *irqs;
    unsigned long *used_irqs;
};

int platform_bus_get_irqn(PlatformBusDevice *platform_bus, SysBusDevice *sbdev,
                          int n);
hwaddr platform_bus_get_mmio_addr(PlatformBusDevice *pbus, SysBusDevice *sbdev,
                                  int n);

void platform_bus_link_device(PlatformBusDevice *pbus, SysBusDevice *sbdev);

#endif /* HW_PLATFORM_BUS_H */
