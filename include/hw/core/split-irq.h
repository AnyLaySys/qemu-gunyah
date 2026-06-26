
#ifndef HW_SPLIT_IRQ_H
#define HW_SPLIT_IRQ_H

#include "hw/sysbus.h"
#include "qom/object.h"

#define TYPE_SPLIT_IRQ "split-irq"

#define MAX_SPLIT_LINES 16


OBJECT_DECLARE_SIMPLE_TYPE(SplitIRQ, SPLIT_IRQ)

struct SplitIRQ {
    DeviceState parent_obj;

    qemu_irq out_irq[MAX_SPLIT_LINES];
    uint16_t num_lines;
};

#endif
