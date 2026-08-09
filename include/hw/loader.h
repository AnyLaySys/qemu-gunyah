#ifndef LOADER_H
#define LOADER_H
#include "hw/nvram/fw_cfg.h"

int64_t get_image_size(const char *filename);
ssize_t load_image_size(const char *filename, void *addr, size_t size);

ssize_t load_image_targphys_as(const char *filename,
                               hwaddr addr, uint64_t max_sz, AddressSpace *as);

ssize_t load_targphys_hex_as(const char *filename, hwaddr *entry,
                             AddressSpace *as);

ssize_t load_image_targphys(const char *filename, hwaddr,
                            uint64_t max_sz);

ssize_t load_image_mr(const char *filename, MemoryRegion *mr);

#define LOAD_IMAGE_MAX_GUNZIP_BYTES (256 << 20)

ssize_t load_image_gzipped_buffer(const char *filename, uint64_t max_sz,
                                  uint8_t **buffer);
ssize_t unpack_efi_zboot_image(uint8_t **buffer, ssize_t *size);

#define ELF_LOAD_FAILED       -1
#define ELF_LOAD_NOT_ELF      -2
#define ELF_LOAD_WRONG_ARCH   -3
#define ELF_LOAD_WRONG_ENDIAN -4
#define ELF_LOAD_TOO_BIG      -5
const char *load_elf_strerror(ssize_t error);

typedef void (*symbol_fn_t)(const char *st_name, int st_info,
                            uint64_t st_value, uint64_t st_size);

ssize_t load_elf_ram_sym(const char *filename,
                         uint64_t (*elf_note_fn)(void *, void *, bool),
                         uint64_t (*translate_fn)(void *, uint64_t),
                         void *translate_opaque, uint64_t *pentry,
                         uint64_t *lowaddr, uint64_t *highaddr,
                         uint32_t *pflags, int elf_data_order, int elf_machine,
                         int clear_lsb, int data_swab,
                         AddressSpace *as, bool load_rom, symbol_fn_t sym_cb);

ssize_t load_elf_as(const char *filename,
                    uint64_t (*elf_note_fn)(void *, void *, bool),
                    uint64_t (*translate_fn)(void *, uint64_t),
                    void *translate_opaque, uint64_t *pentry, uint64_t *lowaddr,
                    uint64_t *highaddr, uint32_t *pflags, int elf_data_order,
                    int elf_machine, int clear_lsb, int data_swab,
                    AddressSpace *as);

ssize_t load_elf(const char *filename,
                 uint64_t (*elf_note_fn)(void *, void *, bool),
                 uint64_t (*translate_fn)(void *, uint64_t),
                 void *translate_opaque, uint64_t *pentry, uint64_t *lowaddr,
                 uint64_t *highaddr, uint32_t *pflags, int elf_data_order,
                 int elf_machine, int clear_lsb, int data_swab);

void load_elf_hdr(const char *filename, void *hdr, bool *is64, Error **errp);

ssize_t load_aout(const char *filename, hwaddr addr, int max_sz,
                  bool big_endian, hwaddr target_page_size);

#define LOAD_UIMAGE_LOADADDR_INVALID (-1)

ssize_t load_uimage_as(const char *filename, hwaddr *ep,
                       hwaddr *loadaddr, int *is_linux,
                       uint64_t (*translate_fn)(void *, uint64_t),
                       void *translate_opaque, AddressSpace *as);

ssize_t load_uimage(const char *filename, hwaddr *ep,
                    hwaddr *loadaddr, int *is_linux,
                    uint64_t (*translate_fn)(void *, uint64_t),
                    void *translate_opaque);

ssize_t load_ramdisk_as(const char *filename, hwaddr addr, uint64_t max_sz,
                        AddressSpace *as);

ssize_t load_ramdisk(const char *filename, hwaddr addr, uint64_t max_sz);

ssize_t gunzip(void *dst, size_t dstlen, uint8_t *src, size_t srclen);

ssize_t read_targphys(const char *name,
                      int fd, hwaddr dst_addr, size_t nbytes);
void pstrcpy_targphys(const char *name,
                      hwaddr dest, int buf_size,
                      const char *source);

ssize_t rom_add_file(const char *file, const char *fw_dir,
                     hwaddr addr, int32_t bootindex,
                     bool has_option_rom, MemoryRegion *mr, AddressSpace *as);
MemoryRegion *rom_add_blob(const char *name, const void *blob, size_t len,
                           size_t max_len, hwaddr addr,
                           const char *fw_file_name,
                           FWCfgCallback fw_callback,
                           void *callback_opaque, AddressSpace *as,
                           bool read_only);
int rom_add_elf_program(const char *name, GMappedFile *mapped_file, void *data,
                        size_t datasize, size_t romsize, hwaddr addr,
                        AddressSpace *as);
int rom_check_and_register_reset(void);
void rom_set_fw(FWCfgState *f);
void rom_set_order_override(int order);
void rom_reset_order_override(void);

void rom_transaction_begin(void);

void rom_transaction_end(bool commit);

int rom_copy(uint8_t *dest, hwaddr addr, size_t size);
void *rom_ptr(hwaddr addr, size_t size);
void *rom_ptr_for_as(AddressSpace *as, hwaddr addr, size_t size);

#define rom_add_file_fixed(_f, _a, _i)          \
    rom_add_file(_f, NULL, _a, _i, false, NULL, NULL)
#define rom_add_blob_fixed(_f, _b, _l, _a)      \
    rom_add_blob(_f, _b, _l, _l, _a, NULL, NULL, NULL, NULL, true)
#define rom_add_file_mr(_f, _mr, _i)            \
    rom_add_file(_f, NULL, 0, _i, false, _mr, NULL)
#define rom_add_file_as(_f, _as, _i)            \
    rom_add_file(_f, NULL, 0, _i, false, NULL, _as)
#define rom_add_file_fixed_as(_f, _a, _i, _as)          \
    rom_add_file(_f, NULL, _a, _i, false, NULL, _as)
#define rom_add_blob_fixed_as(_f, _b, _l, _a, _as)      \
    rom_add_blob(_f, _b, _l, _l, _a, NULL, NULL, NULL, _as, true)

ssize_t rom_add_option(const char *file, int32_t bootindex);

#define UBOOT_MAX_GUNZIP_BYTES (64 << 20)

typedef struct RomGap {
    hwaddr base;
    size_t size;
} RomGap;

RomGap rom_find_largest_gap_between(hwaddr base, size_t size);

#endif
