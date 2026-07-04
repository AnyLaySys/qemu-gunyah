#ifndef PTIMER_H
#define PTIMER_H

#include "qemu/timer.h"


#define PTIMER_POLICY_LEGACY                0

#define PTIMER_POLICY_WRAP_AFTER_ONE_PERIOD (1 << 0)

#define PTIMER_POLICY_CONTINUOUS_TRIGGER    (1 << 1)

#define PTIMER_POLICY_NO_IMMEDIATE_TRIGGER  (1 << 2)

#define PTIMER_POLICY_NO_IMMEDIATE_RELOAD   (1 << 3)

#define PTIMER_POLICY_NO_COUNTER_ROUND_DOWN (1 << 4)

#define PTIMER_POLICY_TRIGGER_ONLY_ON_DECREMENT (1 << 5)

typedef struct ptimer_state ptimer_state;
typedef void (*ptimer_cb)(void *opaque);

ptimer_state *ptimer_init(ptimer_cb callback,
                          void *callback_opaque,
                          uint8_t policy_mask);

void ptimer_free(ptimer_state *s);

void ptimer_transaction_begin(ptimer_state *s);

void ptimer_transaction_commit(ptimer_state *s);

void ptimer_set_period(ptimer_state *s, int64_t period);

void ptimer_set_period_from_clock(ptimer_state *s, const Clock *clock,
                                  unsigned int divisor);

void ptimer_set_freq(ptimer_state *s, uint32_t freq);

uint64_t ptimer_get_limit(ptimer_state *s);

void ptimer_set_limit(ptimer_state *s, uint64_t limit, int reload);

uint64_t ptimer_get_count(ptimer_state *s);

void ptimer_set_count(ptimer_state *s, uint64_t count);

void ptimer_run(ptimer_state *s, int oneshot);

void ptimer_stop(ptimer_state *s);

extern const VMStateDescription vmstate_ptimer;

#define VMSTATE_PTIMER(_field, _state) \
    VMSTATE_STRUCT_POINTER_V(_field, _state, 1, vmstate_ptimer, ptimer_state)

#define VMSTATE_PTIMER_ARRAY(_f, _s, _n)                                \
    VMSTATE_ARRAY_OF_POINTER_TO_STRUCT(_f, _s, _n, 0,                   \
                                       vmstate_ptimer, ptimer_state)

#endif
