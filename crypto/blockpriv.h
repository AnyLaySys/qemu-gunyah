
#ifndef QCRYPTO_BLOCKPRIV_H
#define QCRYPTO_BLOCKPRIV_H

#include "crypto/block.h"
#include "qemu/thread.h"

typedef struct QCryptoBlockDriver QCryptoBlockDriver;

struct QCryptoBlock {
    QCryptoBlockFormat format;

    const QCryptoBlockDriver *driver;
    void *opaque;

    QCryptoCipherAlgo alg;
    QCryptoCipherMode mode;
    uint8_t *key;
    size_t nkey;

    QCryptoCipher **free_ciphers;
    size_t max_free_ciphers;
    size_t n_free_ciphers;
    QCryptoIVGen *ivgen;
    QemuMutex mutex;

    QCryptoHashAlgo kdfhash;
    size_t niv;
    uint64_t payload_offset; /* In bytes */
    uint64_t sector_size; /* In bytes */

    bool detached_header; /* True if disk has a detached LUKS header */
};

struct QCryptoBlockDriver {
    int (*open)(QCryptoBlock *block,
                QCryptoBlockOpenOptions *options,
                const char *optprefix,
                QCryptoBlockReadFunc readfunc,
                void *opaque,
                unsigned int flags,
                Error **errp);

    int (*create)(QCryptoBlock *block,
                  QCryptoBlockCreateOptions *options,
                  const char *optprefix,
                  QCryptoBlockInitFunc initfunc,
                  QCryptoBlockWriteFunc writefunc,
                  void *opaque,
                  Error **errp);

    int (*amend)(QCryptoBlock *block,
                 QCryptoBlockReadFunc readfunc,
                 QCryptoBlockWriteFunc writefunc,
                 void *opaque,
                 QCryptoBlockAmendOptions *options,
                 bool force,
                 Error **errp);

    int (*get_info)(QCryptoBlock *block,
                    QCryptoBlockInfo *info,
                    Error **errp);

    void (*cleanup)(QCryptoBlock *block);

    int (*encrypt)(QCryptoBlock *block,
                   uint64_t startsector,
                   uint8_t *buf,
                   size_t len,
                   Error **errp);
    int (*decrypt)(QCryptoBlock *block,
                   uint64_t startsector,
                   uint8_t *buf,
                   size_t len,
                   Error **errp);

    bool (*has_format)(const uint8_t *buf,
                       size_t buflen);
};


int qcrypto_block_cipher_decrypt_helper(QCryptoCipher *cipher,
                                        size_t niv,
                                        QCryptoIVGen *ivgen,
                                        int sectorsize,
                                        uint64_t offset,
                                        uint8_t *buf,
                                        size_t len,
                                        Error **errp);

int qcrypto_block_cipher_encrypt_helper(QCryptoCipher *cipher,
                                        size_t niv,
                                        QCryptoIVGen *ivgen,
                                        int sectorsize,
                                        uint64_t offset,
                                        uint8_t *buf,
                                        size_t len,
                                        Error **errp);

int qcrypto_block_decrypt_helper(QCryptoBlock *block,
                                 int sectorsize,
                                 uint64_t offset,
                                 uint8_t *buf,
                                 size_t len,
                                 Error **errp);

int qcrypto_block_encrypt_helper(QCryptoBlock *block,
                                 int sectorsize,
                                 uint64_t offset,
                                 uint8_t *buf,
                                 size_t len,
                                 Error **errp);

int qcrypto_block_init_cipher(QCryptoBlock *block,
                              QCryptoCipherAlgo alg,
                              QCryptoCipherMode mode,
                              const uint8_t *key, size_t nkey,
                              Error **errp);

void qcrypto_block_free_cipher(QCryptoBlock *block);

#endif /* QCRYPTO_BLOCKPRIV_H */
