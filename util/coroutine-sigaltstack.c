
#undef _FORTIFY_SOURCE
#define _FORTIFY_SOURCE 0

#include "qemu/osdep.h"
#include <pthread.h>
#include "qemu/coroutine_int.h"

#ifdef CONFIG_SAFESTACK
#error "SafeStack is not compatible with code run in alternate signal stacks"
#endif

typedef struct {
    Coroutine base;
    void *stack;
    size_t stack_size;
    sigjmp_buf env;
} CoroutineSigAltStack;

typedef struct {
    Coroutine *current;

    CoroutineSigAltStack leader;

    sigjmp_buf tr_reenter;
    volatile sig_atomic_t tr_called;
    void *tr_handler;
} CoroutineThreadState;

static pthread_key_t thread_state_key;

static CoroutineThreadState *coroutine_get_thread_state(void)
{
    CoroutineThreadState *s = pthread_getspecific(thread_state_key);

    if (!s) {
        s = g_malloc0(sizeof(*s));
        s->current = &s->leader.base;
        pthread_setspecific(thread_state_key, s);
    }
    return s;
}

static void qemu_coroutine_thread_cleanup(void *opaque)
{
    CoroutineThreadState *s = opaque;

    g_free(s);
}

static void __attribute__((constructor)) coroutine_init(void)
{
    int ret;

    ret = pthread_key_create(&thread_state_key, qemu_coroutine_thread_cleanup);
    if (ret != 0) {
        fprintf(stderr, "unable to create leader key: %s\n", strerror(errno));
        abort();
    }
}

static void coroutine_bootstrap(CoroutineSigAltStack *self, Coroutine *co)
{
    if (!sigsetjmp(self->env, 0)) {
        siglongjmp(*(sigjmp_buf *)co->entry_arg, 1);
    }

    while (true) {
        co->entry(co->entry_arg);
        qemu_coroutine_switch(co, co->caller, COROUTINE_TERMINATE);
    }
}

static void coroutine_trampoline(int signal)
{
    CoroutineSigAltStack *self;
    Coroutine *co;
    CoroutineThreadState *coTS;

    coTS = coroutine_get_thread_state();
    self = coTS->tr_handler;
    coTS->tr_called = 1;
    co = &self->base;

    if (!sigsetjmp(coTS->tr_reenter, 0)) {
        return;
    }

    coroutine_bootstrap(self, co);
}

Coroutine *qemu_coroutine_new(void)
{
    CoroutineSigAltStack *co;
    CoroutineThreadState *coTS;
    struct sigaction sa;
    struct sigaction osa;
    stack_t ss;
    stack_t oss;
    sigset_t sigs;
    sigset_t osigs;
    sigjmp_buf old_env;
    static pthread_mutex_t sigusr2_mutex = PTHREAD_MUTEX_INITIALIZER;


    co = g_malloc0(sizeof(*co));
    co->stack_size = COROUTINE_STACK_SIZE;
    co->stack = qemu_alloc_stack(&co->stack_size);
    co->base.entry_arg = &old_env; /* stash away our jmp_buf */

    coTS = coroutine_get_thread_state();
    coTS->tr_handler = co;

    sigemptyset(&sigs);
    sigaddset(&sigs, SIGUSR2);
    pthread_sigmask(SIG_BLOCK, &sigs, &osigs);
    sa.sa_handler = coroutine_trampoline;
    sigfillset(&sa.sa_mask);
    sa.sa_flags = SA_ONSTACK;

    pthread_mutex_lock(&sigusr2_mutex);
    if (sigaction(SIGUSR2, &sa, &osa) != 0) {
        abort();
    }

    ss.ss_sp = co->stack;
    ss.ss_size = co->stack_size;
    ss.ss_flags = 0;
    if (sigaltstack(&ss, &oss) < 0) {
        abort();
    }

    coTS->tr_called = 0;
    pthread_kill(pthread_self(), SIGUSR2);
    sigfillset(&sigs);
    sigdelset(&sigs, SIGUSR2);
    while (!coTS->tr_called) {
        sigsuspend(&sigs);
    }

    sigaltstack(NULL, &ss);
    ss.ss_flags = SS_DISABLE;
    if (sigaltstack(&ss, NULL) < 0) {
        abort();
    }
    sigaltstack(NULL, &ss);
    if (!(oss.ss_flags & SS_DISABLE)) {
        sigaltstack(&oss, NULL);
    }

    sigaction(SIGUSR2, &osa, NULL);
    pthread_mutex_unlock(&sigusr2_mutex);

    pthread_sigmask(SIG_SETMASK, &osigs, NULL);

    if (!sigsetjmp(old_env, 0)) {
        siglongjmp(coTS->tr_reenter, 1);
    }


    return &co->base;
}

void qemu_coroutine_delete(Coroutine *co_)
{
    CoroutineSigAltStack *co = DO_UPCAST(CoroutineSigAltStack, base, co_);

    qemu_free_stack(co->stack, co->stack_size);
    g_free(co);
}

CoroutineAction qemu_coroutine_switch(Coroutine *from_, Coroutine *to_,
                                      CoroutineAction action)
{
    CoroutineSigAltStack *from = DO_UPCAST(CoroutineSigAltStack, base, from_);
    CoroutineSigAltStack *to = DO_UPCAST(CoroutineSigAltStack, base, to_);
    CoroutineThreadState *s = coroutine_get_thread_state();
    int ret;

    s->current = to_;

    ret = sigsetjmp(from->env, 0);
    if (ret == 0) {
        siglongjmp(to->env, action);
    }
    return ret;
}

Coroutine *qemu_coroutine_self(void)
{
    CoroutineThreadState *s = coroutine_get_thread_state();

    return s->current;
}

bool qemu_in_coroutine(void)
{
    CoroutineThreadState *s = pthread_getspecific(thread_state_key);

    return s && s->current->caller;
}

