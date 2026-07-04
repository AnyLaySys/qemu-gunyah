
#ifndef SYSTEM_TPM_UTIL_H
#define SYSTEM_TPM_UTIL_H

#include "system/tpm.h"
#include "qemu/bswap.h"

void tpm_util_write_fatal_error_response(uint8_t *out, uint32_t out_len);

bool tpm_util_is_selftest(const uint8_t *in, uint32_t in_len);

int tpm_util_test_tpmdev(int tpm_fd, TPMVersion *tpm_version);

static inline uint16_t tpm_cmd_get_tag(const void *b)
{
    return lduw_be_p(b);
}

static inline void tpm_cmd_set_tag(void *b, uint16_t tag)
{
    stw_be_p(b, tag);
}

static inline uint32_t tpm_cmd_get_size(const void *b)
{
    return ldl_be_p(b + 2);
}

static inline void tpm_cmd_set_size(void *b, uint32_t size)
{
    stl_be_p(b + 2, size);
}

static inline uint32_t tpm_cmd_get_ordinal(const void *b)
{
    return ldl_be_p(b + 6);
}

static inline uint32_t tpm_cmd_get_errcode(const void *b)
{
    return ldl_be_p(b + 6);
}

static inline void tpm_cmd_set_error(void *b, uint32_t error)
{
    stl_be_p(b + 6, error);
}

void tpm_util_show_buffer(const unsigned char *buffer,
                          size_t buffer_size, const char *string);

#endif /* SYSTEM_TPM_UTIL_H */
