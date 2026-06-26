
#include "qemu/osdep.h"
#include "migration/vmstate.h"
#include "hw/clock.h"

static bool muldiv_needed(void *opaque)
{
    Clock *clk = opaque;

    return clk->multiplier != 1 || clk->divider != 1;
}

static int clock_pre_load(void *opaque)
{
    Clock *clk = opaque;
    clk->multiplier = 1;
    clk->divider = 1;

    return 0;
}

const VMStateDescription vmstate_muldiv = {
    .name = "clock/muldiv",
    .version_id = 1,
    .minimum_version_id = 1,
    .needed = muldiv_needed,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(multiplier, Clock),
        VMSTATE_UINT32(divider, Clock),
        VMSTATE_END_OF_LIST()
    },
};

const VMStateDescription vmstate_clock = {
    .name = "clock",
    .version_id = 0,
    .minimum_version_id = 0,
    .pre_load = clock_pre_load,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT64(period, Clock),
        VMSTATE_END_OF_LIST()
    },
    .subsections = (const VMStateDescription * const []) {
        &vmstate_muldiv,
        NULL
    },
};
