
#ifndef QDEV_CLOCK_H
#define QDEV_CLOCK_H

#include "hw/clock.h"

Clock *qdev_init_clock_in(DeviceState *dev, const char *name,
                          ClockCallback *callback, void *opaque,
                          unsigned int events);

Clock *qdev_init_clock_out(DeviceState *dev, const char *name);

Clock *qdev_get_clock_in(DeviceState *dev, const char *name);

Clock *qdev_get_clock_out(DeviceState *dev, const char *name);

void qdev_connect_clock_in(DeviceState *dev, const char *name, Clock *source);

Clock *qdev_alias_clock(DeviceState *dev, const char *name,
                        DeviceState *alias_dev, const char *alias_name);

void qdev_finalize_clocklist(DeviceState *dev);

struct ClockPortInitElem {
    const char *name;
    bool is_output;
    ClockCallback *callback;
    unsigned int callback_events;
    size_t offset;
};

#define clock_offset_value(devstate, field) \
    (offsetof(devstate, field) + \
     type_check(Clock *, typeof_field(devstate, field)))

#define QDEV_CLOCK(out_not_in, devstate, field, cb, cbevents) {  \
    .name = (stringify(field)), \
    .is_output = out_not_in, \
    .callback = cb, \
    .callback_events = cbevents, \
    .offset = clock_offset_value(devstate, field), \
}

#define QDEV_CLOCK_IN(devstate, field, callback, cbevents)       \
    QDEV_CLOCK(false, devstate, field, callback, cbevents)

#define QDEV_CLOCK_OUT(devstate, field) \
    QDEV_CLOCK(true, devstate, field, NULL, 0)

#define QDEV_CLOCK_END { .name = NULL }

typedef struct ClockPortInitElem ClockPortInitArray[];

void qdev_init_clocks(DeviceState *dev, const ClockPortInitArray clocks);

#endif /* QDEV_CLOCK_H */
