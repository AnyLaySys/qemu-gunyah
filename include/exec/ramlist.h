#ifndef RAMLIST_H
#define RAMLIST_H

#include "qemu/queue.h"
#include "qemu/thread.h"
#include "qemu/rcu.h"
#include "qemu/rcu_queue.h"

typedef struct RAMBlockNotifier RAMBlockNotifier;

#define DIRTY_MEMORY_VGA       0
#define DIRTY_MEMORY_CODE      1
#define DIRTY_MEMORY_MIGRATION 2
#define DIRTY_MEMORY_NUM       3        /* num of dirty bits */

#define DIRTY_MEMORY_BLOCK_SIZE ((ram_addr_t)256 * 1024 * 8)
typedef struct {
    struct rcu_head rcu;
    unsigned long *blocks[];
} DirtyMemoryBlocks;

typedef struct RAMList {
    QemuMutex mutex;
    RAMBlock *mru_block;
    QLIST_HEAD(, RAMBlock) blocks;
    DirtyMemoryBlocks *dirty_memory[DIRTY_MEMORY_NUM];
    unsigned int num_dirty_blocks;
    uint32_t version;
    QLIST_HEAD(, RAMBlockNotifier) ramblock_notifiers;
} RAMList;
extern RAMList ram_list;

#define  INTERNAL_RAMBLOCK_FOREACH(block)  \
    QLIST_FOREACH_RCU(block, &ram_list.blocks, next)
#define RAMBLOCK_FOREACH(block) INTERNAL_RAMBLOCK_FOREACH(block)

void qemu_mutex_lock_ramlist(void);
void qemu_mutex_unlock_ramlist(void);

struct RAMBlockNotifier {
    void (*ram_block_added)(RAMBlockNotifier *n, void *host, size_t size,
                            size_t max_size);
    void (*ram_block_removed)(RAMBlockNotifier *n, void *host, size_t size,
                              size_t max_size);
    void (*ram_block_resized)(RAMBlockNotifier *n, void *host, size_t old_size,
                              size_t new_size);
    QLIST_ENTRY(RAMBlockNotifier) next;
};

void ram_block_notifier_add(RAMBlockNotifier *n);
void ram_block_notifier_remove(RAMBlockNotifier *n);
void ram_block_notify_add(void *host, size_t size, size_t max_size);
void ram_block_notify_remove(void *host, size_t size, size_t max_size);
void ram_block_notify_resize(void *host, size_t old_size, size_t new_size);

GString *ram_block_format(void);

#endif /* RAMLIST_H */
