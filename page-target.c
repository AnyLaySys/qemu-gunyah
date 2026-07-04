
#include "qemu/osdep.h"
#include "exec/target_page.h"

int qemu_target_page_bits_min(void)
{
    return TARGET_PAGE_BITS_MIN;
}

size_t qemu_target_pages_to_MiB(size_t pages)
{
    int page_bits = TARGET_PAGE_BITS;

    g_assert(page_bits < 20);

    return pages >> (20 - page_bits);
}
