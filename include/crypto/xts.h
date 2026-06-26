
#ifndef QCRYPTO_XTS_H
#define QCRYPTO_XTS_H


#define XTS_BLOCK_SIZE 16

typedef void xts_cipher_func(const void *ctx,
                             size_t length,
                             uint8_t *dst,
                             const uint8_t *src);

void xts_decrypt(const void *datactx,
                 const void *tweakctx,
                 xts_cipher_func *encfunc,
                 xts_cipher_func *decfunc,
                 uint8_t *iv,
                 size_t length,
                 uint8_t *dst,
                 const uint8_t *src);

void xts_encrypt(const void *datactx,
                 const void *tweakctx,
                 xts_cipher_func *encfunc,
                 xts_cipher_func *decfunc,
                 uint8_t *iv,
                 size_t length,
                 uint8_t *dst,
                 const uint8_t *src);


#endif /* QCRYPTO_XTS_H */
