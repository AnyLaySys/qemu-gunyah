
#ifndef HW_RTC_PL031_H
#define HW_RTC_PL031_H

#include "hw/sysbus.h"
#include "qemu/timer.h"
#include "qom/object.h"

#define TYPE_PL031 "pl031"
OBJECT_DECLARE_SIMPLE_TYPE(PL031State, PL031)

struct PL031State {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    QEMUTimer *timer;
    qemu_irq irq;

    uint32_t tick_offset_vmstate;
    uint32_t tick_offset;
    bool tick_offset_migrated;
    bool migrate_tick_offset;

    uint32_t mr;
    uint32_t lr;
    uint32_t cr;
    uint32_t im;
    uint32_t is;
};

#endif
