























#include "qemu/osdep.h"
#include "qemu/cutils.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "system/cpus.h"
#include "system/qtest.h"
#include "qemu/main-loop.h"
#include "qemu/option.h"
#include "qemu/seqlock.h"
#include "system/replay.h"
#include "system/runstate.h"
#include "hw/core/cpu.h"
#include "system/cpu-timers.h"
#include "system/cpu-timers-internal.h"







static bool icount_sleep = true;

#define MAX_ICOUNT_SHIFT 10

bool icount_align_option;


ICountMode use_icount = ICOUNT_DISABLED;

static void icount_enable_precise(void)
{

    use_icount = ICOUNT_PRECISE;
}

static void icount_enable_adaptive(void)
{

    use_icount = ICOUNT_ADAPTATIVE;
}






static int64_t icount_get_executed(CPUState *cpu)
{
    return (cpu->icount_budget -
            (cpu->neg.icount_decr.u16.low + cpu->icount_extra));
}






static void icount_update_locked(CPUState *cpu)
{
    int64_t executed = icount_get_executed(cpu);
    cpu->icount_budget -= executed;

    qatomic_set_i64(&timers_state.qemu_icount,
                    timers_state.qemu_icount + executed);
}






void icount_update(CPUState *cpu)
{
    seqlock_write_lock(&timers_state.vm_clock_seqlock,
                       &timers_state.vm_clock_lock);
    icount_update_locked(cpu);
    seqlock_write_unlock(&timers_state.vm_clock_seqlock,
                         &timers_state.vm_clock_lock);
}

static int64_t icount_get_raw_locked(void)
{
    CPUState *cpu = current_cpu;

    if (cpu && cpu->running) {
        if (!cpu->neg.can_do_io) {
            error_report("Bad icount read");
            exit(1);
        }

        icount_update_locked(cpu);
    }

    return qatomic_read_i64(&timers_state.qemu_icount);
}

static int64_t icount_get_locked(void)
{
    int64_t icount = icount_get_raw_locked();
    return qatomic_read_i64(&timers_state.qemu_icount_bias) +
        icount_to_ns(icount);
}

int64_t icount_get_raw(void)
{
    int64_t icount;
    unsigned start;

    do {
        start = seqlock_read_begin(&timers_state.vm_clock_seqlock);
        icount = icount_get_raw_locked();
    } while (seqlock_read_retry(&timers_state.vm_clock_seqlock, start));

    return icount;
}


int64_t icount_get(void)
{
    int64_t icount;
    unsigned start;

    do {
        start = seqlock_read_begin(&timers_state.vm_clock_seqlock);
        icount = icount_get_locked();
    } while (seqlock_read_retry(&timers_state.vm_clock_seqlock, start));

    return icount;
}

int64_t icount_to_ns(int64_t icount)
{
    return icount << qatomic_read(&timers_state.icount_time_shift);
}







#define ICOUNT_WOBBLE (NANOSECONDS_PER_SECOND / 10)

static void icount_adjust(void)
{
    int64_t cur_time;
    int64_t cur_icount;
    int64_t delta;


    if (!runstate_is_running()) {
        return;
    }

    seqlock_write_lock(&timers_state.vm_clock_seqlock,
                       &timers_state.vm_clock_lock);
    cur_time = REPLAY_CLOCK_LOCKED(REPLAY_CLOCK_VIRTUAL_RT,
                                   cpu_get_clock_locked());
    cur_icount = icount_get_locked();

    delta = cur_icount - cur_time;

    if (delta > 0
        && timers_state.last_delta + ICOUNT_WOBBLE < delta * 2
        && timers_state.icount_time_shift > 0) {

        qatomic_set(&timers_state.icount_time_shift,
                    timers_state.icount_time_shift - 1);
    }
    if (delta < 0
        && timers_state.last_delta - ICOUNT_WOBBLE > delta * 2
        && timers_state.icount_time_shift < MAX_ICOUNT_SHIFT) {

        qatomic_set(&timers_state.icount_time_shift,
                    timers_state.icount_time_shift + 1);
    }
    timers_state.last_delta = delta;
    qatomic_set_i64(&timers_state.qemu_icount_bias,
                    cur_icount - (timers_state.qemu_icount
                                  << timers_state.icount_time_shift));
    seqlock_write_unlock(&timers_state.vm_clock_seqlock,
                         &timers_state.vm_clock_lock);
}

static void icount_adjust_rt(void *opaque)
{
    timer_mod(timers_state.icount_rt_timer,
              qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL_RT) + 1000);
    icount_adjust();
}

static void icount_adjust_vm(void *opaque)
{
    timer_mod(timers_state.icount_vm_timer,
                   qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) +
                   NANOSECONDS_PER_SECOND / 10);
    icount_adjust();
}

int64_t icount_round(int64_t count)
{
    int shift = qatomic_read(&timers_state.icount_time_shift);
    return (count + (1 << shift) - 1) >> shift;
}

static void icount_warp_rt(void)
{
    unsigned seq;
    int64_t warp_start;





    do {
        seq = seqlock_read_begin(&timers_state.vm_clock_seqlock);
        warp_start = timers_state.vm_clock_warp_start;
    } while (seqlock_read_retry(&timers_state.vm_clock_seqlock, seq));

    if (warp_start == -1) {
        return;
    }

    seqlock_write_lock(&timers_state.vm_clock_seqlock,
                       &timers_state.vm_clock_lock);
    if (runstate_is_running()) {
        int64_t clock = REPLAY_CLOCK_LOCKED(REPLAY_CLOCK_VIRTUAL_RT,
                                            cpu_get_clock_locked());
        int64_t warp_delta;

        warp_delta = clock - timers_state.vm_clock_warp_start;
        if (icount_enabled() == ICOUNT_ADAPTATIVE) {





            int64_t cur_icount = icount_get_locked();
            int64_t delta = clock - cur_icount;

            if (delta < 0) {
                delta = 0;
            }
            warp_delta = MIN(warp_delta, delta);
        }
        qatomic_set_i64(&timers_state.qemu_icount_bias,
                        timers_state.qemu_icount_bias + warp_delta);
    }
    timers_state.vm_clock_warp_start = -1;
    seqlock_write_unlock(&timers_state.vm_clock_seqlock,
                       &timers_state.vm_clock_lock);

    if (qemu_clock_expired(QEMU_CLOCK_VIRTUAL)) {
        qemu_clock_notify(QEMU_CLOCK_VIRTUAL);
    }
}

static void icount_timer_cb(void *opaque)
{




    icount_warp_rt();
}

void icount_start_warp_timer(void)
{
    int64_t clock;
    int64_t deadline;

    assert(icount_enabled());





    if (!runstate_is_running()) {
        return;
    }

    if (replay_mode != REPLAY_MODE_PLAY) {
        if (!all_cpu_threads_idle()) {
            return;
        }

        if (qtest_enabled()) {

            return;
        }

        replay_checkpoint(CHECKPOINT_CLOCK_WARP_START);
    } else {

        if (!replay_checkpoint(CHECKPOINT_CLOCK_WARP_START)) {






            if (replay_has_event()) {
                qemu_clock_notify(QEMU_CLOCK_VIRTUAL);
            }
            return;
        }
    }


    clock = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL_RT);
    deadline = qemu_clock_deadline_ns_all(QEMU_CLOCK_VIRTUAL,
                                          ~QEMU_TIMER_ATTR_EXTERNAL);
    if (deadline < 0) {
        if (!icount_sleep) {
            warn_report_once("icount sleep disabled and no active timers");
        }
        return;
    }

    if (deadline > 0) {







        if (!icount_sleep) {







            seqlock_write_lock(&timers_state.vm_clock_seqlock,
                               &timers_state.vm_clock_lock);
            qatomic_set_i64(&timers_state.qemu_icount_bias,
                            timers_state.qemu_icount_bias + deadline);
            seqlock_write_unlock(&timers_state.vm_clock_seqlock,
                                 &timers_state.vm_clock_lock);
            qemu_clock_notify(QEMU_CLOCK_VIRTUAL);
        } else {








            seqlock_write_lock(&timers_state.vm_clock_seqlock,
                               &timers_state.vm_clock_lock);
            if (timers_state.vm_clock_warp_start == -1
                || timers_state.vm_clock_warp_start > clock) {
                timers_state.vm_clock_warp_start = clock;
            }
            seqlock_write_unlock(&timers_state.vm_clock_seqlock,
                                 &timers_state.vm_clock_lock);
            timer_mod_anticipate(timers_state.icount_warp_timer,
                                 clock + deadline);
        }
    } else if (deadline == 0) {
        qemu_clock_notify(QEMU_CLOCK_VIRTUAL);
    }
}

void icount_account_warp_timer(void)
{
    if (!icount_sleep) {
        return;
    }





    if (!runstate_is_running()) {
        return;
    }

    replay_async_events();


    if (!replay_checkpoint(CHECKPOINT_CLOCK_WARP_ACCOUNT)) {
        return;
    }

    timer_del(timers_state.icount_warp_timer);
    icount_warp_rt();
}

bool icount_configure(QemuOpts *opts, Error **errp)
{
    const char *option = qemu_opt_get(opts, "shift");
    bool sleep = qemu_opt_get_bool(opts, "sleep", true);
    bool align = qemu_opt_get_bool(opts, "align", false);
    long time_shift = -1;

    if (!option) {
        if (qemu_opt_get(opts, "align") != NULL) {
            error_setg(errp, "Please specify shift option when using align");
            return false;
        }
        return true;
    }

    if (align && !sleep) {
        error_setg(errp, "align=on and sleep=off are incompatible");
        return false;
    }

    if (strcmp(option, "auto") != 0) {
        if (qemu_strtol(option, NULL, 0, &time_shift) < 0
            || time_shift < 0 || time_shift > MAX_ICOUNT_SHIFT) {
            error_setg(errp, "icount: Invalid shift value");
            return false;
        }
    } else if (icount_align_option) {
        error_setg(errp, "shift=auto and align=on are incompatible");
        return false;
    } else if (!icount_sleep) {
        error_setg(errp, "shift=auto and sleep=off are incompatible");
        return false;
    }

    icount_sleep = sleep;
    if (icount_sleep) {
        timers_state.icount_warp_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL_RT,
                                         icount_timer_cb, NULL);
    }

    icount_align_option = align;

    if (time_shift >= 0) {
        timers_state.icount_time_shift = time_shift;
        icount_enable_precise();
        return true;
    }

    icount_enable_adaptive();





    timers_state.icount_time_shift = 3;








    timers_state.vm_clock_warp_start = -1;
    timers_state.icount_rt_timer = timer_new_ms(QEMU_CLOCK_VIRTUAL_RT,
                                   icount_adjust_rt, NULL);
    timer_mod(timers_state.icount_rt_timer,
                   qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL_RT) + 1000);
    timers_state.icount_vm_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL,
                                        icount_adjust_vm, NULL);
    timer_mod(timers_state.icount_vm_timer,
                   qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) +
                   NANOSECONDS_PER_SECOND / 10);
    return true;
}

void icount_notify_exit(void)
{
    assert(icount_enabled());

    if (current_cpu) {
        qemu_cpu_kick(current_cpu);
        qemu_clock_notify(QEMU_CLOCK_VIRTUAL);
    }
}
