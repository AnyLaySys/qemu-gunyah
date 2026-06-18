
























#include "qemu/osdep.h"
#include "system/tcg.h"
#include "system/replay.h"
#include "system/cpu-timers.h"
#include "qemu/main-loop.h"
#include "qemu/notify.h"
#include "qemu/guest-random.h"
#include "hw/boards.h"
#include "tcg/startup.h"
#include "tcg-accel-ops.h"
#include "tcg-accel-ops-mttcg.h"

typedef struct MttcgForceRcuNotifier {
    Notifier notifier;
    CPUState *cpu;
} MttcgForceRcuNotifier;

static void do_nothing(CPUState *cpu, run_on_cpu_data d)
{
}

static void mttcg_force_rcu(Notifier *notify, void *data)
{
    CPUState *cpu = container_of(notify, MttcgForceRcuNotifier, notifier)->cpu;





    async_run_on_cpu(cpu, do_nothing, RUN_ON_CPU_NULL);
}







static void *mttcg_cpu_thread_fn(void *arg)
{
    MttcgForceRcuNotifier force_rcu;
    CPUState *cpu = arg;

    assert(tcg_enabled());
    g_assert(!icount_enabled());

    rcu_register_thread();
    force_rcu.notifier.notify = mttcg_force_rcu;
    force_rcu.cpu = cpu;
    rcu_add_force_rcu_notifier(&force_rcu.notifier);
    tcg_register_thread();

    bql_lock();
    qemu_thread_get_self(cpu->thread);

    cpu->thread_id = qemu_get_thread_id();
    cpu->neg.can_do_io = true;
    current_cpu = cpu;
    cpu_thread_signal_created(cpu);
    qemu_guest_random_seed_thread_part2(cpu->random_seed);


    cpu->exit_request = 1;

    do {
        if (cpu_can_run(cpu)) {
            int r;
            bql_unlock();
            r = tcg_cpu_exec(cpu);
            bql_lock();
            switch (r) {
            case EXCP_DEBUG:
                cpu_handle_guest_debug(cpu);
                break;
            case EXCP_HALTED:




                break;
            case EXCP_ATOMIC:
                bql_unlock();
                cpu_exec_step_atomic(cpu);
                bql_lock();
            default:

                break;
            }
        }

        qatomic_set_mb(&cpu->exit_request, 0);
        qemu_wait_io_event(cpu);
    } while (!cpu->unplug || cpu_can_run(cpu));

    tcg_cpu_destroy(cpu);
    bql_unlock();
    rcu_remove_force_rcu_notifier(&force_rcu.notifier);
    rcu_unregister_thread();
    return NULL;
}

void mttcg_kick_vcpu_thread(CPUState *cpu)
{
    cpu_exit(cpu);
}

void mttcg_start_vcpu_thread(CPUState *cpu)
{
    char thread_name[VCPU_THREAD_NAME_SIZE];

    g_assert(tcg_enabled());
    tcg_cpu_init_cflags(cpu, current_machine->smp.max_cpus > 1);


    snprintf(thread_name, VCPU_THREAD_NAME_SIZE, "CPU %d/TCG",
             cpu->cpu_index);

    qemu_thread_create(cpu->thread, thread_name, mttcg_cpu_thread_fn,
                       cpu, QEMU_THREAD_JOINABLE);
}
