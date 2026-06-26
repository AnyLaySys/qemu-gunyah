
#ifndef QIO_CHANNEL_WATCH_H
#define QIO_CHANNEL_WATCH_H

#include "io/channel.h"


GSource *qio_channel_create_fd_watch(QIOChannel *ioc,
                                     int fd,
                                     GIOCondition condition);

GSource *qio_channel_create_socket_watch(QIOChannel *ioc,
                                         int fd,
                                         GIOCondition condition);

GSource *qio_channel_create_fd_pair_watch(QIOChannel *ioc,
                                          int fdread,
                                          int fdwrite,
                                          GIOCondition condition);

#endif /* QIO_CHANNEL_WATCH_H */
