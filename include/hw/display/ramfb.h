#ifndef RAMFB_H
#define RAMFB_H

#include "state/vmstate.h"

typedef struct RAMFBState RAMFBState;
void ramfb_display_update(QemuConsole *con, RAMFBState *s);
RAMFBState *ramfb_setup(Error **errp);

extern const VMStateDescription ramfb_vmstate;

#define TYPE_RAMFB_DEVICE "ramfb"

#endif /* RAMFB_H */
