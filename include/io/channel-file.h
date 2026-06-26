
#ifndef QIO_CHANNEL_FILE_H
#define QIO_CHANNEL_FILE_H

#include "io/channel.h"
#include "qom/object.h"

#define TYPE_QIO_CHANNEL_FILE "qio-channel-file"
OBJECT_DECLARE_SIMPLE_TYPE(QIOChannelFile, QIO_CHANNEL_FILE)



struct QIOChannelFile {
    QIOChannel parent;
    int fd;
};


QIOChannelFile *
qio_channel_file_new_fd(int fd);

QIOChannelFile *
qio_channel_file_new_dupfd(int fd, Error **errp);

QIOChannelFile *
qio_channel_file_new_path(const char *path,
                          int flags,
                          mode_t mode,
                          Error **errp);

#endif /* QIO_CHANNEL_FILE_H */
