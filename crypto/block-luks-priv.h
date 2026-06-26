
#include "qapi/error.h"
#include "qemu/bswap.h"

#include "block-luks.h"

#include "crypto/hash.h"
#include "crypto/afsplit.h"
#include "crypto/pbkdf.h"
#include "crypto/secret.h"
#include "crypto/random.h"
#include "qemu/uuid.h"

#include "qemu/bitmap.h"


typedef struct QCryptoBlockLUKSHeader QCryptoBlockLUKSHeader;
typedef struct QCryptoBlockLUKSKeySlot QCryptoBlockLUKSKeySlot;


#define QCRYPTO_BLOCK_LUKS_VERSION 1

#define QCRYPTO_BLOCK_LUKS_MAGIC_LEN 6
#define QCRYPTO_BLOCK_LUKS_CIPHER_NAME_LEN 32
#define QCRYPTO_BLOCK_LUKS_CIPHER_MODE_LEN 32
#define QCRYPTO_BLOCK_LUKS_HASH_SPEC_LEN 32
#define QCRYPTO_BLOCK_LUKS_DIGEST_LEN 20
#define QCRYPTO_BLOCK_LUKS_SALT_LEN 32
#define QCRYPTO_BLOCK_LUKS_UUID_LEN 40
#define QCRYPTO_BLOCK_LUKS_NUM_KEY_SLOTS 8
#define QCRYPTO_BLOCK_LUKS_STRIPES 4000
#define QCRYPTO_BLOCK_LUKS_MIN_SLOT_KEY_ITERS 1000
#define QCRYPTO_BLOCK_LUKS_MIN_MASTER_KEY_ITERS 1000
#define QCRYPTO_BLOCK_LUKS_KEY_SLOT_OFFSET 4096

#define QCRYPTO_BLOCK_LUKS_KEY_SLOT_DISABLED 0x0000DEAD
#define QCRYPTO_BLOCK_LUKS_KEY_SLOT_ENABLED 0x00AC71F3

#define QCRYPTO_BLOCK_LUKS_SECTOR_SIZE 512LL

#define QCRYPTO_BLOCK_LUKS_DEFAULT_ITER_TIME_MS 2000
#define QCRYPTO_BLOCK_LUKS_ERASE_ITERATIONS 40

static const char qcrypto_block_luks_magic[QCRYPTO_BLOCK_LUKS_MAGIC_LEN] = {
    'L', 'U', 'K', 'S', 0xBA, 0xBE
};

struct QCryptoBlockLUKSKeySlot {
    uint32_t active;
    uint32_t iterations;
    uint8_t salt[QCRYPTO_BLOCK_LUKS_SALT_LEN];
    uint32_t key_offset_sector;
    uint32_t stripes;
};

struct QCryptoBlockLUKSHeader {
    char magic[QCRYPTO_BLOCK_LUKS_MAGIC_LEN];

    uint16_t version;

    char cipher_name[QCRYPTO_BLOCK_LUKS_CIPHER_NAME_LEN];

    char cipher_mode[QCRYPTO_BLOCK_LUKS_CIPHER_MODE_LEN];

    char hash_spec[QCRYPTO_BLOCK_LUKS_HASH_SPEC_LEN];

    uint32_t payload_offset_sector;

    uint32_t master_key_len;

    uint8_t master_key_digest[QCRYPTO_BLOCK_LUKS_DIGEST_LEN];

    uint8_t master_key_salt[QCRYPTO_BLOCK_LUKS_SALT_LEN];

    uint32_t master_key_iterations;

    uint8_t uuid[QCRYPTO_BLOCK_LUKS_UUID_LEN];

    QCryptoBlockLUKSKeySlot key_slots[QCRYPTO_BLOCK_LUKS_NUM_KEY_SLOTS];
};


void
qcrypto_block_luks_to_disk_endian(QCryptoBlockLUKSHeader *hdr);
void
qcrypto_block_luks_from_disk_endian(QCryptoBlockLUKSHeader *hdr);
