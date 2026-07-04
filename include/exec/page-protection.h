#ifndef EXEC_PAGE_PROT_COMMON_H
#define EXEC_PAGE_PROT_COMMON_H

#define PAGE_READ      0x0001
#define PAGE_WRITE     0x0002
#define PAGE_EXEC      0x0004
#define PAGE_RWX       (PAGE_READ | PAGE_WRITE | PAGE_EXEC)
#define PAGE_VALID     0x0008
#define PAGE_WRITE_ORG 0x0010
#define PAGE_WRITE_INV 0x0020
#define PAGE_RESET     0x0040
#define PAGE_ANON      0x0080

#define PAGE_TARGET_1  0x0200
#define PAGE_TARGET_2  0x0400

#define PAGE_PASSTHROUGH 0x0800

#ifdef CONFIG_USER_ONLY

void TSA_NO_TSA mmap_lock(void);
void TSA_NO_TSA mmap_unlock(void);
bool have_mmap_lock(void);

static inline void mmap_unlock_guard(void *unused)
{
    mmap_unlock();
}

#define WITH_MMAP_LOCK_GUARD() \
    for (int _mmap_lock_iter __attribute__((cleanup(mmap_unlock_guard))) \
         = (mmap_lock(), 0); _mmap_lock_iter == 0; _mmap_lock_iter = 1)
#else

static inline void mmap_lock(void) {}
static inline void mmap_unlock(void) {}
#define WITH_MMAP_LOCK_GUARD()

#endif /* !CONFIG_USER_ONLY */

#endif
