
#include <nettle/asn1.h>

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "rsakey.h"

static bool DumpMPI(struct asn1_der_iterator *i, QCryptoAkCipherMPI *mpi)
{
    mpi->data = g_memdup2(i->data, i->length);
    mpi->len = i->length;
    return true;
}

static bool GetMPI(struct asn1_der_iterator *i, QCryptoAkCipherMPI *mpi)
{
    if (asn1_der_iterator_next(i) != ASN1_ITERATOR_PRIMITIVE ||
        i->type != ASN1_INTEGER) {
        return false;
    }
    return DumpMPI(i, mpi);
}

static QCryptoAkCipherRSAKey *qcrypto_nettle_rsa_private_key_parse(
    const uint8_t *key, size_t keylen, Error **errp)
{
    QCryptoAkCipherRSAKey *rsa = g_new0(QCryptoAkCipherRSAKey, 1);
    struct asn1_der_iterator i;
    uint32_t version;
    int tag;

    if (asn1_der_iterator_first(&i, keylen, key) != ASN1_ITERATOR_CONSTRUCTED ||
        i.type != ASN1_SEQUENCE ||
        asn1_der_decode_constructed_last(&i) != ASN1_ITERATOR_PRIMITIVE ||
        i.type != ASN1_INTEGER ||
        !asn1_der_get_uint32(&i, &version) ||
        version > 1 ||
        !GetMPI(&i, &rsa->n) ||
        !GetMPI(&i, &rsa->e) ||
        !GetMPI(&i, &rsa->d) ||
        !GetMPI(&i, &rsa->p) ||
        !GetMPI(&i, &rsa->q) ||
        !GetMPI(&i, &rsa->dp) ||
        !GetMPI(&i, &rsa->dq) ||
        !GetMPI(&i, &rsa->u)) {
        goto error;
    }

    if (version == 1) {
        tag = asn1_der_iterator_next(&i);
        if (tag == ASN1_ITERATOR_END) {
            return rsa;
        }
        if (tag != ASN1_ITERATOR_CONSTRUCTED ||
            i.type != ASN1_SEQUENCE) {
            goto error;
        }
    }

    if (asn1_der_iterator_next(&i) != ASN1_ITERATOR_END) {
        goto error;
    }

    return rsa;

error:
    error_setg(errp, "Failed to parse RSA private key");
    qcrypto_akcipher_rsakey_free(rsa);
    return NULL;
}

static QCryptoAkCipherRSAKey *qcrypto_nettle_rsa_public_key_parse(
    const uint8_t *key, size_t keylen, Error **errp)
{

    QCryptoAkCipherRSAKey *rsa = g_new0(QCryptoAkCipherRSAKey, 1);
    struct asn1_der_iterator i;

    if (asn1_der_iterator_first(&i, keylen, key) != ASN1_ITERATOR_CONSTRUCTED ||
        i.type != ASN1_SEQUENCE ||
        asn1_der_decode_constructed_last(&i) != ASN1_ITERATOR_PRIMITIVE ||
        !DumpMPI(&i, &rsa->n) ||
        !GetMPI(&i, &rsa->e) ||
        asn1_der_iterator_next(&i) != ASN1_ITERATOR_END) {
        goto error;
    }

    return rsa;

error:
    error_setg(errp, "Failed to parse RSA public key");
    qcrypto_akcipher_rsakey_free(rsa);
    return NULL;
}

QCryptoAkCipherRSAKey *qcrypto_akcipher_rsakey_parse(
    QCryptoAkCipherKeyType type, const uint8_t *key,
    size_t keylen, Error **errp)
{
    switch (type) {
    case QCRYPTO_AK_CIPHER_KEY_TYPE_PRIVATE:
        return qcrypto_nettle_rsa_private_key_parse(key, keylen, errp);

    case QCRYPTO_AK_CIPHER_KEY_TYPE_PUBLIC:
        return qcrypto_nettle_rsa_public_key_parse(key, keylen, errp);

    default:
        error_setg(errp, "Unknown key type: %d", type);
        return NULL;
    }
}
