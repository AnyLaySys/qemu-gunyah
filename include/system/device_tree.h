
#ifndef DEVICE_TREE_H
#define DEVICE_TREE_H

void *create_device_tree(int *sizep);
void *load_device_tree(const char *filename_path, int *sizep);
#ifdef CONFIG_LINUX
void *load_device_tree_from_sysfs(void);
#endif

char **qemu_fdt_node_path(void *fdt, const char *name, const char *compat,
                          Error **errp);

char **qemu_fdt_node_unit_path(void *fdt, const char *name, Error **errp);

int qemu_fdt_setprop(void *fdt, const char *node_path,
                     const char *property, const void *val, int size);
int qemu_fdt_setprop_cell(void *fdt, const char *node_path,
                          const char *property, uint32_t val);
int qemu_fdt_setprop_u64(void *fdt, const char *node_path,
                         const char *property, uint64_t val);
int qemu_fdt_setprop_string(void *fdt, const char *node_path,
                            const char *property, const char *string);

int qemu_fdt_setprop_string_array(void *fdt, const char *node_path,
                                  const char *prop, char **array, int len);

int qemu_fdt_setprop_phandle(void *fdt, const char *node_path,
                             const char *property,
                             const char *target_node_path);
const void *qemu_fdt_getprop(void *fdt, const char *node_path,
                             const char *property, int *lenp,
                             Error **errp);
uint32_t qemu_fdt_getprop_cell(void *fdt, const char *node_path,
                               const char *property, int *lenp,
                               Error **errp);
uint32_t qemu_fdt_get_phandle(void *fdt, const char *path);
uint32_t qemu_fdt_alloc_phandle(void *fdt);
int qemu_fdt_nop_node(void *fdt, const char *node_path);
int qemu_fdt_add_subnode(void *fdt, const char *name);
int qemu_fdt_add_path(void *fdt, const char *path);

#define qemu_fdt_setprop_cells(fdt, node_path, property, ...)                 \
    do {                                                                      \
        uint32_t qdt_tmp[] = { __VA_ARGS__ };                                 \
        for (unsigned i_ = 0; i_ < ARRAY_SIZE(qdt_tmp); i_++) {               \
            qdt_tmp[i_] = cpu_to_be32(qdt_tmp[i_]);                           \
        }                                                                     \
        qemu_fdt_setprop(fdt, node_path, property, qdt_tmp,                   \
                         sizeof(qdt_tmp));                                    \
    } while (0)

int qemu_fdt_setprop_sized_cells_from_array(void *fdt,
                                            const char *node_path,
                                            const char *property,
                                            int numvalues,
                                            uint64_t *values);

#define qemu_fdt_setprop_sized_cells(fdt, node_path, property, ...)       \
    ({                                                                    \
        uint64_t qdt_tmp[] = { __VA_ARGS__ };                             \
        qemu_fdt_setprop_sized_cells_from_array(fdt, node_path,           \
                                                property,                 \
                                                ARRAY_SIZE(qdt_tmp) / 2,  \
                                                qdt_tmp);                 \
    })


void qemu_fdt_randomize_seeds(void *fdt);

#define FDT_PCI_RANGE_RELOCATABLE          0x80000000
#define FDT_PCI_RANGE_PREFETCHABLE         0x40000000
#define FDT_PCI_RANGE_ALIASED              0x20000000
#define FDT_PCI_RANGE_TYPE_MASK            0x03000000
#define FDT_PCI_RANGE_MMIO_64BIT           0x03000000
#define FDT_PCI_RANGE_MMIO                 0x02000000
#define FDT_PCI_RANGE_IOPORT               0x01000000
#define FDT_PCI_RANGE_CONFIG               0x00000000

#endif /* DEVICE_TREE_H */
