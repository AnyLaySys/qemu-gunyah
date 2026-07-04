
#ifndef QEMU_SYSTEM_RESET_H
#define QEMU_SYSTEM_RESET_H

#include "hw/resettable.h"
#include "qapi/qapi-events-run-state.h"

typedef void QEMUResetHandler(void *opaque);

void qemu_register_resettable(Object *obj);

void qemu_unregister_resettable(Object *obj);

void qemu_register_reset(QEMUResetHandler *func, void *opaque);

void qemu_register_reset_nosnapshotload(QEMUResetHandler *func, void *opaque);

void qemu_unregister_reset(QEMUResetHandler *func, void *opaque);

void qemu_devices_reset(ResetType type);

#endif
