
#ifndef HW_SERIAL_MM_H
#define HW_SERIAL_MM_H

#include "hw/char/serial.h"
#include "exec/memory.h"
#include "chardev/char.h"
#include "hw/sysbus.h"
#include "qom/object.h"

#define TYPE_SERIAL_MM "serial-mm"
OBJECT_DECLARE_SIMPLE_TYPE(SerialMM, SERIAL_MM)

struct SerialMM {
    SysBusDevice parent;

    SerialState serial;

    uint8_t regshift;
    uint8_t endianness;
};

SerialMM *serial_mm_init(MemoryRegion *address_space,
                         hwaddr base, int regshift,
                         qemu_irq irq, int baudbase,
                         Chardev *chr, enum device_endian end);

#endif
