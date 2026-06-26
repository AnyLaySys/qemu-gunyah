
#ifndef USER_GUEST_HOST_H
#define USER_GUEST_HOST_H

#include "user/abitypes.h"
#include "user/guest-base.h"
#include "cpu.h"

extern unsigned long reserved_va;

#define GUEST_ADDR_MAX_                                                 \
    ((MIN_CONST(TARGET_VIRT_ADDR_SPACE_BITS, TARGET_ABI_BITS) <= 32) ?  \
     UINT32_MAX : ~0ul)
#define GUEST_ADDR_MAX    (reserved_va ? : GUEST_ADDR_MAX_)

#ifndef TARGET_TAGGED_ADDRESSES
static inline abi_ptr cpu_untagged_addr(CPUState *cs, abi_ptr x)
{
    return x;
}
#endif

static inline void *g2h_untagged(abi_ptr x)
{
    return (void *)((uintptr_t)(x) + guest_base);
}

static inline void *g2h(CPUState *cs, abi_ptr x)
{
    return g2h_untagged(cpu_untagged_addr(cs, x));
}

static inline bool guest_addr_valid_untagged(abi_ulong x)
{
    return x <= GUEST_ADDR_MAX;
}

static inline bool guest_range_valid_untagged(abi_ulong start, abi_ulong len)
{
    return len - 1 <= GUEST_ADDR_MAX && start <= GUEST_ADDR_MAX - len + 1;
}

#define h2g_valid(x) \
    (HOST_LONG_BITS <= TARGET_VIRT_ADDR_SPACE_BITS || \
     (uintptr_t)(x) - guest_base <= GUEST_ADDR_MAX)

#define h2g_nocheck(x) ({ \
    uintptr_t __ret = (uintptr_t)(x) - guest_base; \
    (abi_ptr)__ret; \
})

#define h2g(x) ({ \
    /* Check if given address fits target address space */ \
    assert(h2g_valid(x)); \
    h2g_nocheck(x); \
})

#endif
