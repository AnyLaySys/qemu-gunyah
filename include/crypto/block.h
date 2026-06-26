
#ifndef QCRYPTO_BLOCK_H
#define QCRYPTO_BLOCK_H

#include "crypto/cipher.h"
#include "crypto/ivgen.h"

typedef struct QCryptoBlock QCryptoBlock;


typedef int (*QCryptoBlockReadFunc)(QCryptoBlock *block,
                                    size_t offset,
                                    uint8_t *buf,
                                    size_t buflen,
                                    void *opaque,
                                    Error **errp);

typedef int (*QCryptoBlockInitFunc)(QCryptoBlock *block,
                                    size_t headerlen,
                                    void *opaque,
                                    Error **errp);

typedef int (*QCryptoBlockWriteFunc)(QCryptoBlock *block,
                                     size_t offset,
                                     const uint8_t *buf,
                                     size_t buflen,
                                     void *opaque,
                                     Error **errp);

bool qcrypto_block_has_format(QCryptoBlockFormat format,
                              const uint8_t *buf,
                              size_t buflen);

typedef enum {
    QCRYPTO_BLOCK_OPEN_NO_IO = (1 << 0),
    QCRYPTO_BLOCK_OPEN_DETACHED = (1 << 1),
} QCryptoBlockOpenFlags;

QCryptoBlock *qcrypto_block_open(QCryptoBlockOpenOptions *options,
                                 const char *optprefix,
                                 QCryptoBlockReadFunc readfunc,
                                 void *opaque,
                                 unsigned int flags,
                                 Error **errp);

typedef enum {
    QCRYPTO_BLOCK_CREATE_DETACHED = (1 << 0),
} QCryptoBlockCreateFlags;

QCryptoBlock *qcrypto_block_create(QCryptoBlockCreateOptions *options,
                                   const char *optprefix,
                                   QCryptoBlockInitFunc initfunc,
                                   QCryptoBlockWriteFunc writefunc,
                                   void *opaque,
                                   unsigned int flags,
                                   Error **errp);

int qcrypto_block_amend_options(QCryptoBlock *block,
                                QCryptoBlockReadFunc readfunc,
                                QCryptoBlockWriteFunc writefunc,
                                void *opaque,
                                QCryptoBlockAmendOptions *options,
                                bool force,
                                Error **errp);


bool
qcrypto_block_calculate_payload_offset(QCryptoBlockCreateOptions *create_opts,
                                       const char *optprefix,
                                       size_t *len,
                                       Error **errp);


QCryptoBlockInfo *qcrypto_block_get_info(QCryptoBlock *block,
                                         Error **errp);

int qcrypto_block_decrypt(QCryptoBlock *block,
                          uint64_t offset,
                          uint8_t *buf,
                          size_t len,
                          Error **errp);

int qcrypto_block_encrypt(QCryptoBlock *block,
                          uint64_t offset,
                          uint8_t *buf,
                          size_t len,
                          Error **errp);

QCryptoCipher *qcrypto_block_get_cipher(QCryptoBlock *block);

QCryptoIVGen *qcrypto_block_get_ivgen(QCryptoBlock *block);


QCryptoHashAlgo qcrypto_block_get_kdf_hash(QCryptoBlock *block);

uint64_t qcrypto_block_get_payload_offset(QCryptoBlock *block);

uint64_t qcrypto_block_get_sector_size(QCryptoBlock *block);

void qcrypto_block_free(QCryptoBlock *block);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(QCryptoBlock, qcrypto_block_free)

#endif /* QCRYPTO_BLOCK_H */
