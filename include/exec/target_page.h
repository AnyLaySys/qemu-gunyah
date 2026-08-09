
#ifndef EXEC_TARGET_PAGE_H
#define EXEC_TARGET_PAGE_H

#ifdef COMPILING_PER_TARGET
#include "cpu-param.h"
#include "exec/target_long.h"
#define TARGET_PAGE_TYPE  target_long
#else
#define TARGET_PAGE_BITS_VARY
#define TARGET_PAGE_TYPE  int
#endif

#ifdef TARGET_PAGE_BITS_VARY
# include "exec/page-vary.h"
extern const TargetPageBits target_page;
#  define TARGET_PAGE_BITS   target_page.bits
#  define TARGET_PAGE_MASK   ((TARGET_PAGE_TYPE)target_page.mask)
# define TARGET_PAGE_SIZE    (-(int)TARGET_PAGE_MASK)
#else
# define TARGET_PAGE_BITS_MIN TARGET_PAGE_BITS
# define TARGET_PAGE_SIZE    (1 << TARGET_PAGE_BITS)
# define TARGET_PAGE_MASK    ((TARGET_PAGE_TYPE)-1 << TARGET_PAGE_BITS)
#endif

#define TARGET_PAGE_ALIGN(addr) ROUND_UP((addr), TARGET_PAGE_SIZE)

static inline size_t qemu_target_page_size(void)
{
    return TARGET_PAGE_SIZE;
}

static inline int qemu_target_page_mask(void)
{
    return TARGET_PAGE_MASK;
}

static inline int qemu_target_page_bits(void)
{
    return TARGET_PAGE_BITS;
}

#endif
