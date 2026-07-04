#ifndef USER_PAGE_PROTECTION_H
#define USER_PAGE_PROTECTION_H

#ifndef CONFIG_USER_ONLY
#error Cannot include this header from system emulation
#endif

#include "cpu-param.h"
#include "exec/target_long.h"
#include "exec/translation-block.h"

void page_protect(tb_page_addr_t page_addr);
int page_unprotect(tb_page_addr_t address, uintptr_t pc);

int page_get_flags(target_ulong address);

void page_set_flags(target_ulong start, target_ulong last, int flags);

void page_reset_target_data(target_ulong start, target_ulong last);

bool page_check_range(target_ulong start, target_ulong last, int flags);

bool page_check_range_empty(target_ulong start, target_ulong last);

target_ulong page_find_range_empty(target_ulong min, target_ulong max,
                                   target_ulong len, target_ulong align);

__attribute__((returns_nonnull))
void *page_get_target_data(target_ulong address);

typedef int (*walk_memory_regions_fn)(void *, target_ulong,
                                      target_ulong, unsigned long);

int walk_memory_regions(void *, walk_memory_regions_fn);

void page_dump(FILE *f);

#endif
