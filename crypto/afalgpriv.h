
#ifndef QCRYPTO_AFALGPRIV_H
#define QCRYPTO_AFALGPRIV_H

#include <linux/if_alg.h>
#include "crypto/cipher.h"

#define SALG_TYPE_LEN_MAX 14
#define SALG_NAME_LEN_MAX 64

#ifndef SOL_ALG
#define SOL_ALG 279
#endif

#define AFALG_TYPE_CIPHER "skcipher"
#define AFALG_TYPE_HASH "hash"

#define ALG_OPTYPE_LEN 4
#define ALG_MSGIV_LEN(len) (sizeof(struct af_alg_iv) + (len))

typedef struct QCryptoAFAlgo QCryptoAFAlgo;

struct QCryptoAFAlgo {
    QCryptoCipher base;

    int tfmfd;
    int opfd;
    struct msghdr *msg;
    struct cmsghdr *cmsg;
};

QCryptoAFAlgo *
qcrypto_afalg_comm_alloc(const char *type, const char *name,
                         Error **errp);

void qcrypto_afalg_comm_free(QCryptoAFAlgo *afalg);

#endif
