#ifndef QEMU_AES_H
#define QEMU_AES_H

#define AES_MAXNR 14
#define AES_BLOCK_SIZE 16

struct aes_key_st {
    uint32_t rd_key[4 *(AES_MAXNR + 1)];
    int rounds;
};
typedef struct aes_key_st AES_KEY;

#define AES_set_encrypt_key QEMU_AES_set_encrypt_key
#define AES_set_decrypt_key QEMU_AES_set_decrypt_key
#define AES_encrypt QEMU_AES_encrypt
#define AES_decrypt QEMU_AES_decrypt

int AES_set_encrypt_key(const unsigned char *userKey, const int bits,
                        AES_KEY *key);
int AES_set_decrypt_key(const unsigned char *userKey, const int bits,
                        AES_KEY *key);

void AES_encrypt(const unsigned char *in, unsigned char *out,
                 const AES_KEY *key);
void AES_decrypt(const unsigned char *in, unsigned char *out,
                 const AES_KEY *key);

extern const uint8_t AES_sbox[256];
extern const uint8_t AES_isbox[256];


extern const uint32_t AES_Te0[256], AES_Td0[256];

#endif
