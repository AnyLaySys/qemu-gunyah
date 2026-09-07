#include "qemu/osdep.h"
#include <poll.h>
#include "qemu/rcu_queue.h"
#include "aio-posix.h"

enum {
    FDMON_IO_URING_ENTRIES  = 128,

    FDMON_IO_URING_PENDING  = (1 << 0),
    FDMON_IO_URING_ADD      = (1 << 1),
    FDMON_IO_URING_REMOVE   = (1 << 2),
};

static inline int poll_events_from_pfd(int pfd_events)
{
    return (pfd_events & G_IO_IN ? POLLIN : 0) |
           (pfd_events & G_IO_OUT ? POLLOUT : 0) |
           (pfd_events & G_IO_HUP ? POLLHUP : 0) |
           (pfd_events & G_IO_ERR ? POLLERR : 0);
}

static inline int pfd_events_from_poll(int poll_events)
{
    return (poll_events & POLLIN ? G_IO_IN : 0) |
           (poll_events & POLLOUT ? G_IO_OUT : 0) |
           (poll_events & POLLHUP ? G_IO_HUP : 0) |
           (poll_events & POLLERR ? G_IO_ERR : 0);
}

static struct io_uring_sqe *get_sqe(AioContext *ctx)
{
    struct io_uring *ring = &ctx->fdmon_io_uring;
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    int ret;

    if (likely(sqe)) {
        return sqe;
    }

    do {
        ret = io_uring_submit(ring);
    } while (ret == -EINTR);

    assert(ret > 1);
    sqe = io_uring_get_sqe(ring);
    assert(sqe);
    return sqe;
}

static void enqueue(AioHandlerSList *head, AioHandler *node, unsigned flags)
{
    unsigned old_flags;

    old_flags = qatomic_fetch_or(&node->flags, FDMON_IO_URING_PENDING | flags);
    if (!(old_flags & FDMON_IO_URING_PENDING)) {
        QSLIST_INSERT_HEAD_ATOMIC(head, node, node_submitted);
    }
}

static AioHandler *dequeue(AioHandlerSList *head, unsigned *flags)
{
    AioHandler *node = QSLIST_FIRST(head);

    if (!node) {
        return NULL;
    }

    QSLIST_REMOVE_HEAD(head, node_submitted);

    *flags = qatomic_fetch_and(&node->flags, ~(FDMON_IO_URING_PENDING |
                                              FDMON_IO_URING_ADD));
    return node;
}

static void fdmon_io_uring_update(AioContext *ctx,
                                  AioHandler *old_node,
                                  AioHandler *new_node)
{
    if (new_node) {
        enqueue(&ctx->submit_list, new_node, FDMON_IO_URING_ADD);
    }

    if (old_node) {

        assert(!QLIST_IS_INSERTED(old_node, node_deleted));
        old_node->node_deleted.le_prev = &old_node->node_deleted.le_next;

        enqueue(&ctx->submit_list, old_node, FDMON_IO_URING_REMOVE);
    }
}

static void add_poll_add_sqe(AioContext *ctx, AioHandler *node)
{
    struct io_uring_sqe *sqe = get_sqe(ctx);
    int events = poll_events_from_pfd(node->pfd.events);

    io_uring_prep_poll_add(sqe, node->pfd.fd, events);
    io_uring_sqe_set_data(sqe, node);
}

static void add_poll_remove_sqe(AioContext *ctx, AioHandler *node)
{
    struct io_uring_sqe *sqe = get_sqe(ctx);

#ifdef LIBURING_HAVE_DATA64
    io_uring_prep_poll_remove(sqe, (uintptr_t)node);
#else
    io_uring_prep_poll_remove(sqe, node);
#endif
    io_uring_sqe_set_data(sqe, NULL);
}

static void add_timeout_sqe(AioContext *ctx, int64_t ns)
{
    struct io_uring_sqe *sqe;
    struct __kernel_timespec ts = {
        .tv_sec = ns / NANOSECONDS_PER_SECOND,
        .tv_nsec = ns % NANOSECONDS_PER_SECOND,
    };

    sqe = get_sqe(ctx);
    io_uring_prep_timeout(sqe, &ts, 1, 0);
    io_uring_sqe_set_data(sqe, NULL);
}

static void fill_sq_ring(AioContext *ctx)
{
    AioHandlerSList submit_list;
    AioHandler *node;
    unsigned flags;

    QSLIST_MOVE_ATOMIC(&submit_list, &ctx->submit_list);

    while ((node = dequeue(&submit_list, &flags))) {

        if (flags & FDMON_IO_URING_ADD) {
            add_poll_add_sqe(ctx, node);
        }
        if (flags & FDMON_IO_URING_REMOVE) {
            add_poll_remove_sqe(ctx, node);
        }
    }
}

static bool process_cqe(AioContext *ctx,
                        AioHandlerList *ready_list,
                        struct io_uring_cqe *cqe)
{
    AioHandler *node = io_uring_cqe_get_data(cqe);
    unsigned flags;

    if (!node) {
        return false;
    }

    flags = qatomic_fetch_and(&node->flags, ~FDMON_IO_URING_REMOVE);
    if (flags & FDMON_IO_URING_REMOVE) {
        QLIST_INSERT_HEAD_RCU(&ctx->deleted_aio_handlers, node, node_deleted);
        return false;
    }

    aio_add_ready_handler(ready_list, node, pfd_events_from_poll(cqe->res));

    add_poll_add_sqe(ctx, node);
    return true;
}

static int process_cq_ring(AioContext *ctx, AioHandlerList *ready_list)
{
    struct io_uring *ring = &ctx->fdmon_io_uring;
    struct io_uring_cqe *cqe;
    unsigned num_cqes = 0;
    unsigned num_ready = 0;
    unsigned head;

    io_uring_for_each_cqe(ring, head, cqe) {
        if (process_cqe(ctx, ready_list, cqe)) {
            num_ready++;
        }

        num_cqes++;
    }

    io_uring_cq_advance(ring, num_cqes);
    return num_ready;
}

static int fdmon_io_uring_wait(AioContext *ctx, AioHandlerList *ready_list,
                               int64_t timeout)
{
    unsigned wait_nr = 1;
    int ret;

    if (timeout == 0) {
        wait_nr = 0;
    } else if (timeout > 0) {
        add_timeout_sqe(ctx, timeout);
    }

    fill_sq_ring(ctx);

    do {
        ret = io_uring_submit_and_wait(&ctx->fdmon_io_uring, wait_nr);
    } while (ret == -EINTR);

    assert(ret >= 0);

    return process_cq_ring(ctx, ready_list);
}

static bool fdmon_io_uring_need_wait(AioContext *ctx)
{

    if (io_uring_cq_ready(&ctx->fdmon_io_uring)) {
        return true;
    }

    if (io_uring_sq_ready(&ctx->fdmon_io_uring)) {
        return true;
    }

    if (!QSLIST_EMPTY_RCU(&ctx->submit_list)) {
        return true;
    }

    return false;
}

static const FDMonOps fdmon_io_uring_ops = {
    .update = fdmon_io_uring_update,
    .wait = fdmon_io_uring_wait,
    .need_wait = fdmon_io_uring_need_wait,
};

bool fdmon_io_uring_setup(AioContext *ctx)
{
    int ret;

    ret = io_uring_queue_init(FDMON_IO_URING_ENTRIES, &ctx->fdmon_io_uring, 0);
    if (ret != 0) {
        return false;
    }

    QSLIST_INIT(&ctx->submit_list);
    ctx->fdmon_ops = &fdmon_io_uring_ops;
    return true;
}

void fdmon_io_uring_destroy(AioContext *ctx)
{
    if (ctx->fdmon_ops == &fdmon_io_uring_ops) {
        AioHandler *node;

        io_uring_queue_exit(&ctx->fdmon_io_uring);

        while ((node = QSLIST_FIRST_RCU(&ctx->submit_list))) {
            unsigned flags = qatomic_fetch_and(&node->flags,
                    ~(FDMON_IO_URING_PENDING |
                      FDMON_IO_URING_ADD |
                      FDMON_IO_URING_REMOVE));

            if (flags & FDMON_IO_URING_REMOVE) {
                QLIST_INSERT_HEAD_RCU(&ctx->deleted_aio_handlers, node, node_deleted);
            }

            QSLIST_REMOVE_HEAD_RCU(&ctx->submit_list, node_submitted);
        }

        ctx->fdmon_ops = &fdmon_poll_ops;
    }
}
