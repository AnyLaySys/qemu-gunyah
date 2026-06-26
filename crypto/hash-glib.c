
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "crypto/hash.h"
#include "hashpriv.h"


static int qcrypto_hash_alg_map[QCRYPTO_HASH_ALGO__MAX] = {
    [QCRYPTO_HASH_ALGO_MD5] = G_CHECKSUM_MD5,
    [QCRYPTO_HASH_ALGO_SHA1] = G_CHECKSUM_SHA1,
    [QCRYPTO_HASH_ALGO_SHA224] = -1,
    [QCRYPTO_HASH_ALGO_SHA256] = G_CHECKSUM_SHA256,
    [QCRYPTO_HASH_ALGO_SHA384] = G_CHECKSUM_SHA384,
    [QCRYPTO_HASH_ALGO_SHA512] = G_CHECKSUM_SHA512,
    [QCRYPTO_HASH_ALGO_RIPEMD160] = -1,
};

gboolean qcrypto_hash_supports(QCryptoHashAlgo alg)
{
    if (alg < G_N_ELEMENTS(qcrypto_hash_alg_map) &&
        qcrypto_hash_alg_map[alg] != -1) {
        return true;
    }
    return false;
}

static
QCryptoHash *qcrypto_glib_hash_new(QCryptoHashAlgo alg,
                                   Error **errp)
{
    QCryptoHash *hash;

    hash = g_new(QCryptoHash, 1);
    hash->alg = alg;
    hash->opaque = g_checksum_new(qcrypto_hash_alg_map[alg]);

    return hash;
}

static
void qcrypto_glib_hash_free(QCryptoHash *hash)
{
    if (hash->opaque) {
        g_checksum_free(hash->opaque);
    }

    g_free(hash);
}


static
int qcrypto_glib_hash_update(QCryptoHash *hash,
                             const struct iovec *iov,
                             size_t niov,
                             Error **errp)
{
    GChecksum *ctx = hash->opaque;

    for (int i = 0; i < niov; i++) {
        g_checksum_update(ctx, iov[i].iov_base, iov[i].iov_len);
    }

    return 0;
}

static
int qcrypto_glib_hash_finalize(QCryptoHash *hash,
                               uint8_t **result,
                               size_t *result_len,
                               Error **errp)
{
    int ret;
    GChecksum *ctx = hash->opaque;

    ret = g_checksum_type_get_length(qcrypto_hash_alg_map[hash->alg]);
    if (ret < 0) {
        error_setg(errp, "Unable to get hash length");
        *result_len = 0;
        return -1;
    }

    if (*result_len == 0) {
        *result_len = ret;
        *result = g_new(uint8_t, *result_len);
    } else if (*result_len != ret) {
        error_setg(errp,
                   "Result buffer size %zu is smaller than hash %d",
                   *result_len, ret);
        return -1;
    }

    g_checksum_get_digest(ctx, *result, result_len);
    return 0;
}

QCryptoHashDriver qcrypto_hash_lib_driver = {
    .hash_new      = qcrypto_glib_hash_new,
    .hash_update   = qcrypto_glib_hash_update,
    .hash_finalize = qcrypto_glib_hash_finalize,
    .hash_free     = qcrypto_glib_hash_free,
};
