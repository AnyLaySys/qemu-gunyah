

#ifndef MEMORY_INTERNAL_H
#define MEMORY_INTERNAL_H

#include "cpu.h"

#ifndef CONFIG_USER_ONLY
static inline AddressSpaceDispatch *flatview_to_dispatch(FlatView *fv)
{
    return fv->dispatch;
}

static inline AddressSpaceDispatch *address_space_to_dispatch(AddressSpace *as)
{
    return flatview_to_dispatch(address_space_to_flatview(as));
}

FlatView *address_space_get_flatview(AddressSpace *as);
void flatview_unref(FlatView *view);

extern const MemoryRegionOps unassigned_mem_ops;

void flatview_add_to_dispatch(FlatView *fv, MemoryRegionSection *section);
AddressSpaceDispatch *address_space_dispatch_new(FlatView *fv);
void address_space_dispatch_compact(AddressSpaceDispatch *d);
void address_space_dispatch_free(AddressSpaceDispatch *d);

void mtree_print_dispatch(struct AddressSpaceDispatch *d,
                          MemoryRegion *root);
#endif
#endif
