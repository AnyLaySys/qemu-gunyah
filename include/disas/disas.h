#ifndef QEMU_DISAS_H
#define QEMU_DISAS_H

const char *lookup_symbol(uint64_t orig_addr);

struct syminfo;
struct elf32_sym;
struct elf64_sym;

typedef const char *(*lookup_symbol_t)(struct syminfo *s, uint64_t orig_addr);

struct syminfo {
    lookup_symbol_t lookup_symbol;
    unsigned int disas_num_syms;
    union {
      struct elf32_sym *elf32;
      struct elf64_sym *elf64;
    } disas_symtab;
    const char *disas_strtab;
    struct syminfo *next;
};

extern struct syminfo *syminfos;

#endif /* QEMU_DISAS_H */
