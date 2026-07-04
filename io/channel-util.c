
#include "qemu/osdep.h"
#include "io/channel-util.h"
#include "io/channel-file.h"
#include "io/channel-socket.h"


QIOChannel *qio_channel_new_fd(int fd,
                               Error **errp)
{
    QIOChannel *ioc;

    if (fd_is_socket(fd)) {
        ioc = QIO_CHANNEL(qio_channel_socket_new_fd(fd, errp));
    } else {
        ioc = QIO_CHANNEL(qio_channel_file_new_fd(fd));
    }
    return ioc;
}


void qio_channel_util_set_aio_fd_handler(int read_fd,
                                         AioContext *read_ctx,
                                         IOHandler *io_read,
                                         int write_fd,
                                         AioContext *write_ctx,
                                         IOHandler *io_write,
                                         void *opaque)
{
    if (read_fd == write_fd && read_ctx == write_ctx) {
        aio_set_fd_handler(read_ctx, read_fd, io_read, io_write,
                NULL, NULL, opaque);
    } else {
        if (read_ctx) {
            aio_set_fd_handler(read_ctx, read_fd, io_read, NULL,
                    NULL, NULL, opaque);
        }
        if (write_ctx) {
            aio_set_fd_handler(write_ctx, write_fd, NULL, io_write,
                    NULL, NULL, opaque);
        }
    }
}
