#ifndef QEMU_OSDEP_H
#define QEMU_OSDEP_H

#if !defined _FORTIFY_SOURCE && defined __OPTIMIZE__ && __OPTIMIZE__ && defined __linux__
# define _FORTIFY_SOURCE 2
#endif

#include "config-host.h"
#ifdef COMPILING_PER_TARGET
#include CONFIG_TARGET
#else
#include "exec/poison.h"
#endif

#pragma GCC poison HOST_WORDS_BIGENDIAN

#pragma GCC poison TARGET_WORDS_BIGENDIAN

#include "qemu/compiler.h"

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif
#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#ifdef __APPLE__
#define daemon qemu_fake_daemon_function
#include <stdlib.h>
#undef daemon
QEMU_EXTERN_C int daemon(int, int);
#endif

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602 /* Windows 8 API (should be >= the one from glib) */
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#ifdef __MINGW32__
#define __USE_MINGW_ANSI_STDIO 1
#endif

#ifdef __FreeBSD__
#define _WANT_FREEBSD11_STAT
#define _WANT_FREEBSD11_STATFS
#define _WANT_FREEBSD11_DIRENT
#define _WANT_KERNEL_ERRNO
#define _WANT_SEMUN
#endif

#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>

#include <string.h>
#include <strings.h>
#include <inttypes.h>
#include <limits.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <assert.h>
#include <setjmp.h>
#include <signal.h>

#ifdef CONFIG_IOVEC
#include <sys/uio.h>
#endif

#if defined(__linux__) && defined(__sparc__)
#include <sys/shm.h>
#endif

#ifndef _WIN32
#include <sys/wait.h>
#else
#define WIFEXITED(x)   1
#define WEXITSTATUS(x) (x)
#endif

#ifdef __APPLE__
#include <AvailabilityMacros.h>
#endif

#include "glib-compat.h"

#ifdef _WIN32
#include "system/os-win32.h"
#endif

#ifdef CONFIG_POSIX
#include "system/os-posix.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "qemu/typedefs.h"

#ifdef __clang__
#define coroutine_fn QEMU_ANNOTATE("coroutine_fn")
#else
#define coroutine_fn
#endif

#ifdef __clang__
#define coroutine_mixed_fn QEMU_ANNOTATE("coroutine_mixed_fn")
#else
#define coroutine_mixed_fn
#endif

#ifdef __clang__
#define no_coroutine_fn QEMU_ANNOTATE("no_coroutine_fn")
#else
#define no_coroutine_fn
#endif


#ifdef __MINGW32__
#undef assert
#define assert(x)  g_assert(x)
#endif

G_NORETURN
void QEMU_ERROR("code path is reachable")
    qemu_build_not_reached_always(void);
#if defined(__OPTIMIZE__) && !defined(__NO_INLINE__)
#define qemu_build_not_reached()  qemu_build_not_reached_always()
#else
#define qemu_build_not_reached()  g_assert_not_reached()
#endif

#define qemu_build_assert(test)  while (!(test)) qemu_build_not_reached()

#ifndef WCOREDUMP
#define WCOREDUMP(status) 0
#endif
#ifdef NDEBUG
#error building with NDEBUG is not supported
#endif
#ifdef G_DISABLE_ASSERT
#error building with G_DISABLE_ASSERT is not supported
#endif

#ifndef OFF_MAX
#define OFF_MAX (sizeof (off_t) == 8 ? INT64_MAX : INT32_MAX)
#endif

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif
#ifndef O_BINARY
#define O_BINARY 0
#endif
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif
#ifndef MAP_NORESERVE
#define MAP_NORESERVE 0
#endif
#ifndef ENOMEDIUM
#define ENOMEDIUM ENODEV
#endif
#if !defined(ENOTSUP)
#define ENOTSUP 4096
#endif
#if !defined(ECANCELED)
#define ECANCELED 4097
#endif
#if !defined(EMEDIUMTYPE)
#define EMEDIUMTYPE 4098
#endif
#if !defined(ESHUTDOWN)
#define ESHUTDOWN 4099
#endif

#define RETRY_ON_EINTR(expr) \
    (__extension__                                          \
        ({ typeof(expr) __result;                               \
           do {                                             \
                __result = (expr);         \
           } while (__result == -1 && errno == EINTR);     \
           __result; }))



#define TYPE_SIGNED(t) (!((t)0 < (t)-1))

#define TYPE_WIDTH(t) (sizeof(t) * CHAR_BIT)

#define TYPE_MAXIMUM(t)                                                \
  ((t) (!TYPE_SIGNED(t)                                                \
        ? (t)-1                                                        \
        : ((((t)1 << (TYPE_WIDTH(t) - 2)) - 1) * 2 + 1)))

#ifndef TIME_MAX
#define TIME_MAX TYPE_MAXIMUM(time_t)
#endif

#ifdef HAVE_BROKEN_SIZE_MAX
#undef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

#define MIN_INTERNAL(a, b, _a, _b)                      \
    ({                                                  \
        typeof(1 ? (a) : (b)) _a = (a), _b = (b);       \
        _a < _b ? _a : _b;                              \
    })
#undef MIN
#define MIN(a, b) \
    MIN_INTERNAL((a), (b), MAKE_IDENTIFIER(_a), MAKE_IDENTIFIER(_b))

#define MAX_INTERNAL(a, b, _a, _b)                      \
    ({                                                  \
        typeof(1 ? (a) : (b)) _a = (a), _b = (b);       \
        _a > _b ? _a : _b;                              \
    })
#undef MAX
#define MAX(a, b) \
    MAX_INTERNAL((a), (b), MAKE_IDENTIFIER(_a), MAKE_IDENTIFIER(_b))

#ifdef __COVERITY__
# define MIN_CONST(a, b) ((a) < (b) ? (a) : (b))
# define MAX_CONST(a, b) ((a) > (b) ? (a) : (b))
#else
# define MIN_CONST(a, b)                                        \
    __builtin_choose_expr(                                      \
        __builtin_constant_p(a) && __builtin_constant_p(b),     \
        (a) < (b) ? (a) : (b),                                  \
        ((void)0))
# define MAX_CONST(a, b)                                        \
    __builtin_choose_expr(                                      \
        __builtin_constant_p(a) && __builtin_constant_p(b),     \
        (a) > (b) ? (a) : (b),                                  \
        ((void)0))
#endif

#define MIN_NON_ZERO_INTERNAL(a, b, _a, _b)             \
    ({                                                  \
        typeof(1 ? (a) : (b)) _a = (a), _b = (b);       \
        _a == 0 ? _b : (_b == 0 || _b > _a) ? _a : _b;  \
    })
#define MIN_NON_ZERO(a, b) \
    MIN_NON_ZERO_INTERNAL((a), (b), MAKE_IDENTIFIER(_a), MAKE_IDENTIFIER(_b))

#define QEMU_ALIGN_DOWN(n, m) ((n) / (m) * (m))

#define QEMU_ALIGN_UP(n, m) QEMU_ALIGN_DOWN((n) + (m) - 1, (m))

#define QEMU_IS_ALIGNED(n, m) (((n) % (m)) == 0)

#define QEMU_ALIGN_PTR_DOWN(p, n) \
    ((typeof(p))QEMU_ALIGN_DOWN((uintptr_t)(p), (n)))

#define QEMU_ALIGN_PTR_UP(p, n) \
    ((typeof(p))QEMU_ALIGN_UP((uintptr_t)(p), (n)))

#define QEMU_PTR_IS_ALIGNED(p, n) QEMU_IS_ALIGNED((uintptr_t)(p), (n))

#ifndef ROUND_DOWN
#define ROUND_DOWN(n, d) ((n) & -(0 ? (n) : (d)))
#endif

#ifndef ROUND_UP
#define ROUND_UP(n, d) ROUND_DOWN((n) + (d) - 1, (d))
#endif

#ifndef DIV_ROUND_UP
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#endif

#define QEMU_IS_ARRAY(x) (!__builtin_types_compatible_p(typeof(x), \
                                                        typeof(&(x)[0])))
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) ((sizeof(x) / sizeof((x)[0])) + \
                       QEMU_BUILD_BUG_ON_ZERO(!QEMU_IS_ARRAY(x)))
#endif

int qemu_daemon(int nochdir, int noclose);
void *qemu_anon_ram_alloc(size_t size, uint64_t *align, bool shared,
                          bool noreserve);
void qemu_anon_ram_free(void *ptr, size_t size);
int qemu_shm_alloc(size_t size, Error **errp);

#ifdef _WIN32
#define HAVE_CHARDEV_SERIAL 1
#define HAVE_CHARDEV_PARALLEL 1
#else
#if defined(__linux__) || defined(__sun__) || defined(__FreeBSD__)   \
    || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) \
    || defined(__GLIBC__) || defined(__APPLE__)
#define HAVE_CHARDEV_SERIAL 1
#endif
#if defined(__linux__) || defined(__FreeBSD__) \
    || defined(__FreeBSD_kernel__) || defined(__DragonFly__)
#define HAVE_CHARDEV_PARALLEL 1
#endif
#endif

#if defined(__HAIKU__)
#define SIGIO SIGPOLL
#endif

#ifdef HAVE_MADVISE_WITHOUT_PROTOTYPE
int madvise(char *, size_t, int);
#endif

#if defined(CONFIG_LINUX)
#ifndef BUS_MCEERR_AR
#define BUS_MCEERR_AR 4
#endif
#ifndef BUS_MCEERR_AO
#define BUS_MCEERR_AO 5
#endif
#endif

#if defined(__linux__) && \
    (defined(__x86_64__) || defined(__arm__) || defined(__aarch64__) \
     || defined(__powerpc64__))
#  define QEMU_VMALLOC_ALIGN (512 * 4096)
#elif defined(__linux__) && defined(__s390x__)
#  define QEMU_VMALLOC_ALIGN (256 * 4096)
#elif defined(__linux__) && defined(__sparc__)
#  define QEMU_VMALLOC_ALIGN MAX(qemu_real_host_page_size(), SHMLBA)
#elif defined(__linux__) && defined(__loongarch__)
#  define QEMU_VMALLOC_ALIGN (qemu_real_host_page_size() * \
                         qemu_real_host_page_size() / sizeof(long))
#else
#  define QEMU_VMALLOC_ALIGN qemu_real_host_page_size()
#endif

#ifdef CONFIG_POSIX
struct qemu_signalfd_siginfo {
    uint32_t ssi_signo;   /* Signal number */
    int32_t  ssi_errno;   /* Error number (unused) */
    int32_t  ssi_code;    /* Signal code */
    uint32_t ssi_pid;     /* PID of sender */
    uint32_t ssi_uid;     /* Real UID of sender */
    int32_t  ssi_fd;      /* File descriptor (SIGIO) */
    uint32_t ssi_tid;     /* Kernel timer ID (POSIX timers) */
    uint32_t ssi_band;    /* Band event (SIGIO) */
    uint32_t ssi_overrun; /* POSIX timer overrun count */
    uint32_t ssi_trapno;  /* Trap number that caused signal */
    int32_t  ssi_status;  /* Exit status or signal (SIGCHLD) */
    int32_t  ssi_int;     /* Integer sent by sigqueue(2) */
    uint64_t ssi_ptr;     /* Pointer sent by sigqueue(2) */
    uint64_t ssi_utime;   /* User CPU time consumed (SIGCHLD) */
    uint64_t ssi_stime;   /* System CPU time consumed (SIGCHLD) */
    uint64_t ssi_addr;    /* Address that generated signal
                             (for hardware-generated signals) */
    uint8_t  pad[48];     /* Pad size to 128 bytes (allow for
                             additional fields in the future) */
};

int qemu_signalfd(const sigset_t *mask);
void sigaction_invoke(struct sigaction *action,
                      struct qemu_signalfd_siginfo *info);
#endif

int qemu_open_old(const char *name, int flags, ...);
int qemu_open(const char *name, int flags, Error **errp);
int qemu_create(const char *name, int flags, mode_t mode, Error **errp);
int qemu_close(int fd);
int qemu_unlink(const char *name);
#ifndef _WIN32
int qemu_dup_flags(int fd, int flags);
int qemu_dup(int fd);
int qemu_lock_fd(int fd, int64_t start, int64_t len, bool exclusive);
int qemu_unlock_fd(int fd, int64_t start, int64_t len);
int qemu_lock_fd_test(int fd, int64_t start, int64_t len, bool exclusive);
bool qemu_has_ofd_lock(void);
#endif

bool qemu_has_direct_io(void);

#if defined(__HAIKU__) && defined(__i386__)
#define FMT_pid "%ld"
#elif defined(WIN64)
#define FMT_pid "%" PRId64
#else
#define FMT_pid "%d"
#endif

bool qemu_write_pidfile(const char *pidfile, Error **errp);

int qemu_get_thread_id(void);

int qemu_kill_thread(int tid, int sig);

#ifndef CONFIG_IOVEC
struct iovec {
    void *iov_base;
    size_t iov_len;
};
#define IOV_MAX 1024

ssize_t readv(int fd, const struct iovec *iov, int iov_cnt);
ssize_t writev(int fd, const struct iovec *iov, int iov_cnt);
#endif

#ifdef _WIN32
static inline void qemu_timersub(const struct timeval *val1,
                                 const struct timeval *val2,
                                 struct timeval *res)
{
    res->tv_sec = val1->tv_sec - val2->tv_sec;
    if (val1->tv_usec < val2->tv_usec) {
        res->tv_sec--;
        res->tv_usec = val1->tv_usec - val2->tv_usec + 1000 * 1000;
    } else {
        res->tv_usec = val1->tv_usec - val2->tv_usec;
    }
}
#else
#define qemu_timersub timersub
#endif

ssize_t qemu_write_full(int fd, const void *buf, size_t count)
    G_GNUC_WARN_UNUSED_RESULT;

void qemu_set_cloexec(int fd);

char *qemu_get_local_state_dir(void);

unsigned long qemu_getauxval(unsigned long type);

void qemu_set_tty_echo(int fd, bool echo);

typedef struct ThreadContext ThreadContext;

bool qemu_prealloc_mem(int fd, char *area, size_t sz, int max_threads,
                       ThreadContext *tc, bool async, Error **errp);

bool qemu_finish_async_prealloc_mem(Error **errp);

char *qemu_get_pid_name(pid_t pid);

static inline uintptr_t qemu_real_host_page_size(void)
{
    return getpagesize();
}

static inline intptr_t qemu_real_host_page_mask(void)
{
    return -(intptr_t)qemu_real_host_page_size();
}

static inline void qemu_reset_optind(void)
{
#ifdef HAVE_OPTRESET
    optind = 1;
    optreset = 1;
#else
    optind = 0;
#endif
}

int qemu_fdatasync(int fd);

void qemu_close_all_open_fd(const int *skip, unsigned int nskip);

int qemu_msync(void *addr, size_t length, int fd);

size_t qemu_get_host_physmem(void);

#ifdef __APPLE__
static inline void qemu_thread_jit_execute(void)
{
    pthread_jit_write_protect_np(true);
}

static inline void qemu_thread_jit_write(void)
{
    pthread_jit_write_protect_np(false);
}
#else
static inline void qemu_thread_jit_write(void) {}
static inline void qemu_thread_jit_execute(void) {}
#endif

#ifndef HAVE_SYSTEM_FUNCTION
#define system platform_does_not_support_system
static inline int platform_does_not_support_system(const char *command)
{
    errno = ENOSYS;
    return -1;
}
#endif /* !HAVE_SYSTEM_FUNCTION */

#ifdef __cplusplus
}
#endif

#endif
