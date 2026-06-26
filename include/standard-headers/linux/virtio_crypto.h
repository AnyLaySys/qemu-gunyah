#ifndef _VIRTIO_CRYPTO_H
#define _VIRTIO_CRYPTO_H
#include "standard-headers/linux/types.h"
#include "standard-headers/linux/virtio_types.h"
#include "standard-headers/linux/virtio_ids.h"
#include "standard-headers/linux/virtio_config.h"


#define VIRTIO_CRYPTO_SERVICE_CIPHER 0
#define VIRTIO_CRYPTO_SERVICE_HASH   1
#define VIRTIO_CRYPTO_SERVICE_MAC    2
#define VIRTIO_CRYPTO_SERVICE_AEAD   3
#define VIRTIO_CRYPTO_SERVICE_AKCIPHER 4

#define VIRTIO_CRYPTO_OPCODE(service, op)   (((service) << 8) | (op))

struct virtio_crypto_ctrl_header {
#define VIRTIO_CRYPTO_CIPHER_CREATE_SESSION \
	   VIRTIO_CRYPTO_OPCODE(VIRTIO_CRYPTO_SERVICE_CIPHER, 0x02)
#define VIRTIO_CRYPTO_CIPHER_DESTROY_SESSION \
	   VIRTIO_CRYPTO_OPCODE(VIRTIO_CRYPTO_SERVICE_CIPHER, 0x03)
#define VIRTIO_CRYPTO_HASH_CREATE_SESSION \
	   VIRTIO_CRYPTO_OPCODE(VIRTIO_CRYPTO_SERVICE_HASH, 0x02)
#define VIRTIO_CRYPTO_HASH_DESTROY_SESSION \
	   VIRTIO_CRYPTO_OPCODE(VIRTIO_CRYPTO_SERVICE_HASH, 0x03)
#define VIRTIO_CRYPTO_MAC_CREATE_SESSION \
	   VIRTIO_CRYPTO_OPCODE(VIRTIO_CRYPTO_SERVICE_MAC, 0x02)
#define VIRTIO_CRYPTO_MAC_DESTROY_SESSION \
	   VIRTIO_CRYPTO_OPCODE(VIRTIO_CRYPTO_SERVICE_MAC, 0x03)
#define VIRTIO_CRYPTO_AEAD_CREATE_SESSION \
	   VIRTIO_CRYPTO_OPCODE(VIRTIO_CRYPTO_SERVICE_AEAD, 0x02)
#define VIRTIO_CRYPTO_AEAD_DESTROY_SESSION \
	   VIRTIO_CRYPTO_OPCODE(VIRTIO_CRYPTO_SERVICE_AEAD, 0x03)
#define VIRTIO_CRYPTO_AKCIPHER_CREATE_SESSION \
	   VIRTIO_CRYPTO_OPCODE(VIRTIO_CRYPTO_SERVICE_AKCIPHER, 0x04)
#define VIRTIO_CRYPTO_AKCIPHER_DESTROY_SESSION \
	   VIRTIO_CRYPTO_OPCODE(VIRTIO_CRYPTO_SERVICE_AKCIPHER, 0x05)
	uint32_t opcode;
	uint32_t algo;
	uint32_t flag;
	uint32_t queue_id;
};

struct virtio_crypto_cipher_session_para {
#define VIRTIO_CRYPTO_NO_CIPHER                 0
#define VIRTIO_CRYPTO_CIPHER_ARC4               1
#define VIRTIO_CRYPTO_CIPHER_AES_ECB            2
#define VIRTIO_CRYPTO_CIPHER_AES_CBC            3
#define VIRTIO_CRYPTO_CIPHER_AES_CTR            4
#define VIRTIO_CRYPTO_CIPHER_DES_ECB            5
#define VIRTIO_CRYPTO_CIPHER_DES_CBC            6
#define VIRTIO_CRYPTO_CIPHER_3DES_ECB           7
#define VIRTIO_CRYPTO_CIPHER_3DES_CBC           8
#define VIRTIO_CRYPTO_CIPHER_3DES_CTR           9
#define VIRTIO_CRYPTO_CIPHER_KASUMI_F8          10
#define VIRTIO_CRYPTO_CIPHER_SNOW3G_UEA2        11
#define VIRTIO_CRYPTO_CIPHER_AES_F8             12
#define VIRTIO_CRYPTO_CIPHER_AES_XTS            13
#define VIRTIO_CRYPTO_CIPHER_ZUC_EEA3           14
	uint32_t algo;
	uint32_t keylen;

#define VIRTIO_CRYPTO_OP_ENCRYPT  1
#define VIRTIO_CRYPTO_OP_DECRYPT  2
	uint32_t op;
	uint32_t padding;
};

struct virtio_crypto_session_input {
	uint64_t session_id;
	uint32_t status;
	uint32_t padding;
};

struct virtio_crypto_cipher_session_req {
	struct virtio_crypto_cipher_session_para para;
	uint8_t padding[32];
};

struct virtio_crypto_hash_session_para {
#define VIRTIO_CRYPTO_NO_HASH            0
#define VIRTIO_CRYPTO_HASH_MD5           1
#define VIRTIO_CRYPTO_HASH_SHA1          2
#define VIRTIO_CRYPTO_HASH_SHA_224       3
#define VIRTIO_CRYPTO_HASH_SHA_256       4
#define VIRTIO_CRYPTO_HASH_SHA_384       5
#define VIRTIO_CRYPTO_HASH_SHA_512       6
#define VIRTIO_CRYPTO_HASH_SHA3_224      7
#define VIRTIO_CRYPTO_HASH_SHA3_256      8
#define VIRTIO_CRYPTO_HASH_SHA3_384      9
#define VIRTIO_CRYPTO_HASH_SHA3_512      10
#define VIRTIO_CRYPTO_HASH_SHA3_SHAKE128      11
#define VIRTIO_CRYPTO_HASH_SHA3_SHAKE256      12
	uint32_t algo;
	uint32_t hash_result_len;
	uint8_t padding[8];
};

struct virtio_crypto_hash_create_session_req {
	struct virtio_crypto_hash_session_para para;
	uint8_t padding[40];
};

struct virtio_crypto_mac_session_para {
#define VIRTIO_CRYPTO_NO_MAC                       0
#define VIRTIO_CRYPTO_MAC_HMAC_MD5                 1
#define VIRTIO_CRYPTO_MAC_HMAC_SHA1                2
#define VIRTIO_CRYPTO_MAC_HMAC_SHA_224             3
#define VIRTIO_CRYPTO_MAC_HMAC_SHA_256             4
#define VIRTIO_CRYPTO_MAC_HMAC_SHA_384             5
#define VIRTIO_CRYPTO_MAC_HMAC_SHA_512             6
#define VIRTIO_CRYPTO_MAC_CMAC_3DES                25
#define VIRTIO_CRYPTO_MAC_CMAC_AES                 26
#define VIRTIO_CRYPTO_MAC_KASUMI_F9                27
#define VIRTIO_CRYPTO_MAC_SNOW3G_UIA2              28
#define VIRTIO_CRYPTO_MAC_GMAC_AES                 41
#define VIRTIO_CRYPTO_MAC_GMAC_TWOFISH             42
#define VIRTIO_CRYPTO_MAC_CBCMAC_AES               49
#define VIRTIO_CRYPTO_MAC_CBCMAC_KASUMI_F9         50
#define VIRTIO_CRYPTO_MAC_XCBC_AES                 53
	uint32_t algo;
	uint32_t hash_result_len;
	uint32_t auth_key_len;
	uint32_t padding;
};

struct virtio_crypto_mac_create_session_req {
	struct virtio_crypto_mac_session_para para;
	uint8_t padding[40];
};

struct virtio_crypto_aead_session_para {
#define VIRTIO_CRYPTO_NO_AEAD     0
#define VIRTIO_CRYPTO_AEAD_GCM    1
#define VIRTIO_CRYPTO_AEAD_CCM    2
#define VIRTIO_CRYPTO_AEAD_CHACHA20_POLY1305  3
	uint32_t algo;
	uint32_t key_len;
	uint32_t hash_result_len;
	uint32_t aad_len;
	uint32_t op;
	uint32_t padding;
};

struct virtio_crypto_aead_create_session_req {
	struct virtio_crypto_aead_session_para para;
	uint8_t padding[32];
};

struct virtio_crypto_rsa_session_para {
#define VIRTIO_CRYPTO_RSA_RAW_PADDING   0
#define VIRTIO_CRYPTO_RSA_PKCS1_PADDING 1
	uint32_t padding_algo;

#define VIRTIO_CRYPTO_RSA_NO_HASH   0
#define VIRTIO_CRYPTO_RSA_MD2       1
#define VIRTIO_CRYPTO_RSA_MD3       2
#define VIRTIO_CRYPTO_RSA_MD4       3
#define VIRTIO_CRYPTO_RSA_MD5       4
#define VIRTIO_CRYPTO_RSA_SHA1      5
#define VIRTIO_CRYPTO_RSA_SHA256    6
#define VIRTIO_CRYPTO_RSA_SHA384    7
#define VIRTIO_CRYPTO_RSA_SHA512    8
#define VIRTIO_CRYPTO_RSA_SHA224    9
	uint32_t hash_algo;
};

struct virtio_crypto_ecdsa_session_para {
#define VIRTIO_CRYPTO_CURVE_UNKNOWN   0
#define VIRTIO_CRYPTO_CURVE_NIST_P192 1
#define VIRTIO_CRYPTO_CURVE_NIST_P224 2
#define VIRTIO_CRYPTO_CURVE_NIST_P256 3
#define VIRTIO_CRYPTO_CURVE_NIST_P384 4
#define VIRTIO_CRYPTO_CURVE_NIST_P521 5
	uint32_t curve_id;
	uint32_t padding;
};

struct virtio_crypto_akcipher_session_para {
#define VIRTIO_CRYPTO_NO_AKCIPHER    0
#define VIRTIO_CRYPTO_AKCIPHER_RSA   1
#define VIRTIO_CRYPTO_AKCIPHER_DSA   2
#define VIRTIO_CRYPTO_AKCIPHER_ECDSA 3
	uint32_t algo;

#define VIRTIO_CRYPTO_AKCIPHER_KEY_TYPE_PUBLIC  1
#define VIRTIO_CRYPTO_AKCIPHER_KEY_TYPE_PRIVATE 2
	uint32_t keytype;
	uint32_t keylen;

	union {
		struct virtio_crypto_rsa_session_para rsa;
		struct virtio_crypto_ecdsa_session_para ecdsa;
	} u;
};

struct virtio_crypto_akcipher_create_session_req {
	struct virtio_crypto_akcipher_session_para para;
	uint8_t padding[36];
};

struct virtio_crypto_alg_chain_session_para {
#define VIRTIO_CRYPTO_SYM_ALG_CHAIN_ORDER_HASH_THEN_CIPHER  1
#define VIRTIO_CRYPTO_SYM_ALG_CHAIN_ORDER_CIPHER_THEN_HASH  2
	uint32_t alg_chain_order;
#define VIRTIO_CRYPTO_SYM_HASH_MODE_PLAIN    1
#define VIRTIO_CRYPTO_SYM_HASH_MODE_AUTH     2
#define VIRTIO_CRYPTO_SYM_HASH_MODE_NESTED   3
	uint32_t hash_mode;
	struct virtio_crypto_cipher_session_para cipher_param;
	union {
		struct virtio_crypto_hash_session_para hash_param;
		struct virtio_crypto_mac_session_para mac_param;
		uint8_t padding[16];
	} u;
	uint32_t aad_len;
	uint32_t padding;
};

struct virtio_crypto_alg_chain_session_req {
	struct virtio_crypto_alg_chain_session_para para;
};

struct virtio_crypto_sym_create_session_req {
	union {
		struct virtio_crypto_cipher_session_req cipher;
		struct virtio_crypto_alg_chain_session_req chain;
		uint8_t padding[48];
	} u;


#define VIRTIO_CRYPTO_SYM_OP_NONE  0
#define VIRTIO_CRYPTO_SYM_OP_CIPHER  1
#define VIRTIO_CRYPTO_SYM_OP_ALGORITHM_CHAINING  2
	uint32_t op_type;
	uint32_t padding;
};

struct virtio_crypto_destroy_session_req {
	uint64_t  session_id;
	uint8_t padding[48];
};

struct virtio_crypto_op_ctrl_req {
	struct virtio_crypto_ctrl_header header;

	union {
		struct virtio_crypto_sym_create_session_req
			sym_create_session;
		struct virtio_crypto_hash_create_session_req
			hash_create_session;
		struct virtio_crypto_mac_create_session_req
			mac_create_session;
		struct virtio_crypto_aead_create_session_req
			aead_create_session;
		struct virtio_crypto_akcipher_create_session_req
			akcipher_create_session;
		struct virtio_crypto_destroy_session_req
			destroy_session;
		uint8_t padding[56];
	} u;
};

struct virtio_crypto_op_header {
#define VIRTIO_CRYPTO_CIPHER_ENCRYPT \
	VIRTIO_CRYPTO_OPCODE(VIRTIO_CRYPTO_SERVICE_CIPHER, 0x00)
#define VIRTIO_CRYPTO_CIPHER_DECRYPT \
	VIRTIO_CRYPTO_OPCODE(VIRTIO_CRYPTO_SERVICE_CIPHER, 0x01)
#define VIRTIO_CRYPTO_HASH \
	VIRTIO_CRYPTO_OPCODE(VIRTIO_CRYPTO_SERVICE_HASH, 0x00)
#define VIRTIO_CRYPTO_MAC \
	VIRTIO_CRYPTO_OPCODE(VIRTIO_CRYPTO_SERVICE_MAC, 0x00)
#define VIRTIO_CRYPTO_AEAD_ENCRYPT \
	VIRTIO_CRYPTO_OPCODE(VIRTIO_CRYPTO_SERVICE_AEAD, 0x00)
#define VIRTIO_CRYPTO_AEAD_DECRYPT \
	VIRTIO_CRYPTO_OPCODE(VIRTIO_CRYPTO_SERVICE_AEAD, 0x01)
#define VIRTIO_CRYPTO_AKCIPHER_ENCRYPT \
	VIRTIO_CRYPTO_OPCODE(VIRTIO_CRYPTO_SERVICE_AKCIPHER, 0x00)
#define VIRTIO_CRYPTO_AKCIPHER_DECRYPT \
	VIRTIO_CRYPTO_OPCODE(VIRTIO_CRYPTO_SERVICE_AKCIPHER, 0x01)
#define VIRTIO_CRYPTO_AKCIPHER_SIGN \
	VIRTIO_CRYPTO_OPCODE(VIRTIO_CRYPTO_SERVICE_AKCIPHER, 0x02)
#define VIRTIO_CRYPTO_AKCIPHER_VERIFY \
	VIRTIO_CRYPTO_OPCODE(VIRTIO_CRYPTO_SERVICE_AKCIPHER, 0x03)
	uint32_t opcode;
	uint32_t algo;
	uint64_t session_id;
	uint32_t flag;
	uint32_t padding;
};

struct virtio_crypto_cipher_para {
	uint32_t iv_len;
	uint32_t src_data_len;
	uint32_t dst_data_len;
	uint32_t padding;
};

struct virtio_crypto_hash_para {
	uint32_t src_data_len;
	uint32_t hash_result_len;
};

struct virtio_crypto_mac_para {
	struct virtio_crypto_hash_para hash;
};

struct virtio_crypto_aead_para {
	uint32_t iv_len;
	uint32_t aad_len;
	uint32_t src_data_len;
	uint32_t dst_data_len;
};

struct virtio_crypto_cipher_data_req {
	struct virtio_crypto_cipher_para para;
	uint8_t padding[24];
};

struct virtio_crypto_hash_data_req {
	struct virtio_crypto_hash_para para;
	uint8_t padding[40];
};

struct virtio_crypto_mac_data_req {
	struct virtio_crypto_mac_para para;
	uint8_t padding[40];
};

struct virtio_crypto_alg_chain_data_para {
	uint32_t iv_len;
	uint32_t src_data_len;
	uint32_t dst_data_len;
	uint32_t cipher_start_src_offset;
	uint32_t len_to_cipher;
	uint32_t hash_start_src_offset;
	uint32_t len_to_hash;
	uint32_t aad_len;
	uint32_t hash_result_len;
	uint32_t reserved;
};

struct virtio_crypto_alg_chain_data_req {
	struct virtio_crypto_alg_chain_data_para para;
};

struct virtio_crypto_sym_data_req {
	union {
		struct virtio_crypto_cipher_data_req cipher;
		struct virtio_crypto_alg_chain_data_req chain;
		uint8_t padding[40];
	} u;

	uint32_t op_type;
	uint32_t padding;
};

struct virtio_crypto_aead_data_req {
	struct virtio_crypto_aead_para para;
	uint8_t padding[32];
};

struct virtio_crypto_akcipher_para {
	uint32_t src_data_len;
	uint32_t dst_data_len;
};

struct virtio_crypto_akcipher_data_req {
	struct virtio_crypto_akcipher_para para;
	uint8_t padding[40];
};

struct virtio_crypto_op_data_req {
	struct virtio_crypto_op_header header;

	union {
		struct virtio_crypto_sym_data_req  sym_req;
		struct virtio_crypto_hash_data_req hash_req;
		struct virtio_crypto_mac_data_req mac_req;
		struct virtio_crypto_aead_data_req aead_req;
		struct virtio_crypto_akcipher_data_req akcipher_req;
		uint8_t padding[48];
	} u;
};

#define VIRTIO_CRYPTO_OK        0
#define VIRTIO_CRYPTO_ERR       1
#define VIRTIO_CRYPTO_BADMSG    2
#define VIRTIO_CRYPTO_NOTSUPP   3
#define VIRTIO_CRYPTO_INVSESS   4 /* Invalid session id */
#define VIRTIO_CRYPTO_NOSPC     5 /* no free session ID */
#define VIRTIO_CRYPTO_KEY_REJECTED 6 /* Signature verification failed */

#define VIRTIO_CRYPTO_S_HW_READY  (1 << 0)

struct virtio_crypto_config {
	uint32_t  status;

	uint32_t  max_dataqueues;

	uint32_t crypto_services;

	uint32_t cipher_algo_l;
	uint32_t cipher_algo_h;
	uint32_t hash_algo;
	uint32_t mac_algo_l;
	uint32_t mac_algo_h;
	uint32_t aead_algo;
	uint32_t max_cipher_key_len;
	uint32_t max_auth_key_len;
	uint32_t akcipher_algo;
	uint64_t max_size;
};

struct virtio_crypto_inhdr {
	uint8_t status;
};
#endif
