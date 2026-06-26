
#ifndef HW_LOADER_FIT_H
#define HW_LOADER_FIT_H

#include "exec/hwaddr.h"

struct fit_loader_match {
    const char *compatible;
    const void *data;
};

struct fit_loader {
    const struct fit_loader_match *matches;
    hwaddr (*addr_to_phys)(void *opaque, uint64_t addr);
    void *(*fdt_filter)(void *opaque, const void *fdt,
                        const void *match_data, hwaddr *load_addr);
    const void *(*kernel_filter)(void *opaque, const void *kernel,
                                 hwaddr *load_addr, hwaddr *entry_addr);
};

int load_fit(const struct fit_loader *ldr, const char *filename, void **pfdt,
             void *opaque);

#endif /* HW_LOADER_FIT_H */
