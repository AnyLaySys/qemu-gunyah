#ifndef QEMU_CPU_H
#define QEMU_CPU_H

#include "hw/qdev-core.h"
#include "disas/dis-asm.h"
#include "exec/breakpoint.h"
#include "exec/hwaddr.h"
#include "exec/vaddr.h"
#include "exec/memattrs.h"
#include "exec/mmu-access-type.h"
#include "exec/tlb-common.h"
#include "qapi/qapi-types-machine.h"
#include "qapi/qapi-types-run-state.h"
#include "qemu/bitmap.h"
#include "qemu/rcu_queue.h"
#include "qemu/queue.h"
#include "qemu/lockcnt.h"
#include "qemu/thread.h"
#include "qom/object.h"

typedef int (*WriteCoreDumpFunction)(const void *buf, size_t size,
                                     void *opaque);


#define TYPE_CPU "cpu"

#define CPU(obj) ((CPUState *)(obj))

typedef struct CPUClass CPUClass;
DECLARE_CLASS_CHECKERS(CPUClass, CPU,
                       TYPE_CPU)

#define OBJECT_DECLARE_CPU_TYPE(CpuInstanceType, CpuClassType, CPU_MODULE_OBJ_NAME) \
    typedef struct ArchCPU CpuInstanceType; \
    OBJECT_DECLARE_TYPE(ArchCPU, CpuClassType, CPU_MODULE_OBJ_NAME);

typedef struct CPUWatchpoint CPUWatchpoint;

struct CPUAddressSpace;

struct CPUJumpCache;

struct AccelCPUClass;

struct SysemuCPUOps;

struct CPUClass {
    DeviceClass parent_class;

    ObjectClass *(*class_by_name)(const char *cpu_model);
    void (*parse_features)(const char *typename, char *str, Error **errp);

    int (*mmu_index)(CPUState *cpu, bool ifetch);
    int (*memory_rw_debug)(CPUState *cpu, vaddr addr,
                           uint8_t *buf, size_t len, bool is_write);
    void (*dump_state)(CPUState *cpu, FILE *, int flags);
    void (*query_cpu_fast)(CPUState *cpu, CpuInfoFast *value);
    int64_t (*get_arch_id)(CPUState *cpu);
    void (*set_pc)(CPUState *cpu, vaddr value);
    vaddr (*get_pc)(CPUState *cpu);
    void (*disas_set_info)(CPUState *cpu, disassemble_info *info);

    const char *deprecation_note;
    struct AccelCPUClass *accel_cpu;

    const struct SysemuCPUOps *sysemu_ops;

    void (*init_accel_cpu)(struct AccelCPUClass *accel_cpu, CPUClass *cc);

    int reset_dump_flags;
};

#define NB_MMU_MODES 16

#define CPU_VTLB_SIZE 8

struct CPUTLBEntryFull {
    hwaddr xlat_section;

    hwaddr phys_addr;

    MemTxAttrs attrs;

    uint8_t prot;

    uint8_t lg_page_size;

    uint8_t tlb_fill_flags;

    uint8_t slow_flags[MMU_ACCESS_COUNT];

    union {
        struct {
            uint8_t pte_attrs;
            uint8_t shareability;
            bool guarded;
        } arm;
    } extra;
};

typedef struct CPUTLBDesc {
    vaddr large_page_addr;
    vaddr large_page_mask;
    int64_t window_begin_ns;
    size_t window_max_entries;
    size_t n_used_entries;
    size_t vindex;
    CPUTLBEntry vtable[CPU_VTLB_SIZE];
    CPUTLBEntryFull vfulltlb[CPU_VTLB_SIZE];
    CPUTLBEntryFull *fulltlb;
} CPUTLBDesc;

typedef struct CPUTLBCommon {
    QemuSpin lock;
    uint16_t dirty;
    size_t full_flush_count;
    size_t part_flush_count;
    size_t elide_flush_count;
} CPUTLBCommon;

typedef struct CPUTLB {
} CPUTLB;

typedef union IcountDecr {
    uint32_t u32;
    struct {
#if HOST_BIG_ENDIAN
        uint16_t high;
        uint16_t low;
#else
        uint16_t low;
        uint16_t high;
#endif
    } u16;
} IcountDecr;

typedef struct CPUNegativeOffsetState {
    CPUTLB tlb;
    IcountDecr icount_decr;
    bool can_do_io;
} CPUNegativeOffsetState;

struct KVMState;
struct kvm_run;


typedef union {
    int           host_int;
    unsigned long host_ulong;
    void         *host_ptr;
    vaddr         target_ptr;
} run_on_cpu_data;

#define RUN_ON_CPU_HOST_PTR(p)    ((run_on_cpu_data){.host_ptr = (p)})
#define RUN_ON_CPU_HOST_INT(i)    ((run_on_cpu_data){.host_int = (i)})
#define RUN_ON_CPU_HOST_ULONG(ul) ((run_on_cpu_data){.host_ulong = (ul)})
#define RUN_ON_CPU_TARGET_PTR(v)  ((run_on_cpu_data){.target_ptr = (v)})
#define RUN_ON_CPU_NULL           RUN_ON_CPU_HOST_PTR(NULL)

typedef void (*run_on_cpu_func)(CPUState *cpu, run_on_cpu_data data);

struct qemu_work_item;

#define CPU_UNSET_NUMA_NODE_ID -1

struct CPUState {
    DeviceState parent_obj;
    CPUClass *cc;

    int nr_threads;

    struct QemuThread *thread;
#ifdef _WIN32
    QemuSemaphore sem;
#endif
    int thread_id;
    bool running, has_waiter;
    struct QemuCond *halt_cond;
    bool thread_kicked;
    bool created;
    bool stop;
    bool stopped;

    bool start_powered_off;

    bool unplug;
    bool crash_occurred;
    bool exit_request;
    int exclusive_context_count;
    uint32_t cflags_next_tb;
    uint32_t interrupt_request;
    int singlestep_enabled;
    int64_t icount_budget;
    int64_t icount_extra;
    uint64_t random_seed;
    sigjmp_buf jmp_env;

    QemuMutex work_mutex;
    QSIMPLEQ_HEAD(, qemu_work_item) work_list;

    struct CPUAddressSpace *cpu_ases;
    int cpu_ases_count;
    int num_ases;
    AddressSpace *as;
    MemoryRegion *memory;

    struct CPUJumpCache *tb_jmp_cache;

    QTAILQ_ENTRY(CPUState) node;

    QTAILQ_HEAD(, CPUBreakpoint) breakpoints;

    QTAILQ_HEAD(, CPUWatchpoint) watchpoints;
    CPUWatchpoint *watchpoint_hit;

    void *opaque;

    uintptr_t mem_io_pc;

    int kvm_fd;
    struct KVMState *kvm_state;
    struct kvm_run *kvm_run;
    struct kvm_dirty_gfn *kvm_dirty_gfns;
    uint32_t kvm_fetch_index;
    uint64_t dirty_pages;
    int kvm_vcpu_stats_fd;
    bool vcpu_dirty;

    QemuLockCnt in_ioctl_lock;

    int cpu_index;
    int cluster_index;
    uint32_t halted;
    int32_t exception_index;

    AccelCPUState *accel;

    bool throttle_thread_scheduled;

    int64_t throttle_us_per_full;

    bool ignore_memory_transaction_failures;

    bool prctl_unalign_sigbus;

    char neg_align[-sizeof(CPUNegativeOffsetState) % 16] QEMU_ALIGNED(16);
    CPUNegativeOffsetState neg;
};

QEMU_BUILD_BUG_ON(offsetof(CPUState, neg) !=
                  sizeof(CPUState) - sizeof(CPUNegativeOffsetState));

static inline CPUArchState *cpu_env(CPUState *cpu)
{
    return (CPUArchState *)(cpu + 1);
}

typedef QTAILQ_HEAD(CPUTailQ, CPUState) CPUTailQ;
extern CPUTailQ cpus_queue;

#define first_cpu        QTAILQ_FIRST_RCU(&cpus_queue)
#define CPU_NEXT(cpu)    QTAILQ_NEXT_RCU(cpu, node)
#define CPU_FOREACH(cpu) QTAILQ_FOREACH_RCU(cpu, &cpus_queue, node)
#define CPU_FOREACH_SAFE(cpu, next_cpu) \
    QTAILQ_FOREACH_SAFE_RCU(cpu, &cpus_queue, node, next_cpu)

#ifdef __ANDROID__
CPUState **android_current_cpu_ptr(void);
#define current_cpu (*android_current_cpu_ptr())
#else
extern __thread CPUState *current_cpu;
#endif

bool cpu_paging_enabled(const CPUState *cpu);

#if !defined(CONFIG_USER_ONLY)

GuestPanicInformation *cpu_get_crash_info(CPUState *cpu);

#endif /* !CONFIG_USER_ONLY */

enum CPUDumpFlags {
    CPU_DUMP_CODE = 0x00010000,
    CPU_DUMP_FPU  = 0x00020000,
    CPU_DUMP_CCOP = 0x00040000,
    CPU_DUMP_VPU  = 0x00080000,
};

void cpu_dump_state(CPUState *cpu, FILE *f, int flags);

#ifndef CONFIG_USER_ONLY
hwaddr cpu_get_phys_page_attrs_debug(CPUState *cpu, vaddr addr,
                                     MemTxAttrs *attrs);

hwaddr cpu_get_phys_page_debug(CPUState *cpu, vaddr addr);

int cpu_asidx_from_attrs(CPUState *cpu, MemTxAttrs attrs);

bool cpu_virtio_is_big_endian(CPUState *cpu);

bool cpu_has_work(CPUState *cpu);

#endif /* CONFIG_USER_ONLY */

void cpu_list_add(CPUState *cpu);

void cpu_list_remove(CPUState *cpu);

void cpu_reset(CPUState *cpu);

ObjectClass *cpu_class_by_name(const char *typename, const char *cpu_model);

char *cpu_model_from_type(const char *typename);

CPUState *cpu_create(const char *typename);

const char *parse_cpu_option(const char *cpu_option);

bool qemu_cpu_is_self(CPUState *cpu);

void qemu_cpu_kick(CPUState *cpu);

bool cpu_is_stopped(CPUState *cpu);

void do_run_on_cpu(CPUState *cpu, run_on_cpu_func func, run_on_cpu_data data,
                   QemuMutex *mutex);

void run_on_cpu(CPUState *cpu, run_on_cpu_func func, run_on_cpu_data data);

void async_run_on_cpu(CPUState *cpu, run_on_cpu_func func, run_on_cpu_data data);

void async_safe_run_on_cpu(CPUState *cpu, run_on_cpu_func func, run_on_cpu_data data);

static inline bool cpu_in_exclusive_context(const CPUState *cpu)
{
    return cpu->exclusive_context_count;
}

CPUState *qemu_get_cpu(int index);

bool cpu_exists(int64_t id);

CPUState *cpu_by_arch_id(int64_t id);


void cpu_interrupt(CPUState *cpu, int mask);

static inline void cpu_set_pc(CPUState *cpu, vaddr addr)
{
    cpu->cc->set_pc(cpu, addr);
}

void cpu_reset_interrupt(CPUState *cpu, int mask);

void cpu_exit(CPUState *cpu);

void cpu_pause(CPUState *cpu);

void cpu_resume(CPUState *cpu);

void cpu_remove_sync(CPUState *cpu);

void free_queued_cpu_work(CPUState *cpu);

void process_queued_cpu_work(CPUState *cpu);

void cpu_exec_start(CPUState *cpu);

void cpu_exec_end(CPUState *cpu);

void start_exclusive(void);

void end_exclusive(void);

void qemu_init_vcpu(CPUState *cpu);

#define SSTEP_ENABLE  0x1  /* Enable simulated HW single stepping */
#define SSTEP_NOIRQ   0x2  /* Do not use IRQ while single stepping */
#define SSTEP_NOTIMER 0x4  /* Do not Timers while single stepping */

void cpu_single_step(CPUState *cpu, int enabled);

#define BP_MEM_READ           0x01
#define BP_MEM_WRITE          0x02
#define BP_MEM_ACCESS         (BP_MEM_READ | BP_MEM_WRITE)
#define BP_STOP_BEFORE_ACCESS 0x04
#define BP_CPU                0x20
#define BP_ANY                BP_CPU
#define BP_HIT_SHIFT          6
#define BP_WATCHPOINT_HIT_READ  (BP_MEM_READ << BP_HIT_SHIFT)
#define BP_WATCHPOINT_HIT_WRITE (BP_MEM_WRITE << BP_HIT_SHIFT)
#define BP_WATCHPOINT_HIT       (BP_MEM_ACCESS << BP_HIT_SHIFT)

int cpu_breakpoint_insert(CPUState *cpu, vaddr pc, int flags,
                          CPUBreakpoint **breakpoint);
int cpu_breakpoint_remove(CPUState *cpu, vaddr pc, int flags);
void cpu_breakpoint_remove_by_ref(CPUState *cpu, CPUBreakpoint *breakpoint);
void cpu_breakpoint_remove_all(CPUState *cpu, int mask);

static inline bool cpu_breakpoint_test(CPUState *cpu, vaddr pc, int mask)
{
    CPUBreakpoint *bp;

    if (unlikely(!QTAILQ_EMPTY(&cpu->breakpoints))) {
        QTAILQ_FOREACH(bp, &cpu->breakpoints, entry) {
            if (bp->pc == pc && (bp->flags & mask)) {
                return true;
            }
        }
    }
    return false;
}

#if defined(CONFIG_USER_ONLY)
static inline int cpu_watchpoint_insert(CPUState *cpu, vaddr addr, vaddr len,
                                        int flags, CPUWatchpoint **watchpoint)
{
    return -ENOSYS;
}

static inline int cpu_watchpoint_remove(CPUState *cpu, vaddr addr,
                                        vaddr len, int flags)
{
    return -ENOSYS;
}

static inline void cpu_watchpoint_remove_by_ref(CPUState *cpu,
                                                CPUWatchpoint *wp)
{
}

static inline void cpu_watchpoint_remove_all(CPUState *cpu, int mask)
{
}
#else
int cpu_watchpoint_insert(CPUState *cpu, vaddr addr, vaddr len,
                          int flags, CPUWatchpoint **watchpoint);
int cpu_watchpoint_remove(CPUState *cpu, vaddr addr,
                          vaddr len, int flags);
void cpu_watchpoint_remove_by_ref(CPUState *cpu, CPUWatchpoint *watchpoint);
void cpu_watchpoint_remove_all(CPUState *cpu, int mask);
#endif

AddressSpace *cpu_get_address_space(CPUState *cpu, int asidx);

G_NORETURN void cpu_abort(CPUState *cpu, const char *fmt, ...)
    G_GNUC_PRINTF(2, 3);

void cpu_class_init_props(DeviceClass *dc);
void cpu_exec_class_post_init(CPUClass *cc);
void cpu_exec_initfn(CPUState *cpu);
void cpu_vmstate_register(CPUState *cpu);
void cpu_vmstate_unregister(CPUState *cpu);
bool cpu_exec_realizefn(CPUState *cpu, Error **errp);
void cpu_exec_unrealizefn(CPUState *cpu);
void cpu_exec_reset_hold(CPUState *cpu);

const char *target_name(void);

#ifdef COMPILING_PER_TARGET

#ifndef CONFIG_USER_ONLY

extern const VMStateDescription vmstate_cpu_common;

#define VMSTATE_CPU() {                                                     \
    .name = "parent_obj",                                                   \
    .size = sizeof(CPUState),                                               \
    .vmsd = &vmstate_cpu_common,                                            \
    .flags = VMS_STRUCT,                                                    \
    .offset = 0,                                                            \
}
#endif /* !CONFIG_USER_ONLY */

#endif /* COMPILING_PER_TARGET */

#define UNASSIGNED_CPU_INDEX -1
#define UNASSIGNED_CLUSTER_INDEX -1

#endif
