
#ifndef GUNYAH_INT_H
#define GUNYAH_INT_H

#include "qemu/accel.h"
#include "qemu/typedefs.h"
#include "qemu/thread.h"
#include "system/gunyah_report.h"

typedef struct gunyah_slot {
    uint64_t start;
    uint64_t size;
    uint8_t *mem;
    uint32_t id;
    uint32_t flags;
    bool lend;
} gunyah_slot;
#define GUNYAH_MAX_MEM_SLOTS    2048
struct GUNYAHState {
    AccelState parent_obj;
    QemuMutex slots_lock;
    gunyah_slot slots[GUNYAH_MAX_MEM_SLOTS];
    uint32_t nr_slots;
    int fd;
    int vmfd;
    uint64_t swiotlb_size;
    bool preshmem_reserved;
    uint32_t vm_started;
    uint64_t dtb_start;
    uint64_t dtb_size;
    bool protected_vm;
    uint64_t kernel_entry;
    uint32_t msi_vectors;
};
struct AccelCPUState {
    int fd;
    struct gh_vcpu_run *run;
    size_t run_size;
    uint64_t last_fault_addr;
    int same_fault_count;
};

extern bool gunyah_vm_stopped;

int gunyah_create_vm(void);

void gunyah_start_vm(void);

int gunyah_vm_ioctl(int type, ...);

void *gunyah_cpu_thread_fn(void *arg);

GUNYAHState *get_gunyah_state(void);

int gunyah_arch_put_registers(CPUState *cs, int level);

void gunyah_cpu_synchronize_post_reset(CPUState *cpu);
#endif
