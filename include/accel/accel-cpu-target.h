
#ifndef ACCEL_CPU_TARGET_H
#define ACCEL_CPU_TARGET_H


#include "qom/object.h"
#include "cpu.h"

#define TYPE_ACCEL_CPU "accel-" CPU_RESOLVING_TYPE
#define ACCEL_CPU_NAME(name) (name "-" TYPE_ACCEL_CPU)
typedef struct AccelCPUClass AccelCPUClass;
DECLARE_CLASS_CHECKERS(AccelCPUClass, ACCEL_CPU, TYPE_ACCEL_CPU)

typedef struct AccelCPUClass {
    ObjectClass parent_class;

    void (*cpu_class_init)(CPUClass *cc);
    void (*cpu_instance_init)(CPUState *cpu);
    bool (*cpu_target_realize)(CPUState *cpu, Error **errp);
} AccelCPUClass;

#endif /* ACCEL_CPU_H */
