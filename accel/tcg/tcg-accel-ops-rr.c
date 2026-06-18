
























#include "qemu/osdep.h"
#include "qemu/lockable.h"
#include "system/tcg.h"
#include "system/replay.h"
#include "system/cpu-timers.h"
#include "qemu/main-loop.h"
#include "qemu/notify.h"
#include "qemu/guest-random.h"
#include "exec/cpu-common.h"
#include "tcg/startup.h"
#include "tcg-accel-ops.h"
#include "tcg-accel-ops-rr.h"
#include "tcg-accel-ops-icount.h"


void rr_kick_vcpu_thread(CPUState *unused)
{
    CPUState *cpu;

    CPU_FOREACH(cpu) {
        cpu_exit(cpu);
    };
}













static QEMUTimer *rr_kick_vcpu_timer;
static CPUState *rr_current_cpu;

static inline int64_t rr_next_kick_time(void)
{
    return qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + TCG_KICK_PERIOD;
}


static void rr_kick_next_cpu(void)
{
    CPUState *cpu;
    do {
        cpu = qatomic_read(&rr_current_cpu);
        if (cpu) {
            cpu_exit(cpu);
        }

        smp_mb();
    } while (cpu != qatomic_read(&rr_current_cpu));
}

static void rr_kick_thread(void *opaque)
{
    timer_mod(rr_kick_vcpu_timer, rr_next_kick_time());
    rr_kick_next_cpu();
}

static void rr_start_kick_timer(void)
{
    if (!rr_kick_vcpu_timer && CPU_NEXT(first_cpu)) {
        rr_kick_vcpu_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL,
                                           rr_kick_thread, NULL);
    }
    if (rr_kick_vcpu_timer && !timer_pending(rr_kick_vcpu_timer)) {
        timer_mod(rr_kick_vcpu_timer, rr_next_kick_time());
    }
}

static void rr_stop_kick_timer(void)
{
    if (rr_kick_vcpu_timer && timer_pending(rr_kick_vcpu_timer)) {
        timer_del(rr_kick_vcpu_timer);
    }
}

static void rr_wait_io_event(void)
{
    CPUState *cpu;

    while (all_cpu_threads_idle()) {
        rr_stop_kick_timer();
        qemu_cond_wait_bql(first_cpu->halt_cond);
    }

    rr_start_kick_timer();

    CPU_FOREACH(cpu) {
        qemu_wait_io_event_common(cpu);
    }
}





static void rr_deal_with_unplugged_cpus(void)
{
    CPUState *cpu;

    CPU_FOREACH(cpu) {
        if (cpu->unplug && !cpu_can_run(cpu)) {
            tcg_cpu_destroy(cpu);
            break;
        }
    }
}

static void rr_force_rcu(Notifier *notify, void *data)
{
    rr_kick_next_cpu();
}









static int rr_cpu_count(void)
{
    static unsigned int last_gen_id = ~0;
    static int cpu_count;
    CPUState *cpu;

    QEMU_LOCK_GUARD(&qemu_cpu_list_lock);

    if (cpu_list_generation_id_get() != last_gen_id) {
        cpu_count = 0;
        CPU_FOREACH(cpu) {
            ++cpu_count;
        }
        last_gen_id = cpu_list_generation_id_get();
    }

    return cpu_count;
}









static void *rr_cpu_thread_fn(void *arg)
{
    Notifier force_rcu;
    CPUState *cpu = arg;

    assert(tcg_enabled());
    rcu_register_thread();
    force_rcu.notify = rr_force_rcu;
    rcu_add_force_rcu_notifier(&force_rcu);
    tcg_register_thread();

    bql_lock();
    qemu_thread_get_self(cpu->thread);

    cpu->thread_id = qemu_get_thread_id();
    cpu->neg.can_do_io = true;
    cpu_thread_signal_created(cpu);
    qemu_guest_random_seed_thread_part2(cpu->random_seed);


    while (first_cpu->stopped) {
        qemu_cond_wait_bql(first_cpu->halt_cond);


        CPU_FOREACH(cpu) {
            current_cpu = cpu;
            qemu_wait_io_event_common(cpu);
        }
    }

    rr_start_kick_timer();

    cpu = first_cpu;


    cpu->exit_request = 1;

    while (1) {

        int64_t cpu_budget = 0;

        bql_unlock();
        replay_mutex_lock();
        bql_lock();

        if (icount_enabled()) {
            int cpu_count = rr_cpu_count();


            icount_account_warp_timer();




            icount_handle_deadline();

            cpu_budget = icount_percpu_budget(cpu_count);
        }

        replay_mutex_unlock();

        if (!cpu) {
            cpu = first_cpu;
        }

        while (cpu && cpu_work_list_empty(cpu) && !cpu->exit_request) {

            qatomic_set_mb(&rr_current_cpu, cpu);

            current_cpu = cpu;

            qemu_clock_enable(QEMU_CLOCK_VIRTUAL,
                              (cpu->singlestep_enabled & SSTEP_NOTIMER) == 0);

            if (cpu_can_run(cpu)) {
                int r;

                bql_unlock();
                if (icount_enabled()) {
                    icount_prepare_for_run(cpu, cpu_budget);
                }
                r = tcg_cpu_exec(cpu);
                if (icount_enabled()) {
                    icount_process_data(cpu);
                }
                bql_lock();

                if (r == EXCP_DEBUG) {
                    cpu_handle_guest_debug(cpu);
                    break;
                } else if (r == EXCP_ATOMIC) {
                    bql_unlock();
                    cpu_exec_step_atomic(cpu);
                    bql_lock();
                    break;
                }
            } else if (cpu->stop) {
                if (cpu->unplug) {
                    cpu = CPU_NEXT(cpu);
                }
                break;
            }

            cpu = CPU_NEXT(cpu);
        }


        qatomic_set(&rr_current_cpu, NULL);

        if (cpu && cpu->exit_request) {
            qatomic_set_mb(&cpu->exit_request, 0);
        }

        if (icount_enabled() && all_cpu_threads_idle()) {




            qemu_notify_event();
        }

        rr_wait_io_event();
        rr_deal_with_unplugged_cpus();
    }

    g_assert_not_reached();
}

void rr_start_vcpu_thread(CPUState *cpu)
{
    char thread_name[VCPU_THREAD_NAME_SIZE];
    static QemuCond *single_tcg_halt_cond;
    static QemuThread *single_tcg_cpu_thread;

    g_assert(tcg_enabled());
    tcg_cpu_init_cflags(cpu, false);

    if (!single_tcg_cpu_thread) {
        single_tcg_halt_cond = cpu->halt_cond;
        single_tcg_cpu_thread = cpu->thread;


        snprintf(thread_name, VCPU_THREAD_NAME_SIZE, "ALL CPUs/TCG");
        qemu_thread_create(cpu->thread, thread_name,
                           rr_cpu_thread_fn,
                           cpu, QEMU_THREAD_JOINABLE);
    } else {

        g_free(cpu->thread);
        qemu_cond_destroy(cpu->halt_cond);
        g_free(cpu->halt_cond);
        cpu->thread = single_tcg_cpu_thread;
        cpu->halt_cond = single_tcg_halt_cond;


        cpu->thread_id = first_cpu->thread_id;
        cpu->neg.can_do_io = 1;
        cpu->created = true;
    }
}
