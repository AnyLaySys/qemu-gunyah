
#ifndef QEMU_TRANSACTIONS_H
#define QEMU_TRANSACTIONS_H

#include <gmodule.h>

typedef struct TransactionActionDrv {
    void (*abort)(void *opaque);
    void (*commit)(void *opaque);
    void (*clean)(void *opaque);
} TransactionActionDrv;

typedef struct Transaction Transaction;

Transaction *tran_new(void);
void tran_add(Transaction *tran, TransactionActionDrv *drv, void *opaque);
void tran_abort(Transaction *tran);
void tran_commit(Transaction *tran);

static inline void tran_finalize(Transaction *tran, int ret)
{
    if (ret < 0) {
        tran_abort(tran);
    } else {
        tran_commit(tran);
    }
}

#endif /* QEMU_TRANSACTIONS_H */
