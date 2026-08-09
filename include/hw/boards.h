
#ifndef HW_BOARDS_H
#define HW_BOARDS_H

#include "exec/memory.h"
#include "system/hostmem.h"
#include "system/blockdev.h"
#include "qapi/qapi-types-machine.h"
#include "qemu/module.h"
#include "qom/object.h"
#include "hw/core/cpu.h"
#include "hw/resettable.h"

#define TYPE_MACHINE_SUFFIX "-machine"

#define MACHINE_TYPE_NAME(machinename) (machinename TYPE_MACHINE_SUFFIX)

#define TYPE_MACHINE "machine"
#undef MACHINE  /* BSD defines it and QEMU does not use it */
OBJECT_DECLARE_TYPE(MachineState, MachineClass, MACHINE)

extern MachineState *current_machine;

const char *machine_class_default_cpu_type(MachineClass *mc);

void machine_add_audiodev_property(MachineClass *mc);
void machine_run_board_init(MachineState *machine, Error **errp);
bool machine_usb(MachineState *machine);
int machine_phandle_start(MachineState *machine);
bool machine_dump_guest_core(MachineState *machine);
bool machine_mem_merge(MachineState *machine);
bool machine_require_guest_memfd(MachineState *machine);
HotpluggableCPUList *machine_query_hotpluggable_cpus(MachineState *machine);
void machine_set_cpu_numa_node(MachineState *machine,
                               const CpuInstanceProperties *props,
                               Error **errp);
void machine_parse_smp_config(MachineState *ms,
                              const SMPConfiguration *config, Error **errp);
bool machine_parse_smp_cache(MachineState *ms,
                             const SmpCachePropertiesList *caches,
                             Error **errp);
unsigned int machine_topo_get_cores_per_socket(const MachineState *ms);
unsigned int machine_topo_get_threads_per_socket(const MachineState *ms);
CpuTopologyLevel machine_get_cache_topo_level(const MachineState *ms,
                                              CacheLevelAndType cache);
void machine_set_cache_topo_level(MachineState *ms, CacheLevelAndType cache,
                                  CpuTopologyLevel level);
bool machine_check_smp_cache(const MachineState *ms, Error **errp);
void machine_memory_devices_init(MachineState *ms, hwaddr base, uint64_t size);

void machine_class_allow_dynamic_sysbus_dev(MachineClass *mc, const char *type);

bool device_type_is_dynamic_sysbus(MachineClass *mc, const char *type);

bool device_is_dynamic_sysbus(MachineClass *mc, DeviceState *dev);

MemoryRegion *machine_consume_memdev(MachineState *machine,
                                     HostMemoryBackend *backend);

typedef struct CPUArchId {
    uint64_t arch_id;
    int64_t vcpus_count;
    CpuInstanceProperties props;
    CPUState *cpu;
    const char *type;
} CPUArchId;

typedef struct {
    int len;
    CPUArchId cpus[];
} CPUArchIdList;

typedef struct {
    bool prefer_sockets;
    bool dies_supported;
    bool clusters_supported;
    bool has_clusters;
    bool books_supported;
    bool drawers_supported;
    bool modules_supported;
    bool cache_supported[CACHE_LEVEL_AND_TYPE__MAX];
    bool has_caches;
} SMPCompatProps;

struct MachineClass {
    ObjectClass parent_class;

    const char *family; /* NULL iff @name identifies a standalone machtype */
    char *name;
    const char *alias;
    const char *desc;
    const char *deprecation_reason;

    void (*init)(MachineState *state);
    void (*reset)(MachineState *state, ResetType type);
    void (*wakeup)(MachineState *state);
    int (*kvm_type)(MachineState *machine, const char *arg);
    int (*hvf_get_physical_address_range)(MachineState *machine);

    BlockInterfaceType block_default_type;
    int units_per_default_bus;
    int max_cpus;
    int min_cpus;
    int default_cpus;
    unsigned int no_serial:1,
        no_parallel:1,
        no_cdrom:1,
        pci_allow_0_address:1,
        legacy_fw_cfg_order:1;
    bool is_default;
    const char *default_machine_opts;
    const char *default_boot_order;
    const char *default_nic;
    GPtrArray *compat_props;
    const char *hw_version;
    ram_addr_t default_ram_size;
    const char *default_cpu_type;
    bool default_kernel_irqchip_split;
    bool option_rom_has_mr;
    bool rom_file_has_mr;
    int minimum_page_bits;
    bool has_hotpluggable_cpus;
    bool ignore_memory_transaction_failures;
    int numa_mem_align_shift;
    const char * const *valid_cpu_types;
    strList *allowed_dynamic_sysbus_devices;
    bool auto_enable_numa_with_memhp;
    bool auto_enable_numa_with_memdev;
    bool ignore_boot_device_suffixes;
    bool smbus_no_migration_support;
    bool nvdimm_supported;
    bool numa_mem_supported;
    bool auto_enable_numa;
    bool cpu_cluster_has_numa_boundary;
    SMPCompatProps smp_props;
    const char *default_ram_id;

    HotplugHandler *(*get_hotplug_handler)(MachineState *machine,
                                           DeviceState *dev);
    bool (*hotplug_allowed)(MachineState *state, DeviceState *dev,
                            Error **errp);
    CpuInstanceProperties (*cpu_index_to_instance_props)(MachineState *machine,
                                                         unsigned cpu_index);
    const CPUArchIdList *(*possible_cpu_arch_ids)(MachineState *machine);
    int64_t (*get_default_cpu_node_id)(const MachineState *ms, int idx);
    ram_addr_t (*fixup_ram_size)(ram_addr_t size);
    bool (*create_default_memdev)(MachineState *ms, Error **errp);
};

typedef struct DeviceMemoryState {
    hwaddr base;
    MemoryRegion mr;
    AddressSpace as;
    MemoryListener listener;
    uint64_t dimm_size;
    uint64_t used_region_size;
    unsigned int required_memslots;
    unsigned int used_memslots;
    unsigned int memslot_auto_decision_active;
} DeviceMemoryState;

typedef struct CpuTopology {
    unsigned int cpus;
    unsigned int drawers;
    unsigned int books;
    unsigned int sockets;
    unsigned int dies;
    unsigned int clusters;
    unsigned int modules;
    unsigned int cores;
    unsigned int threads;
    unsigned int max_cpus;
} CpuTopology;

typedef struct SmpCache {
    SmpCacheProperties props[CACHE_LEVEL_AND_TYPE__MAX];
} SmpCache;

struct MachineState {
    Object parent_obj;


    void *fdt;
    char *dtb;
    char *dumpdtb;
    int phandle_start;
    char *dt_compatible;
    bool dump_guest_core;
    bool mem_merge;
    bool usb;
    bool usb_disabled;
    char *firmware;
    bool iommu;
    bool suppress_vmdesc;
    bool enable_graphics;
    ConfidentialGuestSupport *cgs;
    HostMemoryBackend *memdev;
    bool aux_ram_share;
    MemoryRegion *ram;
    DeviceMemoryState *device_memory;

    char *audiodev;

    ram_addr_t ram_size;
    ram_addr_t maxram_size;
    uint64_t   ram_slots;
    BootConfiguration boot_config;
    char *kernel_filename;
    char *kernel_cmdline;
    char *initrd_filename;
    const char *cpu_type;
    AccelState *accelerator;
    CPUArchIdList *possible_cpus;
    CpuTopology smp;
    SmpCache smp_cache;
    struct NVDIMMState *nvdimms_state;
    struct NumaState *numa_state;
};


#define _MACHINE_VER_PICK(x1, x2, x3, x4, x5, x6, ...) x6

#define _MACHINE_VER_STR2(major, minor) \
    #major "." #minor

#define _MACHINE_VER_STR3(major, minor, micro) \
    #major "." #minor "." #micro

#define _MACHINE_VER_STR4(major, minor, _unused_, tag) \
    #major "." #minor "-" #tag

#define _MACHINE_VER_STR5(major, minor, micro, _unused_, tag) \
    #major "." #minor "." #micro "-" #tag

#define MACHINE_VER_STR(...) \
    _MACHINE_VER_PICK(__VA_ARGS__, \
                      _MACHINE_VER_STR5, \
                      _MACHINE_VER_STR4, \
                      _MACHINE_VER_STR3, \
                      _MACHINE_VER_STR2) (__VA_ARGS__)


#define _MACHINE_VER_TYPE_NAME2(prefix, major, minor)   \
    prefix "-" #major "." #minor TYPE_MACHINE_SUFFIX

#define _MACHINE_VER_TYPE_NAME3(prefix, major, minor, micro) \
    prefix "-" #major "." #minor "." #micro TYPE_MACHINE_SUFFIX

#define _MACHINE_VER_TYPE_NAME4(prefix, major, minor, _unused_, tag) \
    prefix "-" #major "." #minor "-" #tag TYPE_MACHINE_SUFFIX

#define _MACHINE_VER_TYPE_NAME5(prefix, major, minor, micro, _unused_, tag) \
    prefix "-" #major "." #minor "." #micro "-" #tag TYPE_MACHINE_SUFFIX

#define MACHINE_VER_TYPE_NAME(prefix, ...) \
    _MACHINE_VER_PICK(__VA_ARGS__, \
                      _MACHINE_VER_TYPE_NAME5, \
                      _MACHINE_VER_TYPE_NAME4, \
                      _MACHINE_VER_TYPE_NAME3, \
                      _MACHINE_VER_TYPE_NAME2) (prefix, __VA_ARGS__)

#define _MACHINE_VER_SYM2(sym, prefix, major, minor) \
    prefix ## _machine_ ## major ## _ ## minor ## _ ## sym

#define _MACHINE_VER_SYM3(sym, prefix, major, minor, micro) \
    prefix ## _machine_ ## major ## _ ## minor ## _ ## micro ## _ ## sym

#define _MACHINE_VER_SYM4(sym, prefix, major, minor, _unused_, tag) \
    prefix ## _machine_ ## major ## _ ## minor ## _ ## tag ## _ ## sym

#define _MACHINE_VER_SYM5(sym, prefix, major, minor, micro, _unused_, tag) \
    prefix ## _machine_ ## major ## _ ## minor ## _ ## micro ## _ ## tag ## _ ## sym

#define MACHINE_VER_SYM(sym, prefix, ...) \
    _MACHINE_VER_PICK(__VA_ARGS__, \
                      _MACHINE_VER_SYM5, \
                      _MACHINE_VER_SYM4, \
                      _MACHINE_VER_SYM3, \
                      _MACHINE_VER_SYM2) (sym, prefix, __VA_ARGS__)


#define MACHINE_VER_DELETION_MAJOR 6
#define MACHINE_VER_DEPRECATION_MAJOR 3

#define MACHINE_VER_DEPRECATION_MSG \
    "machines more than " stringify(MACHINE_VER_DEPRECATION_MAJOR) \
    " years old are subject to deletion after " \
    stringify(MACHINE_VER_DELETION_MAJOR) " years"

#define _MACHINE_VER_IS_EXPIRED_IMPL(cutoff, major, minor) \
    (((QEMU_VERSION_MAJOR - major) > cutoff) || \
     (((QEMU_VERSION_MAJOR - major) == cutoff) && \
      (QEMU_VERSION_MINOR - minor) >= 0))

#define _MACHINE_VER_IS_EXPIRED2(cutoff, major, minor) \
    _MACHINE_VER_IS_EXPIRED_IMPL(cutoff, major, minor)
#define _MACHINE_VER_IS_EXPIRED3(cutoff, major, minor, micro) \
    _MACHINE_VER_IS_EXPIRED_IMPL(cutoff, major, minor)
#define _MACHINE_VER_IS_EXPIRED4(cutoff, major, minor, _unused, tag) \
    _MACHINE_VER_IS_EXPIRED_IMPL(cutoff, major, minor)
#define _MACHINE_VER_IS_EXPIRED5(cutoff, major, minor, micro, _unused, tag)   \
    _MACHINE_VER_IS_EXPIRED_IMPL(cutoff, major, minor)

#define _MACHINE_IS_EXPIRED(cutoff, ...) \
    _MACHINE_VER_PICK(__VA_ARGS__, \
                      _MACHINE_VER_IS_EXPIRED5, \
                      _MACHINE_VER_IS_EXPIRED4, \
                      _MACHINE_VER_IS_EXPIRED3, \
                      _MACHINE_VER_IS_EXPIRED2) (cutoff, __VA_ARGS__)

#define MACHINE_VER_IS_DEPRECATED(...) \
    _MACHINE_IS_EXPIRED(MACHINE_VER_DEPRECATION_MAJOR, __VA_ARGS__)

#define MACHINE_VER_SHOULD_DELETE(...) \
    _MACHINE_IS_EXPIRED(MACHINE_VER_DELETION_MAJOR, __VA_ARGS__)

#define MACHINE_VER_DEPRECATION(...) \
    do { \
        if (MACHINE_VER_IS_DEPRECATED(__VA_ARGS__)) { \
            mc->deprecation_reason = MACHINE_VER_DEPRECATION_MSG; \
        } \
    } while (0)

#define MACHINE_VER_DELETION(...) \
    do { \
        if (MACHINE_VER_SHOULD_DELETE(__VA_ARGS__)) { \
            if (getenv("QEMU_DELETE_MACHINES") || \
                QEMU_VERSION_MAJOR > 10 || (QEMU_VERSION_MAJOR == 10 && \
                                            QEMU_VERSION_MINOR >= 1)) { \
                return; \
            } \
        } \
    } while (0)

#define DEFINE_MACHINE(namestr, machine_initfn) \
    static void machine_initfn##_class_init(ObjectClass *oc, void *data) \
    { \
        MachineClass *mc = MACHINE_CLASS(oc); \
        machine_initfn(mc); \
    } \
    static const TypeInfo machine_initfn##_typeinfo = { \
        .name       = MACHINE_TYPE_NAME(namestr), \
        .parent     = TYPE_MACHINE, \
        .class_init = machine_initfn##_class_init, \
    }; \
    static void machine_initfn##_register_types(void) \
    { \
        type_register_static(&machine_initfn##_typeinfo); \
    } \
    type_init(machine_initfn##_register_types)

#endif
