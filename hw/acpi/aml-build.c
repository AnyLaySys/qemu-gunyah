
#include "qemu/osdep.h"
#include <glib/gprintf.h>
#include "hw/acpi/aml-build.h"
#include "qemu/bswap.h"
#include "qemu/bitops.h"
#include "system/numa.h"
#include "hw/boards.h"
#include "hw/acpi/tpm.h"
#include "hw/pci/pci_host.h"
#include "hw/pci/pci_bus.h"
#include "hw/pci/pci_bridge.h"
#include "qemu/cutils.h"

static GArray *build_alloc_array(void)
{
    return g_array_new(false, true /* clear */, 1);
}

static void build_free_array(GArray *array)
{
    g_array_free(array, true);
}

static void build_prepend_byte(GArray *array, uint8_t val)
{
    g_array_prepend_val(array, val);
}

static void build_append_byte(GArray *array, uint8_t val)
{
    g_array_append_val(array, val);
}

static void build_append_padded_str(GArray *array, const char *str,
                                    size_t maxlen, char pad)
{
    size_t i;
    size_t len = strlen(str);

    g_assert(len <= maxlen);
    g_array_append_vals(array, str, len);
    for (i = maxlen - len; i > 0; i--) {
        g_array_append_val(array, pad);
    }
}

static void build_append_array(GArray *array, GArray *val)
{
    g_array_append_vals(array, val->data, val->len);
}

#define ACPI_NAMESEG_LEN 4

void crs_range_insert(GPtrArray *ranges, uint64_t base, uint64_t limit)
{
    CrsRangeEntry *entry;

    entry = g_malloc(sizeof(*entry));
    entry->base = base;
    entry->limit = limit;

    g_ptr_array_add(ranges, entry);
}

static void crs_range_free(gpointer data)
{
    CrsRangeEntry *entry = (CrsRangeEntry *)data;
    g_free(entry);
}

void crs_range_set_init(CrsRangeSet *range_set)
{
    range_set->io_ranges = g_ptr_array_new_with_free_func(crs_range_free);
    range_set->mem_ranges = g_ptr_array_new_with_free_func(crs_range_free);
    range_set->mem_64bit_ranges =
            g_ptr_array_new_with_free_func(crs_range_free);
}

void crs_range_set_free(CrsRangeSet *range_set)
{
    g_ptr_array_free(range_set->io_ranges, true);
    g_ptr_array_free(range_set->mem_ranges, true);
    g_ptr_array_free(range_set->mem_64bit_ranges, true);
}

static gint crs_range_compare(gconstpointer a, gconstpointer b)
{
    CrsRangeEntry *entry_a = *(CrsRangeEntry **)a;
    CrsRangeEntry *entry_b = *(CrsRangeEntry **)b;

    if (entry_a->base < entry_b->base) {
        return -1;
    } else if (entry_a->base > entry_b->base) {
        return 1;
    } else {
        return 0;
    }
}

void crs_replace_with_free_ranges(GPtrArray *ranges,
                                  uint64_t start, uint64_t end)
{
    GPtrArray *free_ranges = g_ptr_array_new();
    uint64_t free_base = start;
    int i;

    g_ptr_array_sort(ranges, crs_range_compare);
    for (i = 0; i < ranges->len; i++) {
        CrsRangeEntry *used = g_ptr_array_index(ranges, i);

        if (free_base < used->base) {
            crs_range_insert(free_ranges, free_base, used->base - 1);
        }

        free_base = used->limit + 1;
    }

    if (free_base < end) {
        crs_range_insert(free_ranges, free_base, end);
    }

    g_ptr_array_set_size(ranges, 0);
    for (i = 0; i < free_ranges->len; i++) {
        g_ptr_array_add(ranges, g_ptr_array_index(free_ranges, i));
    }

    g_ptr_array_free(free_ranges, true);
}

static void crs_range_merge(GPtrArray *range)
{
    GPtrArray *tmp = g_ptr_array_new_with_free_func(crs_range_free);
    CrsRangeEntry *entry;
    uint64_t range_base, range_limit;
    int i;

    if (!range->len) {
        return;
    }

    g_ptr_array_sort(range, crs_range_compare);

    entry = g_ptr_array_index(range, 0);
    range_base = entry->base;
    range_limit = entry->limit;
    for (i = 1; i < range->len; i++) {
        entry = g_ptr_array_index(range, i);
        if (entry->base - 1 == range_limit) {
            range_limit = entry->limit;
        } else {
            crs_range_insert(tmp, range_base, range_limit);
            range_base = entry->base;
            range_limit = entry->limit;
        }
    }
    crs_range_insert(tmp, range_base, range_limit);

    g_ptr_array_set_size(range, 0);
    for (i = 0; i < tmp->len; i++) {
        entry = g_ptr_array_index(tmp, i);
        crs_range_insert(range, entry->base, entry->limit);
    }
    g_ptr_array_free(tmp, true);
}

static void
build_append_nameseg(GArray *array, const char *seg)
{
    int len;

    len = strlen(seg);
    assert(len <= ACPI_NAMESEG_LEN);

    g_array_append_vals(array, seg, len);
    g_array_append_vals(array, "____", ACPI_NAMESEG_LEN - len);
}

static void G_GNUC_PRINTF(2, 0)
build_append_namestringv(GArray *array, const char *format, va_list ap)
{
    char *s;
    char **segs;
    char **segs_iter;
    int seg_count = 0;

    s = g_strdup_vprintf(format, ap);
    segs = g_strsplit(s, ".", 0);
    g_free(s);

    segs_iter = segs;
    while (*segs_iter) {
        ++segs_iter;
        ++seg_count;
    }
    assert(seg_count > 0 && seg_count <= 255);

    s = *segs;
    while (*s == '\\' || *s == '^') {
        build_append_byte(array, *s);
        ++s;
    }

    switch (seg_count) {
    case 1:
        if (!*s) {
            build_append_byte(array, 0x00); /* NullName */
        } else {
            build_append_nameseg(array, s);
        }
        break;

    case 2:
        build_append_byte(array, 0x2E); /* DualNamePrefix */
        build_append_nameseg(array, s);
        build_append_nameseg(array, segs[1]);
        break;
    default:
        build_append_byte(array, 0x2F); /* MultiNamePrefix */
        build_append_byte(array, seg_count);

        build_append_nameseg(array, s);

        segs_iter = segs + 1;
        while (*segs_iter) {
            build_append_nameseg(array, *segs_iter);
            ++segs_iter;
        }
        break;
    }
    g_strfreev(segs);
}

G_GNUC_PRINTF(2, 3)
static void build_append_namestring(GArray *array, const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    build_append_namestringv(array, format, ap);
    va_end(ap);
}

enum {
    PACKAGE_LENGTH_1BYTE_SHIFT = 6, /* Up to 63 - use extra 2 bits. */
    PACKAGE_LENGTH_2BYTE_SHIFT = 4,
    PACKAGE_LENGTH_3BYTE_SHIFT = 12,
    PACKAGE_LENGTH_4BYTE_SHIFT = 20,
};

static void
build_prepend_package_length(GArray *package, unsigned length, bool incl_self)
{
    uint8_t byte;
    unsigned length_bytes;

    if (length + 1 < (1 << PACKAGE_LENGTH_1BYTE_SHIFT)) {
        length_bytes = 1;
    } else if (length + 2 < (1 << PACKAGE_LENGTH_3BYTE_SHIFT)) {
        length_bytes = 2;
    } else if (length + 3 < (1 << PACKAGE_LENGTH_4BYTE_SHIFT)) {
        length_bytes = 3;
    } else {
        length_bytes = 4;
    }

    if (incl_self) {
        length += length_bytes;
    }

    switch (length_bytes) {
    case 1:
        byte = length;
        build_prepend_byte(package, byte);
        return;
    case 4:
        byte = length >> PACKAGE_LENGTH_4BYTE_SHIFT;
        build_prepend_byte(package, byte);
        length &= (1 << PACKAGE_LENGTH_4BYTE_SHIFT) - 1;
    case 3:
        byte = length >> PACKAGE_LENGTH_3BYTE_SHIFT;
        build_prepend_byte(package, byte);
        length &= (1 << PACKAGE_LENGTH_3BYTE_SHIFT) - 1;
    case 2:
        byte = length >> PACKAGE_LENGTH_2BYTE_SHIFT;
        build_prepend_byte(package, byte);
        length &= (1 << PACKAGE_LENGTH_2BYTE_SHIFT) - 1;
    }
    byte = ((length_bytes - 1) << PACKAGE_LENGTH_1BYTE_SHIFT) | length;
    build_prepend_byte(package, byte);
}

static void
build_append_pkg_length(GArray *array, unsigned length, bool incl_self)
{
    GArray *tmp = build_alloc_array();

    build_prepend_package_length(tmp, length, incl_self);
    build_append_array(array, tmp);
    build_free_array(tmp);
}

static void build_package(GArray *package, uint8_t op)
{
    build_prepend_package_length(package, package->len, true);
    build_prepend_byte(package, op);
}

static void build_extop_package(GArray *package, uint8_t op)
{
    build_package(package, op);
    build_prepend_byte(package, 0x5B); /* ExtOpPrefix */
}

void build_append_int_noprefix(GArray *table, uint64_t value, int size)
{
    int i;

    for (i = 0; i < size; ++i) {
        build_append_byte(table, value & 0xFF);
        value = value >> 8;
    }
}

static void build_append_int(GArray *table, uint64_t value)
{
    if (value == 0x00) {
        build_append_byte(table, 0x00); /* ZeroOp */
    } else if (value == 0x01) {
        build_append_byte(table, 0x01); /* OneOp */
    } else if (value <= 0xFF) {
        build_append_byte(table, 0x0A); /* BytePrefix */
        build_append_int_noprefix(table, value, 1);
    } else if (value <= 0xFFFF) {
        build_append_byte(table, 0x0B); /* WordPrefix */
        build_append_int_noprefix(table, value, 2);
    } else if (value <= 0xFFFFFFFF) {
        build_append_byte(table, 0x0C); /* DWordPrefix */
        build_append_int_noprefix(table, value, 4);
    } else {
        build_append_byte(table, 0x0E); /* QWordPrefix */
        build_append_int_noprefix(table, value, 8);
    }
}

void build_append_gas(GArray *table, AmlAddressSpace as,
                      uint8_t bit_width, uint8_t bit_offset,
                      uint8_t access_width, uint64_t address)
{
    build_append_int_noprefix(table, as, 1);
    build_append_int_noprefix(table, bit_width, 1);
    build_append_int_noprefix(table, bit_offset, 1);
    build_append_int_noprefix(table, access_width, 1);
    build_append_int_noprefix(table, address, 8);
}

int
build_append_named_dword(GArray *array, const char *name_format, ...)
{
    int offset;
    va_list ap;

    build_append_byte(array, 0x08); /* NameOp */
    va_start(ap, name_format);
    build_append_namestringv(array, name_format, ap);
    va_end(ap);

    build_append_byte(array, 0x0C); /* DWordPrefix */

    offset = array->len;
    build_append_int_noprefix(array, 0x00000000, 4);
    assert(array->len == offset + 4);

    return offset;
}

static GPtrArray *alloc_list;

static Aml *aml_alloc(void)
{
    Aml *var = g_new0(typeof(*var), 1);

    g_ptr_array_add(alloc_list, var);
    var->block_flags = AML_NO_OPCODE;
    var->buf = build_alloc_array();
    return var;
}

static Aml *aml_opcode(uint8_t op)
{
    Aml *var = aml_alloc();

    var->op  = op;
    var->block_flags = AML_OPCODE;
    return var;
}

static Aml *aml_bundle(uint8_t op, AmlBlockFlags flags)
{
    Aml *var = aml_alloc();

    var->op  = op;
    var->block_flags = flags;
    return var;
}

static void aml_free(gpointer data, gpointer user_data)
{
    Aml *var = data;
    build_free_array(var->buf);
    g_free(var);
}

Aml *init_aml_allocator(void)
{
    assert(!alloc_list);
    alloc_list = g_ptr_array_new();
    return aml_alloc();
}

void free_aml_allocator(void)
{
    g_ptr_array_foreach(alloc_list, aml_free, NULL);
    g_ptr_array_free(alloc_list, true);
    alloc_list = 0;
}

static void build_buffer(GArray *array, uint8_t op)
{
    GArray *data = build_alloc_array();

    build_append_int(data, array->len);
    g_array_prepend_vals(array, data->data, data->len);
    build_free_array(data);
    build_package(array, op);
}

void aml_append(Aml *parent_ctx, Aml *child)
{
    GArray *buf = build_alloc_array();
    build_append_array(buf, child->buf);

    switch (child->block_flags) {
    case AML_OPCODE:
        build_append_byte(parent_ctx->buf, child->op);
        break;
    case AML_EXT_PACKAGE:
        build_extop_package(buf, child->op);
        break;
    case AML_PACKAGE:
        build_package(buf, child->op);
        break;
    case AML_RES_TEMPLATE:
        build_append_byte(buf, 0x79); /* EndTag */
        build_append_byte(buf, 0);
    case AML_BUFFER:
        build_buffer(buf, child->op);
        break;
    case AML_NO_OPCODE:
        break;
    default:
        g_assert_not_reached();
    }
    build_append_array(parent_ctx->buf, buf);
    build_free_array(buf);
}

Aml *aml_scope(const char *name_format, ...)
{
    va_list ap;
    Aml *var = aml_bundle(0x10 /* ScopeOp */, AML_PACKAGE);
    va_start(ap, name_format);
    build_append_namestringv(var->buf, name_format, ap);
    va_end(ap);
    return var;
}

Aml *aml_return(Aml *val)
{
    Aml *var = aml_opcode(0xA4 /* ReturnOp */);
    aml_append(var, val);
    return var;
}

Aml *aml_debug(void)
{
    Aml *var = aml_alloc();
    build_append_byte(var->buf, 0x5B); /* ExtOpPrefix */
    build_append_byte(var->buf, 0x31); /* DebugOp */
    return var;
}

Aml *aml_int(const uint64_t val)
{
    Aml *var = aml_alloc();
    build_append_int(var->buf, val);
    return var;
}

Aml *aml_name(const char *name_format, ...)
{
    va_list ap;
    Aml *var = aml_alloc();
    va_start(ap, name_format);
    build_append_namestringv(var->buf, name_format, ap);
    va_end(ap);
    return var;
}

Aml *aml_name_decl(const char *name, Aml *val)
{
    Aml *var = aml_opcode(0x08 /* NameOp */);
    build_append_namestring(var->buf, "%s", name);
    aml_append(var, val);
    return var;
}

Aml *aml_arg(int pos)
{
    uint8_t op = 0x68 /* ARG0 op */ + pos;

    assert(pos <= 6);
    return aml_opcode(op);
}

Aml *aml_to_integer(Aml *arg)
{
    Aml *var = aml_opcode(0x99 /* ToIntegerOp */);
    aml_append(var, arg);
    build_append_byte(var->buf, 0x00 /* NullNameOp */);
    return var;
}

Aml *aml_to_hexstring(Aml *src, Aml *dst)
{
    Aml *var = aml_opcode(0x98 /* ToHexStringOp */);
    aml_append(var, src);
    if (dst) {
        aml_append(var, dst);
    } else {
        build_append_byte(var->buf, 0x00 /* NullNameOp */);
    }
    return var;
}

Aml *aml_to_buffer(Aml *src, Aml *dst)
{
    Aml *var = aml_opcode(0x96 /* ToBufferOp */);
    aml_append(var, src);
    if (dst) {
        aml_append(var, dst);
    } else {
        build_append_byte(var->buf, 0x00 /* NullNameOp */);
    }
    return var;
}

Aml *aml_to_decimalstring(Aml *src, Aml *dst)
{
    Aml *var = aml_opcode(0x97 /* ToDecimalStringOp */);
    aml_append(var, src);
    if (dst) {
        aml_append(var, dst);
    } else {
        build_append_byte(var->buf, 0x00 /* NullNameOp */);
    }
    return var;
}

Aml *aml_store(Aml *val, Aml *target)
{
    Aml *var = aml_opcode(0x70 /* StoreOp */);
    aml_append(var, val);
    aml_append(var, target);
    return var;
}

static Aml *
build_opcode_2arg_dst(uint8_t op, Aml *arg1, Aml *arg2, Aml *dst)
{
    Aml *var = aml_opcode(op);
    aml_append(var, arg1);
    aml_append(var, arg2);
    if (dst) {
        aml_append(var, dst);
    } else {
        build_append_byte(var->buf, 0x00 /* NullNameOp */);
    }
    return var;
}

Aml *aml_and(Aml *arg1, Aml *arg2, Aml *dst)
{
    return build_opcode_2arg_dst(0x7B /* AndOp */, arg1, arg2, dst);
}

Aml *aml_or(Aml *arg1, Aml *arg2, Aml *dst)
{
    return build_opcode_2arg_dst(0x7D /* OrOp */, arg1, arg2, dst);
}

Aml *aml_land(Aml *arg1, Aml *arg2)
{
    Aml *var = aml_opcode(0x90 /* LAndOp */);
    aml_append(var, arg1);
    aml_append(var, arg2);
    return var;
}

Aml *aml_lor(Aml *arg1, Aml *arg2)
{
    Aml *var = aml_opcode(0x91 /* LOrOp */);
    aml_append(var, arg1);
    aml_append(var, arg2);
    return var;
}

Aml *aml_shiftleft(Aml *arg1, Aml *count)
{
    return build_opcode_2arg_dst(0x79 /* ShiftLeftOp */, arg1, count, NULL);
}

Aml *aml_shiftright(Aml *arg1, Aml *count, Aml *dst)
{
    return build_opcode_2arg_dst(0x7A /* ShiftRightOp */, arg1, count, dst);
}

Aml *aml_lless(Aml *arg1, Aml *arg2)
{
    Aml *var = aml_opcode(0x95 /* LLessOp */);
    aml_append(var, arg1);
    aml_append(var, arg2);
    return var;
}

Aml *aml_add(Aml *arg1, Aml *arg2, Aml *dst)
{
    return build_opcode_2arg_dst(0x72 /* AddOp */, arg1, arg2, dst);
}

Aml *aml_subtract(Aml *arg1, Aml *arg2, Aml *dst)
{
    return build_opcode_2arg_dst(0x74 /* SubtractOp */, arg1, arg2, dst);
}

Aml *aml_increment(Aml *arg)
{
    Aml *var = aml_opcode(0x75 /* IncrementOp */);
    aml_append(var, arg);
    return var;
}

Aml *aml_decrement(Aml *arg)
{
    Aml *var = aml_opcode(0x76 /* DecrementOp */);
    aml_append(var, arg);
    return var;
}

Aml *aml_index(Aml *arg1, Aml *idx)
{
    return build_opcode_2arg_dst(0x88 /* IndexOp */, arg1, idx, NULL);
}

Aml *aml_notify(Aml *arg1, Aml *arg2)
{
    Aml *var = aml_opcode(0x86 /* NotifyOp */);
    aml_append(var, arg1);
    aml_append(var, arg2);
    return var;
}

Aml *aml_break(void)
{
    Aml *var = aml_opcode(0xa5 /* BreakOp */);
    return var;
}

Aml *aml_call0(const char *method)
{
    Aml *var = aml_alloc();
    build_append_namestring(var->buf, "%s", method);
    return var;
}

Aml *aml_call1(const char *method, Aml *arg1)
{
    Aml *var = aml_alloc();
    build_append_namestring(var->buf, "%s", method);
    aml_append(var, arg1);
    return var;
}

Aml *aml_call2(const char *method, Aml *arg1, Aml *arg2)
{
    Aml *var = aml_alloc();
    build_append_namestring(var->buf, "%s", method);
    aml_append(var, arg1);
    aml_append(var, arg2);
    return var;
}

Aml *aml_call3(const char *method, Aml *arg1, Aml *arg2, Aml *arg3)
{
    Aml *var = aml_alloc();
    build_append_namestring(var->buf, "%s", method);
    aml_append(var, arg1);
    aml_append(var, arg2);
    aml_append(var, arg3);
    return var;
}

Aml *aml_call4(const char *method, Aml *arg1, Aml *arg2, Aml *arg3, Aml *arg4)
{
    Aml *var = aml_alloc();
    build_append_namestring(var->buf, "%s", method);
    aml_append(var, arg1);
    aml_append(var, arg2);
    aml_append(var, arg3);
    aml_append(var, arg4);
    return var;
}

Aml *aml_call5(const char *method, Aml *arg1, Aml *arg2, Aml *arg3, Aml *arg4,
               Aml *arg5)
{
    Aml *var = aml_alloc();
    build_append_namestring(var->buf, "%s", method);
    aml_append(var, arg1);
    aml_append(var, arg2);
    aml_append(var, arg3);
    aml_append(var, arg4);
    aml_append(var, arg5);
    return var;
}

Aml *aml_call6(const char *method, Aml *arg1, Aml *arg2, Aml *arg3, Aml *arg4,
               Aml *arg5, Aml *arg6)
{
    Aml *var = aml_alloc();
    build_append_namestring(var->buf, "%s", method);
    aml_append(var, arg1);
    aml_append(var, arg2);
    aml_append(var, arg3);
    aml_append(var, arg4);
    aml_append(var, arg5);
    aml_append(var, arg6);
    return var;
}


static Aml *aml_gpio_connection(AmlGpioConnectionType type,
                                AmlConsumerAndProducer con_and_pro,
                                uint8_t flags, AmlPinConfig pin_config,
                                uint16_t output_drive,
                                uint16_t debounce_timeout,
                                const uint32_t pin_list[], uint32_t pin_count,
                                const char *resource_source_name,
                                const uint8_t *vendor_data,
                                uint16_t vendor_data_len)
{
    Aml *var = aml_alloc();
    const uint16_t min_desc_len = 0x16;
    uint16_t resource_source_name_len, length;
    uint16_t pin_table_offset, resource_source_name_offset, vendor_data_offset;
    uint32_t i;

    assert(resource_source_name);
    resource_source_name_len = strlen(resource_source_name) + 1;
    length = min_desc_len + resource_source_name_len + vendor_data_len;
    pin_table_offset = min_desc_len + 1;
    resource_source_name_offset = pin_table_offset + pin_count * 2;
    vendor_data_offset = resource_source_name_offset + resource_source_name_len;

    build_append_byte(var->buf, 0x8C);  /* GPIO Connection Descriptor */
    build_append_int_noprefix(var->buf, length, 2); /* Length */
    build_append_byte(var->buf, 1);     /* Revision ID */
    build_append_byte(var->buf, type);  /* GPIO Connection Type */
    build_append_int_noprefix(var->buf, con_and_pro, 2);
    build_append_int_noprefix(var->buf, flags, 2);
    build_append_byte(var->buf, pin_config);
    build_append_int_noprefix(var->buf, output_drive, 2);
    build_append_int_noprefix(var->buf, debounce_timeout, 2);
    build_append_int_noprefix(var->buf, pin_table_offset, 2);
    build_append_byte(var->buf, 0);     /* Resource Source Index */
    build_append_int_noprefix(var->buf, resource_source_name_offset, 2);
    build_append_int_noprefix(var->buf, vendor_data_offset, 2);
    build_append_int_noprefix(var->buf, vendor_data_len, 2);
    for (i = 0; i < pin_count; i++) {
        build_append_int_noprefix(var->buf, pin_list[i], 2);
    }

    build_append_namestring(var->buf, "%s", resource_source_name);
    build_append_byte(var->buf, '\0');

    if (vendor_data != NULL) {
        g_array_append_vals(var->buf, vendor_data, vendor_data_len);
    }

    return var;
}

Aml *aml_gpio_int(AmlConsumerAndProducer con_and_pro,
                  AmlLevelAndEdge edge_level,
                  AmlActiveHighAndLow active_level, AmlShared shared,
                  AmlPinConfig pin_config, uint16_t debounce_timeout,
                  const uint32_t pin_list[], uint32_t pin_count,
                  const char *resource_source_name,
                  const uint8_t *vendor_data, uint16_t vendor_data_len)
{
    uint8_t flags = edge_level | (active_level << 1) | (shared << 3);

    return aml_gpio_connection(AML_INTERRUPT_CONNECTION, con_and_pro, flags,
                               pin_config, 0, debounce_timeout, pin_list,
                               pin_count, resource_source_name, vendor_data,
                               vendor_data_len);
}

Aml *aml_memory32_fixed(uint32_t addr, uint32_t size,
                        AmlReadAndWrite read_and_write)
{
    Aml *var = aml_alloc();
    build_append_byte(var->buf, 0x86); /* Memory32Fixed Resource Descriptor */
    build_append_byte(var->buf, 9);    /* Length, bits[7:0] value = 9 */
    build_append_byte(var->buf, 0);    /* Length, bits[15:8] value = 0 */
    build_append_byte(var->buf, read_and_write); /* Write status, 1 rw 0 ro */

    build_append_byte(var->buf, extract32(addr, 0, 8));  /* bits[7:0] */
    build_append_byte(var->buf, extract32(addr, 8, 8));  /* bits[15:8] */
    build_append_byte(var->buf, extract32(addr, 16, 8)); /* bits[23:16] */
    build_append_byte(var->buf, extract32(addr, 24, 8)); /* bits[31:24] */

    build_append_byte(var->buf, extract32(size, 0, 8));  /* bits[7:0] */
    build_append_byte(var->buf, extract32(size, 8, 8));  /* bits[15:8] */
    build_append_byte(var->buf, extract32(size, 16, 8)); /* bits[23:16] */
    build_append_byte(var->buf, extract32(size, 24, 8)); /* bits[31:24] */
    return var;
}

Aml *aml_interrupt(AmlConsumerAndProducer con_and_pro,
                   AmlLevelAndEdge level_and_edge,
                   AmlActiveHighAndLow high_and_low, AmlShared shared,
                   uint32_t *irq_list, uint8_t irq_count)
{
    int i;
    Aml *var = aml_alloc();
    uint8_t irq_flags = con_and_pro | (level_and_edge << 1)
                        | (high_and_low << 2) | (shared << 3);
    const int header_bytes_in_len = 2;
    uint16_t len = header_bytes_in_len + irq_count * sizeof(uint32_t);

    assert(irq_count > 0);

    build_append_byte(var->buf, 0x89); /* Extended irq descriptor */
    build_append_byte(var->buf, len & 0xFF); /* Length, bits[7:0] */
    build_append_byte(var->buf, len >> 8); /* Length, bits[15:8] */
    build_append_byte(var->buf, irq_flags); /* Interrupt Vector Information. */
    build_append_byte(var->buf, irq_count);   /* Interrupt table length */

    for (i = 0; i < irq_count; i++) {
        build_append_int_noprefix(var->buf, irq_list[i], 4);
    }
    return var;
}

Aml *aml_io(AmlIODecode dec, uint16_t min_base, uint16_t max_base,
            uint8_t aln, uint8_t len)
{
    Aml *var = aml_alloc();
    build_append_byte(var->buf, 0x47); /* IO port descriptor */
    build_append_byte(var->buf, dec);
    build_append_byte(var->buf, min_base & 0xff);
    build_append_byte(var->buf, (min_base >> 8) & 0xff);
    build_append_byte(var->buf, max_base & 0xff);
    build_append_byte(var->buf, (max_base >> 8) & 0xff);
    build_append_byte(var->buf, aln);
    build_append_byte(var->buf, len);
    return var;
}

Aml *aml_irq_no_flags(uint8_t irq)
{
    uint16_t irq_mask;
    Aml *var = aml_alloc();

    assert(irq < 16);
    build_append_byte(var->buf, 0x22); /* IRQ descriptor 2 byte form */

    irq_mask = 1U << irq;
    build_append_byte(var->buf, irq_mask & 0xFF); /* IRQ mask bits[7:0] */
    build_append_byte(var->buf, irq_mask >> 8); /* IRQ mask bits[15:8] */
    return var;
}

Aml *aml_lnot(Aml *arg)
{
    Aml *var = aml_opcode(0x92 /* LNotOp */);
    aml_append(var, arg);
    return var;
}

Aml *aml_equal(Aml *arg1, Aml *arg2)
{
    Aml *var = aml_opcode(0x93 /* LequalOp */);
    aml_append(var, arg1);
    aml_append(var, arg2);
    return var;
}

Aml *aml_lgreater(Aml *arg1, Aml *arg2)
{
    Aml *var = aml_opcode(0x94 /* LGreaterOp */);
    aml_append(var, arg1);
    aml_append(var, arg2);
    return var;
}

Aml *aml_lgreater_equal(Aml *arg1, Aml *arg2)
{
    Aml *var = aml_opcode(0x92 /* LNotOp */);
    build_append_byte(var->buf, 0x95 /* LLessOp */);
    aml_append(var, arg1);
    aml_append(var, arg2);
    return var;
}

Aml *aml_if(Aml *predicate)
{
    Aml *var = aml_bundle(0xA0 /* IfOp */, AML_PACKAGE);
    aml_append(var, predicate);
    return var;
}

Aml *aml_else(void)
{
    Aml *var = aml_bundle(0xA1 /* ElseOp */, AML_PACKAGE);
    return var;
}

Aml *aml_while(Aml *predicate)
{
    Aml *var = aml_bundle(0xA2 /* WhileOp */, AML_PACKAGE);
    aml_append(var, predicate);
    return var;
}

Aml *aml_method(const char *name, int arg_count, AmlSerializeFlag sflag)
{
    Aml *var = aml_bundle(0x14 /* MethodOp */, AML_PACKAGE);
    int methodflags;

    assert(arg_count < 8);
    methodflags = arg_count | (sflag << 3);

    build_append_namestring(var->buf, "%s", name);
    build_append_byte(var->buf, methodflags); /* MethodFlags: ArgCount */
    return var;
}

Aml *aml_device(const char *name_format, ...)
{
    va_list ap;
    Aml *var = aml_bundle(0x82 /* DeviceOp */, AML_EXT_PACKAGE);
    va_start(ap, name_format);
    build_append_namestringv(var->buf, name_format, ap);
    va_end(ap);
    return var;
}

Aml *aml_resource_template(void)
{
    Aml *var = aml_bundle(0x11 /* BufferOp */, AML_RES_TEMPLATE);
    return var;
}

Aml *aml_buffer(int buffer_size, uint8_t *byte_list)
{
    int i;
    Aml *var = aml_bundle(0x11 /* BufferOp */, AML_BUFFER);

    for (i = 0; i < buffer_size; i++) {
        if (byte_list == NULL) {
            build_append_byte(var->buf, 0x0);
        } else {
            build_append_byte(var->buf, byte_list[i]);
        }
    }

    return var;
}

Aml *aml_package(uint8_t num_elements)
{
    Aml *var = aml_bundle(0x12 /* PackageOp */, AML_PACKAGE);
    build_append_byte(var->buf, num_elements);
    return var;
}

Aml *aml_operation_region(const char *name, AmlRegionSpace rs,
                          Aml *offset, uint32_t len)
{
    Aml *var = aml_alloc();
    build_append_byte(var->buf, 0x5B); /* ExtOpPrefix */
    build_append_byte(var->buf, 0x80); /* OpRegionOp */
    build_append_namestring(var->buf, "%s", name);
    build_append_byte(var->buf, rs);
    aml_append(var, offset);
    build_append_int(var->buf, len);
    return var;
}

Aml *aml_named_field(const char *name, unsigned length)
{
    Aml *var = aml_alloc();
    build_append_nameseg(var->buf, name);
    build_append_pkg_length(var->buf, length, false);
    return var;
}

Aml *aml_reserved_field(unsigned length)
{
    Aml *var = aml_alloc();
    build_append_byte(var->buf, 0x00);
    build_append_pkg_length(var->buf, length, false);
    return var;
}

Aml *aml_field(const char *name, AmlAccessType type, AmlLockRule lock,
               AmlUpdateRule rule)
{
    Aml *var = aml_bundle(0x81 /* FieldOp */, AML_EXT_PACKAGE);
    uint8_t flags = rule << 5 | type;

    flags |= lock << 4; /* LockRule at 4 bit offset */

    build_append_namestring(var->buf, "%s", name);
    build_append_byte(var->buf, flags);
    return var;
}

static
Aml *create_field_common(int opcode, Aml *srcbuf, Aml *index, const char *name)
{
    Aml *var = aml_opcode(opcode);
    aml_append(var, srcbuf);
    aml_append(var, index);
    build_append_namestring(var->buf, "%s", name);
    return var;
}

Aml *aml_create_field(Aml *srcbuf, Aml *bit_index, Aml *num_bits,
                      const char *name)
{
    Aml *var = aml_alloc();
    build_append_byte(var->buf, 0x5B); /* ExtOpPrefix */
    build_append_byte(var->buf, 0x13); /* CreateFieldOp */
    aml_append(var, srcbuf);
    aml_append(var, bit_index);
    aml_append(var, num_bits);
    build_append_namestring(var->buf, "%s", name);
    return var;
}

Aml *aml_create_dword_field(Aml *srcbuf, Aml *index, const char *name)
{
    return create_field_common(0x8A /* CreateDWordFieldOp */,
                               srcbuf, index, name);
}

Aml *aml_create_qword_field(Aml *srcbuf, Aml *index, const char *name)
{
    return create_field_common(0x8F /* CreateQWordFieldOp */,
                               srcbuf, index, name);
}

Aml *aml_string(const char *name_format, ...)
{
    Aml *var = aml_opcode(0x0D /* StringPrefix */);
    va_list ap;
    char *s;
    int len;

    va_start(ap, name_format);
    len = g_vasprintf(&s, name_format, ap);
    va_end(ap);

    g_array_append_vals(var->buf, s, len + 1);
    g_free(s);

    return var;
}

Aml *aml_local(int num)
{
    uint8_t op = 0x60 /* Local0Op */ + num;

    assert(num <= 7);
    return aml_opcode(op);
}

Aml *aml_varpackage(uint32_t num_elements)
{
    Aml *var = aml_bundle(0x13 /* VarPackageOp */, AML_PACKAGE);
    build_append_int(var->buf, num_elements);
    return var;
}

Aml *aml_processor(uint8_t proc_id, uint32_t pblk_addr, uint8_t pblk_len,
                   const char *name_format, ...)
{
    va_list ap;
    Aml *var = aml_bundle(0x83 /* ProcessorOp */, AML_EXT_PACKAGE);
    va_start(ap, name_format);
    build_append_namestringv(var->buf, name_format, ap);
    va_end(ap);
    build_append_byte(var->buf, proc_id); /* ProcID */
    build_append_int_noprefix(var->buf, pblk_addr, sizeof(pblk_addr));
    build_append_byte(var->buf, pblk_len); /* PblkLen */
    return var;
}

static uint8_t Hex2Digit(char c)
{
    if (c >= 'A') {
        return c - 'A' + 10;
    }

    return c - '0';
}

Aml *aml_eisaid(const char *str)
{
    Aml *var = aml_alloc();
    uint32_t id;

    g_assert(strlen(str) == 7);
    id = (str[0] - 0x40) << 26 |
    (str[1] - 0x40) << 21 |
    (str[2] - 0x40) << 16 |
    Hex2Digit(str[3]) << 12 |
    Hex2Digit(str[4]) << 8 |
    Hex2Digit(str[5]) << 4 |
    Hex2Digit(str[6]);

    build_append_byte(var->buf, 0x0C); /* DWordPrefix */
    build_append_int_noprefix(var->buf, bswap32(id), sizeof(id));
    return var;
}

static Aml *aml_as_desc_header(AmlResourceType type, AmlMinFixed min_fixed,
                               AmlMaxFixed max_fixed, AmlDecode dec,
                               uint8_t type_flags)
{
    uint8_t flags = max_fixed | min_fixed | dec;
    Aml *var = aml_alloc();

    build_append_byte(var->buf, type);
    build_append_byte(var->buf, flags);
    build_append_byte(var->buf, type_flags); /* Type Specific Flags */
    return var;
}

static Aml *aml_word_as_desc(AmlResourceType type, AmlMinFixed min_fixed,
                             AmlMaxFixed max_fixed, AmlDecode dec,
                             uint16_t addr_gran, uint16_t addr_min,
                             uint16_t addr_max, uint16_t addr_trans,
                             uint16_t len, uint8_t type_flags)
{
    Aml *var = aml_alloc();

    build_append_byte(var->buf, 0x88); /* Word Address Space Descriptor */
    build_append_byte(var->buf, 0x0D);
    build_append_byte(var->buf, 0x0);

    aml_append(var,
        aml_as_desc_header(type, min_fixed, max_fixed, dec, type_flags));
    build_append_int_noprefix(var->buf, addr_gran, sizeof(addr_gran));
    build_append_int_noprefix(var->buf, addr_min, sizeof(addr_min));
    build_append_int_noprefix(var->buf, addr_max, sizeof(addr_max));
    build_append_int_noprefix(var->buf, addr_trans, sizeof(addr_trans));
    build_append_int_noprefix(var->buf, len, sizeof(len));
    return var;
}

static Aml *aml_dword_as_desc(AmlResourceType type, AmlMinFixed min_fixed,
                              AmlMaxFixed max_fixed, AmlDecode dec,
                              uint32_t addr_gran, uint32_t addr_min,
                              uint32_t addr_max, uint32_t addr_trans,
                              uint32_t len, uint8_t type_flags)
{
    Aml *var = aml_alloc();

    build_append_byte(var->buf, 0x87); /* DWord Address Space Descriptor */
    build_append_byte(var->buf, 23);
    build_append_byte(var->buf, 0x0);


    aml_append(var,
        aml_as_desc_header(type, min_fixed, max_fixed, dec, type_flags));
    build_append_int_noprefix(var->buf, addr_gran, sizeof(addr_gran));
    build_append_int_noprefix(var->buf, addr_min, sizeof(addr_min));
    build_append_int_noprefix(var->buf, addr_max, sizeof(addr_max));
    build_append_int_noprefix(var->buf, addr_trans, sizeof(addr_trans));
    build_append_int_noprefix(var->buf, len, sizeof(len));
    return var;
}

static Aml *aml_qword_as_desc(AmlResourceType type, AmlMinFixed min_fixed,
                              AmlMaxFixed max_fixed, AmlDecode dec,
                              uint64_t addr_gran, uint64_t addr_min,
                              uint64_t addr_max, uint64_t addr_trans,
                              uint64_t len, uint8_t type_flags)
{
    Aml *var = aml_alloc();

    build_append_byte(var->buf, 0x8A); /* QWord Address Space Descriptor */
    build_append_byte(var->buf, 0x2B);
    build_append_byte(var->buf, 0x0);

    aml_append(var,
        aml_as_desc_header(type, min_fixed, max_fixed, dec, type_flags));
    build_append_int_noprefix(var->buf, addr_gran, sizeof(addr_gran));
    build_append_int_noprefix(var->buf, addr_min, sizeof(addr_min));
    build_append_int_noprefix(var->buf, addr_max, sizeof(addr_max));
    build_append_int_noprefix(var->buf, addr_trans, sizeof(addr_trans));
    build_append_int_noprefix(var->buf, len, sizeof(len));
    return var;
}

Aml *aml_word_bus_number(AmlMinFixed min_fixed, AmlMaxFixed max_fixed,
                         AmlDecode dec, uint16_t addr_gran,
                         uint16_t addr_min, uint16_t addr_max,
                         uint16_t addr_trans, uint16_t len)

{
    return aml_word_as_desc(AML_BUS_NUMBER_RANGE, min_fixed, max_fixed, dec,
                            addr_gran, addr_min, addr_max, addr_trans, len, 0);
}

Aml *aml_word_io(AmlMinFixed min_fixed, AmlMaxFixed max_fixed,
                 AmlDecode dec, AmlISARanges isa_ranges,
                 uint16_t addr_gran, uint16_t addr_min,
                 uint16_t addr_max, uint16_t addr_trans,
                 uint16_t len)

{
    return aml_word_as_desc(AML_IO_RANGE, min_fixed, max_fixed, dec,
                            addr_gran, addr_min, addr_max, addr_trans, len,
                            isa_ranges);
}

Aml *aml_dword_io(AmlMinFixed min_fixed, AmlMaxFixed max_fixed,
                 AmlDecode dec, AmlISARanges isa_ranges,
                 uint32_t addr_gran, uint32_t addr_min,
                 uint32_t addr_max, uint32_t addr_trans,
                 uint32_t len)

{
    return aml_dword_as_desc(AML_IO_RANGE, min_fixed, max_fixed, dec,
                            addr_gran, addr_min, addr_max, addr_trans, len,
                            isa_ranges);
}

Aml *aml_dword_memory(AmlDecode dec, AmlMinFixed min_fixed,
                      AmlMaxFixed max_fixed, AmlCacheable cacheable,
                      AmlReadAndWrite read_and_write,
                      uint32_t addr_gran, uint32_t addr_min,
                      uint32_t addr_max, uint32_t addr_trans,
                      uint32_t len)
{
    uint8_t flags = read_and_write | (cacheable << 1);

    return aml_dword_as_desc(AML_MEMORY_RANGE, min_fixed, max_fixed,
                             dec, addr_gran, addr_min, addr_max,
                             addr_trans, len, flags);
}

Aml *aml_qword_memory(AmlDecode dec, AmlMinFixed min_fixed,
                      AmlMaxFixed max_fixed, AmlCacheable cacheable,
                      AmlReadAndWrite read_and_write,
                      uint64_t addr_gran, uint64_t addr_min,
                      uint64_t addr_max, uint64_t addr_trans,
                      uint64_t len)
{
    uint8_t flags = read_and_write | (cacheable << 1);

    return aml_qword_as_desc(AML_MEMORY_RANGE, min_fixed, max_fixed,
                             dec, addr_gran, addr_min, addr_max,
                             addr_trans, len, flags);
}

Aml *aml_dma(AmlDmaType typ, AmlDmaBusMaster bm, AmlTransferSize sz,
             uint8_t channel)
{
    Aml *var = aml_alloc();
    uint8_t flags = sz | bm << 2 | typ << 5;

    assert(channel < 8);
    build_append_byte(var->buf, 0x2A);    /* Byte 0: DMA Descriptor */
    build_append_byte(var->buf, 1U << channel); /* Byte 1: _DMA - DmaChannel */
    build_append_byte(var->buf, flags);   /* Byte 2 */
    return var;
}

Aml *aml_sleep(uint64_t msec)
{
    Aml *var = aml_alloc();
    build_append_byte(var->buf, 0x5B); /* ExtOpPrefix */
    build_append_byte(var->buf, 0x22); /* SleepOp */
    aml_append(var, aml_int(msec));
    return var;
}

static uint8_t Hex2Byte(const char *src)
{
    int hi, lo;

    hi = Hex2Digit(src[0]);
    assert(hi >= 0);
    assert(hi <= 15);

    lo = Hex2Digit(src[1]);
    assert(lo >= 0);
    assert(lo <= 15);
    return (hi << 4) | lo;
}

Aml *aml_touuid(const char *uuid)
{
    Aml *var = aml_bundle(0x11 /* BufferOp */, AML_BUFFER);

    assert(strlen(uuid) == 36);
    assert(uuid[8] == '-');
    assert(uuid[13] == '-');
    assert(uuid[18] == '-');
    assert(uuid[23] == '-');

    build_append_byte(var->buf, Hex2Byte(uuid + 6));  /* dd - at offset 00 */
    build_append_byte(var->buf, Hex2Byte(uuid + 4));  /* cc - at offset 01 */
    build_append_byte(var->buf, Hex2Byte(uuid + 2));  /* bb - at offset 02 */
    build_append_byte(var->buf, Hex2Byte(uuid + 0));  /* aa - at offset 03 */

    build_append_byte(var->buf, Hex2Byte(uuid + 11)); /* ff - at offset 04 */
    build_append_byte(var->buf, Hex2Byte(uuid + 9));  /* ee - at offset 05 */

    build_append_byte(var->buf, Hex2Byte(uuid + 16)); /* hh - at offset 06 */
    build_append_byte(var->buf, Hex2Byte(uuid + 14)); /* gg - at offset 07 */

    build_append_byte(var->buf, Hex2Byte(uuid + 19)); /* ii - at offset 08 */
    build_append_byte(var->buf, Hex2Byte(uuid + 21)); /* jj - at offset 09 */

    build_append_byte(var->buf, Hex2Byte(uuid + 24)); /* kk - at offset 10 */
    build_append_byte(var->buf, Hex2Byte(uuid + 26)); /* ll - at offset 11 */
    build_append_byte(var->buf, Hex2Byte(uuid + 28)); /* mm - at offset 12 */
    build_append_byte(var->buf, Hex2Byte(uuid + 30)); /* nn - at offset 13 */
    build_append_byte(var->buf, Hex2Byte(uuid + 32)); /* oo - at offset 14 */
    build_append_byte(var->buf, Hex2Byte(uuid + 34)); /* pp - at offset 15 */

    return var;
}

Aml *aml_unicode(const char *str)
{
    int i = 0;
    Aml *var = aml_bundle(0x11 /* BufferOp */, AML_BUFFER);

    do {
        build_append_byte(var->buf, str[i]);
        build_append_byte(var->buf, 0);
        i++;
    } while (i <= strlen(str));

    return var;
}

Aml *aml_refof(Aml *arg)
{
    Aml *var = aml_opcode(0x71 /* RefOfOp */);
    aml_append(var, arg);
    return var;
}

Aml *aml_derefof(Aml *arg)
{
    Aml *var = aml_opcode(0x83 /* DerefOfOp */);
    aml_append(var, arg);
    return var;
}

Aml *aml_sizeof(Aml *arg)
{
    Aml *var = aml_opcode(0x87 /* SizeOfOp */);
    aml_append(var, arg);
    return var;
}

Aml *aml_mutex(const char *name, uint8_t sync_level)
{
    Aml *var = aml_alloc();
    build_append_byte(var->buf, 0x5B); /* ExtOpPrefix */
    build_append_byte(var->buf, 0x01); /* MutexOp */
    build_append_namestring(var->buf, "%s", name);
    assert(!(sync_level & 0xF0));
    build_append_byte(var->buf, sync_level);
    return var;
}

Aml *aml_acquire(Aml *mutex, uint16_t timeout)
{
    Aml *var = aml_alloc();
    build_append_byte(var->buf, 0x5B); /* ExtOpPrefix */
    build_append_byte(var->buf, 0x23); /* AcquireOp */
    aml_append(var, mutex);
    build_append_int_noprefix(var->buf, timeout, sizeof(timeout));
    return var;
}

Aml *aml_release(Aml *mutex)
{
    Aml *var = aml_alloc();
    build_append_byte(var->buf, 0x5B); /* ExtOpPrefix */
    build_append_byte(var->buf, 0x27); /* ReleaseOp */
    aml_append(var, mutex);
    return var;
}

Aml *aml_alias(const char *source_object, const char *alias_object)
{
    Aml *var = aml_opcode(0x06 /* AliasOp */);
    aml_append(var, aml_name("%s", source_object));
    aml_append(var, aml_name("%s", alias_object));
    return var;
}

Aml *aml_concatenate(Aml *source1, Aml *source2, Aml *target)
{
    return build_opcode_2arg_dst(0x73 /* ConcatOp */, source1, source2,
                                 target);
}

Aml *aml_object_type(Aml *object)
{
    Aml *var = aml_opcode(0x8E /* ObjectTypeOp */);
    aml_append(var, object);
    return var;
}

void acpi_table_begin(AcpiTable *desc, GArray *array)
{

    desc->array = array;
    desc->table_offset = array->len;

    g_assert(strlen(desc->sig) == 4);
    g_array_append_vals(array, desc->sig, 4); /* Signature */
    build_append_int_noprefix(array, 0, 4); /* Length */
    build_append_int_noprefix(array, desc->rev, 1); /* Revision */
    build_append_int_noprefix(array, 0, 1); /* Checksum */
    build_append_padded_str(array, desc->oem_id, 6, '\0'); /* OEMID */
    build_append_padded_str(array, desc->oem_table_id, 8, '\0');
    build_append_int_noprefix(array, 1, 4); /* OEM Revision */
    g_array_append_vals(array, ACPI_BUILD_APPNAME8, 4); /* Creator ID */
    build_append_int_noprefix(array, 1, 4); /* Creator Revision */
}

void acpi_table_end(BIOSLinker *linker, AcpiTable *desc)
{
    const unsigned checksum_offset = 9;
    uint32_t table_len = desc->array->len - desc->table_offset;
    uint32_t table_len_le = cpu_to_le32(table_len);
    gchar *len_ptr = &desc->array->data[desc->table_offset + 4];

    memcpy(len_ptr, &table_len_le, sizeof table_len_le);

    bios_linker_loader_add_checksum(linker, ACPI_BUILD_TABLE_FILE,
        desc->table_offset, table_len, desc->table_offset + checksum_offset);
}

void *acpi_data_push(GArray *table_data, unsigned size)
{
    unsigned off = table_data->len;
    g_array_set_size(table_data, off + size);
    return table_data->data + off;
}

unsigned acpi_data_len(GArray *table)
{
    assert(g_array_get_element_size(table) == 1);
    return table->len;
}

void acpi_add_table(GArray *table_offsets, GArray *table_data)
{
    uint32_t offset = table_data->len;
    g_array_append_val(table_offsets, offset);
}

void acpi_build_tables_init(AcpiBuildTables *tables)
{
    tables->rsdp = g_array_new(false, true /* clear */, 1);
    tables->table_data = g_array_new(false, true /* clear */, 1);
    tables->tcpalog = g_array_new(false, true /* clear */, 1);
    tables->vmgenid = g_array_new(false, true /* clear */, 1);
    tables->hardware_errors = g_array_new(false, true /* clear */, 1);
    tables->linker = bios_linker_loader_init();
}

void acpi_build_tables_cleanup(AcpiBuildTables *tables, bool mfre)
{
    bios_linker_loader_cleanup(tables->linker);
    g_array_free(tables->rsdp, true);
    g_array_free(tables->table_data, true);
    g_array_free(tables->tcpalog, mfre);
    g_array_free(tables->vmgenid, mfre);
    g_array_free(tables->hardware_errors, mfre);
}

void
build_rsdp(GArray *tbl, BIOSLinker *linker, AcpiRsdpData *rsdp_data)
{
    int tbl_off = tbl->len; /* Table offset in the RSDP file */

    switch (rsdp_data->revision) {
    case 0:
        g_assert(rsdp_data->rsdt_tbl_offset);
        break;
    case 2:
        g_assert(rsdp_data->xsdt_tbl_offset);
        break;
    default:
        g_assert_not_reached();
    }

    bios_linker_loader_alloc(linker, ACPI_BUILD_RSDP_FILE, tbl, 16,
                             true /* fseg memory */);

    g_array_append_vals(tbl, "RSD PTR ", 8); /* Signature */
    build_append_int_noprefix(tbl, 0, 1); /* Checksum */
    g_array_append_vals(tbl, rsdp_data->oem_id, 6); /* OEMID */
    build_append_int_noprefix(tbl, rsdp_data->revision, 1); /* Revision */
    build_append_int_noprefix(tbl, 0, 4); /* RsdtAddress */
    if (rsdp_data->rsdt_tbl_offset) {
        bios_linker_loader_add_pointer(linker, ACPI_BUILD_RSDP_FILE,
                                       tbl_off + 16, 4,
                                       ACPI_BUILD_TABLE_FILE,
                                       *rsdp_data->rsdt_tbl_offset);
    }

    bios_linker_loader_add_checksum(linker, ACPI_BUILD_RSDP_FILE,
                                    tbl_off, 20, /* ACPI rev 1.0 RSDP size */
                                    8);

    if (rsdp_data->revision == 0) {
        return;
    }

    build_append_int_noprefix(tbl, 36, 4); /* Length */

    build_append_int_noprefix(tbl, 0, 8); /* XsdtAddress */
    bios_linker_loader_add_pointer(linker, ACPI_BUILD_RSDP_FILE,
                                   tbl_off + 24, 8,
                                   ACPI_BUILD_TABLE_FILE,
                                   *rsdp_data->xsdt_tbl_offset);

    build_append_int_noprefix(tbl, 0, 1); /* Extended Checksum */
    build_append_int_noprefix(tbl, 0, 3); /* Reserved */

    bios_linker_loader_add_checksum(linker, ACPI_BUILD_RSDP_FILE,
                                    tbl_off, 36, /* ACPI rev 2.0 RSDP size */
                                    32);
}

void
build_rsdt(GArray *table_data, BIOSLinker *linker, GArray *table_offsets,
           const char *oem_id, const char *oem_table_id)
{
    int i;
    AcpiTable table = { .sig = "RSDT", .rev = 1,
                        .oem_id = oem_id, .oem_table_id = oem_table_id };

    acpi_table_begin(&table, table_data);
    for (i = 0; i < table_offsets->len; ++i) {
        uint32_t ref_tbl_offset = g_array_index(table_offsets, uint32_t, i);
        uint32_t rsdt_entry_offset = table.array->len;

        build_append_int_noprefix(table.array, 0, 4);

        bios_linker_loader_add_pointer(linker,
            ACPI_BUILD_TABLE_FILE, rsdt_entry_offset, 4,
            ACPI_BUILD_TABLE_FILE, ref_tbl_offset);

    }
    acpi_table_end(linker, &table);
}

void
build_xsdt(GArray *table_data, BIOSLinker *linker, GArray *table_offsets,
           const char *oem_id, const char *oem_table_id)
{
    int i;
    AcpiTable table = { .sig = "XSDT", .rev = 1,
                        .oem_id = oem_id, .oem_table_id = oem_table_id };

    acpi_table_begin(&table, table_data);

    for (i = 0; i < table_offsets->len; ++i) {
        uint64_t ref_tbl_offset = g_array_index(table_offsets, uint32_t, i);
        uint64_t xsdt_entry_offset = table.array->len;

        build_append_int_noprefix(table.array, 0, 8);

        bios_linker_loader_add_pointer(linker,
            ACPI_BUILD_TABLE_FILE, xsdt_entry_offset, 8,
            ACPI_BUILD_TABLE_FILE, ref_tbl_offset);
    }
    acpi_table_end(linker, &table);
}

void build_srat_memory(GArray *table_data, uint64_t base,
                       uint64_t len, int node, MemoryAffinityFlags flags)
{
    build_append_int_noprefix(table_data, 1, 1); /* Type */
    build_append_int_noprefix(table_data, 40, 1); /* Length */
    build_append_int_noprefix(table_data, node, 4); /* Proximity Domain */
    build_append_int_noprefix(table_data, 0, 2); /* Reserved */
    build_append_int_noprefix(table_data, base, 4); /* Base Address Low */
    build_append_int_noprefix(table_data, base >> 32, 4);
    build_append_int_noprefix(table_data, len, 4); /* Length Low */
    build_append_int_noprefix(table_data, len >> 32, 4); /* Length High */
    build_append_int_noprefix(table_data, 0, 4); /* Reserved */
    build_append_int_noprefix(table_data, flags, 4); /* Flags */
    build_append_int_noprefix(table_data, 0, 8); /* Reserved */
}

static void build_append_srat_pci_device_handle(GArray *table_data,
                                                uint16_t segment,
                                                uint8_t bus, uint8_t devfn)
{
    build_append_int_noprefix(table_data, segment, 2);
    build_append_int_noprefix(table_data, bus, 1);
    build_append_int_noprefix(table_data, devfn, 1);
    build_append_int_noprefix(table_data, 0, 12);
}

static void build_append_srat_acpi_device_handle(GArray *table_data,
                                                 const char *hid,
                                                 uint32_t uid)
{
    assert(strlen(hid) == 8);
    for (int i = 0; i < 8; i++) {
        build_append_int_noprefix(table_data, hid[i], 1);
    }
    build_append_int_noprefix(table_data, uid, 4);
    build_append_int_noprefix(table_data, 0, 4);
}

void build_srat_pci_generic_initiator(GArray *table_data, uint32_t node,
                                      uint16_t segment, uint8_t bus,
                                      uint8_t devfn)
{
    build_append_int_noprefix(table_data, 5, 1);
    build_append_int_noprefix(table_data, 32, 1);
    build_append_int_noprefix(table_data, 0, 1);
    build_append_int_noprefix(table_data, 1, 1);
    build_append_int_noprefix(table_data, node, 4);
    build_append_srat_pci_device_handle(table_data, segment, bus, devfn);
    build_append_int_noprefix(table_data, 1, 4);
    build_append_int_noprefix(table_data, 0, 4);
}

void build_srat_acpi_generic_port(GArray *table_data, uint32_t node,
                                  const char *hid, uint32_t uid)
{
    build_append_int_noprefix(table_data, 6, 1);
    build_append_int_noprefix(table_data, 32, 1);
    build_append_int_noprefix(table_data, 0, 1);
    build_append_int_noprefix(table_data, 0, 1);
    build_append_int_noprefix(table_data, node, 4);
    build_append_srat_acpi_device_handle(table_data, hid, uid);
    build_append_int_noprefix(table_data, 1, 4);
    build_append_int_noprefix(table_data, 0, 4);
}

void build_slit(GArray *table_data, BIOSLinker *linker, MachineState *ms,
                const char *oem_id, const char *oem_table_id)
{
    int i, j;
    int nb_numa_nodes = ms->numa_state->num_nodes;
    AcpiTable table = { .sig = "SLIT", .rev = 1,
                        .oem_id = oem_id, .oem_table_id = oem_table_id };

    acpi_table_begin(&table, table_data);

    build_append_int_noprefix(table_data, nb_numa_nodes, 8);
    for (i = 0; i < nb_numa_nodes; i++) {
        for (j = 0; j < nb_numa_nodes; j++) {
            assert(ms->numa_state->nodes[i].distance[j]);
            build_append_int_noprefix(table_data,
                                      ms->numa_state->nodes[i].distance[j],
                                      1);
        }
    }
    acpi_table_end(linker, &table);
}

static void build_processor_hierarchy_node(GArray *tbl, uint32_t flags,
                                           uint32_t parent, uint32_t id,
                                           uint32_t *priv_rsrc,
                                           uint32_t priv_num)
{
    int i;

    build_append_byte(tbl, 0);                 /* Type 0 - processor */
    build_append_byte(tbl, 20 + priv_num * 4); /* Length */
    build_append_int_noprefix(tbl, 0, 2);      /* Reserved */
    build_append_int_noprefix(tbl, flags, 4);  /* Flags */
    build_append_int_noprefix(tbl, parent, 4); /* Parent */
    build_append_int_noprefix(tbl, id, 4);     /* ACPI Processor ID */

    build_append_int_noprefix(tbl, priv_num, 4);

    if (priv_num > 0) {
        assert(priv_rsrc);
        for (i = 0; i < priv_num; i++) {
            build_append_int_noprefix(tbl, priv_rsrc[i], 4);
        }
    }
}

void build_spcr(GArray *table_data, BIOSLinker *linker,
                const AcpiSpcrData *f, const uint8_t rev,
                const char *oem_id, const char *oem_table_id, const char *name)
{
    AcpiTable table = { .sig = "SPCR", .rev = rev, .oem_id = oem_id,
                        .oem_table_id = oem_table_id };

    acpi_table_begin(&table, table_data);
    build_append_int_noprefix(table_data, f->interface_type, 1);
    build_append_int_noprefix(table_data, 0, 3);
    build_append_gas(table_data, f->base_addr.id, f->base_addr.width,
                     f->base_addr.offset, f->base_addr.size,
                     f->base_addr.addr);
    build_append_int_noprefix(table_data, f->interrupt_type, 1);
    build_append_int_noprefix(table_data, f->pc_interrupt, 1);
    build_append_int_noprefix(table_data, f->interrupt, 4);
    build_append_int_noprefix(table_data, f->baud_rate, 1);
    build_append_int_noprefix(table_data, f->parity, 1);
    build_append_int_noprefix(table_data, f->stop_bits, 1);
    build_append_int_noprefix(table_data, f->flow_control, 1);
    build_append_int_noprefix(table_data, f->language, 1);
    build_append_int_noprefix(table_data, f->terminal_type, 1);
    build_append_int_noprefix(table_data, f->pci_device_id, 2);
    build_append_int_noprefix(table_data, f->pci_vendor_id, 2);
    build_append_int_noprefix(table_data, f->pci_bus, 1);
    build_append_int_noprefix(table_data, f->pci_device, 1);
    build_append_int_noprefix(table_data, f->pci_function, 1);
    build_append_int_noprefix(table_data, f->pci_flags, 4);
    build_append_int_noprefix(table_data, f->pci_segment, 1);
    if (rev < 4) {
        build_append_int_noprefix(table_data, 0, 4);
    } else {
        build_append_int_noprefix(table_data, f->uart_clk_freq, 4);
        build_append_int_noprefix(table_data, f->precise_baudrate, 4);
        build_append_int_noprefix(table_data, f->namespace_string_length, 2);
        build_append_int_noprefix(table_data, f->namespace_string_offset, 2);
        g_array_append_vals(table_data, name, f->namespace_string_length);
    }
    acpi_table_end(linker, &table);
}
void build_pptt(GArray *table_data, BIOSLinker *linker, MachineState *ms,
                const char *oem_id, const char *oem_table_id)
{
    MachineClass *mc = MACHINE_GET_CLASS(ms);
    CPUArchIdList *cpus = ms->possible_cpus;
    int64_t socket_id = -1, cluster_id = -1, core_id = -1;
    uint32_t socket_offset = 0, cluster_offset = 0, core_offset = 0;
    uint32_t pptt_start = table_data->len;
    int n;
    AcpiTable table = { .sig = "PPTT", .rev = 2,
                        .oem_id = oem_id, .oem_table_id = oem_table_id };

    acpi_table_begin(&table, table_data);

    for (n = 0; n < cpus->len; n++) {
        if (cpus->cpus[n].props.socket_id != socket_id) {
            assert(cpus->cpus[n].props.socket_id > socket_id);
            socket_id = cpus->cpus[n].props.socket_id;
            cluster_id = -1;
            core_id = -1;
            socket_offset = table_data->len - pptt_start;
            build_processor_hierarchy_node(table_data,
                (1 << 0), /* Physical package */
                0, socket_id, NULL, 0);
        }

        if (mc->smp_props.clusters_supported && mc->smp_props.has_clusters) {
            if (cpus->cpus[n].props.cluster_id != cluster_id) {
                assert(cpus->cpus[n].props.cluster_id > cluster_id);
                cluster_id = cpus->cpus[n].props.cluster_id;
                core_id = -1;
                cluster_offset = table_data->len - pptt_start;
                build_processor_hierarchy_node(table_data,
                    (0 << 0), /* Not a physical package */
                    socket_offset, cluster_id, NULL, 0);
            }
        } else {
            cluster_offset = socket_offset;
        }

        if (ms->smp.threads == 1) {
            build_processor_hierarchy_node(table_data,
                (1 << 1) | /* ACPI Processor ID valid */
                (1 << 3),  /* Node is a Leaf */
                cluster_offset, n, NULL, 0);
        } else {
            if (cpus->cpus[n].props.core_id != core_id) {
                assert(cpus->cpus[n].props.core_id > core_id);
                core_id = cpus->cpus[n].props.core_id;
                core_offset = table_data->len - pptt_start;
                build_processor_hierarchy_node(table_data,
                    (0 << 0), /* Not a physical package */
                    cluster_offset, core_id, NULL, 0);
            }

            build_processor_hierarchy_node(table_data,
                (1 << 1) | /* ACPI Processor ID valid */
                (1 << 2) | /* Processor is a Thread */
                (1 << 3),  /* Node is a Leaf */
                core_offset, n, NULL, 0);
        }
    }

    acpi_table_end(linker, &table);
}

void build_fadt(GArray *tbl, BIOSLinker *linker, const AcpiFadtData *f,
                const char *oem_id, const char *oem_table_id)
{
    int off;
    AcpiTable table = { .sig = "FACP", .rev = f->rev,
                        .oem_id = oem_id, .oem_table_id = oem_table_id };

    acpi_table_begin(&table, tbl);

    off = tbl->len;
    build_append_int_noprefix(tbl, 0, 4); /* FIRMWARE_CTRL */
    if (f->facs_tbl_offset) { /* don't patch if not supported by platform */
        bios_linker_loader_add_pointer(linker,
            ACPI_BUILD_TABLE_FILE, off, 4,
            ACPI_BUILD_TABLE_FILE, *f->facs_tbl_offset);
    }

    off = tbl->len;
    build_append_int_noprefix(tbl, 0, 4); /* DSDT */
    if (f->dsdt_tbl_offset) { /* don't patch if not supported by platform */
        bios_linker_loader_add_pointer(linker,
            ACPI_BUILD_TABLE_FILE, off, 4,
            ACPI_BUILD_TABLE_FILE, *f->dsdt_tbl_offset);
    }

    build_append_int_noprefix(tbl, f->int_model /* Multiple APIC */, 1);
    build_append_int_noprefix(tbl, 0 /* Unspecified */, 1);
    build_append_int_noprefix(tbl, f->sci_int, 2); /* SCI_INT */
    build_append_int_noprefix(tbl, f->smi_cmd, 4); /* SMI_CMD */
    build_append_int_noprefix(tbl, f->acpi_enable_cmd, 1); /* ACPI_ENABLE */
    build_append_int_noprefix(tbl, f->acpi_disable_cmd, 1); /* ACPI_DISABLE */
    build_append_int_noprefix(tbl, 0 /* not supported */, 1); /* S4BIOS_REQ */
    build_append_int_noprefix(tbl, 0, 1);
    build_append_int_noprefix(tbl, f->pm1a_evt.address, 4); /* PM1a_EVT_BLK */
    build_append_int_noprefix(tbl, 0, 4); /* PM1b_EVT_BLK */
    build_append_int_noprefix(tbl, f->pm1a_cnt.address, 4); /* PM1a_CNT_BLK */
    build_append_int_noprefix(tbl, 0, 4); /* PM1b_CNT_BLK */
    build_append_int_noprefix(tbl, 0, 4); /* PM2_CNT_BLK */
    build_append_int_noprefix(tbl, f->pm_tmr.address, 4); /* PM_TMR_BLK */
    build_append_int_noprefix(tbl, f->gpe0_blk.address, 4); /* GPE0_BLK */
    build_append_int_noprefix(tbl, 0, 4); /* GPE1_BLK */
    build_append_int_noprefix(tbl, f->pm1a_evt.bit_width / 8, 1);
    build_append_int_noprefix(tbl, f->pm1a_cnt.bit_width / 8, 1);
    build_append_int_noprefix(tbl, 0, 1); /* PM2_CNT_LEN */
    build_append_int_noprefix(tbl, f->pm_tmr.bit_width / 8, 1); /* PM_TMR_LEN */
    build_append_int_noprefix(tbl, f->gpe0_blk.bit_width / 8, 1);
    build_append_int_noprefix(tbl, 0, 1); /* GPE1_BLK_LEN */
    build_append_int_noprefix(tbl, 0, 1); /* GPE1_BASE */
    build_append_int_noprefix(tbl, 0, 1); /* CST_CNT */
    build_append_int_noprefix(tbl, f->plvl2_lat, 2); /* P_LVL2_LAT */
    build_append_int_noprefix(tbl, f->plvl3_lat, 2); /* P_LVL3_LAT */
    build_append_int_noprefix(tbl, 0, 2); /* FLUSH_SIZE */
    build_append_int_noprefix(tbl, 0, 2); /* FLUSH_STRIDE */
    build_append_int_noprefix(tbl, 0, 1); /* DUTY_OFFSET */
    build_append_int_noprefix(tbl, 0, 1); /* DUTY_WIDTH */
    build_append_int_noprefix(tbl, 0, 1); /* DAY_ALRM */
    build_append_int_noprefix(tbl, 0, 1); /* MON_ALRM */
    build_append_int_noprefix(tbl, f->rtc_century, 1); /* CENTURY */
    if (f->rev == 1) {
        build_append_int_noprefix(tbl, 0, 2);
    } else {
        build_append_int_noprefix(tbl, f->iapc_boot_arch, 2);
    }
    build_append_int_noprefix(tbl, 0, 1); /* Reserved */
    build_append_int_noprefix(tbl, f->flags, 4); /* Flags */

    if (f->rev == 1) {
        goto done;
    }

    build_append_gas_from_struct(tbl, &f->reset_reg); /* RESET_REG */
    build_append_int_noprefix(tbl, f->reset_val, 1); /* RESET_VALUE */
    if ((f->rev >= 6) || ((f->rev == 5) && f->minor_ver > 0)) {
        build_append_int_noprefix(tbl, f->arm_boot_arch, 2); /* ARM_BOOT_ARCH */
        build_append_int_noprefix(tbl, f->minor_ver, 1);
    } else {
        build_append_int_noprefix(tbl, 0, 3); /* Reserved up to ACPI 5.0 */
    }
    build_append_int_noprefix(tbl, 0, 8); /* X_FIRMWARE_CTRL */

    off = tbl->len;
    build_append_int_noprefix(tbl, 0, 8); /* X_DSDT */
    if (f->xdsdt_tbl_offset) {
        bios_linker_loader_add_pointer(linker,
            ACPI_BUILD_TABLE_FILE, off, 8,
            ACPI_BUILD_TABLE_FILE, *f->xdsdt_tbl_offset);
    }

    build_append_gas_from_struct(tbl, &f->pm1a_evt); /* X_PM1a_EVT_BLK */
    build_append_gas(tbl, AML_AS_SYSTEM_MEMORY, 0 , 0, 0, 0);
    build_append_gas_from_struct(tbl, &f->pm1a_cnt); /* X_PM1a_CNT_BLK */
    build_append_gas(tbl, AML_AS_SYSTEM_MEMORY, 0 , 0, 0, 0);
    build_append_gas(tbl, AML_AS_SYSTEM_MEMORY, 0 , 0, 0, 0);
    build_append_gas_from_struct(tbl, &f->pm_tmr); /* X_PM_TMR_BLK */
    build_append_gas_from_struct(tbl, &f->gpe0_blk); /* X_GPE0_BLK */
    build_append_gas(tbl, AML_AS_SYSTEM_MEMORY, 0 , 0, 0, 0); /* X_GPE1_BLK */

    if (f->rev <= 4) {
        goto done;
    }

    build_append_gas_from_struct(tbl, &f->sleep_ctl);
    build_append_gas_from_struct(tbl, &f->sleep_sts);

    if (f->rev == 5) {
        goto done;
    }

    build_append_padded_str(tbl, "QEMU", 8, '\0');

    assert(f->rev == 6);

done:
    acpi_table_end(linker, &table);
}

#ifdef CONFIG_TPM
void build_tpm2(GArray *table_data, BIOSLinker *linker, GArray *tcpalog,
                const char *oem_id, const char *oem_table_id)
{
    uint8_t start_method_params[12] = {};
    unsigned log_addr_offset;
    uint64_t control_area_start_address;
    TPMIf *tpmif = tpm_find();
    uint32_t start_method;
    AcpiTable table = { .sig = "TPM2", .rev = 4,
                        .oem_id = oem_id, .oem_table_id = oem_table_id };

    acpi_table_begin(&table, table_data);

    build_append_int_noprefix(table_data, TPM2_ACPI_CLASS_CLIENT, 2);
    build_append_int_noprefix(table_data, 0, 2);
    if (TPM_IS_TIS_ISA(tpmif) || TPM_IS_TIS_SYSBUS(tpmif)) {
        control_area_start_address = 0;
        start_method = TPM2_START_METHOD_MMIO;
    } else if (TPM_IS_CRB(tpmif)) {
        control_area_start_address = TPM_CRB_ADDR_CTRL;
        start_method = TPM2_START_METHOD_CRB;
    } else {
        g_assert_not_reached();
    }
    build_append_int_noprefix(table_data, control_area_start_address, 8);
    build_append_int_noprefix(table_data, start_method, 4);

    g_array_append_vals(table_data, &start_method_params,
                        ARRAY_SIZE(start_method_params));

    build_append_int_noprefix(table_data, TPM_LOG_AREA_MINIMUM_SIZE, 4);

    acpi_data_push(tcpalog, TPM_LOG_AREA_MINIMUM_SIZE);
    bios_linker_loader_alloc(linker, ACPI_BUILD_TPMLOG_FILE, tcpalog, 1,
                             false);

    log_addr_offset = table_data->len;

    build_append_int_noprefix(table_data, 0, 8);
    bios_linker_loader_add_pointer(linker, ACPI_BUILD_TABLE_FILE,
                                   log_addr_offset, 8,
                                   ACPI_BUILD_TPMLOG_FILE, 0);
    acpi_table_end(linker, &table);
}
#endif

Aml *build_crs(PCIHostState *host, CrsRangeSet *range_set, uint32_t io_offset,
               uint32_t mmio32_offset, uint64_t mmio64_offset,
               uint16_t bus_nr_offset)
{
    Aml *crs = aml_resource_template();
    CrsRangeSet temp_range_set;
    CrsRangeEntry *entry;
    uint8_t max_bus = pci_bus_num(host->bus);
    uint8_t type;
    int devfn;
    int i;

    crs_range_set_init(&temp_range_set);
    for (devfn = 0; devfn < ARRAY_SIZE(host->bus->devices); devfn++) {
        uint64_t range_base, range_limit;
        PCIDevice *dev = host->bus->devices[devfn];

        if (!dev) {
            continue;
        }

        for (i = 0; i < PCI_NUM_REGIONS; i++) {
            PCIIORegion *r = &dev->io_regions[i];

            range_base = r->addr;
            range_limit = r->addr + r->size - 1;

            if (!range_base || range_base > range_limit) {
                continue;
            }

            if (r->type & PCI_BASE_ADDRESS_SPACE_IO) {
                crs_range_insert(temp_range_set.io_ranges,
                                 range_base, range_limit);
            } else { /* "memory" */
                uint64_t length = range_limit - range_base + 1;
                if (range_limit <= UINT32_MAX && length <= UINT32_MAX) {
                    crs_range_insert(temp_range_set.mem_ranges, range_base,
                                     range_limit);
                } else {
                    crs_range_insert(temp_range_set.mem_64bit_ranges,
                                     range_base, range_limit);
                }
            }
        }

        type = dev->config[PCI_HEADER_TYPE] & ~PCI_HEADER_TYPE_MULTI_FUNCTION;
        if (type == PCI_HEADER_TYPE_BRIDGE) {
            uint8_t subordinate = dev->config[PCI_SUBORDINATE_BUS];
            if (subordinate > max_bus) {
                max_bus = subordinate;
            }

            range_base = pci_bridge_get_base(dev, PCI_BASE_ADDRESS_SPACE_IO);
            range_limit = pci_bridge_get_limit(dev, PCI_BASE_ADDRESS_SPACE_IO);

            if (range_base && range_base <= range_limit) {
                crs_range_insert(temp_range_set.io_ranges,
                                 range_base, range_limit);
            }

            range_base =
                pci_bridge_get_base(dev, PCI_BASE_ADDRESS_SPACE_MEMORY);
            range_limit =
                pci_bridge_get_limit(dev, PCI_BASE_ADDRESS_SPACE_MEMORY);

            if (range_base && range_base <= range_limit) {
                uint64_t length = range_limit - range_base + 1;
                if (range_limit <= UINT32_MAX && length <= UINT32_MAX) {
                    crs_range_insert(temp_range_set.mem_ranges,
                                     range_base, range_limit);
                } else {
                    crs_range_insert(temp_range_set.mem_64bit_ranges,
                                     range_base, range_limit);
                }
            }

            range_base =
                pci_bridge_get_base(dev, PCI_BASE_ADDRESS_MEM_PREFETCH);
            range_limit =
                pci_bridge_get_limit(dev, PCI_BASE_ADDRESS_MEM_PREFETCH);

            if (range_base && range_base <= range_limit) {
                uint64_t length = range_limit - range_base + 1;
                if (range_limit <= UINT32_MAX && length <= UINT32_MAX) {
                    crs_range_insert(temp_range_set.mem_ranges,
                                     range_base, range_limit);
                } else {
                    crs_range_insert(temp_range_set.mem_64bit_ranges,
                                     range_base, range_limit);
                }
            }
        }
    }

    crs_range_merge(temp_range_set.io_ranges);
    for (i = 0; i < temp_range_set.io_ranges->len; i++) {
        entry = g_ptr_array_index(temp_range_set.io_ranges, i);
        aml_append(crs,
                   aml_dword_io(AML_MIN_FIXED, AML_MAX_FIXED,
                                AML_POS_DECODE, AML_ENTIRE_RANGE,
                                0, entry->base, entry->limit, io_offset,
                                entry->limit - entry->base + 1));
        crs_range_insert(range_set->io_ranges, entry->base, entry->limit);
    }

    crs_range_merge(temp_range_set.mem_ranges);
    for (i = 0; i < temp_range_set.mem_ranges->len; i++) {
        entry = g_ptr_array_index(temp_range_set.mem_ranges, i);
        assert(entry->limit <= UINT32_MAX &&
               (entry->limit - entry->base + 1) <= UINT32_MAX);
        aml_append(crs,
                   aml_dword_memory(AML_POS_DECODE, AML_MIN_FIXED,
                                    AML_MAX_FIXED, AML_NON_CACHEABLE,
                                    AML_READ_WRITE,
                                    0, entry->base, entry->limit, mmio32_offset,
                                    entry->limit - entry->base + 1));
        crs_range_insert(range_set->mem_ranges, entry->base, entry->limit);
    }

    crs_range_merge(temp_range_set.mem_64bit_ranges);
    for (i = 0; i < temp_range_set.mem_64bit_ranges->len; i++) {
        entry = g_ptr_array_index(temp_range_set.mem_64bit_ranges, i);
        aml_append(crs,
                   aml_qword_memory(AML_POS_DECODE, AML_MIN_FIXED,
                                    AML_MAX_FIXED, AML_NON_CACHEABLE,
                                    AML_READ_WRITE,
                                    0, entry->base, entry->limit, mmio64_offset,
                                    entry->limit - entry->base + 1));
        crs_range_insert(range_set->mem_64bit_ranges,
                         entry->base, entry->limit);
    }

    crs_range_set_free(&temp_range_set);

    aml_append(crs,
        aml_word_bus_number(AML_MIN_FIXED, AML_MAX_FIXED, AML_POS_DECODE,
                            0,
                            pci_bus_num(host->bus),
                            max_bus,
                            bus_nr_offset,
                            max_bus - pci_bus_num(host->bus) + 1));

    return crs;
}

static Aml *aml_serial_bus_device(uint8_t serial_bus_type, uint8_t flags,
                                  uint16_t type_flags,
                                  uint8_t revid, uint16_t data_length,
                                  uint16_t resource_source_len)
{
    Aml *var = aml_alloc();
    uint16_t length = data_length + resource_source_len + 9;

    build_append_byte(var->buf, 0x8e); /* Serial Bus Connection Descriptor */
    build_append_int_noprefix(var->buf, length, sizeof(length));
    build_append_byte(var->buf, 1);    /* Revision ID */
    build_append_byte(var->buf, 0);    /* Resource Source Index */
    build_append_byte(var->buf, serial_bus_type); /* Serial Bus Type */
    build_append_byte(var->buf, flags); /* General Flags */
    build_append_int_noprefix(var->buf, type_flags, /* Type Specific Flags */
                              sizeof(type_flags));
    build_append_byte(var->buf, revid); /* Type Specification Revision ID */
    build_append_int_noprefix(var->buf, data_length, sizeof(data_length));

    return var;
}

Aml *aml_i2c_serial_bus_device(uint16_t address, const char *resource_source)
{
    uint16_t resource_source_len = strlen(resource_source) + 1;
    Aml *var = aml_serial_bus_device(AML_SERIAL_BUS_TYPE_I2C, 0, 0, 1,
                                     6, resource_source_len);

    build_append_int_noprefix(var->buf, 100000, 4);
    build_append_int_noprefix(var->buf, address, sizeof(address));

    g_array_append_vals(var->buf, resource_source, resource_source_len);

    return var;
}
