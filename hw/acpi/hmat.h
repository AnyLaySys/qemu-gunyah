
#ifndef HMAT_H
#define HMAT_H

#include "hw/acpi/bios-linker-loader.h"
#include "system/numa.h"

#define HMAT_PROXIMITY_INITIATOR_VALID  0x1

void build_hmat(GArray *table_data, BIOSLinker *linker, NumaState *numa_state,
                const char *oem_id, const char *oem_table_id);

#endif
