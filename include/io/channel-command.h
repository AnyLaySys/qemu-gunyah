
#ifndef QIO_CHANNEL_COMMAND_H
#define QIO_CHANNEL_COMMAND_H

#include "io/channel.h"
#include "qom/object.h"

#define TYPE_QIO_CHANNEL_COMMAND "qio-channel-command"
OBJECT_DECLARE_SIMPLE_TYPE(QIOChannelCommand, QIO_CHANNEL_COMMAND)




struct QIOChannelCommand {
    QIOChannel parent;
    int writefd;
    int readfd;
    GPid pid;
#ifdef WIN32
    bool blocking;
#endif
};


QIOChannelCommand *
qio_channel_command_new_spawn(const char *const argv[],
                              int flags,
                              Error **errp);


#endif /* QIO_CHANNEL_COMMAND_H */
