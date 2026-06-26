
#ifndef QEMU_PROGRESS_METER_H
#define QEMU_PROGRESS_METER_H

#include "qemu/thread.h"

typedef struct ProgressMeter {
    uint64_t current;

    uint64_t total;

    QemuMutex lock; /* protects concurrent access to above fields */
} ProgressMeter;

void progress_init(ProgressMeter *pm);
void progress_destroy(ProgressMeter *pm);

void progress_get_snapshot(ProgressMeter *pm, uint64_t *current,
                           uint64_t *total);

void progress_work_done(ProgressMeter *pm, uint64_t done);

void progress_set_remaining(ProgressMeter *pm, uint64_t remaining);

void progress_increase_remaining(ProgressMeter *pm, uint64_t delta);

#endif /* QEMU_PROGRESS_METER_H */
