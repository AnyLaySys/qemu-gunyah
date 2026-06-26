
#ifndef MEMORY_H
#define MEMORY_H

#ifndef CONFIG_USER_ONLY

#include "exec/cpu-common.h"
#include "exec/hwaddr.h"
#include "exec/memattrs.h"
#include "exec/memop.h"
#include "exec/ramlist.h"
#include "qemu/bswap.h"
#include "qemu/queue.h"
#include "qemu/int128.h"
#include "qemu/range.h"
#include "qemu/notify.h"
#include "qom/object.h"
#include "qemu/rcu.h"

#define RAM_ADDR_INVALID (~(ram_addr_t)0)

#define MAX_PHYS_ADDR_SPACE_BITS 62
#define MAX_PHYS_ADDR            (((hwaddr)1 << MAX_PHYS_ADDR_SPACE_BITS) - 1)

#define TYPE_MEMORY_REGION "memory-region"
DECLARE_INSTANCE_CHECKER(MemoryRegion, MEMORY_REGION,
                         TYPE_MEMORY_REGION)

#define TYPE_IOMMU_MEMORY_REGION "iommu-memory-region"
typedef struct IOMMUMemoryRegionClass IOMMUMemoryRegionClass;
DECLARE_OBJ_CHECKERS(IOMMUMemoryRegion, IOMMUMemoryRegionClass,
                     IOMMU_MEMORY_REGION, TYPE_IOMMU_MEMORY_REGION)

#define TYPE_RAM_DISCARD_MANAGER "ram-discard-manager"
typedef struct RamDiscardManagerClass RamDiscardManagerClass;
typedef struct RamDiscardManager RamDiscardManager;
DECLARE_OBJ_CHECKERS(RamDiscardManager, RamDiscardManagerClass,
                     RAM_DISCARD_MANAGER, TYPE_RAM_DISCARD_MANAGER);

#ifdef CONFIG_FUZZ
void fuzz_dma_read_cb(size_t addr,
                      size_t len,
                      MemoryRegion *mr);
#else
static inline void fuzz_dma_read_cb(size_t addr,
                                    size_t len,
                                    MemoryRegion *mr)
{
}
#endif


#define GLOBAL_DIRTY_MIGRATION  (1U << 0)

#define GLOBAL_DIRTY_DIRTY_RATE (1U << 1)

#define GLOBAL_DIRTY_LIMIT      (1U << 2)

#define GLOBAL_DIRTY_MASK  (0x7)

extern unsigned int global_dirty_tracking;

typedef struct MemoryRegionOps MemoryRegionOps;

struct ReservedRegion {
    Range range;
    unsigned type;
};

struct MemoryRegionSection {
    Int128 size;
    MemoryRegion *mr;
    FlatView *fv;
    hwaddr offset_within_region;
    hwaddr offset_within_address_space;
    bool readonly;
    bool nonvolatile;
    bool unmergeable;
};

typedef struct IOMMUTLBEntry IOMMUTLBEntry;

typedef enum {
    IOMMU_NONE = 0,
    IOMMU_RO   = 1,
    IOMMU_WO   = 2,
    IOMMU_RW   = 3,
} IOMMUAccessFlags;

#define IOMMU_ACCESS_FLAG(r, w) (((r) ? IOMMU_RO : 0) | ((w) ? IOMMU_WO : 0))

struct IOMMUTLBEntry {
    AddressSpace    *target_as;
    hwaddr           iova;
    hwaddr           translated_addr;
    hwaddr           addr_mask;  /* 0xfff = 4k translation */
    IOMMUAccessFlags perm;
};

typedef enum {
    IOMMU_NOTIFIER_NONE = 0,
    IOMMU_NOTIFIER_UNMAP = 0x1,
    IOMMU_NOTIFIER_MAP = 0x2,
    IOMMU_NOTIFIER_DEVIOTLB_UNMAP = 0x04,
} IOMMUNotifierFlag;

#define IOMMU_NOTIFIER_IOTLB_EVENTS (IOMMU_NOTIFIER_MAP | IOMMU_NOTIFIER_UNMAP)
#define IOMMU_NOTIFIER_DEVIOTLB_EVENTS IOMMU_NOTIFIER_DEVIOTLB_UNMAP
#define IOMMU_NOTIFIER_ALL (IOMMU_NOTIFIER_IOTLB_EVENTS | \
                            IOMMU_NOTIFIER_DEVIOTLB_EVENTS)

struct IOMMUNotifier;
typedef void (*IOMMUNotify)(struct IOMMUNotifier *notifier,
                            IOMMUTLBEntry *data);

struct IOMMUNotifier {
    IOMMUNotify notify;
    IOMMUNotifierFlag notifier_flags;
    hwaddr start;
    hwaddr end;
    int iommu_idx;
    QLIST_ENTRY(IOMMUNotifier) node;
};
typedef struct IOMMUNotifier IOMMUNotifier;

typedef struct IOMMUTLBEvent {
    IOMMUNotifierFlag type;
    IOMMUTLBEntry entry;
} IOMMUTLBEvent;

#define RAM_PREALLOC   (1 << 0)

#define RAM_SHARED     (1 << 1)

#define RAM_RESIZEABLE (1 << 2)

#define RAM_UF_ZEROPAGE (1 << 3)

#define RAM_MIGRATABLE (1 << 4)

#define RAM_PMEM (1 << 5)


#define RAM_UF_WRITEPROTECT (1 << 6)

#define RAM_NORESERVE (1 << 7)

#define RAM_PROTECTED (1 << 8)

#define RAM_NAMED_FILE (1 << 9)

#define RAM_READONLY (1 << 10)

#define RAM_READONLY_FD (1 << 11)

#define RAM_GUEST_MEMFD   (1 << 12)

#define RAM_PRIVATE (1 << 13)

static inline void iommu_notifier_init(IOMMUNotifier *n, IOMMUNotify fn,
                                       IOMMUNotifierFlag flags,
                                       hwaddr start, hwaddr end,
                                       int iommu_idx)
{
    n->notify = fn;
    n->notifier_flags = flags;
    n->start = start;
    n->end = end;
    n->iommu_idx = iommu_idx;
}

struct MemoryRegionOps {
    uint64_t (*read)(void *opaque,
                     hwaddr addr,
                     unsigned size);
    void (*write)(void *opaque,
                  hwaddr addr,
                  uint64_t data,
                  unsigned size);

    MemTxResult (*read_with_attrs)(void *opaque,
                                   hwaddr addr,
                                   uint64_t *data,
                                   unsigned size,
                                   MemTxAttrs attrs);
    MemTxResult (*write_with_attrs)(void *opaque,
                                    hwaddr addr,
                                    uint64_t data,
                                    unsigned size,
                                    MemTxAttrs attrs);

    enum device_endian endianness;
    struct {
        unsigned min_access_size;
        unsigned max_access_size;
         bool unaligned;
        bool (*accepts)(void *opaque, hwaddr addr,
                        unsigned size, bool is_write,
                        MemTxAttrs attrs);
    } valid;
    struct {
        unsigned min_access_size;
        unsigned max_access_size;
        bool unaligned;
    } impl;
};

typedef struct MemoryRegionClass {
    ObjectClass parent_class;
} MemoryRegionClass;


enum IOMMUMemoryRegionAttr {
    IOMMU_ATTR_SPAPR_TCE_FD
};

struct IOMMUMemoryRegionClass {
    MemoryRegionClass parent_class;

    IOMMUTLBEntry (*translate)(IOMMUMemoryRegion *iommu, hwaddr addr,
                               IOMMUAccessFlags flag, int iommu_idx);
    uint64_t (*get_min_page_size)(IOMMUMemoryRegion *iommu);
    int (*notify_flag_changed)(IOMMUMemoryRegion *iommu,
                               IOMMUNotifierFlag old_flags,
                               IOMMUNotifierFlag new_flags,
                               Error **errp);
    void (*replay)(IOMMUMemoryRegion *iommu, IOMMUNotifier *notifier);

    int (*get_attr)(IOMMUMemoryRegion *iommu, enum IOMMUMemoryRegionAttr attr,
                    void *data);

    int (*attrs_to_index)(IOMMUMemoryRegion *iommu, MemTxAttrs attrs);

    int (*num_indexes)(IOMMUMemoryRegion *iommu);
};

typedef struct RamDiscardListener RamDiscardListener;
typedef int (*NotifyRamPopulate)(RamDiscardListener *rdl,
                                 MemoryRegionSection *section);
typedef void (*NotifyRamDiscard)(RamDiscardListener *rdl,
                                 MemoryRegionSection *section);

struct RamDiscardListener {
    NotifyRamPopulate notify_populate;

    NotifyRamDiscard notify_discard;

    bool double_discard_supported;

    MemoryRegionSection *section;
    QLIST_ENTRY(RamDiscardListener) next;
};

static inline void ram_discard_listener_init(RamDiscardListener *rdl,
                                             NotifyRamPopulate populate_fn,
                                             NotifyRamDiscard discard_fn,
                                             bool double_discard_supported)
{
    rdl->notify_populate = populate_fn;
    rdl->notify_discard = discard_fn;
    rdl->double_discard_supported = double_discard_supported;
}

typedef int (*ReplayRamPopulate)(MemoryRegionSection *section, void *opaque);
typedef void (*ReplayRamDiscard)(MemoryRegionSection *section, void *opaque);

struct RamDiscardManagerClass {
    InterfaceClass parent_class;


    uint64_t (*get_min_granularity)(const RamDiscardManager *rdm,
                                    const MemoryRegion *mr);

    bool (*is_populated)(const RamDiscardManager *rdm,
                         const MemoryRegionSection *section);

    int (*replay_populated)(const RamDiscardManager *rdm,
                            MemoryRegionSection *section,
                            ReplayRamPopulate replay_fn, void *opaque);

    void (*replay_discarded)(const RamDiscardManager *rdm,
                             MemoryRegionSection *section,
                             ReplayRamDiscard replay_fn, void *opaque);

    void (*register_listener)(RamDiscardManager *rdm,
                              RamDiscardListener *rdl,
                              MemoryRegionSection *section);

    void (*unregister_listener)(RamDiscardManager *rdm,
                                RamDiscardListener *rdl);
};

uint64_t ram_discard_manager_get_min_granularity(const RamDiscardManager *rdm,
                                                 const MemoryRegion *mr);

bool ram_discard_manager_is_populated(const RamDiscardManager *rdm,
                                      const MemoryRegionSection *section);

int ram_discard_manager_replay_populated(const RamDiscardManager *rdm,
                                         MemoryRegionSection *section,
                                         ReplayRamPopulate replay_fn,
                                         void *opaque);

void ram_discard_manager_replay_discarded(const RamDiscardManager *rdm,
                                          MemoryRegionSection *section,
                                          ReplayRamDiscard replay_fn,
                                          void *opaque);

void ram_discard_manager_register_listener(RamDiscardManager *rdm,
                                           RamDiscardListener *rdl,
                                           MemoryRegionSection *section);

void ram_discard_manager_unregister_listener(RamDiscardManager *rdm,
                                             RamDiscardListener *rdl);

bool memory_get_xlat_addr(IOMMUTLBEntry *iotlb, void **vaddr,
                          ram_addr_t *ram_addr, bool *read_only,
                          bool *mr_has_discard_manager, Error **errp);

typedef struct CoalescedMemoryRange CoalescedMemoryRange;
typedef struct MemoryRegionIoeventfd MemoryRegionIoeventfd;

struct MemoryRegion {
    Object parent_obj;


    bool romd_mode;
    bool ram;
    bool subpage;
    bool readonly; /* For RAM regions */
    bool nonvolatile;
    bool rom_device;
    bool flush_coalesced_mmio;
    bool unmergeable;
    uint8_t dirty_log_mask;
    bool is_iommu;
    RAMBlock *ram_block;
    Object *owner;
    DeviceState *dev;

    const MemoryRegionOps *ops;
    void *opaque;
    MemoryRegion *container;
    int mapped_via_alias; /* Mapped via an alias, container might be NULL */
    Int128 size;
    hwaddr addr;
    void (*destructor)(MemoryRegion *mr);
    uint64_t align;
    bool terminates;
    bool ram_device;
    bool enabled;
    uint8_t vga_logging_count;
    MemoryRegion *alias;
    hwaddr alias_offset;
    int32_t priority;
    QTAILQ_HEAD(, MemoryRegion) subregions;
    QTAILQ_ENTRY(MemoryRegion) subregions_link;
    QTAILQ_HEAD(, CoalescedMemoryRange) coalesced;
    const char *name;
    unsigned ioeventfd_nb;
    MemoryRegionIoeventfd *ioeventfds;
    RamDiscardManager *rdm; /* Only for RAM */

    bool disable_reentrancy_guard;
};

struct IOMMUMemoryRegion {
    MemoryRegion parent_obj;

    QLIST_HEAD(, IOMMUNotifier) iommu_notify;
    IOMMUNotifierFlag iommu_notify_flags;
};

#define IOMMU_NOTIFIER_FOREACH(n, mr) \
    QLIST_FOREACH((n), &(mr)->iommu_notify, node)

#define MEMORY_LISTENER_PRIORITY_MIN            0
#define MEMORY_LISTENER_PRIORITY_ACCEL          10
#define MEMORY_LISTENER_PRIORITY_DEV_BACKEND    10

struct MemoryListener {
    void (*begin)(MemoryListener *listener);

    void (*commit)(MemoryListener *listener);

    void (*region_add)(MemoryListener *listener, MemoryRegionSection *section);

    void (*region_del)(MemoryListener *listener, MemoryRegionSection *section);

    void (*region_nop)(MemoryListener *listener, MemoryRegionSection *section);

    void (*log_start)(MemoryListener *listener, MemoryRegionSection *section,
                      int old_val, int new_val);

    void (*log_stop)(MemoryListener *listener, MemoryRegionSection *section,
                     int old_val, int new_val);

    void (*log_sync)(MemoryListener *listener, MemoryRegionSection *section);

    void (*log_sync_global)(MemoryListener *listener, bool last_stage);

    void (*log_clear)(MemoryListener *listener, MemoryRegionSection *section);

    bool (*log_global_start)(MemoryListener *listener, Error **errp);

    void (*log_global_stop)(MemoryListener *listener);

    void (*log_global_after_sync)(MemoryListener *listener);

    void (*eventfd_add)(MemoryListener *listener, MemoryRegionSection *section,
                        bool match_data, uint64_t data, EventNotifier *e);

    void (*eventfd_del)(MemoryListener *listener, MemoryRegionSection *section,
                        bool match_data, uint64_t data, EventNotifier *e);

    void (*coalesced_io_add)(MemoryListener *listener, MemoryRegionSection *section,
                               hwaddr addr, hwaddr len);

    void (*coalesced_io_del)(MemoryListener *listener, MemoryRegionSection *section,
                               hwaddr addr, hwaddr len);
    unsigned priority;

    const char *name;

    AddressSpace *address_space;
    QTAILQ_ENTRY(MemoryListener) link;
    QTAILQ_ENTRY(MemoryListener) link_as;
};

typedef struct AddressSpaceMapClient {
    QEMUBH *bh;
    QLIST_ENTRY(AddressSpaceMapClient) link;
} AddressSpaceMapClient;

#define DEFAULT_MAX_BOUNCE_BUFFER_SIZE (4096)

struct AddressSpace {
    struct rcu_head rcu;
    char *name;
    MemoryRegion *root;

    struct FlatView *current_map;

    int ioeventfd_nb;
    int ioeventfd_notifiers;
    struct MemoryRegionIoeventfd *ioeventfds;
    QTAILQ_HEAD(, MemoryListener) listeners;
    QTAILQ_ENTRY(AddressSpace) address_spaces_link;

    size_t max_bounce_buffer_size;
    size_t bounce_buffer_size;
    QemuMutex map_client_list_lock;
    QLIST_HEAD(, AddressSpaceMapClient) map_client_list;
};

typedef struct AddressSpaceDispatch AddressSpaceDispatch;
typedef struct FlatRange FlatRange;

struct FlatView {
    struct rcu_head rcu;
    unsigned ref;
    FlatRange *ranges;
    unsigned nr;
    unsigned nr_allocated;
    struct AddressSpaceDispatch *dispatch;
    MemoryRegion *root;
};

static inline FlatView *address_space_to_flatview(AddressSpace *as)
{
    return qatomic_rcu_read(&as->current_map);
}

typedef bool (*flatview_cb)(Int128 start,
                            Int128 len,
                            const MemoryRegion *mr,
                            hwaddr offset_in_region,
                            void *opaque);

void flatview_for_each_range(FlatView *fv, flatview_cb cb, void *opaque);

static inline bool MemoryRegionSection_eq(MemoryRegionSection *a,
                                          MemoryRegionSection *b)
{
    return a->mr == b->mr &&
           a->fv == b->fv &&
           a->offset_within_region == b->offset_within_region &&
           a->offset_within_address_space == b->offset_within_address_space &&
           int128_eq(a->size, b->size) &&
           a->readonly == b->readonly &&
           a->nonvolatile == b->nonvolatile;
}

MemoryRegionSection *memory_region_section_new_copy(MemoryRegionSection *s);

void memory_region_section_free_copy(MemoryRegionSection *s);

void memory_region_init(MemoryRegion *mr,
                        Object *owner,
                        const char *name,
                        uint64_t size);

void memory_region_ref(MemoryRegion *mr);

void memory_region_unref(MemoryRegion *mr);

void memory_region_init_io(MemoryRegion *mr,
                           Object *owner,
                           const MemoryRegionOps *ops,
                           void *opaque,
                           const char *name,
                           uint64_t size);

bool memory_region_init_ram_nomigrate(MemoryRegion *mr,
                                      Object *owner,
                                      const char *name,
                                      uint64_t size,
                                      Error **errp);

bool memory_region_init_ram_flags_nomigrate(MemoryRegion *mr,
                                            Object *owner,
                                            const char *name,
                                            uint64_t size,
                                            uint32_t ram_flags,
                                            Error **errp);

bool memory_region_init_resizeable_ram(MemoryRegion *mr,
                                       Object *owner,
                                       const char *name,
                                       uint64_t size,
                                       uint64_t max_size,
                                       void (*resized)(const char*,
                                                       uint64_t length,
                                                       void *host),
                                       Error **errp);
#ifdef CONFIG_POSIX

bool memory_region_init_ram_from_file(MemoryRegion *mr,
                                      Object *owner,
                                      const char *name,
                                      uint64_t size,
                                      uint64_t align,
                                      uint32_t ram_flags,
                                      const char *path,
                                      ram_addr_t offset,
                                      Error **errp);

bool memory_region_init_ram_from_fd(MemoryRegion *mr,
                                    Object *owner,
                                    const char *name,
                                    uint64_t size,
                                    uint32_t ram_flags,
                                    int fd,
                                    ram_addr_t offset,
                                    Error **errp);
#endif

void memory_region_init_ram_ptr(MemoryRegion *mr,
                                Object *owner,
                                const char *name,
                                uint64_t size,
                                void *ptr);

void memory_region_init_ram_device_ptr(MemoryRegion *mr,
                                       Object *owner,
                                       const char *name,
                                       uint64_t size,
                                       void *ptr);

void memory_region_init_alias(MemoryRegion *mr,
                              Object *owner,
                              const char *name,
                              MemoryRegion *orig,
                              hwaddr offset,
                              uint64_t size);

bool memory_region_init_rom_nomigrate(MemoryRegion *mr,
                                      Object *owner,
                                      const char *name,
                                      uint64_t size,
                                      Error **errp);

bool memory_region_init_rom_device_nomigrate(MemoryRegion *mr,
                                             Object *owner,
                                             const MemoryRegionOps *ops,
                                             void *opaque,
                                             const char *name,
                                             uint64_t size,
                                             Error **errp);

void memory_region_init_iommu(void *_iommu_mr,
                              size_t instance_size,
                              const char *mrtypename,
                              Object *owner,
                              const char *name,
                              uint64_t size);

bool memory_region_init_ram(MemoryRegion *mr,
                            Object *owner,
                            const char *name,
                            uint64_t size,
                            Error **errp);

bool memory_region_init_ram_guest_memfd(MemoryRegion *mr,
                                        Object *owner,
                                        const char *name,
                                        uint64_t size,
                                        Error **errp);

bool memory_region_init_rom(MemoryRegion *mr,
                            Object *owner,
                            const char *name,
                            uint64_t size,
                            Error **errp);

bool memory_region_init_rom_device(MemoryRegion *mr,
                                   Object *owner,
                                   const MemoryRegionOps *ops,
                                   void *opaque,
                                   const char *name,
                                   uint64_t size,
                                   Error **errp);


Object *memory_region_owner(MemoryRegion *mr);

uint64_t memory_region_size(MemoryRegion *mr);

static inline bool memory_region_is_ram(MemoryRegion *mr)
{
    return mr->ram;
}

bool memory_region_is_ram_device(MemoryRegion *mr);

static inline bool memory_region_is_romd(MemoryRegion *mr)
{
    return mr->rom_device && mr->romd_mode;
}

bool memory_region_is_protected(MemoryRegion *mr);

bool memory_region_has_guest_memfd(MemoryRegion *mr);

static inline IOMMUMemoryRegion *memory_region_get_iommu(MemoryRegion *mr)
{
    if (mr->alias) {
        return memory_region_get_iommu(mr->alias);
    }
    if (mr->is_iommu) {
        return (IOMMUMemoryRegion *) mr;
    }
    return NULL;
}

static inline IOMMUMemoryRegionClass *memory_region_get_iommu_class_nocheck(
        IOMMUMemoryRegion *iommu_mr)
{
    return (IOMMUMemoryRegionClass *) (((Object *)iommu_mr)->class);
}

#define memory_region_is_iommu(mr) (memory_region_get_iommu(mr) != NULL)

uint64_t memory_region_iommu_get_min_page_size(IOMMUMemoryRegion *iommu_mr);

void memory_region_notify_iommu(IOMMUMemoryRegion *iommu_mr,
                                int iommu_idx,
                                const IOMMUTLBEvent event);

void memory_region_notify_iommu_one(IOMMUNotifier *notifier,
                                    const IOMMUTLBEvent *event);

void memory_region_unmap_iommu_notifier_range(IOMMUNotifier *notifier);


int memory_region_register_iommu_notifier(MemoryRegion *mr,
                                          IOMMUNotifier *n, Error **errp);

void memory_region_iommu_replay(IOMMUMemoryRegion *iommu_mr, IOMMUNotifier *n);

void memory_region_unregister_iommu_notifier(MemoryRegion *mr,
                                             IOMMUNotifier *n);

int memory_region_iommu_get_attr(IOMMUMemoryRegion *iommu_mr,
                                 enum IOMMUMemoryRegionAttr attr,
                                 void *data);

int memory_region_iommu_attrs_to_index(IOMMUMemoryRegion *iommu_mr,
                                       MemTxAttrs attrs);

int memory_region_iommu_num_indexes(IOMMUMemoryRegion *iommu_mr);

const char *memory_region_name(const MemoryRegion *mr);

bool memory_region_is_logging(MemoryRegion *mr, uint8_t client);

uint8_t memory_region_get_dirty_log_mask(MemoryRegion *mr);

static inline bool memory_region_is_rom(MemoryRegion *mr)
{
    return mr->ram && mr->readonly;
}

static inline bool memory_region_is_nonvolatile(MemoryRegion *mr)
{
    return mr->nonvolatile;
}

int memory_region_get_fd(MemoryRegion *mr);

MemoryRegion *memory_region_from_host(void *ptr, ram_addr_t *offset);

void *memory_region_get_ram_ptr(MemoryRegion *mr);

void memory_region_ram_resize(MemoryRegion *mr, ram_addr_t newsize,
                              Error **errp);

void memory_region_msync(MemoryRegion *mr, hwaddr addr, hwaddr size);

void memory_region_writeback(MemoryRegion *mr, hwaddr addr, hwaddr size);

void memory_region_set_log(MemoryRegion *mr, bool log, unsigned client);

void memory_region_set_dirty(MemoryRegion *mr, hwaddr addr,
                             hwaddr size);

void memory_region_clear_dirty_bitmap(MemoryRegion *mr, hwaddr start,
                                      hwaddr len);

DirtyBitmapSnapshot *memory_region_snapshot_and_clear_dirty(MemoryRegion *mr,
                                                            hwaddr addr,
                                                            hwaddr size,
                                                            unsigned client);

bool memory_region_snapshot_get_dirty(MemoryRegion *mr,
                                      DirtyBitmapSnapshot *snap,
                                      hwaddr addr, hwaddr size);

void memory_region_reset_dirty(MemoryRegion *mr, hwaddr addr,
                               hwaddr size, unsigned client);

void memory_region_flush_rom_device(MemoryRegion *mr, hwaddr addr, hwaddr size);

void memory_region_set_readonly(MemoryRegion *mr, bool readonly);

void memory_region_set_nonvolatile(MemoryRegion *mr, bool nonvolatile);

void memory_region_rom_device_set_romd(MemoryRegion *mr, bool romd_mode);

void memory_region_set_coalescing(MemoryRegion *mr);

void memory_region_add_coalescing(MemoryRegion *mr,
                                  hwaddr offset,
                                  uint64_t size);

void memory_region_clear_coalescing(MemoryRegion *mr);

void memory_region_set_flush_coalesced(MemoryRegion *mr);

void memory_region_clear_flush_coalesced(MemoryRegion *mr);

void memory_region_add_eventfd(MemoryRegion *mr,
                               hwaddr addr,
                               unsigned size,
                               bool match_data,
                               uint64_t data,
                               EventNotifier *e);

void memory_region_del_eventfd(MemoryRegion *mr,
                               hwaddr addr,
                               unsigned size,
                               bool match_data,
                               uint64_t data,
                               EventNotifier *e);

void memory_region_add_subregion(MemoryRegion *mr,
                                 hwaddr offset,
                                 MemoryRegion *subregion);
void memory_region_add_subregion_overlap(MemoryRegion *mr,
                                         hwaddr offset,
                                         MemoryRegion *subregion,
                                         int priority);

ram_addr_t memory_region_get_ram_addr(MemoryRegion *mr);

uint64_t memory_region_get_alignment(const MemoryRegion *mr);
void memory_region_del_subregion(MemoryRegion *mr,
                                 MemoryRegion *subregion);

void memory_region_set_enabled(MemoryRegion *mr, bool enabled);

void memory_region_set_address(MemoryRegion *mr, hwaddr addr);

void memory_region_set_size(MemoryRegion *mr, uint64_t size);

void memory_region_set_alias_offset(MemoryRegion *mr,
                                    hwaddr offset);

void memory_region_set_unmergeable(MemoryRegion *mr, bool unmergeable);

bool memory_region_present(MemoryRegion *container, hwaddr addr);

bool memory_region_is_mapped(MemoryRegion *mr);

RamDiscardManager *memory_region_get_ram_discard_manager(MemoryRegion *mr);

static inline bool memory_region_has_ram_discard_manager(MemoryRegion *mr)
{
    return !!memory_region_get_ram_discard_manager(mr);
}

void memory_region_set_ram_discard_manager(MemoryRegion *mr,
                                           RamDiscardManager *rdm);

MemoryRegionSection memory_region_find(MemoryRegion *mr,
                                       hwaddr addr, uint64_t size);

void memory_global_dirty_log_sync(bool last_stage);

void memory_global_after_dirty_log_sync(void);

void memory_region_transaction_begin(void);

void memory_region_transaction_commit(void);

void memory_listener_register(MemoryListener *listener, AddressSpace *filter);

void memory_listener_unregister(MemoryListener *listener);

bool memory_global_dirty_log_start(unsigned int flags, Error **errp);

void memory_global_dirty_log_stop(unsigned int flags);

void mtree_info(bool flatview, bool dispatch_tree, bool owner, bool disabled);

bool memory_region_access_valid(MemoryRegion *mr, hwaddr addr,
                                unsigned size, bool is_write,
                                MemTxAttrs attrs);

MemTxResult memory_region_dispatch_read(MemoryRegion *mr,
                                        hwaddr addr,
                                        uint64_t *pval,
                                        MemOp op,
                                        MemTxAttrs attrs);
MemTxResult memory_region_dispatch_write(MemoryRegion *mr,
                                         hwaddr addr,
                                         uint64_t data,
                                         MemOp op,
                                         MemTxAttrs attrs);

void address_space_init(AddressSpace *as, MemoryRegion *root, const char *name);

void address_space_destroy(AddressSpace *as);

void address_space_remove_listeners(AddressSpace *as);

MemTxResult address_space_rw(AddressSpace *as, hwaddr addr,
                             MemTxAttrs attrs, void *buf,
                             hwaddr len, bool is_write);

MemTxResult address_space_write(AddressSpace *as, hwaddr addr,
                                MemTxAttrs attrs,
                                const void *buf, hwaddr len);

MemTxResult address_space_write_rom(AddressSpace *as, hwaddr addr,
                                    MemTxAttrs attrs,
                                    const void *buf, hwaddr len);


#define SUFFIX
#define ARG1         as
#define ARG1_DECL    AddressSpace *as
#include "exec/memory_ldst.h.inc"

#define SUFFIX
#define ARG1         as
#define ARG1_DECL    AddressSpace *as
#include "exec/memory_ldst_phys.h.inc"

struct MemoryRegionCache {
    uint8_t *ptr;
    hwaddr xlat;
    hwaddr len;
    FlatView *fv;
    MemoryRegionSection mrs;
    bool is_write;
};


#define SUFFIX       _cached_slow
#define ARG1         cache
#define ARG1_DECL    MemoryRegionCache *cache
#include "exec/memory_ldst.h.inc"

static inline uint8_t address_space_ldub_cached(MemoryRegionCache *cache,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result)
{
    assert(addr < cache->len);
    if (likely(cache->ptr)) {
        return ldub_p(cache->ptr + addr);
    } else {
        return address_space_ldub_cached_slow(cache, addr, attrs, result);
    }
}

static inline void address_space_stb_cached(MemoryRegionCache *cache,
    hwaddr addr, uint8_t val, MemTxAttrs attrs, MemTxResult *result)
{
    assert(addr < cache->len);
    if (likely(cache->ptr)) {
        stb_p(cache->ptr + addr, val);
    } else {
        address_space_stb_cached_slow(cache, addr, val, attrs, result);
    }
}

#define ENDIANNESS   _le
#include "exec/memory_ldst_cached.h.inc"

#define ENDIANNESS   _be
#include "exec/memory_ldst_cached.h.inc"

#define SUFFIX       _cached
#define ARG1         cache
#define ARG1_DECL    MemoryRegionCache *cache
#include "exec/memory_ldst_phys.h.inc"

int64_t address_space_cache_init(MemoryRegionCache *cache,
                                 AddressSpace *as,
                                 hwaddr addr,
                                 hwaddr len,
                                 bool is_write);

static inline void address_space_cache_init_empty(MemoryRegionCache *cache)
{
    cache->mrs.mr = NULL;
    cache->fv = NULL;
}

void address_space_cache_invalidate(MemoryRegionCache *cache,
                                    hwaddr addr,
                                    hwaddr access_len);

void address_space_cache_destroy(MemoryRegionCache *cache);

IOMMUTLBEntry address_space_get_iotlb_entry(AddressSpace *as, hwaddr addr,
                                            bool is_write, MemTxAttrs attrs);

MemoryRegion *flatview_translate(FlatView *fv,
                                 hwaddr addr, hwaddr *xlat,
                                 hwaddr *len, bool is_write,
                                 MemTxAttrs attrs);

static inline MemoryRegion *address_space_translate(AddressSpace *as,
                                                    hwaddr addr, hwaddr *xlat,
                                                    hwaddr *len, bool is_write,
                                                    MemTxAttrs attrs)
{
    return flatview_translate(address_space_to_flatview(as),
                              addr, xlat, len, is_write, attrs);
}

bool address_space_access_valid(AddressSpace *as, hwaddr addr, hwaddr len,
                                bool is_write, MemTxAttrs attrs);

void *address_space_map(AddressSpace *as, hwaddr addr,
                        hwaddr *plen, bool is_write, MemTxAttrs attrs);

void address_space_unmap(AddressSpace *as, void *buffer, hwaddr len,
                         bool is_write, hwaddr access_len);

void address_space_register_map_client(AddressSpace *as, QEMUBH *bh);

void address_space_unregister_map_client(AddressSpace *as, QEMUBH *bh);

MemTxResult address_space_read_full(AddressSpace *as, hwaddr addr,
                                    MemTxAttrs attrs, void *buf, hwaddr len);
MemTxResult flatview_read_continue(FlatView *fv, hwaddr addr,
                                   MemTxAttrs attrs, void *buf,
                                   hwaddr len, hwaddr addr1, hwaddr l,
                                   MemoryRegion *mr);
void *qemu_map_ram_ptr(RAMBlock *ram_block, ram_addr_t addr);

MemTxResult address_space_read_cached_slow(MemoryRegionCache *cache,
                                           hwaddr addr, void *buf, hwaddr len);
MemTxResult address_space_write_cached_slow(MemoryRegionCache *cache,
                                            hwaddr addr, const void *buf,
                                            hwaddr len);

int memory_access_size(MemoryRegion *mr, unsigned l, hwaddr addr);
bool prepare_mmio_access(MemoryRegion *mr);

static inline bool memory_region_supports_direct_access(MemoryRegion *mr)
{
    if (memory_region_is_romd(mr)) {
        return true;
    }
    if (!memory_region_is_ram(mr)) {
        return false;
    }
    return !memory_region_is_ram_device(mr);
}

static inline bool memory_access_is_direct(MemoryRegion *mr, bool is_write,
                                           MemTxAttrs attrs)
{
    if (!memory_region_supports_direct_access(mr)) {
        return false;
    }
    if (is_write && !attrs.debug) {
        return !mr->readonly && !mr->rom_device;
    }
    return true;
}

static inline __attribute__((__always_inline__))
MemTxResult address_space_read(AddressSpace *as, hwaddr addr,
                               MemTxAttrs attrs, void *buf,
                               hwaddr len)
{
    MemTxResult result = MEMTX_OK;
    hwaddr l, addr1;
    void *ptr;
    MemoryRegion *mr;
    FlatView *fv;

    if (__builtin_constant_p(len)) {
        if (len) {
            RCU_READ_LOCK_GUARD();
            fv = address_space_to_flatview(as);
            l = len;
            mr = flatview_translate(fv, addr, &addr1, &l, false, attrs);
            if (len == l && memory_access_is_direct(mr, false, attrs)) {
                ptr = qemu_map_ram_ptr(mr->ram_block, addr1);
                memcpy(buf, ptr, len);
            } else {
                result = flatview_read_continue(fv, addr, attrs, buf, len,
                                                addr1, l, mr);
            }
        }
    } else {
        result = address_space_read_full(as, addr, attrs, buf, len);
    }
    return result;
}

static inline MemTxResult
address_space_read_cached(MemoryRegionCache *cache, hwaddr addr,
                          void *buf, hwaddr len)
{
    assert(addr < cache->len && len <= cache->len - addr);
    fuzz_dma_read_cb(cache->xlat + addr, len, cache->mrs.mr);
    if (likely(cache->ptr)) {
        memcpy(buf, cache->ptr + addr, len);
        return MEMTX_OK;
    } else {
        return address_space_read_cached_slow(cache, addr, buf, len);
    }
}

static inline MemTxResult
address_space_write_cached(MemoryRegionCache *cache, hwaddr addr,
                           const void *buf, hwaddr len)
{
    assert(addr < cache->len && len <= cache->len - addr);
    if (likely(cache->ptr)) {
        memcpy(cache->ptr + addr, buf, len);
        return MEMTX_OK;
    } else {
        return address_space_write_cached_slow(cache, addr, buf, len);
    }
}

MemTxResult address_space_set(AddressSpace *as, hwaddr addr,
                              uint8_t c, hwaddr len, MemTxAttrs attrs);

#ifdef COMPILING_PER_TARGET
static inline MemOp devend_memop(enum device_endian end)
{
    QEMU_BUILD_BUG_ON(DEVICE_HOST_ENDIAN != DEVICE_LITTLE_ENDIAN &&
                      DEVICE_HOST_ENDIAN != DEVICE_BIG_ENDIAN);

#if HOST_BIG_ENDIAN != TARGET_BIG_ENDIAN
    return (end == DEVICE_HOST_ENDIAN) ? 0 : MO_BSWAP;
#else
    const int non_host_endianness =
        DEVICE_LITTLE_ENDIAN ^ DEVICE_BIG_ENDIAN ^ DEVICE_HOST_ENDIAN;

    return (end == non_host_endianness) ? MO_BSWAP : 0;
#endif
}
#endif /* COMPILING_PER_TARGET */

int ram_block_discard_disable(bool state);

int ram_block_uncoordinated_discard_disable(bool state);

int ram_block_discard_require(bool state);

int ram_block_coordinated_discard_require(bool state);

bool ram_block_discard_is_disabled(void);

bool ram_block_discard_is_required(void);

#endif

#endif
