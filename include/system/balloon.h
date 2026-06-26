
#ifndef QEMU_BALLOON_H
#define QEMU_BALLOON_H

#include "exec/cpu-common.h"
#include "qapi/qapi-types-machine.h"

typedef void (QEMUBalloonEvent)(void *opaque, ram_addr_t target);
typedef void (QEMUBalloonStatus)(void *opaque, BalloonInfo *info);

int qemu_add_balloon_handler(QEMUBalloonEvent *event_func,
                             QEMUBalloonStatus *stat_func, void *opaque);
void qemu_remove_balloon_handler(void *opaque);

#endif
