
#include "qemu/osdep.h"

#include "qemu/transactions.h"
#include "qemu/queue.h"

typedef struct TransactionAction {
    TransactionActionDrv *drv;
    void *opaque;
    QSLIST_ENTRY(TransactionAction) entry;
} TransactionAction;

struct Transaction {
    QSLIST_HEAD(, TransactionAction) actions;
};

Transaction *tran_new(void)
{
    Transaction *tran = g_new(Transaction, 1);

    QSLIST_INIT(&tran->actions);

    return tran;
}

void tran_add(Transaction *tran, TransactionActionDrv *drv, void *opaque)
{
    TransactionAction *act;

    act = g_new(TransactionAction, 1);
    *act = (TransactionAction) {
        .drv = drv,
        .opaque = opaque
    };

    QSLIST_INSERT_HEAD(&tran->actions, act, entry);
}

void tran_abort(Transaction *tran)
{
    TransactionAction *act, *next;

    QSLIST_FOREACH(act, &tran->actions, entry) {
        if (act->drv->abort) {
            act->drv->abort(act->opaque);
        }
    }

    QSLIST_FOREACH_SAFE(act, &tran->actions, entry, next) {
        if (act->drv->clean) {
            act->drv->clean(act->opaque);
        }

        g_free(act);
    }

    g_free(tran);
}

void tran_commit(Transaction *tran)
{
    TransactionAction *act, *next;

    QSLIST_FOREACH(act, &tran->actions, entry) {
        if (act->drv->commit) {
            act->drv->commit(act->opaque);
        }
    }

    QSLIST_FOREACH_SAFE(act, &tran->actions, entry, next) {
        if (act->drv->clean) {
            act->drv->clean(act->opaque);
        }

        g_free(act);
    }

    g_free(tran);
}
