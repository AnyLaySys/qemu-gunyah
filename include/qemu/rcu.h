#ifndef QEMU_RCU_H
#define QEMU_RCU_H



#include "qemu/thread.h"
#include "qemu/queue.h"
#include "qemu/atomic.h"
#include "qemu/notify.h"
#include "qemu/sys_membarrier.h"
#include "qemu/coroutine-tls.h"


#ifdef DEBUG_RCU
#define rcu_assert(args...)    assert(args)
#else
#define rcu_assert(args...)
#endif

extern unsigned long rcu_gp_ctr;

extern QemuEvent rcu_gp_event;

struct rcu_reader_data {
    unsigned long ctr;
    bool waiting;

    unsigned depth;

    QLIST_ENTRY(rcu_reader_data) node;

    NotifierList force_rcu;
};

QEMU_DECLARE_CO_TLS(struct rcu_reader_data, rcu_reader)

static inline void rcu_read_lock(void)
{
    struct rcu_reader_data *p_rcu_reader = get_ptr_rcu_reader();
    unsigned ctr;

    if (p_rcu_reader->depth++ > 0) {
        return;
    }

    ctr = qatomic_read(&rcu_gp_ctr);
    qatomic_set(&p_rcu_reader->ctr, ctr);

    smp_mb_placeholder();
}

static inline void rcu_read_unlock(void)
{
    struct rcu_reader_data *p_rcu_reader = get_ptr_rcu_reader();

    assert(p_rcu_reader->depth != 0);
    if (--p_rcu_reader->depth > 0) {
        return;
    }

    qatomic_store_release(&p_rcu_reader->ctr, 0);

    smp_mb_placeholder();
    if (unlikely(qatomic_read(&p_rcu_reader->waiting))) {
        qatomic_set(&p_rcu_reader->waiting, false);
        qemu_event_set(&rcu_gp_event);
    }
}

void synchronize_rcu(void);

void rcu_register_thread(void);
void rcu_unregister_thread(void);

void rcu_enable_atfork(void);
void rcu_disable_atfork(void);

struct rcu_head;
typedef void RCUCBFunc(struct rcu_head *head);

struct rcu_head {
    struct rcu_head *next;
    RCUCBFunc *func;
};

void call_rcu1(struct rcu_head *head, RCUCBFunc *func);
void drain_call_rcu(void);

#define call_rcu(head, func, field)                                      \
    call_rcu1(({                                                         \
         char __attribute__((unused))                                    \
            offset_must_be_zero[-offsetof(typeof(*(head)), field)],      \
            func_type_invalid = (func) - (void (*)(typeof(head)))(func); \
         &(head)->field;                                                 \
      }),                                                                \
      (RCUCBFunc *)(func))

#define g_free_rcu(obj, field) \
    call_rcu1(({                                                         \
        char __attribute__((unused))                                     \
            offset_must_be_zero[-offsetof(typeof(*(obj)), field)];       \
        &(obj)->field;                                                   \
      }),                                                                \
      (RCUCBFunc *)g_free);

typedef void RCUReadAuto;
static inline RCUReadAuto *rcu_read_auto_lock(void)
{
    rcu_read_lock();
    return (void *)(uintptr_t)0x1;
}

static inline void rcu_read_auto_unlock(RCUReadAuto *r)
{
    rcu_read_unlock();
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(RCUReadAuto, rcu_read_auto_unlock)

#define WITH_RCU_READ_LOCK_GUARD() \
    WITH_RCU_READ_LOCK_GUARD_(glue(_rcu_read_auto, __COUNTER__))

#define WITH_RCU_READ_LOCK_GUARD_(var) \
    for (g_autoptr(RCUReadAuto) var = rcu_read_auto_lock(); \
        (var); rcu_read_auto_unlock(var), (var) = NULL)

#define RCU_READ_LOCK_GUARD() \
    g_autoptr(RCUReadAuto) _rcu_read_auto __attribute__((unused)) = rcu_read_auto_lock()

void rcu_add_force_rcu_notifier(Notifier *n);
void rcu_remove_force_rcu_notifier(Notifier *n);

#endif /* QEMU_RCU_H */
