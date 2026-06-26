

#ifndef PCI_HOST_H
#define PCI_HOST_H

#include "hw/sysbus.h"
#include "qom/object.h"

#define PCI_HOST_BYPASS_IOMMU "bypass-iommu"

#define TYPE_PCI_HOST_BRIDGE "pci-host-bridge"
OBJECT_DECLARE_TYPE(PCIHostState, PCIHostBridgeClass, PCI_HOST_BRIDGE)

struct PCIHostState {
    SysBusDevice busdev;

    MemoryRegion conf_mem;
    MemoryRegion data_mem;
    MemoryRegion mmcfg;
    uint32_t config_reg;
    bool mig_enabled;
    PCIBus *bus;
    bool bypass_iommu;

    QLIST_ENTRY(PCIHostState) next;
};

struct PCIHostBridgeClass {
    SysBusDeviceClass parent_class;

    const char *(*root_bus_path)(PCIHostState *, PCIBus *);
};

void pci_host_config_write_common(PCIDevice *pci_dev, uint32_t addr,
                                  uint32_t limit, uint32_t val, uint32_t len);
uint32_t pci_host_config_read_common(PCIDevice *pci_dev, uint32_t addr,
                                     uint32_t limit, uint32_t len);

void pci_data_write(PCIBus *s, uint32_t addr, uint32_t val, unsigned len);
uint32_t pci_data_read(PCIBus *s, uint32_t addr, unsigned len);

extern const MemoryRegionOps pci_host_conf_le_ops;
extern const MemoryRegionOps pci_host_conf_be_ops;
extern const MemoryRegionOps pci_host_data_le_ops;

#endif /* PCI_HOST_H */
