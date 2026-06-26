
#ifndef HW_OR_IRQ_H
#define HW_OR_IRQ_H

#include "hw/sysbus.h"
#include "qom/object.h"

#define TYPE_OR_IRQ "or-irq"

#define MAX_OR_LINES      48

OBJECT_DECLARE_SIMPLE_TYPE(OrIRQState, OR_IRQ)

struct OrIRQState {
    DeviceState parent_obj;

    qemu_irq out_irq;
    bool levels[MAX_OR_LINES];
    uint16_t num_lines;
};

#endif
