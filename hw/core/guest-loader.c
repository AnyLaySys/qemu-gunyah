

#include "qemu/osdep.h"
#include "hw/core/cpu.h"
#include "system/dma.h"
#include "hw/loader.h"
#include "hw/qdev-properties.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "guest-loader.h"
#include "system/device_tree.h"
#include "hw/boards.h"

static void loader_insert_platform_data(GuestLoaderState *s, int size,
                                        Error **errp)
{
    MachineState *machine = MACHINE(qdev_get_machine());
    void *fdt = machine->fdt;
    g_autofree char *node = g_strdup_printf("/chosen/module@0x%08" PRIx64,
                                            s->addr);
    uint64_t reg_attr[2] = {cpu_to_be64(s->addr), cpu_to_be64(size)};

    if (!fdt) {
        error_setg(errp, "Cannot modify FDT fields if the machine has none");
        return;
    }

    qemu_fdt_add_subnode(fdt, node);
    qemu_fdt_setprop(fdt, node, "reg", &reg_attr, sizeof(reg_attr));

    if (s->kernel) {
        const char *compat[2] = { "multiboot,module", "multiboot,kernel" };
        if (qemu_fdt_setprop_string_array(fdt, node, "compatible",
                                          (char **) &compat,
                                          ARRAY_SIZE(compat)) < 0) {
            error_setg(errp, "couldn't set %s/compatible", node);
            return;
        }
        if (s->args) {
            if (qemu_fdt_setprop_string(fdt, node, "bootargs", s->args) < 0) {
                error_setg(errp, "couldn't set %s/bootargs", node);
            }
        }
    } else if (s->initrd) {
        const char *compat[2] = { "multiboot,module", "multiboot,ramdisk" };
        if (qemu_fdt_setprop_string_array(fdt, node, "compatible",
                                          (char **) &compat,
                                          ARRAY_SIZE(compat)) < 0) {
            error_setg(errp, "couldn't set %s/compatible", node);
            return;
        }
    }
}

static void guest_loader_realize(DeviceState *dev, Error **errp)
{
    GuestLoaderState *s = GUEST_LOADER(dev);
    char *file = s->kernel ? s->kernel : s->initrd;
    int size = 0;

    if (s->kernel && s->initrd) {
        error_setg(errp, "Cannot specify a kernel and initrd in same stanza");
        return;
    } else if (!s->kernel && !s->initrd)  {
        error_setg(errp, "Need to specify a kernel or initrd image");
        return;
    } else if (!s->addr) {
        error_setg(errp, "Need to specify the address of guest blob");
        return;
    } else if (s->args && !s->kernel) {
        error_setg(errp, "Boot args only relevant to kernel blobs");
    }

    size = load_image_targphys_as(file, s->addr, current_machine->ram_size,
                                  NULL);
    if (size < 0) {
        error_setg(errp, "Cannot load specified image %s", file);
        return;
    }

    loader_insert_platform_data(s, size, errp);
}

static const Property guest_loader_props[] = {
    DEFINE_PROP_UINT64("addr", GuestLoaderState, addr, 0),
    DEFINE_PROP_STRING("kernel", GuestLoaderState, kernel),
    DEFINE_PROP_STRING("bootargs", GuestLoaderState, args),
    DEFINE_PROP_STRING("initrd", GuestLoaderState, initrd),
};

static void guest_loader_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = guest_loader_realize;
    device_class_set_props(dc, guest_loader_props);
    dc->desc = "Guest Loader";
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static const TypeInfo guest_loader_info = {
    .name = TYPE_GUEST_LOADER,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(GuestLoaderState),
    .class_init = guest_loader_class_init,
};

static void guest_loader_register_type(void)
{
    type_register_static(&guest_loader_info);
}

type_init(guest_loader_register_type)
