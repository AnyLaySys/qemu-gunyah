#include "qemu/osdep.h"
#include <liburing.h>
#include "block/aio.h"
#include "qemu/queue.h"
#include "block/block.h"
#include "block/raw-aio.h"
#include "qemu/coroutine.h"
#include "qemu/defer-call.h"
#include "qapi/error.h"
#include "system/block-backend.h"
#include "trace.h"

#include "qemu/coroutine_int.h"

#define MAX_ENTRIES 128

typedef struct LuringAIOCB {
    Coroutine *co;
    struct io_uring_sqe sqeq;
    ssize_t ret;
    QEMUIOVector *qiov;
    bool is_read;
    QSIMPLEQ_ENTRY(LuringAIOCB) next;

    int total_read;
    QEMUIOVector resubmit_qiov;
} LuringAIOCB;

typedef struct LuringQueue {
    unsigned int in_queue;
    unsigned int in_flight;
    bool blocked;
    QSIMPLEQ_HEAD(, LuringAIOCB) submit_queue;
} LuringQueue;

struct LuringState {
    AioContext *aio_context;

    struct io_uring ring;

    LuringQueue io_q;

    QEMUBH *completion_bh;
};

static void luring_resubmit(LuringState *s, LuringAIOCB *luringcb)
{
    QSIMPLEQ_INSERT_TAIL(&s->io_q.submit_queue, luringcb, next);
    s->io_q.in_queue++;
}

static void luring_resubmit_short_read(LuringState *s, LuringAIOCB *luringcb,
                                       int nread)
{
    QEMUIOVector *resubmit_qiov;
    size_t remaining;

    luringcb->total_read += nread;
    remaining = luringcb->qiov->size - luringcb->total_read;

    resubmit_qiov = &luringcb->resubmit_qiov;
    if (resubmit_qiov->iov == NULL) {
        qemu_iovec_init(resubmit_qiov, luringcb->qiov->niov);
    } else {
        qemu_iovec_reset(resubmit_qiov);
    }
    qemu_iovec_concat(resubmit_qiov, luringcb->qiov, luringcb->total_read,
                      remaining);

    luringcb->sqeq.off += nread;
    luringcb->sqeq.addr = (uintptr_t)luringcb->resubmit_qiov.iov;
    luringcb->sqeq.len = luringcb->resubmit_qiov.niov;

    luring_resubmit(s, luringcb);
}

static void luring_process_completions(LuringState *s)
{
    struct io_uring_cqe *cqes;
    int total_bytes;

    defer_call_begin();

    qemu_bh_schedule(s->completion_bh);

    while (io_uring_peek_cqe(&s->ring, &cqes) == 0) {
        LuringAIOCB *luringcb;
        int ret;

        if (!cqes) {
            break;
        }

        luringcb = io_uring_cqe_get_data(cqes);
        ret = cqes->res;
        io_uring_cqe_seen(&s->ring, cqes);
        cqes = NULL;

        s->io_q.in_flight--;

        total_bytes = ret + luringcb->total_read;

        if (ret < 0) {

            if (ret == -EINTR || ret == -EAGAIN) {
                luring_resubmit(s, luringcb);
                continue;
            }
        } else if (!luringcb->qiov) {
            goto end;
        } else if (total_bytes == luringcb->qiov->size) {
            ret = 0;

        } else {

            if (luringcb->is_read) {
                if (ret > 0) {
                    luring_resubmit_short_read(s, luringcb, ret);
                    continue;
                } else {

                    qemu_iovec_memset(luringcb->qiov, total_bytes, 0,
                                      luringcb->qiov->size - total_bytes);
                    ret = 0;
                }
            } else {
                ret = -ENOSPC;
            }
        }
end:
        luringcb->ret = ret;
        qemu_iovec_destroy(&luringcb->resubmit_qiov);

        assert(luringcb->co->ctx == s->aio_context);
        if (!qemu_coroutine_entered(luringcb->co)) {
            aio_co_wake(luringcb->co);
        }
    }

    qemu_bh_cancel(s->completion_bh);

    defer_call_end();
}

static int ioq_submit(LuringState *s)
{
    int ret = 0;
    LuringAIOCB *luringcb, *luringcb_next;

    while (s->io_q.in_queue > 0) {

        QSIMPLEQ_FOREACH_SAFE(luringcb, &s->io_q.submit_queue, next,
                              luringcb_next) {
            struct io_uring_sqe *sqes = io_uring_get_sqe(&s->ring);
            if (!sqes) {
                break;
            }

            *sqes = luringcb->sqeq;
            QSIMPLEQ_REMOVE_HEAD(&s->io_q.submit_queue, next);
        }
        ret = io_uring_submit(&s->ring);

        if (ret <= 0) {
            if (ret == -EAGAIN || ret == -EINTR) {
                continue;
            }
            break;
        }
        s->io_q.in_flight += ret;
        s->io_q.in_queue  -= ret;
    }
    s->io_q.blocked = (s->io_q.in_queue > 0);

    if (s->io_q.in_flight) {

        luring_process_completions(s);
    }
    return ret;
}

static void luring_process_completions_and_submit(LuringState *s)
{
    luring_process_completions(s);

    if (s->io_q.in_queue > 0) {
        ioq_submit(s);
    }
}

static void qemu_luring_completion_bh(void *opaque)
{
    LuringState *s = opaque;
    luring_process_completions_and_submit(s);
}

static void qemu_luring_completion_cb(void *opaque)
{
    LuringState *s = opaque;
    luring_process_completions_and_submit(s);
}

static bool qemu_luring_poll_cb(void *opaque)
{
    LuringState *s = opaque;

    return io_uring_cq_ready(&s->ring);
}

static void qemu_luring_poll_ready(void *opaque)
{
    LuringState *s = opaque;

    luring_process_completions_and_submit(s);
}

static void ioq_init(LuringQueue *io_q)
{
    QSIMPLEQ_INIT(&io_q->submit_queue);
    io_q->in_queue = 0;
    io_q->in_flight = 0;
    io_q->blocked = false;
}

static void luring_deferred_fn(void *opaque)
{
    LuringState *s = opaque;
    if (!s->io_q.blocked && s->io_q.in_queue > 0) {
        ioq_submit(s);
    }
}

static int luring_do_submit(int fd, LuringAIOCB *luringcb, LuringState *s,
                            uint64_t offset, int type, BdrvRequestFlags flags)
{
    int ret;
    struct io_uring_sqe *sqes = &luringcb->sqeq;

    switch (type) {
    case QEMU_AIO_WRITE:
#ifdef HAVE_IO_URING_PREP_WRITEV2
    {
        int luring_flags = (flags & BDRV_REQ_FUA) ? RWF_DSYNC : 0;
        io_uring_prep_writev2(sqes, fd, luringcb->qiov->iov,
                              luringcb->qiov->niov, offset, luring_flags);
    }
#else
        assert(flags == 0);
        io_uring_prep_writev(sqes, fd, luringcb->qiov->iov,
                             luringcb->qiov->niov, offset);
#endif
        break;
    case QEMU_AIO_ZONE_APPEND:
        io_uring_prep_writev(sqes, fd, luringcb->qiov->iov,
                             luringcb->qiov->niov, offset);
        break;
    case QEMU_AIO_READ:
        io_uring_prep_readv(sqes, fd, luringcb->qiov->iov,
                            luringcb->qiov->niov, offset);
        break;
    case QEMU_AIO_FLUSH:
        io_uring_prep_fsync(sqes, fd, IORING_FSYNC_DATASYNC);
        break;
    default:
        fprintf(stderr, "%s: invalid AIO request type, aborting 0x%x.\n",
                        __func__, type);
        abort();
    }
    io_uring_sqe_set_data(sqes, luringcb);

    QSIMPLEQ_INSERT_TAIL(&s->io_q.submit_queue, luringcb, next);
    s->io_q.in_queue++;
    if (!s->io_q.blocked) {
        if (s->io_q.in_flight + s->io_q.in_queue >= MAX_ENTRIES) {
            ret = ioq_submit(s);
            return ret;
        }

        defer_call(luring_deferred_fn, s);
    }
    return 0;
}

int coroutine_fn luring_co_submit(BlockDriverState *bs, int fd, uint64_t offset,
                                  QEMUIOVector *qiov, int type,
                                  BdrvRequestFlags flags)
{
    int ret;
    AioContext *ctx = qemu_get_current_aio_context();
    LuringState *s = aio_get_linux_io_uring(ctx);
    LuringAIOCB luringcb = {
        .co         = qemu_coroutine_self(),
        .ret        = -EINPROGRESS,
        .qiov       = qiov,
        .is_read    = (type == QEMU_AIO_READ),
    };
    ret = luring_do_submit(fd, &luringcb, s, offset, type, flags);

    if (ret < 0) {
        return ret;
    }

    if (luringcb.ret == -EINPROGRESS) {
        qemu_coroutine_yield();
    }
    return luringcb.ret;
}

void luring_detach_aio_context(LuringState *s, AioContext *old_context)
{
    aio_set_fd_handler(old_context, s->ring.ring_fd,
                       NULL, NULL, NULL, NULL, s);
    qemu_bh_delete(s->completion_bh);
    s->aio_context = NULL;
}

void luring_attach_aio_context(LuringState *s, AioContext *new_context)
{
    s->aio_context = new_context;
    s->completion_bh = aio_bh_new(new_context, qemu_luring_completion_bh, s);
    aio_set_fd_handler(s->aio_context, s->ring.ring_fd,
                       qemu_luring_completion_cb, NULL,
                       qemu_luring_poll_cb, qemu_luring_poll_ready, s);
}

LuringState *luring_init(Error **errp)
{
    int rc;
    LuringState *s = g_new0(LuringState, 1);
    struct io_uring *ring = &s->ring;


    rc = io_uring_queue_init(MAX_ENTRIES, ring, 0);
    if (rc < 0) {
        error_setg_errno(errp, -rc, "failed to init linux io_uring ring");
        g_free(s);
        return NULL;
    }

    ioq_init(&s->io_q);
    return s;

}

void luring_cleanup(LuringState *s)
{
    io_uring_queue_exit(&s->ring);
    g_free(s);
}

bool luring_has_fua(void)
{
#ifdef HAVE_IO_URING_PREP_WRITEV2
    return true;
#else
    return false;
#endif
}
