
#ifndef BLOCK_RAM_REGISTRAR_H
#define BLOCK_RAM_REGISTRAR_H

#include "exec/ramlist.h"

typedef struct {
    BlockBackend *blk;
    RAMBlockNotifier notifier;
    bool ok;
} BlockRAMRegistrar;

void blk_ram_registrar_init(BlockRAMRegistrar *r, BlockBackend *blk);
void blk_ram_registrar_destroy(BlockRAMRegistrar *r);

static inline bool blk_ram_registrar_ok(BlockRAMRegistrar *r)
{
    return r->ok;
}

#endif /* BLOCK_RAM_REGISTRAR_H */
