
#include "qemu/osdep.h"
#include "disas/disas.h"
#include "disas/capstone.h"
#include "hw/core/cpu.h"
#include "disas-internal.h"


struct syminfo *syminfos = NULL;

static void perror_memory(int status, bfd_vma memaddr,
                          struct disassemble_info *info)
{
    if (status != EIO) {
        info->fprintf_func(info->stream, "Unknown error %d\n", status);
    } else {
        info->fprintf_func(info->stream,
                           "Address 0x%" PRIx64 " is out of bounds.\n",
                           memaddr);
    }
}

static void print_address(bfd_vma addr, struct disassemble_info *info)
{
    info->fprintf_func(info->stream, "0x%" PRIx64, addr);
}

static int symbol_at_address(bfd_vma addr, struct disassemble_info *info)
{
    return 1;
}

void disas_initialize_debug(CPUDebug *s)
{
    memset(s, 0, sizeof(*s));
    s->info.arch = bfd_arch_unknown;
    s->info.cap_arch = -1;
    s->info.cap_insn_unit = 4;
    s->info.cap_insn_split = 4;
    s->info.memory_error_func = perror_memory;
    s->info.symbol_at_address_func = symbol_at_address;
}

void disas_initialize_debug_target(CPUDebug *s, CPUState *cpu)
{
    disas_initialize_debug(s);

    s->cpu = cpu;
    s->info.print_address_func = print_address;
    s->info.endian = BFD_ENDIAN_UNKNOWN;

    if (cpu->cc->disas_set_info) {
        cpu->cc->disas_set_info(cpu, &s->info);
        g_assert(s->info.endian != BFD_ENDIAN_UNKNOWN);
    }
}

int disas_gstring_printf(FILE *stream, const char *fmt, ...)
{
    GString *s = (GString *)stream;
    int initial_len = s->len;
    va_list va;

    va_start(va, fmt);
    g_string_append_vprintf(s, fmt, va);
    va_end(va);

    return s->len - initial_len;
}

const char *lookup_symbol(uint64_t orig_addr)
{
    const char *symbol = "";
    struct syminfo *s;

    for (s = syminfos; s; s = s->next) {
        symbol = s->lookup_symbol(s, orig_addr);
        if (symbol[0] != '\0') {
            break;
        }
    }

    return symbol;
}
