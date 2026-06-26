
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/base64.h"

static const char *base64_valid_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=\n";

uint8_t *qbase64_decode(const char *input,
                        size_t in_len,
                        size_t *out_len,
                        Error **errp)
{
    *out_len = 0;

    if (in_len != -1) {
        if (input[in_len] != '\0') {
            error_setg(errp, "Base64 data is not NUL terminated");
            return NULL;
        }
        if (memchr(input, '\0', in_len) != NULL) {
            error_setg(errp, "Base64 data contains embedded NUL characters");
            return NULL;
        }

    } else {
        in_len = strlen(input);
    }

    if (strspn(input, base64_valid_chars) != in_len) {
        error_setg(errp, "Base64 data contains invalid characters");
        return NULL;
    }

    return g_base64_decode(input, out_len);
}
