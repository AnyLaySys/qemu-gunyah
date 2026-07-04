
#ifndef QIO_CHANNEL_UTIL_H
#define QIO_CHANNEL_UTIL_H

#include "io/channel.h"



QIOChannel *qio_channel_new_fd(int fd,
                               Error **errp);

void qio_channel_util_set_aio_fd_handler(int read_fd,
                                         AioContext *read_ctx,
                                         IOHandler *io_read,
                                         int write_fd,
                                         AioContext *write_ctx,
                                         IOHandler *io_write,
                                         void *opaque);

#endif /* QIO_CHANNEL_UTIL_H */
