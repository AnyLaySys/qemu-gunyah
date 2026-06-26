
#ifndef TRACE__EVENT_INTERNAL_H
#define TRACE__EVENT_INTERNAL_H

#define TRACE_VCPU_EVENT_NONE ((uint32_t)-1)

typedef struct TraceEvent {
    uint32_t id;
    const char * name;
    const bool sstate;
    uint16_t *dstate;
} TraceEvent;

void trace_event_set_state_dynamic_init(TraceEvent *ev, bool state);

#endif /* TRACE__EVENT_INTERNAL_H */
