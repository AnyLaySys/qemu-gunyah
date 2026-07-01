
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "hw/acpi/acpi.h"
#include "hw/acpi/aml-build.h"
#include "hw/acpi/vmgenid.h"
#include "hw/nvram/fw_cfg.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "state/vmstate.h"
#include "system/reset.h"

void vmgenid_build_acpi(VmGenIdState *vms, GArray *table_data, GArray *guid,
                        BIOSLinker *linker, const char *oem_id)
{
    Aml *ssdt, *dev, *scope, *method, *addr, *if_ctx;
    uint32_t vgia_offset;
    QemuUUID guid_le;
    AcpiTable table = { .sig = "SSDT", .rev = 1,
                        .oem_id = oem_id, .oem_table_id = "VMGENID" };

    g_array_set_size(guid, VMGENID_FW_CFG_SIZE - ARRAY_SIZE(guid_le.data));
    guid_le = qemu_uuid_bswap(vms->guid);
    g_array_insert_vals(guid, VMGENID_GUID_OFFSET, guid_le.data,
                        ARRAY_SIZE(guid_le.data));

    acpi_table_begin(&table, table_data);
    ssdt = init_aml_allocator();

    vgia_offset = table_data->len +
        build_append_named_dword(ssdt->buf, "VGIA");
    scope = aml_scope("\\_SB");
    dev = aml_device("VGEN");
    aml_append(dev, aml_name_decl("_HID", aml_string("QEMUVGID")));
    aml_append(dev, aml_name_decl("_CID", aml_string("VM_Gen_Counter")));
    aml_append(dev, aml_name_decl("_DDN", aml_string("VM_Gen_Counter")));

    method = aml_method("_STA", 0, AML_NOTSERIALIZED);
    addr = aml_local(0);
    aml_append(method, aml_store(aml_int(0xf), addr));
    if_ctx = aml_if(aml_equal(aml_name("VGIA"), aml_int(0)));
    aml_append(if_ctx, aml_store(aml_int(0), addr));
    aml_append(method, if_ctx);
    aml_append(method, aml_return(addr));
    aml_append(dev, method);

    method = aml_method("ADDR", 0, AML_NOTSERIALIZED);

    addr = aml_local(0);
    aml_append(method, aml_store(aml_package(2), addr));

    aml_append(method, aml_store(aml_add(aml_name("VGIA"),
                                         aml_int(VMGENID_GUID_OFFSET), NULL),
                                 aml_index(addr, aml_int(0))));
    aml_append(method, aml_store(aml_int(0), aml_index(addr, aml_int(1))));
    aml_append(method, aml_return(addr));

    aml_append(dev, method);
    aml_append(scope, dev);
    aml_append(ssdt, scope);

    method = aml_method("\\_GPE._E05", 0, AML_NOTSERIALIZED);
    aml_append(method, aml_notify(aml_name("\\_SB.VGEN"), aml_int(0x80)));
    aml_append(ssdt, method);

    g_array_append_vals(table_data, ssdt->buf->data, ssdt->buf->len);

    bios_linker_loader_alloc(linker, VMGENID_GUID_FW_CFG_FILE, guid, 4096,
                             false /* page boundary, high memory */);

    bios_linker_loader_write_pointer(linker,
        VMGENID_ADDR_FW_CFG_FILE, 0, sizeof(uint64_t),
        VMGENID_GUID_FW_CFG_FILE, VMGENID_GUID_OFFSET);

    bios_linker_loader_add_pointer(linker,
        ACPI_BUILD_TABLE_FILE, vgia_offset, sizeof(uint32_t),
        VMGENID_GUID_FW_CFG_FILE, 0);

    acpi_table_end(linker, &table);
    free_aml_allocator();
}

void vmgenid_add_fw_cfg(VmGenIdState *vms, FWCfgState *s, GArray *guid)
{
    fw_cfg_add_file(s, VMGENID_GUID_FW_CFG_FILE, guid->data,
                    VMGENID_FW_CFG_SIZE);
    fw_cfg_add_file_callback(s, VMGENID_ADDR_FW_CFG_FILE, NULL, NULL, NULL,
                             vms->vmgenid_addr_le,
                             ARRAY_SIZE(vms->vmgenid_addr_le), false);
}

static void vmgenid_update_guest(VmGenIdState *vms)
{
    Object *obj = object_resolve_path_type("", TYPE_ACPI_DEVICE_IF, NULL);
    uint32_t vmgenid_addr;
    QemuUUID guid_le;

    if (obj) {
        memcpy(&vmgenid_addr, vms->vmgenid_addr_le, sizeof(vmgenid_addr));
        vmgenid_addr = le32_to_cpu(vmgenid_addr);
        if (vmgenid_addr) {
            guid_le = qemu_uuid_bswap(vms->guid);
            cpu_physical_memory_write(vmgenid_addr, guid_le.data,
                                      sizeof(guid_le.data));
            acpi_send_event(DEVICE(obj), ACPI_VMGENID_CHANGE_STATUS);
        }
    }
}

static int vmgenid_post_load(void *opaque, int version_id)
{
    VmGenIdState *vms = opaque;
    vmgenid_update_guest(vms);
    return 0;
}

static const VMStateDescription vmstate_vmgenid = {
    .name = "vmgenid",
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = vmgenid_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT8_ARRAY(vmgenid_addr_le, VmGenIdState, sizeof(uint64_t)),
        VMSTATE_END_OF_LIST()
    },
};

static void vmgenid_handle_reset(void *opaque)
{
    VmGenIdState *vms = VMGENID(opaque);
    memset(vms->vmgenid_addr_le, 0, ARRAY_SIZE(vms->vmgenid_addr_le));
}

static void vmgenid_realize(DeviceState *dev, Error **errp)
{
    VmGenIdState *vms = VMGENID(dev);

    if (!bios_linker_loader_can_write_pointer()) {
        error_setg(errp, "%s requires DMA write support in fw_cfg, "
                   "which this machine type does not provide", TYPE_VMGENID);
        return;
    }

    if (!find_vmgenid_dev()) {
        error_setg(errp, "at most one %s device is permitted", TYPE_VMGENID);
        return;
    }

    qemu_register_reset(vmgenid_handle_reset, vms);

    vmgenid_update_guest(vms);
}

static const Property vmgenid_device_properties[] = {
    DEFINE_PROP_UUID(VMGENID_GUID, VmGenIdState, guid),
};

static void vmgenid_device_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->vmsd = &vmstate_vmgenid;
    dc->realize = vmgenid_realize;
    device_class_set_props(dc, vmgenid_device_properties);
    dc->hotpluggable = false;
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static const TypeInfo vmgenid_device_info = {
    .name          = TYPE_VMGENID,
    .parent        = TYPE_DEVICE,
    .instance_size = sizeof(VmGenIdState),
    .class_init    = vmgenid_device_class_init,
};

static void vmgenid_register_types(void)
{
    type_register_static(&vmgenid_device_info);
}

type_init(vmgenid_register_types)
