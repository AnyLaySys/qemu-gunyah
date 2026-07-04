
#ifndef ACCEL_OPS_H
#define ACCEL_OPS_H

#include "exec/vaddr.h"
#include "qom/object.h"

#define ACCEL_OPS_SUFFIX "-ops"
#define TYPE_ACCEL_OPS "accel" ACCEL_OPS_SUFFIX
#define ACCEL_OPS_NAME(name) (name "-" TYPE_ACCEL_OPS)

DECLARE_CLASS_CHECKERS(AccelOpsClass, ACCEL_OPS, TYPE_ACCEL_OPS)

struct AccelOpsClass {
    ObjectClass parent_class;

    void (*ops_init)(AccelOpsClass *ops);

    bool (*cpus_are_resettable)(void);
    void (*cpu_reset_hold)(CPUState *cpu);

    void (*create_vcpu_thread)(CPUState *cpu); /* MANDATORY NON-NULL */
    void (*kick_vcpu_thread)(CPUState *cpu);
    bool (*cpu_thread_is_idle)(CPUState *cpu);

    void (*synchronize_post_reset)(CPUState *cpu);
    void (*synchronize_post_init)(CPUState *cpu);
    void (*synchronize_state)(CPUState *cpu);
    void (*synchronize_pre_loadvm)(CPUState *cpu);
    void (*synchronize_pre_resume)(bool step_pending);

    void (*handle_interrupt)(CPUState *cpu, int mask);

    int64_t (*get_virtual_clock)(void);
    void (*set_virtual_clock)(int64_t time);

    int64_t (*get_elapsed_ticks)(void);

    bool (*supports_guest_debug)(void);
    int (*update_guest_debug)(CPUState *cpu);
    int (*insert_breakpoint)(CPUState *cpu, int type, vaddr addr, vaddr len);
    int (*remove_breakpoint)(CPUState *cpu, int type, vaddr addr, vaddr len);
    void (*remove_all_breakpoints)(CPUState *cpu);
};

#endif /* ACCEL_OPS_H */
