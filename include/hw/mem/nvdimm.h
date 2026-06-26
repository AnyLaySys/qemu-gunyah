
#ifndef QEMU_NVDIMM_H
#define QEMU_NVDIMM_H

#include "hw/mem/pc-dimm.h"
#include "hw/acpi/bios-linker-loader.h"
#include "qemu/uuid.h"
#include "hw/acpi/aml-build.h"
#include "qom/object.h"

#define MIN_NAMESPACE_LABEL_SIZE      (128UL << 10)

#define TYPE_NVDIMM      "nvdimm"
OBJECT_DECLARE_TYPE(NVDIMMDevice, NVDIMMClass, NVDIMM)

#define NVDIMM_LABEL_SIZE_PROP "label-size"
#define NVDIMM_UUID_PROP       "uuid"
#define NVDIMM_UNARMED_PROP    "unarmed"

struct NVDIMMDevice {
    PCDIMMDevice parent_obj;


    uint64_t label_size;

    void *label_data;

    MemoryRegion *nvdimm_mr;

    bool unarmed;

    bool readonly;

    QemuUUID uuid;
};

struct NVDIMMClass {
    PCDIMMDeviceClass parent_class;


    void (*read_label_data)(NVDIMMDevice *nvdimm, void *buf,
                            uint64_t size, uint64_t offset);
    void (*write_label_data)(NVDIMMDevice *nvdimm, const void *buf,
                             uint64_t size, uint64_t offset);
    void (*realize)(NVDIMMDevice *nvdimm, Error **errp);
    void (*unrealize)(NVDIMMDevice *nvdimm);
};

#define NVDIMM_DSM_MEM_FILE     "etc/acpi/nvdimm-mem"

#define NVDIMM_ACPI_IO_BASE     0x0a18
#define NVDIMM_ACPI_IO_LEN      4

struct NvdimmFitBuffer {
    GArray *fit;
    bool dirty;
};
typedef struct NvdimmFitBuffer NvdimmFitBuffer;

struct NVDIMMState {
    bool is_enabled;

    GArray *dsm_mem;

    NvdimmFitBuffer fit_buf;

    MemoryRegion io_mr;

    int32_t persistence;
    char    *persistence_string;
    struct AcpiGenericAddress dsm_io;
};
typedef struct NVDIMMState NVDIMMState;

void nvdimm_init_acpi_state(NVDIMMState *state, MemoryRegion *io,
                            struct AcpiGenericAddress dsm_io,
                            FWCfgState *fw_cfg, Object *owner);
void nvdimm_build_srat(GArray *table_data);
void nvdimm_build_acpi(GArray *table_offsets, GArray *table_data,
                       BIOSLinker *linker, NVDIMMState *state,
                       uint32_t ram_slots, const char *oem_id,
                       const char *oem_table_id);
void nvdimm_plug(NVDIMMState *state);
void nvdimm_acpi_plug_cb(HotplugHandler *hotplug_dev, DeviceState *dev);
#endif
