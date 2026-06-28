#include "qemu/osdep.h"
#include <sys/random.h>
#include "qapi/error.h"
#include "crypto/block.h"
#include "crypto/hash.h"
#include "crypto/init.h"
#include "crypto/random.h"
#include "crypto/tlssession.h"
#include "crypto/x509-utils.h"
int qcrypto_init(Error **errp){return 0;}
int qcrypto_random_init(Error **errp){return 0;}
int qcrypto_random_bytes(void*buf,size_t buflen,Error**errp){uint8_t*p=buf;while(buflen){ssize_t n=getrandom(p,buflen,0);if(n<0){if(errno==EINTR)continue;error_setg_errno(errp,errno,"getrandom");return -1;}p+=n;buflen-=n;}return 0;}
bool qcrypto_tls_creds_check_endpoint(QCryptoTLSCreds *creds,QCryptoTLSCredsEndpoint endpoint,Error **errp){error_setg(errp,"crypto disabled");return false;}
QCryptoTLSSession*qcrypto_tls_session_new(QCryptoTLSCreds*creds,const char*hostname,const char*aclname,QCryptoTLSCredsEndpoint endpoint,Error**errp){error_setg(errp,"crypto disabled");return NULL;}
void qcrypto_tls_session_free(QCryptoTLSSession*sess){}
int qcrypto_tls_session_check_credentials(QCryptoTLSSession*sess,Error**errp){error_setg(errp,"crypto disabled");return -1;}
void qcrypto_tls_session_set_callbacks(QCryptoTLSSession*sess,QCryptoTLSSessionWriteFunc writeFunc,QCryptoTLSSessionReadFunc readFunc,void*opaque){}
ssize_t qcrypto_tls_session_write(QCryptoTLSSession*sess,const char*buf,size_t len,Error**errp){error_setg(errp,"crypto disabled");return -1;}
ssize_t qcrypto_tls_session_read(QCryptoTLSSession*sess,char*buf,size_t len,bool gracefulTermination,Error**errp){error_setg(errp,"crypto disabled");return -1;}
size_t qcrypto_tls_session_check_pending(QCryptoTLSSession*sess){return 0;}
int qcrypto_tls_session_handshake(QCryptoTLSSession*sess,Error**errp){error_setg(errp,"crypto disabled");return -1;}
int qcrypto_tls_session_bye(QCryptoTLSSession*session,Error**errp){return 0;}
int qcrypto_tls_session_get_key_size(QCryptoTLSSession*sess,Error**errp){error_setg(errp,"crypto disabled");return -1;}
char*qcrypto_tls_session_get_peer_name(QCryptoTLSSession*sess){return NULL;}
gboolean qcrypto_hash_supports(QCryptoHashAlgo alg){return false;}
size_t qcrypto_hash_digest_len(QCryptoHashAlgo alg){return 0;}
int qcrypto_hash_bytesv(QCryptoHashAlgo alg,const struct iovec*iov,size_t niov,uint8_t**result,size_t*resultlen,Error**errp){error_setg(errp,"crypto disabled");return -1;}
int qcrypto_hash_bytes(QCryptoHashAlgo alg,const char*buf,size_t len,uint8_t**result,size_t*resultlen,Error**errp){error_setg(errp,"crypto disabled");return -1;}
int qcrypto_hash_digestv(QCryptoHashAlgo alg,const struct iovec*iov,size_t niov,char**digest,Error**errp){error_setg(errp,"crypto disabled");return -1;}
int qcrypto_hash_updatev(QCryptoHash*hash,const struct iovec*iov,size_t niov,Error**errp){error_setg(errp,"crypto disabled");return -1;}
int qcrypto_hash_update(QCryptoHash*hash,const char*buf,size_t len,Error**errp){error_setg(errp,"crypto disabled");return -1;}
int qcrypto_hash_finalize_digest(QCryptoHash*hash,char**digest,Error**errp){error_setg(errp,"crypto disabled");return -1;}
int qcrypto_hash_finalize_base64(QCryptoHash*hash,char**base64,Error**errp){error_setg(errp,"crypto disabled");return -1;}
int qcrypto_hash_finalize_bytes(QCryptoHash*hash,uint8_t**result,size_t*result_len,Error**errp){error_setg(errp,"crypto disabled");return -1;}
QCryptoHash*qcrypto_hash_new(QCryptoHashAlgo alg,Error**errp){error_setg(errp,"crypto disabled");return NULL;}
void qcrypto_hash_free(QCryptoHash*hash){g_free(hash);}
int qcrypto_hash_digest(QCryptoHashAlgo alg,const char*buf,size_t len,char**digest,Error**errp){error_setg(errp,"crypto disabled");return -1;}
int qcrypto_hash_base64v(QCryptoHashAlgo alg,const struct iovec*iov,size_t niov,char**base64,Error**errp){error_setg(errp,"crypto disabled");return -1;}
int qcrypto_hash_base64(QCryptoHashAlgo alg,const char*buf,size_t len,char**base64,Error**errp){error_setg(errp,"crypto disabled");return -1;}
bool qcrypto_block_has_format(QCryptoBlockFormat format,const uint8_t*buf,size_t buflen){return false;}
QCryptoBlock*qcrypto_block_open(QCryptoBlockOpenOptions*options,const char*optprefix,QCryptoBlockReadFunc readfunc,void*opaque,unsigned int flags,Error**errp){error_setg(errp,"crypto disabled");return NULL;}
QCryptoBlock*qcrypto_block_create(QCryptoBlockCreateOptions*options,const char*optprefix,QCryptoBlockInitFunc initfunc,QCryptoBlockWriteFunc writefunc,void*opaque,unsigned int flags,Error**errp){error_setg(errp,"crypto disabled");return NULL;}
int qcrypto_block_amend_options(QCryptoBlock*block,QCryptoBlockReadFunc readfunc,QCryptoBlockWriteFunc writefunc,void*opaque,QCryptoBlockAmendOptions*options,bool force,Error**errp){error_setg(errp,"crypto disabled");return -1;}
bool qcrypto_block_calculate_payload_offset(QCryptoBlockCreateOptions*create_opts,const char*optprefix,size_t*len,Error**errp){error_setg(errp,"crypto disabled");return false;}
QCryptoBlockInfo*qcrypto_block_get_info(QCryptoBlock*block,Error**errp){return NULL;}
int qcrypto_block_decrypt(QCryptoBlock*block,uint64_t offset,uint8_t*buf,size_t len,Error**errp){error_setg(errp,"crypto disabled");return -1;}
int qcrypto_block_encrypt(QCryptoBlock*block,uint64_t offset,uint8_t*buf,size_t len,Error**errp){error_setg(errp,"crypto disabled");return -1;}
QCryptoCipher*qcrypto_block_get_cipher(QCryptoBlock*block){return NULL;}
QCryptoIVGen*qcrypto_block_get_ivgen(QCryptoBlock*block){return NULL;}
QCryptoHashAlgo qcrypto_block_get_kdf_hash(QCryptoBlock*block){return QCRYPTO_HASH_ALGO__MAX;}
uint64_t qcrypto_block_get_payload_offset(QCryptoBlock*block){return 0;}
uint64_t qcrypto_block_get_sector_size(QCryptoBlock*block){return 512;}
void qcrypto_block_free(QCryptoBlock*block){g_free(block);}
int qcrypto_get_x509_cert_fingerprint(uint8_t*cert,size_t size,QCryptoHashAlgo hash,uint8_t*result,size_t*resultlen,Error**errp){error_setg(errp,"crypto disabled");return -1;}
