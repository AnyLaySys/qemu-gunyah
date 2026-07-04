
#ifndef REPLICATION_H
#define REPLICATION_H

#include "qapi/qapi-types-block-core.h"
#include "qemu/module.h"
#include "qemu/queue.h"

typedef struct ReplicationOps ReplicationOps;
typedef struct ReplicationState ReplicationState;


struct ReplicationState {
    void *opaque;
    ReplicationOps *ops;
    QLIST_ENTRY(ReplicationState) node;
};

struct ReplicationOps {
    void (*start)(ReplicationState *rs, ReplicationMode mode, Error **errp);
    void (*stop)(ReplicationState *rs, bool failover, Error **errp);
    void (*checkpoint)(ReplicationState *rs, Error **errp);
    void (*get_error)(ReplicationState *rs, Error **errp);
};

ReplicationState *replication_new(void *opaque, ReplicationOps *ops);

void replication_remove(ReplicationState *rs);

void replication_start_all(ReplicationMode mode, Error **errp);

void replication_do_checkpoint_all(Error **errp);

void replication_get_error_all(Error **errp);

void replication_stop_all(bool failover, Error **errp);

#endif /* REPLICATION_H */
