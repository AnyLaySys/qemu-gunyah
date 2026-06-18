

















#include "qemu/osdep.h"
#include "accel/tcg/cpu-ops.h"
#include "disas/disas.h"
#include "exec/vaddr.h"
#include "exec/exec-all.h"
#include "tcg/tcg.h"
#include "qemu/bitops.h"
#include "qemu/rcu.h"
#include "exec/cpu_ldst.h"
#include "user/cpu_loop.h"
#include "qemu/main-loop.h"
#include "user/page-protection.h"
#include "exec/page-protection.h"
#include "exec/helper-proto.h"
#include "qemu/atomic128.h"
#include "qemu/bswap.h"
#include "qemu/int128.h"
#include "trace.h"
#include "tcg/tcg-ldst.h"
#include "internal-common.h"
#include "internal-target.h"
#include "tb-internal.h"

__thread uintptr_t helper_retaddr;



void cpu_interrupt(CPUState *cpu, int mask)
{
    g_assert(bql_locked());
    cpu->interrupt_request |= mask;
    qatomic_set(&cpu->neg.icount_decr.u16.high, -1);
}




MMUAccessType adjust_signal_pc(uintptr_t *pc, bool is_write)
{
    switch (helper_retaddr) {
    default:






        *pc = helper_retaddr;
        break;

    case 0:
















        *pc += GETPC_ADJ;
        break;

    case 1:










        *pc = 0;
        return MMU_INST_FETCH;
    }

    return is_write ? MMU_DATA_STORE : MMU_DATA_LOAD;
}



















bool handle_sigsegv_accerr_write(CPUState *cpu, sigset_t *old_set,
                                 uintptr_t host_pc, abi_ptr guest_addr)
{
    switch (page_unprotect(guest_addr, host_pc)) {
    case 0:




        return false;
    case 1:




        return true;
    case 2:




        sigprocmask(SIG_SETMASK, old_set, NULL);
        cpu_loop_exit_noexc(cpu);

    default:
        g_assert_not_reached();
    }
}

typedef struct PageFlagsNode {
    struct rcu_head rcu;
    IntervalTreeNode itree;
    int flags;
} PageFlagsNode;

static IntervalTreeRoot pageflags_root;

static PageFlagsNode *pageflags_find(target_ulong start, target_ulong last)
{
    IntervalTreeNode *n;

    n = interval_tree_iter_first(&pageflags_root, start, last);
    return n ? container_of(n, PageFlagsNode, itree) : NULL;
}

static PageFlagsNode *pageflags_next(PageFlagsNode *p, target_ulong start,
                                     target_ulong last)
{
    IntervalTreeNode *n;

    n = interval_tree_iter_next(&p->itree, start, last);
    return n ? container_of(n, PageFlagsNode, itree) : NULL;
}

int walk_memory_regions(void *priv, walk_memory_regions_fn fn)
{
    IntervalTreeNode *n;
    int rc = 0;

    mmap_lock();
    for (n = interval_tree_iter_first(&pageflags_root, 0, -1);
         n != NULL;
         n = interval_tree_iter_next(n, 0, -1)) {
        PageFlagsNode *p = container_of(n, PageFlagsNode, itree);

        rc = fn(priv, n->start, n->last + 1, p->flags);
        if (rc != 0) {
            break;
        }
    }
    mmap_unlock();

    return rc;
}

static int dump_region(void *priv, target_ulong start,
                       target_ulong end, unsigned long prot)
{
    FILE *f = (FILE *)priv;

    fprintf(f, TARGET_FMT_lx"-"TARGET_FMT_lx" "TARGET_FMT_lx" %c%c%c\n",
            start, end, end - start,
            ((prot & PAGE_READ) ? 'r' : '-'),
            ((prot & PAGE_WRITE) ? 'w' : '-'),
            ((prot & PAGE_EXEC) ? 'x' : '-'));
    return 0;
}


void page_dump(FILE *f)
{
    const int length = sizeof(target_ulong) * 2;

    fprintf(f, "%-*s %-*s %-*s %s\n",
            length, "start", length, "end", length, "size", "prot");
    walk_memory_regions(f, dump_region);
}

int page_get_flags(target_ulong address)
{
    PageFlagsNode *p = pageflags_find(address, address);






    if (p) {
        return p->flags;
    }
    if (have_mmap_lock()) {
        return 0;
    }

    mmap_lock();
    p = pageflags_find(address, address);
    mmap_unlock();
    return p ? p->flags : 0;
}


static void pageflags_create(target_ulong start, target_ulong last, int flags)
{
    PageFlagsNode *p = g_new(PageFlagsNode, 1);

    p->itree.start = start;
    p->itree.last = last;
    p->flags = flags;
    interval_tree_insert(&p->itree, &pageflags_root);
}


static bool pageflags_unset(target_ulong start, target_ulong last)
{
    bool inval_tb = false;

    while (true) {
        PageFlagsNode *p = pageflags_find(start, last);
        target_ulong p_last;

        if (!p) {
            break;
        }

        if (p->flags & PAGE_EXEC) {
            inval_tb = true;
        }

        interval_tree_remove(&p->itree, &pageflags_root);
        p_last = p->itree.last;

        if (p->itree.start < start) {

            p->itree.last = start - 1;
            interval_tree_insert(&p->itree, &pageflags_root);
            if (last < p_last) {
                pageflags_create(last + 1, p_last, p->flags);
                break;
            }
        } else if (p_last <= last) {

            g_free_rcu(p, rcu);
        } else {

            p->itree.start = last + 1;
            interval_tree_insert(&p->itree, &pageflags_root);
            break;
        }
    }

    return inval_tb;
}





static void pageflags_create_merge(target_ulong start, target_ulong last,
                                   int flags)
{
    PageFlagsNode *next = NULL, *prev = NULL;

    if (start > 0) {
        prev = pageflags_find(start - 1, start - 1);
        if (prev) {
            if (prev->flags == flags) {
                interval_tree_remove(&prev->itree, &pageflags_root);
            } else {
                prev = NULL;
            }
        }
    }
    if (last + 1 != 0) {
        next = pageflags_find(last + 1, last + 1);
        if (next) {
            if (next->flags == flags) {
                interval_tree_remove(&next->itree, &pageflags_root);
            } else {
                next = NULL;
            }
        }
    }

    if (prev) {
        if (next) {
            prev->itree.last = next->itree.last;
            g_free_rcu(next, rcu);
        } else {
            prev->itree.last = last;
        }
        interval_tree_insert(&prev->itree, &pageflags_root);
    } else if (next) {
        next->itree.start = start;
        interval_tree_insert(&next->itree, &pageflags_root);
    } else {
        pageflags_create(start, last, flags);
    }
}





#ifndef PAGE_TARGET_STICKY
#define PAGE_TARGET_STICKY  0
#endif
#define PAGE_STICKY  (PAGE_ANON | PAGE_PASSTHROUGH | PAGE_TARGET_STICKY)


static bool pageflags_set_clear(target_ulong start, target_ulong last,
                                int set_flags, int clear_flags)
{
    PageFlagsNode *p;
    target_ulong p_start, p_last;
    int p_flags, merge_flags;
    bool inval_tb = false;

 restart:
    p = pageflags_find(start, last);
    if (!p) {
        if (set_flags) {
            pageflags_create_merge(start, last, set_flags);
        }
        goto done;
    }

    p_start = p->itree.start;
    p_last = p->itree.last;
    p_flags = p->flags;

    merge_flags = (p_flags & ~clear_flags) | set_flags;





    if ((p_flags & PAGE_EXEC)
        && (!(merge_flags & PAGE_EXEC)
            || (merge_flags & ~p_flags & PAGE_WRITE))) {
        inval_tb = true;
    }





    if (start == p_start && last == p_last) {
        if (merge_flags) {
            p->flags = merge_flags;
        } else {
            interval_tree_remove(&p->itree, &pageflags_root);
            g_free_rcu(p, rcu);
        }
        goto done;
    }





    if (set_flags != merge_flags) {
        if (p_start < start) {
            interval_tree_remove(&p->itree, &pageflags_root);
            p->itree.last = start - 1;
            interval_tree_insert(&p->itree, &pageflags_root);

            if (last < p_last) {
                if (merge_flags) {
                    pageflags_create(start, last, merge_flags);
                }
                pageflags_create(last + 1, p_last, p_flags);
            } else {
                if (merge_flags) {
                    pageflags_create(start, p_last, merge_flags);
                }
                if (p_last < last) {
                    start = p_last + 1;
                    goto restart;
                }
            }
        } else {
            if (start < p_start && set_flags) {
                pageflags_create(start, p_start - 1, set_flags);
            }
            if (last < p_last) {
                interval_tree_remove(&p->itree, &pageflags_root);
                p->itree.start = last + 1;
                interval_tree_insert(&p->itree, &pageflags_root);
                if (merge_flags) {
                    pageflags_create(start, last, merge_flags);
                }
            } else {
                if (merge_flags) {
                    p->flags = merge_flags;
                } else {
                    interval_tree_remove(&p->itree, &pageflags_root);
                    g_free_rcu(p, rcu);
                }
                if (p_last < last) {
                    start = p_last + 1;
                    goto restart;
                }
            }
        }
        goto done;
    }


    if (set_flags == p_flags) {
        if (start < p_start) {
            interval_tree_remove(&p->itree, &pageflags_root);
            p->itree.start = start;
            interval_tree_insert(&p->itree, &pageflags_root);
        }
        if (p_last < last) {
            start = p_last + 1;
            goto restart;
        }
        goto done;
    }


    interval_tree_remove(&p->itree, &pageflags_root);
    if (p_start < start) {
        p->itree.last = start - 1;
        interval_tree_insert(&p->itree, &pageflags_root);

        if (p_last < last) {
            goto restart;
        }
        if (last < p_last) {
            pageflags_create(last + 1, p_last, p_flags);
        }
    } else if (last < p_last) {
        p->itree.start = last + 1;
        interval_tree_insert(&p->itree, &pageflags_root);
    } else {
        g_free_rcu(p, rcu);
        goto restart;
    }
    if (set_flags) {
        pageflags_create(start, last, set_flags);
    }

 done:
    return inval_tb;
}

void page_set_flags(target_ulong start, target_ulong last, int flags)
{
    bool reset = false;
    bool inval_tb = false;




    assert(start <= last);
    assert(last <= GUEST_ADDR_MAX);

    assert(!(flags & PAGE_ANON) || (flags & PAGE_RESET));
    assert_memory_lock();

    start &= TARGET_PAGE_MASK;
    last |= ~TARGET_PAGE_MASK;

    if (!(flags & PAGE_VALID)) {
        flags = 0;
    } else {
        reset = flags & PAGE_RESET;
        flags &= ~PAGE_RESET;
        if (flags & PAGE_WRITE) {
            flags |= PAGE_WRITE_ORG;
        }
    }

    if (!flags || reset) {
        page_reset_target_data(start, last);
        inval_tb |= pageflags_unset(start, last);
    }
    if (flags) {
        inval_tb |= pageflags_set_clear(start, last, flags,
                                        ~(reset ? 0 : PAGE_STICKY));
    }
    if (inval_tb) {
        tb_invalidate_phys_range(start, last);
    }
}

bool page_check_range(target_ulong start, target_ulong len, int flags)
{
    target_ulong last;
    int locked;
    bool ret;

    if (len == 0) {
        return true;
    }

    last = start + len - 1;
    if (last < start) {
        return false;
    }

    locked = have_mmap_lock();
    while (true) {
        PageFlagsNode *p = pageflags_find(start, last);
        int missing;

        if (!p) {
            if (!locked) {




                mmap_lock();
                locked = -1;
                p = pageflags_find(start, last);
            }
            if (!p) {
                ret = false;
                break;
            }
        }
        if (start < p->itree.start) {
            ret = false;
            break;
        }

        missing = flags & ~p->flags;
        if (missing & ~PAGE_WRITE) {
            ret = false;
            break;
        }
        if (missing & PAGE_WRITE) {
            if (!(p->flags & PAGE_WRITE_ORG)) {
                ret = false;
                break;
            }

            if (!page_unprotect(start, 0)) {
                ret = false;
                break;
            }

            if (last - start < TARGET_PAGE_SIZE) {
                ret = true;
                break;
            }
            start += TARGET_PAGE_SIZE;
            continue;
        }

        if (last <= p->itree.last) {
            ret = true;
            break;
        }
        start = p->itree.last + 1;
    }


    if (locked < 0) {
        mmap_unlock();
    }
    return ret;
}

bool page_check_range_empty(target_ulong start, target_ulong last)
{
    assert(last >= start);
    assert_memory_lock();
    return pageflags_find(start, last) == NULL;
}

target_ulong page_find_range_empty(target_ulong min, target_ulong max,
                                   target_ulong len, target_ulong align)
{
    target_ulong len_m1, align_m1;

    assert(min <= max);
    assert(max <= GUEST_ADDR_MAX);
    assert(len != 0);
    assert(is_power_of_2(align));
    assert_memory_lock();

    len_m1 = len - 1;
    align_m1 = align - 1;


    while (1) {
        PageFlagsNode *p;


        min = (min + align_m1) & ~align_m1;
        if (min > max) {
            return -1;
        }
        if (len_m1 > max - min) {
            return -1;
        }

        p = pageflags_find(min, min + len_m1);
        if (p == NULL) {

            return min;
        }
        if (max <= p->itree.last) {

            return -1;
        }

        min = p->itree.last + 1;
    }
}

void page_protect(tb_page_addr_t address)
{
    PageFlagsNode *p;
    target_ulong start, last;
    int host_page_size = qemu_real_host_page_size();
    int prot;

    assert_memory_lock();

    if (host_page_size <= TARGET_PAGE_SIZE) {
        start = address & TARGET_PAGE_MASK;
        last = start + TARGET_PAGE_SIZE - 1;
    } else {
        start = address & -host_page_size;
        last = start + host_page_size - 1;
    }

    p = pageflags_find(start, last);
    if (!p) {
        return;
    }
    prot = p->flags;

    if (unlikely(p->itree.last < last)) {

        assert(TARGET_PAGE_SIZE < host_page_size);
        while ((p = pageflags_next(p, start, last)) != NULL) {
            prot |= p->flags;
        }
    }

    if (prot & PAGE_WRITE) {
        pageflags_set_clear(start, last, 0, PAGE_WRITE);
        mprotect(g2h_untagged(start), last - start + 1,
                 prot & (PAGE_READ | PAGE_EXEC) ? PROT_READ : PROT_NONE);
    }
}








int page_unprotect(tb_page_addr_t address, uintptr_t pc)
{
    PageFlagsNode *p;
    bool current_tb_invalidated;






    mmap_lock();

    p = pageflags_find(address, address);


    if (!p || !(p->flags & PAGE_WRITE_ORG)) {
        mmap_unlock();
        return 0;
    }

    current_tb_invalidated = false;
    if (p->flags & PAGE_WRITE) {





#ifdef TARGET_HAS_PRECISE_SMC
        TranslationBlock *current_tb = tcg_tb_lookup(pc);
        if (current_tb) {
            current_tb_invalidated = tb_cflags(current_tb) & CF_INVALID;
        }
#endif
    } else {
        int host_page_size = qemu_real_host_page_size();
        target_ulong start, len, i;
        int prot;

        if (host_page_size <= TARGET_PAGE_SIZE) {
            start = address & TARGET_PAGE_MASK;
            len = TARGET_PAGE_SIZE;
            prot = p->flags | PAGE_WRITE;
            pageflags_set_clear(start, start + len - 1, PAGE_WRITE, 0);
            current_tb_invalidated = tb_invalidate_phys_page_unwind(start, pc);
        } else {
            start = address & -host_page_size;
            len = host_page_size;
            prot = 0;

            for (i = 0; i < len; i += TARGET_PAGE_SIZE) {
                target_ulong addr = start + i;

                p = pageflags_find(addr, addr);
                if (p) {
                    prot |= p->flags;
                    if (p->flags & PAGE_WRITE_ORG) {
                        prot |= PAGE_WRITE;
                        pageflags_set_clear(addr, addr + TARGET_PAGE_SIZE - 1,
                                            PAGE_WRITE, 0);
                    }
                }




                current_tb_invalidated |=
                    tb_invalidate_phys_page_unwind(addr, pc);
            }
        }
        if (prot & PAGE_EXEC) {
            prot = (prot & ~PAGE_EXEC) | PAGE_READ;
        }
        mprotect((void *)g2h_untagged(start), len, prot & PAGE_RWX);
    }
    mmap_unlock();


    return current_tb_invalidated ? 2 : 1;
}

static int probe_access_internal(CPUArchState *env, vaddr addr,
                                 int fault_size, MMUAccessType access_type,
                                 bool nonfault, uintptr_t ra)
{
    int acc_flag;
    bool maperr;

    switch (access_type) {
    case MMU_DATA_STORE:
        acc_flag = PAGE_WRITE_ORG;
        break;
    case MMU_DATA_LOAD:
        acc_flag = PAGE_READ;
        break;
    case MMU_INST_FETCH:
        acc_flag = PAGE_EXEC;
        break;
    default:
        g_assert_not_reached();
    }

    if (guest_addr_valid_untagged(addr)) {
        int page_flags = page_get_flags(addr);
        if (page_flags & acc_flag) {
            if (access_type != MMU_INST_FETCH
                && cpu_plugin_mem_cbs_enabled(env_cpu(env))) {
                return TLB_MMIO;
            }
            return 0;
        }
        maperr = !(page_flags & PAGE_VALID);
    } else {
        maperr = true;
    }

    if (nonfault) {
        return TLB_INVALID_MASK;
    }

    cpu_loop_exit_sigsegv(env_cpu(env), addr, access_type, maperr, ra);
}

int probe_access_flags(CPUArchState *env, vaddr addr, int size,
                       MMUAccessType access_type, int mmu_idx,
                       bool nonfault, void **phost, uintptr_t ra)
{
    int flags;

    g_assert(-(addr | TARGET_PAGE_MASK) >= size);
    flags = probe_access_internal(env, addr, size, access_type, nonfault, ra);
    *phost = (flags & TLB_INVALID_MASK) ? NULL : g2h(env_cpu(env), addr);
    return flags;
}

void *probe_access(CPUArchState *env, vaddr addr, int size,
                   MMUAccessType access_type, int mmu_idx, uintptr_t ra)
{
    int flags;

    g_assert(-(addr | TARGET_PAGE_MASK) >= size);
    flags = probe_access_internal(env, addr, size, access_type, false, ra);
    g_assert((flags & ~TLB_MMIO) == 0);

    return size ? g2h(env_cpu(env), addr) : NULL;
}

tb_page_addr_t get_page_addr_code_hostp(CPUArchState *env, vaddr addr,
                                        void **hostp)
{
    int flags;

    flags = probe_access_internal(env, addr, 1, MMU_INST_FETCH, false, 0);
    g_assert(flags == 0);

    if (hostp) {
        *hostp = g2h_untagged(addr);
    }
    return addr;
}

#ifdef TARGET_PAGE_DATA_SIZE





#define TPD_PAGES  64
#define TBD_MASK   (TARGET_PAGE_MASK * TPD_PAGES)

typedef struct TargetPageDataNode {
    struct rcu_head rcu;
    IntervalTreeNode itree;
    char data[] __attribute__((aligned));
} TargetPageDataNode;

static IntervalTreeRoot targetdata_root;

void page_reset_target_data(target_ulong start, target_ulong last)
{
    IntervalTreeNode *n, *next;

    assert_memory_lock();

    start &= TARGET_PAGE_MASK;
    last |= ~TARGET_PAGE_MASK;

    for (n = interval_tree_iter_first(&targetdata_root, start, last),
         next = n ? interval_tree_iter_next(n, start, last) : NULL;
         n != NULL;
         n = next,
         next = next ? interval_tree_iter_next(n, start, last) : NULL) {
        target_ulong n_start, n_last, p_ofs, p_len;
        TargetPageDataNode *t = container_of(n, TargetPageDataNode, itree);

        if (n->start >= start && n->last <= last) {
            interval_tree_remove(n, &targetdata_root);
            g_free_rcu(t, rcu);
            continue;
        }

        if (n->start < start) {
            n_start = start;
            p_ofs = (start - n->start) >> TARGET_PAGE_BITS;
        } else {
            n_start = n->start;
            p_ofs = 0;
        }
        n_last = MIN(last, n->last);
        p_len = (n_last + 1 - n_start) >> TARGET_PAGE_BITS;

        memset(t->data + p_ofs * TARGET_PAGE_DATA_SIZE, 0,
               p_len * TARGET_PAGE_DATA_SIZE);
    }
}

void *page_get_target_data(target_ulong address)
{
    IntervalTreeNode *n;
    TargetPageDataNode *t;
    target_ulong page, region, p_ofs;

    page = address & TARGET_PAGE_MASK;
    region = address & TBD_MASK;

    n = interval_tree_iter_first(&targetdata_root, page, page);
    if (!n) {






        mmap_lock();
        n = interval_tree_iter_first(&targetdata_root, page, page);
        if (!n) {
            t = g_malloc0(sizeof(TargetPageDataNode)
                          + TPD_PAGES * TARGET_PAGE_DATA_SIZE);
            n = &t->itree;
            n->start = region;
            n->last = region | ~TBD_MASK;
            interval_tree_insert(n, &targetdata_root);
        }
        mmap_unlock();
    }

    t = container_of(n, TargetPageDataNode, itree);
    p_ofs = (page - region) >> TARGET_PAGE_BITS;
    return t->data + p_ofs * TARGET_PAGE_DATA_SIZE;
}
#else
void page_reset_target_data(target_ulong start, target_ulong last) { }
#endif



static void *cpu_mmu_lookup(CPUState *cpu, vaddr addr,
                            MemOp mop, uintptr_t ra, MMUAccessType type)
{
    int a_bits = memop_alignment_bits(mop);
    void *ret;


    if (unlikely(addr & ((1 << a_bits) - 1))) {
        cpu_loop_exit_sigbus(cpu, addr, type, ra);
    }

    ret = g2h(cpu, addr);
    set_helper_retaddr(ra);
    return ret;
}


int cpu_memory_rw_debug(CPUState *cpu, vaddr addr,
                        void *ptr, size_t len, bool is_write)
{
    int flags;
    vaddr l, page;
    uint8_t *buf = ptr;
    ssize_t written;
    int ret = -1;
    int fd = -1;

    mmap_lock();

    while (len > 0) {
        page = addr & TARGET_PAGE_MASK;
        l = (page + TARGET_PAGE_SIZE) - addr;
        if (l > len) {
            l = len;
        }
        flags = page_get_flags(page);
        if (!(flags & PAGE_VALID)) {
            goto out_close;
        }
        if (is_write) {
            if (flags & PAGE_WRITE) {
                memcpy(g2h(cpu, addr), buf, l);
            } else {

                if (fd == -1) {
                    fd = open("/proc/self/mem", O_WRONLY);
                    if (fd == -1) {
                        goto out;
                    }
                }








                tb_invalidate_phys_range(addr, addr + l - 1);
                written = pwrite(fd, buf, l,
                                 (off_t)(uintptr_t)g2h_untagged(addr));
                if (written != l) {
                    goto out_close;
                }
            }
        } else if (flags & PAGE_READ) {
            memcpy(buf, g2h(cpu, addr), l);
        } else {

            if (fd == -1) {
                fd = open("/proc/self/mem", O_RDONLY);
                if (fd == -1) {
                    goto out;
                }
            }
            if (pread(fd, buf, l,
                      (off_t)(uintptr_t)g2h_untagged(addr)) != l) {
                goto out_close;
            }
        }
        len -= l;
        buf += l;
        addr += l;
    }
    ret = 0;
out_close:
    if (fd != -1) {
        close(fd);
    }
out:
    mmap_unlock();

    return ret;
}

#include "ldst_atomicity.c.inc"

static uint8_t do_ld1_mmu(CPUState *cpu, vaddr addr, MemOpIdx oi,
                          uintptr_t ra, MMUAccessType access_type)
{
    void *haddr;
    uint8_t ret;

    cpu_req_mo(TCG_MO_LD_LD | TCG_MO_ST_LD);
    haddr = cpu_mmu_lookup(cpu, addr, get_memop(oi), ra, access_type);
    ret = ldub_p(haddr);
    clear_helper_retaddr();
    return ret;
}

static uint16_t do_ld2_mmu(CPUState *cpu, vaddr addr, MemOpIdx oi,
                           uintptr_t ra, MMUAccessType access_type)
{
    void *haddr;
    uint16_t ret;
    MemOp mop = get_memop(oi);

    cpu_req_mo(TCG_MO_LD_LD | TCG_MO_ST_LD);
    haddr = cpu_mmu_lookup(cpu, addr, mop, ra, access_type);
    ret = load_atom_2(cpu, ra, haddr, mop);
    clear_helper_retaddr();

    if (mop & MO_BSWAP) {
        ret = bswap16(ret);
    }
    return ret;
}

static uint32_t do_ld4_mmu(CPUState *cpu, vaddr addr, MemOpIdx oi,
                           uintptr_t ra, MMUAccessType access_type)
{
    void *haddr;
    uint32_t ret;
    MemOp mop = get_memop(oi);

    cpu_req_mo(TCG_MO_LD_LD | TCG_MO_ST_LD);
    haddr = cpu_mmu_lookup(cpu, addr, mop, ra, access_type);
    ret = load_atom_4(cpu, ra, haddr, mop);
    clear_helper_retaddr();

    if (mop & MO_BSWAP) {
        ret = bswap32(ret);
    }
    return ret;
}

static uint64_t do_ld8_mmu(CPUState *cpu, vaddr addr, MemOpIdx oi,
                           uintptr_t ra, MMUAccessType access_type)
{
    void *haddr;
    uint64_t ret;
    MemOp mop = get_memop(oi);

    cpu_req_mo(TCG_MO_LD_LD | TCG_MO_ST_LD);
    haddr = cpu_mmu_lookup(cpu, addr, mop, ra, access_type);
    ret = load_atom_8(cpu, ra, haddr, mop);
    clear_helper_retaddr();

    if (mop & MO_BSWAP) {
        ret = bswap64(ret);
    }
    return ret;
}

static Int128 do_ld16_mmu(CPUState *cpu, abi_ptr addr,
                          MemOpIdx oi, uintptr_t ra)
{
    void *haddr;
    Int128 ret;
    MemOp mop = get_memop(oi);

    tcg_debug_assert((mop & MO_SIZE) == MO_128);
    cpu_req_mo(TCG_MO_LD_LD | TCG_MO_ST_LD);
    haddr = cpu_mmu_lookup(cpu, addr, mop, ra, MMU_DATA_LOAD);
    ret = load_atom_16(cpu, ra, haddr, mop);
    clear_helper_retaddr();

    if (mop & MO_BSWAP) {
        ret = bswap128(ret);
    }
    return ret;
}

static void do_st1_mmu(CPUState *cpu, vaddr addr, uint8_t val,
                       MemOpIdx oi, uintptr_t ra)
{
    void *haddr;

    cpu_req_mo(TCG_MO_LD_ST | TCG_MO_ST_ST);
    haddr = cpu_mmu_lookup(cpu, addr, get_memop(oi), ra, MMU_DATA_STORE);
    stb_p(haddr, val);
    clear_helper_retaddr();
}

static void do_st2_mmu(CPUState *cpu, vaddr addr, uint16_t val,
                       MemOpIdx oi, uintptr_t ra)
{
    void *haddr;
    MemOp mop = get_memop(oi);

    cpu_req_mo(TCG_MO_LD_ST | TCG_MO_ST_ST);
    haddr = cpu_mmu_lookup(cpu, addr, mop, ra, MMU_DATA_STORE);

    if (mop & MO_BSWAP) {
        val = bswap16(val);
    }
    store_atom_2(cpu, ra, haddr, mop, val);
    clear_helper_retaddr();
}

static void do_st4_mmu(CPUState *cpu, vaddr addr, uint32_t val,
                       MemOpIdx oi, uintptr_t ra)
{
    void *haddr;
    MemOp mop = get_memop(oi);

    cpu_req_mo(TCG_MO_LD_ST | TCG_MO_ST_ST);
    haddr = cpu_mmu_lookup(cpu, addr, mop, ra, MMU_DATA_STORE);

    if (mop & MO_BSWAP) {
        val = bswap32(val);
    }
    store_atom_4(cpu, ra, haddr, mop, val);
    clear_helper_retaddr();
}

static void do_st8_mmu(CPUState *cpu, vaddr addr, uint64_t val,
                       MemOpIdx oi, uintptr_t ra)
{
    void *haddr;
    MemOp mop = get_memop(oi);

    cpu_req_mo(TCG_MO_LD_ST | TCG_MO_ST_ST);
    haddr = cpu_mmu_lookup(cpu, addr, mop, ra, MMU_DATA_STORE);

    if (mop & MO_BSWAP) {
        val = bswap64(val);
    }
    store_atom_8(cpu, ra, haddr, mop, val);
    clear_helper_retaddr();
}

static void do_st16_mmu(CPUState *cpu, vaddr addr, Int128 val,
                        MemOpIdx oi, uintptr_t ra)
{
    void *haddr;
    MemOpIdx mop = get_memop(oi);

    cpu_req_mo(TCG_MO_LD_ST | TCG_MO_ST_ST);
    haddr = cpu_mmu_lookup(cpu, addr, mop, ra, MMU_DATA_STORE);

    if (mop & MO_BSWAP) {
        val = bswap128(val);
    }
    store_atom_16(cpu, ra, haddr, mop, val);
    clear_helper_retaddr();
}

uint32_t cpu_ldub_code(CPUArchState *env, abi_ptr ptr)
{
    uint32_t ret;

    set_helper_retaddr(1);
    ret = ldub_p(g2h_untagged(ptr));
    clear_helper_retaddr();
    return ret;
}

uint32_t cpu_lduw_code(CPUArchState *env, abi_ptr ptr)
{
    uint32_t ret;

    set_helper_retaddr(1);
    ret = lduw_p(g2h_untagged(ptr));
    clear_helper_retaddr();
    return ret;
}

uint32_t cpu_ldl_code(CPUArchState *env, abi_ptr ptr)
{
    uint32_t ret;

    set_helper_retaddr(1);
    ret = ldl_p(g2h_untagged(ptr));
    clear_helper_retaddr();
    return ret;
}

uint64_t cpu_ldq_code(CPUArchState *env, abi_ptr ptr)
{
    uint64_t ret;

    set_helper_retaddr(1);
    ret = ldq_p(g2h_untagged(ptr));
    clear_helper_retaddr();
    return ret;
}

uint8_t cpu_ldb_code_mmu(CPUArchState *env, abi_ptr addr,
                         MemOpIdx oi, uintptr_t ra)
{
    void *haddr;
    uint8_t ret;

    haddr = cpu_mmu_lookup(env_cpu(env), addr, oi, ra, MMU_INST_FETCH);
    ret = ldub_p(haddr);
    clear_helper_retaddr();
    return ret;
}

uint16_t cpu_ldw_code_mmu(CPUArchState *env, abi_ptr addr,
                          MemOpIdx oi, uintptr_t ra)
{
    void *haddr;
    uint16_t ret;

    haddr = cpu_mmu_lookup(env_cpu(env), addr, oi, ra, MMU_INST_FETCH);
    ret = lduw_p(haddr);
    clear_helper_retaddr();
    if (get_memop(oi) & MO_BSWAP) {
        ret = bswap16(ret);
    }
    return ret;
}

uint32_t cpu_ldl_code_mmu(CPUArchState *env, abi_ptr addr,
                          MemOpIdx oi, uintptr_t ra)
{
    void *haddr;
    uint32_t ret;

    haddr = cpu_mmu_lookup(env_cpu(env), addr, oi, ra, MMU_INST_FETCH);
    ret = ldl_p(haddr);
    clear_helper_retaddr();
    if (get_memop(oi) & MO_BSWAP) {
        ret = bswap32(ret);
    }
    return ret;
}

uint64_t cpu_ldq_code_mmu(CPUArchState *env, abi_ptr addr,
                          MemOpIdx oi, uintptr_t ra)
{
    void *haddr;
    uint64_t ret;

    haddr = cpu_mmu_lookup(env_cpu(env), addr, oi, ra, MMU_DATA_LOAD);
    ret = ldq_p(haddr);
    clear_helper_retaddr();
    if (get_memop(oi) & MO_BSWAP) {
        ret = bswap64(ret);
    }
    return ret;
}

#include "ldst_common.c.inc"




static void *atomic_mmu_lookup(CPUState *cpu, vaddr addr, MemOpIdx oi,
                               int size, uintptr_t retaddr)
{
    MemOp mop = get_memop(oi);
    int a_bits = memop_alignment_bits(mop);
    void *ret;


    if (unlikely(addr & ((1 << a_bits) - 1))) {
        cpu_loop_exit_sigbus(cpu, addr, MMU_DATA_STORE, retaddr);
    }


    if (unlikely(addr & (size - 1))) {
        cpu_loop_exit_atomic(cpu, retaddr);
    }

    ret = g2h(cpu, addr);
    set_helper_retaddr(retaddr);
    return ret;
}

#include "atomic_common.c.inc"






#define ATOMIC_NAME(X) \
    glue(glue(glue(cpu_atomic_ ## X, SUFFIX), END), _mmu)
#define ATOMIC_MMU_CLEANUP do { clear_helper_retaddr(); } while (0)

#define DATA_SIZE 1
#include "atomic_template.h"

#define DATA_SIZE 2
#include "atomic_template.h"

#define DATA_SIZE 4
#include "atomic_template.h"

#ifdef CONFIG_ATOMIC64
#define DATA_SIZE 8
#include "atomic_template.h"
#endif

#if defined(CONFIG_ATOMIC128) || HAVE_CMPXCHG128
#define DATA_SIZE 16
#include "atomic_template.h"
#endif
