


















#include "qemu/osdep.h"
#include "qemu/qemu-print.h"
#include "qapi/error.h"
#include "qapi/type-helpers.h"
#include "hw/core/cpu.h"
#include "accel/tcg/cpu-ops.h"
#include "trace.h"
#include "disas/disas.h"
#include "exec/cpu-common.h"
#include "exec/page-protection.h"
#include "exec/translation-block.h"
#include "tcg/tcg.h"
#include "qemu/atomic.h"
#include "qemu/rcu.h"
#include "exec/log.h"
#include "qemu/main-loop.h"
#include "exec/cpu-all.h"
#include "system/cpu-timers.h"
#include "exec/replay-core.h"
#include "system/tcg.h"
#include "exec/helper-proto-common.h"
#include "tb-jmp-cache.h"
#include "tb-hash.h"
#include "tb-context.h"
#include "tb-internal.h"
#include "internal-common.h"
#include "internal-target.h"



typedef struct SyncClocks {
    int64_t diff_clk;
    int64_t last_cpu_icount;
    int64_t realtime_clock;
} SyncClocks;

#if !defined(CONFIG_USER_ONLY)




#define VM_CLOCK_ADVANCE 3000000
#define THRESHOLD_REDUCE 1.5
#define MAX_DELAY_PRINT_RATE 2000000000LL
#define MAX_NB_PRINTS 100

int64_t max_delay;
int64_t max_advance;

static void align_clocks(SyncClocks *sc, CPUState *cpu)
{
    int64_t cpu_icount;

    if (!icount_align_option) {
        return;
    }

    cpu_icount = cpu->icount_extra + cpu->neg.icount_decr.u16.low;
    sc->diff_clk += icount_to_ns(sc->last_cpu_icount - cpu_icount);
    sc->last_cpu_icount = cpu_icount;

    if (sc->diff_clk > VM_CLOCK_ADVANCE) {
#ifndef _WIN32
        struct timespec sleep_delay, rem_delay;
        sleep_delay.tv_sec = sc->diff_clk / 1000000000LL;
        sleep_delay.tv_nsec = sc->diff_clk % 1000000000LL;
        if (nanosleep(&sleep_delay, &rem_delay) < 0) {
            sc->diff_clk = rem_delay.tv_sec * 1000000000LL + rem_delay.tv_nsec;
        } else {
            sc->diff_clk = 0;
        }
#else
        Sleep(sc->diff_clk / SCALE_MS);
        sc->diff_clk = 0;
#endif
    }
}

static void print_delay(const SyncClocks *sc)
{
    static float threshold_delay;
    static int64_t last_realtime_clock;
    static int nb_prints;

    if (icount_align_option &&
        sc->realtime_clock - last_realtime_clock >= MAX_DELAY_PRINT_RATE &&
        nb_prints < MAX_NB_PRINTS) {
        if ((-sc->diff_clk / (float)1000000000LL > threshold_delay) ||
            (-sc->diff_clk / (float)1000000000LL <
             (threshold_delay - THRESHOLD_REDUCE))) {
            threshold_delay = (-sc->diff_clk / 1000000000LL) + 1;
            qemu_printf("Warning: The guest is now late by %.1f to %.1f seconds\n",
                        threshold_delay - 1,
                        threshold_delay);
            nb_prints++;
            last_realtime_clock = sc->realtime_clock;
        }
    }
}

static void init_delay_params(SyncClocks *sc, CPUState *cpu)
{
    if (!icount_align_option) {
        return;
    }
    sc->realtime_clock = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL_RT);
    sc->diff_clk = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) - sc->realtime_clock;
    sc->last_cpu_icount
        = cpu->icount_extra + cpu->neg.icount_decr.u16.low;
    if (sc->diff_clk < max_delay) {
        max_delay = sc->diff_clk;
    }
    if (sc->diff_clk > max_advance) {
        max_advance = sc->diff_clk;
    }



    print_delay(sc);
}
#else
static void align_clocks(SyncClocks *sc, const CPUState *cpu)
{
}

static void init_delay_params(SyncClocks *sc, const CPUState *cpu)
{
}
#endif

struct tb_desc {
    vaddr pc;
    uint64_t cs_base;
    CPUArchState *env;
    tb_page_addr_t page_addr0;
    uint32_t flags;
    uint32_t cflags;
};

static bool tb_lookup_cmp(const void *p, const void *d)
{
    const TranslationBlock *tb = p;
    const struct tb_desc *desc = d;

    if ((tb_cflags(tb) & CF_PCREL || tb->pc == desc->pc) &&
        tb_page_addr0(tb) == desc->page_addr0 &&
        tb->cs_base == desc->cs_base &&
        tb->flags == desc->flags &&
        tb_cflags(tb) == desc->cflags) {

        tb_page_addr_t tb_phys_page1 = tb_page_addr1(tb);
        if (tb_phys_page1 == -1) {
            return true;
        } else {
            tb_page_addr_t phys_page1;
            vaddr virt_page1;










            virt_page1 = TARGET_PAGE_ALIGN(desc->pc);
            phys_page1 = get_page_addr_code(desc->env, virt_page1);
            if (tb_phys_page1 == phys_page1) {
                return true;
            }
        }
    }
    return false;
}

static TranslationBlock *tb_htable_lookup(CPUState *cpu, vaddr pc,
                                          uint64_t cs_base, uint32_t flags,
                                          uint32_t cflags)
{
    tb_page_addr_t phys_pc;
    struct tb_desc desc;
    uint32_t h;

    desc.env = cpu_env(cpu);
    desc.cs_base = cs_base;
    desc.flags = flags;
    desc.cflags = cflags;
    desc.pc = pc;
    phys_pc = get_page_addr_code(desc.env, pc);
    if (phys_pc == -1) {
        return NULL;
    }
    desc.page_addr0 = phys_pc;
    h = tb_hash_func(phys_pc, (cflags & CF_PCREL ? 0 : pc),
                     flags, cs_base, cflags);
    return qht_lookup_custom(&tb_ctx.htable, &desc, h, tb_lookup_cmp);
}















static inline TranslationBlock *tb_lookup(CPUState *cpu, vaddr pc,
                                          uint64_t cs_base, uint32_t flags,
                                          uint32_t cflags)
{
    TranslationBlock *tb;
    CPUJumpCache *jc;
    uint32_t hash;


    tcg_debug_assert(!(cflags & CF_INVALID));

    hash = tb_jmp_cache_hash_func(pc);
    jc = cpu->tb_jmp_cache;

    tb = qatomic_read(&jc->array[hash].tb);
    if (likely(tb &&
               jc->array[hash].pc == pc &&
               tb->cs_base == cs_base &&
               tb->flags == flags &&
               tb_cflags(tb) == cflags)) {
        goto hit;
    }

    tb = tb_htable_lookup(cpu, pc, cs_base, flags, cflags);
    if (tb == NULL) {
        return NULL;
    }

    jc->array[hash].pc = pc;
    qatomic_set(&jc->array[hash].tb, tb);

hit:




    assert((tb_cflags(tb) & CF_PCREL) || tb->pc == pc);
    return tb;
}

static void log_cpu_exec(vaddr pc, CPUState *cpu,
                         const TranslationBlock *tb)
{
    if (qemu_log_in_addr_range(pc)) {
        qemu_log_mask(CPU_LOG_EXEC,
                      "Trace %d: %p [%08" PRIx64
                      "/%016" VADDR_PRIx "/%08x/%08x] %s\n",
                      cpu->cpu_index, tb->tc.ptr, tb->cs_base, pc,
                      tb->flags, tb->cflags, lookup_symbol(pc));

        if (qemu_loglevel_mask(CPU_LOG_TB_CPU)) {
            FILE *logfile = qemu_log_trylock();
            if (logfile) {
                int flags = 0;

                if (qemu_loglevel_mask(CPU_LOG_TB_FPU)) {
                    flags |= CPU_DUMP_FPU;
                }
#if defined(TARGET_I386)
                flags |= CPU_DUMP_CCOP;
#endif
                if (qemu_loglevel_mask(CPU_LOG_TB_VPU)) {
                    flags |= CPU_DUMP_VPU;
                }
                cpu_dump_state(cpu, logfile, flags);
                qemu_log_unlock(logfile);
            }
        }
    }
}

static bool check_for_breakpoints_slow(CPUState *cpu, vaddr pc,
                                       uint32_t *cflags)
{
    CPUBreakpoint *bp;
    bool match_page = false;










    if (cpu->singlestep_enabled) {
        return false;
    }

    QTAILQ_FOREACH(bp, &cpu->breakpoints, entry) {




        if (pc == bp->pc) {
            bool match_bp = false;

            if (bp->flags & BP_GDB) {
                match_bp = true;
            } else if (bp->flags & BP_CPU) {
#ifdef CONFIG_USER_ONLY
                g_assert_not_reached();
#else
                const TCGCPUOps *tcg_ops = cpu->cc->tcg_ops;
                assert(tcg_ops->debug_check_breakpoint);
                match_bp = tcg_ops->debug_check_breakpoint(cpu);
#endif
            }

            if (match_bp) {
                cpu->exception_index = EXCP_DEBUG;
                return true;
            }
        } else if (((pc ^ bp->pc) & TARGET_PAGE_MASK) == 0) {
            match_page = true;
        }
    }













    if (match_page) {
        *cflags = (*cflags & ~CF_COUNT_MASK) | CF_NO_GOTO_TB | CF_BP_PAGE | 1;
    }
    return false;
}

static inline bool check_for_breakpoints(CPUState *cpu, vaddr pc,
                                         uint32_t *cflags)
{
    return unlikely(!QTAILQ_EMPTY(&cpu->breakpoints)) &&
        check_for_breakpoints_slow(cpu, pc, cflags);
}









const void *HELPER(lookup_tb_ptr)(CPUArchState *env)
{
    CPUState *cpu = env_cpu(env);
    TranslationBlock *tb;
    vaddr pc;
    uint64_t cs_base;
    uint32_t flags, cflags;








    cpu->neg.can_do_io = true;
    cpu_get_tb_cpu_state(env, &pc, &cs_base, &flags);

    cflags = curr_cflags(cpu);
    if (check_for_breakpoints(cpu, pc, &cflags)) {
        cpu_loop_exit(cpu);
    }

    tb = tb_lookup(cpu, pc, cs_base, flags, cflags);
    if (tb == NULL) {
        return tcg_code_gen_epilogue;
    }

    if (qemu_loglevel_mask(CPU_LOG_TB_CPU | CPU_LOG_EXEC)) {
        log_cpu_exec(pc, cpu, tb);
    }

    return tb->tc.ptr;
}


static vaddr log_pc(CPUState *cpu, const TranslationBlock *tb)
{
    if (tb_cflags(tb) & CF_PCREL) {
        return cpu->cc->get_pc(cpu);
    } else {
        return tb->pc;
    }
}











static inline TranslationBlock * QEMU_DISABLE_CFI
cpu_tb_exec(CPUState *cpu, TranslationBlock *itb, int *tb_exit)
{
    uintptr_t ret;
    TranslationBlock *last_tb;
    const void *tb_ptr = itb->tc.ptr;

    if (qemu_loglevel_mask(CPU_LOG_TB_CPU | CPU_LOG_EXEC)) {
        log_cpu_exec(log_pc(cpu, itb), cpu, itb);
    }

    qemu_thread_jit_execute();
    ret = tcg_qemu_tb_exec(cpu_env(cpu), tb_ptr);
    cpu->neg.can_do_io = true;
    qemu_plugin_disable_mem_helpers(cpu);








    last_tb = tcg_splitwx_to_rw((void *)(ret & ~TB_EXIT_MASK));
    *tb_exit = ret & TB_EXIT_MASK;

    trace_exec_tb_exit(last_tb, *tb_exit);

    if (*tb_exit > TB_EXIT_IDX1) {




        CPUClass *cc = cpu->cc;
        const TCGCPUOps *tcg_ops = cc->tcg_ops;

        if (tcg_ops->synchronize_from_tb) {
            tcg_ops->synchronize_from_tb(cpu, last_tb);
        } else {
            tcg_debug_assert(!(tb_cflags(last_tb) & CF_PCREL));
            assert(cc->set_pc);
            cc->set_pc(cpu, last_tb->pc);
        }
        if (qemu_loglevel_mask(CPU_LOG_EXEC)) {
            vaddr pc = log_pc(cpu, last_tb);
            if (qemu_log_in_addr_range(pc)) {
                qemu_log("Stopped execution of TB chain before %p [%016"
                         VADDR_PRIx "] %s\n",
                         last_tb->tc.ptr, pc, lookup_symbol(pc));
            }
        }
    }






    if (unlikely(cpu->singlestep_enabled) && cpu->exception_index == -1) {
        cpu->exception_index = EXCP_DEBUG;
        cpu_loop_exit(cpu);
    }

    return last_tb;
}


static void cpu_exec_enter(CPUState *cpu)
{
    const TCGCPUOps *tcg_ops = cpu->cc->tcg_ops;

    if (tcg_ops->cpu_exec_enter) {
        tcg_ops->cpu_exec_enter(cpu);
    }
}

static void cpu_exec_exit(CPUState *cpu)
{
    const TCGCPUOps *tcg_ops = cpu->cc->tcg_ops;

    if (tcg_ops->cpu_exec_exit) {
        tcg_ops->cpu_exec_exit(cpu);
    }
}

static void cpu_exec_longjmp_cleanup(CPUState *cpu)
{

    g_assert(cpu == current_cpu);

#ifdef CONFIG_USER_ONLY
    clear_helper_retaddr();
    if (have_mmap_lock()) {
        mmap_unlock();
    }
#else















    if (tcg_ctx->gen_tb) {
        tb_unlock_pages(tcg_ctx->gen_tb);
        tcg_ctx->gen_tb = NULL;
    }
#endif
    if (bql_locked()) {
        bql_unlock();
    }
    assert_no_pages_locked();
}

void cpu_exec_step_atomic(CPUState *cpu)
{
    CPUArchState *env = cpu_env(cpu);
    TranslationBlock *tb;
    vaddr pc;
    uint64_t cs_base;
    uint32_t flags, cflags;
    int tb_exit;

    if (sigsetjmp(cpu->jmp_env, 0) == 0) {
        start_exclusive();
        g_assert(cpu == current_cpu);
        g_assert(!cpu->running);
        cpu->running = true;

        cpu_get_tb_cpu_state(env, &pc, &cs_base, &flags);

        cflags = curr_cflags(cpu);

        cflags &= ~CF_PARALLEL;

        cflags |= CF_NO_GOTO_TB | CF_NO_GOTO_PTR | 1;







        tb = tb_lookup(cpu, pc, cs_base, flags, cflags);
        if (tb == NULL) {
            mmap_lock();
            tb = tb_gen_code(cpu, pc, cs_base, flags, cflags);
            mmap_unlock();
        }

        cpu_exec_enter(cpu);

        trace_exec_tb(tb, pc);
        cpu_tb_exec(cpu, tb, &tb_exit);
        cpu_exec_exit(cpu);
    } else {
        cpu_exec_longjmp_cleanup(cpu);
    }






    g_assert(cpu_in_exclusive_context(cpu));
    cpu->running = false;
    end_exclusive();
}

void tb_set_jmp_target(TranslationBlock *tb, int n, uintptr_t addr)
{





    const TranslationBlock *c_tb = tcg_splitwx_to_rx(tb);
    uintptr_t offset = tb->jmp_insn_offset[n];
    uintptr_t jmp_rx = (uintptr_t)tb->tc.ptr + offset;
    uintptr_t jmp_rw = jmp_rx - tcg_splitwx_diff;

    tb->jmp_target_addr[n] = addr;
    tb_target_set_jmp_target(c_tb, n, jmp_rx, jmp_rw);
}

static inline void tb_add_jump(TranslationBlock *tb, int n,
                               TranslationBlock *tb_next)
{
    uintptr_t old;

    qemu_thread_jit_write();
    assert(n < ARRAY_SIZE(tb->jmp_list_next));
    qemu_spin_lock(&tb_next->jmp_lock);


    if (tb_next->cflags & CF_INVALID) {
        goto out_unlock_next;
    }

    old = qatomic_cmpxchg(&tb->jmp_dest[n], (uintptr_t)NULL,
                          (uintptr_t)tb_next);
    if (old) {
        goto out_unlock_next;
    }


    tb_set_jmp_target(tb, n, (uintptr_t)tb_next->tc.ptr);


    tb->jmp_list_next[n] = tb_next->jmp_list_head;
    tb_next->jmp_list_head = (uintptr_t)tb | n;

    qemu_spin_unlock(&tb_next->jmp_lock);

    qemu_log_mask(CPU_LOG_EXEC, "Linking TBs %p index %d -> %p\n",
                  tb->tc.ptr, n, tb_next->tc.ptr);
    return;

 out_unlock_next:
    qemu_spin_unlock(&tb_next->jmp_lock);
    return;
}

static inline bool cpu_handle_halt(CPUState *cpu)
{
#ifndef CONFIG_USER_ONLY
    if (cpu->halted) {
        const TCGCPUOps *tcg_ops = cpu->cc->tcg_ops;
        bool leave_halt = tcg_ops->cpu_exec_halt(cpu);

        if (!leave_halt) {
            return true;
        }

        cpu->halted = 0;
    }
#endif

    return false;
}

static inline void cpu_handle_debug_exception(CPUState *cpu)
{
    const TCGCPUOps *tcg_ops = cpu->cc->tcg_ops;
    CPUWatchpoint *wp;

    if (!cpu->watchpoint_hit) {
        QTAILQ_FOREACH(wp, &cpu->watchpoints, entry) {
            wp->flags &= ~BP_WATCHPOINT_HIT;
        }
    }

    if (tcg_ops->debug_excp_handler) {
        tcg_ops->debug_excp_handler(cpu);
    }
}

static inline bool cpu_handle_exception(CPUState *cpu, int *ret)
{
    if (cpu->exception_index < 0) {
#ifndef CONFIG_USER_ONLY
        if (replay_has_exception()
            && cpu->neg.icount_decr.u16.low + cpu->icount_extra == 0) {

            cpu->cflags_next_tb = (curr_cflags(cpu) & ~CF_USE_ICOUNT)
                | CF_NOIRQ | 1;
        }
#endif
        return false;
    }

    if (cpu->exception_index >= EXCP_INTERRUPT) {

        *ret = cpu->exception_index;
        if (*ret == EXCP_DEBUG) {
            cpu_handle_debug_exception(cpu);
        }
        cpu->exception_index = -1;
        return true;
    }

#if defined(CONFIG_USER_ONLY)




#if defined(TARGET_I386)
    const TCGCPUOps *tcg_ops = cpu->cc->tcg_ops;
    tcg_ops->fake_user_interrupt(cpu);
#endif
    *ret = cpu->exception_index;
    cpu->exception_index = -1;
    return true;
#else
    if (replay_exception()) {
        const TCGCPUOps *tcg_ops = cpu->cc->tcg_ops;

        bql_lock();
        tcg_ops->do_interrupt(cpu);
        bql_unlock();
        cpu->exception_index = -1;

        if (unlikely(cpu->singlestep_enabled)) {





            *ret = EXCP_DEBUG;
            cpu_handle_debug_exception(cpu);
            return true;
        }
    } else if (!replay_has_interrupt()) {

        *ret = EXCP_INTERRUPT;
        return true;
    }
#endif

    return false;
}

static inline bool icount_exit_request(CPUState *cpu)
{
    if (!icount_enabled()) {
        return false;
    }
    if (cpu->cflags_next_tb != -1 && !(cpu->cflags_next_tb & CF_USE_ICOUNT)) {
        return false;
    }
    return cpu->neg.icount_decr.u16.low + cpu->icount_extra == 0;
}

static inline bool cpu_handle_interrupt(CPUState *cpu,
                                        TranslationBlock **last_tb)
{





    if (cpu->cflags_next_tb != -1 && cpu->cflags_next_tb & CF_NOIRQ) {
        return false;
    }






    qatomic_set_mb(&cpu->neg.icount_decr.u16.high, 0);

    if (unlikely(qatomic_read(&cpu->interrupt_request))) {
        int interrupt_request;
        bql_lock();
        interrupt_request = cpu->interrupt_request;
        if (unlikely(cpu->singlestep_enabled & SSTEP_NOIRQ)) {

            interrupt_request &= ~CPU_INTERRUPT_SSTEP_MASK;
        }
        if (interrupt_request & CPU_INTERRUPT_DEBUG) {
            cpu->interrupt_request &= ~CPU_INTERRUPT_DEBUG;
            cpu->exception_index = EXCP_DEBUG;
            bql_unlock();
            return true;
        }
#if !defined(CONFIG_USER_ONLY)
        if (replay_mode == REPLAY_MODE_PLAY && !replay_has_interrupt()) {

        } else if (interrupt_request & CPU_INTERRUPT_HALT) {
            replay_interrupt();
            cpu->interrupt_request &= ~CPU_INTERRUPT_HALT;
            cpu->halted = 1;
            cpu->exception_index = EXCP_HLT;
            bql_unlock();
            return true;
        }
#if defined(TARGET_I386)
        else if (interrupt_request & CPU_INTERRUPT_INIT) {
            X86CPU *x86_cpu = X86_CPU(cpu);
            CPUArchState *env = &x86_cpu->env;
            replay_interrupt();
            cpu_svm_check_intercept_param(env, SVM_EXIT_INIT, 0, 0);
            do_cpu_init(x86_cpu);
            cpu->exception_index = EXCP_HALTED;
            bql_unlock();
            return true;
        }
#else
        else if (interrupt_request & CPU_INTERRUPT_RESET) {
            replay_interrupt();
            cpu_reset(cpu);
            bql_unlock();
            return true;
        }
#endif




        else {
            const TCGCPUOps *tcg_ops = cpu->cc->tcg_ops;

            if (tcg_ops->cpu_exec_interrupt(cpu, interrupt_request)) {
                if (!tcg_ops->need_replay_interrupt ||
                    tcg_ops->need_replay_interrupt(interrupt_request)) {
                    replay_interrupt();
                }





                if (unlikely(cpu->singlestep_enabled)) {
                    cpu->exception_index = EXCP_DEBUG;
                    bql_unlock();
                    return true;
                }
                cpu->exception_index = -1;
                *last_tb = NULL;
            }


            interrupt_request = cpu->interrupt_request;
        }
#endif
        if (interrupt_request & CPU_INTERRUPT_EXITTB) {
            cpu->interrupt_request &= ~CPU_INTERRUPT_EXITTB;


            *last_tb = NULL;
        }


        bql_unlock();
    }


    if (unlikely(qatomic_read(&cpu->exit_request)) || icount_exit_request(cpu)) {
        qatomic_set(&cpu->exit_request, 0);
        if (cpu->exception_index == -1) {
            cpu->exception_index = EXCP_INTERRUPT;
        }
        return true;
    }

    return false;
}

static inline void cpu_loop_exec_tb(CPUState *cpu, TranslationBlock *tb,
                                    vaddr pc, TranslationBlock **last_tb,
                                    int *tb_exit)
{
    trace_exec_tb(tb, pc);
    tb = cpu_tb_exec(cpu, tb, tb_exit);
    if (*tb_exit != TB_EXIT_REQUESTED) {
        *last_tb = tb;
        return;
    }

    *last_tb = NULL;
    if (cpu_loop_exit_requested(cpu)) {







        return;
    }


    assert(icount_enabled());
#ifndef CONFIG_USER_ONLY

    icount_update(cpu);

    int32_t insns_left = MIN(0xffff, cpu->icount_budget);
    cpu->neg.icount_decr.u16.low = insns_left;
    cpu->icount_extra = cpu->icount_budget - insns_left;






    if (insns_left > 0 && insns_left < tb->icount)  {
        assert(insns_left <= CF_COUNT_MASK);
        assert(cpu->icount_extra == 0);
        cpu->cflags_next_tb = (tb->cflags & ~CF_COUNT_MASK) | insns_left;
    }
#endif
}



static int __attribute__((noinline))
cpu_exec_loop(CPUState *cpu, SyncClocks *sc)
{
    int ret;


    while (!cpu_handle_exception(cpu, &ret)) {
        TranslationBlock *last_tb = NULL;
        int tb_exit = 0;

        while (!cpu_handle_interrupt(cpu, &last_tb)) {
            TranslationBlock *tb;
            vaddr pc;
            uint64_t cs_base;
            uint32_t flags, cflags;

            cpu_get_tb_cpu_state(cpu_env(cpu), &pc, &cs_base, &flags);








            cflags = cpu->cflags_next_tb;
            if (cflags == -1) {
                cflags = curr_cflags(cpu);
            } else {
                cpu->cflags_next_tb = -1;
            }

            if (check_for_breakpoints(cpu, pc, &cflags)) {
                break;
            }

            tb = tb_lookup(cpu, pc, cs_base, flags, cflags);
            if (tb == NULL) {
                CPUJumpCache *jc;
                uint32_t h;

                mmap_lock();
                tb = tb_gen_code(cpu, pc, cs_base, flags, cflags);
                mmap_unlock();





                h = tb_jmp_cache_hash_func(pc);
                jc = cpu->tb_jmp_cache;
                jc->array[h].pc = pc;
                qatomic_set(&jc->array[h].tb, tb);
            }

#ifndef CONFIG_USER_ONLY






            if (tb_page_addr1(tb) != -1) {
                last_tb = NULL;
            }
#endif

            if (last_tb) {
                tb_add_jump(last_tb, tb_exit, tb);
            }

            cpu_loop_exec_tb(cpu, tb, pc, &last_tb, &tb_exit);



            align_clocks(sc, cpu);
        }
    }
    return ret;
}

static int cpu_exec_setjmp(CPUState *cpu, SyncClocks *sc)
{

    if (unlikely(sigsetjmp(cpu->jmp_env, 0) != 0)) {
        cpu_exec_longjmp_cleanup(cpu);
    }

    return cpu_exec_loop(cpu, sc);
}

int cpu_exec(CPUState *cpu)
{
    int ret;
    SyncClocks sc = { 0 };


    current_cpu = cpu;

    if (cpu_handle_halt(cpu)) {
        return EXCP_HALTED;
    }

    RCU_READ_LOCK_GUARD();
    cpu_exec_enter(cpu);







    init_delay_params(&sc, cpu);

    ret = cpu_exec_setjmp(cpu, &sc);

    cpu_exec_exit(cpu);
    return ret;
}

bool tcg_exec_realizefn(CPUState *cpu, Error **errp)
{
    static bool tcg_target_initialized;

    if (!tcg_target_initialized) {

        const TCGCPUOps *tcg_ops = cpu->cc->tcg_ops;
#ifndef CONFIG_USER_ONLY
        assert(tcg_ops->cpu_exec_halt);
        assert(tcg_ops->cpu_exec_interrupt);
#endif
        assert(tcg_ops->translate_code);
        tcg_ops->initialize();
        tcg_target_initialized = true;
    }

    cpu->tb_jmp_cache = g_new0(CPUJumpCache, 1);
    tlb_init(cpu);
#ifndef CONFIG_USER_ONLY
    tcg_iommu_init_notifier_list(cpu);
#endif


    return true;
}


void tcg_exec_unrealizefn(CPUState *cpu)
{
#ifndef CONFIG_USER_ONLY
    tcg_iommu_free_notifier_list(cpu);
#endif

    tlb_destroy(cpu);
    g_free_rcu(cpu->tb_jmp_cache, rcu);
}
