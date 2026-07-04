
#include "qemu/osdep.h"
#include "hw/qdev-core.h"
#include "hw/qdev-properties.h"
#include "hw/core/cpu.h"
#include "migration/vmstate.h"

static const Property cpu_user_props[] = {
    DEFINE_PROP_BOOL("prctl-unalign-sigbus", CPUState,
                     prctl_unalign_sigbus, false),
};

void cpu_class_init_props(DeviceClass *dc)
{
    device_class_set_props(dc, cpu_user_props);
}

void cpu_exec_class_post_init(CPUClass *cc)
{
}

void cpu_exec_initfn(CPUState *cpu)
{
}

void cpu_vmstate_register(CPUState *cpu)
{
    assert(qdev_get_vmsd(DEVICE(cpu)) == NULL ||
           qdev_get_vmsd(DEVICE(cpu))->unmigratable);
}

void cpu_vmstate_unregister(CPUState *cpu)
{
}
