
#ifndef EXEC_MEMOPIDX_H
#define EXEC_MEMOPIDX_H

#include "exec/memop.h"

typedef uint32_t MemOpIdx;

static inline MemOpIdx make_memop_idx(MemOp op, unsigned idx)
{
    return (op << 4) | idx;
}

static inline MemOp get_memop(MemOpIdx oi)
{
    return oi >> 4;
}

static inline unsigned get_mmuidx(MemOpIdx oi)
{
    return oi & 15;
}

#endif
