
#ifndef HW_PL011_H
#define HW_PL011_H

#include "hw/sysbus.h"
#include "chardev/char-fe.h"
#include "qom/object.h"

#define TYPE_PL011 "pl011"
OBJECT_DECLARE_SIMPLE_TYPE(PL011State, PL011)

#define TYPE_PL011_LUMINARY "pl011_luminary"

#define PL011_FIFO_DEPTH 16

struct PL011State {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    uint32_t flags;
    uint32_t lcr;
    uint32_t rsr;
    uint32_t cr;
    uint32_t dmacr;
    uint32_t int_enabled;
    uint32_t int_level;
    uint32_t read_fifo[PL011_FIFO_DEPTH];
    uint32_t ilpr;
    uint32_t ibrd;
    uint32_t fbrd;
    uint32_t ifl;
    int read_pos;
    int read_count;
    int read_trigger;
    CharBackend chr;
    qemu_irq irq[6];
    Clock *clk;
    bool migrate_clk;
    const unsigned char *id;
    uint8_t padding_for_rust[16];
};

DeviceState *pl011_create(hwaddr addr, qemu_irq irq, Chardev *chr);

#endif
