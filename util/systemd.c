
#include "qemu/osdep.h"
#include "qemu/systemd.h"
#include "qemu/cutils.h"
#include "qemu/error-report.h"

#ifndef _WIN32
unsigned int check_socket_activation(void)
{
    const char *s;
    unsigned long pid;
    unsigned long nr_fds;
    unsigned int i;
    int fd;
    int f;
    int err;

    s = getenv("LISTEN_PID");
    if (s == NULL) {
        return 0;
    }
    err = qemu_strtoul(s, NULL, 10, &pid);
    if (err) {
        return 0;
    }
    if (pid != getpid()) {
        return 0;
    }

    s = getenv("LISTEN_FDS");
    if (s == NULL) {
        return 0;
    }
    err = qemu_strtoul(s, NULL, 10, &nr_fds);
    if (err) {
        return 0;
    }
    assert(nr_fds <= UINT_MAX);

    unsetenv("LISTEN_FDS");
    unsetenv("LISTEN_PID");
    unsetenv("LISTEN_FDNAMES");

    for (i = 0; i < nr_fds; ++i) {
        fd = FIRST_SOCKET_ACTIVATION_FD + i;
        f = fcntl(fd, F_GETFD);
        if (f == -1 || fcntl(fd, F_SETFD, f | FD_CLOEXEC) == -1) {
            error_report("Socket activation failed: "
                         "invalid file descriptor fd = %d: %s",
                         fd, g_strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    return (unsigned int) nr_fds;
}

#else /* !_WIN32 */
unsigned int check_socket_activation(void)
{
    return 0;
}
#endif
