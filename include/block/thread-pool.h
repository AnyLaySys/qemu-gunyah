
#ifndef QEMU_THREAD_POOL_H
#define QEMU_THREAD_POOL_H

#include "block/aio.h"

#define THREAD_POOL_MAX_THREADS_DEFAULT         64

typedef int ThreadPoolFunc(void *opaque);

typedef struct ThreadPoolAio ThreadPoolAio;

ThreadPoolAio *thread_pool_new_aio(struct AioContext *ctx);
void thread_pool_free_aio(ThreadPoolAio *pool);

BlockAIOCB *thread_pool_submit_aio(ThreadPoolFunc *func, void *arg,
                                   BlockCompletionFunc *cb, void *opaque);
int coroutine_fn thread_pool_submit_co(ThreadPoolFunc *func, void *arg);
void thread_pool_update_params(ThreadPoolAio *pool, struct AioContext *ctx);

typedef struct ThreadPool ThreadPool;

ThreadPool *thread_pool_new(void);

void thread_pool_free(ThreadPool *pool);

void thread_pool_submit(ThreadPool *pool, ThreadPoolFunc *func,
                        void *opaque, GDestroyNotify opaque_destroy);

void thread_pool_submit_immediate(ThreadPool *pool, ThreadPoolFunc *func,
                                  void *opaque, GDestroyNotify opaque_destroy);

void thread_pool_wait(ThreadPool *pool);

bool thread_pool_set_max_threads(ThreadPool *pool, int max_threads);

bool thread_pool_adjust_max_threads_to_work(ThreadPool *pool);

#endif
