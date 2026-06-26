
#include "qemu/osdep.h"
#include "qapi/error.h"

#include "ivgenpriv.h"
#include "ivgen-plain.h"
#include "ivgen-plain64.h"
#include "ivgen-essiv.h"


QCryptoIVGen *qcrypto_ivgen_new(QCryptoIVGenAlgo alg,
                                QCryptoCipherAlgo cipheralg,
                                QCryptoHashAlgo hash,
                                const uint8_t *key, size_t nkey,
                                Error **errp)
{
    QCryptoIVGen *ivgen = g_new0(QCryptoIVGen, 1);

    ivgen->algorithm = alg;
    ivgen->cipher = cipheralg;
    ivgen->hash = hash;

    switch (alg) {
    case QCRYPTO_IV_GEN_ALGO_PLAIN:
        ivgen->driver = &qcrypto_ivgen_plain;
        break;
    case QCRYPTO_IV_GEN_ALGO_PLAIN64:
        ivgen->driver = &qcrypto_ivgen_plain64;
        break;
    case QCRYPTO_IV_GEN_ALGO_ESSIV:
        ivgen->driver = &qcrypto_ivgen_essiv;
        break;
    default:
        error_setg(errp, "Unknown block IV generator algorithm %d", alg);
        g_free(ivgen);
        return NULL;
    }

    if (ivgen->driver->init(ivgen, key, nkey, errp) < 0) {
        g_free(ivgen);
        return NULL;
    }

    return ivgen;
}


int qcrypto_ivgen_calculate(QCryptoIVGen *ivgen,
                            uint64_t sector,
                            uint8_t *iv, size_t niv,
                            Error **errp)
{
    return ivgen->driver->calculate(ivgen, sector, iv, niv, errp);
}


QCryptoIVGenAlgo qcrypto_ivgen_get_algorithm(QCryptoIVGen *ivgen)
{
    return ivgen->algorithm;
}


QCryptoCipherAlgo qcrypto_ivgen_get_cipher(QCryptoIVGen *ivgen)
{
    return ivgen->cipher;
}


QCryptoHashAlgo qcrypto_ivgen_get_hash(QCryptoIVGen *ivgen)
{
    return ivgen->hash;
}


void qcrypto_ivgen_free(QCryptoIVGen *ivgen)
{
    if (!ivgen) {
        return;
    }
    ivgen->driver->cleanup(ivgen);
    g_free(ivgen);
}
