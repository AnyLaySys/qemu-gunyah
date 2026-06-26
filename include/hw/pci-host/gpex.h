
#ifndef HW_GPEX_H
#define HW_GPEX_H

#include "exec/hwaddr.h"
#include "hw/sysbus.h"
#include "hw/pci/pci_device.h"
#include "hw/pci/pcie_host.h"
#include "qom/object.h"

#define TYPE_GPEX_HOST "gpex-pcihost"
OBJECT_DECLARE_SIMPLE_TYPE(GPEXHost, GPEX_HOST)

#define TYPE_GPEX_ROOT_DEVICE "gpex-root"
OBJECT_DECLARE_SIMPLE_TYPE(GPEXRootState, GPEX_ROOT_DEVICE)

struct GPEXRootState {
    PCIDevice parent_obj;
};

struct GPEXConfig {
    MemMapEntry ecam;
    MemMapEntry mmio32;
    MemMapEntry mmio64;
    MemMapEntry pio;
    int         irq;
    PCIBus      *bus;
};

typedef struct GPEXIrq GPEXIrq;
struct GPEXHost {
    PCIExpressHost parent_obj;

    GPEXRootState gpex_root;

    MemoryRegion io_ioport;
    MemoryRegion io_mmio;
    MemoryRegion io_ioport_window;
    MemoryRegion io_mmio_window;
    GPEXIrq *irq;
    uint8_t num_irqs;

    bool allow_unmapped_accesses;

    struct GPEXConfig gpex_cfg;
};

int gpex_set_irq_num(GPEXHost *s, int index, int gsi);

void acpi_dsdt_add_gpex(Aml *scope, struct GPEXConfig *cfg);
void acpi_dsdt_add_gpex_host(Aml *scope, uint32_t irq);

#define PCI_HOST_PIO_BASE               "x-pio-base"
#define PCI_HOST_PIO_SIZE               "x-pio-size"
#define PCI_HOST_ECAM_BASE              "x-ecam-base"
#define PCI_HOST_ECAM_SIZE              "x-ecam-size"
#define PCI_HOST_BELOW_4G_MMIO_BASE     "x-below-4g-mmio-base"
#define PCI_HOST_BELOW_4G_MMIO_SIZE     "x-below-4g-mmio-size"
#define PCI_HOST_ABOVE_4G_MMIO_BASE     "x-above-4g-mmio-base"
#define PCI_HOST_ABOVE_4G_MMIO_SIZE     "x-above-4g-mmio-size"

#endif /* HW_GPEX_H */
