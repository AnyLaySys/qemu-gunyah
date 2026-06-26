
#ifndef EXEC__TRANSLATOR_H
#define EXEC__TRANSLATOR_H


#include "qemu/bswap.h"
#include "exec/vaddr.h"

typedef enum DisasJumpType {
    DISAS_NEXT,
    DISAS_TOO_MANY,
    DISAS_NORETURN,
    DISAS_TARGET_0,
    DISAS_TARGET_1,
    DISAS_TARGET_2,
    DISAS_TARGET_3,
    DISAS_TARGET_4,
    DISAS_TARGET_5,
    DISAS_TARGET_6,
    DISAS_TARGET_7,
    DISAS_TARGET_8,
    DISAS_TARGET_9,
    DISAS_TARGET_10,
    DISAS_TARGET_11,
} DisasJumpType;

struct DisasContextBase {
    TranslationBlock *tb;
    vaddr pc_first;
    vaddr pc_next;
    DisasJumpType is_jmp;
    int num_insns;
    int max_insns;
    bool plugin_enabled;
    bool fake_insn;
    struct TCGOp *insn_start;
    void *host_addr[2];

    int record_start;
    int record_len;
    uint8_t record[32];
};

typedef struct TranslatorOps {
    void (*init_disas_context)(DisasContextBase *db, CPUState *cpu);
    void (*tb_start)(DisasContextBase *db, CPUState *cpu);
    void (*insn_start)(DisasContextBase *db, CPUState *cpu);
    void (*translate_insn)(DisasContextBase *db, CPUState *cpu);
    void (*tb_stop)(DisasContextBase *db, CPUState *cpu);
    bool (*disas_log)(const DisasContextBase *db, CPUState *cpu, FILE *f);
} TranslatorOps;

void translator_loop(CPUState *cpu, TranslationBlock *tb, int *max_insns,
                     vaddr pc, void *host_pc, const TranslatorOps *ops,
                     DisasContextBase *db);

bool translator_use_goto_tb(DisasContextBase *db, vaddr dest);

bool translator_io_start(DisasContextBase *db);


uint8_t translator_ldub(CPUArchState *env, DisasContextBase *db, vaddr pc);
uint16_t translator_lduw(CPUArchState *env, DisasContextBase *db, vaddr pc);
uint32_t translator_ldl(CPUArchState *env, DisasContextBase *db, vaddr pc);
uint64_t translator_ldq(CPUArchState *env, DisasContextBase *db, vaddr pc);

static inline uint16_t
translator_lduw_swap(CPUArchState *env, DisasContextBase *db,
                     vaddr pc, bool do_swap)
{
    uint16_t ret = translator_lduw(env, db, pc);
    if (do_swap) {
        ret = bswap16(ret);
    }
    return ret;
}

static inline uint32_t
translator_ldl_swap(CPUArchState *env, DisasContextBase *db,
                    vaddr pc, bool do_swap)
{
    uint32_t ret = translator_ldl(env, db, pc);
    if (do_swap) {
        ret = bswap32(ret);
    }
    return ret;
}

static inline uint64_t
translator_ldq_swap(CPUArchState *env, DisasContextBase *db,
                    vaddr pc, bool do_swap)
{
    uint64_t ret = translator_ldq(env, db, pc);
    if (do_swap) {
        ret = bswap64(ret);
    }
    return ret;
}

void translator_fake_ld(DisasContextBase *db, const void *data, size_t len);

bool translator_st(const DisasContextBase *db, void *dest,
                   vaddr addr, size_t len);

size_t translator_st_len(const DisasContextBase *db);

bool translator_is_same_page(const DisasContextBase *db, vaddr addr);

#endif /* EXEC__TRANSLATOR_H */
