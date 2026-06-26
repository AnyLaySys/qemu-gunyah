
#ifndef QEMU_PMEM_H
#define QEMU_PMEM_H

#ifdef CONFIG_LIBPMEM
#include <libpmem.h>
#else  /* !CONFIG_LIBPMEM */

static inline void *
pmem_memcpy_persist(void *pmemdest, const void *src, size_t len)
{
    g_assert_not_reached();
}

static inline void
pmem_persist(const void *addr, size_t len)
{
    g_assert_not_reached();
}

#endif /* CONFIG_LIBPMEM */

#endif /* QEMU_PMEM_H */
