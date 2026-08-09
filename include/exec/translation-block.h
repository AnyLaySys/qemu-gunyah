
#ifndef EXEC_TRANSLATION_BLOCK_H
#define EXEC_TRANSLATION_BLOCK_H

#include "qemu/atomic.h"
#include "qemu/thread.h"
#include "exec/cpu-common.h"
#include "exec/vaddr.h"
#ifdef CONFIG_USER_ONLY
#include "qemu/interval-tree.h"
#endif

#if defined(CONFIG_USER_ONLY)
typedef vaddr tb_page_addr_t;
#define TB_PAGE_ADDR_FMT "%" VADDR_PRIx
#else
typedef ram_addr_t tb_page_addr_t;
#define TB_PAGE_ADDR_FMT RAM_ADDR_FMT
#endif

struct tb_tc {
    const void *ptr;    /* pointer to the translated code */
    size_t size;
};

struct TranslationBlock {
    vaddr pc;

    uint64_t cs_base;

    uint32_t flags; /* flags defining in which context the code was generated */
    uint32_t cflags;    /* compile flags */

#define CF_COUNT_MASK    0x000001ff
#define CF_NO_GOTO_TB    0x00000200 /* Do not chain with goto_tb */
#define CF_NO_GOTO_PTR   0x00000400 /* Do not chain with goto_ptr */
#define CF_SINGLE_STEP   0x00000800 /* gdbstub single-step in effect */
#define CF_MEMI_ONLY     0x00001000 /* Only instrument memory ops */
#define CF_USE_ICOUNT    0x00002000
#define CF_INVALID       0x00004000 /* TB is stale. Set with @jmp_lock held */
#define CF_PARALLEL      0x00008000 /* Generate code for a parallel context */
#define CF_NOIRQ         0x00010000 /* Generate an uninterruptible TB */
#define CF_PCREL         0x00020000 /* Opcodes in TB are PC-relative */
#define CF_BP_PAGE       0x00040000 /* Breakpoint present in code page */
#define CF_CLUSTER_MASK  0xff000000 /* Top 8 bits are cluster ID */
#define CF_CLUSTER_SHIFT 24


    uint16_t size;
    uint16_t icount;

    struct tb_tc tc;

#ifdef CONFIG_USER_ONLY
    IntervalTreeNode itree;
#else
    uintptr_t page_next[2];
    tb_page_addr_t page_addr[2];
#endif

    QemuSpin jmp_lock;

#define TB_JMP_OFFSET_INVALID 0xffff /* indicates no jump generated */
    uint16_t jmp_reset_offset[2]; /* offset of original jump target */
    uint16_t jmp_insn_offset[2];  /* offset of direct jump insn */
    uintptr_t jmp_target_addr[2]; /* target address */

    uintptr_t jmp_list_head;
    uintptr_t jmp_list_next[2];
    uintptr_t jmp_dest[2];
};

#define CODE_GEN_ALIGN  16

static inline uint32_t tb_cflags(const TranslationBlock *tb)
{
    return qatomic_read(&tb->cflags);
}

#endif /* EXEC_TRANSLATION_BLOCK_H */
