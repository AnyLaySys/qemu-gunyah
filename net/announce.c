
#include "qemu/osdep.h"
#include "net/announce.h"
#include "qapi/clone-visitor.h"
#include "qapi/qapi-visit-net.h"
#include "trace.h"

int64_t qemu_announce_timer_step(AnnounceTimer *timer)
{
    int64_t step;

    step =  timer->params.initial +
            (timer->params.rounds - timer->round - 1) *
            timer->params.step;

    if (step < 0 || step > timer->params.max) {
        step = timer->params.max;
    }
    timer_mod(timer->tm, qemu_clock_get_ms(timer->type) + step);

    return step;
}

void qemu_announce_timer_del(AnnounceTimer *timer)
{
    if (timer->tm) {
        timer_free(timer->tm);
        timer->tm = NULL;
    }
    qapi_free_strList(timer->params.interfaces);
    timer->params.interfaces = NULL;
    trace_qemu_announce_timer_del(timer->params.id);
    g_free(timer->params.id);
    timer->params.id = NULL;
}

void qemu_announce_timer_reset(AnnounceTimer *timer,
                               AnnounceParameters *params,
                               QEMUClockType type,
                               QEMUTimerCB *cb,
                               void *opaque)
{
    qemu_announce_timer_del(timer);

    QAPI_CLONE_MEMBERS(AnnounceParameters, &timer->params, params);
    timer->round = params->rounds;
    timer->type = type;
    timer->tm = timer_new_ms(type, cb, opaque);
}
