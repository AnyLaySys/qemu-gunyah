
#include "qemu/osdep.h"
#include "qemu/coroutine.h"
#include "qemu/progress_meter.h"

void progress_init(ProgressMeter *pm)
{
    qemu_mutex_init(&pm->lock);
}

void progress_destroy(ProgressMeter *pm)
{
    qemu_mutex_destroy(&pm->lock);
}

void progress_get_snapshot(ProgressMeter *pm, uint64_t *current,
                           uint64_t *total)
{
    QEMU_LOCK_GUARD(&pm->lock);

    *current = pm->current;
    *total = pm->total;
}

void progress_work_done(ProgressMeter *pm, uint64_t done)
{
    QEMU_LOCK_GUARD(&pm->lock);
    pm->current += done;
}

void progress_set_remaining(ProgressMeter *pm, uint64_t remaining)
{
    QEMU_LOCK_GUARD(&pm->lock);
    pm->total = pm->current + remaining;
}

void progress_increase_remaining(ProgressMeter *pm, uint64_t delta)
{
    QEMU_LOCK_GUARD(&pm->lock);
    pm->total += delta;
}
