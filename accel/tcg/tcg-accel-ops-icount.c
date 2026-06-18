
























#include "qemu/osdep.h"
#include "system/replay.h"
#include "system/cpu-timers.h"
#include "qemu/main-loop.h"
#include "qemu/guest-random.h"
#include "hw/core/cpu.h"

#include "tcg-accel-ops.h"
#include "tcg-accel-ops-icount.h"
#include "tcg-accel-ops-rr.h"

static int64_t icount_get_limit(void)
{
    int64_t deadline;

    if (replay_mode != REPLAY_MODE_PLAY) {




        deadline = qemu_clock_deadline_ns_all(QEMU_CLOCK_VIRTUAL,
                                              QEMU_TIMER_ATTR_ALL);

        deadline = qemu_soonest_timeout(deadline,
                qemu_clock_deadline_ns_all(QEMU_CLOCK_REALTIME,
                                           QEMU_TIMER_ATTR_ALL));







        if ((deadline < 0) || (deadline > INT32_MAX)) {
            deadline = INT32_MAX;
        }

        return icount_round(deadline);
    } else {
        return replay_get_instructions();
    }
}

static void icount_notify_aio_contexts(void)
{

    qemu_clock_notify(QEMU_CLOCK_VIRTUAL);
    qemu_clock_run_timers(QEMU_CLOCK_VIRTUAL);
}

void icount_handle_deadline(void)
{
    assert(qemu_in_vcpu_thread());
    int64_t deadline = qemu_clock_deadline_ns_all(QEMU_CLOCK_VIRTUAL,
                                                  QEMU_TIMER_ATTR_ALL);






    if (deadline == 0) {
        icount_notify_aio_contexts();
    }
}


int64_t icount_percpu_budget(int cpu_count)
{
    int64_t limit = icount_get_limit();
    int64_t timeslice = limit / cpu_count;

    if (timeslice == 0) {
        timeslice = limit;
    }

    return timeslice;
}

void icount_prepare_for_run(CPUState *cpu, int64_t cpu_budget)
{
    int insns_left;






    g_assert(cpu->neg.icount_decr.u16.low == 0);
    g_assert(cpu->icount_extra == 0);

    replay_mutex_lock();

    cpu->icount_budget = MIN(icount_get_limit(), cpu_budget);
    insns_left = MIN(0xffff, cpu->icount_budget);
    cpu->neg.icount_decr.u16.low = insns_left;
    cpu->icount_extra = cpu->icount_budget - insns_left;

    if (cpu->icount_budget == 0) {




        bql_lock();
        icount_notify_aio_contexts();
        bql_unlock();
    }
}

void icount_process_data(CPUState *cpu)
{

    icount_update(cpu);


    cpu->neg.icount_decr.u16.low = 0;
    cpu->icount_extra = 0;
    cpu->icount_budget = 0;

    replay_account_executed_instructions();

    replay_mutex_unlock();
}

void icount_handle_interrupt(CPUState *cpu, int mask)
{
    int old_mask = cpu->interrupt_request;

    tcg_handle_interrupt(cpu, mask);
    if (qemu_cpu_is_self(cpu) &&
        !cpu->neg.can_do_io
        && (mask & ~old_mask) != 0) {
        cpu_abort(cpu, "Raised interrupt while not in I/O function");
    }
}
