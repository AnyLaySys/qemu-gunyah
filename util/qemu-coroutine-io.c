#include "qemu/osdep.h"
#include "qemu/sockets.h"
#include "qemu/coroutine.h"
#include "qemu/iov.h"
#include "qemu/main-loop.h"

ssize_t coroutine_fn
qemu_co_sendv_recvv(int sockfd, struct iovec *iov, unsigned iov_cnt,
                    size_t offset, size_t bytes, bool do_send)
{
    size_t done = 0;
    ssize_t ret;
    while (done < bytes) {
        ret = iov_send_recv(sockfd, iov, iov_cnt,
                            offset + done, bytes - done, do_send);
        if (ret > 0) {
            done += ret;
        } else if (ret < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                qemu_coroutine_yield();
            } else if (done == 0) {
                return -errno;
            } else {
                break;
            }
        } else if (ret == 0 && !do_send) {
            break;
        }
    }
    return done;
}

ssize_t coroutine_fn
qemu_co_send_recv(int sockfd, void *buf, size_t bytes, bool do_send)
{
    struct iovec iov = { .iov_base = buf, .iov_len = bytes };
    return qemu_co_sendv_recvv(sockfd, &iov, 1, 0, bytes, do_send);
}

typedef struct {
    AioContext *ctx;
    Coroutine *co;
    int fd;
} FDYieldUntilData;

static void fd_coroutine_enter(void *opaque)
{
    FDYieldUntilData *data = opaque;
    aio_set_fd_handler(data->ctx, data->fd, NULL, NULL, NULL, NULL, NULL);
    qemu_coroutine_enter(data->co);
}

void coroutine_fn yield_until_fd_readable(int fd)
{
    FDYieldUntilData data;

    assert(qemu_in_coroutine());
    data.ctx = qemu_get_current_aio_context();
    data.co = qemu_coroutine_self();
    data.fd = fd;
    aio_set_fd_handler(data.ctx, fd, fd_coroutine_enter, NULL, NULL, NULL,
                       &data);
    qemu_coroutine_yield();
}
