
#ifndef TRACE__CONTROL_INTERNAL_H
#define TRACE__CONTROL_INTERNAL_H

extern int trace_events_enabled_count;


static inline bool trace_event_is_pattern(const char *str)
{
    assert(str != NULL);
    return strchr(str, '*') != NULL;
}

static inline uint32_t trace_event_get_id(TraceEvent *ev)
{
    assert(ev != NULL);
    return ev->id;
}

static inline const char * trace_event_get_name(TraceEvent *ev)
{
    assert(ev != NULL);
    return ev->name;
}

static inline bool trace_event_get_state_static(TraceEvent *ev)
{
    assert(ev != NULL);
    return ev->sstate;
}

#define trace_event_get_state_dynamic_by_id(id) \
    (unlikely(trace_events_enabled_count) && _ ## id ## _DSTATE)

static inline bool trace_event_get_state_dynamic(TraceEvent *ev)
{
    return unlikely(trace_events_enabled_count) && *ev->dstate;
}

void trace_event_register_group(TraceEvent **events);

#endif /* TRACE__CONTROL_INTERNAL_H */
