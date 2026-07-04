
#include "qemu/osdep.h"
#include "qemu/chardev_open.h"

static int open_cdev_internal(const char *path, dev_t cdev)
{
    struct stat st;
    int fd;

    fd = qemu_open_old(path, O_RDWR);
    if (fd == -1) {
        return -1;
    }
    if (fstat(fd, &st) || !S_ISCHR(st.st_mode) ||
        (cdev != 0 && st.st_rdev != cdev)) {
        close(fd);
        return -1;
    }
    return fd;
}

static int open_cdev_robust(dev_t cdev)
{
    g_autofree char *devpath = NULL;

    devpath = g_strdup_printf("/dev/char/%u:%u", major(cdev), minor(cdev));
    return open_cdev_internal(devpath, cdev);
}

int open_cdev(const char *devpath, dev_t cdev)
{
    int fd;

    fd = open_cdev_internal(devpath, cdev);
    if (fd == -1 && cdev != 0) {
        return open_cdev_robust(cdev);
    }
    return fd;
}
