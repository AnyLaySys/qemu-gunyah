
#ifndef QEMU_PCIE_SRIOV_H
#define QEMU_PCIE_SRIOV_H

#include "hw/pci/pci.h"

typedef struct PCIESriovPF {
    uint8_t vf_bar_type[PCI_NUM_REGIONS];   /* Store type for each VF bar */
    PCIDevice **vf;     /* Pointer to an array of num_vfs VF devices */
} PCIESriovPF;

typedef struct PCIESriovVF {
    PCIDevice *pf;      /* Pointer back to owner physical function */
    uint16_t vf_number; /* Logical VF number of this function */
} PCIESriovVF;

bool pcie_sriov_pf_init(PCIDevice *dev, uint16_t offset,
                        const char *vfname, uint16_t vf_dev_id,
                        uint16_t init_vfs, uint16_t total_vfs,
                        uint16_t vf_offset, uint16_t vf_stride,
                        Error **errp);
void pcie_sriov_pf_exit(PCIDevice *dev);

void pcie_sriov_pf_init_vf_bar(PCIDevice *dev, int region_num,
                               uint8_t type, dma_addr_t size);

void pcie_sriov_vf_register_bar(PCIDevice *dev, int region_num,
                                MemoryRegion *memory);

#define SRIOV_SUP_PGSIZE_MINREQ 0x553

void pcie_sriov_pf_add_sup_pgsize(PCIDevice *dev, uint16_t opt_sup_pgsize);

void pcie_sriov_config_write(PCIDevice *dev, uint32_t address,
                             uint32_t val, int len);

void pcie_sriov_pf_post_load(PCIDevice *dev);

void pcie_sriov_pf_reset(PCIDevice *dev);

uint16_t pcie_sriov_vf_number(PCIDevice *dev);

PCIDevice *pcie_sriov_get_pf(PCIDevice *dev);

PCIDevice *pcie_sriov_get_vf_at_index(PCIDevice *dev, int n);

uint16_t pcie_sriov_num_vfs(PCIDevice *dev);

#endif /* QEMU_PCIE_SRIOV_H */
