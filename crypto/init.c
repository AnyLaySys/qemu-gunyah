
#include "qemu/osdep.h"
#include "crypto/init.h"
#include "qapi/error.h"
#include "qemu/thread.h"

#ifdef CONFIG_GNUTLS
#include <gnutls/gnutls.h>
#include <gnutls/crypto.h>
#endif

#ifdef CONFIG_GCRYPT
#include <gcrypt.h>
#endif

#include "crypto/random.h"


int qcrypto_init(Error **errp)
{
#ifdef CONFIG_GNUTLS
    int ret;
    ret = gnutls_global_init();
    if (ret < 0) {
        error_setg(errp,
                   "Unable to initialize GNUTLS library: %s",
                   gnutls_strerror(ret));
        return -1;
    }
#endif

#ifdef CONFIG_GCRYPT
    if (!gcry_check_version(NULL)) {
        error_setg(errp, "Unable to initialize gcrypt");
        return -1;
    }
    gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);
#endif

    if (qcrypto_random_init(errp) < 0) {
        return -1;
    }

    return 0;
}
