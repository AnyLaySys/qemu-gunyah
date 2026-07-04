
#include "qemu/osdep.h"
#include "qemu/host-utils.h"
#include "qemu/memalign.h"
#include "trace.h"

void *qemu_try_memalign(size_t alignment, size_t size)
{
    void *ptr;

    if (alignment < sizeof(void*)) {
        alignment = sizeof(void*);
    } else {
        g_assert(is_power_of_2(alignment));
    }

    if (size == 0) {
        size++;
    }
#if defined(CONFIG_POSIX_MEMALIGN)
    int ret;
    ret = posix_memalign(&ptr, alignment, size);
    if (ret != 0) {
        errno = ret;
        ptr = NULL;
    }
#elif defined(CONFIG_ALIGNED_MALLOC)
    ptr = _aligned_malloc(size, alignment);
#elif defined(CONFIG_VALLOC)
    ptr = valloc(size);
#elif defined(CONFIG_MEMALIGN)
    ptr = memalign(alignment, size);
#else
    #error No function to allocate aligned memory available
#endif
    trace_qemu_memalign(alignment, size, ptr);
    return ptr;
}

void *qemu_memalign(size_t alignment, size_t size)
{
    void *p = qemu_try_memalign(alignment, size);
    if (p) {
        return p;
    }
    fprintf(stderr,
            "qemu_memalign: failed to allocate %zu bytes at alignment %zu: %s\n",
            size, alignment, strerror(errno));
    abort();
}

void qemu_vfree(void *ptr)
{
    trace_qemu_vfree(ptr);
#if !defined(CONFIG_POSIX_MEMALIGN) && defined(CONFIG_ALIGNED_MALLOC)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}
