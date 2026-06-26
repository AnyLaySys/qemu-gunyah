

#include "qemu/osdep.h"
#include "system/dma.h"
#include "system/reset.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "hw/qdev-properties.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "hw/core/generic-loader.h"

#define CPU_NONE 0xFFFFFFFF

static void generic_loader_reset(void *opaque)
{
    GenericLoaderState *s = GENERIC_LOADER(opaque);

    if (s->set_pc) {
        cpu_reset(s->cpu);
        cpu_set_pc(s->cpu, s->addr);
    }

    if (s->data_len) {
        assert(s->data_len <= sizeof(s->data));
        dma_memory_write(s->cpu->as, s->addr, &s->data, s->data_len,
                         MEMTXATTRS_UNSPECIFIED);
    }
}

static void generic_loader_realize(DeviceState *dev, Error **errp)
{
    GenericLoaderState *s = GENERIC_LOADER(dev);
    hwaddr entry;
    ssize_t size = 0;

    s->set_pc = false;

    if (s->data || s->data_len  || s->data_be) {
        if (s->file) {
            error_setg(errp, "Specifying a file is not supported when loading "
                       "memory values");
            return;
        } else if (s->force_raw) {
            error_setg(errp, "Specifying force-raw is not supported when "
                       "loading memory values");
            return;
        } else if (!s->data_len) {
            error_setg(errp, "Both data and data-len must be specified");
            return;
        } else if (s->data_len > 8) {
            error_setg(errp, "data-len cannot be greater then 8 bytes");
            return;
        }
    } else if (s->file || s->force_raw)  {
        if (s->data || s->data_len || s->data_be) {
            error_setg(errp, "data can not be specified when loading an "
                       "image");
            return;
        }
        if (s->cpu_num != CPU_NONE) {
            s->set_pc = true;
        }
    } else if (s->addr) {
        if (s->data || s->data_len || s->data_be) {
            error_setg(errp, "data can not be specified when setting a "
                       "program counter");
            return;
        } else if (s->cpu_num == CPU_NONE) {
            error_setg(errp, "cpu_num must be specified when setting a "
                       "program counter");
            return;
        }
        s->set_pc = true;
    } else {
        error_setg(errp, "please include valid arguments");
        return;
    }

    qemu_register_reset(generic_loader_reset, dev);

    if (s->cpu_num != CPU_NONE) {
        s->cpu = qemu_get_cpu(s->cpu_num);
        if (!s->cpu) {
            error_setg(errp, "Specified boot CPU#%d is nonexistent",
                       s->cpu_num);
            return;
        }
    } else {
        s->cpu = first_cpu;
    }

    if (s->file) {
        AddressSpace *as = s->cpu ? s->cpu->as :  NULL;

        if (!s->force_raw) {
            size = load_elf_as(s->file, NULL, NULL, NULL, &entry, NULL, NULL,
                               NULL, ELFDATANONE, 0, 0, 0, as);

            if (size < 0) {
                size = load_uimage_as(s->file, &entry, NULL, NULL, NULL, NULL,
                                      as);
            }

            if (size < 0) {
                size = load_targphys_hex_as(s->file, &entry, as);
            }
        }

        if (size < 0 || s->force_raw) {
            size = load_image_targphys_as(s->file, s->addr, current_machine->ram_size, as);
        } else {
            s->addr = entry;
        }

        if (size < 0) {
            error_setg(errp, "Cannot load specified image %s", s->file);
            return;
        }
    }

    if (s->data_be) {
        s->data = cpu_to_be64(s->data);
    } else {
        s->data = cpu_to_le64(s->data);
    }
}

static void generic_loader_unrealize(DeviceState *dev)
{
    qemu_unregister_reset(generic_loader_reset, dev);
}

static const Property generic_loader_props[] = {
    DEFINE_PROP_UINT64("addr", GenericLoaderState, addr, 0),
    DEFINE_PROP_UINT64("data", GenericLoaderState, data, 0),
    DEFINE_PROP_UINT8("data-len", GenericLoaderState, data_len, 0),
    DEFINE_PROP_BOOL("data-be", GenericLoaderState, data_be, false),
    DEFINE_PROP_UINT32("cpu-num", GenericLoaderState, cpu_num, CPU_NONE),
    DEFINE_PROP_BOOL("force-raw", GenericLoaderState, force_raw, false),
    DEFINE_PROP_STRING("file", GenericLoaderState, file),
};

static void generic_loader_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = generic_loader_realize;
    dc->unrealize = generic_loader_unrealize;
    device_class_set_props(dc, generic_loader_props);
    dc->desc = "Generic Loader";
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static const TypeInfo generic_loader_info = {
    .name = TYPE_GENERIC_LOADER,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(GenericLoaderState),
    .class_init = generic_loader_class_init,
};

static void generic_loader_register_type(void)
{
    type_register_static(&generic_loader_info);
}

type_init(generic_loader_register_type)
