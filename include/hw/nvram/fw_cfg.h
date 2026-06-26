#ifndef FW_CFG_H
#define FW_CFG_H

#include "exec/hwaddr.h"
#include "standard-headers/linux/qemu_fw_cfg.h"
#include "hw/sysbus.h"
#include "system/dma.h"
#include "qom/object.h"

#define TYPE_FW_CFG     "fw_cfg"
#define TYPE_FW_CFG_IO  "fw_cfg_io"
#define TYPE_FW_CFG_MEM "fw_cfg_mem"
#define TYPE_FW_CFG_DATA_GENERATOR_INTERFACE "fw_cfg-data-generator"

OBJECT_DECLARE_SIMPLE_TYPE(FWCfgState, FW_CFG)
OBJECT_DECLARE_SIMPLE_TYPE(FWCfgIoState, FW_CFG_IO)
OBJECT_DECLARE_SIMPLE_TYPE(FWCfgMemState, FW_CFG_MEM)

typedef struct FWCfgDataGeneratorClass FWCfgDataGeneratorClass;
DECLARE_CLASS_CHECKERS(FWCfgDataGeneratorClass, FW_CFG_DATA_GENERATOR,
                       TYPE_FW_CFG_DATA_GENERATOR_INTERFACE)

struct FWCfgDataGeneratorClass {
    InterfaceClass parent_class;

    GByteArray *(*get_data)(Object *obj, Error **errp);
};

typedef struct fw_cfg_file FWCfgFile;

#define FW_CFG_ORDER_OVERRIDE_VGA    70
#define FW_CFG_ORDER_OVERRIDE_NIC    80
#define FW_CFG_ORDER_OVERRIDE_USER   100
#define FW_CFG_ORDER_OVERRIDE_DEVICE 110

void fw_cfg_set_order_override(FWCfgState *fw_cfg, int order);
void fw_cfg_reset_order_override(FWCfgState *fw_cfg);

typedef struct FWCfgFiles {
    uint32_t  count;
    FWCfgFile f[];
} FWCfgFiles;

typedef struct fw_cfg_dma_access FWCfgDmaAccess;

typedef void (*FWCfgCallback)(void *opaque);
typedef void (*FWCfgWriteCallback)(void *opaque, off_t start, size_t len);

typedef struct FWCfgEntry FWCfgEntry;

struct FWCfgState {
    SysBusDevice parent_obj;

    uint16_t file_slots;
    FWCfgEntry *entries[2];
    int *entry_order;
    FWCfgFiles *files;
    uint16_t cur_entry;
    uint32_t cur_offset;
    Notifier machine_ready;

    int fw_cfg_order_override;

    bool dma_enabled;
    dma_addr_t dma_addr;
    AddressSpace *dma_as;
    MemoryRegion dma_iomem;

    bool acpi_mr_restore;
    uint64_t table_mr_size;
    uint64_t linker_mr_size;
    uint64_t rsdp_mr_size;
};

struct FWCfgIoState {
    FWCfgState parent_obj;

    MemoryRegion comb_iomem;
};

struct FWCfgMemState {
    FWCfgState parent_obj;

    MemoryRegion ctl_iomem, data_iomem;
    uint32_t data_width;
    MemoryRegionOps wide_data_ops;
};

void fw_cfg_add_bytes(FWCfgState *s, uint16_t key, void *data, size_t len);

void fw_cfg_add_string(FWCfgState *s, uint16_t key, const char *value);

void fw_cfg_modify_string(FWCfgState *s, uint16_t key, const char *value);

void fw_cfg_add_i16(FWCfgState *s, uint16_t key, uint16_t value);

void fw_cfg_modify_i16(FWCfgState *s, uint16_t key, uint16_t value);

void fw_cfg_add_i32(FWCfgState *s, uint16_t key, uint32_t value);

void fw_cfg_modify_i32(FWCfgState *s, uint16_t key, uint32_t value);

void fw_cfg_add_i64(FWCfgState *s, uint16_t key, uint64_t value);

void fw_cfg_modify_i64(FWCfgState *s, uint16_t key, uint64_t value);

void fw_cfg_add_file(FWCfgState *s, const char *filename, void *data,
                     size_t len);

void fw_cfg_add_file_callback(FWCfgState *s, const char *filename,
                              FWCfgCallback select_cb,
                              FWCfgWriteCallback write_cb,
                              void *callback_opaque,
                              void *data, size_t len, bool read_only);

void *fw_cfg_modify_file(FWCfgState *s, const char *filename, void *data,
                         size_t len);

bool fw_cfg_add_file_from_generator(FWCfgState *s,
                                    Object *parent, const char *part,
                                    const char *filename, Error **errp);

FWCfgState *fw_cfg_init_io_dma(uint32_t iobase, uint32_t dma_iobase,
                                AddressSpace *dma_as);
FWCfgState *fw_cfg_init_mem(hwaddr ctl_addr, hwaddr data_addr);
FWCfgState *fw_cfg_init_mem_wide(hwaddr ctl_addr,
                                 hwaddr data_addr, uint32_t data_width,
                                 hwaddr dma_addr, AddressSpace *dma_as);

FWCfgState *fw_cfg_find(void);
bool fw_cfg_dma_enabled(void *opaque);

const char *fw_cfg_arch_key_name(uint16_t key);

void load_image_to_fw_cfg(FWCfgState *fw_cfg, uint16_t size_key,
                          uint16_t data_key, const char *image_name,
                          bool try_decompress);

#endif
