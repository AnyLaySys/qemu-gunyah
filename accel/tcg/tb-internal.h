







#ifndef ACCEL_TCG_TB_INTERNAL_TARGET_H
#define ACCEL_TCG_TB_INTERNAL_TARGET_H

#include "exec/cpu-all.h"
#include "exec/exec-all.h"
#include "exec/translation-block.h"










#define GETPC_ADJ   2

#ifdef CONFIG_SOFTMMU

#define CPU_TLB_DYN_MIN_BITS 6
#define CPU_TLB_DYN_DEFAULT_BITS 8

# if HOST_LONG_BITS == 32

#  define CPU_TLB_DYN_MAX_BITS (32 - TARGET_PAGE_BITS)
# else







#  ifdef TARGET_PAGE_BITS_VARY
#   define CPU_TLB_DYN_MAX_BITS                                  \
    MIN(22, TARGET_VIRT_ADDR_SPACE_BITS - TARGET_PAGE_BITS)
#  else
#   define CPU_TLB_DYN_MAX_BITS                                  \
    MIN_CONST(22, TARGET_VIRT_ADDR_SPACE_BITS - TARGET_PAGE_BITS)
#  endif
# endif

#endif

#ifdef CONFIG_USER_ONLY
#include "user/page-protection.h"






static inline void tb_lock_page0(tb_page_addr_t p0)
{
    page_protect(p0);
}

static inline void tb_lock_page1(tb_page_addr_t p0, tb_page_addr_t p1)
{
    page_protect(p1);
}

static inline void tb_unlock_page1(tb_page_addr_t p0, tb_page_addr_t p1) { }
static inline void tb_unlock_pages(TranslationBlock *tb) { }
#else
void tb_lock_page0(tb_page_addr_t);
void tb_lock_page1(tb_page_addr_t, tb_page_addr_t);
void tb_unlock_page1(tb_page_addr_t, tb_page_addr_t);
void tb_unlock_pages(TranslationBlock *);
#endif

#ifdef CONFIG_SOFTMMU
void tb_invalidate_phys_range_fast(ram_addr_t ram_addr,
                                   unsigned size,
                                   uintptr_t retaddr);
#endif

bool tb_invalidate_phys_page_unwind(tb_page_addr_t addr, uintptr_t pc);

#endif
