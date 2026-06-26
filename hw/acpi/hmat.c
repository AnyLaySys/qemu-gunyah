
#include "qemu/osdep.h"
#include "qemu/units.h"
#include "system/numa.h"
#include "hw/acpi/aml-build.h"
#include "hw/acpi/hmat.h"

static void build_hmat_mpda(GArray *table_data, uint16_t flags,
                            uint32_t initiator, uint32_t mem_node)
{

    build_append_int_noprefix(table_data, 0, 2);
    build_append_int_noprefix(table_data, 0, 2);
    build_append_int_noprefix(table_data, 40, 4);
    build_append_int_noprefix(table_data, flags, 2);
    build_append_int_noprefix(table_data, 0, 2);
    build_append_int_noprefix(table_data, initiator, 4);
    build_append_int_noprefix(table_data, mem_node, 4);
    build_append_int_noprefix(table_data, 0, 4);
    build_append_int_noprefix(table_data, 0, 8);
    build_append_int_noprefix(table_data, 0, 8);
}

static void build_hmat_lb(GArray *table_data, HMAT_LB_Info *hmat_lb,
                          uint32_t num_initiator, uint32_t num_target,
                          uint32_t *initiator_list)
{
    int i, index;
    uint32_t initiator_to_index[MAX_NODES] = {};
    HMAT_LB_Data *lb_data;
    uint16_t *entry_list;
    uint32_t base;
    uint32_t lb_length
        = 32 /* Table length up to and including Entry Base Unit */
        + 4 * num_initiator /* Initiator Proximity Domain List */
        + 4 * num_target /* Target Proximity Domain List */
        + 2 * num_initiator * num_target; /* Latency or Bandwidth Entries */

    build_append_int_noprefix(table_data, 1, 2);
    build_append_int_noprefix(table_data, 0, 2);
    build_append_int_noprefix(table_data, lb_length, 4);
    assert(!(hmat_lb->hierarchy >> 4));
    build_append_int_noprefix(table_data, hmat_lb->hierarchy, 1);
    build_append_int_noprefix(table_data, hmat_lb->data_type, 1);
    build_append_int_noprefix(table_data, 0, 2);
    build_append_int_noprefix(table_data, num_initiator, 4);
    build_append_int_noprefix(table_data, num_target, 4);
    build_append_int_noprefix(table_data, 0, 4);

    if (hmat_lb->data_type <= HMAT_LB_DATA_WRITE_LATENCY) {
        base = hmat_lb->base * 1000;
    } else {
        base = hmat_lb->base / MiB;
    }
    build_append_int_noprefix(table_data, base, 8);

    for (i = 0; i < num_initiator; i++) {
        build_append_int_noprefix(table_data, initiator_list[i], 4);
        initiator_to_index[initiator_list[i]] = i;
    }

    for (i = 0; i < num_target; i++) {
        build_append_int_noprefix(table_data, i, 4);
    }

    entry_list = g_new0(uint16_t, num_initiator * num_target);
    for (i = 0; i < hmat_lb->list->len; i++) {
        lb_data = &g_array_index(hmat_lb->list, HMAT_LB_Data, i);
        index = initiator_to_index[lb_data->initiator] * num_target +
            lb_data->target;

        entry_list[index] = (uint16_t)(lb_data->data / hmat_lb->base);
    }

    for (i = 0; i < num_initiator * num_target; i++) {
        build_append_int_noprefix(table_data, entry_list[i], 2);
    }

    g_free(entry_list);
}

static void build_hmat_cache(GArray *table_data, uint8_t total_levels,
                             NumaHmatCacheOptions *hmat_cache)
{
    uint32_t cache_attr = total_levels;

    cache_attr |= (uint32_t) hmat_cache->level << 4;

    cache_attr |= (uint32_t) hmat_cache->associativity << 8;

    cache_attr |= (uint32_t) hmat_cache->policy << 12;

    cache_attr |= (uint32_t) hmat_cache->line << 16;

    build_append_int_noprefix(table_data, 2, 2);
    build_append_int_noprefix(table_data, 0, 2);
    build_append_int_noprefix(table_data, 32, 4);
    build_append_int_noprefix(table_data, hmat_cache->node_id, 4);
    build_append_int_noprefix(table_data, 0, 4);
    build_append_int_noprefix(table_data, hmat_cache->size, 8);
    build_append_int_noprefix(table_data, cache_attr, 4);
    build_append_int_noprefix(table_data, 0, 2);
    build_append_int_noprefix(table_data, 0, 2);
}

static void hmat_build_table_structs(GArray *table_data, NumaState *numa_state)
{
    uint16_t flags;
    uint32_t num_initiator = 0;
    uint32_t initiator_list[MAX_NODES];
    int i, hierarchy, type, cache_level, total_levels;
    HMAT_LB_Info *hmat_lb;
    NumaHmatCacheOptions *hmat_cache;

    build_append_int_noprefix(table_data, 0, 4); /* Reserved */

    for (i = 0; i < numa_state->num_nodes; i++) {
        if (!numa_state->nodes[i].node_mem) {
            continue;
        }
        flags = 0;

        if (numa_state->nodes[i].initiator < MAX_NODES) {
            flags |= HMAT_PROXIMITY_INITIATOR_VALID;
        }

        build_hmat_mpda(table_data, flags, numa_state->nodes[i].initiator, i);
    }

    for (i = 0; i < numa_state->num_nodes; i++) {
        if (numa_state->nodes[i].has_cpu || numa_state->nodes[i].has_gi) {
            initiator_list[num_initiator++] = i;
        }
    }

    for (hierarchy = HMAT_LB_MEM_MEMORY;
         hierarchy <= HMAT_LB_MEM_CACHE_3RD_LEVEL; hierarchy++) {
        for (type = HMAT_LB_DATA_ACCESS_LATENCY;
             type <= HMAT_LB_DATA_WRITE_BANDWIDTH; type++) {
            hmat_lb = numa_state->hmat_lb[hierarchy][type];

            if (hmat_lb && hmat_lb->list->len) {
                build_hmat_lb(table_data, hmat_lb, num_initiator,
                              numa_state->num_nodes, initiator_list);
            }
        }
    }

    for (i = 0; i < numa_state->num_nodes; i++) {
        total_levels = 0;
        for (cache_level = 1; cache_level < HMAT_LB_LEVELS; cache_level++) {
            if (numa_state->hmat_cache[i][cache_level]) {
                total_levels++;
            }
        }
        for (cache_level = 0; cache_level <= total_levels; cache_level++) {
            hmat_cache = numa_state->hmat_cache[i][cache_level];
            if (hmat_cache) {
                build_hmat_cache(table_data, total_levels, hmat_cache);
            }
        }
    }
}

void build_hmat(GArray *table_data, BIOSLinker *linker, NumaState *numa_state,
                const char *oem_id, const char *oem_table_id)
{
    AcpiTable table = { .sig = "HMAT", .rev = 2,
                        .oem_id = oem_id, .oem_table_id = oem_table_id };

    acpi_table_begin(&table, table_data);
    hmat_build_table_structs(table_data, numa_state);
    acpi_table_end(linker, &table);
}
