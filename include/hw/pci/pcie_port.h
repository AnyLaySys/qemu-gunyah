
#ifndef QEMU_PCIE_PORT_H
#define QEMU_PCIE_PORT_H

#include "hw/pci/pci_bridge.h"
#include "hw/pci/pci_bus.h"
#include "hw/pci/pci_device.h"
#include "qom/object.h"

#define TYPE_PCIE_PORT "pcie-port"
OBJECT_DECLARE_SIMPLE_TYPE(PCIEPort, PCIE_PORT)

struct PCIEPort {
    PCIBridge   parent_obj;

    uint8_t     port;
};

void pcie_port_init_reg(PCIDevice *d);

PCIDevice *pcie_find_port_by_pn(PCIBus *bus, uint8_t pn);
PCIDevice *pcie_find_port_first(PCIBus *bus);
int pcie_count_ds_ports(PCIBus *bus);

#define TYPE_PCIE_SLOT "pcie-slot"
OBJECT_DECLARE_SIMPLE_TYPE(PCIESlot, PCIE_SLOT)

struct PCIESlot {
    PCIEPort    parent_obj;

    uint8_t     chassis;
    uint16_t    slot;

    PCIExpLinkSpeed speed;
    PCIExpLinkWidth width;

    bool        disable_acs;

    bool        hotplug;

    bool        hide_native_hotplug_cap;

    QLIST_ENTRY(PCIESlot) next;
};

void pcie_chassis_create(uint8_t chassis_number);
int pcie_chassis_add_slot(struct PCIESlot *slot);
void pcie_chassis_del_slot(PCIESlot *s);

#define TYPE_PCIE_ROOT_PORT         "pcie-root-port-base"
typedef struct PCIERootPortClass PCIERootPortClass;
DECLARE_CLASS_CHECKERS(PCIERootPortClass, PCIE_ROOT_PORT,
                       TYPE_PCIE_ROOT_PORT)

struct PCIERootPortClass {
    PCIDeviceClass parent_class;
    DeviceRealize parent_realize;
    ResettablePhases parent_phases;

    uint8_t (*aer_vector)(const PCIDevice *dev);
    int (*interrupts_init)(PCIDevice *dev, Error **errp);
    void (*interrupts_uninit)(PCIDevice *dev);

    int exp_offset;
    int aer_offset;
    int ssvid_offset;
    int acs_offset;    /* If nonzero, optional ACS capability offset */
    int ssid;
};

#endif /* QEMU_PCIE_PORT_H */
