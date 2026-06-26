#ifndef HW_CPU_CORE_H
#define HW_CPU_CORE_H

#include "hw/qdev-core.h"
#include "qom/object.h"

#define TYPE_CPU_CORE "cpu-core"

OBJECT_DECLARE_SIMPLE_TYPE(CPUCore, CPU_CORE)

struct CPUCore {
    DeviceState parent_obj;

    int core_id;
    int nr_threads;
};


#define CPU_CORE_PROP_CORE_ID "core-id"

#endif
