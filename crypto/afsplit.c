
#include "qemu/osdep.h"
#include "qemu/bswap.h"
#include "crypto/afsplit.h"
#include "crypto/random.h"


static void qcrypto_afsplit_xor(size_t blocklen,
                                const uint8_t *in1,
                                const uint8_t *in2,
                                uint8_t *out)
{
    size_t i;
    for (i = 0; i < blocklen; i++) {
        out[i] = in1[i] ^ in2[i];
    }
}


static int qcrypto_afsplit_hash(QCryptoHashAlgo hash,
                                size_t blocklen,
                                uint8_t *block,
                                Error **errp)
{
    size_t digestlen = qcrypto_hash_digest_len(hash);

    size_t hashcount = blocklen / digestlen;
    size_t finallen = blocklen % digestlen;
    uint32_t i;

    if (finallen) {
        hashcount++;
    } else {
        finallen = digestlen;
    }

    for (i = 0; i < hashcount; i++) {
        g_autofree uint8_t *out = NULL;
        size_t outlen = 0;
        uint32_t iv = cpu_to_be32(i);
        struct iovec in[] = {
            { .iov_base = &iv,
              .iov_len = sizeof(iv) },
            { .iov_base = block + (i * digestlen),
              .iov_len = (i == (hashcount - 1)) ? finallen : digestlen },
        };

        if (qcrypto_hash_bytesv(hash,
                                in,
                                G_N_ELEMENTS(in),
                                &out, &outlen,
                                errp) < 0) {
            return -1;
        }

        assert(outlen == digestlen);
        memcpy(block + (i * digestlen), out,
               (i == (hashcount - 1)) ? finallen : digestlen);
    }

    return 0;
}


int qcrypto_afsplit_encode(QCryptoHashAlgo hash,
                           size_t blocklen,
                           uint32_t stripes,
                           const uint8_t *in,
                           uint8_t *out,
                           Error **errp)
{
    g_autofree uint8_t *block = g_new0(uint8_t, blocklen);
    size_t i;

    for (i = 0; i < (stripes - 1); i++) {
        if (qcrypto_random_bytes(out + (i * blocklen), blocklen, errp) < 0) {
            return -1;
        }

        qcrypto_afsplit_xor(blocklen,
                            out + (i * blocklen),
                            block,
                            block);
        if (qcrypto_afsplit_hash(hash, blocklen, block,
                                 errp) < 0) {
            return -1;
        }
    }
    qcrypto_afsplit_xor(blocklen,
                        in,
                        block,
                        out + (i * blocklen));
    return 0;
}


int qcrypto_afsplit_decode(QCryptoHashAlgo hash,
                           size_t blocklen,
                           uint32_t stripes,
                           const uint8_t *in,
                           uint8_t *out,
                           Error **errp)
{
    g_autofree uint8_t *block = g_new0(uint8_t, blocklen);
    size_t i;

    for (i = 0; i < (stripes - 1); i++) {
        qcrypto_afsplit_xor(blocklen,
                            in + (i * blocklen),
                            block,
                            block);
        if (qcrypto_afsplit_hash(hash, blocklen, block,
                                 errp) < 0) {
            return -1;
        }
    }

    qcrypto_afsplit_xor(blocklen,
                        in + (i * blocklen),
                        block,
                        out);
    return 0;
}
