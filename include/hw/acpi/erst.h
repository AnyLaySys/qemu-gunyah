#ifndef HW_ACPI_ERST_H
#define HW_ACPI_ERST_H

#include "hw/acpi/bios-linker-loader.h"
#include "qom/object.h"

void build_erst(GArray *table_data, BIOSLinker *linker, Object *erst_dev,
                const char *oem_id, const char *oem_table_id);

#define TYPE_ACPI_ERST "acpi-erst"

static inline Object *find_erst_dev(void)
{
    return object_resolve_path_type("", TYPE_ACPI_ERST, NULL);
}
#endif
