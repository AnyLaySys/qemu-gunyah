
#include "qemu/osdep.h"
#include "qemu/bswap.h"
#include "ivgen-plain.h"

static int qcrypto_ivgen_plain_init(QCryptoIVGen *ivgen,
                                    const uint8_t *key, size_t nkey,
                                    Error **errp)
{
    return 0;
}

static int qcrypto_ivgen_plain_calculate(QCryptoIVGen *ivgen,
                                         uint64_t sector,
                                         uint8_t *iv, size_t niv,
                                         Error **errp)
{
    size_t ivprefix;
    uint32_t shortsector = cpu_to_le32((sector & 0xffffffff));
    ivprefix = sizeof(shortsector);
    if (ivprefix > niv) {
        ivprefix = niv;
    }
    memcpy(iv, &shortsector, ivprefix);
    if (ivprefix < niv) {
        memset(iv + ivprefix, 0, niv - ivprefix);
    }
    return 0;
}

static void qcrypto_ivgen_plain_cleanup(QCryptoIVGen *ivgen)
{
}


struct QCryptoIVGenDriver qcrypto_ivgen_plain = {
    .init = qcrypto_ivgen_plain_init,
    .calculate = qcrypto_ivgen_plain_calculate,
    .cleanup = qcrypto_ivgen_plain_cleanup,
};

