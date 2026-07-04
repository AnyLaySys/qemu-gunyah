#ifndef STREAM_H
#define STREAM_H

#include "qom/object.h"

#define TYPE_STREAM_SINK "stream-sink"

typedef struct StreamSinkClass StreamSinkClass;
DECLARE_CLASS_CHECKERS(StreamSinkClass, STREAM_SINK,
                       TYPE_STREAM_SINK)
#define STREAM_SINK(obj) \
     INTERFACE_CHECK(StreamSink, (obj), TYPE_STREAM_SINK)

typedef struct StreamSink StreamSink;

typedef void (*StreamCanPushNotifyFn)(void *opaque);

struct StreamSinkClass {
    InterfaceClass parent;
    bool (*can_push)(StreamSink *obj, StreamCanPushNotifyFn notify,
                     void *notify_opaque);
    size_t (*push)(StreamSink *obj, unsigned char *buf, size_t len, bool eop);
};

size_t
stream_push(StreamSink *sink, uint8_t *buf, size_t len, bool eop);

bool
stream_can_push(StreamSink *sink, StreamCanPushNotifyFn notify,
                void *notify_opaque);


#endif /* STREAM_H */
