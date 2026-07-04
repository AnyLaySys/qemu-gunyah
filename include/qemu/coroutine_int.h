
#ifndef QEMU_COROUTINE_INT_H
#define QEMU_COROUTINE_INT_H

#include "qemu/queue.h"
#include "qemu/coroutine.h"

#ifdef CONFIG_SAFESTACK
extern __thread void *__safestack_unsafe_stack_ptr;
#endif

#define COROUTINE_STACK_SIZE (1 << 20)

typedef enum {
    COROUTINE_YIELD = 1,
    COROUTINE_TERMINATE = 2,
    COROUTINE_ENTER = 3,
} CoroutineAction;

struct Coroutine {
    CoroutineEntry *entry;
    void *entry_arg;
    Coroutine *caller;

    QSLIST_ENTRY(Coroutine) pool_next;

    size_t locks_held;

    AioContext *ctx;

    const char *scheduled;

    QSIMPLEQ_ENTRY(Coroutine) co_queue_next;

    QSIMPLEQ_HEAD(, Coroutine) co_queue_wakeup;

    QSLIST_ENTRY(Coroutine) co_scheduled_next;
};

Coroutine *qemu_coroutine_new(void);
void qemu_coroutine_delete(Coroutine *co);
CoroutineAction qemu_coroutine_switch(Coroutine *from, Coroutine *to,
                                      CoroutineAction action);

#endif
