#ifndef HW_ACPI_IPMI_H
#define HW_ACPI_IPMI_H

#include "hw/acpi/acpi_aml_interface.h"

void build_ipmi_dev_aml(AcpiDevAmlIf *adev, Aml *scope);

#endif /* HW_ACPI_IPMI_H */
