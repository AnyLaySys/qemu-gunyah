
#ifndef HW_ACPI_PCIHP_H
#define HW_ACPI_PCIHP_H

#include "hw/acpi/acpi.h"
#include "hw/hotplug.h"

#define ACPI_PCIHP_IO_BASE_PROP "acpi-pcihp-io-base"
#define ACPI_PCIHP_IO_LEN_PROP "acpi-pcihp-io-len"

typedef struct AcpiPciHpPciStatus {
    uint32_t up;
    uint32_t down;
    uint32_t hotplug_enable;
} AcpiPciHpPciStatus;

#define ACPI_PCIHP_PROP_BSEL "acpi-pcihp-bsel"
#define ACPI_PCIHP_MAX_HOTPLUG_BUS 256
#define ACPI_PCIHP_BSEL_DEFAULT 0x0

typedef struct AcpiPciHpState {
    AcpiPciHpPciStatus acpi_pcihp_pci_status[ACPI_PCIHP_MAX_HOTPLUG_BUS];
    uint32_t hotplug_select;
    uint32_t acpi_index;
    PCIBus *root;
    MemoryRegion io;
    uint16_t io_base;
    uint16_t io_len;
    bool use_acpi_hotplug_bridge;
    bool use_acpi_root_pci_hotplug;
} AcpiPciHpState;

void acpi_pcihp_init(Object *owner, AcpiPciHpState *, PCIBus *root,
                     MemoryRegion *io, uint16_t io_base);

bool acpi_pcihp_is_hotpluggbale_bus(AcpiPciHpState *s, BusState *bus);
void acpi_pcihp_device_pre_plug_cb(HotplugHandler *hotplug_dev,
                                   DeviceState *dev, Error **errp);
void acpi_pcihp_device_plug_cb(HotplugHandler *hotplug_dev, AcpiPciHpState *s,
                               DeviceState *dev, Error **errp);
void acpi_pcihp_device_unplug_cb(HotplugHandler *hotplug_dev, AcpiPciHpState *s,
                                 DeviceState *dev, Error **errp);
void acpi_pcihp_device_unplug_request_cb(HotplugHandler *hotplug_dev,
                                         AcpiPciHpState *s, DeviceState *dev,
                                         Error **errp);

void acpi_pcihp_reset(AcpiPciHpState *s);

void build_append_pcihp_slots(Aml *parent_scope, PCIBus *bus);

extern const VMStateDescription vmstate_acpi_pcihp_pci_status;

#define VMSTATE_PCI_HOTPLUG(pcihp, state, test_pcihp, test_acpi_index) \
        VMSTATE_UINT32_TEST(pcihp.hotplug_select, state, \
                            test_pcihp), \
        VMSTATE_STRUCT_ARRAY_TEST(pcihp.acpi_pcihp_pci_status, state, \
                                  ACPI_PCIHP_MAX_HOTPLUG_BUS, \
                                  test_pcihp, 1, \
                                  vmstate_acpi_pcihp_pci_status, \
                                  AcpiPciHpPciStatus), \
        VMSTATE_UINT32_TEST(pcihp.acpi_index, state, \
                            test_acpi_index)

#endif
