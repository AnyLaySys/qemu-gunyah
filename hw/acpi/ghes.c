
#include "qemu/osdep.h"
#include "qemu/units.h"
#include "hw/acpi/ghes.h"
#include "hw/acpi/aml-build.h"
#include "qemu/error-report.h"
#include "hw/acpi/generic_event_device.h"
#include "hw/nvram/fw_cfg.h"
#include "qemu/uuid.h"

#define ACPI_HW_ERROR_FW_CFG_FILE           "etc/hardware_errors"
#define ACPI_HW_ERROR_ADDR_FW_CFG_FILE      "etc/hardware_errors_addr"

#define ACPI_GHES_MAX_RAW_DATA_LENGTH   (1 * KiB)

#define ACPI_GHES_SOURCE_GENERIC_ERROR_V2   10

#define GAS_ADDR_OFFSET 4

#define ACPI_GHES_DATA_LENGTH               72

#define ACPI_GHES_MEM_CPER_LENGTH           80

#define ACPI_GEBS_UNCORRECTABLE         1

#define ACPI_GHES_GESB_SIZE                 20

enum AcpiGenericErrorSeverity {
    ACPI_CPER_SEV_RECOVERABLE = 0,
    ACPI_CPER_SEV_FATAL = 1,
    ACPI_CPER_SEV_CORRECTED = 2,
    ACPI_CPER_SEV_NONE = 3,
};

static void build_ghes_hw_error_notification(GArray *table, const uint8_t type)
{
    build_append_int_noprefix(table, type, 1);
    build_append_int_noprefix(table, 28, 1);
    build_append_int_noprefix(table, 0, 2);
    build_append_int_noprefix(table, 0, 4);
    build_append_int_noprefix(table, 0, 4);
    build_append_int_noprefix(table, 0, 4);
    build_append_int_noprefix(table, 0, 4);
    build_append_int_noprefix(table, 0, 4);
    build_append_int_noprefix(table, 0, 4);
}

static void acpi_ghes_generic_error_data(GArray *table,
                const uint8_t *section_type, uint32_t error_severity,
                uint8_t validation_bits, uint8_t flags,
                uint32_t error_data_length, QemuUUID fru_id,
                uint64_t time_stamp)
{
    const uint8_t fru_text[20] = {0};

    g_array_append_vals(table, section_type, 16);

    build_append_int_noprefix(table, error_severity, 4);
    build_append_int_noprefix(table, 0x300, 2);
    build_append_int_noprefix(table, validation_bits, 1);
    build_append_int_noprefix(table, flags, 1);
    build_append_int_noprefix(table, error_data_length, 4);

    g_array_append_vals(table, fru_id.data, ARRAY_SIZE(fru_id.data));

    g_array_append_vals(table, fru_text, sizeof(fru_text));

    build_append_int_noprefix(table, time_stamp, 8);
}

static void acpi_ghes_generic_error_status(GArray *table, uint32_t block_status,
                uint32_t raw_data_offset, uint32_t raw_data_length,
                uint32_t data_length, uint32_t error_severity)
{
    build_append_int_noprefix(table, block_status, 4);
    build_append_int_noprefix(table, raw_data_offset, 4);
    build_append_int_noprefix(table, raw_data_length, 4);
    build_append_int_noprefix(table, data_length, 4);
    build_append_int_noprefix(table, error_severity, 4);
}

static void acpi_ghes_build_append_mem_cper(GArray *table,
                                            uint64_t error_physical_addr)
{

    build_append_int_noprefix(table,
                              (1ULL << 14) | /* Type Valid */
                              (1ULL << 1) /* Physical Address Valid */,
                              8);
    build_append_int_noprefix(table, 0, 8);
    build_append_int_noprefix(table, error_physical_addr, 8);
    build_append_int_noprefix(table, 0, 48);
    build_append_int_noprefix(table, 0 /* Unknown error */, 1);
    build_append_int_noprefix(table, 0, 7);
}

static void
ghes_gen_err_data_uncorrectable_recoverable(GArray *block,
                                            const uint8_t *section_type,
                                            int data_length)
{
    QemuUUID fru_id = {};

    acpi_ghes_generic_error_status(block, ACPI_GEBS_UNCORRECTABLE,
        0, 0, data_length, ACPI_CPER_SEV_RECOVERABLE);

    acpi_ghes_generic_error_data(block, section_type,
        ACPI_CPER_SEV_RECOVERABLE, 0, 0,
        ACPI_GHES_MEM_CPER_LENGTH, fru_id, 0);
}

static void build_ghes_error_table(GArray *hardware_errors, BIOSLinker *linker)
{
    int i, error_status_block_offset;

    for (i = 0; i < ACPI_GHES_ERROR_SOURCE_COUNT; i++) {
        build_append_int_noprefix(hardware_errors, 0, sizeof(uint64_t));
    }

    for (i = 0; i < ACPI_GHES_ERROR_SOURCE_COUNT; i++) {
        build_append_int_noprefix(hardware_errors, 1, sizeof(uint64_t));
    }

    error_status_block_offset = hardware_errors->len;

    acpi_data_push(hardware_errors,
        ACPI_GHES_MAX_RAW_DATA_LENGTH * ACPI_GHES_ERROR_SOURCE_COUNT);

    bios_linker_loader_alloc(linker, ACPI_HW_ERROR_FW_CFG_FILE,
                             hardware_errors, sizeof(uint64_t), false);

    for (i = 0; i < ACPI_GHES_ERROR_SOURCE_COUNT; i++) {
        bios_linker_loader_add_pointer(linker,
                                       ACPI_HW_ERROR_FW_CFG_FILE,
                                       sizeof(uint64_t) * i,
                                       sizeof(uint64_t),
                                       ACPI_HW_ERROR_FW_CFG_FILE,
                                       error_status_block_offset +
                                       i * ACPI_GHES_MAX_RAW_DATA_LENGTH);
    }

    bios_linker_loader_write_pointer(linker, ACPI_HW_ERROR_ADDR_FW_CFG_FILE, 0,
                                     sizeof(uint64_t),
                                     ACPI_HW_ERROR_FW_CFG_FILE, 0);
}

static void build_ghes_v2(GArray *table_data,
                          BIOSLinker *linker,
                          enum AcpiGhesNotifyType notify,
                          uint16_t source_id)
{
    uint64_t address_offset;

    build_append_int_noprefix(table_data, ACPI_GHES_SOURCE_GENERIC_ERROR_V2, 2);
    build_append_int_noprefix(table_data, source_id, 2);
    build_append_int_noprefix(table_data, 0xffff, 2);
    build_append_int_noprefix(table_data, 0, 1);
    build_append_int_noprefix(table_data, 1, 1);

    build_append_int_noprefix(table_data, 1, 4);
    build_append_int_noprefix(table_data, 1, 4);
    build_append_int_noprefix(table_data, ACPI_GHES_MAX_RAW_DATA_LENGTH, 4);

    address_offset = table_data->len;
    build_append_gas(table_data, AML_AS_SYSTEM_MEMORY, 0x40, 0,
                     4 /* QWord access */, 0);
    bios_linker_loader_add_pointer(linker, ACPI_BUILD_TABLE_FILE,
                                   address_offset + GAS_ADDR_OFFSET,
                                   sizeof(uint64_t),
                                   ACPI_HW_ERROR_FW_CFG_FILE,
                                   source_id * sizeof(uint64_t));

    build_ghes_hw_error_notification(table_data, notify);

    build_append_int_noprefix(table_data, ACPI_GHES_MAX_RAW_DATA_LENGTH, 4);

    address_offset = table_data->len;
    build_append_gas(table_data, AML_AS_SYSTEM_MEMORY, 0x40, 0,
                     4 /* QWord access */, 0);
    bios_linker_loader_add_pointer(linker, ACPI_BUILD_TABLE_FILE,
                                   address_offset + GAS_ADDR_OFFSET,
                                   sizeof(uint64_t),
                                   ACPI_HW_ERROR_FW_CFG_FILE,
                                   (ACPI_GHES_ERROR_SOURCE_COUNT + source_id)
                                   * sizeof(uint64_t));

    build_append_int_noprefix(table_data, ~0x1ULL, 8);
    build_append_int_noprefix(table_data, 0x1, 8);
}

void acpi_build_hest(GArray *table_data, GArray *hardware_errors,
                     BIOSLinker *linker,
                     const char *oem_id, const char *oem_table_id)
{
    AcpiTable table = { .sig = "HEST", .rev = 1,
                        .oem_id = oem_id, .oem_table_id = oem_table_id };

    build_ghes_error_table(hardware_errors, linker);

    acpi_table_begin(&table, table_data);

    build_append_int_noprefix(table_data, ACPI_GHES_ERROR_SOURCE_COUNT, 4);
    build_ghes_v2(table_data, linker,
                  ACPI_GHES_NOTIFY_SEA, ACPI_HEST_SRC_ID_SEA);

    acpi_table_end(linker, &table);
}

void acpi_ghes_add_fw_cfg(AcpiGhesState *ags, FWCfgState *s,
                          GArray *hardware_error)
{
    fw_cfg_add_file(s, ACPI_HW_ERROR_FW_CFG_FILE, hardware_error->data,
                    hardware_error->len);

    fw_cfg_add_file_callback(s, ACPI_HW_ERROR_ADDR_FW_CFG_FILE, NULL, NULL,
        NULL, &(ags->hw_error_le), sizeof(ags->hw_error_le), false);

    ags->present = true;
}

static void get_hw_error_offsets(uint64_t ghes_addr,
                                 uint64_t *cper_addr,
                                 uint64_t *read_ack_register_addr)
{
    if (!ghes_addr) {
        return;
    }


    cpu_physical_memory_read(ghes_addr, cper_addr,
                             sizeof(*cper_addr));

    *cper_addr = le64_to_cpu(*cper_addr);

    *read_ack_register_addr = ghes_addr + sizeof(uint64_t);
}

static void ghes_record_cper_errors(const void *cper, size_t len,
                                    uint16_t source_id, Error **errp)
{
    uint64_t cper_addr = 0, read_ack_register_addr = 0, read_ack_register;
    AcpiGedState *acpi_ged_state;
    AcpiGhesState *ags;

    if (len > ACPI_GHES_MAX_RAW_DATA_LENGTH) {
        error_setg(errp, "GHES CPER record is too big: %zd", len);
        return;
    }

    acpi_ged_state = ACPI_GED(object_resolve_path_type("", TYPE_ACPI_GED,
                                                       NULL));
    if (!acpi_ged_state) {
        error_setg(errp, "Can't find ACPI_GED object");
        return;
    }
    ags = &acpi_ged_state->ghes_state;

    assert(ACPI_GHES_ERROR_SOURCE_COUNT == 1);
    get_hw_error_offsets(le64_to_cpu(ags->hw_error_le),
                         &cper_addr, &read_ack_register_addr);

    if (!cper_addr) {
        error_setg(errp, "can not find Generic Error Status Block");
        return;
    }

    cpu_physical_memory_read(read_ack_register_addr,
                             &read_ack_register, sizeof(read_ack_register));

    if (!read_ack_register) {
        error_setg(errp,
                   "OSPM does not acknowledge previous error,"
                   " so can not record CPER for current error anymore");
        return;
    }

    read_ack_register = cpu_to_le64(0);
    cpu_physical_memory_write(read_ack_register_addr,
                              &read_ack_register, sizeof(uint64_t));

    cpu_physical_memory_write(cper_addr, cper, len);
}

int acpi_ghes_memory_errors(uint16_t source_id, uint64_t physical_address)
{
    const uint8_t guid[] =
          UUID_LE(0xA5BC1114, 0x6F64, 0x4EDE, 0xB8, 0x63, 0x3E, 0x83, \
                  0xED, 0x7C, 0x83, 0xB1);
    Error *errp = NULL;
    int data_length;
    GArray *block;

    block = g_array_new(false, true /* clear */, 1);

    data_length = ACPI_GHES_DATA_LENGTH + ACPI_GHES_MEM_CPER_LENGTH;
    assert((data_length + ACPI_GHES_GESB_SIZE) <=
            ACPI_GHES_MAX_RAW_DATA_LENGTH);

    ghes_gen_err_data_uncorrectable_recoverable(block, guid, data_length);

    acpi_ghes_build_append_mem_cper(block, physical_address);

    ghes_record_cper_errors(block->data, block->len, source_id, &errp);

    g_array_free(block, true);

    if (errp) {
        error_report_err(errp);
        return -1;
    }

    return 0;
}

bool acpi_ghes_present(void)
{
    AcpiGedState *acpi_ged_state;
    AcpiGhesState *ags;

    acpi_ged_state = ACPI_GED(object_resolve_path_type("", TYPE_ACPI_GED,
                                                       NULL));

    if (!acpi_ged_state) {
        return false;
    }
    ags = &acpi_ged_state->ghes_state;
    return ags->present;
}
