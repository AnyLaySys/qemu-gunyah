#ifndef QEMU_CUTILS_H
#define QEMU_CUTILS_H

const char *si_prefix(unsigned int exp10);

const char *iec_binary_prefix(unsigned int exp2);

void pstrcpy(char *buf, int buf_size, const char *str);
void strpadcpy(char *buf, int buf_size, const char *str, char pad);
char *pstrcat(char *buf, int buf_size, const char *s);
int strstart(const char *str, const char *val, const char **ptr);
int stristart(const char *str, const char *val, const char **ptr);
int qemu_strnlen(const char *s, int max_len);
char *qemu_strsep(char **input, const char *delim);
#ifdef HAVE_STRCHRNUL
static inline const char *qemu_strchrnul(const char *s, int c)
{
    return strchrnul(s, c);
}
#else
const char *qemu_strchrnul(const char *s, int c);
#endif
time_t mktimegm(struct tm *tm);
int qemu_parse_fd(const char *param);
int qemu_strtoi(const char *nptr, const char **endptr, int base,
                int *result);
int qemu_strtoui(const char *nptr, const char **endptr, int base,
                 unsigned int *result);
int qemu_strtol(const char *nptr, const char **endptr, int base,
                long *result);
int qemu_strtoul(const char *nptr, const char **endptr, int base,
                 unsigned long *result);
int qemu_strtoi64(const char *nptr, const char **endptr, int base,
                  int64_t *result);
int qemu_strtou64(const char *nptr, const char **endptr, int base,
                  uint64_t *result);
int qemu_strtod(const char *nptr, const char **endptr, double *result);
int qemu_strtod_finite(const char *nptr, const char **endptr, double *result);

int parse_uint(const char *s, const char **endptr, int base, uint64_t *value);
int parse_uint_full(const char *s, int base, uint64_t *value);

int qemu_strtosz(const char *nptr, const char **end, uint64_t *result);
int qemu_strtosz_MiB(const char *nptr, const char **end, uint64_t *result);
int qemu_strtosz_metric(const char *nptr, const char **end, uint64_t *result);

char *size_to_str(uint64_t val);

char *freq_to_str(uint64_t freq_hz);

#define STR_OR_NULL(str) ((str) ? (str) : "null")


bool buffer_is_zero_ool(const void *vbuf, size_t len);
bool buffer_is_zero_ge256(const void *vbuf, size_t len);
bool test_buffer_is_zero_next_accel(void);

static inline bool buffer_is_zero_sample3(const char *buf, size_t len)
{
    return !buf[0] && !buf[len - 1] && !buf[len / 2];
}

#ifdef __OPTIMIZE__
static inline bool buffer_is_zero(const void *buf, size_t len)
{
    return (__builtin_constant_p(len) && len >= 256
            ? buffer_is_zero_sample3(buf, len) &&
              buffer_is_zero_ge256(buf, len)
            : buffer_is_zero_ool(buf, len));
}
#else
#define buffer_is_zero  buffer_is_zero_ool
#endif


int uleb128_encode_small(uint8_t *out, uint32_t n);
int uleb128_decode_small(const uint8_t *in, uint32_t *n);

int qemu_pstrcmp0(const char **str1, const char **str2);

void qemu_init_exec_dir(const char *argv0);

char *get_relocated_path(const char *dir);

static inline const char *yes_no(bool b)
{
     return b ? "yes" : "no";
}

int parse_debug_env(const char *name, int max, int initial);

GString *qemu_hexdump_line(GString *str, const void *buf, size_t len,
                           size_t unit_len, size_t block_len);


void qemu_hexdump(FILE *fp, const char *prefix,
                  const void *bufptr, size_t size);

void qemu_hexdump_to_buffer(char *restrict buffer, size_t buffer_size,
                            const uint8_t *restrict data, size_t data_size);

#endif
