
#include "qemu/osdep.h"

#define ZLIB_CONST
#include <zlib.h>

#ifdef CONFIG_ZSTD
#include <zstd.h>
#include <zstd_errors.h>
#endif

#include "qcow2.h"
#include "block/block-io.h"
#include "block/thread-pool.h"

static int coroutine_fn
qcow2_co_process(BlockDriverState *bs, ThreadPoolFunc *func, void *arg)
{
    int ret;
    BDRVQcow2State *s = bs->opaque;

    qemu_co_mutex_lock(&s->lock);
    while (s->nb_threads >= QCOW2_MAX_THREADS) {
        qemu_co_queue_wait(&s->thread_task_queue, &s->lock);
    }
    s->nb_threads++;
    qemu_co_mutex_unlock(&s->lock);

    ret = thread_pool_submit_co(func, arg);

    qemu_co_mutex_lock(&s->lock);
    s->nb_threads--;
    qemu_co_queue_next(&s->thread_task_queue);
    qemu_co_mutex_unlock(&s->lock);

    return ret;
}



typedef ssize_t (*Qcow2CompressFunc)(void *dest, size_t dest_size,
                                     const void *src, size_t src_size);
typedef struct Qcow2CompressData {
    void *dest;
    size_t dest_size;
    const void *src;
    size_t src_size;
    ssize_t ret;

    Qcow2CompressFunc func;
} Qcow2CompressData;

static ssize_t qcow2_zlib_compress(void *dest, size_t dest_size,
                                   const void *src, size_t src_size)
{
    ssize_t ret;
    z_stream strm;

    memset(&strm, 0, sizeof(strm));
    ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                       -12, 9, Z_DEFAULT_STRATEGY);
    if (ret != Z_OK) {
        return -EIO;
    }

    strm.avail_in = src_size;
    strm.next_in = (void *) src;
    strm.avail_out = dest_size;
    strm.next_out = dest;

    ret = deflate(&strm, Z_FINISH);
    if (ret == Z_STREAM_END) {
        ret = dest_size - strm.avail_out;
    } else {
        ret = (ret == Z_OK ? -ENOMEM : -EIO);
    }

    deflateEnd(&strm);

    return ret;
}

static ssize_t qcow2_zlib_decompress(void *dest, size_t dest_size,
                                     const void *src, size_t src_size)
{
    int ret;
    z_stream strm;

    memset(&strm, 0, sizeof(strm));
    strm.avail_in = src_size;
    strm.next_in = (void *) src;
    strm.avail_out = dest_size;
    strm.next_out = dest;

    ret = inflateInit2(&strm, -12);
    if (ret != Z_OK) {
        return -EIO;
    }

    ret = inflate(&strm, Z_FINISH);
    if ((ret == Z_STREAM_END || ret == Z_BUF_ERROR) && strm.avail_out == 0) {
        ret = 0;
    } else {
        ret = -EIO;
    }

    inflateEnd(&strm);

    return ret;
}

#ifdef CONFIG_ZSTD

static ssize_t qcow2_zstd_compress(void *dest, size_t dest_size,
                                   const void *src, size_t src_size)
{
    ssize_t ret;
    size_t zstd_ret;
    ZSTD_outBuffer output = {
        .dst = dest,
        .size = dest_size,
        .pos = 0
    };
    ZSTD_inBuffer input = {
        .src = src,
        .size = src_size,
        .pos = 0
    };
    ZSTD_CCtx *cctx = ZSTD_createCCtx();

    if (!cctx) {
        return -EIO;
    }
    zstd_ret = ZSTD_compressStream2(cctx, &output, &input, ZSTD_e_end);

    if (zstd_ret) {
        if (zstd_ret > output.size - output.pos) {
            ret = -ENOMEM;
        } else {
            ret = -EIO;
        }
        goto out;
    }

    assert(output.pos <= dest_size);
    ret = output.pos;
out:
    ZSTD_freeCCtx(cctx);
    return ret;
}

static ssize_t qcow2_zstd_decompress(void *dest, size_t dest_size,
                                     const void *src, size_t src_size)
{
    size_t zstd_ret = 0;
    ssize_t ret = 0;
    ZSTD_outBuffer output = {
        .dst = dest,
        .size = dest_size,
        .pos = 0
    };
    ZSTD_inBuffer input = {
        .src = src,
        .size = src_size,
        .pos = 0
    };
    ZSTD_DCtx *dctx = ZSTD_createDCtx();

    if (!dctx) {
        return -EIO;
    }

    while (output.pos < output.size) {
        size_t last_in_pos = input.pos;
        size_t last_out_pos = output.pos;
        zstd_ret = ZSTD_decompressStream(dctx, &output, &input);

        if (ZSTD_isError(zstd_ret)) {
            ret = -EIO;
            break;
        }

        if (last_in_pos >= input.pos &&
            last_out_pos >= output.pos) {
            ret = -EIO;
            break;
        }
    }
    if (zstd_ret > 0) {
        ret = -EIO;
    }

    ZSTD_freeDCtx(dctx);
    assert(ret == 0 || ret == -EIO);
    return ret;
}
#endif

static int qcow2_compress_pool_func(void *opaque)
{
    Qcow2CompressData *data = opaque;

    data->ret = data->func(data->dest, data->dest_size,
                           data->src, data->src_size);

    return 0;
}

static ssize_t coroutine_fn
qcow2_co_do_compress(BlockDriverState *bs, void *dest, size_t dest_size,
                     const void *src, size_t src_size, Qcow2CompressFunc func)
{
    Qcow2CompressData arg = {
        .dest = dest,
        .dest_size = dest_size,
        .src = src,
        .src_size = src_size,
        .func = func,
    };

    qcow2_co_process(bs, qcow2_compress_pool_func, &arg);

    return arg.ret;
}

ssize_t coroutine_fn
qcow2_co_compress(BlockDriverState *bs, void *dest, size_t dest_size,
                  const void *src, size_t src_size)
{
    BDRVQcow2State *s = bs->opaque;
    Qcow2CompressFunc fn;

    switch (s->compression_type) {
    case QCOW2_COMPRESSION_TYPE_ZLIB:
        fn = qcow2_zlib_compress;
        break;

#ifdef CONFIG_ZSTD
    case QCOW2_COMPRESSION_TYPE_ZSTD:
        fn = qcow2_zstd_compress;
        break;
#endif
    default:
        abort();
    }

    return qcow2_co_do_compress(bs, dest, dest_size, src, src_size, fn);
}

ssize_t coroutine_fn
qcow2_co_decompress(BlockDriverState *bs, void *dest, size_t dest_size,
                    const void *src, size_t src_size)
{
    BDRVQcow2State *s = bs->opaque;
    Qcow2CompressFunc fn;

    switch (s->compression_type) {
    case QCOW2_COMPRESSION_TYPE_ZLIB:
        fn = qcow2_zlib_decompress;
        break;

#ifdef CONFIG_ZSTD
    case QCOW2_COMPRESSION_TYPE_ZSTD:
        fn = qcow2_zstd_decompress;
        break;
#endif
    default:
        abort();
    }

    return qcow2_co_do_compress(bs, dest, dest_size, src, src_size, fn);
}
