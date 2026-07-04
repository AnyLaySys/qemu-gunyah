
#ifndef QEMU_CACHEFLUSH_H
#define QEMU_CACHEFLUSH_H


#if defined(__i386__) || defined(__x86_64__) || defined(__s390__)

static inline void flush_idcache_range(uintptr_t rx, uintptr_t rw, size_t len)
{
}

#else

void flush_idcache_range(uintptr_t rx, uintptr_t rw, size_t len);

#endif

#endif /* QEMU_CACHEFLUSH_H */
