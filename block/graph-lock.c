
#include "qemu/osdep.h"
#include "qemu/main-loop.h"
#include "block/graph-lock.h"
#include "block/block.h"
#include "block/block_int.h"

BdrvGraphLock graph_lock;

static QemuMutex aio_context_list_lock;

static int has_writer;

static uint32_t orphaned_reader_count;

static CoQueue reader_queue;

struct BdrvGraphRWlock {
    uint32_t reader_count;

    QTAILQ_ENTRY(BdrvGraphRWlock) next_aio;
};

static QTAILQ_HEAD(, BdrvGraphRWlock) aio_context_list =
    QTAILQ_HEAD_INITIALIZER(aio_context_list);

static void __attribute__((__constructor__)) bdrv_init_graph_lock(void)
{
    qemu_mutex_init(&aio_context_list_lock);
    qemu_co_queue_init(&reader_queue);
}

void register_aiocontext(AioContext *ctx)
{
    ctx->bdrv_graph = g_new0(BdrvGraphRWlock, 1);
    QEMU_LOCK_GUARD(&aio_context_list_lock);
    assert(ctx->bdrv_graph->reader_count == 0);
    QTAILQ_INSERT_TAIL(&aio_context_list, ctx->bdrv_graph, next_aio);
}

void unregister_aiocontext(AioContext *ctx)
{
    QEMU_LOCK_GUARD(&aio_context_list_lock);
    orphaned_reader_count += ctx->bdrv_graph->reader_count;
    QTAILQ_REMOVE(&aio_context_list, ctx->bdrv_graph, next_aio);
    g_free(ctx->bdrv_graph);
}

static uint32_t reader_count(void)
{
    BdrvGraphRWlock *brdv_graph;
    uint32_t rd;

    QEMU_LOCK_GUARD(&aio_context_list_lock);

    rd = orphaned_reader_count;
    QTAILQ_FOREACH(brdv_graph, &aio_context_list, next_aio) {
        rd += qatomic_read(&brdv_graph->reader_count);
    }

    assert((int32_t)rd >= 0);
    return rd;
}

void no_coroutine_fn bdrv_graph_wrlock(void)
{
    GLOBAL_STATE_CODE();
    assert(!qatomic_read(&has_writer));
    assert(!qemu_in_coroutine());

    bdrv_drain_all_begin_nopoll();

    do {
        qatomic_set(&has_writer, 0);
        AIO_WAIT_WHILE_UNLOCKED(NULL, reader_count() >= 1);
        qatomic_set(&has_writer, 1);

        smp_mb();
    } while (reader_count() >= 1);

    bdrv_drain_all_end();
}

void no_coroutine_fn bdrv_graph_wrunlock(void)
{
    GLOBAL_STATE_CODE();
    assert(qatomic_read(&has_writer));

    WITH_QEMU_LOCK_GUARD(&aio_context_list_lock) {
        qatomic_store_release(&has_writer, 0);

        qemu_co_enter_all(&reader_queue, &aio_context_list_lock);
    }

    aio_bh_poll(qemu_get_aio_context());
}

void coroutine_fn bdrv_graph_co_rdlock(void)
{
    BdrvGraphRWlock *bdrv_graph;
    bdrv_graph = qemu_get_current_aio_context()->bdrv_graph;

    for (;;) {
        qatomic_set(&bdrv_graph->reader_count,
                    bdrv_graph->reader_count + 1);
        smp_mb();

        if (!qatomic_read(&has_writer)) {
            break;
        }

        WITH_QEMU_LOCK_GUARD(&aio_context_list_lock) {
            if (!qatomic_read(&has_writer)) {
                return;
            }

            bdrv_graph->reader_count--;
            aio_wait_kick();
            qemu_co_queue_wait(&reader_queue, &aio_context_list_lock);
        }
    }
}

void coroutine_fn bdrv_graph_co_rdunlock(void)
{
    BdrvGraphRWlock *bdrv_graph;
    bdrv_graph = qemu_get_current_aio_context()->bdrv_graph;

    qatomic_store_release(&bdrv_graph->reader_count,
                          bdrv_graph->reader_count - 1);
    smp_mb();

    if (qatomic_read(&has_writer)) {
        aio_wait_kick();
    }
}

void bdrv_graph_rdlock_main_loop(void)
{
    GLOBAL_STATE_CODE();
    assert(!qemu_in_coroutine());
}

void bdrv_graph_rdunlock_main_loop(void)
{
    GLOBAL_STATE_CODE();
    assert(!qemu_in_coroutine());
}

void assert_bdrv_graph_readable(void)
{
#ifdef CONFIG_DEBUG_GRAPH_LOCK
    assert(qemu_in_main_thread() || reader_count());
#endif
}

void assert_bdrv_graph_writable(void)
{
    assert(qemu_in_main_thread());
    assert(qatomic_read(&has_writer));
}
