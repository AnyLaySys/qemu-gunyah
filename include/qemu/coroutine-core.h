
#ifndef QEMU_COROUTINE_CORE_H
#define QEMU_COROUTINE_CORE_H



typedef struct Coroutine Coroutine;
typedef struct CoMutex CoMutex;

typedef void coroutine_fn CoroutineEntry(void *opaque);

Coroutine *qemu_coroutine_create(CoroutineEntry *entry, void *opaque);

void qemu_coroutine_enter(Coroutine *coroutine);

void qemu_coroutine_enter_if_inactive(Coroutine *co);

void qemu_aio_coroutine_enter(AioContext *ctx, Coroutine *co);

void coroutine_fn qemu_coroutine_yield(void);

AioContext *qemu_coroutine_get_aio_context(Coroutine *co);

Coroutine *qemu_coroutine_self(void);

bool qemu_in_coroutine(void);

bool qemu_coroutine_entered(Coroutine *co);

void qemu_co_mutex_init(CoMutex *mutex);

void coroutine_fn qemu_co_mutex_lock(CoMutex *mutex);

void coroutine_fn qemu_co_mutex_unlock(CoMutex *mutex);

#endif
