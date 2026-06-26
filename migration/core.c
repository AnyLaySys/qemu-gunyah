
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "exec/cpu-common.h"
#include "exec/memory.h"
#include "migration/qemu-file.h"
#include "migration/register.h"
#include "migration/vmstate.h"
#include "qemu/timer.h"

int register_savevm_live(const char *idstr, uint32_t instance_id,
                         int version_id, const SaveVMHandlers *ops,
                         void *opaque)
{
    return 0;
}

void unregister_savevm(VMStateIf *obj, const char *idstr, void *opaque)
{
}

int vmstate_register_with_alias_id(VMStateIf *obj, uint32_t instance_id,
                                   const VMStateDescription *vmsd,
                                   void *opaque, int alias_id,
                                   int required_for_version,
                                   Error **errp)
{
    return 0;
}

int vmstate_replace_hack_for_ppc(VMStateIf *obj, int instance_id,
                                 const VMStateDescription *vmsd,
                                 void *opaque)
{
    return 0;
}

void vmstate_unregister(VMStateIf *obj, const VMStateDescription *vmsd,
                        void *opaque)
{
}

void vmstate_register_ram(MemoryRegion *mr, DeviceState *dev)
{
    qemu_ram_set_idstr(mr->ram_block, memory_region_name(mr), dev);
}

void vmstate_unregister_ram(MemoryRegion *mr, DeviceState *dev)
{
    qemu_ram_unset_idstr(mr->ram_block);
}

void vmstate_register_ram_global(MemoryRegion *mr)
{
    vmstate_register_ram(mr, NULL);
}

bool vmstate_check_only_migratable(const VMStateDescription *vmsd)
{
    return true;
}

void timer_put(QEMUFile *f, QEMUTimer *ts)
{
    qemu_put_be64(f, timer_expire_time_ns(ts));
}

void timer_get(QEMUFile *f, QEMUTimer *ts)
{
    uint64_t expire_time = qemu_get_be64(f);

    if (expire_time != -1) {
        timer_mod_ns(ts, expire_time);
    } else {
        timer_del(ts);
    }
}

static int get_timer(QEMUFile *f, void *pv, size_t size,
                     const VMStateField *field)
{
    timer_get(f, pv);
    return 0;
}

static int put_timer(QEMUFile *f, void *pv, size_t size,
                     const VMStateField *field, JSONWriter *vmdesc)
{
    timer_put(f, pv);
    return 0;
}

const VMStateInfo vmstate_info_timer = {
    .name = "timer",
    .get = get_timer,
    .put = put_timer,
};
