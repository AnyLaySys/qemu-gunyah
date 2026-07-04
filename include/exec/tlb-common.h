#ifndef EXEC_TLB_COMMON_H
#define EXEC_TLB_COMMON_H 1

#define CPU_TLB_ENTRY_BITS (HOST_LONG_BITS == 32 ? 4 : 5)

typedef union CPUTLBEntry {
    struct {
        uintptr_t addr_read;
        uintptr_t addr_write;
        uintptr_t addr_code;
        uintptr_t addend;
    };
    uintptr_t addr_idx[(1 << CPU_TLB_ENTRY_BITS) / sizeof(uintptr_t)];
} CPUTLBEntry;

QEMU_BUILD_BUG_ON(sizeof(CPUTLBEntry) != (1 << CPU_TLB_ENTRY_BITS));

typedef struct CPUTLBDescFast {
    uintptr_t mask;
    CPUTLBEntry *table;
} CPUTLBDescFast QEMU_ALIGNED(2 * sizeof(void *));

#endif /* EXEC_TLB_COMMON_H */
