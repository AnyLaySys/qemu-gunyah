
#include "qemu/osdep.h"
#include "hw/acpi/acpi.h"

char unsigned *acpi_tables;
size_t acpi_tables_len;

void acpi_table_add(const QemuOpts *opts, Error **errp)
{
    g_assert_not_reached();
}

bool acpi_builtin(void)
{
    return false;
}
