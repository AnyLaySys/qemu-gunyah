
#include "qemu/osdep.h"

#include "qapi/error.h"
#include "qemu/error-report.h"
#include "qemu/memfd.h"
#include "qemu/host-utils.h"

#if defined CONFIG_LINUX && !defined CONFIG_MEMFD
#include <sys/syscall.h>
#include <asm/unistd.h>

int memfd_create(const char *name, unsigned int flags)
{
#ifdef __NR_memfd_create
    return syscall(__NR_memfd_create, name, flags);
#else
    errno = ENOSYS;
    return -1;
#endif
}
#endif

int qemu_memfd_create(const char *name, size_t size, bool hugetlb,
                      uint64_t hugetlbsize, unsigned int seals, Error **errp)
{
    int htsize = hugetlbsize ? ctz64(hugetlbsize) : 0;

    if (htsize && 1ULL << htsize != hugetlbsize) {
        error_setg(errp, "Hugepage size must be a power of 2");
        return -1;
    }

    htsize = htsize << MFD_HUGE_SHIFT;

#ifdef CONFIG_LINUX
    int mfd = -1;
    unsigned int flags = MFD_CLOEXEC;

    if (seals) {
        flags |= MFD_ALLOW_SEALING;
    }
    if (hugetlb) {
        flags |= MFD_HUGETLB;
        flags |= htsize;
    }
    mfd = memfd_create(name, flags);
    if (mfd < 0) {
        error_setg_errno(errp, errno,
                         "failed to create memfd with flags 0x%x", flags);
        goto err;
    }

    if (ftruncate(mfd, size) == -1) {
        error_setg_errno(errp, errno, "failed to resize memfd to %zu", size);
        goto err;
    }

    if (seals && fcntl(mfd, F_ADD_SEALS, seals) == -1) {
        error_setg_errno(errp, errno, "failed to add seals 0x%x", seals);
        goto err;
    }

    return mfd;

err:
    if (mfd >= 0) {
        close(mfd);
    }
#else
    error_setg_errno(errp, ENOSYS, "failed to create memfd");
#endif
    return -1;
}

void *qemu_memfd_alloc(const char *name, size_t size, unsigned int seals,
                       int *fd, Error **errp)
{
    void *ptr;
    int mfd = qemu_memfd_create(name, size, false, 0, seals, NULL);

    if (mfd == -1) {
        mfd = qemu_memfd_create(name, size, false, 0, 0, NULL);
    }

    if (mfd == -1) {
        const char *tmpdir = g_get_tmp_dir();
        gchar *fname;

        fname = g_strdup_printf("%s/memfd-XXXXXX", tmpdir);
        mfd = mkstemp(fname);
        unlink(fname);
        g_free(fname);

        if (mfd == -1 ||
            ftruncate(mfd, size) == -1) {
            goto err;
        }
    }

    ptr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, mfd, 0);
    if (ptr == MAP_FAILED) {
        goto err;
    }

    *fd = mfd;
    return ptr;

err:
    error_setg_errno(errp, errno, "failed to allocate shared memory");
    if (mfd >= 0) {
        close(mfd);
    }
    return NULL;
}

void qemu_memfd_free(void *ptr, size_t size, int fd)
{
    if (ptr) {
        if (munmap(ptr, size) != 0) {
            error_report("memfd munmap() failed: %s", strerror(errno));
        }
    }

    if (fd != -1) {
        if (close(fd) != 0) {
            error_report("memfd close() failed: %s", strerror(errno));
        }
    }
}

enum {
    MEMFD_KO,
    MEMFD_OK,
    MEMFD_TODO
};

bool qemu_memfd_alloc_check(void)
{
    static int memfd_check = MEMFD_TODO;

    if (memfd_check == MEMFD_TODO) {
        int fd;
        void *ptr;

        fd = -1;
        ptr = qemu_memfd_alloc("test", 4096, 0, &fd, NULL);
        memfd_check = ptr ? MEMFD_OK : MEMFD_KO;
        qemu_memfd_free(ptr, 4096, fd);
    }

    return memfd_check == MEMFD_OK;
}

bool qemu_memfd_check(unsigned int flags)
{
#ifdef CONFIG_LINUX
    int mfd;
    static int memfd_check = MEMFD_TODO;

    if (!flags && memfd_check != MEMFD_TODO) {
        return memfd_check;
    }

    mfd = memfd_create("test", flags | MFD_CLOEXEC);
    if (mfd >= 0) {
        close(mfd);
    }
    if (!flags) {
        memfd_check = (mfd >= 0) ? MEMFD_OK : MEMFD_KO;
    }
    return (mfd >= 0);

#endif

    return false;
}
