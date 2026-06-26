#ifndef VMCOREINFO_H
#define VMCOREINFO_H

#include "hw/qdev-core.h"
#include "standard-headers/linux/qemu_fw_cfg.h"
#include "qom/object.h"

#define TYPE_VMCOREINFO "vmcoreinfo"
typedef struct VMCoreInfoState VMCoreInfoState;
DECLARE_INSTANCE_CHECKER(VMCoreInfoState, VMCOREINFO, TYPE_VMCOREINFO)

typedef struct fw_cfg_vmcoreinfo FWCfgVMCoreInfo;

struct VMCoreInfoState {
    DeviceState parent_obj;

    bool has_vmcoreinfo;
    FWCfgVMCoreInfo vmcoreinfo;
};

static inline VMCoreInfoState *vmcoreinfo_find(void)
{
    Object *o = object_resolve_path_type("", TYPE_VMCOREINFO, NULL);

    return o ? VMCOREINFO(o) : NULL;
}

#endif
