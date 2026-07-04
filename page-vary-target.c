
#define IN_PAGE_VARY 1

#include "qemu/osdep.h"
#include "exec/page-vary.h"
#include "exec/exec-all.h"

bool set_preferred_target_page_bits(int bits)
{
#ifdef TARGET_PAGE_BITS_VARY
    assert(bits >= TARGET_PAGE_BITS_MIN);
    return set_preferred_target_page_bits_common(bits);
#else
    return true;
#endif
}

void finalize_target_page_bits(void)
{
    finalize_target_page_bits_common(TARGET_PAGE_BITS_MIN);
}
