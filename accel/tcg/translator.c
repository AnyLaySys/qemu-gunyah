








#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/error-report.h"
#include "exec/exec-all.h"
#include "exec/translator.h"
#include "exec/cpu_ldst.h"
#include "exec/plugin-gen.h"
#include "exec/cpu_ldst.h"
#include "exec/tswap.h"
#include "tcg/tcg-op-common.h"
#include "internal-target.h"
#include "disas/disas.h"
#include "tb-internal.h"

static void set_can_do_io(DisasContextBase *db, bool val)
{
    QEMU_BUILD_BUG_ON(sizeof_field(CPUState, neg.can_do_io) != 1);
    tcg_gen_st8_i32(tcg_constant_i32(val), tcg_env,
                    offsetof(ArchCPU, parent_obj.neg.can_do_io) -
                    offsetof(ArchCPU, env));
}

bool translator_io_start(DisasContextBase *db)
{




    if (db->is_jmp == DISAS_NEXT) {
        db->is_jmp = DISAS_TOO_MANY;
    }
    return true;
}

static TCGOp *gen_tb_start(DisasContextBase *db, uint32_t cflags)
{
    TCGv_i32 count = NULL;
    TCGOp *icount_start_insn = NULL;

    if ((cflags & CF_USE_ICOUNT) || !(cflags & CF_NOIRQ)) {
        count = tcg_temp_new_i32();
        tcg_gen_ld_i32(count, tcg_env,
                       offsetof(ArchCPU, parent_obj.neg.icount_decr.u32)
                       - offsetof(ArchCPU, env));
    }

    if (cflags & CF_USE_ICOUNT) {





        tcg_gen_sub_i32(count, count, tcg_constant_i32(0));
        icount_start_insn = tcg_last_op();
    }








    if (cflags & CF_NOIRQ) {
        tcg_ctx->exitreq_label = NULL;
    } else {
        tcg_ctx->exitreq_label = gen_new_label();
        tcg_gen_brcondi_i32(TCG_COND_LT, count, 0, tcg_ctx->exitreq_label);
    }

    if (cflags & CF_USE_ICOUNT) {
        tcg_gen_st16_i32(count, tcg_env,
                         offsetof(ArchCPU, parent_obj.neg.icount_decr.u16.low)
                         - offsetof(ArchCPU, env));
    }

    return icount_start_insn;
}

static void gen_tb_end(const TranslationBlock *tb, uint32_t cflags,
                       TCGOp *icount_start_insn, int num_insns)
{
    if (cflags & CF_USE_ICOUNT) {




        tcg_set_insn_param(icount_start_insn, 2,
                           tcgv_i32_arg(tcg_constant_i32(num_insns)));
    }

    if (tcg_ctx->exitreq_label) {
        gen_set_label(tcg_ctx->exitreq_label);
        tcg_gen_exit_tb(tb, TB_EXIT_REQUESTED);
    }
}

bool translator_is_same_page(const DisasContextBase *db, vaddr addr)
{
    return ((addr ^ db->pc_first) & TARGET_PAGE_MASK) == 0;
}

bool translator_use_goto_tb(DisasContextBase *db, vaddr dest)
{

    if (tb_cflags(db->tb) & CF_NO_GOTO_TB) {
        return false;
    }


    return translator_is_same_page(db, dest);
}

void translator_loop(CPUState *cpu, TranslationBlock *tb, int *max_insns,
                     vaddr pc, void *host_pc, const TranslatorOps *ops,
                     DisasContextBase *db)
{
    uint32_t cflags = tb_cflags(tb);
    TCGOp *icount_start_insn;
    TCGOp *first_insn_start = NULL;
    bool plugin_enabled;


    db->tb = tb;
    db->pc_first = pc;
    db->pc_next = pc;
    db->is_jmp = DISAS_NEXT;
    db->num_insns = 0;
    db->max_insns = *max_insns;
    db->insn_start = NULL;
    db->fake_insn = false;
    db->host_addr[0] = host_pc;
    db->host_addr[1] = NULL;
    db->record_start = 0;
    db->record_len = 0;

    ops->init_disas_context(db, cpu);
    tcg_debug_assert(db->is_jmp == DISAS_NEXT);


    icount_start_insn = gen_tb_start(db, cflags);
    ops->tb_start(db, cpu);
    tcg_debug_assert(db->is_jmp == DISAS_NEXT);

    plugin_enabled = plugin_gen_tb_start(cpu, db);
    db->plugin_enabled = plugin_enabled;

    while (true) {
        *max_insns = ++db->num_insns;
        ops->insn_start(db, cpu);
        db->insn_start = tcg_last_op();
        if (first_insn_start == NULL) {
            first_insn_start = db->insn_start;
        }
        tcg_debug_assert(db->is_jmp == DISAS_NEXT);

        if (plugin_enabled) {
            plugin_gen_insn_start(cpu, db);
        }







        ops->translate_insn(db, cpu);










        if (plugin_enabled) {
            plugin_gen_insn_end();
        }


        if (db->is_jmp != DISAS_NEXT) {
            break;
        }



        if (tcg_op_buf_full() || db->num_insns >= db->max_insns) {
            db->is_jmp = DISAS_TOO_MANY;
            break;
        }
    }


    ops->tb_stop(db, cpu);
    gen_tb_end(tb, cflags, icount_start_insn, db->num_insns);





    if (db->num_insns == 1) {
        tcg_debug_assert(first_insn_start == db->insn_start);
    } else {
        tcg_debug_assert(first_insn_start != db->insn_start);
        tcg_ctx->emit_before_op = first_insn_start;
        set_can_do_io(db, false);
    }
    tcg_ctx->emit_before_op = db->insn_start;
    set_can_do_io(db, true);
    tcg_ctx->emit_before_op = NULL;


    tb->size = db->pc_next - db->pc_first;
    tb->icount = db->num_insns;

    if (plugin_enabled) {
        plugin_gen_tb_end(cpu, db->num_insns);
    }

    if (qemu_loglevel_mask(CPU_LOG_TB_IN_ASM)
        && qemu_log_in_addr_range(db->pc_first)) {
        FILE *logfile = qemu_log_trylock();
        if (logfile) {
            fprintf(logfile, "----------------\n");

            if (!ops->disas_log ||
                !ops->disas_log(db, cpu, logfile)) {
                fprintf(logfile, "IN: %s\n", lookup_symbol(db->pc_first));
                target_disas(logfile, cpu, db);
            }
            fprintf(logfile, "\n");
            qemu_log_unlock(logfile);
        }
    }
}

static bool translator_ld(CPUArchState *env, DisasContextBase *db,
                          void *dest, vaddr pc, size_t len)
{
    TranslationBlock *tb = db->tb;
    vaddr last = pc + len - 1;
    void *host;
    vaddr base;


    if (unlikely(tb_page_addr0(tb) == -1)) {

        tcg_debug_assert(db->max_insns == 1);
        return false;
    }

    host = db->host_addr[0];
    base = db->pc_first;

    if (likely(((base ^ last) & TARGET_PAGE_MASK) == 0)) {

        memcpy(dest, host + (pc - base), len);
        return true;
    }

    if (unlikely(((base ^ pc) & TARGET_PAGE_MASK) == 0)) {

        size_t len0 = -(pc | TARGET_PAGE_MASK);
        memcpy(dest, host + (pc - base), len0);
        pc += len0;
        dest += len0;
        len -= len0;
    }











    base = (base & TARGET_PAGE_MASK) + TARGET_PAGE_SIZE;
    assert(((base ^ pc) & TARGET_PAGE_MASK) == 0);
    assert(((base ^ last) & TARGET_PAGE_MASK) == 0);
    host = db->host_addr[1];

    if (host == NULL) {
        tb_page_addr_t page0, old_page1, new_page1;

        new_page1 = get_page_addr_code_hostp(env, base, &db->host_addr[1]);





        if (unlikely(new_page1 == -1)) {
            tb_unlock_pages(tb);
            tb_set_page_addr0(tb, -1);

            db->max_insns = db->num_insns;
            return false;
        }








        old_page1 = tb_page_addr1(tb);
        if (likely(new_page1 != old_page1)) {
            page0 = tb_page_addr0(tb);
            if (unlikely(old_page1 != -1)) {
                tb_unlock_page1(page0, old_page1);
            }
            tb_set_page_addr1(tb, new_page1);
            tb_lock_page1(page0, new_page1);
        }
        host = db->host_addr[1];
    }

    memcpy(dest, host + (pc - base), len);
    return true;
}

static void record_save(DisasContextBase *db, vaddr pc,
                        const void *from, int size)
{
    int offset;


    if (pc < db->pc_first) {
        return;
    }





    offset = pc - db->pc_first;






    if (db->record_len == 0) {
        db->record_start = offset;
        db->record_len = size;
    } else {
        assert(offset == db->record_start + db->record_len);
        assert(db->record_len + size <= sizeof(db->record));
        db->record_len += size;
    }

    memcpy(db->record + (offset - db->record_start), from, size);
}

size_t translator_st_len(const DisasContextBase *db)
{
    return db->fake_insn ? db->record_len : db->tb->size;
}

bool translator_st(const DisasContextBase *db, void *dest,
                   vaddr addr, size_t len)
{
    size_t offset, offset_end;

    if (addr < db->pc_first) {
        return false;
    }
    offset = addr - db->pc_first;
    offset_end = offset + len;
    if (offset_end > translator_st_len(db)) {
        return false;
    }

    if (!db->fake_insn) {
        size_t offset_page1 = -(db->pc_first | TARGET_PAGE_MASK);


        if (db->host_addr[0]) {
            if (offset_end <= offset_page1) {
                memcpy(dest, db->host_addr[0] + offset, len);
                return true;
            }
            if (offset < offset_page1) {
                size_t len0 = offset_page1 - offset;
                memcpy(dest, db->host_addr[0] + offset, len0);
                offset += len0;
                dest += len0;
            }
        }


        if (db->host_addr[1] && offset >= offset_page1) {
            memcpy(dest, db->host_addr[1] + (offset - offset_page1),
                   offset_end - offset);
            return true;
        }
    }


    if (db->record_len != 0 &&
        offset >= db->record_start &&
        offset_end <= db->record_start + db->record_len) {
        memcpy(dest, db->record + (offset - db->record_start),
               offset_end - offset);
        return true;
    }
    return false;
}

uint8_t translator_ldub(CPUArchState *env, DisasContextBase *db, vaddr pc)
{
    uint8_t raw;

    if (!translator_ld(env, db, &raw, pc, sizeof(raw))) {
        raw = cpu_ldub_code(env, pc);
        record_save(db, pc, &raw, sizeof(raw));
    }
    return raw;
}

uint16_t translator_lduw(CPUArchState *env, DisasContextBase *db, vaddr pc)
{
    uint16_t raw, tgt;

    if (translator_ld(env, db, &raw, pc, sizeof(raw))) {
        tgt = tswap16(raw);
    } else {
        tgt = cpu_lduw_code(env, pc);
        raw = tswap16(tgt);
        record_save(db, pc, &raw, sizeof(raw));
    }
    return tgt;
}

uint32_t translator_ldl(CPUArchState *env, DisasContextBase *db, vaddr pc)
{
    uint32_t raw, tgt;

    if (translator_ld(env, db, &raw, pc, sizeof(raw))) {
        tgt = tswap32(raw);
    } else {
        tgt = cpu_ldl_code(env, pc);
        raw = tswap32(tgt);
        record_save(db, pc, &raw, sizeof(raw));
    }
    return tgt;
}

uint64_t translator_ldq(CPUArchState *env, DisasContextBase *db, vaddr pc)
{
    uint64_t raw, tgt;

    if (translator_ld(env, db, &raw, pc, sizeof(raw))) {
        tgt = tswap64(raw);
    } else {
        tgt = cpu_ldq_code(env, pc);
        raw = tswap64(tgt);
        record_save(db, pc, &raw, sizeof(raw));
    }
    return tgt;
}

void translator_fake_ld(DisasContextBase *db, const void *data, size_t len)
{
    db->fake_insn = true;
    record_save(db, db->pc_first, data, len);
}
