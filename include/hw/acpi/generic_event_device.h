
#ifndef HW_ACPI_GENERIC_EVENT_DEVICE_H
#define HW_ACPI_GENERIC_EVENT_DEVICE_H

#include "hw/sysbus.h"
#include "hw/acpi/ghes.h"
#include "hw/acpi/cpu.h"
#include "qom/object.h"

#define ACPI_POWER_BUTTON_DEVICE "PWRB"

#define TYPE_ACPI_GED "acpi-ged"
OBJECT_DECLARE_SIMPLE_TYPE(AcpiGedState, ACPI_GED)

#define ACPI_GED_EVT_SEL_OFFSET    0x0
#define ACPI_GED_EVT_SEL_LEN       0x4

#define ACPI_GED_REG_SLEEP_CTL     0x00
#define ACPI_GED_REG_SLEEP_STS     0x01
#define ACPI_GED_REG_RESET         0x02
#define ACPI_GED_REG_COUNT         0x03

#define ACPI_GED_RESET_VALUE       0x42

#define ACPI_GED_SLP_TYP_POS       0x2   /* SLP_TYPx Bit Offset */
#define ACPI_GED_SLP_TYP_MASK      0x07  /* SLP_TYPx 3-bit mask */
#define ACPI_GED_SLP_TYP_S5        0x05  /* System _S5 State (Soft Off) */
#define ACPI_GED_SLP_EN            0x20  /* SLP_EN write-only bit */

#define GED_DEVICE      "GED"
#define AML_GED_EVT_REG "EREG"
#define AML_GED_EVT_SEL "ESEL"
#define AML_GED_EVT_CPU_SCAN_METHOD "\\_SB.GED.CSCN"

#define ACPI_GED_PWR_DOWN_EVT      0x2
#define ACPI_GED_CPU_HOTPLUG_EVT    0x8

typedef struct GEDState {
    MemoryRegion evt;
    MemoryRegion regs;
    uint32_t     sel;
} GEDState;

struct AcpiGedState {
    SysBusDevice parent_obj;
    CPUHotplugState cpuhp_state;
    MemoryRegion container_cpuhp;
    GEDState ged_state;
    uint32_t ged_event_bitmap;
    qemu_irq irq;
    AcpiGhesState ghes_state;
};

void build_ged_aml(Aml *table, const char* name, HotplugHandler *hotplug_dev,
                   uint32_t ged_irq, AmlRegionSpace rs, hwaddr ged_base);
void acpi_dsdt_add_power_button(Aml *scope);

#endif
