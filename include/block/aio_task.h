
#ifndef BLOCK_AIO_TASK_H
#define BLOCK_AIO_TASK_H

typedef struct AioTaskPool AioTaskPool;
typedef struct AioTask AioTask;
typedef int coroutine_fn (*AioTaskFunc)(AioTask *task);
struct AioTask {
    AioTaskPool *pool;
    AioTaskFunc func;
    int ret;
};

AioTaskPool *coroutine_fn aio_task_pool_new(int max_busy_tasks);
void aio_task_pool_free(AioTaskPool *);

int aio_task_pool_status(AioTaskPool *pool);

void coroutine_fn aio_task_pool_start_task(AioTaskPool *pool, AioTask *task);

void coroutine_fn aio_task_pool_wait_slot(AioTaskPool *pool);
void coroutine_fn aio_task_pool_wait_one(AioTaskPool *pool);
void coroutine_fn aio_task_pool_wait_all(AioTaskPool *pool);

#endif /* BLOCK_AIO_TASK_H */
