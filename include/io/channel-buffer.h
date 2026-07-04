
#ifndef QIO_CHANNEL_BUFFER_H
#define QIO_CHANNEL_BUFFER_H

#include "io/channel.h"
#include "qom/object.h"

#define TYPE_QIO_CHANNEL_BUFFER "qio-channel-buffer"
OBJECT_DECLARE_SIMPLE_TYPE(QIOChannelBuffer, QIO_CHANNEL_BUFFER)



struct QIOChannelBuffer {
    QIOChannel parent;
    size_t capacity; /* Total allocated memory */
    size_t usage;    /* Current size of data */
    size_t offset;   /* Offset for future I/O ops */
    uint8_t *data;
};


QIOChannelBuffer *
qio_channel_buffer_new(size_t capacity);

#endif /* QIO_CHANNEL_BUFFER_H */
