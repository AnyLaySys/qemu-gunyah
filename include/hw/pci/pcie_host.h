
#ifndef PCIE_HOST_H
#define PCIE_HOST_H

#include "hw/pci/pci_host.h"
#include "exec/memory.h"
#include "qom/object.h"

#define TYPE_PCIE_HOST_BRIDGE "pcie-host-bridge"
OBJECT_DECLARE_SIMPLE_TYPE(PCIExpressHost, PCIE_HOST_BRIDGE)

#define PCIE_HOST_MCFG_BASE "MCFG"
#define PCIE_HOST_MCFG_SIZE "mcfg_size"

#define PCIE_BASE_ADDR_UNMAPPED  ((hwaddr)-1ULL)

struct PCIExpressHost {
    PCIHostState pci;


    hwaddr  base_addr;

    hwaddr  size;

    MemoryRegion mmio;
};

void pcie_host_mmcfg_unmap(PCIExpressHost *e);
void pcie_host_mmcfg_init(PCIExpressHost *e, uint32_t size);
void pcie_host_mmcfg_map(PCIExpressHost *e, hwaddr addr, uint32_t size);
void pcie_host_mmcfg_update(PCIExpressHost *e,
                            int enable,
                            hwaddr addr,
                            uint32_t size);

#define PCIE_MMCFG_SIZE_MAX             (1ULL << 28)
#define PCIE_MMCFG_SIZE_MIN             (1ULL << 20)
#define PCIE_MMCFG_BUS_BIT              20
#define PCIE_MMCFG_BUS_MASK             0xff
#define PCIE_MMCFG_DEVFN_BIT            12
#define PCIE_MMCFG_DEVFN_MASK           0xff
#define PCIE_MMCFG_CONFOFFSET_MASK      0xfff
#define PCIE_MMCFG_BUS(addr)            (((addr) >> PCIE_MMCFG_BUS_BIT) & \
                                         PCIE_MMCFG_BUS_MASK)
#define PCIE_MMCFG_DEVFN(addr)          (((addr) >> PCIE_MMCFG_DEVFN_BIT) & \
                                         PCIE_MMCFG_DEVFN_MASK)
#define PCIE_MMCFG_CONFOFFSET(addr)     ((addr) & PCIE_MMCFG_CONFOFFSET_MASK)

#endif /* PCIE_HOST_H */
