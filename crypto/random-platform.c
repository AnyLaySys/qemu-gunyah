

#include "qemu/osdep.h"

#include "crypto/random.h"
#include "qapi/error.h"

#ifdef _WIN32
#include <wincrypt.h>
static HCRYPTPROV hCryptProv;
#else
# ifdef CONFIG_GETRANDOM
#  include <sys/random.h>
# endif

static int fd;
#endif

int qcrypto_random_init(Error **errp)
{
#ifdef _WIN32
    if (!CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL,
                             CRYPT_SILENT | CRYPT_VERIFYCONTEXT)) {
        error_setg_win32(errp, GetLastError(),
                         "Unable to create cryptographic provider");
        return -1;
    }
#else
# ifdef CONFIG_GETRANDOM
    if (getrandom(&(char){0}, 0, 0) == 0) {
        
        fd = -1;
        return 0;
    }
    
# endif
    fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd == -1 && errno == ENOENT) {
        fd = open("/dev/random", O_RDONLY | O_CLOEXEC);
    }
    if (fd < 0) {
        error_setg_errno(errp, errno, "No /dev/urandom or /dev/random");
        return -1;
    }
#endif
    return 0;
}

int qcrypto_random_bytes(void *buf,
                         size_t buflen,
                         Error **errp)
{
#ifdef _WIN32
    if (!CryptGenRandom(hCryptProv, buflen, buf)) {
        error_setg_win32(errp, GetLastError(),
                         "Unable to read random bytes");
        return -1;
    }
#else
# ifdef CONFIG_GETRANDOM
    if (likely(fd < 0)) {
        while (1) {
            ssize_t got = getrandom(buf, buflen, 0);
            if (likely(got == buflen)) {
                return 0;
            }
            if (got >= 0) {
                buflen -= got;
                buf += got;
            } else if (errno != EINTR) {
                error_setg_errno(errp, errno, "getrandom");
                return -1;
            }
        }
    }
    
# endif
    while (1) {
        ssize_t got = read(fd, buf, buflen);
        if (likely(got == buflen)) {
            return 0;
        }
        if (got > 0) {
            buflen -= got;
            buf += got;
        } else if (got == 0) {
            error_setg(errp, "Unexpected EOF reading random bytes");
            return -1;
        } else if (errno != EINTR) {
            error_setg_errno(errp, errno, "Unable to read random bytes");
            return -1;
        }
    }
#endif
    return 0;
}
