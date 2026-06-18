


















#include "qemu/osdep.h"

#include "trace.h"
#include "disas/disas.h"
#include "exec/exec-all.h"
#include "tcg/tcg.h"
#if defined(CONFIG_USER_ONLY)
#include "qemu.h"
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#include <sys/param.h>
#if __FreeBSD_version >= 700104
#define HAVE_KINFO_GETVMMAP
#define sigqueue sigqueue_freebsd
#include <sys/proc.h>
#include <machine/profile.h>
#define _KERNEL
#include <sys/user.h>
#undef _KERNEL
#undef sigqueue
#include <libutil.h>
#endif
#endif
#else
#include "exec/ram_addr.h"
#endif

#include "exec/cputlb.h"
#include "exec/page-protection.h"
#include "tb-internal.h"
#include "exec/translator.h"
#include "exec/tb-flush.h"
#include "qemu/bitmap.h"
#include "qemu/qemu-print.h"
#include "qemu/main-loop.h"
#include "qemu/cacheinfo.h"
#include "qemu/timer.h"
#include "exec/log.h"
#include "system/cpu-timers.h"
#include "system/tcg.h"
#include "qapi/error.h"
#include "accel/tcg/cpu-ops.h"
#include "tb-jmp-cache.h"
#include "tb-hash.h"
#include "tb-context.h"
#include "tb-internal.h"
#include "internal-common.h"
#include "internal-target.h"
#include "tcg/perf.h"
#include "tcg/insn-start-words.h"

TBContext tb_ctx;





static uint8_t *encode_sleb128(uint8_t *p, int64_t val)
{
    int more, byte;

    do {
        byte = val & 0x7f;
        val >>= 7;
        more = !((val == 0 && (byte & 0x40) == 0)
                 || (val == -1 && (byte & 0x40) != 0));
        if (more) {
            byte |= 0x80;
        }
        *p++ = byte;
    } while (more);

    return p;
}





static int64_t decode_sleb128(const uint8_t **pp)
{
    const uint8_t *p = *pp;
    int64_t val = 0;
    int byte, shift = 0;

    do {
        byte = *p++;
        val |= (int64_t)(byte & 0x7f) << shift;
        shift += 7;
    } while (byte & 0x80);
    if (shift < 64 && (byte & 0x40)) {
        val |= -(int64_t)1 << shift;
    }

    *pp = p;
    return val;
}













static int encode_search(TranslationBlock *tb, uint8_t *block)
{
    uint8_t *highwater = tcg_ctx->code_gen_highwater;
    uint64_t *insn_data = tcg_ctx->gen_insn_data;
    uint16_t *insn_end_off = tcg_ctx->gen_insn_end_off;
    uint8_t *p = block;
    int i, j, n;

    for (i = 0, n = tb->icount; i < n; ++i) {
        uint64_t prev, curr;

        for (j = 0; j < TARGET_INSN_START_WORDS; ++j) {
            if (i == 0) {
                prev = (!(tb_cflags(tb) & CF_PCREL) && j == 0 ? tb->pc : 0);
            } else {
                prev = insn_data[(i - 1) * TARGET_INSN_START_WORDS + j];
            }
            curr = insn_data[i * TARGET_INSN_START_WORDS + j];
            p = encode_sleb128(p, curr - prev);
        }
        prev = (i == 0 ? 0 : insn_end_off[i - 1]);
        curr = insn_end_off[i];
        p = encode_sleb128(p, curr - prev);





        if (unlikely(p > highwater)) {
            return -1;
        }
    }

    return p - block;
}

static int cpu_unwind_data_from_tb(TranslationBlock *tb, uintptr_t host_pc,
                                   uint64_t *data)
{
    uintptr_t iter_pc = (uintptr_t)tb->tc.ptr;
    const uint8_t *p = tb->tc.ptr + tb->tc.size;
    int i, j, num_insns = tb->icount;

    host_pc -= GETPC_ADJ;

    if (host_pc < iter_pc) {
        return -1;
    }

    memset(data, 0, sizeof(uint64_t) * TARGET_INSN_START_WORDS);
    if (!(tb_cflags(tb) & CF_PCREL)) {
        data[0] = tb->pc;
    }





    for (i = 0; i < num_insns; ++i) {
        for (j = 0; j < TARGET_INSN_START_WORDS; ++j) {
            data[j] += decode_sleb128(&p);
        }
        iter_pc += decode_sleb128(&p);
        if (iter_pc > host_pc) {
            return num_insns - i;
        }
    }
    return -1;
}





void cpu_restore_state_from_tb(CPUState *cpu, TranslationBlock *tb,
                               uintptr_t host_pc)
{
    uint64_t data[TARGET_INSN_START_WORDS];
    int insns_left = cpu_unwind_data_from_tb(tb, host_pc, data);

    if (insns_left < 0) {
        return;
    }

    if (tb_cflags(tb) & CF_USE_ICOUNT) {
        assert(icount_enabled());




        cpu->neg.icount_decr.u16.low += insns_left;
    }

    cpu->cc->tcg_ops->restore_state_to_opc(cpu, tb, data);
}

bool cpu_restore_state(CPUState *cpu, uintptr_t host_pc)
{










    if (in_code_gen_buffer((const void *)(host_pc - tcg_splitwx_diff))) {
        TranslationBlock *tb = tcg_tb_lookup(host_pc);
        if (tb) {
            cpu_restore_state_from_tb(cpu, tb, host_pc);
            return true;
        }
    }
    return false;
}

bool cpu_unwind_state_data(CPUState *cpu, uintptr_t host_pc, uint64_t *data)
{
    if (in_code_gen_buffer((const void *)(host_pc - tcg_splitwx_diff))) {
        TranslationBlock *tb = tcg_tb_lookup(host_pc);
        if (tb) {
            return cpu_unwind_data_from_tb(tb, host_pc, data) >= 0;
        }
    }
    return false;
}

void page_init(void)
{
    page_table_config_init();
}





static int setjmp_gen_code(CPUArchState *env, TranslationBlock *tb,
                           vaddr pc, void *host_pc,
                           int *max_insns, int64_t *ti)
{
    int ret = sigsetjmp(tcg_ctx->jmp_trans, 0);
    if (unlikely(ret != 0)) {
        return ret;
    }

    tcg_func_start(tcg_ctx);

    CPUState *cs = env_cpu(env);
    tcg_ctx->cpu = cs;
    cs->cc->tcg_ops->translate_code(cs, tb, max_insns, pc, host_pc);

    assert(tb->size != 0);
    tcg_ctx->cpu = NULL;
    *max_insns = tb->icount;

    return tcg_gen_code(tcg_ctx, tb, pc);
}


TranslationBlock *tb_gen_code(CPUState *cpu,
                              vaddr pc, uint64_t cs_base,
                              uint32_t flags, int cflags)
{
    CPUArchState *env = cpu_env(cpu);
    TranslationBlock *tb, *existing_tb;
    tb_page_addr_t phys_pc, phys_p2;
    tcg_insn_unit *gen_code_buf;
    int gen_code_size, search_size, max_insns;
    int64_t ti;
    void *host_pc;

    assert_memory_lock();
    qemu_thread_jit_write();

    phys_pc = get_page_addr_code_hostp(env, pc, &host_pc);

    if (phys_pc == -1) {

        cflags = (cflags & ~CF_COUNT_MASK) | 1;
    }

    max_insns = cflags & CF_COUNT_MASK;
    if (max_insns == 0) {
        max_insns = TCG_MAX_INSNS;
    }
    QEMU_BUILD_BUG_ON(CF_COUNT_MASK + 1 != TCG_MAX_INSNS);

 buffer_overflow:
    assert_no_pages_locked();
    tb = tcg_tb_alloc(tcg_ctx);
    if (unlikely(!tb)) {

        tb_flush(cpu);
        mmap_unlock();

        cpu->exception_index = EXCP_INTERRUPT;
        cpu_loop_exit(cpu);
    }

    gen_code_buf = tcg_ctx->code_gen_ptr;
    tb->tc.ptr = tcg_splitwx_to_rx(gen_code_buf);
    if (!(cflags & CF_PCREL)) {
        tb->pc = pc;
    }
    tb->cs_base = cs_base;
    tb->flags = flags;
    tb->cflags = cflags;
    tb_set_page_addr0(tb, phys_pc);
    tb_set_page_addr1(tb, -1);
    if (phys_pc != -1) {
        tb_lock_page0(phys_pc);
    }

    tcg_ctx->gen_tb = tb;
    tcg_ctx->addr_type = TARGET_LONG_BITS == 32 ? TCG_TYPE_I32 : TCG_TYPE_I64;
#ifdef CONFIG_SOFTMMU
    tcg_ctx->page_bits = TARGET_PAGE_BITS;
    tcg_ctx->page_mask = TARGET_PAGE_MASK;
    tcg_ctx->tlb_dyn_max_bits = CPU_TLB_DYN_MAX_BITS;
#endif
    tcg_ctx->insn_start_words = TARGET_INSN_START_WORDS;
#ifdef TCG_GUEST_DEFAULT_MO
    tcg_ctx->guest_mo = TCG_GUEST_DEFAULT_MO;
#else
    tcg_ctx->guest_mo = TCG_MO_ALL;
#endif

 restart_translate:
    trace_translate_block(tb, pc, tb->tc.ptr);

    gen_code_size = setjmp_gen_code(env, tb, pc, host_pc, &max_insns, &ti);
    if (unlikely(gen_code_size < 0)) {
        switch (gen_code_size) {
        case -1:









            qemu_log_mask(CPU_LOG_TB_OP | CPU_LOG_TB_OP_OPT,
                          "Restarting code generation for "
                          "code_gen_buffer overflow\n");
            tb_unlock_pages(tb);
            tcg_ctx->gen_tb = NULL;
            goto buffer_overflow;

        case -2:









            assert(max_insns > 1);
            max_insns /= 2;
            qemu_log_mask(CPU_LOG_TB_OP | CPU_LOG_TB_OP_OPT,
                          "Restarting code generation with "
                          "smaller translation block (max %d insns)\n",
                          max_insns);






            phys_p2 = tb_page_addr1(tb);
            if (unlikely(phys_p2 != -1)) {
                tb_unlock_page1(phys_pc, phys_p2);
                tb_set_page_addr1(tb, -1);
            }
            goto restart_translate;

        case -3:






            qemu_log_mask(CPU_LOG_TB_OP | CPU_LOG_TB_OP_OPT,
                          "Restarting code generation with re-locked pages");
            goto restart_translate;

        default:
            g_assert_not_reached();
        }
    }
    tcg_ctx->gen_tb = NULL;

    search_size = encode_search(tb, (void *)gen_code_buf + gen_code_size);
    if (unlikely(search_size < 0)) {
        tb_unlock_pages(tb);
        goto buffer_overflow;
    }
    tb->tc.size = gen_code_size;





    perf_report_code(pc, tb, tcg_splitwx_to_rx(gen_code_buf));

    if (qemu_loglevel_mask(CPU_LOG_TB_OUT_ASM) &&
        qemu_log_in_addr_range(pc)) {
        FILE *logfile = qemu_log_trylock();
        if (logfile) {
            int code_size, data_size;
            const tcg_target_ulong *rx_data_gen_ptr;
            size_t chunk_start;
            int insn = 0;

            if (tcg_ctx->data_gen_ptr) {
                rx_data_gen_ptr = tcg_splitwx_to_rx(tcg_ctx->data_gen_ptr);
                code_size = (const void *)rx_data_gen_ptr - tb->tc.ptr;
                data_size = gen_code_size - code_size;
            } else {
                rx_data_gen_ptr = 0;
                code_size = gen_code_size;
                data_size = 0;
            }


            fprintf(logfile, "OUT: [size=%d]\n", gen_code_size);
            fprintf(logfile,
                    "  -- guest addr 0x%016" PRIx64 " + tb prologue\n",
                    tcg_ctx->gen_insn_data[insn * TARGET_INSN_START_WORDS]);
            chunk_start = tcg_ctx->gen_insn_end_off[insn];
            disas(logfile, tb->tc.ptr, chunk_start);






            while (insn < tb->icount) {
                size_t chunk_end = tcg_ctx->gen_insn_end_off[insn];
                if (chunk_end > chunk_start) {
                    fprintf(logfile, "  -- guest addr 0x%016" PRIx64 "\n",
                            tcg_ctx->gen_insn_data[insn * TARGET_INSN_START_WORDS]);
                    disas(logfile, tb->tc.ptr + chunk_start,
                          chunk_end - chunk_start);
                    chunk_start = chunk_end;
                }
                insn++;
            }

            if (chunk_start < code_size) {
                fprintf(logfile, "  -- tb slow paths + alignment\n");
                disas(logfile, tb->tc.ptr + chunk_start,
                      code_size - chunk_start);
            }


            if (data_size) {
                int i;
                fprintf(logfile, "  data: [size=%d]\n", data_size);
                for (i = 0; i < data_size / sizeof(tcg_target_ulong); i++) {
                    if (sizeof(tcg_target_ulong) == 8) {
                        fprintf(logfile,
                                "0x%08" PRIxPTR ":  .quad  0x%016" TCG_PRIlx "\n",
                                (uintptr_t)&rx_data_gen_ptr[i], rx_data_gen_ptr[i]);
                    } else if (sizeof(tcg_target_ulong) == 4) {
                        fprintf(logfile,
                                "0x%08" PRIxPTR ":  .long  0x%08" TCG_PRIlx "\n",
                                (uintptr_t)&rx_data_gen_ptr[i], rx_data_gen_ptr[i]);
                    } else {
                        qemu_build_not_reached();
                    }
                }
            }
            fprintf(logfile, "\n");
            qemu_log_unlock(logfile);
        }
    }

    qatomic_set(&tcg_ctx->code_gen_ptr, (void *)
        ROUND_UP((uintptr_t)gen_code_buf + gen_code_size + search_size,
                 CODE_GEN_ALIGN));


    qemu_spin_init(&tb->jmp_lock);
    tb->jmp_list_head = (uintptr_t)NULL;
    tb->jmp_list_next[0] = (uintptr_t)NULL;
    tb->jmp_list_next[1] = (uintptr_t)NULL;
    tb->jmp_dest[0] = (uintptr_t)NULL;
    tb->jmp_dest[1] = (uintptr_t)NULL;


    if (tb->jmp_reset_offset[0] != TB_JMP_OFFSET_INVALID) {
        tb_reset_jump(tb, 0);
    }
    if (tb->jmp_reset_offset[1] != TB_JMP_OFFSET_INVALID) {
        tb_reset_jump(tb, 1);
    }






    tcg_tb_insert(tb);















    if (tb_page_addr0(tb) == -1) {
        assert_no_pages_locked();
        return tb;
    }





    existing_tb = tb_link_page(tb);
    assert_no_pages_locked();


    if (unlikely(existing_tb != tb)) {
        uintptr_t orig_aligned = (uintptr_t)gen_code_buf;

        orig_aligned -= ROUND_UP(sizeof(*tb), qemu_icache_linesize);
        qatomic_set(&tcg_ctx->code_gen_ptr, (void *)orig_aligned);
        tcg_tb_remove(tb);
        return existing_tb;
    }
    return tb;
}


void tb_check_watchpoint(CPUState *cpu, uintptr_t retaddr)
{
    TranslationBlock *tb;

    assert_memory_lock();

    tb = tcg_tb_lookup(retaddr);
    if (tb) {

        cpu_restore_state_from_tb(cpu, tb, retaddr);
        tb_phys_invalidate(tb, -1);
    } else {


        CPUArchState *env = cpu_env(cpu);
        vaddr pc;
        uint64_t cs_base;
        tb_page_addr_t addr;
        uint32_t flags;

        cpu_get_tb_cpu_state(env, &pc, &cs_base, &flags);
        addr = get_page_addr_code(env, pc);
        if (addr != -1) {
            tb_invalidate_phys_range(addr, addr);
        }
    }
}

#ifndef CONFIG_USER_ONLY






void cpu_io_recompile(CPUState *cpu, uintptr_t retaddr)
{
    TranslationBlock *tb;
    CPUClass *cc;
    uint32_t n;

    tb = tcg_tb_lookup(retaddr);
    if (!tb) {
        cpu_abort(cpu, "cpu_io_recompile: could not find TB for pc=%p",
                  (void *)retaddr);
    }
    cpu_restore_state_from_tb(cpu, tb, retaddr);






    n = 1;
    cc = cpu->cc;
    if (cc->tcg_ops->io_recompile_replay_branch &&
        cc->tcg_ops->io_recompile_replay_branch(cpu, tb)) {
        cpu->neg.icount_decr.u16.low++;
        n = 2;
    }








    cpu->cflags_next_tb = curr_cflags(cpu) | CF_MEMI_ONLY | CF_NOIRQ | n;

    if (qemu_loglevel_mask(CPU_LOG_EXEC)) {
        vaddr pc = cpu->cc->get_pc(cpu);
        if (qemu_log_in_addr_range(pc)) {
            qemu_log("cpu_io_recompile: rewound execution of TB to %016"
                     VADDR_PRIx "\n", pc);
        }
    }

    cpu_loop_exit_noexc(cpu);
}

#endif





void tcg_flush_jmp_cache(CPUState *cpu)
{
    CPUJumpCache *jc = cpu->tb_jmp_cache;


    if (unlikely(jc == NULL)) {
        return;
    }

    for (int i = 0; i < TB_JMP_CACHE_SIZE; i++) {
        qatomic_set(&jc->array[i].tb, NULL);
    }
}
