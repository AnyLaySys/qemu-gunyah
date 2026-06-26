
#ifndef MEMORY_MAPPING_H
#define MEMORY_MAPPING_H

#include "qemu/queue.h"
#include "exec/cpu-common.h"

typedef struct GuestPhysBlock {
    hwaddr target_start;

    hwaddr target_end;

    uint8_t *host_addr;

    MemoryRegion *mr;

    QTAILQ_ENTRY(GuestPhysBlock) next;
} GuestPhysBlock;

typedef struct GuestPhysBlockList {
    unsigned num;
    QTAILQ_HEAD(, GuestPhysBlock) head;
} GuestPhysBlockList;

typedef struct MemoryMapping {
    hwaddr phys_addr;
    vaddr virt_addr;
    ram_addr_t length;
    QTAILQ_ENTRY(MemoryMapping) next;
} MemoryMapping;

struct MemoryMappingList {
    unsigned int num;
    MemoryMapping *last_mapping;
    QTAILQ_HEAD(, MemoryMapping) head;
};

void memory_mapping_list_add_merge_sorted(MemoryMappingList *list,
                                          hwaddr phys_addr,
                                          hwaddr virt_addr,
                                          ram_addr_t length);

void memory_mapping_list_free(MemoryMappingList *list);

void memory_mapping_list_init(MemoryMappingList *list);

void guest_phys_blocks_free(GuestPhysBlockList *list);
void guest_phys_blocks_init(GuestPhysBlockList *list);
void guest_phys_blocks_append(GuestPhysBlockList *list);

bool qemu_get_guest_memory_mapping(MemoryMappingList *list,
                                   const GuestPhysBlockList *guest_phys_blocks,
                                   Error **errp);

void qemu_get_guest_simple_memory_mapping(MemoryMappingList *list,
                                  const GuestPhysBlockList *guest_phys_blocks);

void memory_mapping_filter(MemoryMappingList *list, int64_t begin,
                           int64_t length);

#endif
