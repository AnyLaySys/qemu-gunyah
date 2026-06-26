
#ifndef TRACE__CONTROL_H
#define TRACE__CONTROL_H

#include "event-internal.h"

typedef struct TraceEventIter {
    size_t event;
    size_t group;
    size_t group_id;
    const char *pattern;
} TraceEventIter;


void trace_event_iter_init_all(TraceEventIter *iter);

void trace_event_iter_init_pattern(TraceEventIter *iter, const char *pattern);

void trace_event_iter_init_group(TraceEventIter *iter, size_t group_id);

TraceEvent *trace_event_iter_next(TraceEventIter *iter);


TraceEvent *trace_event_name(const char *name);

static bool trace_event_is_pattern(const char *str);


static uint32_t trace_event_get_id(TraceEvent *ev);

static const char * trace_event_get_name(TraceEvent *ev);

#define trace_event_get_state(id)                       \
    ((id ##_ENABLED) && trace_event_get_state_dynamic_by_id(id))

#define trace_event_get_state_backends(id)              \
    ((id ##_ENABLED) && id ##_BACKEND_DSTATE())

static bool trace_event_get_state_static(TraceEvent *ev);

static bool trace_event_get_state_dynamic(TraceEvent *ev);

void trace_event_set_state_dynamic(TraceEvent *ev, bool state);

bool trace_init_backends(void);

void trace_init_file(void);

void trace_list_events(FILE *f);

void trace_enable_events(const char *line_buf);

extern QemuOptsList qemu_trace_opts;

void trace_opt_parse(const char *optstr);

uint32_t trace_get_vcpu_event_count(void);


#include "control-internal.h"

#endif /* TRACE__CONTROL_H */
