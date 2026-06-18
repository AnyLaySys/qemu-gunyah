


















#include "qemu/osdep.h"
#include "qemu/interval-tree.h"
#include "qemu/qtree.h"
#include "exec/cputlb.h"
#include "exec/log.h"
#include "exec/exec-all.h"
#include "exec/page-protection.h"
#include "exec/tb-flush.h"
#include "tb-internal.h"
#include "system/tcg.h"
#include "tcg/tcg.h"
#include "tb-hash.h"
#include "tb-context.h"
#include "tb-internal.h"
#include "internal-common.h"
#include "internal-target.h"
#ifdef CONFIG_USER_ONLY
#include "user/page-protection.h"
#endif



#define TB_FOR_EACH_TAGGED(head, tb, n, field)                          \
    for (n = (head) & 1, tb = (TranslationBlock *)((head) & ~1);        \
         tb; tb = (TranslationBlock *)tb->field[n], n = (uintptr_t)tb & 1, \
             tb = (TranslationBlock *)((uintptr_t)tb & ~1))

#define TB_FOR_EACH_JMP(head_tb, tb, n)                                 \
    TB_FOR_EACH_TAGGED((head_tb)->jmp_list_head, tb, n, jmp_list_next)

static bool tb_cmp(const void *ap, const void *bp)
{
    const TranslationBlock *a = ap;
    const TranslationBlock *b = bp;

    return ((tb_cflags(a) & CF_PCREL || a->pc == b->pc) &&
            a->cs_base == b->cs_base &&
            a->flags == b->flags &&
            (tb_cflags(a) & ~CF_INVALID) == (tb_cflags(b) & ~CF_INVALID) &&
            tb_page_addr0(a) == tb_page_addr0(b) &&
            tb_page_addr1(a) == tb_page_addr1(b));
}

void tb_htable_init(void)
{
    unsigned int mode = QHT_MODE_AUTO_RESIZE;

    qht_init(&tb_ctx.htable, tb_cmp, CODE_GEN_HTABLE_SIZE, mode);
}

typedef struct PageDesc PageDesc;

#ifdef CONFIG_USER_ONLY




#define assert_page_locked(pd) tcg_debug_assert(have_mmap_lock())

static inline void tb_lock_pages(const TranslationBlock *tb) { }






static IntervalTreeRoot tb_root;

static void tb_remove_all(void)
{
    assert_memory_lock();
    memset(&tb_root, 0, sizeof(tb_root));
}


static void tb_record(TranslationBlock *tb)
{
    vaddr addr;
    int flags;

    assert_memory_lock();
    tb->itree.last = tb->itree.start + tb->size - 1;


    addr = tb_page_addr0(tb);
    flags = page_get_flags(addr);
    assert(!(flags & PAGE_WRITE));

    addr = tb_page_addr1(tb);
    if (addr != -1) {
        flags = page_get_flags(addr);
        assert(!(flags & PAGE_WRITE));
    }

    interval_tree_insert(&tb->itree, &tb_root);
}


static void tb_remove(TranslationBlock *tb)
{
    assert_memory_lock();
    interval_tree_remove(&tb->itree, &tb_root);
}


#define PAGE_FOR_EACH_TB(start, last, pagedesc, T, N)   \
    for (T = foreach_tb_first(start, last),             \
         N = foreach_tb_next(T, start, last);           \
         T != NULL;                                     \
         T = N, N = foreach_tb_next(N, start, last))

typedef TranslationBlock *PageForEachNext;

static PageForEachNext foreach_tb_first(tb_page_addr_t start,
                                        tb_page_addr_t last)
{
    IntervalTreeNode *n = interval_tree_iter_first(&tb_root, start, last);
    return n ? container_of(n, TranslationBlock, itree) : NULL;
}

static PageForEachNext foreach_tb_next(PageForEachNext tb,
                                       tb_page_addr_t start,
                                       tb_page_addr_t last)
{
    IntervalTreeNode *n;

    if (tb) {
        n = interval_tree_iter_next(&tb->itree, start, last);
        if (n) {
            return container_of(n, TranslationBlock, itree);
        }
    }
    return NULL;
}

#else



#if HOST_LONG_BITS < TARGET_PHYS_ADDR_SPACE_BITS
# define L1_MAP_ADDR_SPACE_BITS  HOST_LONG_BITS
#else
# define L1_MAP_ADDR_SPACE_BITS  TARGET_PHYS_ADDR_SPACE_BITS
#endif


#define V_L2_BITS 10
#define V_L2_SIZE (1 << V_L2_BITS)




static int v_l1_size;
static int v_l1_shift;
static int v_l2_levels;





#define V_L1_MIN_BITS 4
#define V_L1_MAX_BITS (V_L2_BITS + 3)
#define V_L1_MAX_SIZE (1 << V_L1_MAX_BITS)

static void *l1_map[V_L1_MAX_SIZE];

struct PageDesc {
    QemuSpin lock;

    uintptr_t first_tb;
};

void page_table_config_init(void)
{
    uint32_t v_l1_bits;

    assert(TARGET_PAGE_BITS);

    v_l1_bits = (L1_MAP_ADDR_SPACE_BITS - TARGET_PAGE_BITS) % V_L2_BITS;
    if (v_l1_bits < V_L1_MIN_BITS) {
        v_l1_bits += V_L2_BITS;
    }

    v_l1_size = 1 << v_l1_bits;
    v_l1_shift = L1_MAP_ADDR_SPACE_BITS - TARGET_PAGE_BITS - v_l1_bits;
    v_l2_levels = v_l1_shift / V_L2_BITS - 1;

    assert(v_l1_bits <= V_L1_MAX_BITS);
    assert(v_l1_shift % V_L2_BITS == 0);
    assert(v_l2_levels >= 0);
}

static PageDesc *page_find_alloc(tb_page_addr_t index, bool alloc)
{
    PageDesc *pd;
    void **lp;


    lp = l1_map + ((index >> v_l1_shift) & (v_l1_size - 1));


    for (int i = v_l2_levels; i > 0; i--) {
        void **p = qatomic_rcu_read(lp);

        if (p == NULL) {
            void *existing;

            if (!alloc) {
                return NULL;
            }
            p = g_new0(void *, V_L2_SIZE);
            existing = qatomic_cmpxchg(lp, NULL, p);
            if (unlikely(existing)) {
                g_free(p);
                p = existing;
            }
        }

        lp = p + ((index >> (i * V_L2_BITS)) & (V_L2_SIZE - 1));
    }

    pd = qatomic_rcu_read(lp);
    if (pd == NULL) {
        void *existing;

        if (!alloc) {
            return NULL;
        }

        pd = g_new0(PageDesc, V_L2_SIZE);
        for (int i = 0; i < V_L2_SIZE; i++) {
            qemu_spin_init(&pd[i].lock);
        }

        existing = qatomic_cmpxchg(lp, NULL, pd);
        if (unlikely(existing)) {
            for (int i = 0; i < V_L2_SIZE; i++) {
                qemu_spin_destroy(&pd[i].lock);
            }
            g_free(pd);
            pd = existing;
        }
    }

    return pd + (index & (V_L2_SIZE - 1));
}

static inline PageDesc *page_find(tb_page_addr_t index)
{
    return page_find_alloc(index, false);
}














struct page_entry {
    PageDesc *pd;
    tb_page_addr_t index;
    bool locked;
};





















struct page_collection {
    QTree *tree;
    struct page_entry *max;
};

typedef int PageForEachNext;
#define PAGE_FOR_EACH_TB(start, last, pagedesc, tb, n) \
    TB_FOR_EACH_TAGGED((pagedesc)->first_tb, tb, n, page_next)

#ifdef CONFIG_DEBUG_TCG


static GHashTable *ht_pages_locked_debug;

static void ht_pages_locked_debug_init(void)
{
    if (ht_pages_locked_debug) {
        return;
    }
    ht_pages_locked_debug = g_hash_table_new(NULL, NULL);
}

static bool page_is_locked(const PageDesc *pd)
{
    PageDesc *found;

    ht_pages_locked_debug_init();
    found = g_hash_table_lookup(ht_pages_locked_debug, pd);
    return !!found;
}

static void page_lock__debug(PageDesc *pd)
{
    ht_pages_locked_debug_init();
    g_assert(!page_is_locked(pd));
    g_hash_table_insert(ht_pages_locked_debug, pd, pd);
}

static void page_unlock__debug(const PageDesc *pd)
{
    bool removed;

    ht_pages_locked_debug_init();
    g_assert(page_is_locked(pd));
    removed = g_hash_table_remove(ht_pages_locked_debug, pd);
    g_assert(removed);
}

static void do_assert_page_locked(const PageDesc *pd,
                                  const char *file, int line)
{
    if (unlikely(!page_is_locked(pd))) {
        error_report("assert_page_lock: PageDesc %p not locked @ %s:%d",
                     pd, file, line);
        abort();
    }
}
#define assert_page_locked(pd) do_assert_page_locked(pd, __FILE__, __LINE__)

void assert_no_pages_locked(void)
{
    ht_pages_locked_debug_init();
    g_assert(g_hash_table_size(ht_pages_locked_debug) == 0);
}

#else

static inline void page_lock__debug(const PageDesc *pd) { }
static inline void page_unlock__debug(const PageDesc *pd) { }
static inline void assert_page_locked(const PageDesc *pd) { }

#endif

static void page_lock(PageDesc *pd)
{
    page_lock__debug(pd);
    qemu_spin_lock(&pd->lock);
}


static bool page_trylock(PageDesc *pd)
{
    bool busy = qemu_spin_trylock(&pd->lock);
    if (!busy) {
        page_lock__debug(pd);
    }
    return busy;
}

static void page_unlock(PageDesc *pd)
{
    qemu_spin_unlock(&pd->lock);
    page_unlock__debug(pd);
}

void tb_lock_page0(tb_page_addr_t paddr)
{
    page_lock(page_find_alloc(paddr >> TARGET_PAGE_BITS, true));
}

void tb_lock_page1(tb_page_addr_t paddr0, tb_page_addr_t paddr1)
{
    tb_page_addr_t pindex0 = paddr0 >> TARGET_PAGE_BITS;
    tb_page_addr_t pindex1 = paddr1 >> TARGET_PAGE_BITS;
    PageDesc *pd0, *pd1;

    if (pindex0 == pindex1) {

        return;
    }

    pd1 = page_find_alloc(pindex1, true);
    if (pindex0 < pindex1) {

        page_lock(pd1);
        return;
    }


    if (!page_trylock(pd1)) {
        return;
    }





    pd0 = page_find_alloc(pindex0, false);
    page_unlock(pd0);
    page_lock(pd1);
    page_lock(pd0);
    siglongjmp(tcg_ctx->jmp_trans, -3);
}

void tb_unlock_page1(tb_page_addr_t paddr0, tb_page_addr_t paddr1)
{
    tb_page_addr_t pindex0 = paddr0 >> TARGET_PAGE_BITS;
    tb_page_addr_t pindex1 = paddr1 >> TARGET_PAGE_BITS;

    if (pindex0 != pindex1) {
        page_unlock(page_find_alloc(pindex1, false));
    }
}

static void tb_lock_pages(TranslationBlock *tb)
{
    tb_page_addr_t paddr0 = tb_page_addr0(tb);
    tb_page_addr_t paddr1 = tb_page_addr1(tb);
    tb_page_addr_t pindex0 = paddr0 >> TARGET_PAGE_BITS;
    tb_page_addr_t pindex1 = paddr1 >> TARGET_PAGE_BITS;

    if (unlikely(paddr0 == -1)) {
        return;
    }
    if (unlikely(paddr1 != -1) && pindex0 != pindex1) {
        if (pindex0 < pindex1) {
            page_lock(page_find_alloc(pindex0, true));
            page_lock(page_find_alloc(pindex1, true));
            return;
        }
        page_lock(page_find_alloc(pindex1, true));
    }
    page_lock(page_find_alloc(pindex0, true));
}

void tb_unlock_pages(TranslationBlock *tb)
{
    tb_page_addr_t paddr0 = tb_page_addr0(tb);
    tb_page_addr_t paddr1 = tb_page_addr1(tb);
    tb_page_addr_t pindex0 = paddr0 >> TARGET_PAGE_BITS;
    tb_page_addr_t pindex1 = paddr1 >> TARGET_PAGE_BITS;

    if (unlikely(paddr0 == -1)) {
        return;
    }
    if (unlikely(paddr1 != -1) && pindex0 != pindex1) {
        page_unlock(page_find_alloc(pindex1, false));
    }
    page_unlock(page_find_alloc(pindex0, false));
}

static inline struct page_entry *
page_entry_new(PageDesc *pd, tb_page_addr_t index)
{
    struct page_entry *pe = g_malloc(sizeof(*pe));

    pe->index = index;
    pe->pd = pd;
    pe->locked = false;
    return pe;
}

static void page_entry_destroy(gpointer p)
{
    struct page_entry *pe = p;

    g_assert(pe->locked);
    page_unlock(pe->pd);
    g_free(pe);
}


static bool page_entry_trylock(struct page_entry *pe)
{
    bool busy = page_trylock(pe->pd);
    if (!busy) {
        g_assert(!pe->locked);
        pe->locked = true;
    }
    return busy;
}

static void do_page_entry_lock(struct page_entry *pe)
{
    page_lock(pe->pd);
    g_assert(!pe->locked);
    pe->locked = true;
}

static gboolean page_entry_lock(gpointer key, gpointer value, gpointer data)
{
    struct page_entry *pe = value;

    do_page_entry_lock(pe);
    return FALSE;
}

static gboolean page_entry_unlock(gpointer key, gpointer value, gpointer data)
{
    struct page_entry *pe = value;

    if (pe->locked) {
        pe->locked = false;
        page_unlock(pe->pd);
    }
    return FALSE;
}





static bool page_trylock_add(struct page_collection *set, tb_page_addr_t addr)
{
    tb_page_addr_t index = addr >> TARGET_PAGE_BITS;
    struct page_entry *pe;
    PageDesc *pd;

    pe = q_tree_lookup(set->tree, &index);
    if (pe) {
        return false;
    }

    pd = page_find(index);
    if (pd == NULL) {
        return false;
    }

    pe = page_entry_new(pd, index);
    q_tree_insert(set->tree, &pe->index, pe);





    if (set->max == NULL || pe->index > set->max->index) {
        set->max = pe;
        do_page_entry_lock(pe);
        return false;
    }




    return page_entry_trylock(pe);
}

static gint tb_page_addr_cmp(gconstpointer ap, gconstpointer bp, gpointer udata)
{
    tb_page_addr_t a = *(const tb_page_addr_t *)ap;
    tb_page_addr_t b = *(const tb_page_addr_t *)bp;

    if (a == b) {
        return 0;
    } else if (a < b) {
        return -1;
    }
    return 1;
}






static struct page_collection *page_collection_lock(tb_page_addr_t start,
                                                    tb_page_addr_t last)
{
    struct page_collection *set = g_malloc(sizeof(*set));
    tb_page_addr_t index;
    PageDesc *pd;

    start >>= TARGET_PAGE_BITS;
    last >>= TARGET_PAGE_BITS;
    g_assert(start <= last);

    set->tree = q_tree_new_full(tb_page_addr_cmp, NULL, NULL,
                                page_entry_destroy);
    set->max = NULL;
    assert_no_pages_locked();

 retry:
    q_tree_foreach(set->tree, page_entry_lock, NULL);

    for (index = start; index <= last; index++) {
        TranslationBlock *tb;
        PageForEachNext n;

        pd = page_find(index);
        if (pd == NULL) {
            continue;
        }
        if (page_trylock_add(set, index << TARGET_PAGE_BITS)) {
            q_tree_foreach(set->tree, page_entry_unlock, NULL);
            goto retry;
        }
        assert_page_locked(pd);
        PAGE_FOR_EACH_TB(unused, unused, pd, tb, n) {
            if (page_trylock_add(set, tb_page_addr0(tb)) ||
                (tb_page_addr1(tb) != -1 &&
                 page_trylock_add(set, tb_page_addr1(tb)))) {

                q_tree_foreach(set->tree, page_entry_unlock, NULL);
                goto retry;
            }
        }
    }
    return set;
}

static void page_collection_unlock(struct page_collection *set)
{

    q_tree_destroy(set->tree);
    g_free(set);
}


static void tb_remove_all_1(int level, void **lp)
{
    int i;

    if (*lp == NULL) {
        return;
    }
    if (level == 0) {
        PageDesc *pd = *lp;

        for (i = 0; i < V_L2_SIZE; ++i) {
            page_lock(&pd[i]);
            pd[i].first_tb = (uintptr_t)NULL;
            page_unlock(&pd[i]);
        }
    } else {
        void **pp = *lp;

        for (i = 0; i < V_L2_SIZE; ++i) {
            tb_remove_all_1(level - 1, pp + i);
        }
    }
}

static void tb_remove_all(void)
{
    int i, l1_sz = v_l1_size;

    for (i = 0; i < l1_sz; i++) {
        tb_remove_all_1(v_l2_levels, l1_map + i);
    }
}





static void tb_page_add(PageDesc *p, TranslationBlock *tb, unsigned int n)
{
    bool page_already_protected;

    assert_page_locked(p);

    tb->page_next[n] = p->first_tb;
    page_already_protected = p->first_tb != 0;
    p->first_tb = (uintptr_t)tb | n;






    if (!page_already_protected) {
        tlb_protect_code(tb->page_addr[n] & TARGET_PAGE_MASK);
    }
}

static void tb_record(TranslationBlock *tb)
{
    tb_page_addr_t paddr0 = tb_page_addr0(tb);
    tb_page_addr_t paddr1 = tb_page_addr1(tb);
    tb_page_addr_t pindex0 = paddr0 >> TARGET_PAGE_BITS;
    tb_page_addr_t pindex1 = paddr1 >> TARGET_PAGE_BITS;

    assert(paddr0 != -1);
    if (unlikely(paddr1 != -1) && pindex0 != pindex1) {
        tb_page_add(page_find_alloc(pindex1, false), tb, 1);
    }
    tb_page_add(page_find_alloc(pindex0, false), tb, 0);
}

static void tb_page_remove(PageDesc *pd, TranslationBlock *tb)
{
    TranslationBlock *tb1;
    uintptr_t *pprev;
    PageForEachNext n1;

    assert_page_locked(pd);
    pprev = &pd->first_tb;
    PAGE_FOR_EACH_TB(unused, unused, pd, tb1, n1) {
        if (tb1 == tb) {
            *pprev = tb1->page_next[n1];
            return;
        }
        pprev = &tb1->page_next[n1];
    }
    g_assert_not_reached();
}

static void tb_remove(TranslationBlock *tb)
{
    tb_page_addr_t paddr0 = tb_page_addr0(tb);
    tb_page_addr_t paddr1 = tb_page_addr1(tb);
    tb_page_addr_t pindex0 = paddr0 >> TARGET_PAGE_BITS;
    tb_page_addr_t pindex1 = paddr1 >> TARGET_PAGE_BITS;

    assert(paddr0 != -1);
    if (unlikely(paddr1 != -1) && pindex0 != pindex1) {
        tb_page_remove(page_find_alloc(pindex1, false), tb);
    }
    tb_page_remove(page_find_alloc(pindex0, false), tb);
}
#endif


static void do_tb_flush(CPUState *cpu, run_on_cpu_data tb_flush_count)
{
    bool did_flush = false;

    mmap_lock();

    if (tb_ctx.tb_flush_count != tb_flush_count.host_int) {
        goto done;
    }
    did_flush = true;

    CPU_FOREACH(cpu) {
        tcg_flush_jmp_cache(cpu);
    }

    qht_reset_size(&tb_ctx.htable, CODE_GEN_HTABLE_SIZE);
    tb_remove_all();

    tcg_region_reset_all();

    qatomic_inc(&tb_ctx.tb_flush_count);

done:
    mmap_unlock();
    if (did_flush) {
        qemu_plugin_flush_cb();
    }
}

void tb_flush(CPUState *cpu)
{
    if (tcg_enabled()) {
        unsigned tb_flush_count = qatomic_read(&tb_ctx.tb_flush_count);

        if (cpu_in_serial_context(cpu)) {
            do_tb_flush(cpu, RUN_ON_CPU_HOST_INT(tb_flush_count));
        } else {
            async_safe_run_on_cpu(cpu, do_tb_flush,
                                  RUN_ON_CPU_HOST_INT(tb_flush_count));
        }
    }
}


static inline void tb_remove_from_jmp_list(TranslationBlock *orig, int n_orig)
{
    uintptr_t ptr, ptr_locked;
    TranslationBlock *dest;
    TranslationBlock *tb;
    uintptr_t *pprev;
    int n;


    ptr = qatomic_or_fetch(&orig->jmp_dest[n_orig], 1);
    dest = (TranslationBlock *)(ptr & ~1);
    if (dest == NULL) {
        return;
    }

    qemu_spin_lock(&dest->jmp_lock);




    ptr_locked = qatomic_read(&orig->jmp_dest[n_orig]);
    if (ptr_locked != ptr) {
        qemu_spin_unlock(&dest->jmp_lock);





        g_assert(ptr_locked == 1 && dest->cflags & CF_INVALID);
        return;
    }




    pprev = &dest->jmp_list_head;
    TB_FOR_EACH_JMP(dest, tb, n) {
        if (tb == orig && n == n_orig) {
            *pprev = tb->jmp_list_next[n];

            qemu_spin_unlock(&dest->jmp_lock);
            return;
        }
        pprev = &tb->jmp_list_next[n];
    }
    g_assert_not_reached();
}




void tb_reset_jump(TranslationBlock *tb, int n)
{
    uintptr_t addr = (uintptr_t)(tb->tc.ptr + tb->jmp_reset_offset[n]);
    tb_set_jmp_target(tb, n, addr);
}


static inline void tb_jmp_unlink(TranslationBlock *dest)
{
    TranslationBlock *tb;
    int n;

    qemu_spin_lock(&dest->jmp_lock);

    TB_FOR_EACH_JMP(dest, tb, n) {
        tb_reset_jump(tb, n);
        qatomic_and(&tb->jmp_dest[n], (uintptr_t)NULL | 1);

    }
    dest->jmp_list_head = (uintptr_t)NULL;

    qemu_spin_unlock(&dest->jmp_lock);
}

static void tb_jmp_cache_inval_tb(TranslationBlock *tb)
{
    CPUState *cpu;

    if (tb_cflags(tb) & CF_PCREL) {

        CPU_FOREACH(cpu) {
            tcg_flush_jmp_cache(cpu);
        }
    } else {
        uint32_t h = tb_jmp_cache_hash_func(tb->pc);

        CPU_FOREACH(cpu) {
            CPUJumpCache *jc = cpu->tb_jmp_cache;

            if (qatomic_read(&jc->array[h].tb) == tb) {
                qatomic_set(&jc->array[h].tb, NULL);
            }
        }
    }
}






static void do_tb_phys_invalidate(TranslationBlock *tb, bool rm_from_page_list)
{
    uint32_t h;
    tb_page_addr_t phys_pc;
    uint32_t orig_cflags = tb_cflags(tb);

    assert_memory_lock();


    qemu_spin_lock(&tb->jmp_lock);
    qatomic_set(&tb->cflags, tb->cflags | CF_INVALID);
    qemu_spin_unlock(&tb->jmp_lock);


    phys_pc = tb_page_addr0(tb);
    h = tb_hash_func(phys_pc, (orig_cflags & CF_PCREL ? 0 : tb->pc),
                     tb->flags, tb->cs_base, orig_cflags);
    if (!qht_remove(&tb_ctx.htable, tb, h)) {
        return;
    }


    if (rm_from_page_list) {
        tb_remove(tb);
    }


    tb_jmp_cache_inval_tb(tb);


    tb_remove_from_jmp_list(tb, 0);
    tb_remove_from_jmp_list(tb, 1);


    tb_jmp_unlink(tb);

    qatomic_set(&tb_ctx.tb_phys_invalidate_count,
                tb_ctx.tb_phys_invalidate_count + 1);
}

static void tb_phys_invalidate__locked(TranslationBlock *tb)
{
    qemu_thread_jit_write();
    do_tb_phys_invalidate(tb, true);
    qemu_thread_jit_execute();
}





void tb_phys_invalidate(TranslationBlock *tb, tb_page_addr_t page_addr)
{
    if (page_addr == -1 && tb_page_addr0(tb) != -1) {
        tb_lock_pages(tb);
        do_tb_phys_invalidate(tb, true);
        tb_unlock_pages(tb);
    } else {
        do_tb_phys_invalidate(tb, false);
    }
}










TranslationBlock *tb_link_page(TranslationBlock *tb)
{
    void *existing_tb = NULL;
    uint32_t h;

    assert_memory_lock();
    tcg_debug_assert(!(tb->cflags & CF_INVALID));

    tb_record(tb);


    h = tb_hash_func(tb_page_addr0(tb), (tb->cflags & CF_PCREL ? 0 : tb->pc),
                     tb->flags, tb->cs_base, tb->cflags);
    qht_insert(&tb_ctx.htable, tb, h, &existing_tb);


    if (unlikely(existing_tb)) {
        tb_remove(tb);
        tb_unlock_pages(tb);
        return existing_tb;
    }

    tb_unlock_pages(tb);
    return tb;
}

#ifdef CONFIG_USER_ONLY





void tb_invalidate_phys_range(tb_page_addr_t start, tb_page_addr_t last)
{
    TranslationBlock *tb;
    PageForEachNext n;

    assert_memory_lock();

    PAGE_FOR_EACH_TB(start, last, unused, tb, n) {
        tb_phys_invalidate__locked(tb);
    }
}






static void tb_invalidate_phys_page(tb_page_addr_t addr)
{
    tb_page_addr_t start, last;

    start = addr & TARGET_PAGE_MASK;
    last = addr | ~TARGET_PAGE_MASK;
    tb_invalidate_phys_range(start, last);
}








bool tb_invalidate_phys_page_unwind(tb_page_addr_t addr, uintptr_t pc)
{
    TranslationBlock *current_tb;
    bool current_tb_modified;
    TranslationBlock *tb;
    PageForEachNext n;
    tb_page_addr_t last;





#ifndef TARGET_HAS_PRECISE_SMC
    pc = 0;
#endif
    if (!pc) {
        tb_invalidate_phys_page(addr);
        return false;
    }

    assert_memory_lock();
    current_tb = tcg_tb_lookup(pc);

    last = addr | ~TARGET_PAGE_MASK;
    addr &= TARGET_PAGE_MASK;
    current_tb_modified = false;

    PAGE_FOR_EACH_TB(addr, last, unused, tb, n) {
        if (current_tb == tb &&
            (tb_cflags(current_tb) & CF_COUNT_MASK) != 1) {







            current_tb_modified = true;
            cpu_restore_state_from_tb(current_cpu, current_tb, pc);
        }
        tb_phys_invalidate__locked(tb);
    }

    if (current_tb_modified) {

        CPUState *cpu = current_cpu;
        cpu->cflags_next_tb = 1 | CF_NOIRQ | curr_cflags(current_cpu);
        return true;
    }
    return false;
}
#else




static void
tb_invalidate_phys_page_range__locked(struct page_collection *pages,
                                      PageDesc *p, tb_page_addr_t start,
                                      tb_page_addr_t last,
                                      uintptr_t retaddr)
{
    TranslationBlock *tb;
    PageForEachNext n;
#ifdef TARGET_HAS_PRECISE_SMC
    bool current_tb_modified = false;
    TranslationBlock *current_tb = retaddr ? tcg_tb_lookup(retaddr) : NULL;
#endif


    tcg_debug_assert(((start ^ last) & TARGET_PAGE_MASK) == 0);





    PAGE_FOR_EACH_TB(start, last, p, tb, n) {
        tb_page_addr_t tb_start, tb_last;


        tb_start = tb_page_addr0(tb);
        tb_last = tb_start + tb->size - 1;
        if (n == 0) {
            tb_last = MIN(tb_last, tb_start | ~TARGET_PAGE_MASK);
        } else {
            tb_start = tb_page_addr1(tb);
            tb_last = tb_start + (tb_last & ~TARGET_PAGE_MASK);
        }
        if (!(tb_last < start || tb_start > last)) {
#ifdef TARGET_HAS_PRECISE_SMC
            if (current_tb == tb &&
                (tb_cflags(current_tb) & CF_COUNT_MASK) != 1) {







                current_tb_modified = true;
                cpu_restore_state_from_tb(current_cpu, current_tb, retaddr);
            }
#endif
            tb_phys_invalidate__locked(tb);
        }
    }


    if (!p->first_tb) {
        tlb_unprotect_code(start);
    }

#ifdef TARGET_HAS_PRECISE_SMC
    if (current_tb_modified) {
        page_collection_unlock(pages);

        current_cpu->cflags_next_tb = 1 | CF_NOIRQ | curr_cflags(current_cpu);
        mmap_unlock();
        cpu_loop_exit_noexc(current_cpu);
    }
#endif
}








void tb_invalidate_phys_range(tb_page_addr_t start, tb_page_addr_t last)
{
    struct page_collection *pages;
    tb_page_addr_t index, index_last;

    pages = page_collection_lock(start, last);

    index_last = last >> TARGET_PAGE_BITS;
    for (index = start >> TARGET_PAGE_BITS; index <= index_last; index++) {
        PageDesc *pd = page_find(index);
        tb_page_addr_t page_start, page_last;

        if (pd == NULL) {
            continue;
        }
        assert_page_locked(pd);
        page_start = index << TARGET_PAGE_BITS;
        page_last = page_start | ~TARGET_PAGE_MASK;
        page_last = MIN(page_last, last);
        tb_invalidate_phys_page_range__locked(pages, pd,
                                              page_start, page_last, 0);
    }
    page_collection_unlock(pages);
}




static void tb_invalidate_phys_page_fast__locked(struct page_collection *pages,
                                                 tb_page_addr_t start,
                                                 unsigned len, uintptr_t ra)
{
    PageDesc *p;

    p = page_find(start >> TARGET_PAGE_BITS);
    if (!p) {
        return;
    }

    assert_page_locked(p);
    tb_invalidate_phys_page_range__locked(pages, p, start, start + len - 1, ra);
}






void tb_invalidate_phys_range_fast(ram_addr_t ram_addr,
                                   unsigned size,
                                   uintptr_t retaddr)
{
    struct page_collection *pages;

    pages = page_collection_lock(ram_addr, ram_addr + size - 1);
    tb_invalidate_phys_page_fast__locked(pages, ram_addr, size, retaddr);
    page_collection_unlock(pages);
}

#endif
