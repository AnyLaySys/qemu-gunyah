
#ifndef QEMU_PCI_BRIDGE_H
#define QEMU_PCI_BRIDGE_H

#include "hw/pci/pci_device.h"
#include "hw/pci/pci_bus.h"
#include "qom/object.h"

typedef struct PCIBridgeWindows PCIBridgeWindows;

struct PCIBridgeWindows {
    MemoryRegion alias_pref_mem;
    MemoryRegion alias_mem;
    MemoryRegion alias_io;
    MemoryRegion alias_vga[QEMU_PCI_VGA_NUM_REGIONS];
};

#define TYPE_PCI_BRIDGE "base-pci-bridge"
OBJECT_DECLARE_SIMPLE_TYPE(PCIBridge, PCI_BRIDGE)
#define IS_PCI_BRIDGE(dev) object_dynamic_cast(OBJECT(dev), TYPE_PCI_BRIDGE)

struct PCIBridge {
    PCIDevice parent_obj;

    PCIBus sec_bus;
    MemoryRegion address_space_mem;
    MemoryRegion address_space_io;
    AddressSpace as_mem;
    AddressSpace as_io;

    PCIBridgeWindows windows;

    pci_map_irq_fn map_irq;
    const char *bus_name;

    bool pcie_writeable_slt_bug;
};

#define PCI_BRIDGE_DEV_PROP_CHASSIS_NR "chassis_nr"
#define PCI_BRIDGE_DEV_PROP_MSI        "msi"
#define PCI_BRIDGE_DEV_PROP_SHPC       "shpc"
typedef struct PXBDev {
    PCIDevice parent_obj;

    uint8_t bus_nr;
    uint16_t numa_node;
    bool bypass_iommu;
} PXBDev;

typedef struct PXBPCIEDev {
    PXBDev parent_obj;
} PXBPCIEDev;

#define TYPE_PXB_DEV "pxb"
OBJECT_DECLARE_SIMPLE_TYPE(PXBDev, PXB_DEV)

int pci_bridge_ssvid_init(PCIDevice *dev, uint8_t offset,
                          uint16_t svid, uint16_t ssid,
                          Error **errp);

PCIDevice *pci_bridge_get_device(PCIBus *bus);
PCIBus *pci_bridge_get_sec_bus(PCIBridge *br);

pcibus_t pci_bridge_get_base(const PCIDevice *bridge, uint8_t type);
pcibus_t pci_bridge_get_limit(const PCIDevice *bridge, uint8_t type);

void pci_bridge_update_mappings(PCIBridge *br);
void pci_bridge_write_config(PCIDevice *d,
                             uint32_t address, uint32_t val, int len);
void pci_bridge_disable_base_limit(PCIDevice *dev);
void pci_bridge_reset(DeviceState *qdev);

void pci_bridge_initfn(PCIDevice *pci_dev, const char *typename);
void pci_bridge_exitfn(PCIDevice *pci_dev);

void pci_bridge_dev_plug_cb(HotplugHandler *hotplug_dev, DeviceState *dev,
                            Error **errp);
void pci_bridge_dev_unplug_cb(HotplugHandler *hotplug_dev, DeviceState *dev,
                              Error **errp);
void pci_bridge_dev_unplug_request_cb(HotplugHandler *hotplug_dev,
                                      DeviceState *dev, Error **errp);

void pci_bridge_map_irq(PCIBridge *br, const char* bus_name,
                        pci_map_irq_fn map_irq);

#define  PCI_BRIDGE_CTL_VGA_16BIT       0x10    /* VGA 16-bit decode */
#define  PCI_BRIDGE_CTL_DISCARD         0x100   /* Primary discard timer */
#define  PCI_BRIDGE_CTL_SEC_DISCARD     0x200   /* Secondary discard timer */
#define  PCI_BRIDGE_CTL_DISCARD_STATUS  0x400   /* Discard timer status */
#define  PCI_BRIDGE_CTL_DISCARD_SERR    0x800   /* Discard timer SERR# enable */

typedef struct PCIBridgeQemuCap {
    uint8_t id;     /* Standard PCI capability header field */
    uint8_t next;   /* Standard PCI capability header field */
    uint8_t len;    /* Standard PCI vendor-specific capability header field */
    uint8_t type;   /* Red Hat vendor-specific capability type.
                       Types are defined with REDHAT_PCI_CAP_ prefix */

    uint32_t bus_res;   /* Minimum number of buses to reserve */
    uint64_t io;        /* IO space to reserve */
    uint32_t mem;       /* Non-prefetchable memory to reserve */
    uint32_t mem_pref_32; /* Prefetchable memory to reserve (32-bit MMIO) */
    uint64_t mem_pref_64; /* Prefetchable memory to reserve (64-bit MMIO) */
} PCIBridgeQemuCap;

#define REDHAT_PCI_CAP_TYPE_OFFSET      3
#define REDHAT_PCI_CAP_RESOURCE_RESERVE 1

typedef struct PCIResReserve {
    uint32_t bus;
    uint64_t io;
    uint64_t mem_non_pref;
    uint64_t mem_pref_32;
    uint64_t mem_pref_64;
} PCIResReserve;

#define REDHAT_PCI_CAP_RES_RESERVE_BUS_RES     4
#define REDHAT_PCI_CAP_RES_RESERVE_IO          8
#define REDHAT_PCI_CAP_RES_RESERVE_MEM         16
#define REDHAT_PCI_CAP_RES_RESERVE_PREF_MEM_32 20
#define REDHAT_PCI_CAP_RES_RESERVE_PREF_MEM_64 24
#define REDHAT_PCI_CAP_RES_RESERVE_CAP_SIZE    32

int pci_bridge_qemu_reserve_cap_init(PCIDevice *dev, int cap_offset,
                               PCIResReserve res_reserve, Error **errp);

#endif /* QEMU_PCI_BRIDGE_H */
