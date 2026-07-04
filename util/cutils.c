
#include "qemu/osdep.h"
#include "qemu/host-utils.h"
#include <math.h>

#ifdef __FreeBSD__
#include <sys/sysctl.h>
#include <sys/user.h>
#endif

#ifdef __NetBSD__
#include <sys/sysctl.h>
#endif

#ifdef __HAIKU__
#include <kernel/image.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#ifdef G_OS_WIN32
#include <pathcch.h>
#include <wchar.h>
#endif

#include "qemu/ctype.h"
#include "qemu/cutils.h"
#include "qemu/error-report.h"

void strpadcpy(char *buf, int buf_size, const char *str, char pad)
{
    int len = qemu_strnlen(str, buf_size);
    memcpy(buf, str, len);
    memset(buf + len, pad, buf_size - len);
}

void pstrcpy(char *buf, int buf_size, const char *str)
{
    int c;
    char *q = buf;

    if (buf_size <= 0)
        return;

    for(;;) {
        c = *str++;
        if (c == 0 || q >= buf + buf_size - 1)
            break;
        *q++ = c;
    }
    *q = '\0';
}

char *pstrcat(char *buf, int buf_size, const char *s)
{
    int len;
    len = strlen(buf);
    if (len < buf_size)
        pstrcpy(buf + len, buf_size - len, s);
    return buf;
}

int strstart(const char *str, const char *val, const char **ptr)
{
    const char *p, *q;
    p = str;
    q = val;
    while (*q != '\0') {
        if (*p != *q)
            return 0;
        p++;
        q++;
    }
    if (ptr)
        *ptr = p;
    return 1;
}

int stristart(const char *str, const char *val, const char **ptr)
{
    const char *p, *q;
    p = str;
    q = val;
    while (*q != '\0') {
        if (qemu_toupper(*p) != qemu_toupper(*q))
            return 0;
        p++;
        q++;
    }
    if (ptr)
        *ptr = p;
    return 1;
}

int qemu_strnlen(const char *s, int max_len)
{
    int i;

    for(i = 0; i < max_len; i++) {
        if (s[i] == '\0') {
            break;
        }
    }
    return i;
}

char *qemu_strsep(char **input, const char *delim)
{
    char *result = *input;
    if (result != NULL) {
        char *p;

        for (p = result; *p != '\0'; p++) {
            if (strchr(delim, *p)) {
                break;
            }
        }
        if (*p == '\0') {
            *input = NULL;
        } else {
            *p = '\0';
            *input = p + 1;
        }
    }
    return result;
}

time_t mktimegm(struct tm *tm)
{
    time_t t;
    int y = tm->tm_year + 1900, m = tm->tm_mon + 1, d = tm->tm_mday;
    if (m < 3) {
        m += 12;
        y--;
    }
    t = 86400ULL * (d + (153 * m - 457) / 5 + 365 * y + y / 4 - y / 100 + 
                 y / 400 - 719469);
    t += 3600 * tm->tm_hour + 60 * tm->tm_min + tm->tm_sec;
    return t;
}

static int64_t suffix_mul(char suffix, int64_t unit)
{
    switch (qemu_toupper(suffix)) {
    case 'B':
        return 1;
    case 'K':
        return unit;
    case 'M':
        return unit * unit;
    case 'G':
        return unit * unit * unit;
    case 'T':
        return unit * unit * unit * unit;
    case 'P':
        return unit * unit * unit * unit * unit;
    case 'E':
        return unit * unit * unit * unit * unit * unit;
    }
    return -1;
}

static int do_strtosz(const char *nptr, const char **end,
                      const char default_suffix, int64_t unit,
                      uint64_t *result)
{
    int retval;
    const char *endptr;
    unsigned char c;
    uint64_t val = 0, valf = 0;
    int64_t mul;

    retval = parse_uint(nptr, &endptr, 10, &val);
    if (retval == -ERANGE || !nptr) {
        goto out;
    }
    if (retval == 0 && val == 0 && (*endptr == 'x' || *endptr == 'X')) {
        retval = qemu_strtou64(nptr, &endptr, 16, &val);
        if (retval) {
            goto out;
        }
        if (*endptr == '.' || suffix_mul(*endptr, unit) > 0) {
            endptr = nptr;
            retval = -EINVAL;
            goto out;
        }
    } else if (*endptr == '.' || (endptr == nptr && strchr(nptr, '.'))) {
        double fraction = 0.0;

        if (retval == 0 && *endptr == '.' && !isdigit(endptr[1])) {
            endptr++;
        } else {
            char *e;
            const char *tail;
            g_autofree char *copy = g_strdup(endptr);

            e = strchr(copy, 'e');
            if (e) {
                *e = '\0';
            }
            e = strchr(copy, 'E');
            if (e) {
                *e = '\0';
            }
            retval = qemu_strtod_finite(copy, &tail, &fraction);
            endptr += tail - copy;
            if (signbit(fraction)) {
                retval = -ERANGE;
                goto out;
            }
        }

        if (fraction == 1.0) {
            if (val == UINT64_MAX) {
                retval = -ERANGE;
                goto out;
            }
            val++;
        } else if (retval == -ERANGE) {
            valf = 1;
            retval = 0;
        } else {
            valf = (uint64_t)(fraction * 0x1p64);
            if (valf == 0 && fraction > 0.0) {
                valf = 1;
            }
        }
    }
    if (retval) {
        goto out;
    }
    c = *endptr;
    mul = suffix_mul(c, unit);
    if (mul > 0) {
        endptr++;
    } else {
        mul = suffix_mul(default_suffix, unit);
        assert(mul > 0);
    }
    if (mul == 1) {
        if (valf != 0) {
            endptr = nptr;
            retval = -EINVAL;
            goto out;
        }
    } else {
        uint64_t valh, tmp;

        mulu64(&val, &valh, val, mul);
        mulu64(&valf, &tmp, valf, mul);
        val += tmp;
        valh += val < tmp;

        tmp = valf >> 63;
        val += tmp;
        valh += val < tmp;

        if (valh != 0) {
            retval = -ERANGE;
            goto out;
        }
    }

    retval = 0;

out:
    if (end) {
        *end = endptr;
    } else if (nptr && *endptr) {
        retval = -EINVAL;
    }
    if (retval == 0) {
        *result = val;
    } else {
        *result = 0;
        if (end && retval == -EINVAL) {
            *end = nptr;
        }
    }

    return retval;
}

int qemu_strtosz(const char *nptr, const char **end, uint64_t *result)
{
    return do_strtosz(nptr, end, 'B', 1024, result);
}

int qemu_strtosz_MiB(const char *nptr, const char **end, uint64_t *result)
{
    return do_strtosz(nptr, end, 'M', 1024, result);
}

int qemu_strtosz_metric(const char *nptr, const char **end, uint64_t *result)
{
    return do_strtosz(nptr, end, 'B', 1000, result);
}

static int check_strtox_error(const char *nptr, char *ep,
                              const char **endptr, bool check_zero,
                              int libc_errno)
{
    assert(ep >= nptr);

    if (check_zero && ep == nptr && libc_errno == 0) {
        char *tmp;

        errno = 0;
        if (strtol(nptr, &tmp, 10) == 0 && errno == 0 &&
            (*tmp == 'x' || *tmp == 'X')) {
            ep = tmp;
        }
    }

    if (endptr) {
        *endptr = ep;
    }

    if (libc_errno == 0 && ep == nptr) {
        return -EINVAL;
    }

    if (!endptr && *ep) {
        return -EINVAL;
    }

    return -libc_errno;
}

int qemu_strtoi(const char *nptr, const char **endptr, int base,
                int *result)
{
    char *ep;
    long long lresult;

    assert((unsigned) base <= 36 && base != 1);
    if (!nptr) {
        *result = 0;
        if (endptr) {
            *endptr = nptr;
        }
        return -EINVAL;
    }

    errno = 0;
    lresult = strtoll(nptr, &ep, base);
    if (lresult < INT_MIN) {
        *result = INT_MIN;
        errno = ERANGE;
    } else if (lresult > INT_MAX) {
        *result = INT_MAX;
        errno = ERANGE;
    } else {
        *result = lresult;
    }
    return check_strtox_error(nptr, ep, endptr, lresult == 0, errno);
}

int qemu_strtoui(const char *nptr, const char **endptr, int base,
                 unsigned int *result)
{
    char *ep;
    unsigned long long lresult;
    bool neg;

    assert((unsigned) base <= 36 && base != 1);
    if (!nptr) {
        *result = 0;
        if (endptr) {
            *endptr = nptr;
        }
        return -EINVAL;
    }

    errno = 0;
    lresult = strtoull(nptr, &ep, base);

    if (errno == ERANGE) {
        *result = -1;
    } else {
        neg = memchr(nptr, '-', ep - nptr) != NULL;
        if (neg) {
            lresult = -lresult;
        }
        if (lresult > UINT_MAX) {
            *result = UINT_MAX;
            errno = ERANGE;
        } else {
            *result = neg ? -lresult : lresult;
        }
    }
    return check_strtox_error(nptr, ep, endptr, lresult == 0, errno);
}

int qemu_strtol(const char *nptr, const char **endptr, int base,
                long *result)
{
    char *ep;

    assert((unsigned) base <= 36 && base != 1);
    if (!nptr) {
        *result = 0;
        if (endptr) {
            *endptr = nptr;
        }
        return -EINVAL;
    }

    errno = 0;
    *result = strtol(nptr, &ep, base);
    return check_strtox_error(nptr, ep, endptr, *result == 0, errno);
}

int qemu_strtoul(const char *nptr, const char **endptr, int base,
                 unsigned long *result)
{
    char *ep;

    assert((unsigned) base <= 36 && base != 1);
    if (!nptr) {
        *result = 0;
        if (endptr) {
            *endptr = nptr;
        }
        return -EINVAL;
    }

    errno = 0;
    *result = strtoul(nptr, &ep, base);
    if (errno == ERANGE) {
        *result = -1;
    }
    return check_strtox_error(nptr, ep, endptr, *result == 0, errno);
}

int qemu_strtoi64(const char *nptr, const char **endptr, int base,
                 int64_t *result)
{
    char *ep;

    assert((unsigned) base <= 36 && base != 1);
    if (!nptr) {
        *result = 0;
        if (endptr) {
            *endptr = nptr;
        }
        return -EINVAL;
    }

    QEMU_BUILD_BUG_ON(sizeof(int64_t) != sizeof(long long));
    errno = 0;
    *result = strtoll(nptr, &ep, base);
    return check_strtox_error(nptr, ep, endptr, *result == 0, errno);
}

int qemu_strtou64(const char *nptr, const char **endptr, int base,
                  uint64_t *result)
{
    char *ep;

    assert((unsigned) base <= 36 && base != 1);
    if (!nptr) {
        *result = 0;
        if (endptr) {
            *endptr = nptr;
        }
        return -EINVAL;
    }

    QEMU_BUILD_BUG_ON(sizeof(uint64_t) != sizeof(unsigned long long));
    errno = 0;
    *result = strtoull(nptr, &ep, base);
    if (errno == ERANGE) {
        *result = -1;
    }
    return check_strtox_error(nptr, ep, endptr, *result == 0, errno);
}

int qemu_strtod(const char *nptr, const char **endptr, double *result)
{
    char *ep;

    if (!nptr) {
        *result = 0.0;
        if (endptr) {
            *endptr = nptr;
        }
        return -EINVAL;
    }

    errno = 0;
    *result = strtod(nptr, &ep);
    return check_strtox_error(nptr, ep, endptr, false, errno);
}

int qemu_strtod_finite(const char *nptr, const char **endptr, double *result)
{
    const char *tmp;
    int ret;

    ret = qemu_strtod(nptr, &tmp, result);
    if (!isfinite(*result)) {
        if (endptr) {
            *endptr = nptr;
        }
        *result = 0.0;
        ret = -EINVAL;
    } else if (endptr) {
        *endptr = tmp;
    } else if (*tmp) {
        ret = -EINVAL;
    }
    return ret;
}

#ifndef HAVE_STRCHRNUL
const char *qemu_strchrnul(const char *s, int c)
{
    const char *e = strchr(s, c);
    if (!e) {
        e = s + strlen(s);
    }
    return e;
}
#endif

int parse_uint(const char *s, const char **endptr, int base, uint64_t *value)
{
    int r = 0;
    char *endp = (char *)s;
    unsigned long long val = 0;

    assert((unsigned) base <= 36 && base != 1);
    if (!s) {
        r = -EINVAL;
        goto out;
    }

    errno = 0;
    val = strtoull(s, &endp, base);
    if (errno) {
        r = -errno;
        goto out;
    }

    if (endp == s) {
        r = -EINVAL;
        goto out;
    }

    while (qemu_isspace(*s)) {
        s++;
    }
    if (*s == '-') {
        val = 0;
        r = -ERANGE;
        goto out;
    }

out:
    *value = val;
    if (endptr) {
        *endptr = endp;
    } else if (s && *endp) {
        r = -EINVAL;
        *value = 0;
    }
    return r;
}

int parse_uint_full(const char *s, int base, uint64_t *value)
{
    return parse_uint(s, NULL, base, value);
}

int qemu_parse_fd(const char *param)
{
    long fd;
    char *endptr;

    errno = 0;
    fd = strtol(param, &endptr, 10);
    if (param == endptr /* no conversion performed */                    ||
        errno != 0      /* not representable as long; possibly others */ ||
        *endptr != '\0' /* final string not empty */                     ||
        fd < 0          /* invalid as file descriptor */                 ||
        fd > INT_MAX    /* not representable as int */) {
        return -1;
    }
    return fd;
}

int uleb128_encode_small(uint8_t *out, uint32_t n)
{
    g_assert(n <= 0x3fff);
    if (n < 0x80) {
        *out = n;
        return 1;
    } else {
        *out++ = (n & 0x7f) | 0x80;
        *out = n >> 7;
        return 2;
    }
}

int uleb128_decode_small(const uint8_t *in, uint32_t *n)
{
    if (!(*in & 0x80)) {
        *n = *in;
        return 1;
    } else {
        *n = *in++ & 0x7f;
        if (*in & 0x80) {
            return -1;
        }
        *n |= *in << 7;
        return 2;
    }
}

int parse_debug_env(const char *name, int max, int initial)
{
    char *debug_env = getenv(name);
    char *inv = NULL;
    long debug;

    if (!debug_env) {
        return initial;
    }
    errno = 0;
    debug = strtol(debug_env, &inv, 10);
    if (inv == debug_env) {
        return initial;
    }
    if (debug < 0 || debug > max || errno != 0) {
        warn_report("%s not in [0, %d]", name, max);
        return initial;
    }
    return debug;
}

const char *si_prefix(unsigned int exp10)
{
    static const char *prefixes[] = {
        "a", "f", "p", "n", "u", "m", "", "K", "M", "G", "T", "P", "E"
    };

    exp10 += 18;
    assert(exp10 % 3 == 0 && exp10 / 3 < ARRAY_SIZE(prefixes));
    return prefixes[exp10 / 3];
}

const char *iec_binary_prefix(unsigned int exp2)
{
    static const char *prefixes[] = { "", "Ki", "Mi", "Gi", "Ti", "Pi", "Ei" };

    assert(exp2 % 10 == 0 && exp2 / 10 < ARRAY_SIZE(prefixes));
    return prefixes[exp2 / 10];
}

char *size_to_str(uint64_t val)
{
    uint64_t div;
    int i;

    frexp(val / (1000.0 / 1024.0), &i);
    i = (i - 1) / 10 * 10;
    div = 1ULL << i;

    return g_strdup_printf("%0.3g %sB", (double)val / div, iec_binary_prefix(i));
}

char *freq_to_str(uint64_t freq_hz)
{
    double freq = freq_hz;
    size_t exp10 = 0;

    while (freq >= 1000.0) {
        freq /= 1000.0;
        exp10 += 3;
    }

    return g_strdup_printf("%0.3g %sHz", freq, si_prefix(exp10));
}

int qemu_pstrcmp0(const char **str1, const char **str2)
{
    return g_strcmp0(*str1, *str2);
}

static inline bool starts_with_prefix(const char *dir)
{
    size_t prefix_len = strlen(CONFIG_PREFIX);
#pragma GCC diagnostic push
#if !defined(__clang__) || __has_warning("-Warray-bounds=")
#pragma GCC diagnostic ignored "-Warray-bounds="
#endif
    return !memcmp(dir, CONFIG_PREFIX, prefix_len) &&
        (!dir[prefix_len] || G_IS_DIR_SEPARATOR(dir[prefix_len]));
#pragma GCC diagnostic pop
}

static inline const char *next_component(const char *dir, int *p_len)
{
    int len;
    while ((*dir && G_IS_DIR_SEPARATOR(*dir)) ||
           (*dir == '.' && (G_IS_DIR_SEPARATOR(dir[1]) || dir[1] == '\0'))) {
        dir++;
    }
    len = 0;
    while (dir[len] && !G_IS_DIR_SEPARATOR(dir[len])) {
        len++;
    }
    *p_len = len;
    return dir;
}

static const char *exec_dir;

void qemu_init_exec_dir(const char *argv0)
{
#ifdef G_OS_WIN32
    char *p;
    char buf[MAX_PATH];
    DWORD len;

    if (exec_dir) {
        return;
    }

    len = GetModuleFileName(NULL, buf, sizeof(buf) - 1);
    if (len == 0) {
        return;
    }

    buf[len] = 0;
    p = buf + len - 1;
    while (p != buf && *p != '\\') {
        p--;
    }
    *p = 0;
    if (access(buf, R_OK) == 0) {
        exec_dir = g_strdup(buf);
    } else {
        exec_dir = CONFIG_BINDIR;
    }
#else
    char *p = NULL;
    char buf[PATH_MAX];

    if (exec_dir) {
        return;
    }

#if defined(__linux__)
    {
        int len;
        len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (len > 0) {
            buf[len] = 0;
            p = buf;
        }
    }
#elif defined(__FreeBSD__) \
      || (defined(__NetBSD__) && defined(KERN_PROC_PATHNAME))
    {
#if defined(__FreeBSD__)
        static int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
#else
        static int mib[4] = {CTL_KERN, KERN_PROC_ARGS, -1, KERN_PROC_PATHNAME};
#endif
        size_t len = sizeof(buf) - 1;

        *buf = '\0';
        if (!sysctl(mib, ARRAY_SIZE(mib), buf, &len, NULL, 0) &&
            *buf) {
            buf[sizeof(buf) - 1] = '\0';
            p = buf;
        }
    }
#elif defined(__APPLE__)
    {
        char fpath[PATH_MAX];
        uint32_t len = sizeof(fpath);
        if (_NSGetExecutablePath(fpath, &len) == 0) {
            p = realpath(fpath, buf);
            if (!p) {
                return;
            }
        }
    }
#elif defined(__HAIKU__)
    {
        image_info ii;
        int32_t c = 0;

        *buf = '\0';
        while (get_next_image_info(0, &c, &ii) == B_OK) {
            if (ii.type == B_APP_IMAGE) {
                strncpy(buf, ii.name, sizeof(buf));
                buf[sizeof(buf) - 1] = 0;
                p = buf;
                break;
            }
        }
    }
#endif
    if (!p && argv0) {
        p = realpath(argv0, buf);
    }
    if (p) {
        exec_dir = g_path_get_dirname(p);
    } else {
        exec_dir = CONFIG_BINDIR;
    }
#endif
}

char *get_relocated_path(const char *dir)
{
    size_t prefix_len = strlen(CONFIG_PREFIX);
    const char *bindir = CONFIG_BINDIR;
    GString *result;
    int len_dir, len_bindir;

    assert(exec_dir[0]);

    result = g_string_new(exec_dir);
    g_string_append(result, "/qemu-bundle");
    if (access(result->str, R_OK) == 0) {
#ifdef G_OS_WIN32
        const char *src = dir;
        size_t size = mbsrtowcs(NULL, &src, 0, &(mbstate_t){0}) + 1;
        PWSTR wdir = g_new(WCHAR, size);
        mbsrtowcs(wdir, &src, size, &(mbstate_t){0});

        PCWSTR wdir_skipped_root;
        if (PathCchSkipRoot(wdir, &wdir_skipped_root) == S_OK) {
            size = wcsrtombs(NULL, &wdir_skipped_root, 0, &(mbstate_t){0});
            char *cursor = result->str + result->len;
            g_string_set_size(result, result->len + size);
            wcsrtombs(cursor, &wdir_skipped_root, size + 1, &(mbstate_t){0});
        } else {
            g_string_append(result, dir);
        }

        g_free(wdir);
#else
        g_string_append(result, dir);
#endif
        goto out;
    }

    if (IS_ENABLED(CONFIG_RELOCATABLE) &&
        starts_with_prefix(dir) && starts_with_prefix(bindir)) {
        g_string_assign(result, exec_dir);

        len_dir = len_bindir = prefix_len;
        do {
            dir += len_dir;
            bindir += len_bindir;
            dir = next_component(dir, &len_dir);
            bindir = next_component(bindir, &len_bindir);
        } while (len_dir && len_dir == len_bindir && !memcmp(dir, bindir, len_dir));

        while (len_bindir) {
            bindir += len_bindir;
            g_string_append(result, "/..");
            bindir = next_component(bindir, &len_bindir);
        }

        if (*dir) {
            assert(G_IS_DIR_SEPARATOR(dir[-1]));
            g_string_append(result, dir - 1);
        }
        goto out;
    }

    g_string_assign(result, dir);
out:
    return g_string_free(result, false);
}
