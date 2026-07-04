
#include "qemu/osdep.h"
#include "block/aio.h"

void *qemu_aio_get(const AIOCBInfo *aiocb_info, BlockDriverState *bs,
                   BlockCompletionFunc *cb, void *opaque)
{
    BlockAIOCB *acb;

    acb = g_malloc(aiocb_info->aiocb_size);
    acb->aiocb_info = aiocb_info;
    acb->bs = bs;
    acb->cb = cb;
    acb->opaque = opaque;
    acb->refcnt = 1;
    return acb;
}

void qemu_aio_ref(void *p)
{
    BlockAIOCB *acb = p;
    acb->refcnt++;
}

void qemu_aio_unref(void *p)
{
    BlockAIOCB *acb = p;
    assert(acb->refcnt > 0);
    if (--acb->refcnt == 0) {
        g_free(acb);
    }
}
