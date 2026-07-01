
#include "qemu/osdep.h"
#include "hw/acpi/aml-build.h"
#include "hw/acpi/pci.h"
#include "hw/pci/pcie_host.h"

void build_mcfg(GArray *table_data, BIOSLinker *linker, AcpiMcfgInfo *info,
                const char *oem_id, const char *oem_table_id)
{
    AcpiTable table = { .sig = "MCFG", .rev = 1,
                        .oem_id = oem_id, .oem_table_id = oem_table_id };

    acpi_table_begin(&table, table_data);

    build_append_int_noprefix(table_data, 0, 8);
    build_append_int_noprefix(table_data, info->base, 8);
    build_append_int_noprefix(table_data, 0, 2);
    build_append_int_noprefix(table_data, 0, 1);
    build_append_int_noprefix(table_data, PCIE_MMCFG_BUS(info->size - 1), 1);
    build_append_int_noprefix(table_data, 0, 4);

    acpi_table_end(linker, &table);
}
