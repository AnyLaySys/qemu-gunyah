
#include "qemu/osdep.h"
#include "hw/acpi/ghes.h"

int acpi_ghes_memory_errors(uint16_t source_id, uint64_t physical_address)
{
    return -1;
}

bool acpi_ghes_present(void)
{
    return false;
}
