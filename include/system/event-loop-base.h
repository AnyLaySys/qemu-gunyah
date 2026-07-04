#ifndef QEMU_EVENT_LOOP_BASE_H
#define QEMU_EVENT_LOOP_BASE_H

#include "qom/object.h"
#include "block/aio.h"

#define TYPE_EVENT_LOOP_BASE         "event-loop-base"
OBJECT_DECLARE_TYPE(EventLoopBase, EventLoopBaseClass,
                    EVENT_LOOP_BASE)

struct EventLoopBaseClass {
    ObjectClass parent_class;

    void (*init)(EventLoopBase *base, Error **errp);
    void (*update_params)(EventLoopBase *base, Error **errp);
    bool (*can_be_deleted)(EventLoopBase *base);
};

struct EventLoopBase {
    Object parent;

    int64_t aio_max_batch;

    int64_t thread_pool_min;
    int64_t thread_pool_max;
};
#endif
