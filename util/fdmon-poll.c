
#include "qemu/osdep.h"
#include "aio-posix.h"
#include "qemu/rcu_queue.h"

#include <pthread.h>

typedef struct FdmonPollState {
    GPollFD *pollfds;
    AioHandler **nodes;
    unsigned npfd;
    unsigned nalloc;
    Notifier cleanup_notifier;
} FdmonPollState;

static pthread_key_t fdmon_poll_key;
static pthread_once_t fdmon_poll_once = PTHREAD_ONCE_INIT;

static void fdmon_poll_state_destroy(void *ptr)
{
    FdmonPollState *s = ptr;
    if (s) {
        g_free(s->pollfds);
        g_free(s->nodes);
        g_free(s);
    }
}

static void fdmon_poll_key_init(void)
{
    pthread_key_create(&fdmon_poll_key, fdmon_poll_state_destroy);
}

static FdmonPollState *fdmon_poll_get_state(void)
{
    pthread_once(&fdmon_poll_once, fdmon_poll_key_init);
    FdmonPollState *s = pthread_getspecific(fdmon_poll_key);
    if (!s) {
        s = g_new0(FdmonPollState, 1);
        pthread_setspecific(fdmon_poll_key, s);
    }
    return s;
}

static void pollfds_cleanup(Notifier *n, void *unused)
{
    FdmonPollState *s = fdmon_poll_get_state();
    g_assert(s->npfd == 0);
    g_free(s->pollfds);
    g_free(s->nodes);
    s->pollfds = NULL;
    s->nodes = NULL;
    s->nalloc = 0;
}

static void add_pollfd(AioHandler *node)
{
    FdmonPollState *s = fdmon_poll_get_state();
    if (s->npfd == s->nalloc) {
        if (s->nalloc == 0) {
            s->cleanup_notifier.notify = pollfds_cleanup;
            qemu_thread_atexit_add(&s->cleanup_notifier);
            s->nalloc = 8;
        } else {
            g_assert(s->nalloc <= INT_MAX);
            s->nalloc *= 2;
        }
        s->pollfds = g_renew(GPollFD, s->pollfds, s->nalloc);
        s->nodes = g_renew(AioHandler *, s->nodes, s->nalloc);
    }
    s->nodes[s->npfd] = node;
    s->pollfds[s->npfd] = (GPollFD) {
        .fd = node->pfd.fd,
        .events = node->pfd.events,
    };
    s->npfd++;
}

static int fdmon_poll_wait(AioContext *ctx, AioHandlerList *ready_list,
                            int64_t timeout)
{
    FdmonPollState *s = fdmon_poll_get_state();
    AioHandler *node;
    int ret;

    assert(s->npfd == 0);

    QLIST_FOREACH_RCU(node, &ctx->aio_handlers, node) {
        if (!QLIST_IS_INSERTED(node, node_deleted) && node->pfd.events) {
            add_pollfd(node);
        }
    }

    if (fdmon_epoll_try_upgrade(ctx, s->npfd)) {
        s->npfd = 0; /* we won't need pollfds[], reset npfd */
        return ctx->fdmon_ops->wait(ctx, ready_list, timeout);
    }

    ret = qemu_poll_ns(s->pollfds, s->npfd, timeout);
    if (ret > 0) {
        int i;

        for (i = 0; i < s->npfd; i++) {
            int revents = s->pollfds[i].revents;

            if (revents) {
                aio_add_ready_handler(ready_list, s->nodes[i], revents);
            }
        }
    }

    s->npfd = 0;
    return ret;
}

static void fdmon_poll_update(AioContext *ctx,
                              AioHandler *old_node,
                              AioHandler *new_node)
{
}

const FDMonOps fdmon_poll_ops = {
    .update = fdmon_poll_update,
    .wait = fdmon_poll_wait,
    .need_wait = aio_poll_disabled,
};
