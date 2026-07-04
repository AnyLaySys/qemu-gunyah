
#ifndef QEMU_MSI_H
#define QEMU_MSI_H

#include "hw/pci/pci_device.h"

struct MSIMessage {
    uint64_t address;
    uint32_t data;
};

extern bool msi_nonbroken;

void msi_set_message(PCIDevice *dev, MSIMessage msg);
MSIMessage msi_get_message(PCIDevice *dev, unsigned int vector);
bool msi_enabled(const PCIDevice *dev);
void msi_set_enabled(PCIDevice *dev);
int msi_init(struct PCIDevice *dev, uint8_t offset,
             unsigned int nr_vectors, bool msi64bit,
             bool msi_per_vector_mask, Error **errp);
void msi_uninit(struct PCIDevice *dev);
void msi_reset(PCIDevice *dev);
bool msi_is_masked(const PCIDevice *dev, unsigned int vector);
void msi_notify(PCIDevice *dev, unsigned int vector);
void msi_send_message(PCIDevice *dev, MSIMessage msg);
void msi_write_config(PCIDevice *dev, uint32_t addr, uint32_t val, int len);
unsigned int msi_nr_vectors_allocated(const PCIDevice *dev);
void msi_set_mask(PCIDevice *dev, int vector, bool mask, Error **errp);

static inline bool msi_present(const PCIDevice *dev)
{
    return dev->cap_present & QEMU_PCI_CAP_MSI;
}

#endif /* QEMU_MSI_H */
