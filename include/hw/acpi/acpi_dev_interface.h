#ifndef ACPI_DEV_INTERFACE_H
#define ACPI_DEV_INTERFACE_H

#include "qapi/qapi-types-acpi.h"
#include "qom/object.h"
#include "hw/qdev-core.h"

typedef enum {
    ACPI_PCI_HOTPLUG_STATUS = 2,
    ACPI_CPU_HOTPLUG_STATUS = 4,
    ACPI_MEMORY_HOTPLUG_STATUS = 8,
    ACPI_VMGENID_CHANGE_STATUS = 32,
    ACPI_POWER_DOWN_STATUS = 64,
} AcpiEventStatusBits;

#define TYPE_ACPI_DEVICE_IF "acpi-device-interface"

typedef struct AcpiDeviceIfClass AcpiDeviceIfClass;
DECLARE_CLASS_CHECKERS(AcpiDeviceIfClass, ACPI_DEVICE_IF,
                       TYPE_ACPI_DEVICE_IF)
#define ACPI_DEVICE_IF(obj) \
     INTERFACE_CHECK(AcpiDeviceIf, (obj), \
                     TYPE_ACPI_DEVICE_IF)

typedef struct AcpiDeviceIf AcpiDeviceIf;

void acpi_send_event(DeviceState *dev, AcpiEventStatusBits event);

struct AcpiDeviceIfClass {
    InterfaceClass parent_class;

    void (*ospm_status)(AcpiDeviceIf *adev, ACPIOSTInfoList ***list);
    void (*send_event)(AcpiDeviceIf *adev, AcpiEventStatusBits ev);
};
#endif
