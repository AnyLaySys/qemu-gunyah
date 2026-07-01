
#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qemu/accel.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "qapi/qapi-visit-machine.h"
#include "qapi/qapi-commands-machine.h"
#include "qemu/madvise.h"
#include "qom/object_interfaces.h"
#include "system/cpus.h"
#include "system/system.h"
#include "system/reset.h"
#include "system/runstate.h"
#include "system/confidential-guest-support.h"
#include "hw/virtio/virtio-pci.h"
#include "audio/audio.h"

MachineState *current_machine;

static char *machine_get_kernel(Object *obj, Error **errp)
{
    MachineState *ms = MACHINE(obj);

    return g_strdup(ms->kernel_filename);
}

static void machine_set_kernel(Object *obj, const char *value, Error **errp)
{
    MachineState *ms = MACHINE(obj);

    g_free(ms->kernel_filename);
    ms->kernel_filename = g_strdup(value);
}

static char *machine_get_shim(Object *obj, Error **errp)
{
    MachineState *ms = MACHINE(obj);

    return g_strdup(ms->shim_filename);
}

static void machine_set_shim(Object *obj, const char *value, Error **errp)
{
    MachineState *ms = MACHINE(obj);

    g_free(ms->shim_filename);
    ms->shim_filename = g_strdup(value);
}

static char *machine_get_initrd(Object *obj, Error **errp)
{
    MachineState *ms = MACHINE(obj);

    return g_strdup(ms->initrd_filename);
}

static void machine_set_initrd(Object *obj, const char *value, Error **errp)
{
    MachineState *ms = MACHINE(obj);

    g_free(ms->initrd_filename);
    ms->initrd_filename = g_strdup(value);
}

static char *machine_get_append(Object *obj, Error **errp)
{
    MachineState *ms = MACHINE(obj);

    return g_strdup(ms->kernel_cmdline);
}

static void machine_set_append(Object *obj, const char *value, Error **errp)
{
    MachineState *ms = MACHINE(obj);

    g_free(ms->kernel_cmdline);
    ms->kernel_cmdline = g_strdup(value);
}

static char *machine_get_dtb(Object *obj, Error **errp)
{
    MachineState *ms = MACHINE(obj);

    return g_strdup(ms->dtb);
}

static void machine_set_dtb(Object *obj, const char *value, Error **errp)
{
    MachineState *ms = MACHINE(obj);

    g_free(ms->dtb);
    ms->dtb = g_strdup(value);
}

static char *machine_get_dumpdtb(Object *obj, Error **errp)
{
    MachineState *ms = MACHINE(obj);

    return g_strdup(ms->dumpdtb);
}

static void machine_set_dumpdtb(Object *obj, const char *value, Error **errp)
{
    MachineState *ms = MACHINE(obj);

    g_free(ms->dumpdtb);
    ms->dumpdtb = g_strdup(value);
}

static void machine_get_phandle_start(Object *obj, Visitor *v,
                                      const char *name, void *opaque,
                                      Error **errp)
{
    MachineState *ms = MACHINE(obj);
    int64_t value = ms->phandle_start;

    visit_type_int(v, name, &value, errp);
}

static void machine_set_phandle_start(Object *obj, Visitor *v,
                                      const char *name, void *opaque,
                                      Error **errp)
{
    MachineState *ms = MACHINE(obj);
    int64_t value;

    if (!visit_type_int(v, name, &value, errp)) {
        return;
    }

    ms->phandle_start = value;
}

static char *machine_get_dt_compatible(Object *obj, Error **errp)
{
    MachineState *ms = MACHINE(obj);

    return g_strdup(ms->dt_compatible);
}

static void machine_set_dt_compatible(Object *obj, const char *value, Error **errp)
{
    MachineState *ms = MACHINE(obj);

    g_free(ms->dt_compatible);
    ms->dt_compatible = g_strdup(value);
}

static bool machine_get_dump_guest_core(Object *obj, Error **errp)
{
    MachineState *ms = MACHINE(obj);

    return ms->dump_guest_core;
}

static void machine_set_dump_guest_core(Object *obj, bool value, Error **errp)
{
    MachineState *ms = MACHINE(obj);

    if (!value && QEMU_MADV_DONTDUMP == QEMU_MADV_INVALID) {
        error_setg(errp, "Dumping guest memory cannot be disabled on this host");
        return;
    }
    ms->dump_guest_core = value;
}

static bool machine_get_mem_merge(Object *obj, Error **errp)
{
    MachineState *ms = MACHINE(obj);

    return ms->mem_merge;
}

static void machine_set_mem_merge(Object *obj, bool value, Error **errp)
{
    MachineState *ms = MACHINE(obj);

    if (value && QEMU_MADV_MERGEABLE == QEMU_MADV_INVALID) {
        error_setg(errp, "Memory merging is not supported on this host");
        return;
    }
    ms->mem_merge = value;
}

#ifdef CONFIG_POSIX
static bool machine_get_aux_ram_share(Object *obj, Error **errp)
{
    MachineState *ms = MACHINE(obj);

    return ms->aux_ram_share;
}

static void machine_set_aux_ram_share(Object *obj, bool value, Error **errp)
{
    MachineState *ms = MACHINE(obj);

    ms->aux_ram_share = value;
}
#endif

static bool machine_get_graphics(Object *obj, Error **errp)
{
    MachineState *ms = MACHINE(obj);

    return ms->enable_graphics;
}

static void machine_set_graphics(Object *obj, bool value, Error **errp)
{
    MachineState *ms = MACHINE(obj);

    ms->enable_graphics = value;
}

static char *machine_get_firmware(Object *obj, Error **errp)
{
    MachineState *ms = MACHINE(obj);

    return g_strdup(ms->firmware);
}

static void machine_set_firmware(Object *obj, const char *value, Error **errp)
{
    MachineState *ms = MACHINE(obj);

    g_free(ms->firmware);
    ms->firmware = g_strdup(value);
}

static void machine_set_suppress_vmdesc(Object *obj, bool value, Error **errp)
{
    MachineState *ms = MACHINE(obj);

    ms->suppress_vmdesc = value;
}

static bool machine_get_suppress_vmdesc(Object *obj, Error **errp)
{
    MachineState *ms = MACHINE(obj);

    return ms->suppress_vmdesc;
}

static void machine_check_confidential_guest_support(const Object *obj,
                                                     const char *name,
                                                     Object *new_target,
                                                     Error **errp)
{
    if (!object_dynamic_cast(new_target, TYPE_CONFIDENTIAL_GUEST_SUPPORT)) {
        error_setg(errp, "'%s' must be a confidential guest support object",
                   name);
    }
}

static void machine_get_mem(Object *obj, Visitor *v, const char *name,
                            void *opaque, Error **errp)
{
    MachineState *ms = MACHINE(obj);
    MemorySizeConfiguration mem = {
        .has_size = true,
        .size = ms->ram_size,
        .has_max_size = !!ms->ram_slots,
        .max_size = ms->maxram_size,
        .has_slots = !!ms->ram_slots,
        .slots = ms->ram_slots,
    };
    MemorySizeConfiguration *p_mem = &mem;

    visit_type_MemorySizeConfiguration(v, name, &p_mem, &error_abort);
}

static void machine_set_mem(Object *obj, Visitor *v, const char *name,
                            void *opaque, Error **errp)
{
    ERRP_GUARD();
    MachineState *ms = MACHINE(obj);
    MachineClass *mc = MACHINE_GET_CLASS(obj);
    MemorySizeConfiguration *mem;

    if (!visit_type_MemorySizeConfiguration(v, name, &mem, errp)) {
        return;
    }

    if (!mem->has_size) {
        mem->has_size = true;
        mem->size = mc->default_ram_size;
    }
    mem->size = QEMU_ALIGN_UP(mem->size, 8192);
    if (mc->fixup_ram_size) {
        mem->size = mc->fixup_ram_size(mem->size);
    }
    if ((ram_addr_t)mem->size != mem->size) {
        error_setg(errp, "ram size %llu exceeds permitted maximum %llu",
                   (unsigned long long)mem->size,
                   (unsigned long long)RAM_ADDR_MAX);
        goto out_free;
    }

    if (mem->has_max_size) {
        if ((ram_addr_t)mem->max_size != mem->max_size) {
            error_setg(errp, "ram size %llu exceeds permitted maximum %llu",
                       (unsigned long long)mem->max_size,
                       (unsigned long long)RAM_ADDR_MAX);
            goto out_free;
        }
        if (mem->max_size < mem->size) {
            error_setg(errp, "invalid value of maxmem: "
                       "maximum memory size (0x%" PRIx64 ") must be at least "
                       "the initial memory size (0x%" PRIx64 ")",
                       mem->max_size, mem->size);
            goto out_free;
        }
        if (mem->has_slots && mem->slots && mem->max_size == mem->size) {
            error_setg(errp, "invalid value of maxmem: "
                       "memory slots were specified but maximum memory size "
                       "(0x%" PRIx64 ") is equal to the initial memory size "
                       "(0x%" PRIx64 ")", mem->max_size, mem->size);
            goto out_free;
        }
        ms->maxram_size = mem->max_size;
    } else {
        if (mem->has_slots) {
            error_setg(errp, "slots specified but no max-size");
            goto out_free;
        }
        ms->maxram_size = mem->size;
    }
    ms->ram_size = mem->size;
    ms->ram_slots = mem->has_slots ? mem->slots : 0;
out_free:
    qapi_free_MemorySizeConfiguration(mem);
}

void machine_class_allow_dynamic_sysbus_dev(MachineClass *mc, const char *type)
{
    QAPI_LIST_PREPEND(mc->allowed_dynamic_sysbus_devices, g_strdup(type));
}

bool device_is_dynamic_sysbus(MachineClass *mc, DeviceState *dev)
{
    Object *obj = OBJECT(dev);

    if (!object_dynamic_cast(obj, TYPE_SYS_BUS_DEVICE)) {
        return false;
    }

    return device_type_is_dynamic_sysbus(mc, object_get_typename(obj));
}

bool device_type_is_dynamic_sysbus(MachineClass *mc, const char *type)
{
    bool allowed = false;
    strList *wl;
    ObjectClass *klass = object_class_by_name(type);

    for (wl = mc->allowed_dynamic_sysbus_devices;
         !allowed && wl;
         wl = wl->next) {
        allowed |= !!object_class_dynamic_cast(klass, wl->value);
    }

    return allowed;
}

static char *machine_get_audiodev(Object *obj, Error **errp)
{
    MachineState *ms = MACHINE(obj);

    return g_strdup(ms->audiodev);
}

static void machine_set_audiodev(Object *obj, const char *value,
                                 Error **errp)
{
    MachineState *ms = MACHINE(obj);

    if (!audio_state_by_name(value, errp)) {
        return;
    }

    g_free(ms->audiodev);
    ms->audiodev = g_strdup(value);
}

HotpluggableCPUList *machine_query_hotpluggable_cpus(MachineState *machine)
{
    int i;
    HotpluggableCPUList *head = NULL;
    MachineClass *mc = MACHINE_GET_CLASS(machine);

    mc->possible_cpu_arch_ids(machine);

    for (i = 0; i < machine->possible_cpus->len; i++) {
        CPUState *cpu;
        HotpluggableCPU *cpu_item = g_new0(typeof(*cpu_item), 1);

        cpu_item->type = g_strdup(machine->possible_cpus->cpus[i].type);
        cpu_item->vcpus_count = machine->possible_cpus->cpus[i].vcpus_count;
        cpu_item->props = g_memdup(&machine->possible_cpus->cpus[i].props,
                                   sizeof(*cpu_item->props));

        cpu = machine->possible_cpus->cpus[i].cpu;
        if (cpu) {
            cpu_item->qom_path = object_get_canonical_path(OBJECT(cpu));
        }
        QAPI_LIST_PREPEND(head, cpu_item);
    }
    return head;
}

static void machine_get_smp(Object *obj, Visitor *v, const char *name,
                            void *opaque, Error **errp)
{
    MachineState *ms = MACHINE(obj);
    SMPConfiguration *config = &(SMPConfiguration){
        .has_cpus = true, .cpus = ms->smp.cpus,
        .has_drawers = true, .drawers = ms->smp.drawers,
        .has_books = true, .books = ms->smp.books,
        .has_sockets = true, .sockets = ms->smp.sockets,
        .has_dies = true, .dies = ms->smp.dies,
        .has_clusters = true, .clusters = ms->smp.clusters,
        .has_modules = true, .modules = ms->smp.modules,
        .has_cores = true, .cores = ms->smp.cores,
        .has_threads = true, .threads = ms->smp.threads,
        .has_maxcpus = true, .maxcpus = ms->smp.max_cpus,
    };

    if (!visit_type_SMPConfiguration(v, name, &config, &error_abort)) {
        return;
    }
}

static void machine_set_smp(Object *obj, Visitor *v, const char *name,
                            void *opaque, Error **errp)
{
    MachineState *ms = MACHINE(obj);
    g_autoptr(SMPConfiguration) config = NULL;

    if (!visit_type_SMPConfiguration(v, name, &config, errp)) {
        return;
    }

    machine_parse_smp_config(ms, config, errp);
}

static void machine_get_smp_cache(Object *obj, Visitor *v, const char *name,
                                  void *opaque, Error **errp)
{
    MachineState *ms = MACHINE(obj);
    SmpCache *cache = &ms->smp_cache;
    SmpCachePropertiesList *head = NULL;
    SmpCachePropertiesList **tail = &head;

    for (int i = 0; i < CACHE_LEVEL_AND_TYPE__MAX; i++) {
        SmpCacheProperties *node = g_new(SmpCacheProperties, 1);

        node->cache = cache->props[i].cache;
        node->topology = cache->props[i].topology;
        QAPI_LIST_APPEND(tail, node);
    }

    visit_type_SmpCachePropertiesList(v, name, &head, errp);
    qapi_free_SmpCachePropertiesList(head);
}

static void machine_set_smp_cache(Object *obj, Visitor *v, const char *name,
                                  void *opaque, Error **errp)
{
    MachineState *ms = MACHINE(obj);
    SmpCachePropertiesList *caches;

    if (!visit_type_SmpCachePropertiesList(v, name, &caches, errp)) {
        return;
    }

    machine_parse_smp_cache(ms, caches, errp);
    qapi_free_SmpCachePropertiesList(caches);
}

static void machine_get_boot(Object *obj, Visitor *v, const char *name,
                            void *opaque, Error **errp)
{
    MachineState *ms = MACHINE(obj);
    BootConfiguration *config = &ms->boot_config;
    visit_type_BootConfiguration(v, name, &config, &error_abort);
}

static void machine_free_boot_config(MachineState *ms)
{
    g_free(ms->boot_config.order);
    g_free(ms->boot_config.once);
    g_free(ms->boot_config.splash);
}

static void machine_copy_boot_config(MachineState *ms, BootConfiguration *config)
{
    MachineClass *machine_class = MACHINE_GET_CLASS(ms);

    machine_free_boot_config(ms);
    ms->boot_config = *config;
    if (!config->order) {
        ms->boot_config.order = g_strdup(machine_class->default_boot_order);
    }
}

static void machine_set_boot(Object *obj, Visitor *v, const char *name,
                            void *opaque, Error **errp)
{
    ERRP_GUARD();
    MachineState *ms = MACHINE(obj);
    BootConfiguration *config = NULL;

    if (!visit_type_BootConfiguration(v, name, &config, errp)) {
        return;
    }
    if (config->order) {
        validate_bootdevices(config->order, errp);
        if (*errp) {
            goto out_free;
        }
    }
    if (config->once) {
        validate_bootdevices(config->once, errp);
        if (*errp) {
            goto out_free;
        }
    }

    machine_copy_boot_config(ms, config);
    free(config);
    return;

out_free:
    qapi_free_BootConfiguration(config);
}

void machine_add_audiodev_property(MachineClass *mc)
{
    ObjectClass *oc = OBJECT_CLASS(mc);

    object_class_property_add_str(oc, "audiodev",
                                  machine_get_audiodev,
                                  machine_set_audiodev);
    object_class_property_set_description(oc, "audiodev",
                                          "Audiodev to use for default machine devices");
}

static bool create_default_memdev(MachineState *ms, Error **errp)
{
    Object *obj;
    MachineClass *mc = MACHINE_GET_CLASS(ms);
    bool r = false;

    obj = object_new(TYPE_MEMORY_BACKEND_RAM);
    if (!object_property_set_int(obj, "size", ms->ram_size, errp)) {
        goto out;
    }
    object_property_add_child(object_get_objects_root(), mc->default_ram_id,
                              obj);
    if (!object_property_set_bool(obj, "x-use-canonical-path-for-ramblock-id",
                             false, errp)) {
        goto out;
    }
    if (!user_creatable_complete(USER_CREATABLE(obj), errp)) {
        goto out;
    }
    r = object_property_set_link(OBJECT(ms), "memory-backend", obj, errp);

out:
    object_unref(obj);
    return r;
}

static void machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->default_ram_size = 128 * MiB;
    mc->rom_file_has_mr = true;
    mc->smbios_memory_device_size = 2047 * TiB;


    mc->create_default_memdev = create_default_memdev;

    object_class_property_add_str(oc, "kernel",
        machine_get_kernel, machine_set_kernel);
    object_class_property_set_description(oc, "kernel",
        "Linux kernel image file");

    object_class_property_add_str(oc, "shim",
        machine_get_shim, machine_set_shim);
    object_class_property_set_description(oc, "shim",
        "shim.efi file");

    object_class_property_add_str(oc, "initrd",
        machine_get_initrd, machine_set_initrd);
    object_class_property_set_description(oc, "initrd",
        "Linux initial ramdisk file");

    object_class_property_add_str(oc, "append",
        machine_get_append, machine_set_append);
    object_class_property_set_description(oc, "append",
        "Linux kernel command line");

    object_class_property_add_str(oc, "dtb",
        machine_get_dtb, machine_set_dtb);
    object_class_property_set_description(oc, "dtb",
        "Linux kernel device tree file");

    object_class_property_add_str(oc, "dumpdtb",
        machine_get_dumpdtb, machine_set_dumpdtb);
    object_class_property_set_description(oc, "dumpdtb",
        "Dump current dtb to a file and quit");

    object_class_property_add(oc, "boot", "BootConfiguration",
        machine_get_boot, machine_set_boot,
        NULL, NULL);
    object_class_property_set_description(oc, "boot",
        "Boot configuration");

    object_class_property_add(oc, "smp", "SMPConfiguration",
        machine_get_smp, machine_set_smp,
        NULL, NULL);
    object_class_property_set_description(oc, "smp",
        "CPU topology");

    object_class_property_add(oc, "smp-cache", "SmpCachePropertiesWrapper",
        machine_get_smp_cache, machine_set_smp_cache, NULL, NULL);
    object_class_property_set_description(oc, "smp-cache",
        "Cache properties list for SMP machine");

    object_class_property_add(oc, "phandle-start", "int",
        machine_get_phandle_start, machine_set_phandle_start,
        NULL, NULL);
    object_class_property_set_description(oc, "phandle-start",
        "The first phandle ID we may generate dynamically");

    object_class_property_add_str(oc, "dt-compatible",
        machine_get_dt_compatible, machine_set_dt_compatible);
    object_class_property_set_description(oc, "dt-compatible",
        "Overrides the \"compatible\" property of the dt root node");

    object_class_property_add_bool(oc, "dump-guest-core",
        machine_get_dump_guest_core, machine_set_dump_guest_core);
    object_class_property_set_description(oc, "dump-guest-core",
        "Include guest memory in a core dump");

    object_class_property_add_bool(oc, "mem-merge",
        machine_get_mem_merge, machine_set_mem_merge);
    object_class_property_set_description(oc, "mem-merge",
        "Enable/disable memory merge support");

#ifdef CONFIG_POSIX
    object_class_property_add_bool(oc, "aux-ram-share",
                                   machine_get_aux_ram_share,
                                   machine_set_aux_ram_share);
#endif

    object_class_property_add_bool(oc, "graphics",
        machine_get_graphics, machine_set_graphics);
    object_class_property_set_description(oc, "graphics",
        "Set on/off to enable/disable graphics emulation");

    object_class_property_add_str(oc, "firmware",
        machine_get_firmware, machine_set_firmware);
    object_class_property_set_description(oc, "firmware",
        "Firmware image");

    object_class_property_add_bool(oc, "suppress-vmdesc",
        machine_get_suppress_vmdesc, machine_set_suppress_vmdesc);
    object_class_property_set_description(oc, "suppress-vmdesc",
        "Set on to disable state descriptions");

    object_class_property_add_link(oc, "confidential-guest-support",
                                   TYPE_CONFIDENTIAL_GUEST_SUPPORT,
                                   offsetof(MachineState, cgs),
                                   machine_check_confidential_guest_support,
                                   OBJ_PROP_LINK_STRONG);
    object_class_property_set_description(oc, "confidential-guest-support",
                                          "Set confidential guest scheme to support");

    object_class_property_add_link(oc, "memory-backend", TYPE_MEMORY_BACKEND,
                                   offsetof(MachineState, memdev), object_property_allow_set_link,
                                   OBJ_PROP_LINK_STRONG);
    object_class_property_set_description(oc, "memory-backend",
                                          "Set RAM backend"
                                          "Valid value is ID of hostmem based backend");

    object_class_property_add(oc, "memory", "MemorySizeConfiguration",
        machine_get_mem, machine_set_mem,
        NULL, NULL);
    object_class_property_set_description(oc, "memory",
        "Memory size configuration");
}

static void machine_class_base_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    mc->max_cpus = mc->max_cpus ?: 1;
    mc->min_cpus = mc->min_cpus ?: 1;
    mc->default_cpus = mc->default_cpus ?: 1;

    if (!object_class_is_abstract(oc)) {
        const char *cname = object_class_get_name(oc);
        assert(g_str_has_suffix(cname, TYPE_MACHINE_SUFFIX));
        mc->name = g_strndup(cname,
                            strlen(cname) - strlen(TYPE_MACHINE_SUFFIX));
        mc->compat_props = g_ptr_array_new();
    }
}

static void machine_initfn(Object *obj)
{
    MachineState *ms = MACHINE(obj);
    MachineClass *mc = MACHINE_GET_CLASS(obj);

    ms->dump_guest_core = true;
    ms->mem_merge = (QEMU_MADV_MERGEABLE != QEMU_MADV_INVALID);
    ms->enable_graphics = true;
    ms->kernel_cmdline = g_strdup("");
    ms->ram_size = mc->default_ram_size;
    ms->maxram_size = mc->default_ram_size;

    ms->smp.cpus = mc->default_cpus;
    ms->smp.max_cpus = mc->default_cpus;
    ms->smp.drawers = 1;
    ms->smp.books = 1;
    ms->smp.sockets = 1;
    ms->smp.dies = 1;
    ms->smp.clusters = 1;
    ms->smp.modules = 1;
    ms->smp.cores = 1;
    ms->smp.threads = 1;

    for (int i = 0; i < CACHE_LEVEL_AND_TYPE__MAX; i++) {
        ms->smp_cache.props[i].cache = (CacheLevelAndType)i;
        ms->smp_cache.props[i].topology = CPU_TOPOLOGY_LEVEL_DEFAULT;
    }

    machine_copy_boot_config(ms, &(BootConfiguration){ 0 });
}

static void machine_finalize(Object *obj)
{
    MachineState *ms = MACHINE(obj);

    machine_free_boot_config(ms);
    g_free(ms->kernel_filename);
    g_free(ms->initrd_filename);
    g_free(ms->kernel_cmdline);
    g_free(ms->dtb);
    g_free(ms->dumpdtb);
    g_free(ms->dt_compatible);
    g_free(ms->firmware);
    g_free(ms->device_memory);
    g_free(ms->audiodev);
}

int machine_phandle_start(MachineState *machine)
{
    return machine->phandle_start;
}

bool machine_dump_guest_core(MachineState *machine)
{
    return machine->dump_guest_core;
}

bool machine_mem_merge(MachineState *machine)
{
    return machine->mem_merge;
}

bool machine_require_guest_memfd(MachineState *machine)
{
    return machine->cgs && machine->cgs->require_guest_memfd;
}

MemoryRegion *machine_consume_memdev(MachineState *machine,
                                     HostMemoryBackend *backend)
{
    MemoryRegion *ret = host_memory_backend_get_memory(backend);

    if (host_memory_backend_is_mapped(backend)) {
        error_report("memory backend %s can't be used multiple times.",
                     object_get_canonical_path_component(OBJECT(backend)));
        exit(EXIT_FAILURE);
    }
    host_memory_backend_set_mapped(backend, true);
    vmstate_register_ram_global(ret);
    return ret;
}

const char *machine_class_default_cpu_type(MachineClass *mc)
{
    if (mc->valid_cpu_types && !mc->valid_cpu_types[1]) {
        return mc->valid_cpu_types[0];
    }
    return mc->default_cpu_type;
}

static bool is_cpu_type_supported(const MachineState *machine, Error **errp)
{
    MachineClass *mc = MACHINE_GET_CLASS(machine);
    ObjectClass *oc = object_class_by_name(machine->cpu_type);
    CPUClass *cc;
    int i;

    if (mc->valid_cpu_types) {
        assert(mc->valid_cpu_types[0] != NULL);
        for (i = 0; mc->valid_cpu_types[i]; i++) {
            if (object_class_dynamic_cast(oc, mc->valid_cpu_types[i])) {
                break;
            }
        }

        if (!mc->valid_cpu_types[i]) {
            g_autofree char *requested = cpu_model_from_type(machine->cpu_type);
            error_setg(errp, "Invalid CPU model: %s", requested);
            if (!mc->valid_cpu_types[1]) {
                g_autofree char *model = cpu_model_from_type(
                                                 mc->valid_cpu_types[0]);
                error_append_hint(errp, "The only valid type is: %s\n", model);
            } else {
                error_append_hint(errp, "The valid models are: ");
                for (i = 0; mc->valid_cpu_types[i]; i++) {
                    g_autofree char *model = cpu_model_from_type(
                                                 mc->valid_cpu_types[i]);
                    error_append_hint(errp, "%s%s",
                                      model,
                                      mc->valid_cpu_types[i + 1] ? ", " : "");
                }
                error_append_hint(errp, "\n");
            }

            return false;
        }
    }

    cc = CPU_CLASS(oc);
    assert(cc != NULL);
    if (cc->deprecation_note) {
        warn_report("CPU model %s is deprecated -- %s",
                    machine->cpu_type, cc->deprecation_note);
    }

    return true;
}

void machine_run_board_init(MachineState *machine, Error **errp)
{
    ERRP_GUARD();
    MachineClass *machine_class = MACHINE_GET_CLASS(machine);

    if (machine->ram_size > (2047 << 20) && HOST_LONG_BITS == 32) {
        error_setg(errp, "at most 2047 MB RAM can be simulated");
        return;
    }

    if (machine->memdev) {
        ram_addr_t backend_size = object_property_get_uint(OBJECT(machine->memdev),
                                                           "size",  &error_abort);
        if (backend_size != machine->ram_size) {
            error_setg(errp, "Machine memory size does not match the size of the memory backend");
            return;
        }
    } else if (machine_class->default_ram_id && machine->ram_size) {
        if (object_property_find(object_get_objects_root(),
                                 machine_class->default_ram_id)) {
            error_setg(errp, "object's id '%s' is reserved for the default"
                " RAM backend, it can't be used for any other purposes",
                machine_class->default_ram_id);
            error_append_hint(errp,
                "Change the object's 'id' to something else or disable"
                " automatic creation of the default RAM backend by setting"
                " 'memory-backend=%s' with '-machine'.\n",
                machine_class->default_ram_id);
            return;
        }

        if (!machine_class->create_default_memdev(current_machine, errp)) {
            return;
        }
    }

    if (!machine->ram && machine->memdev) {
        machine->ram = machine_consume_memdev(machine, machine->memdev);
    }

    if (machine->cpu_type && !is_cpu_type_supported(machine, errp)) {
        return;
    }

    if (machine->cgs) {
        machine_set_mem_merge(OBJECT(machine), false, &error_abort);

        object_register_sugar_prop(TYPE_VIRTIO_PCI, "disable-legacy",
                                   "on", true);
        object_register_sugar_prop(TYPE_VIRTIO_DEVICE, "iommu_platform",
                                   "on", false);
    }

    accel_init_interfaces(ACCEL_GET_CLASS(machine->accelerator));
    machine_class->init(machine);
    phase_advance(PHASE_MACHINE_INITIALIZED);
}

static NotifierList machine_init_done_notifiers =
    NOTIFIER_LIST_INITIALIZER(machine_init_done_notifiers);

void qemu_add_machine_init_done_notifier(Notifier *notify)
{
    notifier_list_add(&machine_init_done_notifiers, notify);
    if (phase_check(PHASE_MACHINE_READY)) {
        notify->notify(notify, NULL);
    }
}

void qemu_remove_machine_init_done_notifier(Notifier *notify)
{
    notifier_remove(notify);
}

static void handle_machine_dumpdtb(MachineState *ms)
{
    if (!ms->dumpdtb) {
        return;
    }
#ifdef CONFIG_FDT
    qmp_dumpdtb(ms->dumpdtb, &error_fatal);
    exit(0);
#else
    error_report("This machine doesn't have an FDT");
    error_printf("(this machine type definitely doesn't use FDT, and "
                 "this QEMU doesn't have FDT support compiled in)\n");
    exit(1);
#endif
}

void qdev_machine_creation_done(void)
{
    cpu_synchronize_all_post_init();

    if (current_machine->boot_config.once) {
        qemu_boot_set(current_machine->boot_config.once, &error_fatal);
        qemu_register_reset(restore_boot_order, g_strdup(current_machine->boot_config.order));
    }

    phase_advance(PHASE_MACHINE_READY);
    qdev_assert_realized_properly();

    qemu_register_resettable(OBJECT(sysbus_get_default()));

    notifier_list_notify(&machine_init_done_notifiers, NULL);

    handle_machine_dumpdtb(current_machine);

    if (rom_check_and_register_reset() != 0) {
        exit(1);
    }

    qemu_system_reset(SHUTDOWN_CAUSE_NONE);
}

static const TypeInfo machine_info = {
    .name = TYPE_MACHINE,
    .parent = TYPE_OBJECT,
    .abstract = true,
    .class_size = sizeof(MachineClass),
    .class_init    = machine_class_init,
    .class_base_init = machine_class_base_init,
    .instance_size = sizeof(MachineState),
    .instance_init = machine_initfn,
    .instance_finalize = machine_finalize,
};

static void machine_register_types(void)
{
    type_register_static(&machine_info);
}

type_init(machine_register_types)
