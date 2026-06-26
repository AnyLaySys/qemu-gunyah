
#include "qemu/osdep.h"
#include "trace/control.h"


void trace_event_set_state_dynamic_init(TraceEvent *ev, bool state)
{
    trace_event_set_state_dynamic(ev, state);
}

void trace_event_set_state_dynamic(TraceEvent *ev, bool state)
{
    bool state_pre;
    assert(trace_event_get_state_static(ev));

    state_pre = *(ev->dstate);
    if (state_pre != state) {
        if (state) {
            trace_events_enabled_count++;
            *(ev->dstate) = 1;
        } else {
            trace_events_enabled_count--;
            *(ev->dstate) = 0;
        }
    }
}
