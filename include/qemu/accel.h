#ifndef QEMU_ACCEL_H
#define QEMU_ACCEL_H

#include "qom/object.h"
#include "exec/hwaddr.h"

struct AccelState {
    Object parent_obj;
};

typedef struct AccelClass {
    ObjectClass parent_class;

    const char *name;
    int (*init_machine)(MachineState *ms);
#ifndef CONFIG_USER_ONLY
    void (*setup_post)(MachineState *ms, AccelState *accel);
    bool (*has_memory)(MachineState *ms, AddressSpace *as,
                       hwaddr start_addr, hwaddr size);
#endif
    bool (*cpu_common_realize)(CPUState *cpu, Error **errp);
    void (*cpu_common_unrealize)(CPUState *cpu);

    bool *allowed;
    GPtrArray *compat_props;
} AccelClass;

#define TYPE_ACCEL "accel"

#define ACCEL_CLASS_SUFFIX  "-" TYPE_ACCEL
#define ACCEL_CLASS_NAME(a) (a ACCEL_CLASS_SUFFIX)

#define ACCEL_CLASS(klass) \
    OBJECT_CLASS_CHECK(AccelClass, (klass), TYPE_ACCEL)
#define ACCEL(obj) \
    OBJECT_CHECK(AccelState, (obj), TYPE_ACCEL)
#define ACCEL_GET_CLASS(obj) \
    OBJECT_GET_CLASS(AccelClass, (obj), TYPE_ACCEL)

AccelClass *accel_find(const char *opt_name);
AccelState *current_accel(void);
const char *current_accel_name(void);

void accel_init_interfaces(AccelClass *ac);

#ifndef CONFIG_USER_ONLY
int accel_init_machine(AccelState *accel, MachineState *ms);

void accel_setup_post(MachineState *ms);
#endif /* !CONFIG_USER_ONLY */

void accel_cpu_instance_init(CPUState *cpu);

bool accel_cpu_common_realize(CPUState *cpu, Error **errp);

void accel_cpu_common_unrealize(CPUState *cpu);

#endif /* QEMU_ACCEL_H */
