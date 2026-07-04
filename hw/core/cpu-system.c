
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "exec/address-spaces.h"
#include "exec/cputlb.h"
#include "exec/memory.h"
#include "exec/tb-flush.h"
#include "exec/tswap.h"
#include "hw/qdev-core.h"
#include "hw/qdev-properties.h"
#include "hw/core/sysemu-cpu-ops.h"
#include "migration/vmstate.h"
#include "system/tcg.h"

bool cpu_has_work(CPUState *cpu)
{
    return cpu->cc->sysemu_ops->has_work(cpu);
}

bool cpu_paging_enabled(const CPUState *cpu)
{
    if (cpu->cc->sysemu_ops->get_paging_enabled) {
        return cpu->cc->sysemu_ops->get_paging_enabled(cpu);
    }

    return false;
}

hwaddr cpu_get_phys_page_attrs_debug(CPUState *cpu, vaddr addr,
                                     MemTxAttrs *attrs)
{
    hwaddr paddr;

    if (cpu->cc->sysemu_ops->get_phys_page_attrs_debug) {
        paddr = cpu->cc->sysemu_ops->get_phys_page_attrs_debug(cpu, addr,
                                                               attrs);
    } else {
        *attrs = MEMTXATTRS_UNSPECIFIED;
        paddr = cpu->cc->sysemu_ops->get_phys_page_debug(cpu, addr);
    }
    attrs->debug = 1;
    return paddr;
}

hwaddr cpu_get_phys_page_debug(CPUState *cpu, vaddr addr)
{
    MemTxAttrs attrs = {};

    return cpu_get_phys_page_attrs_debug(cpu, addr, &attrs);
}

int cpu_asidx_from_attrs(CPUState *cpu, MemTxAttrs attrs)
{
    int ret = 0;

    if (cpu->cc->sysemu_ops->asidx_from_attrs) {
        ret = cpu->cc->sysemu_ops->asidx_from_attrs(cpu, attrs);
        assert(ret < cpu->num_ases && ret >= 0);
    }
    return ret;
}

bool cpu_virtio_is_big_endian(CPUState *cpu)
{
    if (cpu->cc->sysemu_ops->virtio_is_big_endian) {
        return cpu->cc->sysemu_ops->virtio_is_big_endian(cpu);
    }
    return target_words_bigendian();
}

GuestPanicInformation *cpu_get_crash_info(CPUState *cpu)
{
    GuestPanicInformation *res = NULL;

    if (cpu->cc->sysemu_ops->get_crash_info) {
        res = cpu->cc->sysemu_ops->get_crash_info(cpu);
    }
    return res;
}

static const Property cpu_system_props[] = {
    DEFINE_PROP_LINK("memory", CPUState, memory, TYPE_MEMORY_REGION,
                     MemoryRegion *),
};

static bool cpu_get_start_powered_off(Object *obj, Error **errp)
{
    CPUState *cpu = CPU(obj);
    return cpu->start_powered_off;
}

static void cpu_set_start_powered_off(Object *obj, bool value, Error **errp)
{
    CPUState *cpu = CPU(obj);
    cpu->start_powered_off = value;
}

void cpu_class_init_props(DeviceClass *dc)
{
    ObjectClass *oc = OBJECT_CLASS(dc);

    object_class_property_add_bool(oc, "start-powered-off",
                                   cpu_get_start_powered_off,
                                   cpu_set_start_powered_off);

    device_class_set_props(dc, cpu_system_props);
}

void cpu_exec_class_post_init(CPUClass *cc)
{
    g_assert(cc->sysemu_ops->has_work);
}

void cpu_exec_initfn(CPUState *cpu)
{
    cpu->memory = get_system_memory();
    object_ref(OBJECT(cpu->memory));
}

static int cpu_common_post_load(void *opaque, int version_id)
{
    if (tcg_enabled()) {
        CPUState *cpu = opaque;

        cpu->interrupt_request &= ~0x01;

        tlb_flush(cpu);

        tb_flush(cpu);
    }

    return 0;
}

static int cpu_common_pre_load(void *opaque)
{
    CPUState *cpu = opaque;

    cpu->exception_index = -1;

    return 0;
}

static bool cpu_common_exception_index_needed(void *opaque)
{
    CPUState *cpu = opaque;

    return tcg_enabled() && cpu->exception_index != -1;
}

static const VMStateDescription vmstate_cpu_common_exception_index = {
    .name = "cpu_common/exception_index",
    .version_id = 1,
    .minimum_version_id = 1,
    .needed = cpu_common_exception_index_needed,
    .fields = (const VMStateField[]) {
        VMSTATE_INT32(exception_index, CPUState),
        VMSTATE_END_OF_LIST()
    }
};

static bool cpu_common_crash_occurred_needed(void *opaque)
{
    CPUState *cpu = opaque;

    return cpu->crash_occurred;
}

static const VMStateDescription vmstate_cpu_common_crash_occurred = {
    .name = "cpu_common/crash_occurred",
    .version_id = 1,
    .minimum_version_id = 1,
    .needed = cpu_common_crash_occurred_needed,
    .fields = (const VMStateField[]) {
        VMSTATE_BOOL(crash_occurred, CPUState),
        VMSTATE_END_OF_LIST()
    }
};

const VMStateDescription vmstate_cpu_common = {
    .name = "cpu_common",
    .version_id = 1,
    .minimum_version_id = 1,
    .pre_load = cpu_common_pre_load,
    .post_load = cpu_common_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(halted, CPUState),
        VMSTATE_UINT32(interrupt_request, CPUState),
        VMSTATE_END_OF_LIST()
    },
    .subsections = (const VMStateDescription * const []) {
        &vmstate_cpu_common_exception_index,
        &vmstate_cpu_common_crash_occurred,
        NULL
    }
};

void cpu_vmstate_register(CPUState *cpu)
{
    if (qdev_get_vmsd(DEVICE(cpu)) == NULL) {
        vmstate_register(NULL, cpu->cpu_index, &vmstate_cpu_common, cpu);
    }
    if (cpu->cc->sysemu_ops->legacy_vmsd != NULL) {
        vmstate_register(NULL, cpu->cpu_index,
                         cpu->cc->sysemu_ops->legacy_vmsd, cpu);
    }
}

void cpu_vmstate_unregister(CPUState *cpu)
{
    if (cpu->cc->sysemu_ops->legacy_vmsd != NULL) {
        vmstate_unregister(NULL, cpu->cc->sysemu_ops->legacy_vmsd, cpu);
    }
    if (qdev_get_vmsd(DEVICE(cpu)) == NULL) {
        vmstate_unregister(NULL, &vmstate_cpu_common, cpu);
    }
}
