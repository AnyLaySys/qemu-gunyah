
#include "qemu/osdep.h"
#include "qemu/main-loop.h"
#include "exec/cpu-common.h"
#include "hw/core/cpu.h"
#include "qemu/lockable.h"
#include "trace/trace-root.h"

QemuMutex qemu_cpu_list_lock;
static QemuCond exclusive_cond;
static QemuCond exclusive_resume;
static QemuCond qemu_work_cond;

static int pending_cpus;

void qemu_init_cpu_list(void)
{
    pending_cpus = 0;

    qemu_mutex_init(&qemu_cpu_list_lock);
    qemu_cond_init(&exclusive_cond);
    qemu_cond_init(&exclusive_resume);
    qemu_cond_init(&qemu_work_cond);
}

void cpu_list_lock(void)
{
    qemu_mutex_lock(&qemu_cpu_list_lock);
}

void cpu_list_unlock(void)
{
    qemu_mutex_unlock(&qemu_cpu_list_lock);
}


int cpu_get_free_index(void)
{
    CPUState *some_cpu;
    int max_cpu_index = 0;

    CPU_FOREACH(some_cpu) {
        if (some_cpu->cpu_index >= max_cpu_index) {
            max_cpu_index = some_cpu->cpu_index + 1;
        }
    }
    return max_cpu_index;
}

CPUTailQ cpus_queue = QTAILQ_HEAD_INITIALIZER(cpus_queue);
static unsigned int cpu_list_generation_id;

unsigned int cpu_list_generation_id_get(void)
{
    return cpu_list_generation_id;
}

void cpu_list_add(CPUState *cpu)
{
    static bool cpu_index_auto_assigned;

    QEMU_LOCK_GUARD(&qemu_cpu_list_lock);
    if (cpu->cpu_index == UNASSIGNED_CPU_INDEX) {
        cpu_index_auto_assigned = true;
        cpu->cpu_index = cpu_get_free_index();
        assert(cpu->cpu_index != UNASSIGNED_CPU_INDEX);
    } else {
        assert(!cpu_index_auto_assigned);
    }
    QTAILQ_INSERT_TAIL_RCU(&cpus_queue, cpu, node);
    cpu_list_generation_id++;
}

void cpu_list_remove(CPUState *cpu)
{
    QEMU_LOCK_GUARD(&qemu_cpu_list_lock);
    if (!QTAILQ_IN_USE(cpu, node)) {
        return;
    }

    QTAILQ_REMOVE_RCU(&cpus_queue, cpu, node);
    cpu->cpu_index = UNASSIGNED_CPU_INDEX;
    cpu_list_generation_id++;
}

CPUState *qemu_get_cpu(int index)
{
    CPUState *cpu;

    CPU_FOREACH(cpu) {
        if (cpu->cpu_index == index) {
            return cpu;
        }
    }

    return NULL;
}

#ifdef __ANDROID__
#include <pthread.h>
static pthread_key_t current_cpu_key;
static pthread_once_t current_cpu_once = PTHREAD_ONCE_INIT;
static void current_cpu_key_init(void) {
    pthread_key_create(&current_cpu_key, free);
}
CPUState **android_current_cpu_ptr(void) {
    pthread_once(&current_cpu_once, current_cpu_key_init);
    CPUState **p = (CPUState **)pthread_getspecific(current_cpu_key);
    if (!p) {
        p = (CPUState **)calloc(1, sizeof(CPUState *));
        pthread_setspecific(current_cpu_key, p);
    }
    return p;
}
#else
__thread CPUState *current_cpu;
#endif

struct qemu_work_item {
    QSIMPLEQ_ENTRY(qemu_work_item) node;
    run_on_cpu_func func;
    run_on_cpu_data data;
    bool free, exclusive, done;
};

static void queue_work_on_cpu(CPUState *cpu, struct qemu_work_item *wi)
{
    qemu_mutex_lock(&cpu->work_mutex);
    QSIMPLEQ_INSERT_TAIL(&cpu->work_list, wi, node);
    wi->done = false;
    qemu_mutex_unlock(&cpu->work_mutex);

    qemu_cpu_kick(cpu);
}

void do_run_on_cpu(CPUState *cpu, run_on_cpu_func func, run_on_cpu_data data,
                   QemuMutex *mutex)
{
    struct qemu_work_item wi;

    if (qemu_cpu_is_self(cpu)) {
        func(cpu, data);
        return;
    }

    wi.func = func;
    wi.data = data;
    wi.done = false;
    wi.free = false;
    wi.exclusive = false;

    queue_work_on_cpu(cpu, &wi);
    while (!qatomic_load_acquire(&wi.done)) {
        CPUState *self_cpu = current_cpu;

        qemu_cond_wait(&qemu_work_cond, mutex);
        current_cpu = self_cpu;
    }
}

void async_run_on_cpu(CPUState *cpu, run_on_cpu_func func, run_on_cpu_data data)
{
    struct qemu_work_item *wi;

    wi = g_new0(struct qemu_work_item, 1);
    wi->func = func;
    wi->data = data;
    wi->free = true;

    queue_work_on_cpu(cpu, wi);
}

static inline void exclusive_idle(void)
{
    while (pending_cpus) {
        qemu_cond_wait(&exclusive_resume, &qemu_cpu_list_lock);
    }
}

void start_exclusive(void)
{
    CPUState *other_cpu;
    int running_cpus;

    g_assert(!current_cpu->running);

    if (current_cpu->exclusive_context_count) {
        current_cpu->exclusive_context_count++;
        return;
    }

    qemu_mutex_lock(&qemu_cpu_list_lock);
    exclusive_idle();

    qatomic_set(&pending_cpus, 1);

    smp_mb();
    running_cpus = 0;
    CPU_FOREACH(other_cpu) {
        if (qatomic_read(&other_cpu->running)) {
            other_cpu->has_waiter = true;
            running_cpus++;
            qemu_cpu_kick(other_cpu);
        }
    }

    qatomic_set(&pending_cpus, running_cpus + 1);
    while (pending_cpus > 1) {
        qemu_cond_wait(&exclusive_cond, &qemu_cpu_list_lock);
    }

    qemu_mutex_unlock(&qemu_cpu_list_lock);

    current_cpu->exclusive_context_count = 1;
}

void end_exclusive(void)
{
    current_cpu->exclusive_context_count--;
    if (current_cpu->exclusive_context_count) {
        return;
    }

    qemu_mutex_lock(&qemu_cpu_list_lock);
    qatomic_set(&pending_cpus, 0);
    qemu_cond_broadcast(&exclusive_resume);
    qemu_mutex_unlock(&qemu_cpu_list_lock);
}

void cpu_exec_start(CPUState *cpu)
{
    qatomic_set(&cpu->running, true);

    smp_mb();

    if (unlikely(qatomic_read(&pending_cpus))) {
        QEMU_LOCK_GUARD(&qemu_cpu_list_lock);
        if (!cpu->has_waiter) {
            qatomic_set(&cpu->running, false);
            exclusive_idle();
            qatomic_set(&cpu->running, true);
        } else {
        }
    }
}

void cpu_exec_end(CPUState *cpu)
{
    qatomic_set(&cpu->running, false);

    smp_mb();

    if (unlikely(qatomic_read(&pending_cpus))) {
        QEMU_LOCK_GUARD(&qemu_cpu_list_lock);
        if (cpu->has_waiter) {
            cpu->has_waiter = false;
            qatomic_set(&pending_cpus, pending_cpus - 1);
            if (pending_cpus == 1) {
                qemu_cond_signal(&exclusive_cond);
            }
        }
    }
}

void async_safe_run_on_cpu(CPUState *cpu, run_on_cpu_func func,
                           run_on_cpu_data data)
{
    struct qemu_work_item *wi;

    wi = g_new0(struct qemu_work_item, 1);
    wi->func = func;
    wi->data = data;
    wi->free = true;
    wi->exclusive = true;

    queue_work_on_cpu(cpu, wi);
}

void free_queued_cpu_work(CPUState *cpu)
{
    while (!QSIMPLEQ_EMPTY(&cpu->work_list)) {
        struct qemu_work_item *wi = QSIMPLEQ_FIRST(&cpu->work_list);
        QSIMPLEQ_REMOVE_HEAD(&cpu->work_list, node);
        if (wi->free) {
            g_free(wi);
        }
    }
}

void process_queued_cpu_work(CPUState *cpu)
{
    struct qemu_work_item *wi;

    qemu_mutex_lock(&cpu->work_mutex);
    if (QSIMPLEQ_EMPTY(&cpu->work_list)) {
        qemu_mutex_unlock(&cpu->work_mutex);
        return;
    }
    while (!QSIMPLEQ_EMPTY(&cpu->work_list)) {
        wi = QSIMPLEQ_FIRST(&cpu->work_list);
        QSIMPLEQ_REMOVE_HEAD(&cpu->work_list, node);
        qemu_mutex_unlock(&cpu->work_mutex);
        if (wi->exclusive) {
            bql_unlock();
            start_exclusive();
            wi->func(cpu, wi->data);
            end_exclusive();
            bql_lock();
        } else {
            wi->func(cpu, wi->data);
        }
        qemu_mutex_lock(&cpu->work_mutex);
        if (wi->free) {
            g_free(wi);
        } else {
            qatomic_store_release(&wi->done, true);
        }
    }
    qemu_mutex_unlock(&cpu->work_mutex);
    qemu_cond_broadcast(&qemu_work_cond);
}

int cpu_breakpoint_insert(CPUState *cpu, vaddr pc, int flags,
                          CPUBreakpoint **breakpoint)
{
    CPUBreakpoint *bp;

    bp = g_malloc(sizeof(*bp));

    bp->pc = pc;
    bp->flags = flags;

    QTAILQ_INSERT_TAIL(&cpu->breakpoints, bp, entry);

    if (breakpoint) {
        *breakpoint = bp;
    }

    trace_breakpoint_insert(cpu->cpu_index, pc, flags);
    return 0;
}

int cpu_breakpoint_remove(CPUState *cpu, vaddr pc, int flags)
{
    CPUBreakpoint *bp;

    QTAILQ_FOREACH(bp, &cpu->breakpoints, entry) {
        if (bp->pc == pc && bp->flags == flags) {
            cpu_breakpoint_remove_by_ref(cpu, bp);
            return 0;
        }
    }
    return -ENOENT;
}

void cpu_breakpoint_remove_by_ref(CPUState *cpu, CPUBreakpoint *bp)
{
    QTAILQ_REMOVE(&cpu->breakpoints, bp, entry);

    trace_breakpoint_remove(cpu->cpu_index, bp->pc, bp->flags);
    g_free(bp);
}

void cpu_breakpoint_remove_all(CPUState *cpu, int mask)
{
    CPUBreakpoint *bp, *next;

    QTAILQ_FOREACH_SAFE(bp, &cpu->breakpoints, entry, next) {
        if (bp->flags & mask) {
            cpu_breakpoint_remove_by_ref(cpu, bp);
        }
    }
}
