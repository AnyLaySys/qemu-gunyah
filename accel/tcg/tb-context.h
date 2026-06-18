


















#ifndef QEMU_TB_CONTEXT_H
#define QEMU_TB_CONTEXT_H

#include "qemu/thread.h"
#include "qemu/qht.h"

#define CODE_GEN_HTABLE_BITS     15
#define CODE_GEN_HTABLE_SIZE     (1 << CODE_GEN_HTABLE_BITS)

typedef struct TBContext TBContext;

struct TBContext {

    struct qht htable;


    unsigned tb_flush_count;
    unsigned tb_phys_invalidate_count;
};

extern TBContext tb_ctx;

#endif
