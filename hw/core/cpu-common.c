

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/core/cpu.h"
#include "system/hw_accel.h"
#include "qemu/log.h"
#include "qemu/main-loop.h"
#include "qemu/lockcnt.h"
#include "exec/log.h"
#include "hw/boards.h"
#include "hw/qdev-properties.h"
#include "trace.h"

CPUState *cpu_by_arch_id(int64_t id)
{
    CPUState *cpu;

    CPU_FOREACH(cpu) {
        if (cpu->cc->get_arch_id(cpu) == id) {
            return cpu;
        }
    }
    return NULL;
}

bool cpu_exists(int64_t id)
{
    return !!cpu_by_arch_id(id);
}

CPUState *cpu_create(const char *typename)
{
    Error *err = NULL;
    CPUState *cpu = CPU(object_new(typename));
    if (!qdev_realize(DEVICE(cpu), NULL, &err)) {
        error_report_err(err);
        object_unref(OBJECT(cpu));
        exit(EXIT_FAILURE);
    }
    return cpu;
}


void cpu_reset_interrupt(CPUState *cpu, int mask)
{
    bool need_lock = !bql_locked();

    if (need_lock) {
        bql_lock();
    }
    cpu->interrupt_request &= ~mask;
    if (need_lock) {
        bql_unlock();
    }
}

void cpu_exit(CPUState *cpu)
{
    qatomic_set(&cpu->exit_request, 1);
    
    smp_wmb();
    qatomic_set(&cpu->neg.icount_decr.u16.high, -1);
}

void cpu_dump_state(CPUState *cpu, FILE *f, int flags)
{
    if (cpu->cc->dump_state) {
        cpu_synchronize_state(cpu);
        cpu->cc->dump_state(cpu, f, flags);
    }
}

void cpu_reset(CPUState *cpu)
{
    device_cold_reset(DEVICE(cpu));

    trace_cpu_reset(cpu->cpu_index);
}

static void cpu_common_reset_hold(Object *obj, ResetType type)
{
    CPUState *cpu = CPU(obj);

    if (qemu_loglevel_mask(CPU_LOG_RESET)) {
        qemu_log("CPU Reset (CPU %d)\n", cpu->cpu_index);
        log_cpu_state(cpu, cpu->cc->reset_dump_flags);
    }

    cpu->interrupt_request = 0;
    cpu->halted = cpu->start_powered_off;
    cpu->mem_io_pc = 0;
    cpu->icount_extra = 0;
    qatomic_set(&cpu->neg.icount_decr.u32, 0);
    cpu->neg.can_do_io = true;
    cpu->exception_index = -1;
    cpu->crash_occurred = false;
    cpu->cflags_next_tb = -1;

    cpu_exec_reset_hold(cpu);
}

ObjectClass *cpu_class_by_name(const char *typename, const char *cpu_model)
{
    ObjectClass *oc;
    CPUClass *cc;

    oc = object_class_by_name(typename);
    cc = CPU_CLASS(oc);
    assert(cc->class_by_name);
    assert(cpu_model);
    oc = cc->class_by_name(cpu_model);
    if (object_class_dynamic_cast(oc, typename) &&
        !object_class_is_abstract(oc)) {
        return oc;
    }

    return NULL;
}

static void cpu_common_parse_features(const char *typename, char *features,
                                      Error **errp)
{
    char *val;
    static bool cpu_globals_initialized;
    
    char *saveptr = NULL;
    char *featurestr = features ? strtok_r(features, ",", &saveptr) : NULL;

    
    assert(!cpu_globals_initialized);
    cpu_globals_initialized = true;

    while (featurestr) {
        val = strchr(featurestr, '=');
        if (val) {
            GlobalProperty *prop = g_new0(typeof(*prop), 1);
            *val = 0;
            val++;
            prop->driver = typename;
            prop->property = g_strdup(featurestr);
            prop->value = g_strdup(val);
            qdev_prop_register_global(prop);
        } else {
            error_setg(errp, "Expected key=value format, found %s.",
                       featurestr);
            return;
        }
        featurestr = strtok_r(NULL, ",", &saveptr);
    }
}

bool cpu_exec_realizefn(CPUState *cpu, Error **errp)
{
    if (!accel_cpu_common_realize(cpu, errp)) {
        return false;
    }

    
    cpu_list_add(cpu);

    cpu_vmstate_register(cpu);

    return true;
}

static void cpu_common_realizefn(DeviceState *dev, Error **errp)
{
    CPUState *cpu = CPU(dev);
    Object *machine = qdev_get_machine();

    
    if (object_dynamic_cast(machine, TYPE_MACHINE)) {
        MachineClass *mc = MACHINE_GET_CLASS(machine);

        if (mc) {
            cpu->ignore_memory_transaction_failures =
                mc->ignore_memory_transaction_failures;
        }
    }

    if (dev->hotplugged) {
        cpu_synchronize_post_init(cpu);
        cpu_resume(cpu);
    }

    
}

static void cpu_common_unrealizefn(DeviceState *dev)
{
    CPUState *cpu = CPU(dev);

    
    
    cpu_exec_unrealizefn(cpu);
}

void cpu_exec_unrealizefn(CPUState *cpu)
{
    cpu_vmstate_unregister(cpu);

    cpu_list_remove(cpu);
    
    accel_cpu_common_unrealize(cpu);
}

static void cpu_common_initfn(Object *obj)
{
    CPUState *cpu = CPU(obj);

    cpu_exec_class_post_init(CPU_GET_CLASS(obj));

    
    cpu->cc = CPU_GET_CLASS(cpu);

    cpu->cpu_index = UNASSIGNED_CPU_INDEX;
    cpu->cluster_index = UNASSIGNED_CLUSTER_INDEX;
    cpu->as = NULL;
    cpu->num_ases = 0;
    
    
    cpu->nr_threads = 1;

    
    cpu->thread = g_new0(QemuThread, 1);
    cpu->halt_cond = g_new0(QemuCond, 1);
    qemu_cond_init(cpu->halt_cond);

    qemu_mutex_init(&cpu->work_mutex);
    qemu_lockcnt_init(&cpu->in_ioctl_lock);
    QSIMPLEQ_INIT(&cpu->work_list);
    QTAILQ_INIT(&cpu->breakpoints);
    QTAILQ_INIT(&cpu->watchpoints);

    cpu_exec_initfn(cpu);

    
}

static void cpu_common_finalize(Object *obj)
{
    CPUState *cpu = CPU(obj);

    free_queued_cpu_work(cpu);
    
    qemu_lockcnt_destroy(&cpu->in_ioctl_lock);
    qemu_mutex_destroy(&cpu->work_mutex);
    qemu_cond_destroy(cpu->halt_cond);
    g_free(cpu->halt_cond);
    g_free(cpu->thread);
}

static int64_t cpu_common_get_arch_id(CPUState *cpu)
{
    return cpu->cpu_index;
}

static void cpu_common_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);
    CPUClass *k = CPU_CLASS(klass);

    k->parse_features = cpu_common_parse_features;
    k->get_arch_id = cpu_common_get_arch_id;
    set_bit(DEVICE_CATEGORY_CPU, dc->categories);
    dc->realize = cpu_common_realizefn;
    dc->unrealize = cpu_common_unrealizefn;
    rc->phases.hold = cpu_common_reset_hold;
    cpu_class_init_props(dc);
    
    dc->user_creatable = false;
}

static const TypeInfo cpu_type_info = {
    .name = TYPE_CPU,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(CPUState),
    .instance_init = cpu_common_initfn,
    .instance_finalize = cpu_common_finalize,
    .abstract = true,
    .class_size = sizeof(CPUClass),
    .class_init = cpu_common_class_init,
};

static void cpu_register_types(void)
{
    type_register_static(&cpu_type_info);
}

type_init(cpu_register_types)
