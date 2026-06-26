
#include "qemu/osdep.h"
#include "qemu/range.h"
#include "qemu/reserved-region.h"

GList *resv_region_list_insert(GList *list, ReservedRegion *reg)
{
    ReservedRegion *resv_iter, *new_reg;
    Range *r = &reg->range;
    Range *range_iter;
    GList *l;

    for (l = list; l ; ) {
        resv_iter = (ReservedRegion *)l->data;
        range_iter = &resv_iter->range;

        if (range_compare(range_iter, r) < 0) {
            l = l->next;
        } else if (range_compare(range_iter, r) > 0) {
            return g_list_insert_before(list, l, reg);
        } else { /* there is an overlap */
            if (range_contains_range(r, range_iter)) {
                GList *prev = l->prev;
                g_free(l->data);
                list = g_list_delete_link(list, l);
                if (prev) {
                    l = prev->next;
                } else {
                    l = list;
                }
            } else if (range_contains_range(range_iter, r)) {
                if (range_lob(range_iter) == range_lob(r)) {
                    range_set_bounds(range_iter, range_upb(r) + 1,
                                     range_upb(range_iter));
                    return g_list_insert_before(list, l, reg);
                } else if (range_upb(range_iter) == range_upb(r)) {
                    range_set_bounds(range_iter, range_lob(range_iter),
                                     range_lob(r) - 1);
                    l = l->next;
                } else {
                    uint64_t lob = range_lob(range_iter);
                    range_set_bounds(range_iter, range_upb(r) + 1,
                                     range_upb(range_iter));
                    new_reg = g_new0(ReservedRegion, 1);
                    new_reg->type = resv_iter->type;
                    range_set_bounds(&new_reg->range,
                                     lob, range_lob(r) - 1);
                    list = g_list_insert_before(list, l, new_reg);
                    return g_list_insert_before(list, l, reg);
                }
            } else if (range_lob(r) < range_lob(range_iter)) {
                range_set_bounds(range_iter, range_upb(r) + 1,
                                 range_upb(range_iter));
                return g_list_insert_before(list, l, reg);
            } else { /* intersection on the upper range */
                range_set_bounds(range_iter, range_lob(range_iter),
                                 range_lob(r) - 1);
                l = l->next;
            }
        } /* overlap */
    }
    return g_list_append(list, reg);
}

