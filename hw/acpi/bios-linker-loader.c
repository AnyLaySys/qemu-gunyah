
#include "qemu/osdep.h"
#include "hw/acpi/bios-linker-loader.h"
#include "hw/nvram/fw_cfg.h"

#include "qemu/bswap.h"

#define BIOS_LINKER_LOADER_FILESZ FW_CFG_MAX_FILE_PATH

struct BiosLinkerLoaderEntry {
    uint32_t command;
    union {
        struct {
            char file[BIOS_LINKER_LOADER_FILESZ];
            uint32_t align;
            uint8_t zone;
        } alloc;

        struct {
            char dest_file[BIOS_LINKER_LOADER_FILESZ];
            char src_file[BIOS_LINKER_LOADER_FILESZ];
            uint32_t offset;
            uint8_t size;
        } pointer;

        struct {
            char file[BIOS_LINKER_LOADER_FILESZ];
            uint32_t offset;
            uint32_t start;
            uint32_t length;
        } cksum;

        struct {
            char dest_file[BIOS_LINKER_LOADER_FILESZ];
            char src_file[BIOS_LINKER_LOADER_FILESZ];
            uint32_t dst_offset;
            uint32_t src_offset;
            uint8_t size;
        } wr_pointer;

        char pad[124];
    };
} QEMU_PACKED;
typedef struct BiosLinkerLoaderEntry BiosLinkerLoaderEntry;

enum {
    BIOS_LINKER_LOADER_COMMAND_ALLOCATE          = 0x1,
    BIOS_LINKER_LOADER_COMMAND_ADD_POINTER       = 0x2,
    BIOS_LINKER_LOADER_COMMAND_ADD_CHECKSUM      = 0x3,
    BIOS_LINKER_LOADER_COMMAND_WRITE_POINTER     = 0x4,
};

enum {
    BIOS_LINKER_LOADER_ALLOC_ZONE_HIGH = 0x1,
    BIOS_LINKER_LOADER_ALLOC_ZONE_FSEG = 0x2,
};

typedef struct BiosLinkerFileEntry {
    char *name; /* file name */
    GArray *blob; /* data accosiated with @name */
} BiosLinkerFileEntry;

BIOSLinker *bios_linker_loader_init(void)
{
    BIOSLinker *linker = g_new(BIOSLinker, 1);

    linker->cmd_blob = g_array_new(false, true /* clear */, 1);
    linker->file_list = g_array_new(false, true /* clear */,
                                    sizeof(BiosLinkerFileEntry));
    return linker;
}

void bios_linker_loader_cleanup(BIOSLinker *linker)
{
    int i;
    BiosLinkerFileEntry *entry;

    g_array_free(linker->cmd_blob, true);

    for (i = 0; i < linker->file_list->len; i++) {
        entry = &g_array_index(linker->file_list, BiosLinkerFileEntry, i);
        g_free(entry->name);
    }
    g_array_free(linker->file_list, true);
    g_free(linker);
}

static const BiosLinkerFileEntry *
bios_linker_find_file(const BIOSLinker *linker, const char *name)
{
    int i;
    BiosLinkerFileEntry *entry;

    for (i = 0; i < linker->file_list->len; i++) {
        entry = &g_array_index(linker->file_list, BiosLinkerFileEntry, i);
        if (!strcmp(entry->name, name)) {
            return entry;
        }
    }
    return NULL;
}

bool bios_linker_loader_can_write_pointer(void)
{
    FWCfgState *fw_cfg = fw_cfg_find();
    return fw_cfg && fw_cfg_dma_enabled(fw_cfg);
}

void bios_linker_loader_alloc(BIOSLinker *linker,
                              const char *file_name,
                              GArray *file_blob,
                              uint32_t alloc_align,
                              bool alloc_fseg)
{
    BiosLinkerLoaderEntry entry;
    BiosLinkerFileEntry file = { g_strdup(file_name), file_blob};

    assert(!(alloc_align & (alloc_align - 1)));

    assert(!bios_linker_find_file(linker, file_name));
    g_array_append_val(linker->file_list, file);

    memset(&entry, 0, sizeof entry);
    strncpy(entry.alloc.file, file_name, sizeof entry.alloc.file - 1);
    entry.command = cpu_to_le32(BIOS_LINKER_LOADER_COMMAND_ALLOCATE);
    entry.alloc.align = cpu_to_le32(alloc_align);
    entry.alloc.zone = alloc_fseg ? BIOS_LINKER_LOADER_ALLOC_ZONE_FSEG :
                                    BIOS_LINKER_LOADER_ALLOC_ZONE_HIGH;

    g_array_prepend_vals(linker->cmd_blob, &entry, sizeof entry);
}

void bios_linker_loader_add_checksum(BIOSLinker *linker, const char *file_name,
                                     unsigned start_offset, unsigned size,
                                     unsigned checksum_offset)
{
    BiosLinkerLoaderEntry entry;
    const BiosLinkerFileEntry *file = bios_linker_find_file(linker, file_name);

    assert(file);
    assert(start_offset < file->blob->len);
    assert(start_offset + size <= file->blob->len);
    assert(checksum_offset >= start_offset);
    assert(checksum_offset + 1 <= start_offset + size);

    *(file->blob->data + checksum_offset) = 0;
    memset(&entry, 0, sizeof entry);
    strncpy(entry.cksum.file, file_name, sizeof entry.cksum.file - 1);
    entry.command = cpu_to_le32(BIOS_LINKER_LOADER_COMMAND_ADD_CHECKSUM);
    entry.cksum.offset = cpu_to_le32(checksum_offset);
    entry.cksum.start = cpu_to_le32(start_offset);
    entry.cksum.length = cpu_to_le32(size);

    g_array_append_vals(linker->cmd_blob, &entry, sizeof entry);
}

void bios_linker_loader_add_pointer(BIOSLinker *linker,
                                    const char *dest_file,
                                    uint32_t dst_patched_offset,
                                    uint8_t dst_patched_size,
                                    const char *src_file,
                                    uint32_t src_offset)
{
    uint64_t le_src_offset;
    BiosLinkerLoaderEntry entry;
    const BiosLinkerFileEntry *dst_file =
        bios_linker_find_file(linker, dest_file);
    const BiosLinkerFileEntry *source_file =
        bios_linker_find_file(linker, src_file);

    assert(dst_file);
    assert(source_file);
    assert(dst_patched_offset < dst_file->blob->len);
    assert(dst_patched_offset + dst_patched_size <= dst_file->blob->len);
    assert(src_offset < source_file->blob->len);

    memset(&entry, 0, sizeof entry);
    strncpy(entry.pointer.dest_file, dest_file,
            sizeof entry.pointer.dest_file - 1);
    strncpy(entry.pointer.src_file, src_file,
            sizeof entry.pointer.src_file - 1);
    entry.command = cpu_to_le32(BIOS_LINKER_LOADER_COMMAND_ADD_POINTER);
    entry.pointer.offset = cpu_to_le32(dst_patched_offset);
    entry.pointer.size = dst_patched_size;
    assert(dst_patched_size == 1 || dst_patched_size == 2 ||
           dst_patched_size == 4 || dst_patched_size == 8);

    le_src_offset = cpu_to_le64(src_offset);
    memcpy(dst_file->blob->data + dst_patched_offset,
           &le_src_offset, dst_patched_size);

    g_array_append_vals(linker->cmd_blob, &entry, sizeof entry);
}

void bios_linker_loader_write_pointer(BIOSLinker *linker,
                                    const char *dest_file,
                                    uint32_t dst_patched_offset,
                                    uint8_t dst_patched_size,
                                    const char *src_file,
                                    uint32_t src_offset)
{
    BiosLinkerLoaderEntry entry;
    const BiosLinkerFileEntry *source_file =
        bios_linker_find_file(linker, src_file);

    assert(source_file);
    assert(src_offset < source_file->blob->len);
    memset(&entry, 0, sizeof entry);
    strncpy(entry.wr_pointer.dest_file, dest_file,
            sizeof entry.wr_pointer.dest_file - 1);
    strncpy(entry.wr_pointer.src_file, src_file,
            sizeof entry.wr_pointer.src_file - 1);
    entry.command = cpu_to_le32(BIOS_LINKER_LOADER_COMMAND_WRITE_POINTER);
    entry.wr_pointer.dst_offset = cpu_to_le32(dst_patched_offset);
    entry.wr_pointer.src_offset = cpu_to_le32(src_offset);
    entry.wr_pointer.size = dst_patched_size;
    assert(dst_patched_size == 1 || dst_patched_size == 2 ||
           dst_patched_size == 4 || dst_patched_size == 8);

    g_array_append_vals(linker->cmd_blob, &entry, sizeof entry);
}
