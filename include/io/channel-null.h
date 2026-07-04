
#ifndef QIO_CHANNEL_FILE_H
#define QIO_CHANNEL_FILE_H

#include "io/channel.h"
#include "qom/object.h"

#define TYPE_QIO_CHANNEL_NULL "qio-channel-null"
OBJECT_DECLARE_SIMPLE_TYPE(QIOChannelNull, QIO_CHANNEL_NULL)



struct QIOChannelNull {
    QIOChannel parent;
    bool closed;
};


QIOChannelNull *
qio_channel_null_new(void);

#endif /* QIO_CHANNEL_NULL_H */
