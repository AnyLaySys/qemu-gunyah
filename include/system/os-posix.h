
#ifndef QEMU_OS_POSIX_H
#define QEMU_OS_POSIX_H

#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/un.h>

#ifdef CONFIG_SYSMACROS
#include <sys/sysmacros.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

void os_set_line_buffering(void);
void os_setup_early_signal_handling(void);
void os_set_proc_name(const char *s);
void os_setup_signal_handling(void);
int os_set_daemonize(bool d);
bool is_daemonized(void);
void os_daemonize(void);
bool os_set_runas(const char *user_id);
void os_set_chroot(const char *path);
void os_setup_limits(void);
void os_setup_post(void);
int os_mlock(bool on_fault);

void *qemu_alloc_stack(size_t *sz);

void qemu_free_stack(void *stack, size_t sz);


static inline void qemu_flockfile(FILE *f)
{
    flockfile(f);
}

static inline void qemu_funlockfile(FILE *f)
{
    funlockfile(f);
}

#ifdef __cplusplus
}
#endif

#endif
