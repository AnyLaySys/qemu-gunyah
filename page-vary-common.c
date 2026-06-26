
#define IN_PAGE_VARY 1

#include "qemu/osdep.h"
#include "exec/page-vary.h"


TargetPageBits target_page;

bool set_preferred_target_page_bits_common(int bits)
{
    if (target_page.bits == 0 || target_page.bits > bits) {
        if (target_page.decided) {
            return false;
        }
        target_page.bits = bits;
    }
    return true;
}

void finalize_target_page_bits_common(int min)
{
    if (target_page.bits == 0) {
        target_page.bits = min;
    }
    target_page.mask = -1ull << target_page.bits;
    target_page.decided = true;
}
