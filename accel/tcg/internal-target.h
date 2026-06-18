







#ifndef ACCEL_TCG_INTERNAL_TARGET_H
#define ACCEL_TCG_INTERNAL_TARGET_H

#include "exec/exec-all.h"
#include "exec/translation-block.h"
#include "tb-internal.h"
#include "tcg-target-mo.h"







#ifdef CONFIG_USER_ONLY
#define assert_memory_lock() tcg_debug_assert(have_mmap_lock())
#else
#define assert_memory_lock()
#endif

#if defined(CONFIG_SOFTMMU) && defined(CONFIG_DEBUG_TCG)
void assert_no_pages_locked(void);
#else
static inline void assert_no_pages_locked(void) { }
#endif

#ifdef CONFIG_USER_ONLY
static inline void page_table_config_init(void) { }
#else
void page_table_config_init(void);
#endif

#ifndef CONFIG_USER_ONLY
G_NORETURN void cpu_io_recompile(CPUState *cpu, uintptr_t retaddr);
#endif














#ifdef TCG_GUEST_DEFAULT_MO
# define tcg_req_mo(type) \
    ((type) & TCG_GUEST_DEFAULT_MO & ~TCG_TARGET_DEFAULT_MO)
#else
# define tcg_req_mo(type) ((type) & ~TCG_TARGET_DEFAULT_MO)
#endif








#define cpu_req_mo(type)          \
    do {                          \
        if (tcg_req_mo(type)) {   \
            smp_mb();             \
        }                         \
    } while (0)

#endif
