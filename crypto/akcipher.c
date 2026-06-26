
#include "qemu/osdep.h"
#include "crypto/akcipher.h"
#include "akcipherpriv.h"
#include "der.h"
#include "rsakey.h"

#if defined(CONFIG_GCRYPT)
#include "akcipher-gcrypt.c.inc"
#elif defined(CONFIG_NETTLE) && defined(CONFIG_HOGWEED)
#include "akcipher-nettle.c.inc"
#else
QCryptoAkCipher *qcrypto_akcipher_new(const QCryptoAkCipherOptions *opts,
                                      QCryptoAkCipherKeyType type,
                                      const uint8_t *key, size_t keylen,
                                      Error **errp)
{
    QCryptoAkCipher *akcipher = NULL;

    return akcipher;
}

bool qcrypto_akcipher_supports(QCryptoAkCipherOptions *opts)
{
    return false;
}
#endif

int qcrypto_akcipher_encrypt(QCryptoAkCipher *akcipher,
                             const void *in, size_t in_len,
                             void *out, size_t out_len, Error **errp)
{
    const QCryptoAkCipherDriver *drv = akcipher->driver;

    return drv->encrypt(akcipher, in, in_len, out, out_len, errp);
}

int qcrypto_akcipher_decrypt(QCryptoAkCipher *akcipher,
                             const void *in, size_t in_len,
                             void *out, size_t out_len, Error **errp)
{
    const QCryptoAkCipherDriver *drv = akcipher->driver;

    return drv->decrypt(akcipher, in, in_len, out, out_len, errp);
}

int qcrypto_akcipher_sign(QCryptoAkCipher *akcipher,
                          const void *in, size_t in_len,
                          void *out, size_t out_len, Error **errp)
{
    const QCryptoAkCipherDriver *drv = akcipher->driver;

    return drv->sign(akcipher, in, in_len, out, out_len, errp);
}

int qcrypto_akcipher_verify(QCryptoAkCipher *akcipher,
                            const void *in, size_t in_len,
                            const void *in2, size_t in2_len, Error **errp)
{
    const QCryptoAkCipherDriver *drv = akcipher->driver;

    return drv->verify(akcipher, in, in_len, in2, in2_len, errp);
}

int qcrypto_akcipher_max_plaintext_len(QCryptoAkCipher *akcipher)
{
    return akcipher->max_plaintext_len;
}

int qcrypto_akcipher_max_ciphertext_len(QCryptoAkCipher *akcipher)
{
    return akcipher->max_ciphertext_len;
}

int qcrypto_akcipher_max_signature_len(QCryptoAkCipher *akcipher)
{
    return akcipher->max_signature_len;
}

int qcrypto_akcipher_max_dgst_len(QCryptoAkCipher *akcipher)
{
    return akcipher->max_dgst_len;
}

void qcrypto_akcipher_free(QCryptoAkCipher *akcipher)
{
    const QCryptoAkCipherDriver *drv = akcipher->driver;

    drv->free(akcipher);
}

int qcrypto_akcipher_export_p8info(const QCryptoAkCipherOptions *opts,
                                   uint8_t *key, size_t keylen,
                                   uint8_t **dst, size_t *dst_len,
                                   Error **errp)
{
    switch (opts->alg) {
    case QCRYPTO_AK_CIPHER_ALGO_RSA:
        qcrypto_akcipher_rsakey_export_p8info(key, keylen, dst, dst_len);
        return 0;

    default:
        error_setg(errp, "Unsupported algorithm: %u", opts->alg);
        return -1;
    }
}
