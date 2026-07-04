
#ifndef REQLIST_H
#define REQLIST_H

#include "qemu/coroutine.h"


typedef struct BlockReq {
    int64_t offset;
    int64_t bytes;

    CoQueue wait_queue; /* coroutines blocked on this req */
    QLIST_ENTRY(BlockReq) list;
} BlockReq;

typedef QLIST_HEAD(, BlockReq) BlockReqList;

void reqlist_init_req(BlockReqList *reqs, BlockReq *req, int64_t offset,
                      int64_t bytes);
BlockReq *reqlist_find_conflict(BlockReqList *reqs, int64_t offset,
                                int64_t bytes);

bool coroutine_fn reqlist_wait_one(BlockReqList *reqs, int64_t offset,
                                   int64_t bytes, CoMutex *lock);

void coroutine_fn reqlist_wait_all(BlockReqList *reqs, int64_t offset,
                                   int64_t bytes, CoMutex *lock);

void coroutine_fn reqlist_shrink_req(BlockReq *req, int64_t new_bytes);

void coroutine_fn reqlist_remove_req(BlockReq *req);

#endif /* REQLIST_H */
