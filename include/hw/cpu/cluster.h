#ifndef HW_CPU_CLUSTER_H
#define HW_CPU_CLUSTER_H

#include "hw/qdev-core.h"
#include "qom/object.h"


#define TYPE_CPU_CLUSTER "cpu-cluster"
OBJECT_DECLARE_SIMPLE_TYPE(CPUClusterState, CPU_CLUSTER)

#define MAX_CLUSTERS 255

struct CPUClusterState {
    DeviceState parent_obj;

    uint32_t cluster_id;
};

#endif
