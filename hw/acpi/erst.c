
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/qdev-core.h"
#include "exec/memory.h"
#include "qom/object.h"
#include "hw/pci/pci_device.h"
#include "qom/object_interfaces.h"
#include "qemu/error-report.h"
#include "migration/vmstate.h"
#include "hw/qdev-properties.h"
#include "hw/acpi/acpi.h"
#include "hw/acpi/acpi-defs.h"
#include "hw/acpi/aml-build.h"
#include "hw/acpi/bios-linker-loader.h"
#include "exec/address-spaces.h"
#include "system/hostmem.h"
#include "hw/acpi/erst.h"
#include "trace.h"

#define ACTION_BEGIN_WRITE_OPERATION         0x0
#define ACTION_BEGIN_READ_OPERATION          0x1
#define ACTION_BEGIN_CLEAR_OPERATION         0x2
#define ACTION_END_OPERATION                 0x3
#define ACTION_SET_RECORD_OFFSET             0x4
#define ACTION_EXECUTE_OPERATION             0x5
#define ACTION_CHECK_BUSY_STATUS             0x6
#define ACTION_GET_COMMAND_STATUS            0x7
#define ACTION_GET_RECORD_IDENTIFIER         0x8
#define ACTION_SET_RECORD_IDENTIFIER         0x9
#define ACTION_GET_RECORD_COUNT              0xA
#define ACTION_BEGIN_DUMMY_WRITE_OPERATION   0xB
#define ACTION_RESERVED                      0xC
#define ACTION_GET_ERROR_LOG_ADDRESS_RANGE   0xD
#define ACTION_GET_ERROR_LOG_ADDRESS_LENGTH  0xE
#define ACTION_GET_ERROR_LOG_ADDRESS_RANGE_ATTRIBUTES 0xF
#define ACTION_GET_EXECUTE_OPERATION_TIMINGS 0x10 /* ACPI 6.3 */

#define STATUS_SUCCESS                0x00
#define STATUS_NOT_ENOUGH_SPACE       0x01
#define STATUS_HARDWARE_NOT_AVAILABLE 0x02
#define STATUS_FAILED                 0x03
#define STATUS_RECORD_STORE_EMPTY     0x04
#define STATUS_RECORD_NOT_FOUND       0x05

#define INST_READ_REGISTER                 0x00
#define INST_READ_REGISTER_VALUE           0x01
#define INST_WRITE_REGISTER                0x02
#define INST_WRITE_REGISTER_VALUE          0x03
#define INST_NOOP                          0x04
#define INST_LOAD_VAR1                     0x05
#define INST_LOAD_VAR2                     0x06
#define INST_STORE_VAR1                    0x07
#define INST_ADD                           0x08
#define INST_SUBTRACT                      0x09
#define INST_ADD_VALUE                     0x0A
#define INST_SUBTRACT_VALUE                0x0B
#define INST_STALL                         0x0C
#define INST_STALL_WHILE_TRUE              0x0D
#define INST_SKIP_NEXT_INSTRUCTION_IF_TRUE 0x0E
#define INST_GOTO                          0x0F
#define INST_SET_SRC_ADDRESS_BASE          0x10
#define INST_SET_DST_ADDRESS_BASE          0x11
#define INST_MOVE_DATA                     0x12

#define UEFI_CPER_RECORD_MIN_SIZE 128U
#define UEFI_CPER_RECORD_LENGTH_OFFSET 20U
#define UEFI_CPER_RECORD_ID_OFFSET 96U


#define ERST_REG_SIZE (16UL)
#define ERST_ACTION_OFFSET (0UL) /* action (cmd) */
#define ERST_VALUE_OFFSET  (8UL) /* argument/value (data) */

#define ERST_RECORD_SIZE (8192UL)

#define ACPI_ERST_MEMDEV_PROP "memdev"
#define ACPI_ERST_RECORD_SIZE_PROP "record_size"

#define ERST_UNSPECIFIED_RECORD_ID (0UL)
#define ERST_EMPTY_END_RECORD_ID (~0UL)

#define ERST_IS_VALID_RECORD_ID(rid) \
    ((rid != ERST_UNSPECIFIED_RECORD_ID) && \
     (rid != ERST_EMPTY_END_RECORD_ID))

#define ERST_EXECUTE_OPERATION_MAGIC 0x9CUL
#define ERST_STORE_MAGIC 0x524F545354535245UL /* ERSTSTOR */
typedef struct {
    uint64_t magic;
    uint32_t record_size;
    uint32_t storage_offset; /* offset to record storage beyond header */
    uint16_t version;
    uint16_t reserved;
    uint32_t record_count;
    uint64_t map[]; /* contains record_ids, and position indicates index */
} __attribute__((packed)) ERSTStorageHeader;

#define ACPIERST(obj) \
    OBJECT_CHECK(ERSTDeviceState, (obj), TYPE_ACPI_ERST)

typedef struct {
    PCIDevice parent_obj;

    HostMemoryBackend *hostmem;
    MemoryRegion *hostmem_mr;
    uint32_t storage_size;
    uint32_t default_record_size;

    MemoryRegion iomem_mr;

    MemoryRegion exchange_mr;

    uint8_t operation;
    uint8_t busy_status;
    uint8_t command_status;
    uint32_t record_offset;
    uint64_t reg_action;
    uint64_t reg_value;
    uint64_t record_identifier;
    ERSTStorageHeader *header;
    unsigned first_record_index;
    unsigned last_record_index;
    unsigned next_record_index;

} ERSTDeviceState;

typedef struct {
    GArray *table_data;
    pcibus_t bar;
    uint8_t instruction;
    uint8_t flags;
    uint8_t register_bit_width;
    pcibus_t register_offset;
} BuildSerializationInstructionEntry;

static void build_serialization_instruction(
    BuildSerializationInstructionEntry *e,
    uint8_t serialization_action,
    uint64_t value)
{
    struct AcpiGenericAddress gas;
    uint64_t mask;

    build_append_int_noprefix(e->table_data, serialization_action, 1);
    build_append_int_noprefix(e->table_data, e->instruction, 1);
    build_append_int_noprefix(e->table_data, e->flags, 1);
    build_append_int_noprefix(e->table_data, 0, 1);
    gas.space_id = AML_SYSTEM_MEMORY;
    gas.bit_width = e->register_bit_width;
    gas.bit_offset = 0;
    gas.access_width = (uint8_t)ctz32(e->register_bit_width) - 2;
    gas.address = (uint64_t)(e->bar + e->register_offset);
    build_append_gas_from_struct(e->table_data, &gas);
    build_append_int_noprefix(e->table_data, value, 8);
    mask = (1ULL << (e->register_bit_width - 1) << 1) - 1;
    build_append_int_noprefix(e->table_data, mask, 8);
}

void build_erst(GArray *table_data, BIOSLinker *linker, Object *erst_dev,
    const char *oem_id, const char *oem_table_id)
{
    unsigned action;
    GArray *table_instruction_data = g_array_new(FALSE, FALSE, sizeof(char));
    pcibus_t bar0 = pci_get_bar_addr(PCI_DEVICE(erst_dev), 0);
    AcpiTable table = { .sig = "ERST", .rev = 1, .oem_id = oem_id,
                        .oem_table_id = oem_table_id };
    BuildSerializationInstructionEntry rd_value_32_val = {
        .table_data = table_instruction_data, .bar = bar0, .flags = 0,
        .instruction = INST_READ_REGISTER_VALUE,
        .register_bit_width = 32,
        .register_offset = ERST_VALUE_OFFSET,
    };
    BuildSerializationInstructionEntry rd_value_32 = {
        .table_data = table_instruction_data, .bar = bar0, .flags = 0,
        .instruction = INST_READ_REGISTER,
        .register_bit_width = 32,
        .register_offset = ERST_VALUE_OFFSET,
    };
    BuildSerializationInstructionEntry rd_value_64 = {
        .table_data = table_instruction_data, .bar = bar0, .flags = 0,
        .instruction = INST_READ_REGISTER,
        .register_bit_width = 64,
        .register_offset = ERST_VALUE_OFFSET,
    };
    BuildSerializationInstructionEntry wr_value_32_val = {
        .table_data = table_instruction_data, .bar = bar0, .flags = 0,
        .instruction = INST_WRITE_REGISTER_VALUE,
        .register_bit_width = 32,
        .register_offset = ERST_VALUE_OFFSET,
    };
    BuildSerializationInstructionEntry wr_value_32 = {
        .table_data = table_instruction_data, .bar = bar0, .flags = 0,
        .instruction = INST_WRITE_REGISTER,
        .register_bit_width = 32,
        .register_offset = ERST_VALUE_OFFSET,
    };
    BuildSerializationInstructionEntry wr_value_64 = {
        .table_data = table_instruction_data, .bar = bar0, .flags = 0,
        .instruction = INST_WRITE_REGISTER,
        .register_bit_width = 64,
        .register_offset = ERST_VALUE_OFFSET,
    };
    BuildSerializationInstructionEntry wr_action = {
        .table_data = table_instruction_data, .bar = bar0, .flags = 0,
        .instruction = INST_WRITE_REGISTER_VALUE,
        .register_bit_width = 32,
        .register_offset = ERST_ACTION_OFFSET,
    };

    trace_acpi_erst_pci_bar_0(bar0);

    action = ACTION_BEGIN_WRITE_OPERATION;
    build_serialization_instruction(&wr_action, action, action);

    action = ACTION_BEGIN_READ_OPERATION;
    build_serialization_instruction(&wr_action, action, action);

    action = ACTION_BEGIN_CLEAR_OPERATION;
    build_serialization_instruction(&wr_action, action, action);

    action = ACTION_END_OPERATION;
    build_serialization_instruction(&wr_action, action, action);

    action = ACTION_SET_RECORD_OFFSET;
    build_serialization_instruction(&wr_value_32, action, 0);
    build_serialization_instruction(&wr_action, action, action);

    action = ACTION_EXECUTE_OPERATION;
    build_serialization_instruction(&wr_value_32_val, action,
        ERST_EXECUTE_OPERATION_MAGIC);
    build_serialization_instruction(&wr_action, action, action);

    action = ACTION_CHECK_BUSY_STATUS;
    build_serialization_instruction(&wr_action, action, action);
    build_serialization_instruction(&rd_value_32_val, action, 0x01);

    action = ACTION_GET_COMMAND_STATUS;
    build_serialization_instruction(&wr_action, action, action);
    build_serialization_instruction(&rd_value_32, action, 0);

    action = ACTION_GET_RECORD_IDENTIFIER;
    build_serialization_instruction(&wr_action, action, action);
    build_serialization_instruction(&rd_value_64, action, 0);

    action = ACTION_SET_RECORD_IDENTIFIER;
    build_serialization_instruction(&wr_value_64, action, 0);
    build_serialization_instruction(&wr_action, action, action);

    action = ACTION_GET_RECORD_COUNT;
    build_serialization_instruction(&wr_action, action, action);
    build_serialization_instruction(&rd_value_32, action, 0);

    action = ACTION_BEGIN_DUMMY_WRITE_OPERATION;
    build_serialization_instruction(&wr_action, action, action);

    action = ACTION_GET_ERROR_LOG_ADDRESS_RANGE;
    build_serialization_instruction(&wr_action, action, action);
    build_serialization_instruction(&rd_value_64, action, 0);

    action = ACTION_GET_ERROR_LOG_ADDRESS_LENGTH;
    build_serialization_instruction(&wr_action, action, action);
    build_serialization_instruction(&rd_value_64, action, 0);

    action = ACTION_GET_ERROR_LOG_ADDRESS_RANGE_ATTRIBUTES;
    build_serialization_instruction(&wr_action, action, action);
    build_serialization_instruction(&rd_value_32, action, 0);

    action = ACTION_GET_EXECUTE_OPERATION_TIMINGS;
    build_serialization_instruction(&wr_action, action, action);
    build_serialization_instruction(&rd_value_64, action, 0);

    acpi_table_begin(&table, table_data);

    build_append_int_noprefix(table_data, 48, 4);

    build_append_int_noprefix(table_data,  0, 4);

    g_assert((table_instruction_data->len) % 32 == 0);
    build_append_int_noprefix(table_data,
        (table_instruction_data->len / 32), 4);

    g_array_append_vals(table_data, table_instruction_data->data,
        table_instruction_data->len);
    g_array_free(table_instruction_data, TRUE);

    acpi_table_end(linker, &table);
}

static uint8_t *get_nvram_ptr_by_index(ERSTDeviceState *s, unsigned index)
{
    uint8_t *rc = NULL;
    off_t offset = (index * le32_to_cpu(s->header->record_size));

    g_assert(offset < s->storage_size);

    rc = memory_region_get_ram_ptr(s->hostmem_mr);
    rc += offset;

    return rc;
}

static void make_erst_storage_header(ERSTDeviceState *s)
{
    ERSTStorageHeader *header = s->header;
    unsigned mapsz, headersz;

    header->magic = cpu_to_le64(ERST_STORE_MAGIC);
    header->record_size = cpu_to_le32(s->default_record_size);
    header->version = cpu_to_le16(0x0100);
    header->reserved = cpu_to_le16(0x0000);

    mapsz = s->storage_size / s->default_record_size;
    mapsz *= sizeof(uint64_t);
    headersz = sizeof(ERSTStorageHeader) + mapsz;
    headersz = QEMU_ALIGN_UP(headersz, s->default_record_size);
    header->storage_offset = cpu_to_le32(headersz);

}

static void check_erst_backend_storage(ERSTDeviceState *s, Error **errp)
{
    ERSTStorageHeader *header;
    uint32_t record_size;

    header = memory_region_get_ram_ptr(s->hostmem_mr);
    s->header = header;

    g_assert(QEMU_PTR_IS_ALIGNED(header, sizeof(uint64_t)));

    if (le64_to_cpu(header->magic) == 0UL) {
        make_erst_storage_header(s);
    }

    record_size = le32_to_cpu(header->record_size);
    if (!(
        (record_size) && /* non zero */
        (record_size >= UEFI_CPER_RECORD_MIN_SIZE) &&
        (((record_size - 1) & record_size) == 0) && /* is power of 2 */
        (record_size >= 4096) /* PAGE_SIZE */
        )) {
        error_setg(errp, "ERST record_size %u is invalid", record_size);
        return;
    }

    if (!(
        (le64_to_cpu(header->magic) == ERST_STORE_MAGIC) &&
        ((le32_to_cpu(header->storage_offset) % record_size) == 0) &&
        (le16_to_cpu(header->version) == 0x0100) &&
        (le16_to_cpu(header->reserved) == 0)
        )) {
        error_setg(errp, "ERST backend storage header is invalid");
        return;
    }

    if (((s->storage_size % record_size) != 0) ||
         (record_size > s->storage_size)) {
        error_setg(errp, "ACPI ERST requires storage size be multiple of "
            "record size (%uKiB)", record_size);
        return;
    }

    s->first_record_index = le32_to_cpu(header->storage_offset)
        / record_size;
    s->last_record_index = (s->storage_size / record_size);
}

static void update_map_entry(ERSTDeviceState *s, unsigned index,
    uint64_t record_id)
{
    if (index < s->last_record_index) {
        s->header->map[index] = cpu_to_le64(record_id);
    }
}

static unsigned find_next_empty_record_index(ERSTDeviceState *s)
{
    unsigned rc = 0; /* 0 not a valid index */
    unsigned index = s->first_record_index;

    for (; index < s->last_record_index; ++index) {
        if (le64_to_cpu(s->header->map[index]) == ERST_UNSPECIFIED_RECORD_ID) {
            rc = index;
            break;
        }
    }

    return rc;
}

static unsigned lookup_erst_record(ERSTDeviceState *s,
    uint64_t record_identifier)
{
    unsigned rc = 0; /* 0 not a valid index */

    if (record_identifier != ERST_UNSPECIFIED_RECORD_ID) {
        uint32_t record_count = le32_to_cpu(s->header->record_count);
        unsigned count = 0;
        unsigned index;
        for (index = s->first_record_index; index < s->last_record_index &&
                count < record_count; ++index) {
            if (le64_to_cpu(s->header->map[index]) == record_identifier) {
                rc = index;
                break;
            }
            if (le64_to_cpu(s->header->map[index]) !=
                ERST_UNSPECIFIED_RECORD_ID) {
                ++count;
            }
        }
    }

    return rc;
}

static unsigned get_next_record_identifier(ERSTDeviceState *s,
    uint64_t *record_identifier, bool first)
{
    unsigned found = 0;
    unsigned index;

    if (first) {
        s->next_record_index = s->first_record_index;
    }
    index = s->next_record_index;

    *record_identifier = ERST_EMPTY_END_RECORD_ID;

    if (le32_to_cpu(s->header->record_count)) {
        for (; index < s->last_record_index; ++index) {
            if (le64_to_cpu(s->header->map[index]) !=
                    ERST_UNSPECIFIED_RECORD_ID) {
                    s->next_record_index = index + 1;
                    *record_identifier = le64_to_cpu(s->header->map[index]);
                    found = 1;
                    break;
            }
        }
    }
    if (!found) {
        s->next_record_index = s->first_record_index;
    }

    return STATUS_SUCCESS;
}

static unsigned clear_erst_record(ERSTDeviceState *s)
{
    unsigned rc = STATUS_RECORD_NOT_FOUND;
    unsigned index;

    if (!ERST_IS_VALID_RECORD_ID(s->record_identifier)) {
        return STATUS_FAILED;
    }

    index = lookup_erst_record(s, s->record_identifier);
    if (index) {
        uint32_t record_count;
        update_map_entry(s, index, ERST_UNSPECIFIED_RECORD_ID);
        record_count = le32_to_cpu(s->header->record_count);
        record_count -= 1;
        s->header->record_count = cpu_to_le32(record_count);
        rc = STATUS_SUCCESS;
    }

    return rc;
}

static unsigned read_erst_record(ERSTDeviceState *s)
{
    unsigned rc = STATUS_RECORD_NOT_FOUND;
    unsigned exchange_length;
    unsigned index;

    if (le32_to_cpu(s->header->record_count) == 0) {
        return STATUS_RECORD_STORE_EMPTY;
    }

    exchange_length = memory_region_size(&s->exchange_mr);

    if (s->record_identifier == ERST_UNSPECIFIED_RECORD_ID) {
        get_next_record_identifier(s, &s->record_identifier, true);
    }

    if (s->record_identifier == ERST_EMPTY_END_RECORD_ID) {
        return STATUS_FAILED;
    }

    if (s->record_offset > (exchange_length - UEFI_CPER_RECORD_MIN_SIZE)) {
        return STATUS_FAILED;
    }

    index = lookup_erst_record(s, s->record_identifier);
    if (index) {
        uint8_t *nvram;
        uint8_t *exchange;
        uint32_t record_length;

        exchange = memory_region_get_ram_ptr(&s->exchange_mr);
        exchange += s->record_offset;
        nvram = get_nvram_ptr_by_index(s, index);
        memcpy((uint8_t *)&record_length,
            &nvram[UEFI_CPER_RECORD_LENGTH_OFFSET],
            sizeof(uint32_t));
        record_length = le32_to_cpu(record_length);
        if (record_length < UEFI_CPER_RECORD_MIN_SIZE) {
            rc = STATUS_FAILED;
        }
        if (record_length > exchange_length - s->record_offset) {
            rc = STATUS_FAILED;
        }
        if (rc != STATUS_FAILED) {
            memcpy(exchange, nvram, record_length);
            rc = STATUS_SUCCESS;
        }
    } else {
        get_next_record_identifier(s, &s->record_identifier, true);
    }

    return rc;
}

static unsigned write_erst_record(ERSTDeviceState *s)
{
    unsigned rc = STATUS_FAILED;
    unsigned exchange_length;
    unsigned index;
    uint64_t record_identifier;
    uint32_t record_length;
    uint8_t *exchange;
    uint8_t *nvram = NULL;
    bool record_found = false;

    exchange_length = memory_region_size(&s->exchange_mr);

    if (s->record_offset > (exchange_length - UEFI_CPER_RECORD_MIN_SIZE)) {
        return STATUS_FAILED;
    }

    exchange = memory_region_get_ram_ptr(&s->exchange_mr);
    exchange += s->record_offset;

    memcpy((uint8_t *)&record_length, &exchange[UEFI_CPER_RECORD_LENGTH_OFFSET],
        sizeof(uint32_t));
    record_length = le32_to_cpu(record_length);
    if (record_length < UEFI_CPER_RECORD_MIN_SIZE) {
        return STATUS_FAILED;
    }
    if (record_length > exchange_length - s->record_offset) {
        return STATUS_FAILED;
    }

    memcpy((uint8_t *)&record_identifier, &exchange[UEFI_CPER_RECORD_ID_OFFSET],
        sizeof(uint64_t));
    record_identifier = le64_to_cpu(record_identifier);

    if (!ERST_IS_VALID_RECORD_ID(record_identifier)) {
        return STATUS_FAILED;
    }

    index = lookup_erst_record(s, record_identifier);
    if (index) {
        nvram = get_nvram_ptr_by_index(s, index);
        record_found = true;
    } else {
        index = find_next_empty_record_index(s);
        if (index) {
            nvram = get_nvram_ptr_by_index(s, index);
        } else {
            rc = STATUS_NOT_ENOUGH_SPACE;
        }
    }
    if (nvram) {
        memcpy(nvram, exchange, record_length);
        memset(nvram + record_length, 0xFF, exchange_length - record_length);
        if (!record_found) {
            uint32_t record_count;
            record_count = le32_to_cpu(s->header->record_count);
            record_count += 1; /* writing new record */
            s->header->record_count = cpu_to_le32(record_count);
        }
        update_map_entry(s, index, record_identifier);
        rc = STATUS_SUCCESS;
    }

    return rc;
}


static uint64_t erst_rd_reg64(hwaddr addr,
    uint64_t reg, unsigned size)
{
    uint64_t rdval;
    uint64_t mask;
    unsigned shift;

    if (size == sizeof(uint64_t)) {
        mask = 0xFFFFFFFFFFFFFFFFUL;
        shift = 0;
    } else {
        mask = 0x00000000FFFFFFFFUL;
        shift = ((addr & 0x4) == 0x4) ? 32 : 0;
    }

    rdval = reg;
    rdval >>= shift;
    rdval &= mask;

    return rdval;
}

static uint64_t erst_wr_reg64(hwaddr addr,
    uint64_t reg, uint64_t val, unsigned size)
{
    uint64_t wrval;
    uint64_t mask;
    unsigned shift;

    if (size == sizeof(uint64_t)) {
        mask = 0xFFFFFFFFFFFFFFFFUL;
        shift = 0;
    } else {
        mask = 0x00000000FFFFFFFFUL;
        shift = ((addr & 0x4) == 0x4) ? 32 : 0;
    }

    val &= mask;
    val <<= shift;
    mask <<= shift;
    wrval = reg;
    wrval &= ~mask;
    wrval |= val;

    return wrval;
}

static void erst_reg_write(void *opaque, hwaddr addr,
    uint64_t val, unsigned size)
{
    ERSTDeviceState *s = (ERSTDeviceState *)opaque;

    trace_acpi_erst_reg_write(addr, val, size);

    switch (addr) {
    case ERST_VALUE_OFFSET + 0:
    case ERST_VALUE_OFFSET + 4:
        s->reg_value = erst_wr_reg64(addr, s->reg_value, val, size);
        break;
    case ERST_ACTION_OFFSET + 0:
        switch (val) {
        case ACTION_BEGIN_WRITE_OPERATION:
        case ACTION_BEGIN_READ_OPERATION:
        case ACTION_BEGIN_CLEAR_OPERATION:
        case ACTION_BEGIN_DUMMY_WRITE_OPERATION:
        case ACTION_END_OPERATION:
            s->operation = val;
            break;
        case ACTION_SET_RECORD_OFFSET:
            s->record_offset = s->reg_value;
            break;
        case ACTION_EXECUTE_OPERATION:
            if ((uint8_t)s->reg_value == ERST_EXECUTE_OPERATION_MAGIC) {
                s->busy_status = 1;
                switch (s->operation) {
                case ACTION_BEGIN_WRITE_OPERATION:
                    s->command_status = write_erst_record(s);
                    break;
                case ACTION_BEGIN_READ_OPERATION:
                    s->command_status = read_erst_record(s);
                    break;
                case ACTION_BEGIN_CLEAR_OPERATION:
                    s->command_status = clear_erst_record(s);
                    break;
                case ACTION_BEGIN_DUMMY_WRITE_OPERATION:
                    s->command_status = STATUS_SUCCESS;
                    break;
                case ACTION_END_OPERATION:
                    s->command_status = STATUS_SUCCESS;
                    break;
                default:
                    s->command_status = STATUS_FAILED;
                    break;
                }
                s->busy_status = 0;
            }
            break;
        case ACTION_CHECK_BUSY_STATUS:
            s->reg_value = s->busy_status;
            break;
        case ACTION_GET_COMMAND_STATUS:
            s->reg_value = s->command_status;
            break;
        case ACTION_GET_RECORD_IDENTIFIER:
            s->command_status = get_next_record_identifier(s,
                                    &s->reg_value, false);
            break;
        case ACTION_SET_RECORD_IDENTIFIER:
            s->record_identifier = s->reg_value;
            break;
        case ACTION_GET_RECORD_COUNT:
            s->reg_value = le32_to_cpu(s->header->record_count);
            break;
        case ACTION_GET_ERROR_LOG_ADDRESS_RANGE:
            s->reg_value = (hwaddr)pci_get_bar_addr(PCI_DEVICE(s), 1);
            break;
        case ACTION_GET_ERROR_LOG_ADDRESS_LENGTH:
            s->reg_value = le32_to_cpu(s->header->record_size);
            break;
        case ACTION_GET_ERROR_LOG_ADDRESS_RANGE_ATTRIBUTES:
            s->reg_value = 0x0; /* intentional, not NVRAM mode */
            break;
        case ACTION_GET_EXECUTE_OPERATION_TIMINGS:
            s->reg_value =
                (100ULL << 32) | /* 100us max time */
                (10ULL  <<  0) ; /*  10us min time */
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
}

static uint64_t erst_reg_read(void *opaque, hwaddr addr,
                                unsigned size)
{
    ERSTDeviceState *s = (ERSTDeviceState *)opaque;
    uint64_t val = 0;

    switch (addr) {
    case ERST_ACTION_OFFSET + 0:
    case ERST_ACTION_OFFSET + 4:
        val = erst_rd_reg64(addr, s->reg_action, size);
        break;
    case ERST_VALUE_OFFSET + 0:
    case ERST_VALUE_OFFSET + 4:
        val = erst_rd_reg64(addr, s->reg_value, size);
        break;
    default:
        break;
    }
    trace_acpi_erst_reg_read(addr, val, size);
    return val;
}

static const MemoryRegionOps erst_reg_ops = {
    .read = erst_reg_read,
    .write = erst_reg_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static int erst_post_load(void *opaque, int version_id)
{
    ERSTDeviceState *s = opaque;

    s->header = (ERSTStorageHeader *)get_nvram_ptr_by_index(s, 0);
    trace_acpi_erst_post_load(s->header, le32_to_cpu(s->header->record_size));

    return 0;
}

static const VMStateDescription erst_vmstate  = {
    .name = "acpi-erst",
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = erst_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT8(operation, ERSTDeviceState),
        VMSTATE_UINT8(busy_status, ERSTDeviceState),
        VMSTATE_UINT8(command_status, ERSTDeviceState),
        VMSTATE_UINT32(record_offset, ERSTDeviceState),
        VMSTATE_UINT64(reg_action, ERSTDeviceState),
        VMSTATE_UINT64(reg_value, ERSTDeviceState),
        VMSTATE_UINT64(record_identifier, ERSTDeviceState),
        VMSTATE_UINT32(next_record_index, ERSTDeviceState),
        VMSTATE_END_OF_LIST()
    }
};

static void erst_realizefn(PCIDevice *pci_dev, Error **errp)
{
    ERRP_GUARD();
    ERSTDeviceState *s = ACPIERST(pci_dev);

    trace_acpi_erst_realizefn_in();

    if (!s->hostmem) {
        error_setg(errp, "'" ACPI_ERST_MEMDEV_PROP "' property is not set");
        return;
    } else if (host_memory_backend_is_mapped(s->hostmem)) {
        error_setg(errp, "can't use already busy memdev: %s",
                   object_get_canonical_path_component(OBJECT(s->hostmem)));
        return;
    }

    s->hostmem_mr = host_memory_backend_get_memory(s->hostmem);

    s->storage_size = object_property_get_int(OBJECT(s->hostmem), "size", errp);
    if (*errp) {
        return;
    }

    check_erst_backend_storage(s, errp);
    if (*errp) {
        return;
    }

    memory_region_init_io(&s->iomem_mr, OBJECT(pci_dev), &erst_reg_ops, s,
                          TYPE_ACPI_ERST, ERST_REG_SIZE);
    pci_register_bar(pci_dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem_mr);

    memory_region_init_ram(&s->exchange_mr, OBJECT(pci_dev),
                            "erst.exchange",
                            le32_to_cpu(s->header->record_size), errp);
    if (*errp) {
        return;
    }
    pci_register_bar(pci_dev, 1, PCI_BASE_ADDRESS_SPACE_MEMORY,
                        &s->exchange_mr);

    vmstate_register_ram_global(s->hostmem_mr);

    trace_acpi_erst_realizefn_out(s->storage_size);
}

static void erst_reset(DeviceState *dev)
{
    ERSTDeviceState *s = ACPIERST(dev);

    trace_acpi_erst_reset_in(le32_to_cpu(s->header->record_count));
    s->operation = 0;
    s->busy_status = 0;
    s->command_status = STATUS_SUCCESS;
    s->record_identifier = ERST_UNSPECIFIED_RECORD_ID;
    s->record_offset = 0;
    s->next_record_index = s->first_record_index;
    trace_acpi_erst_reset_out(le32_to_cpu(s->header->record_count));
}

static const Property erst_properties[] = {
    DEFINE_PROP_LINK(ACPI_ERST_MEMDEV_PROP, ERSTDeviceState, hostmem,
                     TYPE_MEMORY_BACKEND, HostMemoryBackend *),
    DEFINE_PROP_UINT32(ACPI_ERST_RECORD_SIZE_PROP, ERSTDeviceState,
                     default_record_size, ERST_RECORD_SIZE),
};

static void erst_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    trace_acpi_erst_class_init_in();
    k->realize = erst_realizefn;
    k->vendor_id = PCI_VENDOR_ID_REDHAT;
    k->device_id = PCI_DEVICE_ID_REDHAT_ACPI_ERST;
    k->revision = 0x00;
    k->class_id = PCI_CLASS_OTHERS;
    device_class_set_legacy_reset(dc, erst_reset);
    dc->vmsd = &erst_vmstate;
    dc->user_creatable = true;
    dc->hotpluggable = false;
    device_class_set_props(dc, erst_properties);
    dc->desc = "ACPI Error Record Serialization Table (ERST) device";
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
    trace_acpi_erst_class_init_out();
}

static const TypeInfo erst_type_info = {
    .name          = TYPE_ACPI_ERST,
    .parent        = TYPE_PCI_DEVICE,
    .class_init    = erst_class_init,
    .instance_size = sizeof(ERSTDeviceState),
    .interfaces = (InterfaceInfo[]) {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { }
    }
};

static void erst_register_types(void)
{
    type_register_static(&erst_type_info);
}

type_init(erst_register_types)
