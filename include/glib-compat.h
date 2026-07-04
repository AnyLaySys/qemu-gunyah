
#ifndef QEMU_GLIB_COMPAT_H
#define QEMU_GLIB_COMPAT_H

#define GLIB_VERSION_MIN_REQUIRED GLIB_VERSION_2_66

#define GLIB_VERSION_MAX_ALLOWED GLIB_VERSION_2_66

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include <glib.h>
#if defined(G_OS_UNIX)
#include <glib-unix.h>
#include <sys/types.h>
#include <pwd.h>
#endif


static inline gpointer g_memdup2_qemu(gconstpointer mem, gsize byte_size)
{
#if GLIB_CHECK_VERSION(2, 68, 0)
    return g_memdup2(mem, byte_size);
#else
    gpointer new_mem;

    if (mem && byte_size != 0) {
        new_mem = g_malloc(byte_size);
        memcpy(new_mem, mem, byte_size);
    } else {
        new_mem = NULL;
    }

    return new_mem;
#endif
}
#define g_memdup2(m, s) g_memdup2_qemu(m, s)

static inline bool
qemu_g_test_slow(void)
{
    static int cached = -1;
    if (cached == -1) {
        cached = g_test_slow() || getenv("G_TEST_SLOW") != NULL;
    }
    return cached;
}

#undef g_test_slow
#undef g_test_thorough
#undef g_test_quick
#define g_test_slow() qemu_g_test_slow()
#define g_test_thorough() qemu_g_test_slow()
#define g_test_quick() (!qemu_g_test_slow())

#pragma GCC diagnostic pop

#ifndef G_NORETURN
#define G_NORETURN G_GNUC_NORETURN
#endif

#endif
