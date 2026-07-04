
#ifndef GENERIC_LOADER_H
#define GENERIC_LOADER_H

#include "elf.h"
#include "hw/qdev-core.h"
#include "qom/object.h"

struct GenericLoaderState {
    DeviceState parent_obj;

    CPUState *cpu;

    uint64_t addr;
    uint64_t data;
    uint8_t data_len;
    uint32_t cpu_num;

    char *file;

    bool force_raw;
    bool data_be;
    bool set_pc;
};

#define TYPE_GENERIC_LOADER "loader"
OBJECT_DECLARE_SIMPLE_TYPE(GenericLoaderState, GENERIC_LOADER)

#endif
