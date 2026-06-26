
#include "qemu/osdep.h"
#include "qemu/rcu.h"
#include "qemu/atomic.h"
#include "qemu/thread.h"
#include "qemu/main-loop.h"
#include "qemu/lockable.h"
#if defined(CONFIG_MALLOC_TRIM)
#include <malloc.h>
#endif

#define RCU_GP_LOCKED           (1UL << 0)
#define RCU_GP_CTR              (1UL << 1)

unsigned long rcu_gp_ctr = RCU_GP_LOCKED;

QemuEvent rcu_gp_event;
static int in_drain_call_rcu;
static QemuMutex rcu_registry_lock;
static QemuMutex rcu_sync_lock;

static inline int rcu_gp_ongoing(unsigned long *ctr)
{
    unsigned long v;

    v = qatomic_read(ctr);
    return v && (v != rcu_gp_ctr);
}

QEMU_DEFINE_CO_TLS(struct rcu_reader_data, rcu_reader)

typedef QLIST_HEAD(, rcu_reader_data) ThreadList;
static ThreadList registry = QLIST_HEAD_INITIALIZER(registry);

static void wait_for_readers(void)
{
    ThreadList qsreaders = QLIST_HEAD_INITIALIZER(qsreaders);
    struct rcu_reader_data *index, *tmp;

    for (;;) {
        qemu_event_reset(&rcu_gp_event);

        QLIST_FOREACH(index, &registry, node) {
            qatomic_set(&index->waiting, true);
        }

        smp_mb_global();

        QLIST_FOREACH_SAFE(index, &registry, node, tmp) {
            if (!rcu_gp_ongoing(&index->ctr)) {
                QLIST_REMOVE(index, node);
                QLIST_INSERT_HEAD(&qsreaders, index, node);

                qatomic_set(&index->waiting, false);
            } else if (qatomic_read(&in_drain_call_rcu)) {
                notifier_list_notify(&index->force_rcu, NULL);
            }
        }

        if (QLIST_EMPTY(&registry)) {
            break;
        }

        qemu_mutex_unlock(&rcu_registry_lock);
        qemu_event_wait(&rcu_gp_event);
        qemu_mutex_lock(&rcu_registry_lock);
    }

    QLIST_SWAP(&registry, &qsreaders, node);
}

void synchronize_rcu(void)
{
    QEMU_LOCK_GUARD(&rcu_sync_lock);

    smp_mb_global();

    QEMU_LOCK_GUARD(&rcu_registry_lock);
    if (!QLIST_EMPTY(&registry)) {
        if (sizeof(rcu_gp_ctr) < 8) {
            qatomic_set(&rcu_gp_ctr, rcu_gp_ctr ^ RCU_GP_CTR);
            wait_for_readers();
            qatomic_set(&rcu_gp_ctr, rcu_gp_ctr ^ RCU_GP_CTR);
        } else {
            qatomic_set(&rcu_gp_ctr, rcu_gp_ctr + RCU_GP_CTR);
        }

        wait_for_readers();
    }
}


#define RCU_CALL_MIN_SIZE        30

static struct rcu_head dummy;
static struct rcu_head *head = &dummy, **tail = &dummy.next;
static int rcu_call_count;
static QemuEvent rcu_call_ready_event;

static void enqueue(struct rcu_head *node)
{
    struct rcu_head **old_tail;

    node->next = NULL;

    old_tail = qatomic_xchg(&tail, &node->next);

    qatomic_store_release(old_tail, node);
}

static struct rcu_head *try_dequeue(void)
{
    struct rcu_head *node, *next;

retry:
    node = head;

    next = qatomic_load_acquire(&node->next);
    if (!next) {
        return NULL;
    }

    if (head == &dummy && qatomic_read(&tail) == &dummy.next) {
        abort();
    }

    head = next;

    if (node == &dummy) {
        enqueue(node);
        goto retry;
    }

    return node;
}

static void *call_rcu_thread(void *opaque)
{
    struct rcu_head *node;

    rcu_register_thread();

    for (;;) {
        int tries = 0;
        int n = qatomic_read(&rcu_call_count);

        while (n == 0 || (n < RCU_CALL_MIN_SIZE && ++tries <= 5)) {
            g_usleep(10000);
            if (n == 0) {
                qemu_event_reset(&rcu_call_ready_event);
                n = qatomic_read(&rcu_call_count);
                if (n == 0) {
#if defined(CONFIG_MALLOC_TRIM)
                    malloc_trim(4 * 1024 * 1024);
#endif
                    qemu_event_wait(&rcu_call_ready_event);
                }
            }
            n = qatomic_read(&rcu_call_count);
        }

        qatomic_sub(&rcu_call_count, n);
        synchronize_rcu();
        bql_lock();
        while (n > 0) {
            node = try_dequeue();
            while (!node) {
                bql_unlock();
                qemu_event_reset(&rcu_call_ready_event);
                node = try_dequeue();
                if (!node) {
                    qemu_event_wait(&rcu_call_ready_event);
                    node = try_dequeue();
                }
                bql_lock();
            }

            n--;
            node->func(node);
        }
        bql_unlock();
    }
    abort();
}

void call_rcu1(struct rcu_head *node, void (*func)(struct rcu_head *node))
{
    node->func = func;
    enqueue(node);
    qatomic_inc(&rcu_call_count);
    qemu_event_set(&rcu_call_ready_event);
}


struct rcu_drain {
    struct rcu_head rcu;
    QemuEvent drain_complete_event;
};

static void drain_rcu_callback(struct rcu_head *node)
{
    struct rcu_drain *event = (struct rcu_drain *)node;
    qemu_event_set(&event->drain_complete_event);
}


void drain_call_rcu(void)
{
    struct rcu_drain rcu_drain;
    bool locked = bql_locked();

    memset(&rcu_drain, 0, sizeof(struct rcu_drain));
    qemu_event_init(&rcu_drain.drain_complete_event, false);

    if (locked) {
        bql_unlock();
    }



    qatomic_inc(&in_drain_call_rcu);
    call_rcu1(&rcu_drain.rcu, drain_rcu_callback);
    qemu_event_wait(&rcu_drain.drain_complete_event);
    qatomic_dec(&in_drain_call_rcu);

    if (locked) {
        bql_lock();
    }

}

void rcu_register_thread(void)
{
    assert(get_ptr_rcu_reader()->ctr == 0);
    qemu_mutex_lock(&rcu_registry_lock);
    QLIST_INSERT_HEAD(&registry, get_ptr_rcu_reader(), node);
    qemu_mutex_unlock(&rcu_registry_lock);
}

void rcu_unregister_thread(void)
{
    qemu_mutex_lock(&rcu_registry_lock);
    QLIST_REMOVE(get_ptr_rcu_reader(), node);
    qemu_mutex_unlock(&rcu_registry_lock);
}

void rcu_add_force_rcu_notifier(Notifier *n)
{
    qemu_mutex_lock(&rcu_registry_lock);
    notifier_list_add(&get_ptr_rcu_reader()->force_rcu, n);
    qemu_mutex_unlock(&rcu_registry_lock);
}

void rcu_remove_force_rcu_notifier(Notifier *n)
{
    qemu_mutex_lock(&rcu_registry_lock);
    notifier_remove(n);
    qemu_mutex_unlock(&rcu_registry_lock);
}

static void rcu_init_complete(void)
{
    QemuThread thread;

    qemu_mutex_init(&rcu_registry_lock);
    qemu_mutex_init(&rcu_sync_lock);
    qemu_event_init(&rcu_gp_event, true);

    qemu_event_init(&rcu_call_ready_event, false);

    qemu_thread_create(&thread, "call_rcu", call_rcu_thread,
                       NULL, QEMU_THREAD_DETACHED);

    rcu_register_thread();
}

static int atfork_depth = 1;

void rcu_enable_atfork(void)
{
    atfork_depth++;
}

void rcu_disable_atfork(void)
{
    atfork_depth--;
}

#ifdef CONFIG_POSIX
static void rcu_init_lock(void)
{
    if (atfork_depth < 1) {
        return;
    }

    qemu_mutex_lock(&rcu_sync_lock);
    qemu_mutex_lock(&rcu_registry_lock);
}

static void rcu_init_unlock(void)
{
    if (atfork_depth < 1) {
        return;
    }

    qemu_mutex_unlock(&rcu_registry_lock);
    qemu_mutex_unlock(&rcu_sync_lock);
}

static void rcu_init_child(void)
{
    if (atfork_depth < 1) {
        return;
    }

    memset(&registry, 0, sizeof(registry));
    rcu_init_complete();
}
#endif

static void __attribute__((__constructor__)) rcu_init(void)
{
    smp_mb_global_init();
#ifdef CONFIG_POSIX
    pthread_atfork(rcu_init_lock, rcu_init_unlock, rcu_init_child);
#endif
    rcu_init_complete();
}
