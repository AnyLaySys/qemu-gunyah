
#include "qemu/osdep.h"
#include "hw/ptimer.h"
#include "migration/vmstate.h"
#include "qemu/host-utils.h"
#include "system/cpu-timers.h"
#include "block/aio.h"
#include "hw/clock.h"

#define DELTA_ADJUST     1
#define DELTA_NO_ADJUST -1

struct ptimer_state
{
    uint8_t enabled; /* 0 = disabled, 1 = periodic, 2 = oneshot.  */
    uint64_t limit;
    uint64_t delta;
    uint32_t period_frac;
    int64_t period;
    int64_t last_event;
    int64_t next_event;
    uint8_t policy_mask;
    QEMUTimer *timer;
    ptimer_cb callback;
    void *callback_opaque;
    bool in_transaction;
    bool need_reload;
};

static void ptimer_trigger(ptimer_state *s)
{
    s->callback(s->callback_opaque);
}

static void ptimer_reload(ptimer_state *s, int delta_adjust)
{
    uint32_t period_frac;
    uint64_t period;
    uint64_t delta;
    bool suppress_trigger = false;

    if (delta_adjust == 0 &&
        (s->policy_mask & PTIMER_POLICY_TRIGGER_ONLY_ON_DECREMENT)) {
        suppress_trigger = true;
    }
    if (s->delta == 0 && !(s->policy_mask & PTIMER_POLICY_NO_IMMEDIATE_TRIGGER)
        && !suppress_trigger) {
        ptimer_trigger(s);
    }

    delta = s->delta;
    period = s->period;
    period_frac = s->period_frac;

    if (delta == 0 && !(s->policy_mask & PTIMER_POLICY_NO_IMMEDIATE_RELOAD)) {
        delta = s->delta = s->limit;
    }

    if (s->period == 0 && s->period_frac == 0) {
        fprintf(stderr, "Timer with period zero, disabling\n");
        timer_del(s->timer);
        s->enabled = 0;
        return;
    }

    if (s->policy_mask & PTIMER_POLICY_WRAP_AFTER_ONE_PERIOD) {
        if (delta_adjust != DELTA_NO_ADJUST) {
            delta += delta_adjust;
        }
    }

    if (delta == 0 && (s->policy_mask & PTIMER_POLICY_CONTINUOUS_TRIGGER)) {
        if (s->enabled == 1 && s->limit == 0) {
            delta = 1;
        }
    }

    if (delta == 0 && (s->policy_mask & PTIMER_POLICY_NO_IMMEDIATE_TRIGGER)) {
        if (delta_adjust != DELTA_NO_ADJUST) {
            delta = 1;
        }
    }

    if (delta == 0 && (s->policy_mask & PTIMER_POLICY_NO_IMMEDIATE_RELOAD)) {
        if (s->enabled == 1 && s->limit != 0) {
            delta = 1;
        }
    }

    if (delta == 0) {
        if (s->enabled == 0) {
            return;
        }
        fprintf(stderr, "Timer with delta zero, disabling\n");
        timer_del(s->timer);
        s->enabled = 0;
        return;
    }


    if (s->enabled == 1 && (delta * period < 10000) && !icount_enabled()) {
        period = 10000 / delta;
        period_frac = 0;
    }

    s->last_event = s->next_event;
    s->next_event = s->last_event + delta * period;
    if (period_frac) {
        s->next_event += ((int64_t)period_frac * delta) >> 32;
    }
    timer_mod(s->timer, s->next_event);
}

static void ptimer_tick(void *opaque)
{
    ptimer_state *s = (ptimer_state *)opaque;
    bool trigger = true;

    ptimer_transaction_begin(s);

    if (s->enabled == 2) {
        s->delta = 0;
        s->enabled = 0;
    } else {
        int delta_adjust = DELTA_ADJUST;

        if (s->delta == 0 || s->limit == 0) {
            delta_adjust = DELTA_NO_ADJUST;
        }

        if (!(s->policy_mask & PTIMER_POLICY_NO_IMMEDIATE_TRIGGER)) {
            trigger = (delta_adjust == DELTA_ADJUST);
        }

        s->delta = s->limit;

        ptimer_reload(s, delta_adjust);
    }

    if (trigger) {
        ptimer_trigger(s);
    }

    ptimer_transaction_commit(s);
}

uint64_t ptimer_get_count(ptimer_state *s)
{
    uint64_t counter;

    if (s->enabled && s->delta != 0) {
        int64_t now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
        int64_t next = s->next_event;
        int64_t last = s->last_event;
        bool expired = (now - next >= 0);
        bool oneshot = (s->enabled == 2);

        if (expired) {
            counter = 0;
        } else {
            uint64_t rem;
            uint64_t div;
            int clz1, clz2;
            int shift;
            uint32_t period_frac = s->period_frac;
            uint64_t period = s->period;

            if (!oneshot && (s->delta * period < 10000) &&
                !icount_enabled()) {
                period = 10000 / s->delta;
                period_frac = 0;
            }


            rem = next - now;
            div = period;

            clz1 = clz64(rem);
            clz2 = clz64(div);
            shift = clz1 < clz2 ? clz1 : clz2;

            rem <<= shift;
            div <<= shift;
            if (shift >= 32) {
                div |= ((uint64_t)period_frac << (shift - 32));
            } else {
                if (shift != 0)
                    div |= (period_frac >> (32 - shift));
                if ((uint32_t)(period_frac << shift))
                    div += 1;
            }
            counter = rem / div;

            if (s->policy_mask & PTIMER_POLICY_WRAP_AFTER_ONE_PERIOD) {
                if (!oneshot && s->delta == s->limit) {
                    if (now == last) {
                        if (counter == s->limit + DELTA_ADJUST) {
                            return 0;
                        }
                    } else if (counter == s->limit) {
                        return 0;
                    }
                }
            }
        }

        if (s->policy_mask & PTIMER_POLICY_NO_COUNTER_ROUND_DOWN) {
            if (now != last) {
                counter += 1;
            }
        }
    } else {
        counter = s->delta;
    }
    return counter;
}

void ptimer_set_count(ptimer_state *s, uint64_t count)
{
    assert(s->in_transaction);
    s->delta = count;
    if (s->enabled) {
        s->need_reload = true;
    }
}

void ptimer_run(ptimer_state *s, int oneshot)
{
    bool was_disabled = !s->enabled;

    assert(s->in_transaction);

    if (was_disabled && s->period == 0 && s->period_frac == 0) {
        fprintf(stderr, "Timer with period zero, disabling\n");
        return;
    }
    s->enabled = oneshot ? 2 : 1;
    if (was_disabled) {
        s->need_reload = true;
    }
}

void ptimer_stop(ptimer_state *s)
{
    assert(s->in_transaction);

    if (!s->enabled)
        return;

    s->delta = ptimer_get_count(s);
    timer_del(s->timer);
    s->enabled = 0;
    s->need_reload = false;
}

void ptimer_set_period(ptimer_state *s, int64_t period)
{
    assert(s->in_transaction);
    s->delta = ptimer_get_count(s);
    s->period = period;
    s->period_frac = 0;
    if (s->enabled) {
        s->need_reload = true;
    }
}

void ptimer_set_period_from_clock(ptimer_state *s, const Clock *clk,
                                  unsigned int divisor)
{
    uint64_t raw_period = clock_get(clk);
    uint64_t period_frac;

    assert(s->in_transaction);
    s->delta = ptimer_get_count(s);
    s->period = extract64(raw_period, 32, 32);
    period_frac = extract64(raw_period, 0, 32);
    s->period *= divisor;
    period_frac *= divisor;
    s->period += extract64(period_frac, 32, 32);
    s->period_frac = (uint32_t)period_frac;

    if (s->enabled) {
        s->need_reload = true;
    }
}

void ptimer_set_freq(ptimer_state *s, uint32_t freq)
{
    assert(s->in_transaction);
    s->delta = ptimer_get_count(s);
    s->period = 1000000000ll / freq;
    s->period_frac = (1000000000ll << 32) / freq;
    if (s->enabled) {
        s->need_reload = true;
    }
}

void ptimer_set_limit(ptimer_state *s, uint64_t limit, int reload)
{
    assert(s->in_transaction);
    s->limit = limit;
    if (reload)
        s->delta = limit;
    if (s->enabled && reload) {
        s->need_reload = true;
    }
}

uint64_t ptimer_get_limit(ptimer_state *s)
{
    return s->limit;
}

void ptimer_transaction_begin(ptimer_state *s)
{
    assert(!s->in_transaction);
    s->in_transaction = true;
    s->need_reload = false;
}

void ptimer_transaction_commit(ptimer_state *s)
{
    assert(s->in_transaction);
    while (s->need_reload && s->enabled) {
        s->need_reload = false;
        s->next_event = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
        ptimer_reload(s, 0);
    }
    s->in_transaction = false;
}

const VMStateDescription vmstate_ptimer = {
    .name = "ptimer",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT8(enabled, ptimer_state),
        VMSTATE_UINT64(limit, ptimer_state),
        VMSTATE_UINT64(delta, ptimer_state),
        VMSTATE_UINT32(period_frac, ptimer_state),
        VMSTATE_INT64(period, ptimer_state),
        VMSTATE_INT64(last_event, ptimer_state),
        VMSTATE_INT64(next_event, ptimer_state),
        VMSTATE_TIMER_PTR(timer, ptimer_state),
        VMSTATE_END_OF_LIST()
    }
};

ptimer_state *ptimer_init(ptimer_cb callback, void *callback_opaque,
                          uint8_t policy_mask)
{
    ptimer_state *s;

    assert(callback);

    s = g_new0(ptimer_state, 1);
    s->timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, ptimer_tick, s);
    s->policy_mask = policy_mask;
    s->callback = callback;
    s->callback_opaque = callback_opaque;

    assert(!((policy_mask & PTIMER_POLICY_TRIGGER_ONLY_ON_DECREMENT) &&
             (policy_mask & PTIMER_POLICY_NO_IMMEDIATE_TRIGGER)));
    return s;
}

void ptimer_free(ptimer_state *s)
{
    timer_free(s->timer);
    g_free(s);
}
