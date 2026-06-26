
#ifndef ACPI_GHES_H
#define ACPI_GHES_H

#include "hw/acpi/bios-linker-loader.h"
#include "qapi/error.h"

enum AcpiGhesNotifyType {
    ACPI_GHES_NOTIFY_POLLED = 0,
    ACPI_GHES_NOTIFY_EXTERNAL = 1,
    ACPI_GHES_NOTIFY_LOCAL = 2,
    ACPI_GHES_NOTIFY_SCI = 3,
    ACPI_GHES_NOTIFY_NMI = 4,
    ACPI_GHES_NOTIFY_CMCI = 5,
    ACPI_GHES_NOTIFY_MCE = 6,
    ACPI_GHES_NOTIFY_GPIO = 7,
    ACPI_GHES_NOTIFY_SEA = 8,
    ACPI_GHES_NOTIFY_SEI = 9,
    ACPI_GHES_NOTIFY_GSIV = 10,
    ACPI_GHES_NOTIFY_SDEI = 11,
    ACPI_GHES_NOTIFY_RESERVED = 12
};

enum {
    ACPI_HEST_SRC_ID_SEA = 0,

    ACPI_GHES_ERROR_SOURCE_COUNT
};

typedef struct AcpiGhesState {
    uint64_t hw_error_le;
    bool present; /* True if GHES is present at all on this board */
} AcpiGhesState;

void acpi_build_hest(GArray *table_data, GArray *hardware_errors,
                     BIOSLinker *linker,
                     const char *oem_id, const char *oem_table_id);
void acpi_ghes_add_fw_cfg(AcpiGhesState *vms, FWCfgState *s,
                          GArray *hardware_errors);
int acpi_ghes_memory_errors(uint16_t source_id, uint64_t error_physical_addr);

bool acpi_ghes_present(void);
#endif
