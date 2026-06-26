
#include "qemu/osdep.h"
#include "hw/acpi/aml-build.h"
#include "hw/acpi/utils.h"
#include "hw/loader.h"

MemoryRegion *acpi_add_rom_blob(FWCfgCallback update, void *opaque,
                                GArray *blob, const char *name)
{
    uint64_t max_size;

    if (!strcmp(name, ACPI_BUILD_TABLE_FILE)) {
        max_size = 0x200000;
    } else if (!strcmp(name, ACPI_BUILD_LOADER_FILE)) {
        max_size = 0x10000;
    } else if (!strcmp(name, ACPI_BUILD_RSDP_FILE)) {
        max_size = 0x1000;
    } else {
        g_assert_not_reached();
    }
    g_assert(acpi_data_len(blob) <= max_size);

    return rom_add_blob(name, blob->data, acpi_data_len(blob), max_size, -1,
                        name, update, opaque, NULL, true);
}
