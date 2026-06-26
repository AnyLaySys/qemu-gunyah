#ifndef CHAR_IO_H
#define CHAR_IO_H

#include "io/channel.h"
#include "chardev/char.h"
#include "qemu/main-loop.h"

GSource *io_add_watch_poll(Chardev *chr,
                        QIOChannel *ioc,
                        IOCanReadHandler *fd_can_read,
                        QIOChannelFunc fd_read,
                        gpointer user_data,
                        GMainContext *context);

void remove_fd_in_watch(Chardev *chr);

int io_channel_send(QIOChannel *ioc, const void *buf, size_t len);

int io_channel_send_full(QIOChannel *ioc, const void *buf, size_t len,
                         int *fds, size_t nfds);

#endif /* CHAR_IO_H */
