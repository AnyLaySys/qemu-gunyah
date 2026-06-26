
#ifndef QCRYPTO_RANDOM_H
#define QCRYPTO_RANDOM_H


int qcrypto_random_bytes(void *buf,
                         size_t buflen,
                         Error **errp);

int qcrypto_random_init(Error **errp);

#endif /* QCRYPTO_RANDOM_H */
