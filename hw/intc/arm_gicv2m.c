

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/sysbus.h"
#include "hw/irq.h"
#include "hw/pci/msi.h"
#include "hw/qdev-properties.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qom/object.h"

#define TYPE_ARM_GICV2M "arm-gicv2m"
OBJECT_DECLARE_SIMPLE_TYPE(ARMGICv2mState, ARM_GICV2M)

#define GICV2M_NUM_SPI_MAX 128

#define V2M_MSI_TYPER           0x008
#define V2M_MSI_SETSPI_NS       0x040
#define V2M_MSI_IIDR            0xFCC
#define V2M_IIDR0               0xFD0
#define V2M_IIDR11              0xFFC

#define PRODUCT_ID_QEMU         0x51 /* ASCII code Q */

struct ARMGICv2mState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    qemu_irq spi[GICV2M_NUM_SPI_MAX];

    uint32_t base_spi;
    uint32_t num_spi;
};

static void gicv2m_set_irq(void *opaque, int irq)
{
    ARMGICv2mState *s = (ARMGICv2mState *)opaque;

    qemu_irq_pulse(s->spi[irq]);
}

static uint64_t gicv2m_read(void *opaque, hwaddr offset,
                            unsigned size)
{
    ARMGICv2mState *s = (ARMGICv2mState *)opaque;
    uint32_t val;

    if (size != 4) {
        qemu_log_mask(LOG_GUEST_ERROR, "gicv2m_read: bad size %u\n", size);
        return 0;
    }

    switch (offset) {
    case V2M_MSI_TYPER:
        val = (s->base_spi + 32) << 16;
        val |= s->num_spi;
        return val;
    case V2M_MSI_IIDR:
        return (PRODUCT_ID_QEMU << 20);
    case V2M_IIDR0 ... V2M_IIDR11:
        return 0;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "gicv2m_read: Bad offset %x\n", (int)offset);
        return 0;
    }
}

static void gicv2m_write(void *opaque, hwaddr offset,
                        uint64_t value, unsigned size)
{
    ARMGICv2mState *s = (ARMGICv2mState *)opaque;

    if (size != 2 && size != 4) {
        qemu_log_mask(LOG_GUEST_ERROR, "gicv2m_write: bad size %u\n", size);
        return;
    }

    switch (offset) {
    case V2M_MSI_SETSPI_NS: {
        int spi;

        spi = (value & 0x3ff) - (s->base_spi + 32);
        if (spi >= 0 && spi < s->num_spi) {
            gicv2m_set_irq(s, spi);
        }
        return;
    }
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "gicv2m_write: Bad offset %x\n", (int)offset);
    }
}

static const MemoryRegionOps gicv2m_ops = {
    .read = gicv2m_read,
    .write = gicv2m_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void gicv2m_realize(DeviceState *dev, Error **errp)
{
    ARMGICv2mState *s = ARM_GICV2M(dev);
    int i;

    if (s->num_spi > GICV2M_NUM_SPI_MAX) {
        error_setg(errp,
                   "requested %u SPIs exceeds GICv2m frame maximum %d",
                   s->num_spi, GICV2M_NUM_SPI_MAX);
        return;
    }

    if (s->base_spi + 32 > 1020 - s->num_spi) {
        error_setg(errp,
                   "requested base SPI %u+%u exceeds max. number 1020",
                   s->base_spi + 32, s->num_spi);
        return;
    }

    for (i = 0; i < s->num_spi; i++) {
        sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->spi[i]);
    }

    msi_nonbroken = true;
}

static void gicv2m_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    ARMGICv2mState *s = ARM_GICV2M(obj);

    memory_region_init_io(&s->iomem, OBJECT(s), &gicv2m_ops, s,
                          "gicv2m", 0x1000);
    sysbus_init_mmio(sbd, &s->iomem);
}

static const Property gicv2m_properties[] = {
    DEFINE_PROP_UINT32("base-spi", ARMGICv2mState, base_spi, 0),
    DEFINE_PROP_UINT32("num-spi", ARMGICv2mState, num_spi, 64),
};

static void gicv2m_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_props(dc, gicv2m_properties);
    dc->realize = gicv2m_realize;
}

static const TypeInfo gicv2m_info = {
    .name          = TYPE_ARM_GICV2M,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(ARMGICv2mState),
    .instance_init = gicv2m_init,
    .class_init    = gicv2m_class_init,
};

static void gicv2m_register_types(void)
{
    type_register_static(&gicv2m_info);
}

type_init(gicv2m_register_types)
