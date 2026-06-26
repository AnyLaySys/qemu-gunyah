
#include "qemu/osdep.h"

#ifdef CONFIG_GETAUXVAL

#include <sys/auxv.h>

unsigned long qemu_getauxval(unsigned long key)
{
    return getauxval(key);
}
#elif defined(__linux__)
#include "elf.h"

typedef struct {
    unsigned long a_type;
    unsigned long a_val;
} ElfW_auxv_t;

static const ElfW_auxv_t *auxv;

static const ElfW_auxv_t *qemu_init_auxval(void)
{
    ElfW_auxv_t *a;
    ssize_t size = 512, r, ofs;
    int fd;

    auxv = a = g_malloc(size);
    a[0].a_type = 0;
    a[0].a_val = 0;

    fd = open("/proc/self/auxv", O_RDONLY);
    if (fd < 0) {
        return a;
    }

    r = read(fd, a, size);

    if (r == size) {
        do {
            ofs = size;
            size *= 2;
            auxv = a = g_realloc(a, size);
            r = read(fd, (char *)a + ofs, ofs);
        } while (r == ofs);
    }

    close(fd);
    return a;
}

unsigned long qemu_getauxval(unsigned long type)
{
    const ElfW_auxv_t *a = auxv;

    if (unlikely(a == NULL)) {
        a = qemu_init_auxval();
    }

    for (; a->a_type != 0; a++) {
        if (a->a_type == type) {
            return a->a_val;
        }
    }

    errno = ENOENT;
    return 0;
}

#elif defined(CONFIG_ELF_AUX_INFO)
#include <sys/auxv.h>

unsigned long qemu_getauxval(unsigned long type)
{
    unsigned long aux = 0;
    int ret = elf_aux_info(type, &aux, sizeof(aux));
    if (ret != 0) {
        errno = ret;
    }
    return aux;
}

#else

unsigned long qemu_getauxval(unsigned long type)
{
    errno = ENOSYS;
    return 0;
}

#endif
