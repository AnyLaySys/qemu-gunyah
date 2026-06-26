
#ifndef REGISTER_H
#define REGISTER_H

#include "hw/qdev-core.h"
#include "exec/memory.h"
#include "hw/registerfields.h"
#include "qom/object.h"

typedef struct RegisterInfo RegisterInfo;
typedef struct RegisterAccessInfo RegisterAccessInfo;
typedef struct RegisterInfoArray RegisterInfoArray;


struct RegisterAccessInfo {
    const char *name;
    uint64_t ro;
    uint64_t w1c;
    uint64_t reset;
    uint64_t cor;
    uint64_t rsvd;
    uint64_t unimp;

    uint64_t (*pre_write)(RegisterInfo *reg, uint64_t val);
    void (*post_write)(RegisterInfo *reg, uint64_t val);

    uint64_t (*post_read)(RegisterInfo *reg, uint64_t val);

    hwaddr addr;
};


struct RegisterInfo {
    DeviceState parent_obj;

    void *data;
    int data_size;

    const RegisterAccessInfo *access;

    void *opaque;
};

#define TYPE_REGISTER "qemu-register"
DECLARE_INSTANCE_CHECKER(RegisterInfo, REGISTER,
                         TYPE_REGISTER)


struct RegisterInfoArray {
    MemoryRegion mem;

    int num_elements;
    RegisterInfo **r;

    bool debug;
    const char *prefix;
};


void register_write(RegisterInfo *reg, uint64_t val, uint64_t we,
                    const char *prefix, bool debug);


uint64_t register_read(RegisterInfo *reg, uint64_t re, const char* prefix,
                       bool debug);


void register_reset(RegisterInfo *reg);


void register_init(RegisterInfo *reg);


void register_write_memory(void *opaque, hwaddr addr, uint64_t value,
                           unsigned size);


uint64_t register_read_memory(void *opaque, hwaddr addr, unsigned size);


RegisterInfoArray *register_init_block8(DeviceState *owner,
                                        const RegisterAccessInfo *rae,
                                        int num, RegisterInfo *ri,
                                        uint8_t *data,
                                        const MemoryRegionOps *ops,
                                        bool debug_enabled,
                                        uint64_t memory_size);

RegisterInfoArray *register_init_block32(DeviceState *owner,
                                         const RegisterAccessInfo *rae,
                                         int num, RegisterInfo *ri,
                                         uint32_t *data,
                                         const MemoryRegionOps *ops,
                                         bool debug_enabled,
                                         uint64_t memory_size);

RegisterInfoArray *register_init_block64(DeviceState *owner,
                                         const RegisterAccessInfo *rae,
                                         int num, RegisterInfo *ri,
                                         uint64_t *data,
                                         const MemoryRegionOps *ops,
                                         bool debug_enabled,
                                         uint64_t memory_size);


void register_finalize_block(RegisterInfoArray *r_array);

#endif
