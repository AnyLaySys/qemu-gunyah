
#ifndef QEMU_OS_WIN32_H
#define QEMU_OS_WIN32_H

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include "qemu/typedefs.h"

#ifdef HAVE_AFUNIX_H
#include <afunix.h>
#else
#define UNIX_PATH_MAX 108

typedef struct sockaddr_un {
    ADDRESS_FAMILY sun_family;
    char sun_path[UNIX_PATH_MAX];
} SOCKADDR_UN, *PSOCKADDR_UN;

#define SIO_AF_UNIX_GETPEERPID _WSAIOR(IOC_VENDOR, 256)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__aarch64__)
#undef setjmp
#undef longjmp
int __mingw_setjmp(jmp_buf);
void __attribute__((noreturn)) __mingw_longjmp(jmp_buf, int);
#define setjmp(env) __mingw_setjmp(env)
#define longjmp(env, val) __mingw_longjmp(env, val)
#elif defined(_WIN64)
# undef setjmp
# define setjmp(env) _setjmp(env, NULL)
#endif /* __aarch64__ */
#define sigjmp_buf jmp_buf
#define sigsetjmp(env, savemask) setjmp(env)
#define siglongjmp(env, val) longjmp(env, val)

#ifndef _POSIX_THREAD_SAFE_FUNCTIONS
#undef gmtime_r
struct tm *gmtime_r(const time_t *timep, struct tm *result);
#undef localtime_r
struct tm *localtime_r(const time_t *timep, struct tm *result);
#endif /* _POSIX_THREAD_SAFE_FUNCTIONS */

static inline void os_setup_signal_handling(void) {}
static inline void os_daemonize(void) {}
static inline void os_setup_post(void) {}
static inline void os_set_proc_name(const char *dummy) {}
void os_set_line_buffering(void);
void os_setup_early_signal_handling(void);

int getpagesize(void);

#if !defined(EPROTONOSUPPORT)
# define EPROTONOSUPPORT EINVAL
#endif

static inline int os_set_daemonize(bool d)
{
    if (d) {
        return -ENOTSUP;
    }
    return 0;
}

static inline bool is_daemonized(void)
{
    return false;
}

static inline int os_mlock(bool on_fault G_GNUC_UNUSED)
{
    return -ENOSYS;
}

static inline void os_setup_limits(void)
{
    return;
}

#define fsync _commit

#if !defined(lseek)
# define lseek _lseeki64
#endif

int qemu_ftruncate64(int, int64_t);

#if !defined(ftruncate)
# define ftruncate qemu_ftruncate64
#endif

static inline char *realpath(const char *path, char *resolved_path)
{
    _fullpath(resolved_path, path, _MAX_PATH);
    return resolved_path;
}

static inline void qemu_flockfile(FILE *f)
{
#ifdef HAVE__LOCK_FILE
    _lock_file(f);
#endif
}

static inline void qemu_funlockfile(FILE *f)
{
#ifdef HAVE__LOCK_FILE
    _unlock_file(f);
#endif
}

bool qemu_socket_select(int sockfd, WSAEVENT hEventObject,
                        long lNetworkEvents, Error **errp);

bool qemu_socket_unselect(int sockfd, Error **errp);


int qemu_close_socket_osfhandle(int fd);

#undef close
#define close qemu_close_wrap
int qemu_close_wrap(int fd);

#undef connect
#define connect qemu_connect_wrap
int qemu_connect_wrap(int sockfd, const struct sockaddr *addr,
                      socklen_t addrlen);

#undef listen
#define listen qemu_listen_wrap
int qemu_listen_wrap(int sockfd, int backlog);

#undef bind
#define bind qemu_bind_wrap
int qemu_bind_wrap(int sockfd, const struct sockaddr *addr,
                   socklen_t addrlen);

#undef socket
#define socket qemu_socket_wrap
int qemu_socket_wrap(int domain, int type, int protocol);

#undef accept
#define accept qemu_accept_wrap
int qemu_accept_wrap(int sockfd, struct sockaddr *addr,
                     socklen_t *addrlen);

#undef shutdown
#define shutdown qemu_shutdown_wrap
int qemu_shutdown_wrap(int sockfd, int how);

#undef ioctlsocket
#define ioctlsocket qemu_ioctlsocket_wrap
int qemu_ioctlsocket_wrap(int fd, int req, void *val);

#undef getsockopt
#define getsockopt qemu_getsockopt_wrap
int qemu_getsockopt_wrap(int sockfd, int level, int optname,
                         void *optval, socklen_t *optlen);

#undef setsockopt
#define setsockopt qemu_setsockopt_wrap
int qemu_setsockopt_wrap(int sockfd, int level, int optname,
                         const void *optval, socklen_t optlen);

#undef getpeername
#define getpeername qemu_getpeername_wrap
int qemu_getpeername_wrap(int sockfd, struct sockaddr *addr,
                          socklen_t *addrlen);

#undef getsockname
#define getsockname qemu_getsockname_wrap
int qemu_getsockname_wrap(int sockfd, struct sockaddr *addr,
                          socklen_t *addrlen);

#undef send
#define send qemu_send_wrap
ssize_t qemu_send_wrap(int sockfd, const void *buf, size_t len, int flags);

#undef sendto
#define sendto qemu_sendto_wrap
ssize_t qemu_sendto_wrap(int sockfd, const void *buf, size_t len, int flags,
                         const struct sockaddr *addr, socklen_t addrlen);

#undef recv
#define recv qemu_recv_wrap
ssize_t qemu_recv_wrap(int sockfd, void *buf, size_t len, int flags);

#undef recvfrom
#define recvfrom qemu_recvfrom_wrap
ssize_t qemu_recvfrom_wrap(int sockfd, void *buf, size_t len, int flags,
                           struct sockaddr *addr, socklen_t *addrlen);

EXCEPTION_DISPOSITION
win32_close_exception_handler(struct _EXCEPTION_RECORD*, void*,
                              struct _CONTEXT*, void*);

void *qemu_win32_map_alloc(size_t size, HANDLE *h, Error **errp);
void qemu_win32_map_free(void *ptr, HANDLE h, Error **errp);

#ifdef __cplusplus
}
#endif

#endif
