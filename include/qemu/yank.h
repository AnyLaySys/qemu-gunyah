
#ifndef YANK_H
#define YANK_H

#include "qapi/qapi-types-yank.h"

typedef void (YankFn)(void *opaque);

bool yank_register_instance(const YankInstance *instance, Error **errp);

void yank_unregister_instance(const YankInstance *instance);

void yank_register_function(const YankInstance *instance,
                            YankFn *func,
                            void *opaque);

void yank_unregister_function(const YankInstance *instance,
                              YankFn *func,
                              void *opaque);

#define BLOCKDEV_YANK_INSTANCE(the_node_name) (&(YankInstance) { \
        .type = YANK_INSTANCE_TYPE_BLOCK_NODE, \
        .u.block_node.node_name = (the_node_name) })

#define CHARDEV_YANK_INSTANCE(the_id) (&(YankInstance) { \
        .type = YANK_INSTANCE_TYPE_CHARDEV, \
        .u.chardev.id = (the_id) })

#define MIGRATION_YANK_INSTANCE (&(YankInstance) { \
        .type = YANK_INSTANCE_TYPE_MIGRATION })

#endif
