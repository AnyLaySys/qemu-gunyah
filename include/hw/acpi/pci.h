
#ifndef HW_ACPI_PCI_H
#define HW_ACPI_PCI_H

#include "hw/acpi/bios-linker-loader.h"
#include "hw/acpi/acpi_aml_interface.h"

typedef struct AcpiMcfgInfo {
    uint64_t base;
    uint32_t size;
} AcpiMcfgInfo;

void build_mcfg(GArray *table_data, BIOSLinker *linker, AcpiMcfgInfo *info,
                const char *oem_id, const char *oem_table_id);
Aml *aml_pci_device_dsm(void);

void build_append_pci_bus_devices(Aml *parent_scope, PCIBus *bus);

#endif
