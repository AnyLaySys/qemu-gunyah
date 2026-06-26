
#ifndef DISAS_DIS_ASM_H
#define DISAS_DIS_ASM_H

#include "qemu/bswap.h"

typedef void *PTR;
typedef uint64_t bfd_vma;
typedef int64_t bfd_signed_vma;
typedef uint8_t bfd_byte;
#define sprintf_vma(s,x) sprintf (s, "%0" PRIx64, x)
#define snprintf_vma(s,ss,x) snprintf (s, ss, "%0" PRIx64, x)

#define BFD64

enum bfd_flavour {
  bfd_target_unknown_flavour,
  bfd_target_aout_flavour,
  bfd_target_coff_flavour,
  bfd_target_ecoff_flavour,
  bfd_target_elf_flavour,
  bfd_target_ieee_flavour,
  bfd_target_nlm_flavour,
  bfd_target_oasys_flavour,
  bfd_target_tekhex_flavour,
  bfd_target_srec_flavour,
  bfd_target_ihex_flavour,
  bfd_target_som_flavour,
  bfd_target_os9k_flavour,
  bfd_target_versados_flavour,
  bfd_target_msdos_flavour,
  bfd_target_evax_flavour
};

enum bfd_endian { BFD_ENDIAN_BIG, BFD_ENDIAN_LITTLE, BFD_ENDIAN_UNKNOWN };

enum bfd_architecture
{
  bfd_arch_unknown,    /* File arch not known */
  bfd_arch_obscure,    /* Arch known, not one of these */
  bfd_arch_m68k,       /* Motorola 68xxx */
#define bfd_mach_m68000 1
#define bfd_mach_m68008 2
#define bfd_mach_m68010 3
#define bfd_mach_m68020 4
#define bfd_mach_m68030 5
#define bfd_mach_m68040 6
#define bfd_mach_m68060 7
#define bfd_mach_cpu32  8
#define bfd_mach_mcf5200  9
#define bfd_mach_mcf5206e 10
#define bfd_mach_mcf5307  11
#define bfd_mach_mcf5407  12
#define bfd_mach_mcf528x  13
#define bfd_mach_mcfv4e   14
#define bfd_mach_mcf521x   15
#define bfd_mach_mcf5249   16
#define bfd_mach_mcf547x   17
#define bfd_mach_mcf548x   18
  bfd_arch_vax,        /* DEC Vax */
  bfd_arch_i960,       /* Intel 960 */

#define bfd_mach_i960_core      1
#define bfd_mach_i960_ka_sa     2
#define bfd_mach_i960_kb_sb     3
#define bfd_mach_i960_mc        4
#define bfd_mach_i960_xa        5
#define bfd_mach_i960_ca        6
#define bfd_mach_i960_jx        7
#define bfd_mach_i960_hx        8

  bfd_arch_a29k,       /* AMD 29000 */
  bfd_arch_sparc,      /* SPARC */
#define bfd_mach_sparc                 1
#define bfd_mach_sparc_sparclet        2
#define bfd_mach_sparc_sparclite       3
#define bfd_mach_sparc_v8plus          4
#define bfd_mach_sparc_v8plusa         5 /* with ultrasparc add'ns.  */
#define bfd_mach_sparc_sparclite_le    6
#define bfd_mach_sparc_v9              7
#define bfd_mach_sparc_v9a             8 /* with ultrasparc add'ns.  */
#define bfd_mach_sparc_v8plusb         9 /* with cheetah add'ns.  */
#define bfd_mach_sparc_v9b             10 /* with cheetah add'ns.  */
#define bfd_mach_sparc_v9_p(mach) \
  ((mach) >= bfd_mach_sparc_v8plus && (mach) <= bfd_mach_sparc_v9b \
   && (mach) != bfd_mach_sparc_sparclite_le)
  bfd_arch_mips,       /* MIPS Rxxxx */
#define bfd_mach_mips3000              3000
#define bfd_mach_mips3900              3900
#define bfd_mach_mips4000              4000
#define bfd_mach_mips4010              4010
#define bfd_mach_mips4100              4100
#define bfd_mach_mips4300              4300
#define bfd_mach_mips4400              4400
#define bfd_mach_mips4600              4600
#define bfd_mach_mips4650              4650
#define bfd_mach_mips5000              5000
#define bfd_mach_mips6000              6000
#define bfd_mach_mips8000              8000
#define bfd_mach_mips10000             10000
#define bfd_mach_mips16                16
  bfd_arch_i386,       /* Intel 386 */
#define bfd_mach_i386_i386 0
#define bfd_mach_i386_i8086 1
#define bfd_mach_i386_i386_intel_syntax 2
#define bfd_mach_x86_64 3
#define bfd_mach_x86_64_intel_syntax 4
  bfd_arch_we32k,      /* AT&T WE32xxx */
  bfd_arch_tahoe,      /* CCI/Harris Tahoe */
  bfd_arch_i860,       /* Intel 860 */
  bfd_arch_romp,       /* IBM ROMP PC/RT */
  bfd_arch_alliant,    /* Alliant */
  bfd_arch_convex,     /* Convex */
  bfd_arch_m88k,       /* Motorola 88xxx */
  bfd_arch_pyramid,    /* Pyramid Technology */
  bfd_arch_h8300,      /* Hitachi H8/300 */
#define bfd_mach_h8300   1
#define bfd_mach_h8300h  2
#define bfd_mach_h8300s  3
  bfd_arch_powerpc,    /* PowerPC */
#define bfd_mach_ppc           0
#define bfd_mach_ppc64         1
#define bfd_mach_ppc_403       403
#define bfd_mach_ppc_403gc     4030
#define bfd_mach_ppc_e500      500
#define bfd_mach_ppc_505       505
#define bfd_mach_ppc_601       601
#define bfd_mach_ppc_602       602
#define bfd_mach_ppc_603       603
#define bfd_mach_ppc_ec603e    6031
#define bfd_mach_ppc_604       604
#define bfd_mach_ppc_620       620
#define bfd_mach_ppc_630       630
#define bfd_mach_ppc_750       750
#define bfd_mach_ppc_860       860
#define bfd_mach_ppc_a35       35
#define bfd_mach_ppc_rs64ii    642
#define bfd_mach_ppc_rs64iii   643
#define bfd_mach_ppc_7400      7400
  bfd_arch_rs6000,     /* IBM RS/6000 */
  bfd_arch_hppa,       /* HP PA RISC */
#define bfd_mach_hppa10        10
#define bfd_mach_hppa11        11
#define bfd_mach_hppa20        20
#define bfd_mach_hppa20w       25
  bfd_arch_d10v,       /* Mitsubishi D10V */
  bfd_arch_z8k,        /* Zilog Z8000 */
#define bfd_mach_z8001         1
#define bfd_mach_z8002         2
  bfd_arch_h8500,      /* Hitachi H8/500 */
  bfd_arch_sh,         /* Hitachi SH */
#define bfd_mach_sh            1
#define bfd_mach_sh2        0x20
#define bfd_mach_sh_dsp     0x2d
#define bfd_mach_sh2a       0x2a
#define bfd_mach_sh2a_nofpu 0x2b
#define bfd_mach_sh2e       0x2e
#define bfd_mach_sh3        0x30
#define bfd_mach_sh3_nommu  0x31
#define bfd_mach_sh3_dsp    0x3d
#define bfd_mach_sh3e       0x3e
#define bfd_mach_sh4        0x40
#define bfd_mach_sh4_nofpu  0x41
#define bfd_mach_sh4_nommu_nofpu  0x42
#define bfd_mach_sh4a       0x4a
#define bfd_mach_sh4a_nofpu 0x4b
#define bfd_mach_sh4al_dsp  0x4d
#define bfd_mach_sh5        0x50
  bfd_arch_alpha,      /* Dec Alpha */
#define bfd_mach_alpha 1
#define bfd_mach_alpha_ev4  0x10
#define bfd_mach_alpha_ev5  0x20
#define bfd_mach_alpha_ev6  0x30
  bfd_arch_arm,        /* Advanced Risc Machines ARM */
#define bfd_mach_arm_unknown  0
#define bfd_mach_arm_2        1
#define bfd_mach_arm_2a       2
#define bfd_mach_arm_3        3
#define bfd_mach_arm_3M       4
#define bfd_mach_arm_4        5
#define bfd_mach_arm_4T       6
#define bfd_mach_arm_5        7
#define bfd_mach_arm_5T       8
#define bfd_mach_arm_5TE      9
#define bfd_mach_arm_XScale   10
#define bfd_mach_arm_ep9312   11
#define bfd_mach_arm_iWMMXt   12
#define bfd_mach_arm_iWMMXt2  13
  bfd_arch_ns32k,      /* National Semiconductors ns32000 */
  bfd_arch_w65,        /* WDC 65816 */
  bfd_arch_tic30,      /* Texas Instruments TMS320C30 */
  bfd_arch_v850,       /* NEC V850 */
#define bfd_mach_v850          0
  bfd_arch_arc,        /* Argonaut RISC Core */
#define bfd_mach_arc_base 0
  bfd_arch_m32r,       /* Mitsubishi M32R/D */
#define bfd_mach_m32r          0  /* backwards compatibility */
  bfd_arch_mn10200,    /* Matsushita MN10200 */
  bfd_arch_mn10300,    /* Matsushita MN10300 */
  bfd_arch_avr,        /* AVR microcontrollers */
#define bfd_mach_avr1       1
#define bfd_mach_avr2       2
#define bfd_mach_avr25      25
#define bfd_mach_avr3       3
#define bfd_mach_avr31      31
#define bfd_mach_avr35      35
#define bfd_mach_avr4       4
#define bfd_mach_avr5       5
#define bfd_mach_avr51      51
#define bfd_mach_avr6       6
#define bfd_mach_avrtiny    100
#define bfd_mach_avrxmega1  101
#define bfd_mach_avrxmega2  102
#define bfd_mach_avrxmega3  103
#define bfd_mach_avrxmega4  104
#define bfd_mach_avrxmega5  105
#define bfd_mach_avrxmega6  106
#define bfd_mach_avrxmega7  107
  bfd_arch_microblaze, /* Xilinx MicroBlaze.  */
  bfd_arch_moxie,      /* The Moxie core.  */
  bfd_arch_ia64,      /* HP/Intel ia64 */
#define bfd_mach_ia64_elf64    64
#define bfd_mach_ia64_elf32    32
  bfd_arch_rx,       /* Renesas RX */
#define bfd_mach_rx            0x75
#define bfd_mach_rx_v2         0x76
#define bfd_mach_rx_v3         0x77
  bfd_arch_loongarch,
  bfd_arch_last
  };
#define bfd_mach_s390_31 31
#define bfd_mach_s390_64 64

typedef struct symbol_cache_entry
{
    const char *name;
    union
    {
        PTR p;
        bfd_vma i;
    } udata;
} asymbol;

typedef int (*fprintf_function)(FILE *f, const char *fmt, ...)
    G_GNUC_PRINTF(2, 3);

enum dis_insn_type {
  dis_noninsn,          /* Not a valid instruction */
  dis_nonbranch,        /* Not a branch instruction */
  dis_branch,           /* Unconditional branch */
  dis_condbranch,       /* Conditional branch */
  dis_jsr,              /* Jump to subroutine */
  dis_condjsr,          /* Conditional jump to subroutine */
  dis_dref,             /* Data reference instruction */
  dis_dref2             /* Two data references in instruction */
};


typedef struct disassemble_info {
  fprintf_function fprintf_func;
  FILE *stream;
  PTR application_data;

  enum bfd_flavour flavour;
  enum bfd_architecture arch;
  unsigned long mach;
  enum bfd_endian endian;

  asymbol **symbols;
  int num_symbols;

  unsigned long flags;
#define INSN_HAS_RELOC  0x80000000
#define INSN_ARM_BE32   0x00010000
  PTR private_data;

  int (*read_memory_func)
    (bfd_vma memaddr, bfd_byte *myaddr, int length,
        struct disassemble_info *info);

  void (*memory_error_func)
    (int status, bfd_vma memaddr, struct disassemble_info *info);

  void (*print_address_func)
    (bfd_vma addr, struct disassemble_info *info);

    int (*print_insn)(bfd_vma addr, struct disassemble_info *info);

  int (* symbol_at_address_func)
    (bfd_vma addr, struct disassemble_info * info);

  const bfd_byte *buffer;
  bfd_vma buffer_vma;
  int buffer_length;

  int bytes_per_line;

  int bytes_per_chunk;
  enum bfd_endian display_endian;


  char insn_info_valid;         /* Branch info has been set. */
  char branch_delay_insns;      /* How many sequential insn's will run before
                                   a branch takes effect.  (0 = normal) */
  char data_size;               /* Size of data reference in insn, in bytes */
  enum dis_insn_type insn_type; /* Type of instruction */
  bfd_vma target;               /* Target address of branch or dref, if known;
                                   zero if unknown.  */
  bfd_vma target2;              /* Second target address for dref2 */

  char * disassembler_options;

  bool show_opcodes;

  void *target_info;

  int cap_arch;
  int cap_mode;
  int cap_insn_unit;
  int cap_insn_split;

} disassemble_info;

typedef int (*disassembler_ftype) (bfd_vma, disassemble_info *);

int print_insn_tci(bfd_vma, disassemble_info*);
int print_insn_big_mips         (bfd_vma, disassemble_info*);
int print_insn_little_mips      (bfd_vma, disassemble_info*);
int print_insn_nanomips         (bfd_vma, disassemble_info*);
int print_insn_m68k             (bfd_vma, disassemble_info*);
int print_insn_z8001            (bfd_vma, disassemble_info*);
int print_insn_z8002            (bfd_vma, disassemble_info*);
int print_insn_h8300            (bfd_vma, disassemble_info*);
int print_insn_h8300h           (bfd_vma, disassemble_info*);
int print_insn_h8300s           (bfd_vma, disassemble_info*);
int print_insn_h8500            (bfd_vma, disassemble_info*);
int print_insn_arm_a64          (bfd_vma, disassemble_info*);
int print_insn_alpha            (bfd_vma, disassemble_info*);
disassembler_ftype arc_get_disassembler (int, int);
int print_insn_sparc            (bfd_vma, disassemble_info*);
int print_insn_big_a29k         (bfd_vma, disassemble_info*);
int print_insn_little_a29k      (bfd_vma, disassemble_info*);
int print_insn_i960             (bfd_vma, disassemble_info*);
int print_insn_sh               (bfd_vma, disassemble_info*);
int print_insn_shl              (bfd_vma, disassemble_info*);
int print_insn_hppa             (bfd_vma, disassemble_info*);
int print_insn_m32r             (bfd_vma, disassemble_info*);
int print_insn_m88k             (bfd_vma, disassemble_info*);
int print_insn_mn10200          (bfd_vma, disassemble_info*);
int print_insn_mn10300          (bfd_vma, disassemble_info*);
int print_insn_ns32k            (bfd_vma, disassemble_info*);
int print_insn_big_powerpc      (bfd_vma, disassemble_info*);
int print_insn_little_powerpc   (bfd_vma, disassemble_info*);
int print_insn_rs6000           (bfd_vma, disassemble_info*);
int print_insn_w65              (bfd_vma, disassemble_info*);
int print_insn_d10v             (bfd_vma, disassemble_info*);
int print_insn_v850             (bfd_vma, disassemble_info*);
int print_insn_tic30            (bfd_vma, disassemble_info*);
int print_insn_microblaze       (bfd_vma, disassemble_info*);
int print_insn_ia64             (bfd_vma, disassemble_info*);
int print_insn_xtensa           (bfd_vma, disassemble_info*);
int print_insn_riscv32          (bfd_vma, disassemble_info*);
int print_insn_riscv64          (bfd_vma, disassemble_info*);
int print_insn_riscv128         (bfd_vma, disassemble_info*);
int print_insn_rx(bfd_vma, disassemble_info *);
int print_insn_hexagon(bfd_vma, disassemble_info *);
int print_insn_loongarch(bfd_vma, disassemble_info *);

#ifdef CONFIG_CAPSTONE
bool cap_disas_target(disassemble_info *info, uint64_t pc, size_t size);
bool cap_disas_host(disassemble_info *info, const void *code, size_t size);
bool cap_disas_monitor(disassemble_info *info, uint64_t pc, int count);
bool cap_disas_plugin(disassemble_info *info, uint64_t pc, size_t size);
#else
# define cap_disas_target(i, p, s)  false
# define cap_disas_host(i, p, s)    false
# define cap_disas_monitor(i, p, c) false
# define cap_disas_plugin(i, p, c)  false
#endif /* CONFIG_CAPSTONE */

#ifndef ATTRIBUTE_UNUSED
#define ATTRIBUTE_UNUSED __attribute__((unused))
#endif


static inline bfd_vma bfd_getl64(const bfd_byte *addr)
{
    return ldq_le_p(addr);
}

static inline bfd_vma bfd_getl32(const bfd_byte *addr)
{
    return (uint32_t)ldl_le_p(addr);
}

static inline bfd_vma bfd_getl16(const bfd_byte *addr)
{
    return lduw_le_p(addr);
}

static inline bfd_vma bfd_getb32(const bfd_byte *addr)
{
    return (uint32_t)ldl_be_p(addr);
}

static inline bfd_vma bfd_getb16(const bfd_byte *addr)
{
    return lduw_be_p(addr);
}

typedef bool bfd_boolean;

#endif /* DISAS_DIS_ASM_H */
