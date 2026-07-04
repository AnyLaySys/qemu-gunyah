
#ifndef QEMU_HW_CLOCK_H
#define QEMU_HW_CLOCK_H

#include "qom/object.h"
#include "qemu/queue.h"
#include "qemu/host-utils.h"
#include "qemu/bitops.h"

#define TYPE_CLOCK "clock"
OBJECT_DECLARE_SIMPLE_TYPE(Clock, CLOCK)

typedef enum ClockEvent {
    ClockUpdate = 1, /* Clock period has just updated */
    ClockPreUpdate = 2, /* Clock period is about to update */
} ClockEvent;

typedef void ClockCallback(void *opaque, ClockEvent event);

#define CLOCK_PERIOD_1SEC (1000000000llu << 32)

#define CLOCK_PERIOD_FROM_NS(ns) ((ns) * (CLOCK_PERIOD_1SEC / 1000000000llu))
#define CLOCK_PERIOD_FROM_HZ(hz) (((hz) != 0) ? CLOCK_PERIOD_1SEC / (hz) : 0u)
#define CLOCK_PERIOD_TO_HZ(per) (((per) != 0) ? CLOCK_PERIOD_1SEC / (per) : 0u)



struct Clock {
    Object parent_obj;


    uint64_t period;
    char *canonical_path;
    ClockCallback *callback;
    void *callback_opaque;
    unsigned int callback_events;

    uint32_t multiplier;
    uint32_t divider;

    Clock *source;
    QLIST_HEAD(, Clock) children;
    QLIST_ENTRY(Clock) sibling;
};

extern const VMStateDescription vmstate_clock;
#define VMSTATE_CLOCK(field, state) \
    VMSTATE_CLOCK_V(field, state, 0)
#define VMSTATE_CLOCK_V(field, state, version) \
    VMSTATE_STRUCT_POINTER_V(field, state, version, vmstate_clock, Clock)
#define VMSTATE_ARRAY_CLOCK(field, state, num) \
    VMSTATE_ARRAY_CLOCK_V(field, state, num, 0)
#define VMSTATE_ARRAY_CLOCK_V(field, state, num, version)          \
    VMSTATE_ARRAY_OF_POINTER_TO_STRUCT(field, state, num, version, \
                                       vmstate_clock, Clock)

void clock_setup_canonical_path(Clock *clk);

Clock *clock_new(Object *parent, const char *name);

void clock_set_callback(Clock *clk, ClockCallback *cb,
                        void *opaque, unsigned int events);

void clock_set_source(Clock *clk, Clock *src);

static inline bool clock_has_source(const Clock *clk)
{
    return clk->source != NULL;
}

bool clock_set(Clock *clk, uint64_t value);

static inline bool clock_set_hz(Clock *clk, unsigned hz)
{
    return clock_set(clk, CLOCK_PERIOD_FROM_HZ(hz));
}

static inline bool clock_set_ns(Clock *clk, unsigned ns)
{
    return clock_set(clk, CLOCK_PERIOD_FROM_NS(ns));
}

void clock_propagate(Clock *clk);

static inline void clock_update(Clock *clk, uint64_t value)
{
    if (clock_set(clk, value)) {
        clock_propagate(clk);
    }
}

static inline void clock_update_hz(Clock *clk, unsigned hz)
{
    clock_update(clk, CLOCK_PERIOD_FROM_HZ(hz));
}

static inline void clock_update_ns(Clock *clk, unsigned ns)
{
    clock_update(clk, CLOCK_PERIOD_FROM_NS(ns));
}

static inline uint64_t clock_get(const Clock *clk)
{
    return clk->period;
}

static inline unsigned clock_get_hz(Clock *clk)
{
    return CLOCK_PERIOD_TO_HZ(clock_get(clk));
}

static inline uint64_t clock_ticks_to_ns(const Clock *clk, uint64_t ticks)
{
    uint64_t ns_low, ns_high;

    mulu64(&ns_low, &ns_high, clk->period, ticks);
    if (ns_high & MAKE_64BIT_MASK(31, 33)) {
        return INT64_MAX;
    }
    return ns_low >> 32 | ns_high << 32;
}

static inline uint64_t clock_ns_to_ticks(const Clock *clk, uint64_t ns)
{
    uint64_t lo = ns << 32;
    uint64_t hi = ns >> 32;
    if (clk->period == 0) {
        return 0;
    }

    divu128(&lo, &hi, clk->period);
    return lo;
}

static inline bool clock_is_enabled(const Clock *clk)
{
    return clock_get(clk) != 0;
}

char *clock_display_freq(Clock *clk);

bool clock_set_mul_div(Clock *clk, uint32_t multiplier, uint32_t divider);

#endif /* QEMU_HW_CLOCK_H */
