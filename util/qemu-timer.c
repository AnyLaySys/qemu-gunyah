
#include "qemu/osdep.h"
#include "qemu/main-loop.h"
#include "qemu/timer.h"
#include "qemu/lockable.h"
#include "system/cpu-timers.h"
#include "system/cpus.h"

#ifdef CONFIG_POSIX
#include <pthread.h>
#endif

#ifdef CONFIG_PPOLL
#include <poll.h>
#endif

#ifdef CONFIG_PRCTL_PR_SET_TIMERSLACK
#include <sys/prctl.h>
#endif


typedef struct QEMUClock {
    QLIST_HEAD(, QEMUTimerList) timerlists;

    QEMUClockType type;
    bool enabled;
} QEMUClock;

QEMUTimerListGroup main_loop_tlg;
static QEMUClock qemu_clocks[QEMU_CLOCK_MAX];


struct QEMUTimerList {
    QEMUClock *clock;
    QemuMutex active_timers_lock;
    QEMUTimer *active_timers;
    QLIST_ENTRY(QEMUTimerList) list;
    QEMUTimerListNotifyCB *notify_cb;
    void *notify_opaque;

    QemuEvent timers_done_ev;
};

static inline QEMUClock *qemu_clock_ptr(QEMUClockType type)
{
    return &qemu_clocks[type];
}

static bool timer_expired_ns(QEMUTimer *timer_head, int64_t current_time)
{
    return timer_head && (timer_head->expire_time <= current_time);
}

QEMUTimerList *timerlist_new(QEMUClockType type,
                             QEMUTimerListNotifyCB *cb,
                             void *opaque)
{
    QEMUTimerList *timer_list;
    QEMUClock *clock = qemu_clock_ptr(type);

    timer_list = g_new0(QEMUTimerList, 1);
    qemu_event_init(&timer_list->timers_done_ev, true);
    timer_list->clock = clock;
    timer_list->notify_cb = cb;
    timer_list->notify_opaque = opaque;
    qemu_mutex_init(&timer_list->active_timers_lock);
    QLIST_INSERT_HEAD(&clock->timerlists, timer_list, list);
    return timer_list;
}

void timerlist_free(QEMUTimerList *timer_list)
{
    assert(!timerlist_has_timers(timer_list));
    if (timer_list->clock) {
        QLIST_REMOVE(timer_list, list);
    }
    qemu_mutex_destroy(&timer_list->active_timers_lock);
    g_free(timer_list);
}

static void qemu_clock_init(QEMUClockType type, QEMUTimerListNotifyCB *notify_cb)
{
    QEMUClock *clock = qemu_clock_ptr(type);

    assert(main_loop_tlg.tl[type] == NULL);

    clock->type = type;
    clock->enabled = (type == QEMU_CLOCK_VIRTUAL ? false : true);
    QLIST_INIT(&clock->timerlists);
    main_loop_tlg.tl[type] = timerlist_new(type, notify_cb, NULL);
}

bool qemu_clock_use_for_deadline(QEMUClockType type)
{
    return !(icount_enabled() && (type == QEMU_CLOCK_VIRTUAL));
}

void qemu_clock_notify(QEMUClockType type)
{
    QEMUTimerList *timer_list;
    QEMUClock *clock = qemu_clock_ptr(type);
    QLIST_FOREACH(timer_list, &clock->timerlists, list) {
        timerlist_notify(timer_list);
    }
}

void qemu_clock_enable(QEMUClockType type, bool enabled)
{
    QEMUClock *clock = qemu_clock_ptr(type);
    QEMUTimerList *tl;
    bool old = clock->enabled;
    clock->enabled = enabled;
    if (enabled && !old) {
        qemu_clock_notify(type);
    } else if (!enabled && old) {
        QLIST_FOREACH(tl, &clock->timerlists, list) {
            qemu_event_wait(&tl->timers_done_ev);
        }
    }
}

bool timerlist_has_timers(QEMUTimerList *timer_list)
{
    return !!qatomic_read(&timer_list->active_timers);
}

bool qemu_clock_has_timers(QEMUClockType type)
{
    return timerlist_has_timers(
        main_loop_tlg.tl[type]);
}

bool timerlist_expired(QEMUTimerList *timer_list)
{
    int64_t expire_time = 0;

    if (!qatomic_read(&timer_list->active_timers)) {
        return false;
    }

    WITH_QEMU_LOCK_GUARD(&timer_list->active_timers_lock) {
        if (!timer_list->active_timers) {
            return false;
        }
        expire_time = timer_list->active_timers->expire_time;
    }

    return expire_time <= qemu_clock_get_ns(timer_list->clock->type);
}

bool qemu_clock_expired(QEMUClockType type)
{
    return timerlist_expired(
        main_loop_tlg.tl[type]);
}


int64_t timerlist_deadline_ns(QEMUTimerList *timer_list)
{
    int64_t delta;
    int64_t expire_time = 0;

    if (!qatomic_read(&timer_list->active_timers)) {
        return -1;
    }

    if (!timer_list->clock->enabled) {
        return -1;
    }

    WITH_QEMU_LOCK_GUARD(&timer_list->active_timers_lock) {
        if (!timer_list->active_timers) {
            return -1;
        }
        expire_time = timer_list->active_timers->expire_time;
    }

    delta = expire_time - qemu_clock_get_ns(timer_list->clock->type);

    if (delta <= 0) {
        return 0;
    }

    return delta;
}

int64_t qemu_clock_deadline_ns_all(QEMUClockType type, int attr_mask)
{
    int64_t deadline = -1;
    int64_t delta;
    int64_t expire_time;
    QEMUTimer *ts;
    QEMUTimerList *timer_list;
    QEMUClock *clock = qemu_clock_ptr(type);

    if (!clock->enabled) {
        return -1;
    }

    QLIST_FOREACH(timer_list, &clock->timerlists, list) {
        if (!qatomic_read(&timer_list->active_timers)) {
            continue;
        }
        qemu_mutex_lock(&timer_list->active_timers_lock);
        ts = timer_list->active_timers;
        while (ts && (ts->attributes & ~attr_mask)) {
            ts = ts->next;
        }
        if (!ts) {
            qemu_mutex_unlock(&timer_list->active_timers_lock);
            continue;
        }
        expire_time = ts->expire_time;
        qemu_mutex_unlock(&timer_list->active_timers_lock);

        delta = expire_time - qemu_clock_get_ns(type);
        if (delta <= 0) {
            delta = 0;
        }
        deadline = qemu_soonest_timeout(deadline, delta);
    }
    return deadline;
}

void timerlist_notify(QEMUTimerList *timer_list)
{
    if (timer_list->notify_cb) {
        timer_list->notify_cb(timer_list->notify_opaque, timer_list->clock->type);
    } else {
        qemu_notify_event();
    }
}

int qemu_timeout_ns_to_ms(int64_t ns)
{
    int64_t ms;
    if (ns < 0) {
        return -1;
    }

    if (!ns) {
        return 0;
    }

    ms = DIV_ROUND_UP(ns, SCALE_MS);

    return MIN(ms, INT32_MAX);
}


int qemu_poll_ns(GPollFD *fds, guint nfds, int64_t timeout)
{
#ifdef CONFIG_PPOLL
    if (timeout < 0) {
        return ppoll((struct pollfd *)fds, nfds, NULL, NULL);
    } else {
        struct timespec ts;
        int64_t tvsec = timeout / 1000000000LL;
        if (tvsec > (int64_t)INT32_MAX) {
            tvsec = INT32_MAX;
        }
        ts.tv_sec = tvsec;
        ts.tv_nsec = timeout % 1000000000LL;
        return ppoll((struct pollfd *)fds, nfds, &ts, NULL);
    }
#else
    return g_poll(fds, nfds, qemu_timeout_ns_to_ms(timeout));
#endif
}


void timer_init_full(QEMUTimer *ts,
                     QEMUTimerListGroup *timer_list_group, QEMUClockType type,
                     int scale, int attributes,
                     QEMUTimerCB *cb, void *opaque)
{
    if (!timer_list_group) {
        timer_list_group = &main_loop_tlg;
    }
    ts->timer_list = timer_list_group->tl[type];
    ts->cb = cb;
    ts->opaque = opaque;
    ts->scale = scale;
    ts->attributes = attributes;
    ts->expire_time = -1;
}

void timer_deinit(QEMUTimer *ts)
{
    assert(ts->expire_time == -1);
    ts->timer_list = NULL;
}

static void timer_del_locked(QEMUTimerList *timer_list, QEMUTimer *ts)
{
    QEMUTimer **pt, *t;

    ts->expire_time = -1;
    pt = &timer_list->active_timers;
    for(;;) {
        t = *pt;
        if (!t)
            break;
        if (t == ts) {
            qatomic_set(pt, t->next);
            break;
        }
        pt = &t->next;
    }
}

static bool timer_mod_ns_locked(QEMUTimerList *timer_list,
                                QEMUTimer *ts, int64_t expire_time)
{
    QEMUTimer **pt, *t;

    pt = &timer_list->active_timers;
    for (;;) {
        t = *pt;
        if (!timer_expired_ns(t, expire_time)) {
            break;
        }
        pt = &t->next;
    }
    ts->expire_time = MAX(expire_time, 0);
    ts->next = *pt;
    qatomic_set(pt, ts);

    return pt == &timer_list->active_timers;
}

static void timerlist_rearm(QEMUTimerList *timer_list)
{
    timerlist_notify(timer_list);
}

void timer_del(QEMUTimer *ts)
{
    QEMUTimerList *timer_list = ts->timer_list;

    if (timer_list) {
        qemu_mutex_lock(&timer_list->active_timers_lock);
        timer_del_locked(timer_list, ts);
        qemu_mutex_unlock(&timer_list->active_timers_lock);
    }
}

void timer_mod_ns(QEMUTimer *ts, int64_t expire_time)
{
    QEMUTimerList *timer_list = ts->timer_list;
    bool rearm;

    qemu_mutex_lock(&timer_list->active_timers_lock);
    timer_del_locked(timer_list, ts);
    rearm = timer_mod_ns_locked(timer_list, ts, expire_time);
    qemu_mutex_unlock(&timer_list->active_timers_lock);

    if (rearm) {
        timerlist_rearm(timer_list);
    }
}

void timer_mod_anticipate_ns(QEMUTimer *ts, int64_t expire_time)
{
    QEMUTimerList *timer_list = ts->timer_list;
    bool rearm = false;

    WITH_QEMU_LOCK_GUARD(&timer_list->active_timers_lock) {
        if (ts->expire_time == -1 || ts->expire_time > expire_time) {
            if (ts->expire_time != -1) {
                timer_del_locked(timer_list, ts);
            }
            rearm = timer_mod_ns_locked(timer_list, ts, expire_time);
        } else {
            rearm = false;
        }
    }
    if (rearm) {
        timerlist_rearm(timer_list);
    }
}

void timer_mod(QEMUTimer *ts, int64_t expire_time)
{
    timer_mod_ns(ts, expire_time * ts->scale);
}

void timer_mod_anticipate(QEMUTimer *ts, int64_t expire_time)
{
    timer_mod_anticipate_ns(ts, expire_time * ts->scale);
}

bool timer_pending(QEMUTimer *ts)
{
    return ts->expire_time >= 0;
}

bool timer_expired(QEMUTimer *timer_head, int64_t current_time)
{
    return timer_expired_ns(timer_head, current_time * timer_head->scale);
}

bool timerlist_run_timers(QEMUTimerList *timer_list)
{
    QEMUTimer *ts;
    int64_t current_time;
    bool progress = false;
    QEMUTimerCB *cb;
    void *opaque;

    if (!qatomic_read(&timer_list->active_timers)) {
        return false;
    }

    qemu_event_reset(&timer_list->timers_done_ev);
    if (!timer_list->clock->enabled) {
        goto out;
    }

    switch (timer_list->clock->type) {
    case QEMU_CLOCK_REALTIME:
        break;
    default:
    case QEMU_CLOCK_VIRTUAL:
        break;
    case QEMU_CLOCK_HOST:
        break;
    case QEMU_CLOCK_VIRTUAL_RT:
        break;
    }

    current_time = qemu_clock_get_ns(timer_list->clock->type);
    qemu_mutex_lock(&timer_list->active_timers_lock);
    while ((ts = timer_list->active_timers)) {
        if (!timer_expired_ns(ts, current_time)) {
            break;
        }

        timer_list->active_timers = ts->next;
        ts->next = NULL;
        ts->expire_time = -1;
        cb = ts->cb;
        opaque = ts->opaque;

        qemu_mutex_unlock(&timer_list->active_timers_lock);
        cb(opaque);
        qemu_mutex_lock(&timer_list->active_timers_lock);

        progress = true;
    }
    qemu_mutex_unlock(&timer_list->active_timers_lock);

out:
    qemu_event_set(&timer_list->timers_done_ev);
    return progress;
}

bool qemu_clock_run_timers(QEMUClockType type)
{
    return timerlist_run_timers(main_loop_tlg.tl[type]);
}

void timerlistgroup_init(QEMUTimerListGroup *tlg,
                         QEMUTimerListNotifyCB *cb, void *opaque)
{
    QEMUClockType type;
    for (type = 0; type < QEMU_CLOCK_MAX; type++) {
        tlg->tl[type] = timerlist_new(type, cb, opaque);
    }
}

void timerlistgroup_deinit(QEMUTimerListGroup *tlg)
{
    QEMUClockType type;
    for (type = 0; type < QEMU_CLOCK_MAX; type++) {
        timerlist_free(tlg->tl[type]);
    }
}

bool timerlistgroup_run_timers(QEMUTimerListGroup *tlg)
{
    QEMUClockType type;
    bool progress = false;
    for (type = 0; type < QEMU_CLOCK_MAX; type++) {
        progress |= timerlist_run_timers(tlg->tl[type]);
    }
    return progress;
}

int64_t timerlistgroup_deadline_ns(QEMUTimerListGroup *tlg)
{
    int64_t deadline = -1;
    QEMUClockType type;
    for (type = 0; type < QEMU_CLOCK_MAX; type++) {
        if (qemu_clock_use_for_deadline(type)) {
            deadline = qemu_soonest_timeout(deadline,
                                            timerlist_deadline_ns(tlg->tl[type]));
        }
    }
    return deadline;
}

int64_t qemu_clock_get_ns(QEMUClockType type)
{
    switch (type) {
    case QEMU_CLOCK_REALTIME:
        return get_clock();
    default:
    case QEMU_CLOCK_VIRTUAL:
        return cpus_get_virtual_clock();
    case QEMU_CLOCK_HOST:
        return get_clock_realtime();
    case QEMU_CLOCK_VIRTUAL_RT:
        return cpu_get_clock();
    }
}

static void qemu_virtual_clock_set_ns(int64_t time)
{
    return cpus_set_virtual_clock(time);
}

void init_clocks(QEMUTimerListNotifyCB *notify_cb)
{
    QEMUClockType type;
    for (type = 0; type < QEMU_CLOCK_MAX; type++) {
        qemu_clock_init(type, notify_cb);
    }

#ifdef CONFIG_PRCTL_PR_SET_TIMERSLACK
    prctl(PR_SET_TIMERSLACK, 1, 0, 0, 0);
#endif
}

uint64_t timer_expire_time_ns(QEMUTimer *ts)
{
    return timer_pending(ts) ? ts->expire_time : -1;
}

bool qemu_clock_run_all_timers(void)
{
    bool progress = false;
    QEMUClockType type;

    for (type = 0; type < QEMU_CLOCK_MAX; type++) {
        if (qemu_clock_use_for_deadline(type)) {
            progress |= qemu_clock_run_timers(type);
        }
    }

    return progress;
}

int64_t qemu_clock_advance_virtual_time(int64_t dest)
{
    int64_t clock = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    AioContext *aio_context;
    aio_context = qemu_get_aio_context();
    while (clock < dest) {
        int64_t deadline = qemu_clock_deadline_ns_all(QEMU_CLOCK_VIRTUAL,
                                                      QEMU_TIMER_ATTR_ALL);
        int64_t warp = qemu_soonest_timeout(dest - clock, deadline);

        qemu_virtual_clock_set_ns(qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + warp);

        qemu_clock_run_timers(QEMU_CLOCK_VIRTUAL);
        timerlist_run_timers(aio_context->tlg.tl[QEMU_CLOCK_VIRTUAL]);
        clock = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    }
    qemu_clock_notify(QEMU_CLOCK_VIRTUAL);

    return clock;
}
