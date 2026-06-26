
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/bswap.h"

#include "block-luks.h"
#include "block-luks-priv.h"

#include "crypto/hash.h"
#include "crypto/afsplit.h"
#include "crypto/pbkdf.h"
#include "crypto/secret.h"
#include "crypto/random.h"
#include "qemu/uuid.h"

#include "qemu/bitmap.h"
#include "qemu/range.h"


typedef struct QCryptoBlockLUKS QCryptoBlockLUKS;

typedef struct QCryptoBlockLUKSNameMap QCryptoBlockLUKSNameMap;
struct QCryptoBlockLUKSNameMap {
    const char *name;
    int id;
};

typedef struct QCryptoBlockLUKSCipherSizeMap QCryptoBlockLUKSCipherSizeMap;
struct QCryptoBlockLUKSCipherSizeMap {
    uint32_t key_bytes;
    int id;
};
typedef struct QCryptoBlockLUKSCipherNameMap QCryptoBlockLUKSCipherNameMap;
struct QCryptoBlockLUKSCipherNameMap {
    const char *name;
    const QCryptoBlockLUKSCipherSizeMap *sizes;
};


static const QCryptoBlockLUKSCipherSizeMap
qcrypto_block_luks_cipher_size_map_aes[] = {
    { 16, QCRYPTO_CIPHER_ALGO_AES_128 },
    { 24, QCRYPTO_CIPHER_ALGO_AES_192 },
    { 32, QCRYPTO_CIPHER_ALGO_AES_256 },
    { 0, 0 },
};

static const QCryptoBlockLUKSCipherSizeMap
qcrypto_block_luks_cipher_size_map_cast5[] = {
    { 16, QCRYPTO_CIPHER_ALGO_CAST5_128 },
    { 0, 0 },
};

static const QCryptoBlockLUKSCipherSizeMap
qcrypto_block_luks_cipher_size_map_serpent[] = {
    { 16, QCRYPTO_CIPHER_ALGO_SERPENT_128 },
    { 24, QCRYPTO_CIPHER_ALGO_SERPENT_192 },
    { 32, QCRYPTO_CIPHER_ALGO_SERPENT_256 },
    { 0, 0 },
};

static const QCryptoBlockLUKSCipherSizeMap
qcrypto_block_luks_cipher_size_map_twofish[] = {
    { 16, QCRYPTO_CIPHER_ALGO_TWOFISH_128 },
    { 24, QCRYPTO_CIPHER_ALGO_TWOFISH_192 },
    { 32, QCRYPTO_CIPHER_ALGO_TWOFISH_256 },
    { 0, 0 },
};

#ifdef CONFIG_CRYPTO_SM4
static const QCryptoBlockLUKSCipherSizeMap
qcrypto_block_luks_cipher_size_map_sm4[] = {
    { 16, QCRYPTO_CIPHER_ALGO_SM4},
    { 0, 0 },
};
#endif

static const QCryptoBlockLUKSCipherNameMap
qcrypto_block_luks_cipher_name_map[] = {
    { "aes", qcrypto_block_luks_cipher_size_map_aes },
    { "cast5", qcrypto_block_luks_cipher_size_map_cast5 },
    { "serpent", qcrypto_block_luks_cipher_size_map_serpent },
    { "twofish", qcrypto_block_luks_cipher_size_map_twofish },
#ifdef CONFIG_CRYPTO_SM4
    { "sm4", qcrypto_block_luks_cipher_size_map_sm4},
#endif
};

QEMU_BUILD_BUG_ON(sizeof(struct QCryptoBlockLUKSKeySlot) != 48);
QEMU_BUILD_BUG_ON(sizeof(struct QCryptoBlockLUKSHeader) != 592);


struct QCryptoBlockLUKS {
    QCryptoBlockLUKSHeader header;

    QCryptoCipherAlgo cipher_alg;

    QCryptoCipherMode cipher_mode;

    QCryptoIVGenAlgo ivgen_alg;

    QCryptoHashAlgo ivgen_hash_alg;

    QCryptoCipherAlgo ivgen_cipher_alg;

    QCryptoHashAlgo hash_alg;

    char *secret;
};


static int qcrypto_block_luks_cipher_name_lookup(const char *name,
                                                 QCryptoCipherMode mode,
                                                 uint32_t key_bytes,
                                                 Error **errp)
{
    const QCryptoBlockLUKSCipherNameMap *map =
        qcrypto_block_luks_cipher_name_map;
    size_t maplen = G_N_ELEMENTS(qcrypto_block_luks_cipher_name_map);
    size_t i, j;

    if (mode == QCRYPTO_CIPHER_MODE_XTS) {
        key_bytes /= 2;
    }

    for (i = 0; i < maplen; i++) {
        if (!g_str_equal(map[i].name, name)) {
            continue;
        }
        for (j = 0; j < map[i].sizes[j].key_bytes; j++) {
            if (map[i].sizes[j].key_bytes == key_bytes) {
                return map[i].sizes[j].id;
            }
        }
    }

    error_setg(errp, "Algorithm '%s' with key size %d bytes not supported",
               name, key_bytes);
    return 0;
}

static const char *
qcrypto_block_luks_cipher_alg_lookup(QCryptoCipherAlgo alg,
                                     Error **errp)
{
    const QCryptoBlockLUKSCipherNameMap *map =
        qcrypto_block_luks_cipher_name_map;
    size_t maplen = G_N_ELEMENTS(qcrypto_block_luks_cipher_name_map);
    size_t i, j;
    for (i = 0; i < maplen; i++) {
        for (j = 0; j < map[i].sizes[j].key_bytes; j++) {
            if (map[i].sizes[j].id == alg) {
                return map[i].name;
            }
        }
    }

    error_setg(errp, "Algorithm '%s' not supported",
               QCryptoCipherAlgo_str(alg));
    return NULL;
}

static int qcrypto_block_luks_name_lookup(const char *name,
                                          const QEnumLookup *map,
                                          const char *type,
                                          Error **errp)
{
    int ret = qapi_enum_parse(map, name, -1, NULL);

    if (ret < 0) {
        error_setg(errp, "%s '%s' not supported", type, name);
        return 0;
    }
    return ret;
}

#define qcrypto_block_luks_cipher_mode_lookup(name, errp)               \
    qcrypto_block_luks_name_lookup(name,                                \
                                   &QCryptoCipherMode_lookup,           \
                                   "Cipher mode",                       \
                                   errp)

#define qcrypto_block_luks_hash_name_lookup(name, errp)                 \
    qcrypto_block_luks_name_lookup(name,                                \
                                   &QCryptoHashAlgo_lookup,        \
                                   "Hash algorithm",                    \
                                   errp)

#define qcrypto_block_luks_ivgen_name_lookup(name, errp)                \
    qcrypto_block_luks_name_lookup(name,                                \
                                   &QCryptoIVGenAlgo_lookup,       \
                                   "IV generator",                      \
                                   errp)


static bool
qcrypto_block_luks_has_format(const uint8_t *buf,
                              size_t buf_size)
{
    const QCryptoBlockLUKSHeader *luks_header = (const void *)buf;

    if (buf_size >= offsetof(QCryptoBlockLUKSHeader, cipher_name) &&
        memcmp(luks_header->magic, qcrypto_block_luks_magic,
               QCRYPTO_BLOCK_LUKS_MAGIC_LEN) == 0 &&
        be16_to_cpu(luks_header->version) == QCRYPTO_BLOCK_LUKS_VERSION) {
        return true;
    } else {
        return false;
    }
}


static QCryptoCipherAlgo
qcrypto_block_luks_essiv_cipher(QCryptoCipherAlgo cipher,
                                QCryptoHashAlgo hash,
                                Error **errp)
{
    size_t digestlen = qcrypto_hash_digest_len(hash);
    size_t keylen = qcrypto_cipher_get_key_len(cipher);
    if (digestlen == keylen) {
        return cipher;
    }

    switch (cipher) {
    case QCRYPTO_CIPHER_ALGO_AES_128:
    case QCRYPTO_CIPHER_ALGO_AES_192:
    case QCRYPTO_CIPHER_ALGO_AES_256:
        if (digestlen == qcrypto_cipher_get_key_len(
                QCRYPTO_CIPHER_ALGO_AES_128)) {
            return QCRYPTO_CIPHER_ALGO_AES_128;
        } else if (digestlen == qcrypto_cipher_get_key_len(
                       QCRYPTO_CIPHER_ALGO_AES_192)) {
            return QCRYPTO_CIPHER_ALGO_AES_192;
        } else if (digestlen == qcrypto_cipher_get_key_len(
                       QCRYPTO_CIPHER_ALGO_AES_256)) {
            return QCRYPTO_CIPHER_ALGO_AES_256;
        } else {
            error_setg(errp, "No AES cipher with key size %zu available",
                       digestlen);
            return 0;
        }
        break;
    case QCRYPTO_CIPHER_ALGO_SERPENT_128:
    case QCRYPTO_CIPHER_ALGO_SERPENT_192:
    case QCRYPTO_CIPHER_ALGO_SERPENT_256:
        if (digestlen == qcrypto_cipher_get_key_len(
                QCRYPTO_CIPHER_ALGO_SERPENT_128)) {
            return QCRYPTO_CIPHER_ALGO_SERPENT_128;
        } else if (digestlen == qcrypto_cipher_get_key_len(
                       QCRYPTO_CIPHER_ALGO_SERPENT_192)) {
            return QCRYPTO_CIPHER_ALGO_SERPENT_192;
        } else if (digestlen == qcrypto_cipher_get_key_len(
                       QCRYPTO_CIPHER_ALGO_SERPENT_256)) {
            return QCRYPTO_CIPHER_ALGO_SERPENT_256;
        } else {
            error_setg(errp, "No Serpent cipher with key size %zu available",
                       digestlen);
            return 0;
        }
        break;
    case QCRYPTO_CIPHER_ALGO_TWOFISH_128:
    case QCRYPTO_CIPHER_ALGO_TWOFISH_192:
    case QCRYPTO_CIPHER_ALGO_TWOFISH_256:
        if (digestlen == qcrypto_cipher_get_key_len(
                QCRYPTO_CIPHER_ALGO_TWOFISH_128)) {
            return QCRYPTO_CIPHER_ALGO_TWOFISH_128;
        } else if (digestlen == qcrypto_cipher_get_key_len(
                       QCRYPTO_CIPHER_ALGO_TWOFISH_192)) {
            return QCRYPTO_CIPHER_ALGO_TWOFISH_192;
        } else if (digestlen == qcrypto_cipher_get_key_len(
                       QCRYPTO_CIPHER_ALGO_TWOFISH_256)) {
            return QCRYPTO_CIPHER_ALGO_TWOFISH_256;
        } else {
            error_setg(errp, "No Twofish cipher with key size %zu available",
                       digestlen);
            return 0;
        }
        break;
    default:
        error_setg(errp, "Cipher %s not supported with essiv",
                   QCryptoCipherAlgo_str(cipher));
        return 0;
    }
}

static int
qcrypto_block_luks_splitkeylen_sectors(const QCryptoBlockLUKS *luks,
                                       unsigned int header_sectors,
                                       unsigned int stripes)
{

    size_t splitkeylen = luks->header.master_key_len * stripes;

    size_t splitkeylen_sectors =
        DIV_ROUND_UP(splitkeylen, QCRYPTO_BLOCK_LUKS_SECTOR_SIZE);

    return ROUND_UP(splitkeylen_sectors, header_sectors);
}


void
qcrypto_block_luks_to_disk_endian(QCryptoBlockLUKSHeader *hdr)
{
    size_t i;

    cpu_to_be16s(&hdr->version);
    cpu_to_be32s(&hdr->payload_offset_sector);
    cpu_to_be32s(&hdr->master_key_len);
    cpu_to_be32s(&hdr->master_key_iterations);

    for (i = 0; i < QCRYPTO_BLOCK_LUKS_NUM_KEY_SLOTS; i++) {
        cpu_to_be32s(&hdr->key_slots[i].active);
        cpu_to_be32s(&hdr->key_slots[i].iterations);
        cpu_to_be32s(&hdr->key_slots[i].key_offset_sector);
        cpu_to_be32s(&hdr->key_slots[i].stripes);
    }
}

void
qcrypto_block_luks_from_disk_endian(QCryptoBlockLUKSHeader *hdr)
{
    size_t i;

    be16_to_cpus(&hdr->version);
    be32_to_cpus(&hdr->payload_offset_sector);
    be32_to_cpus(&hdr->master_key_len);
    be32_to_cpus(&hdr->master_key_iterations);

    for (i = 0; i < QCRYPTO_BLOCK_LUKS_NUM_KEY_SLOTS; i++) {
        be32_to_cpus(&hdr->key_slots[i].active);
        be32_to_cpus(&hdr->key_slots[i].iterations);
        be32_to_cpus(&hdr->key_slots[i].key_offset_sector);
        be32_to_cpus(&hdr->key_slots[i].stripes);
    }
}

static int
qcrypto_block_luks_store_header(QCryptoBlock *block,
                                QCryptoBlockWriteFunc writefunc,
                                void *opaque,
                                Error **errp)
{
    const QCryptoBlockLUKS *luks = block->opaque;
    Error *local_err = NULL;
    g_autofree QCryptoBlockLUKSHeader *hdr_copy = NULL;

    hdr_copy = g_new0(QCryptoBlockLUKSHeader, 1);
    memcpy(hdr_copy, &luks->header, sizeof(QCryptoBlockLUKSHeader));

    qcrypto_block_luks_to_disk_endian(hdr_copy);

    writefunc(block, 0, (const uint8_t *)hdr_copy, sizeof(*hdr_copy),
              opaque, &local_err);

    if (local_err) {
        error_propagate(errp, local_err);
        return -1;
    }
    return 0;
}

static int
qcrypto_block_luks_load_header(QCryptoBlock *block,
                                QCryptoBlockReadFunc readfunc,
                                void *opaque,
                                Error **errp)
{
    int rv;
    QCryptoBlockLUKS *luks = block->opaque;

    rv = readfunc(block, 0,
                  (uint8_t *)&luks->header,
                  sizeof(luks->header),
                  opaque,
                  errp);
    if (rv < 0) {
        return rv;
    }

    qcrypto_block_luks_from_disk_endian(&luks->header);

    return 0;
}

static int
qcrypto_block_luks_check_header(const QCryptoBlockLUKS *luks,
                                unsigned int flags,
                                Error **errp)
{
    size_t i, j;

    unsigned int header_sectors = QCRYPTO_BLOCK_LUKS_KEY_SLOT_OFFSET /
        QCRYPTO_BLOCK_LUKS_SECTOR_SIZE;
    bool detached = flags & QCRYPTO_BLOCK_OPEN_DETACHED;

    if (memcmp(luks->header.magic, qcrypto_block_luks_magic,
               QCRYPTO_BLOCK_LUKS_MAGIC_LEN) != 0) {
        error_setg(errp, "Volume is not in LUKS format");
        return -1;
    }

    if (luks->header.version != QCRYPTO_BLOCK_LUKS_VERSION) {
        error_setg(errp, "LUKS version %" PRIu32 " is not supported",
                   luks->header.version);
        return -1;
    }

    if (!memchr(luks->header.cipher_name, '\0',
                sizeof(luks->header.cipher_name))) {
        error_setg(errp, "LUKS header cipher name is not NUL terminated");
        return -1;
    }

    if (!memchr(luks->header.cipher_mode, '\0',
                sizeof(luks->header.cipher_mode))) {
        error_setg(errp, "LUKS header cipher mode is not NUL terminated");
        return -1;
    }

    if (!memchr(luks->header.hash_spec, '\0',
                sizeof(luks->header.hash_spec))) {
        error_setg(errp, "LUKS header hash spec is not NUL terminated");
        return -1;
    }

    if (!detached && luks->header.payload_offset_sector <
        DIV_ROUND_UP(QCRYPTO_BLOCK_LUKS_KEY_SLOT_OFFSET,
                     QCRYPTO_BLOCK_LUKS_SECTOR_SIZE)) {
        error_setg(errp, "LUKS payload is overlapping with the header");
        return -1;
    }

    if (luks->header.master_key_iterations == 0) {
        error_setg(errp, "LUKS key iteration count is zero");
        return -1;
    }

    for (i = 0 ; i < QCRYPTO_BLOCK_LUKS_NUM_KEY_SLOTS ; i++) {

        const QCryptoBlockLUKSKeySlot *slot1 = &luks->header.key_slots[i];
        unsigned int start1 = slot1->key_offset_sector;
        unsigned int len1 =
            qcrypto_block_luks_splitkeylen_sectors(luks,
                                                   header_sectors,
                                                   slot1->stripes);

        if (slot1->stripes != QCRYPTO_BLOCK_LUKS_STRIPES) {
            error_setg(errp, "Keyslot %zu is corrupted (stripes %d != %d)",
                       i, slot1->stripes, QCRYPTO_BLOCK_LUKS_STRIPES);
            return -1;
        }

        if (slot1->active != QCRYPTO_BLOCK_LUKS_KEY_SLOT_DISABLED &&
            slot1->active != QCRYPTO_BLOCK_LUKS_KEY_SLOT_ENABLED) {
            error_setg(errp,
                       "Keyslot %zu state (active/disable) is corrupted", i);
            return -1;
        }

        if (slot1->active == QCRYPTO_BLOCK_LUKS_KEY_SLOT_ENABLED &&
            slot1->iterations == 0) {
            error_setg(errp, "Keyslot %zu iteration count is zero", i);
            return -1;
        }

        if (start1 < DIV_ROUND_UP(QCRYPTO_BLOCK_LUKS_KEY_SLOT_OFFSET,
                                  QCRYPTO_BLOCK_LUKS_SECTOR_SIZE)) {
            error_setg(errp,
                       "Keyslot %zu is overlapping with the LUKS header",
                       i);
            return -1;
        }

        if (!detached && start1 + len1 > luks->header.payload_offset_sector) {
            error_setg(errp,
                       "Keyslot %zu is overlapping with the encrypted payload",
                       i);
            return -1;
        }

        for (j = i + 1 ; j < QCRYPTO_BLOCK_LUKS_NUM_KEY_SLOTS ; j++) {
            const QCryptoBlockLUKSKeySlot *slot2 = &luks->header.key_slots[j];
            unsigned int start2 = slot2->key_offset_sector;
            unsigned int len2 =
                qcrypto_block_luks_splitkeylen_sectors(luks,
                                                       header_sectors,
                                                       slot2->stripes);

            if (ranges_overlap(start1, len1, start2, len2)) {
                error_setg(errp,
                           "Keyslots %zu and %zu are overlapping in the header",
                           i, j);
                return -1;
            }
        }

    }
    return 0;
}


static int
qcrypto_block_luks_parse_header(QCryptoBlockLUKS *luks, Error **errp)
{
    g_autofree char *cipher_mode = g_strdup(luks->header.cipher_mode);
    char *ivgen_name, *ivhash_name;
    Error *local_err = NULL;

    ivgen_name = strchr(cipher_mode, '-');
    if (!ivgen_name) {
        error_setg(errp, "Unexpected cipher mode string format '%s'",
                   luks->header.cipher_mode);
        return -1;
    }
    *ivgen_name = '\0';
    ivgen_name++;

    ivhash_name = strchr(ivgen_name, ':');
    if (!ivhash_name) {
        luks->ivgen_hash_alg = 0;
    } else {
        *ivhash_name = '\0';
        ivhash_name++;

        luks->ivgen_hash_alg = qcrypto_block_luks_hash_name_lookup(ivhash_name,
                                                                   &local_err);
        if (local_err) {
            error_propagate(errp, local_err);
            return -1;
        }
    }

    luks->cipher_mode = qcrypto_block_luks_cipher_mode_lookup(cipher_mode,
                                                              &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        return -1;
    }

    luks->cipher_alg =
            qcrypto_block_luks_cipher_name_lookup(luks->header.cipher_name,
                                                  luks->cipher_mode,
                                                  luks->header.master_key_len,
                                                  &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        return -1;
    }

    luks->hash_alg =
            qcrypto_block_luks_hash_name_lookup(luks->header.hash_spec,
                                                &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        return -1;
    }

    luks->ivgen_alg = qcrypto_block_luks_ivgen_name_lookup(ivgen_name,
                                                           &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        return -1;
    }

    if (luks->ivgen_alg == QCRYPTO_IV_GEN_ALGO_ESSIV) {
        if (!ivhash_name) {
            error_setg(errp, "Missing IV generator hash specification");
            return -1;
        }
        luks->ivgen_cipher_alg =
                qcrypto_block_luks_essiv_cipher(luks->cipher_alg,
                                                luks->ivgen_hash_alg,
                                                &local_err);
        if (local_err) {
            error_propagate(errp, local_err);
            return -1;
        }
    } else {

        luks->ivgen_cipher_alg = luks->cipher_alg;
    }
    return 0;
}

static int
qcrypto_block_luks_store_key(QCryptoBlock *block,
                             unsigned int slot_idx,
                             const char *password,
                             uint8_t *masterkey,
                             uint64_t iter_time,
                             QCryptoBlockWriteFunc writefunc,
                             void *opaque,
                             Error **errp)
{
    QCryptoBlockLUKS *luks = block->opaque;
    QCryptoBlockLUKSKeySlot *slot;
    g_autofree uint8_t *splitkey = NULL;
    size_t splitkeylen;
    g_autofree uint8_t *slotkey = NULL;
    g_autoptr(QCryptoCipher) cipher = NULL;
    g_autoptr(QCryptoIVGen) ivgen = NULL;
    Error *local_err = NULL;
    uint64_t iters;
    int ret = -1;

    assert(slot_idx < QCRYPTO_BLOCK_LUKS_NUM_KEY_SLOTS);
    slot = &luks->header.key_slots[slot_idx];
    splitkeylen = luks->header.master_key_len * slot->stripes;

    if (qcrypto_random_bytes(slot->salt,
                             QCRYPTO_BLOCK_LUKS_SALT_LEN,
                             errp) < 0) {
        goto cleanup;
    }

    iters = qcrypto_pbkdf2_count_iters(luks->hash_alg,
                                       (uint8_t *)password, strlen(password),
                                       slot->salt,
                                       QCRYPTO_BLOCK_LUKS_SALT_LEN,
                                       luks->header.master_key_len,
                                       &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        goto cleanup;
    }

    if (iters > (ULLONG_MAX / iter_time)) {
        error_setg_errno(errp, ERANGE,
                         "PBKDF iterations %llu too large to scale",
                         (unsigned long long)iters);
        goto cleanup;
    }

    iters = iters * iter_time / 1000;

    if (iters > UINT32_MAX) {
        error_setg_errno(errp, ERANGE,
                         "PBKDF iterations %llu larger than %u",
                         (unsigned long long)iters, UINT32_MAX);
        goto cleanup;
    }

    slot->iterations =
        MAX(iters, QCRYPTO_BLOCK_LUKS_MIN_SLOT_KEY_ITERS);


    slotkey = g_new0(uint8_t, luks->header.master_key_len);
    if (qcrypto_pbkdf2(luks->hash_alg,
                       (uint8_t *)password, strlen(password),
                       slot->salt,
                       QCRYPTO_BLOCK_LUKS_SALT_LEN,
                       slot->iterations,
                       slotkey, luks->header.master_key_len,
                       errp) < 0) {
        goto cleanup;
    }


    cipher = qcrypto_cipher_new(luks->cipher_alg,
                                luks->cipher_mode,
                                slotkey, luks->header.master_key_len,
                                errp);
    if (!cipher) {
        goto cleanup;
    }

    ivgen = qcrypto_ivgen_new(luks->ivgen_alg,
                              luks->ivgen_cipher_alg,
                              luks->ivgen_hash_alg,
                              slotkey, luks->header.master_key_len,
                              errp);
    if (!ivgen) {
        goto cleanup;
    }

    splitkey = g_new0(uint8_t, splitkeylen);

    if (qcrypto_afsplit_encode(luks->hash_alg,
                               luks->header.master_key_len,
                               slot->stripes,
                               masterkey,
                               splitkey,
                               errp) < 0) {
        goto cleanup;
    }

    if (qcrypto_block_cipher_encrypt_helper(cipher, block->niv, ivgen,
                                            QCRYPTO_BLOCK_LUKS_SECTOR_SIZE,
                                            0,
                                            splitkey,
                                            splitkeylen,
                                            errp) < 0) {
        goto cleanup;
    }

    if (writefunc(block,
                  slot->key_offset_sector *
                  QCRYPTO_BLOCK_LUKS_SECTOR_SIZE,
                  splitkey, splitkeylen,
                  opaque,
                  errp) < 0) {
        goto cleanup;
    }

    slot->active = QCRYPTO_BLOCK_LUKS_KEY_SLOT_ENABLED;

    if (qcrypto_block_luks_store_header(block,  writefunc, opaque, errp) < 0) {
        goto cleanup;
    }

    ret = 0;

cleanup:
    if (slotkey) {
        memset(slotkey, 0, luks->header.master_key_len);
    }
    if (splitkey) {
        memset(splitkey, 0, splitkeylen);
    }
    return ret;
}

static int
qcrypto_block_luks_load_key(QCryptoBlock *block,
                            size_t slot_idx,
                            const char *password,
                            uint8_t *masterkey,
                            QCryptoBlockReadFunc readfunc,
                            void *opaque,
                            Error **errp)
{
    QCryptoBlockLUKS *luks = block->opaque;
    const QCryptoBlockLUKSKeySlot *slot;
    g_autofree uint8_t *splitkey = NULL;
    size_t splitkeylen;
    g_autofree uint8_t *possiblekey = NULL;
    int rv;
    g_autoptr(QCryptoCipher) cipher = NULL;
    uint8_t keydigest[QCRYPTO_BLOCK_LUKS_DIGEST_LEN];
    g_autoptr(QCryptoIVGen) ivgen = NULL;
    size_t niv;

    assert(slot_idx < QCRYPTO_BLOCK_LUKS_NUM_KEY_SLOTS);
    slot = &luks->header.key_slots[slot_idx];
    if (slot->active != QCRYPTO_BLOCK_LUKS_KEY_SLOT_ENABLED) {
        return 0;
    }

    splitkeylen = luks->header.master_key_len * slot->stripes;
    splitkey = g_new0(uint8_t, splitkeylen);
    possiblekey = g_new0(uint8_t, luks->header.master_key_len);

    if (qcrypto_pbkdf2(luks->hash_alg,
                       (const uint8_t *)password, strlen(password),
                       slot->salt, QCRYPTO_BLOCK_LUKS_SALT_LEN,
                       slot->iterations,
                       possiblekey, luks->header.master_key_len,
                       errp) < 0) {
        return -1;
    }

    rv = readfunc(block,
                  slot->key_offset_sector * QCRYPTO_BLOCK_LUKS_SECTOR_SIZE,
                  splitkey, splitkeylen,
                  opaque,
                  errp);
    if (rv < 0) {
        return -1;
    }


    cipher = qcrypto_cipher_new(luks->cipher_alg,
                                luks->cipher_mode,
                                possiblekey,
                                luks->header.master_key_len,
                                errp);
    if (!cipher) {
        return -1;
    }

    niv = qcrypto_cipher_get_iv_len(luks->cipher_alg,
                                    luks->cipher_mode);

    ivgen = qcrypto_ivgen_new(luks->ivgen_alg,
                              luks->ivgen_cipher_alg,
                              luks->ivgen_hash_alg,
                              possiblekey,
                              luks->header.master_key_len,
                              errp);
    if (!ivgen) {
        return -1;
    }


    if (qcrypto_block_cipher_decrypt_helper(cipher,
                                            niv,
                                            ivgen,
                                            QCRYPTO_BLOCK_LUKS_SECTOR_SIZE,
                                            0,
                                            splitkey,
                                            splitkeylen,
                                            errp) < 0) {
        return -1;
    }

    if (qcrypto_afsplit_decode(luks->hash_alg,
                               luks->header.master_key_len,
                               slot->stripes,
                               splitkey,
                               masterkey,
                               errp) < 0) {
        return -1;
    }


    if (qcrypto_pbkdf2(luks->hash_alg,
                       masterkey,
                       luks->header.master_key_len,
                       luks->header.master_key_salt,
                       QCRYPTO_BLOCK_LUKS_SALT_LEN,
                       luks->header.master_key_iterations,
                       keydigest,
                       G_N_ELEMENTS(keydigest),
                       errp) < 0) {
        return -1;
    }

    if (memcmp(keydigest, luks->header.master_key_digest,
               QCRYPTO_BLOCK_LUKS_DIGEST_LEN) == 0) {
        return 1;
    }

    return 0;
}


static int
qcrypto_block_luks_find_key(QCryptoBlock *block,
                            const char *password,
                            uint8_t *masterkey,
                            QCryptoBlockReadFunc readfunc,
                            void *opaque,
                            Error **errp)
{
    size_t i;
    int rv;

    for (i = 0; i < QCRYPTO_BLOCK_LUKS_NUM_KEY_SLOTS; i++) {
        rv = qcrypto_block_luks_load_key(block,
                                         i,
                                         password,
                                         masterkey,
                                         readfunc,
                                         opaque,
                                         errp);
        if (rv < 0) {
            goto error;
        }
        if (rv == 1) {
            return 0;
        }
    }

    error_setg(errp, "Invalid password, cannot unlock any keyslot");
 error:
    return -1;
}

static bool
qcrypto_block_luks_slot_active(const QCryptoBlockLUKS *luks,
                               unsigned int slot_idx)
{
    uint32_t val;

    assert(slot_idx < QCRYPTO_BLOCK_LUKS_NUM_KEY_SLOTS);
    val = luks->header.key_slots[slot_idx].active;
    return val == QCRYPTO_BLOCK_LUKS_KEY_SLOT_ENABLED;
}

static unsigned int
qcrypto_block_luks_count_active_slots(const QCryptoBlockLUKS *luks)
{
    size_t i = 0;
    unsigned int ret = 0;

    for (i = 0; i < QCRYPTO_BLOCK_LUKS_NUM_KEY_SLOTS; i++) {
        if (qcrypto_block_luks_slot_active(luks, i)) {
            ret++;
        }
    }
    return ret;
}

static int
qcrypto_block_luks_find_free_keyslot(const QCryptoBlockLUKS *luks)
{
    size_t i;

    for (i = 0; i < QCRYPTO_BLOCK_LUKS_NUM_KEY_SLOTS; i++) {
        if (!qcrypto_block_luks_slot_active(luks, i)) {
            return i;
        }
    }
    return -1;
}

static int
qcrypto_block_luks_erase_key(QCryptoBlock *block,
                             unsigned int slot_idx,
                             QCryptoBlockWriteFunc writefunc,
                             void *opaque,
                             Error **errp)
{
    QCryptoBlockLUKS *luks = block->opaque;
    QCryptoBlockLUKSKeySlot *slot;
    g_autofree uint8_t *garbagesplitkey = NULL;
    size_t splitkeylen;
    size_t i;
    Error *local_err = NULL;
    int ret;

    assert(slot_idx < QCRYPTO_BLOCK_LUKS_NUM_KEY_SLOTS);
    slot = &luks->header.key_slots[slot_idx];

    splitkeylen = luks->header.master_key_len * slot->stripes;
    assert(splitkeylen > 0);

    garbagesplitkey = g_new0(uint8_t, splitkeylen);

    memset(slot->salt, 0, QCRYPTO_BLOCK_LUKS_SALT_LEN);
    slot->iterations = 0;
    slot->active = QCRYPTO_BLOCK_LUKS_KEY_SLOT_DISABLED;

    ret = qcrypto_block_luks_store_header(block, writefunc,
                                          opaque, &local_err);

    if (ret < 0) {
        error_propagate(errp, local_err);
    }
    for (i = 0; i < QCRYPTO_BLOCK_LUKS_ERASE_ITERATIONS; i++) {
        if (qcrypto_random_bytes(garbagesplitkey,
                                 splitkeylen, &local_err) < 0) {
            error_propagate(errp, local_err);

            if (i > 0) {
                return -1;
            }
        }
        if (writefunc(block,
                      slot->key_offset_sector * QCRYPTO_BLOCK_LUKS_SECTOR_SIZE,
                      garbagesplitkey,
                      splitkeylen,
                      opaque,
                      &local_err) < 0) {
            error_propagate(errp, local_err);
            return -1;
        }
    }
    return ret;
}

static int
qcrypto_block_luks_open(QCryptoBlock *block,
                        QCryptoBlockOpenOptions *options,
                        const char *optprefix,
                        QCryptoBlockReadFunc readfunc,
                        void *opaque,
                        unsigned int flags,
                        Error **errp)
{
    QCryptoBlockLUKS *luks = NULL;
    g_autofree uint8_t *masterkey = NULL;
    g_autofree char *password = NULL;

    if (!(flags & QCRYPTO_BLOCK_OPEN_NO_IO)) {
        if (!options->u.luks.key_secret) {
            error_setg(errp, "Parameter '%skey-secret' is required for cipher",
                       optprefix ? optprefix : "");
            return -1;
        }
        password = qcrypto_secret_lookup_as_utf8(
            options->u.luks.key_secret, errp);
        if (!password) {
            return -1;
        }
    }

    luks = g_new0(QCryptoBlockLUKS, 1);
    block->opaque = luks;
    luks->secret = g_strdup(options->u.luks.key_secret);

    if (qcrypto_block_luks_load_header(block, readfunc, opaque, errp) < 0) {
        goto fail;
    }

    if (qcrypto_block_luks_check_header(luks, flags, errp) < 0) {
        goto fail;
    }

    if (qcrypto_block_luks_parse_header(luks, errp) < 0) {
        goto fail;
    }

    if (!(flags & QCRYPTO_BLOCK_OPEN_NO_IO)) {

        masterkey = g_new0(uint8_t, luks->header.master_key_len);

        if (qcrypto_block_luks_find_key(block,
                                        password,
                                        masterkey,
                                        readfunc, opaque,
                                        errp) < 0) {
            goto fail;
        }

        block->kdfhash = luks->hash_alg;
        block->niv = qcrypto_cipher_get_iv_len(luks->cipher_alg,
                                               luks->cipher_mode);

        block->ivgen = qcrypto_ivgen_new(luks->ivgen_alg,
                                         luks->ivgen_cipher_alg,
                                         luks->ivgen_hash_alg,
                                         masterkey,
                                         luks->header.master_key_len,
                                         errp);
        if (!block->ivgen) {
            goto fail;
        }

        if (qcrypto_block_init_cipher(block,
                                      luks->cipher_alg,
                                      luks->cipher_mode,
                                      masterkey,
                                      luks->header.master_key_len,
                                      errp) < 0) {
            goto fail;
        }
    }

    block->sector_size = QCRYPTO_BLOCK_LUKS_SECTOR_SIZE;
    block->payload_offset = luks->header.payload_offset_sector *
        block->sector_size;
    block->detached_header = (block->payload_offset == 0) ? true : false;

    return 0;

 fail:
    qcrypto_block_free_cipher(block);
    qcrypto_ivgen_free(block->ivgen);
    g_free(luks->secret);
    g_free(luks);
    return -1;
}


static void
qcrypto_block_luks_uuid_gen(uint8_t *uuidstr)
{
    QemuUUID uuid;
    qemu_uuid_generate(&uuid);
    qemu_uuid_unparse(&uuid, (char *)uuidstr);
}

static int
qcrypto_block_luks_create(QCryptoBlock *block,
                          QCryptoBlockCreateOptions *options,
                          const char *optprefix,
                          QCryptoBlockInitFunc initfunc,
                          QCryptoBlockWriteFunc writefunc,
                          void *opaque,
                          Error **errp)
{
    QCryptoBlockLUKS *luks;
    QCryptoBlockCreateOptionsLUKS luks_opts;
    Error *local_err = NULL;
    g_autofree uint8_t *masterkey = NULL;
    size_t header_sectors;
    size_t split_key_sectors;
    size_t i;
    g_autofree char *password = NULL;
    const char *cipher_alg;
    const char *cipher_mode;
    const char *ivgen_alg;
    const char *ivgen_hash_alg = NULL;
    const char *hash_alg;
    g_autofree char *cipher_mode_spec = NULL;
    uint64_t iters;
    uint64_t detached_header_size;

    memcpy(&luks_opts, &options->u.luks, sizeof(luks_opts));
    if (!luks_opts.has_iter_time) {
        luks_opts.iter_time = QCRYPTO_BLOCK_LUKS_DEFAULT_ITER_TIME_MS;
    }
    if (!luks_opts.has_cipher_alg) {
        luks_opts.cipher_alg = QCRYPTO_CIPHER_ALGO_AES_256;
    }
    if (!luks_opts.has_cipher_mode) {
        luks_opts.cipher_mode = QCRYPTO_CIPHER_MODE_XTS;
    }
    if (!luks_opts.has_ivgen_alg) {
        luks_opts.ivgen_alg = QCRYPTO_IV_GEN_ALGO_PLAIN64;
    }
    if (!luks_opts.has_hash_alg) {
        luks_opts.hash_alg = QCRYPTO_HASH_ALGO_SHA256;
    }
    if (luks_opts.ivgen_alg == QCRYPTO_IV_GEN_ALGO_ESSIV) {
        if (!luks_opts.has_ivgen_hash_alg) {
            luks_opts.ivgen_hash_alg = QCRYPTO_HASH_ALGO_SHA256;
            luks_opts.has_ivgen_hash_alg = true;
        }
    }

    luks = g_new0(QCryptoBlockLUKS, 1);
    block->opaque = luks;

    luks->cipher_alg = luks_opts.cipher_alg;
    luks->cipher_mode = luks_opts.cipher_mode;
    luks->ivgen_alg = luks_opts.ivgen_alg;
    luks->ivgen_hash_alg = luks_opts.ivgen_hash_alg;
    luks->hash_alg = luks_opts.hash_alg;



    if (!options->u.luks.key_secret) {
        error_setg(errp, "Parameter '%skey-secret' is required for cipher",
                   optprefix ? optprefix : "");
        goto error;
    }
    luks->secret = g_strdup(options->u.luks.key_secret);

    password = qcrypto_secret_lookup_as_utf8(luks_opts.key_secret, errp);
    if (!password) {
        goto error;
    }


    memcpy(luks->header.magic, qcrypto_block_luks_magic,
           QCRYPTO_BLOCK_LUKS_MAGIC_LEN);

    luks->header.version = QCRYPTO_BLOCK_LUKS_VERSION;
    qcrypto_block_luks_uuid_gen(luks->header.uuid);

    cipher_alg = qcrypto_block_luks_cipher_alg_lookup(luks_opts.cipher_alg,
                                                      errp);
    if (!cipher_alg) {
        goto error;
    }

    cipher_mode = QCryptoCipherMode_str(luks_opts.cipher_mode);
    ivgen_alg = QCryptoIVGenAlgo_str(luks_opts.ivgen_alg);
    if (luks_opts.has_ivgen_hash_alg) {
        ivgen_hash_alg = QCryptoHashAlgo_str(luks_opts.ivgen_hash_alg);
        cipher_mode_spec = g_strdup_printf("%s-%s:%s", cipher_mode, ivgen_alg,
                                           ivgen_hash_alg);
    } else {
        cipher_mode_spec = g_strdup_printf("%s-%s", cipher_mode, ivgen_alg);
    }
    hash_alg = QCryptoHashAlgo_str(luks_opts.hash_alg);


    if (strlen(cipher_alg) >= QCRYPTO_BLOCK_LUKS_CIPHER_NAME_LEN) {
        error_setg(errp, "Cipher name '%s' is too long for LUKS header",
                   cipher_alg);
        goto error;
    }
    if (strlen(cipher_mode_spec) >= QCRYPTO_BLOCK_LUKS_CIPHER_MODE_LEN) {
        error_setg(errp, "Cipher mode '%s' is too long for LUKS header",
                   cipher_mode_spec);
        goto error;
    }
    if (strlen(hash_alg) >= QCRYPTO_BLOCK_LUKS_HASH_SPEC_LEN) {
        error_setg(errp, "Hash name '%s' is too long for LUKS header",
                   hash_alg);
        goto error;
    }

    if (luks_opts.ivgen_alg == QCRYPTO_IV_GEN_ALGO_ESSIV) {
        luks->ivgen_cipher_alg =
                qcrypto_block_luks_essiv_cipher(luks_opts.cipher_alg,
                                                luks_opts.ivgen_hash_alg,
                                                &local_err);
        if (local_err) {
            error_propagate(errp, local_err);
            goto error;
        }
    } else {
        luks->ivgen_cipher_alg = luks_opts.cipher_alg;
    }

    strcpy(luks->header.cipher_name, cipher_alg);
    strcpy(luks->header.cipher_mode, cipher_mode_spec);
    strcpy(luks->header.hash_spec, hash_alg);

    luks->header.master_key_len =
        qcrypto_cipher_get_key_len(luks_opts.cipher_alg);

    if (luks_opts.cipher_mode == QCRYPTO_CIPHER_MODE_XTS) {
        luks->header.master_key_len *= 2;
    }

    if (qcrypto_random_bytes(luks->header.master_key_salt,
                             QCRYPTO_BLOCK_LUKS_SALT_LEN,
                             errp) < 0) {
        goto error;
    }

    masterkey = g_new0(uint8_t, luks->header.master_key_len);
    if (qcrypto_random_bytes(masterkey,
                             luks->header.master_key_len, errp) < 0) {
        goto error;
    }


    if (qcrypto_block_init_cipher(block, luks_opts.cipher_alg,
                                  luks_opts.cipher_mode, masterkey,
                                  luks->header.master_key_len, errp) < 0) {
        goto error;
    }

    block->kdfhash = luks_opts.hash_alg;
    block->niv = qcrypto_cipher_get_iv_len(luks_opts.cipher_alg,
                                           luks_opts.cipher_mode);
    block->ivgen = qcrypto_ivgen_new(luks_opts.ivgen_alg,
                                     luks->ivgen_cipher_alg,
                                     luks_opts.ivgen_hash_alg,
                                     masterkey, luks->header.master_key_len,
                                     errp);

    if (!block->ivgen) {
        goto error;
    }


    iters = qcrypto_pbkdf2_count_iters(luks_opts.hash_alg,
                                       masterkey, luks->header.master_key_len,
                                       luks->header.master_key_salt,
                                       QCRYPTO_BLOCK_LUKS_SALT_LEN,
                                       QCRYPTO_BLOCK_LUKS_DIGEST_LEN,
                                       &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        goto error;
    }

    if (iters > (ULLONG_MAX / luks_opts.iter_time)) {
        error_setg_errno(errp, ERANGE,
                         "PBKDF iterations %llu too large to scale",
                         (unsigned long long)iters);
        goto error;
    }

    iters = iters * luks_opts.iter_time / 1000;

    iters /= 8;
    if (iters > UINT32_MAX) {
        error_setg_errno(errp, ERANGE,
                         "PBKDF iterations %llu larger than %u",
                         (unsigned long long)iters, UINT32_MAX);
        goto error;
    }
    iters = MAX(iters, QCRYPTO_BLOCK_LUKS_MIN_MASTER_KEY_ITERS);
    luks->header.master_key_iterations = iters;

    if (qcrypto_pbkdf2(luks_opts.hash_alg,
                       masterkey, luks->header.master_key_len,
                       luks->header.master_key_salt,
                       QCRYPTO_BLOCK_LUKS_SALT_LEN,
                       luks->header.master_key_iterations,
                       luks->header.master_key_digest,
                       QCRYPTO_BLOCK_LUKS_DIGEST_LEN,
                       errp) < 0) {
        goto error;
    }

    header_sectors = QCRYPTO_BLOCK_LUKS_KEY_SLOT_OFFSET /
        QCRYPTO_BLOCK_LUKS_SECTOR_SIZE;

    split_key_sectors =
        qcrypto_block_luks_splitkeylen_sectors(luks,
                                               header_sectors,
                                               QCRYPTO_BLOCK_LUKS_STRIPES);

    for (i = 0; i < QCRYPTO_BLOCK_LUKS_NUM_KEY_SLOTS; i++) {
        QCryptoBlockLUKSKeySlot *slot = &luks->header.key_slots[i];
        slot->active = QCRYPTO_BLOCK_LUKS_KEY_SLOT_DISABLED;

        slot->key_offset_sector = header_sectors + i * split_key_sectors;
        slot->stripes = QCRYPTO_BLOCK_LUKS_STRIPES;
    }

    if (block->detached_header) {
        luks->header.payload_offset_sector = 0;
    } else {
        luks->header.payload_offset_sector = header_sectors +
                QCRYPTO_BLOCK_LUKS_NUM_KEY_SLOTS * split_key_sectors;
    }

    block->sector_size = QCRYPTO_BLOCK_LUKS_SECTOR_SIZE;
    block->payload_offset = luks->header.payload_offset_sector *
        block->sector_size;
    detached_header_size =
        (header_sectors + QCRYPTO_BLOCK_LUKS_NUM_KEY_SLOTS *
         split_key_sectors) * block->sector_size;

    initfunc(block, detached_header_size, opaque, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        goto error;
    }


    if (qcrypto_block_luks_store_key(block,
                                     0,
                                     password,
                                     masterkey,
                                     luks_opts.iter_time,
                                     writefunc,
                                     opaque,
                                     errp) < 0) {
        goto error;
    }

    memset(masterkey, 0, luks->header.master_key_len);

    return 0;

 error:
    if (masterkey) {
        memset(masterkey, 0, luks->header.master_key_len);
    }

    qcrypto_block_free_cipher(block);
    qcrypto_ivgen_free(block->ivgen);

    g_free(luks->secret);
    g_free(luks);
    return -1;
}

static int
qcrypto_block_luks_amend_add_keyslot(QCryptoBlock *block,
                                     QCryptoBlockReadFunc readfunc,
                                     QCryptoBlockWriteFunc writefunc,
                                     void *opaque,
                                     QCryptoBlockAmendOptionsLUKS *opts_luks,
                                     bool force,
                                     Error **errp)
{
    QCryptoBlockLUKS *luks = block->opaque;
    uint64_t iter_time = opts_luks->has_iter_time ?
                         opts_luks->iter_time :
                         QCRYPTO_BLOCK_LUKS_DEFAULT_ITER_TIME_MS;
    int keyslot;
    g_autofree char *old_password = NULL;
    g_autofree char *new_password = NULL;
    g_autofree uint8_t *master_key = NULL;

    char *secret = opts_luks->secret ?: luks->secret;

    if (!opts_luks->new_secret) {
        error_setg(errp, "'new-secret' is required to activate a keyslot");
        return -1;
    }
    if (opts_luks->old_secret) {
        error_setg(errp,
                   "'old-secret' must not be given when activating keyslots");
        return -1;
    }

    if (opts_luks->has_keyslot) {
        keyslot = opts_luks->keyslot;
        if (keyslot < 0 || keyslot >= QCRYPTO_BLOCK_LUKS_NUM_KEY_SLOTS) {
            error_setg(errp,
                       "Invalid keyslot %u specified, must be between 0 and %u",
                       keyslot, QCRYPTO_BLOCK_LUKS_NUM_KEY_SLOTS - 1);
            return -1;
        }
    } else {
        keyslot = qcrypto_block_luks_find_free_keyslot(luks);
        if (keyslot == -1) {
            error_setg(errp,
                       "Can't add a keyslot - all keyslots are in use");
            return -1;
        }
    }

    if (!force && qcrypto_block_luks_slot_active(luks, keyslot)) {
        error_setg(errp,
                   "Refusing to overwrite active keyslot %i - "
                   "please erase it first",
                   keyslot);
        return -1;
    }

    old_password = qcrypto_secret_lookup_as_utf8(secret, errp);
    if (!old_password) {
        return -1;
    }

    master_key = g_new0(uint8_t, luks->header.master_key_len);

    if (qcrypto_block_luks_find_key(block, old_password, master_key,
                                    readfunc, opaque, errp) < 0) {
        error_append_hint(errp, "Failed to retrieve the master key");
        return -1;
    }

    new_password = qcrypto_secret_lookup_as_utf8(opts_luks->new_secret, errp);
    if (!new_password) {
        return -1;
    }

    if (qcrypto_block_luks_store_key(block, keyslot, new_password, master_key,
                                     iter_time, writefunc, opaque, errp)) {
        error_append_hint(errp, "Failed to write to keyslot %i", keyslot);
        return -1;
    }
    return 0;
}

static int
qcrypto_block_luks_amend_erase_keyslots(QCryptoBlock *block,
                                        QCryptoBlockReadFunc readfunc,
                                        QCryptoBlockWriteFunc writefunc,
                                        void *opaque,
                                        QCryptoBlockAmendOptionsLUKS *opts_luks,
                                        bool force,
                                        Error **errp)
{
    QCryptoBlockLUKS *luks = block->opaque;
    g_autofree uint8_t *tmpkey = NULL;
    g_autofree char *old_password = NULL;

    if (opts_luks->new_secret) {
        error_setg(errp,
                   "'new-secret' must not be given when erasing keyslots");
        return -1;
    }
    if (opts_luks->has_iter_time) {
        error_setg(errp,
                   "'iter-time' must not be given when erasing keyslots");
        return -1;
    }
    if (opts_luks->secret) {
        error_setg(errp,
                   "'secret' must not be given when erasing keyslots");
        return -1;
    }

    if (opts_luks->old_secret) {
        old_password = qcrypto_secret_lookup_as_utf8(opts_luks->old_secret,
                                                     errp);
        if (!old_password) {
            return -1;
        }

        tmpkey = g_new0(uint8_t, luks->header.master_key_len);
    }

    if (opts_luks->has_keyslot) {
        int keyslot = opts_luks->keyslot;

        if (keyslot < 0 || keyslot >= QCRYPTO_BLOCK_LUKS_NUM_KEY_SLOTS) {
            error_setg(errp,
                       "Invalid keyslot %i specified, must be between 0 and %i",
                       keyslot, QCRYPTO_BLOCK_LUKS_NUM_KEY_SLOTS - 1);
            return -1;
        }

        if (opts_luks->old_secret) {
            int rv = qcrypto_block_luks_load_key(block,
                                                 keyslot,
                                                 old_password,
                                                 tmpkey,
                                                 readfunc,
                                                 opaque,
                                                 errp);
            if (rv == -1) {
                return -1;
            } else if (rv == 0) {
                error_setg(errp,
                           "Given keyslot %i doesn't contain the given "
                           "old password for erase operation",
                           keyslot);
                return -1;
            }
        }

        if (!force && !qcrypto_block_luks_slot_active(luks, keyslot)) {
            error_setg(errp,
                       "Given keyslot %i is already erased (inactive) ",
                       keyslot);
            return -1;
        }

        if (!force && qcrypto_block_luks_count_active_slots(luks) == 1) {
            error_setg(errp,
                       "Attempt to erase the only active keyslot %i "
                       "which will erase all the data in the image "
                       "irreversibly - refusing operation",
                       keyslot);
            return -1;
        }

        if (qcrypto_block_luks_erase_key(block, keyslot,
                                         writefunc, opaque, errp)) {
            error_append_hint(errp, "Failed to erase keyslot %i", keyslot);
            return -1;
        }

    } else if (opts_luks->old_secret) {

        unsigned long slots_to_erase_bitmap = 0;
        size_t i;
        int slot_count;

        assert(QCRYPTO_BLOCK_LUKS_NUM_KEY_SLOTS <=
               sizeof(slots_to_erase_bitmap) * 8);

        for (i = 0; i < QCRYPTO_BLOCK_LUKS_NUM_KEY_SLOTS; i++) {
            int rv = qcrypto_block_luks_load_key(block,
                                                 i,
                                                 old_password,
                                                 tmpkey,
                                                 readfunc,
                                                 opaque,
                                                 errp);
            if (rv == -1) {
                return -1;
            } else if (rv == 1) {
                bitmap_set(&slots_to_erase_bitmap, i, 1);
            }
        }

        slot_count = bitmap_count_one(&slots_to_erase_bitmap,
                                      QCRYPTO_BLOCK_LUKS_NUM_KEY_SLOTS);
        if (slot_count == 0) {
            error_setg(errp,
                       "No keyslots match given (old) password for erase operation");
            return -1;
        }

        if (!force &&
            slot_count == qcrypto_block_luks_count_active_slots(luks)) {
            error_setg(errp,
                       "All the active keyslots match the (old) password that "
                       "was given and erasing them will erase all the data in "
                       "the image irreversibly - refusing operation");
            return -1;
        }

        for (i = 0; i < QCRYPTO_BLOCK_LUKS_NUM_KEY_SLOTS; i++) {
            if (!test_bit(i, &slots_to_erase_bitmap)) {
                continue;
            }
            if (qcrypto_block_luks_erase_key(block, i, writefunc,
                opaque, errp)) {
                error_append_hint(errp, "Failed to erase keyslot %zu", i);
                return -1;
            }
        }
    } else {
        error_setg(errp,
                   "To erase keyslot(s), either explicit keyslot index "
                   "or the password currently contained in them must be given");
        return -1;
    }
    return 0;
}

static int
qcrypto_block_luks_amend_options(QCryptoBlock *block,
                                 QCryptoBlockReadFunc readfunc,
                                 QCryptoBlockWriteFunc writefunc,
                                 void *opaque,
                                 QCryptoBlockAmendOptions *options,
                                 bool force,
                                 Error **errp)
{
    QCryptoBlockAmendOptionsLUKS *opts_luks = &options->u.luks;

    switch (opts_luks->state) {
    case QCRYPTO_BLOCK_LUKS_KEYSLOT_STATE_ACTIVE:
        return qcrypto_block_luks_amend_add_keyslot(block, readfunc,
                                                    writefunc, opaque,
                                                    opts_luks, force, errp);
    case QCRYPTO_BLOCK_LUKS_KEYSLOT_STATE_INACTIVE:
        return qcrypto_block_luks_amend_erase_keyslots(block, readfunc,
                                                       writefunc, opaque,
                                                       opts_luks, force, errp);
    default:
        g_assert_not_reached();
    }
}

static int qcrypto_block_luks_get_info(QCryptoBlock *block,
                                       QCryptoBlockInfo *info,
                                       Error **errp)
{
    QCryptoBlockLUKS *luks = block->opaque;
    QCryptoBlockInfoLUKSSlot *slot;
    QCryptoBlockInfoLUKSSlotList **tail = &info->u.luks.slots;
    size_t i;

    info->u.luks.cipher_alg = luks->cipher_alg;
    info->u.luks.cipher_mode = luks->cipher_mode;
    info->u.luks.ivgen_alg = luks->ivgen_alg;
    if (info->u.luks.ivgen_alg == QCRYPTO_IV_GEN_ALGO_ESSIV) {
        info->u.luks.has_ivgen_hash_alg = true;
        info->u.luks.ivgen_hash_alg = luks->ivgen_hash_alg;
    }
    info->u.luks.hash_alg = luks->hash_alg;
    info->u.luks.payload_offset = block->payload_offset;
    info->u.luks.master_key_iters = luks->header.master_key_iterations;
    info->u.luks.uuid = g_strndup((const char *)luks->header.uuid,
                                  sizeof(luks->header.uuid));
    info->u.luks.detached_header = block->detached_header;

    for (i = 0; i < QCRYPTO_BLOCK_LUKS_NUM_KEY_SLOTS; i++) {
        slot = g_new0(QCryptoBlockInfoLUKSSlot, 1);
        slot->active = luks->header.key_slots[i].active ==
            QCRYPTO_BLOCK_LUKS_KEY_SLOT_ENABLED;
        slot->key_offset = luks->header.key_slots[i].key_offset_sector
             * QCRYPTO_BLOCK_LUKS_SECTOR_SIZE;
        if (slot->active) {
            slot->has_iters = true;
            slot->iters = luks->header.key_slots[i].iterations;
            slot->has_stripes = true;
            slot->stripes = luks->header.key_slots[i].stripes;
        }

        QAPI_LIST_APPEND(tail, slot);
    }

    return 0;
}


static void qcrypto_block_luks_cleanup(QCryptoBlock *block)
{
    QCryptoBlockLUKS *luks = block->opaque;
    if (luks) {
        g_free(luks->secret);
        g_free(luks);
    }
}


static int
qcrypto_block_luks_decrypt(QCryptoBlock *block,
                           uint64_t offset,
                           uint8_t *buf,
                           size_t len,
                           Error **errp)
{
    assert(QEMU_IS_ALIGNED(offset, QCRYPTO_BLOCK_LUKS_SECTOR_SIZE));
    assert(QEMU_IS_ALIGNED(len, QCRYPTO_BLOCK_LUKS_SECTOR_SIZE));
    return qcrypto_block_decrypt_helper(block,
                                        QCRYPTO_BLOCK_LUKS_SECTOR_SIZE,
                                        offset, buf, len, errp);
}


static int
qcrypto_block_luks_encrypt(QCryptoBlock *block,
                           uint64_t offset,
                           uint8_t *buf,
                           size_t len,
                           Error **errp)
{
    assert(QEMU_IS_ALIGNED(offset, QCRYPTO_BLOCK_LUKS_SECTOR_SIZE));
    assert(QEMU_IS_ALIGNED(len, QCRYPTO_BLOCK_LUKS_SECTOR_SIZE));
    return qcrypto_block_encrypt_helper(block,
                                        QCRYPTO_BLOCK_LUKS_SECTOR_SIZE,
                                        offset, buf, len, errp);
}


const QCryptoBlockDriver qcrypto_block_driver_luks = {
    .open = qcrypto_block_luks_open,
    .create = qcrypto_block_luks_create,
    .amend = qcrypto_block_luks_amend_options,
    .get_info = qcrypto_block_luks_get_info,
    .cleanup = qcrypto_block_luks_cleanup,
    .decrypt = qcrypto_block_luks_decrypt,
    .encrypt = qcrypto_block_luks_encrypt,
    .has_format = qcrypto_block_luks_has_format,
};
