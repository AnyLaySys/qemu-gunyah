

#ifndef QEMU_EXEC_RAMBLOCK_H
#define QEMU_EXEC_RAMBLOCK_H

#ifndef CONFIG_USER_ONLY
#include "cpu-common.h"
#include "qemu/rcu.h"
#include "exec/ramlist.h"

struct RAMBlock {
    struct rcu_head rcu;
    struct MemoryRegion *mr;
    uint8_t *host;
    uint8_t *colo_cache; /* For colo, VM's ram cache */
    ram_addr_t offset;
    ram_addr_t used_length;
    ram_addr_t max_length;
    void (*resized)(const char*, uint64_t length, void *host);
    uint32_t flags;
    char idstr[256];
    QLIST_ENTRY(RAMBlock) next;
    QLIST_HEAD(, RAMBlockNotifier) ramblock_notifiers;
    int fd;
    uint64_t fd_offset;
    int guest_memfd;
    size_t page_size;
    unsigned long *bmap;

    unsigned long *file_bmap;
    off_t bitmap_offset;
    uint64_t pages_offset;

    unsigned long *receivedmap;

    unsigned long *clear_bmap;
    uint8_t clear_bmap_shift;

    ram_addr_t postcopy_length;
};
#endif
#endif
