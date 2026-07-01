
#ifndef QEMU_PCIE_H
#define QEMU_PCIE_H

#include "hw/pci/pci_regs.h"
#include "hw/pci/pcie_regs.h"
#include "hw/pci/pcie_aer.h"

struct PCIExpressDevice {
    uint8_t exp_cap;

    uint16_t aer_cap;
    PCIEAERLog aer_log;

    uint16_t ats_cap;

    uint16_t acs_cap;
};

int pcie_cap_init(PCIDevice *dev, uint8_t offset, uint8_t type,
                  uint8_t port, Error **errp);
int pcie_cap_v1_init(PCIDevice *dev, uint8_t offset,
                     uint8_t type, uint8_t port);
int pcie_endpoint_cap_init(PCIDevice *dev, uint8_t offset);
void pcie_cap_exit(PCIDevice *dev);
int pcie_endpoint_cap_v1_init(PCIDevice *dev, uint8_t offset);
void pcie_cap_v1_exit(PCIDevice *dev);
uint8_t pcie_cap_get_type(const PCIDevice *dev);
uint8_t pcie_cap_get_version(const PCIDevice *dev);
void pcie_cap_flags_set_vector(PCIDevice *dev, uint8_t vector);
uint8_t pcie_cap_flags_get_vector(PCIDevice *dev);

void pcie_cap_deverr_init(PCIDevice *dev);
void pcie_cap_deverr_reset(PCIDevice *dev);

void pcie_cap_lnkctl_init(PCIDevice *dev);
void pcie_cap_lnkctl_reset(PCIDevice *dev);

void pcie_cap_root_init(PCIDevice *dev);
void pcie_cap_root_reset(PCIDevice *dev);

void pcie_cap_flr_init(PCIDevice *dev);
void pcie_cap_flr_write_config(PCIDevice *dev,
                           uint32_t addr, uint32_t val, int len);

void pcie_cap_arifwd_init(PCIDevice *dev);
void pcie_cap_arifwd_reset(PCIDevice *dev);
bool pcie_cap_is_arifwd_enabled(const PCIDevice *dev);

uint16_t pcie_find_capability(PCIDevice *dev, uint16_t cap_id);
void pcie_add_capability(PCIDevice *dev,
                         uint16_t cap_id, uint8_t cap_ver,
                         uint16_t offset, uint16_t size);
void pcie_acs_init(PCIDevice *dev, uint16_t offset);
void pcie_acs_reset(PCIDevice *dev);

void pcie_ari_init(PCIDevice *dev, uint16_t offset);
void pcie_dev_ser_num_init(PCIDevice *dev, uint16_t offset, uint64_t ser_num);
void pcie_ats_init(PCIDevice *dev, uint16_t offset, bool aligned);
void pcie_cap_fill_link_ep_usp(PCIDevice *dev, PCIExpLinkWidth width,
                               PCIExpLinkSpeed speed);

#endif /* QEMU_PCIE_H */
