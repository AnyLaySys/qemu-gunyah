
#include "qemu/osdep.h"
#include "qemu/coroutine-tls.h"
#include "qemu/notify.h"
#include "qemu/thread.h"
#include "qemu/defer-call.h"

typedef struct {
    void (*fn)(void *);
    void *opaque;
} DeferredCall;

typedef struct {
    unsigned nesting_level;
    GArray *deferred_call_array;
} DeferCallThreadState;

QEMU_DEFINE_STATIC_CO_TLS(DeferCallThreadState, defer_call_thread_state);

static void defer_call_atexit(Notifier *n, void *value)
{
    DeferCallThreadState *thread_state = get_ptr_defer_call_thread_state();
    g_array_free(thread_state->deferred_call_array, TRUE);
}

static Notifier defer_call_atexit_notifier;

void defer_call(void (*fn)(void *), void *opaque)
{
    DeferCallThreadState *thread_state = get_ptr_defer_call_thread_state();

    if (thread_state->nesting_level == 0) {
        fn(opaque);
        return;
    }

    GArray *array = thread_state->deferred_call_array;
    if (!array) {
        array = g_array_new(FALSE, FALSE, sizeof(DeferredCall));
        thread_state->deferred_call_array = array;
        defer_call_atexit_notifier.notify = defer_call_atexit;
        qemu_thread_atexit_add(&defer_call_atexit_notifier);
    }

    DeferredCall *fns = (DeferredCall *)array->data;
    DeferredCall new_fn = {
        .fn = fn,
        .opaque = opaque,
    };

    for (guint i = 0; i < array->len; i++) {
        if (memcmp(&fns[i], &new_fn, sizeof(new_fn)) == 0) {
            return; /* already exists */
        }
    }

    g_array_append_val(array, new_fn);
}

void defer_call_begin(void)
{
    DeferCallThreadState *thread_state = get_ptr_defer_call_thread_state();

    assert(thread_state->nesting_level < UINT32_MAX);

    thread_state->nesting_level++;
}

void defer_call_end(void)
{
    DeferCallThreadState *thread_state = get_ptr_defer_call_thread_state();

    assert(thread_state->nesting_level > 0);

    if (--thread_state->nesting_level > 0) {
        return;
    }

    GArray *array = thread_state->deferred_call_array;
    if (!array) {
        return;
    }

    DeferredCall *fns = (DeferredCall *)array->data;

    for (guint i = 0; i < array->len; i++) {
        fns[i].fn(fns[i].opaque);
    }

    g_array_set_size(array, 0);
}
