
#include "qemu/osdep.h"
#include "block/trace.h"
#include "block/block_int.h"
#include "block/qdict.h"
#include "qemu/error-report.h"
#include "block/module_block.h"
#include "qemu/main-loop.h"
#include "qemu/module.h"
#include "qapi/error.h"
#include "qobject/qdict.h"
#include "qobject/qjson.h"
#include "qobject/qnull.h"
#include "qobject/qstring.h"
#include "qapi/qobject-output-visitor.h"
#include "qapi/qapi-visit-block-core.h"
#include "system/block-backend.h"
#include "qemu/notify.h"
#include "qemu/option.h"
#include "qemu/coroutine.h"
#include "qemu/timer.h"
#include "qemu/cutils.h"
#include "qemu/id.h"
#include "qemu/range.h"
#include "qemu/rcu.h"
#include "block/coroutines.h"

#ifdef CONFIG_BSD
#include <sys/ioctl.h>
#include <sys/queue.h>
#if defined(HAVE_SYS_DISK_H)
#include <sys/disk.h>
#endif
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#define NOT_DONE 0x7fffffff /* used while emulated sync operation in progress */

static QTAILQ_HEAD(, BlockDriverState) graph_bdrv_states =
    QTAILQ_HEAD_INITIALIZER(graph_bdrv_states);

static QTAILQ_HEAD(, BlockDriverState) all_bdrv_states =
    QTAILQ_HEAD_INITIALIZER(all_bdrv_states);

static QLIST_HEAD(, BlockDriver) bdrv_drivers =
    QLIST_HEAD_INITIALIZER(bdrv_drivers);

static BlockDriverState *bdrv_open_inherit(const char *filename,
                                           const char *reference,
                                           QDict *options, int flags,
                                           BlockDriverState *parent,
                                           const BdrvChildClass *child_class,
                                           BdrvChildRole child_role,
                                           bool parse_filename,
                                           Error **errp);

static bool bdrv_recurse_has_child(BlockDriverState *bs,
                                   BlockDriverState *child);

static void GRAPH_WRLOCK
bdrv_replace_child_noperm(BdrvChild *child, BlockDriverState *new_bs);

static void GRAPH_WRLOCK
bdrv_remove_child(BdrvChild *child, Transaction *tran);

static int bdrv_reopen_prepare(BDRVReopenState *reopen_state,
                               BlockReopenQueue *queue,
                               Transaction *change_child_tran, Error **errp);
static void bdrv_reopen_commit(BDRVReopenState *reopen_state);
static void bdrv_reopen_abort(BDRVReopenState *reopen_state);

static bool bdrv_backing_overridden(BlockDriverState *bs);

static bool bdrv_change_aio_context(BlockDriverState *bs, AioContext *ctx,
                                    GHashTable *visited, Transaction *tran,
                                    Error **errp);

static int use_bdrv_whitelist;

#ifdef _WIN32
static int is_windows_drive_prefix(const char *filename)
{
    return (((filename[0] >= 'a' && filename[0] <= 'z') ||
             (filename[0] >= 'A' && filename[0] <= 'Z')) &&
            filename[1] == ':');
}

int is_windows_drive(const char *filename)
{
    if (is_windows_drive_prefix(filename) &&
        filename[2] == '\0')
        return 1;
    if (strstart(filename, "\\\\.\\", NULL) ||
        strstart(filename, "//./", NULL))
        return 1;
    return 0;
}
#endif

size_t bdrv_opt_mem_align(BlockDriverState *bs)
{
    if (!bs || !bs->drv) {
        return MAX(4096, qemu_real_host_page_size());
    }
    IO_CODE();

    return bs->bl.opt_mem_alignment;
}

size_t bdrv_min_mem_align(BlockDriverState *bs)
{
    if (!bs || !bs->drv) {
        return MAX(4096, qemu_real_host_page_size());
    }
    IO_CODE();

    return bs->bl.min_mem_alignment;
}

int path_has_protocol(const char *path)
{
    const char *p;

#ifdef _WIN32
    if (is_windows_drive(path) ||
        is_windows_drive_prefix(path)) {
        return 0;
    }
    p = path + strcspn(path, ":/\\");
#else
    p = path + strcspn(path, ":/");
#endif

    return *p == ':';
}

int path_is_absolute(const char *path)
{
#ifdef _WIN32
    if (is_windows_drive(path) || is_windows_drive_prefix(path)) {
        return 1;
    }
    return (*path == '/' || *path == '\\');
#else
    return (*path == '/');
#endif
}

char *path_combine(const char *base_path, const char *filename)
{
    const char *protocol_stripped = NULL;
    const char *p, *p1;
    char *result;
    int len;

    if (path_is_absolute(filename)) {
        return g_strdup(filename);
    }

    if (path_has_protocol(base_path)) {
        protocol_stripped = strchr(base_path, ':');
        if (protocol_stripped) {
            protocol_stripped++;
        }
    }
    p = protocol_stripped ?: base_path;

    p1 = strrchr(base_path, '/');
#ifdef _WIN32
    {
        const char *p2;
        p2 = strrchr(base_path, '\\');
        if (!p1 || p2 > p1) {
            p1 = p2;
        }
    }
#endif
    if (p1) {
        p1++;
    } else {
        p1 = base_path;
    }
    if (p1 > p) {
        p = p1;
    }
    len = p - base_path;

    result = g_malloc(len + strlen(filename) + 1);
    memcpy(result, base_path, len);
    strcpy(result + len, filename);

    return result;
}

void bdrv_parse_filename_strip_prefix(const char *filename, const char *prefix,
                                      QDict *options)
{
    if (strstart(filename, prefix, &filename)) {
        if (path_has_protocol(filename)) {
            GString *fat_filename;

            assert(!path_is_absolute(filename));

            fat_filename = g_string_new("./");
            g_string_append(fat_filename, filename);

            assert(!path_has_protocol(fat_filename->str));

            qdict_put(options, "filename",
                      qstring_from_gstring(fat_filename));
        } else {
            qdict_put_str(options, "filename", filename);
        }
    }
}


bool bdrv_is_read_only(BlockDriverState *bs)
{
    IO_CODE();
    return !(bs->open_flags & BDRV_O_RDWR);
}

static int GRAPH_RDLOCK
bdrv_can_set_read_only(BlockDriverState *bs, bool read_only,
                       bool ignore_allow_rdw, Error **errp)
{
    IO_CODE();

    if (bs->copy_on_read && read_only) {
        error_setg(errp, "Can't set node '%s' to r/o with copy-on-read enabled",
                   bdrv_get_device_or_node_name(bs));
        return -EINVAL;
    }

    if (!read_only && !(bs->open_flags & BDRV_O_ALLOW_RDWR) &&
        !ignore_allow_rdw)
    {
        error_setg(errp, "Node '%s' is read only",
                   bdrv_get_device_or_node_name(bs));
        return -EPERM;
    }

    return 0;
}

int bdrv_apply_auto_read_only(BlockDriverState *bs, const char *errmsg,
                              Error **errp)
{
    int ret = 0;
    IO_CODE();

    if (!(bs->open_flags & BDRV_O_RDWR)) {
        return 0;
    }
    if (!(bs->open_flags & BDRV_O_AUTO_RDONLY)) {
        goto fail;
    }

    ret = bdrv_can_set_read_only(bs, true, false, NULL);
    if (ret < 0) {
        goto fail;
    }

    bs->open_flags &= ~BDRV_O_RDWR;

    return 0;

fail:
    error_setg(errp, "%s", errmsg ?: "Image is read-only");
    return -EACCES;
}

char *bdrv_get_full_backing_filename_from_filename(const char *backed,
                                                   const char *backing,
                                                   Error **errp)
{
    if (backing[0] == '\0') {
        return NULL;
    } else if (path_has_protocol(backing) || path_is_absolute(backing)) {
        return g_strdup(backing);
    } else if (backed[0] == '\0' || strstart(backed, "json:", NULL)) {
        error_setg(errp, "Cannot use relative backing file names for '%s'",
                   backed);
        return NULL;
    } else {
        return path_combine(backed, backing);
    }
}

static char * GRAPH_RDLOCK
bdrv_make_absolute_filename(BlockDriverState *relative_to,
                            const char *filename, Error **errp)
{
    char *dir, *full_name;

    if (!filename || filename[0] == '\0') {
        return NULL;
    } else if (path_has_protocol(filename) || path_is_absolute(filename)) {
        return g_strdup(filename);
    }

    dir = bdrv_dirname(relative_to, errp);
    if (!dir) {
        return NULL;
    }

    full_name = g_strconcat(dir, filename, NULL);
    g_free(dir);
    return full_name;
}

char *bdrv_get_full_backing_filename(BlockDriverState *bs, Error **errp)
{
    GLOBAL_STATE_CODE();
    return bdrv_make_absolute_filename(bs, bs->backing_file, errp);
}

void bdrv_register(BlockDriver *bdrv)
{
    assert(bdrv->format_name);
    GLOBAL_STATE_CODE();
    QLIST_INSERT_HEAD(&bdrv_drivers, bdrv, list);
}

BlockDriverState *bdrv_new(void)
{
    BlockDriverState *bs;
    int i;

    GLOBAL_STATE_CODE();

    bs = g_new0(BlockDriverState, 1);
    for (i = 0; i < BLOCK_OP_TYPE_MAX; i++) {
        QLIST_INIT(&bs->op_blockers[i]);
    }
    qemu_mutex_init(&bs->reqs_lock);
    bs->refcnt = 1;
    bs->aio_context = qemu_get_aio_context();

    qemu_co_queue_init(&bs->flush_queue);

    qemu_co_mutex_init(&bs->bsc_modify_lock);
    bs->block_status_cache = g_new0(BdrvBlockStatusCache, 1);

    for (i = 0; i < bdrv_drain_all_count; i++) {
        bdrv_drained_begin(bs);
    }

    QTAILQ_INSERT_TAIL(&all_bdrv_states, bs, bs_list);

    return bs;
}

static BlockDriver *bdrv_do_find_format(const char *format_name)
{
    BlockDriver *drv1;
    GLOBAL_STATE_CODE();

    QLIST_FOREACH(drv1, &bdrv_drivers, list) {
        if (!strcmp(drv1->format_name, format_name)) {
            return drv1;
        }
    }

    return NULL;
}

BlockDriver *bdrv_find_format(const char *format_name)
{
    BlockDriver *drv1;
    int i;

    GLOBAL_STATE_CODE();

    drv1 = bdrv_do_find_format(format_name);
    if (drv1) {
        return drv1;
    }

    for (i = 0; i < (int)ARRAY_SIZE(block_driver_modules); ++i) {
        if (!strcmp(block_driver_modules[i].format_name, format_name)) {
            Error *local_err = NULL;
            int rv = block_module_load(block_driver_modules[i].library_name,
                                       &local_err);
            if (rv > 0) {
                return bdrv_do_find_format(format_name);
            } else if (rv < 0) {
                error_report_err(local_err);
            }
            break;
        }
    }
    return NULL;
}

static int bdrv_format_is_whitelisted(const char *format_name, bool read_only)
{
    static const char *whitelist_rw[] = {
        CONFIG_BDRV_RW_WHITELIST
        NULL
    };
    static const char *whitelist_ro[] = {
        CONFIG_BDRV_RO_WHITELIST
        NULL
    };
    const char **p;

    if (!whitelist_rw[0] && !whitelist_ro[0]) {
        return 1;               /* no whitelist, anything goes */
    }

    for (p = whitelist_rw; *p; p++) {
        if (!strcmp(format_name, *p)) {
            return 1;
        }
    }
    if (read_only) {
        for (p = whitelist_ro; *p; p++) {
            if (!strcmp(format_name, *p)) {
                return 1;
            }
        }
    }
    return 0;
}

int bdrv_is_whitelisted(BlockDriver *drv, bool read_only)
{
    GLOBAL_STATE_CODE();
    return bdrv_format_is_whitelisted(drv->format_name, read_only);
}

bool bdrv_uses_whitelist(void)
{
    return use_bdrv_whitelist;
}

typedef struct CreateCo {
    BlockDriver *drv;
    char *filename;
    QemuOpts *opts;
    int ret;
    Error *err;
} CreateCo;

int coroutine_fn bdrv_co_create(BlockDriver *drv, const char *filename,
                                QemuOpts *opts, Error **errp)
{
    ERRP_GUARD();
    int ret;
    GLOBAL_STATE_CODE();

    if (!drv->bdrv_co_create_opts) {
        error_setg(errp, "Driver '%s' does not support image creation",
                   drv->format_name);
        return -ENOTSUP;
    }

    ret = drv->bdrv_co_create_opts(drv, filename, opts, errp);
    if (ret < 0 && !*errp) {
        error_setg_errno(errp, -ret, "Could not create image");
    }

    return ret;
}

static int64_t coroutine_fn GRAPH_UNLOCKED
create_file_fallback_truncate(BlockBackend *blk, int64_t minimum_size,
                              Error **errp)
{
    Error *local_err = NULL;
    int64_t size;
    int ret;

    GLOBAL_STATE_CODE();

    ret = blk_co_truncate(blk, minimum_size, false, PREALLOC_MODE_OFF, 0,
                          &local_err);
    if (ret < 0 && ret != -ENOTSUP) {
        error_propagate(errp, local_err);
        return ret;
    }

    size = blk_co_getlength(blk);
    if (size < 0) {
        error_free(local_err);
        error_setg_errno(errp, -size,
                         "Failed to inquire the new image file's length");
        return size;
    }

    if (size < minimum_size) {
        error_propagate(errp, local_err);
        return -ENOTSUP;
    }

    error_free(local_err);
    local_err = NULL;

    return size;
}

static int coroutine_fn
create_file_fallback_zero_first_sector(BlockBackend *blk,
                                       int64_t current_size,
                                       Error **errp)
{
    int64_t bytes_to_clear;
    int ret;

    GLOBAL_STATE_CODE();

    bytes_to_clear = MIN(current_size, BDRV_SECTOR_SIZE);
    if (bytes_to_clear) {
        ret = blk_co_pwrite_zeroes(blk, 0, bytes_to_clear, BDRV_REQ_MAY_UNMAP);
        if (ret < 0) {
            error_setg_errno(errp, -ret,
                             "Failed to clear the new image's first sector");
            return ret;
        }
    }

    return 0;
}

int coroutine_fn bdrv_co_create_opts_simple(BlockDriver *drv,
                                            const char *filename,
                                            QemuOpts *opts,
                                            Error **errp)
{
    ERRP_GUARD();
    BlockBackend *blk;
    QDict *options;
    int64_t size = 0;
    char *buf = NULL;
    PreallocMode prealloc;
    Error *local_err = NULL;
    int ret;

    GLOBAL_STATE_CODE();

    size = qemu_opt_get_size_del(opts, BLOCK_OPT_SIZE, 0);
    buf = qemu_opt_get_del(opts, BLOCK_OPT_PREALLOC);
    prealloc = qapi_enum_parse(&PreallocMode_lookup, buf,
                               PREALLOC_MODE_OFF, &local_err);
    g_free(buf);
    if (local_err) {
        error_propagate(errp, local_err);
        return -EINVAL;
    }

    if (prealloc != PREALLOC_MODE_OFF) {
        error_setg(errp, "Unsupported preallocation mode '%s'",
                   PreallocMode_str(prealloc));
        return -ENOTSUP;
    }

    options = qdict_new();
    qdict_put_str(options, "driver", drv->format_name);

    blk = blk_co_new_open(filename, NULL, options,
                          BDRV_O_RDWR | BDRV_O_RESIZE, errp);
    if (!blk) {
        error_prepend(errp, "Protocol driver '%s' does not support creating "
                      "new images, so an existing image must be selected as "
                      "the target; however, opening the given target as an "
                      "existing image failed: ",
                      drv->format_name);
        return -EINVAL;
    }

    size = create_file_fallback_truncate(blk, size, errp);
    if (size < 0) {
        ret = size;
        goto out;
    }

    ret = create_file_fallback_zero_first_sector(blk, size, errp);
    if (ret < 0) {
        goto out;
    }

    ret = 0;
out:
    blk_co_unref(blk);
    return ret;
}

int coroutine_fn bdrv_co_create_file(const char *filename, QemuOpts *opts,
                                     Error **errp)
{
    QemuOpts *protocol_opts;
    BlockDriver *drv;
    QDict *qdict;
    int ret;

    GLOBAL_STATE_CODE();

    drv = bdrv_find_protocol(filename, true, errp);
    if (drv == NULL) {
        return -ENOENT;
    }

    if (!drv->create_opts) {
        error_setg(errp, "Driver '%s' does not support image creation",
                   drv->format_name);
        return -ENOTSUP;
    }

    qdict = qemu_opts_to_qdict(opts, NULL);
    protocol_opts = qemu_opts_from_qdict(drv->create_opts, qdict, errp);
    if (protocol_opts == NULL) {
        ret = -EINVAL;
        goto out;
    }

    ret = bdrv_co_create(drv, filename, protocol_opts, errp);
out:
    qemu_opts_del(protocol_opts);
    qobject_unref(qdict);
    return ret;
}

int coroutine_fn bdrv_co_delete_file(BlockDriverState *bs, Error **errp)
{
    Error *local_err = NULL;
    int ret;

    IO_CODE();
    assert(bs != NULL);
    assert_bdrv_graph_readable();

    if (!bs->drv) {
        error_setg(errp, "Block node '%s' is not opened", bs->filename);
        return -ENOMEDIUM;
    }

    if (!bs->drv->bdrv_co_delete_file) {
        error_setg(errp, "Driver '%s' does not support image deletion",
                   bs->drv->format_name);
        return -ENOTSUP;
    }

    ret = bs->drv->bdrv_co_delete_file(bs, &local_err);
    if (ret < 0) {
        error_propagate(errp, local_err);
    }

    return ret;
}

void coroutine_fn bdrv_co_delete_file_noerr(BlockDriverState *bs)
{
    Error *local_err = NULL;
    int ret;
    IO_CODE();

    if (!bs) {
        return;
    }

    ret = bdrv_co_delete_file(bs, &local_err);
    if (ret == -ENOTSUP) {
        error_free(local_err);
    } else if (ret < 0) {
        error_report_err(local_err);
    }
}

int bdrv_probe_blocksizes(BlockDriverState *bs, BlockSizes *bsz)
{
    BlockDriver *drv = bs->drv;
    BlockDriverState *filtered = bdrv_filter_bs(bs);
    GLOBAL_STATE_CODE();

    if (drv && drv->bdrv_probe_blocksizes) {
        return drv->bdrv_probe_blocksizes(bs, bsz);
    } else if (filtered) {
        return bdrv_probe_blocksizes(filtered, bsz);
    }

    return -ENOTSUP;
}

int bdrv_probe_geometry(BlockDriverState *bs, HDGeometry *geo)
{
    BlockDriver *drv = bs->drv;
    BlockDriverState *filtered;

    GLOBAL_STATE_CODE();
    GRAPH_RDLOCK_GUARD_MAINLOOP();

    if (drv && drv->bdrv_probe_geometry) {
        return drv->bdrv_probe_geometry(bs, geo);
    }

    filtered = bdrv_filter_bs(bs);
    if (filtered) {
        return bdrv_probe_geometry(filtered, geo);
    }

    return -ENOTSUP;
}

char *create_tmp_file(Error **errp)
{
    int fd;
    const char *tmpdir;
    g_autofree char *filename = NULL;

    tmpdir = g_get_tmp_dir();
#ifndef _WIN32
    if (!g_strcmp0(tmpdir, "/tmp")) {
        tmpdir = "/var/tmp";
    }
#endif

    filename = g_strdup_printf("%s/vl.XXXXXX", tmpdir);
    fd = g_mkstemp(filename);
    if (fd < 0) {
        error_setg_errno(errp, errno, "Could not open temporary file '%s'",
                         filename);
        return NULL;
    }
    close(fd);

    return g_steal_pointer(&filename);
}

static BlockDriver *find_hdev_driver(const char *filename)
{
    int score_max = 0, score;
    BlockDriver *drv = NULL, *d;
    GLOBAL_STATE_CODE();

    QLIST_FOREACH(d, &bdrv_drivers, list) {
        if (d->bdrv_probe_device) {
            score = d->bdrv_probe_device(filename);
            if (score > score_max) {
                score_max = score;
                drv = d;
            }
        }
    }

    return drv;
}

static BlockDriver *bdrv_do_find_protocol(const char *protocol)
{
    BlockDriver *drv1;
    GLOBAL_STATE_CODE();

    QLIST_FOREACH(drv1, &bdrv_drivers, list) {
        if (drv1->protocol_name && !strcmp(drv1->protocol_name, protocol)) {
            return drv1;
        }
    }

    return NULL;
}

BlockDriver *bdrv_find_protocol(const char *filename,
                                bool allow_protocol_prefix,
                                Error **errp)
{
    BlockDriver *drv1;
    char protocol[128];
    int len;
    const char *p;
    int i;

    GLOBAL_STATE_CODE();

    drv1 = find_hdev_driver(filename);
    if (drv1) {
        return drv1;
    }

    if (!path_has_protocol(filename) || !allow_protocol_prefix) {
        return &bdrv_file;
    }

    p = strchr(filename, ':');
    assert(p != NULL);
    len = p - filename;
    if (len > sizeof(protocol) - 1)
        len = sizeof(protocol) - 1;
    memcpy(protocol, filename, len);
    protocol[len] = '\0';

    drv1 = bdrv_do_find_protocol(protocol);
    if (drv1) {
        return drv1;
    }

    for (i = 0; i < (int)ARRAY_SIZE(block_driver_modules); ++i) {
        if (block_driver_modules[i].protocol_name &&
            !strcmp(block_driver_modules[i].protocol_name, protocol)) {
            int rv = block_module_load(block_driver_modules[i].library_name, errp);
            if (rv > 0) {
                drv1 = bdrv_do_find_protocol(protocol);
            } else if (rv < 0) {
                return NULL;
            }
            break;
        }
    }

    if (!drv1) {
        error_setg(errp, "Unknown protocol '%s'", protocol);
    }
    return drv1;
}

BlockDriver *bdrv_probe_all(const uint8_t *buf, int buf_size,
                            const char *filename)
{
    int score_max = 0, score;
    BlockDriver *drv = NULL, *d;
    IO_CODE();

    QLIST_FOREACH(d, &bdrv_drivers, list) {
        if (d->bdrv_probe) {
            score = d->bdrv_probe(buf, buf_size, filename);
            if (score > score_max) {
                score_max = score;
                drv = d;
            }
        }
    }

    return drv;
}

static int find_image_format(BlockBackend *file, const char *filename,
                             BlockDriver **pdrv, Error **errp)
{
    BlockDriver *drv;
    uint8_t buf[BLOCK_PROBE_BUF_SIZE];
    int ret = 0;

    GLOBAL_STATE_CODE();

    if (blk_is_sg(file) || !blk_is_inserted(file) || blk_getlength(file) == 0) {
        *pdrv = &bdrv_raw;
        return ret;
    }

    ret = blk_pread(file, 0, sizeof(buf), buf, 0);
    if (ret < 0) {
        error_setg_errno(errp, -ret, "Could not read image for determining its "
                         "format");
        *pdrv = NULL;
        return ret;
    }

    drv = bdrv_probe_all(buf, sizeof(buf), filename);
    if (!drv) {
        error_setg(errp, "Could not determine image format: No compatible "
                   "driver found");
        *pdrv = NULL;
        return -ENOENT;
    }

    *pdrv = drv;
    return 0;
}

int coroutine_fn bdrv_co_refresh_total_sectors(BlockDriverState *bs,
                                               int64_t hint)
{
    BlockDriver *drv = bs->drv;
    IO_CODE();
    assert_bdrv_graph_readable();

    if (!drv) {
        return -ENOMEDIUM;
    }

    if (bdrv_is_sg(bs))
        return 0;

    if (drv->bdrv_co_getlength) {
        int64_t length = drv->bdrv_co_getlength(bs);
        if (length < 0) {
            return length;
        }
        hint = DIV_ROUND_UP(length, BDRV_SECTOR_SIZE);
    }

    bs->total_sectors = hint;

    if (bs->total_sectors * BDRV_SECTOR_SIZE > BDRV_MAX_LENGTH) {
        return -EFBIG;
    }

    return 0;
}

static void bdrv_join_options(BlockDriverState *bs, QDict *options,
                              QDict *old_options)
{
    GLOBAL_STATE_CODE();
    if (bs->drv && bs->drv->bdrv_join_options) {
        bs->drv->bdrv_join_options(options, old_options);
    } else {
        qdict_join(options, old_options, false);
    }
}

static BlockdevDetectZeroesOptions bdrv_parse_detect_zeroes(QemuOpts *opts,
                                                            int open_flags,
                                                            Error **errp)
{
    Error *local_err = NULL;
    char *value = qemu_opt_get_del(opts, "detect-zeroes");
    BlockdevDetectZeroesOptions detect_zeroes =
        qapi_enum_parse(&BlockdevDetectZeroesOptions_lookup, value,
                        BLOCKDEV_DETECT_ZEROES_OPTIONS_OFF, &local_err);
    GLOBAL_STATE_CODE();
    g_free(value);
    if (local_err) {
        error_propagate(errp, local_err);
        return detect_zeroes;
    }

    if (detect_zeroes == BLOCKDEV_DETECT_ZEROES_OPTIONS_UNMAP &&
        !(open_flags & BDRV_O_UNMAP))
    {
        error_setg(errp, "setting detect-zeroes to unmap is not allowed "
                   "without setting discard operation to unmap");
    }

    return detect_zeroes;
}

int bdrv_parse_aio(const char *mode, int *flags)
{
    if (!strcmp(mode, "threads")) {
    } else if (!strcmp(mode, "native")) {
        *flags |= BDRV_O_NATIVE_AIO;
#ifdef CONFIG_LINUX_IO_URING
    } else if (!strcmp(mode, "io_uring")) {
        *flags |= BDRV_O_IO_URING;
#endif
    } else {
        return -1;
    }

    return 0;
}

int bdrv_parse_discard_flags(const char *mode, int *flags)
{
    *flags &= ~BDRV_O_UNMAP;

    if (!strcmp(mode, "off") || !strcmp(mode, "ignore")) {
    } else if (!strcmp(mode, "on") || !strcmp(mode, "unmap")) {
        *flags |= BDRV_O_UNMAP;
    } else {
        return -1;
    }

    return 0;
}

int bdrv_parse_cache_mode(const char *mode, int *flags, bool *writethrough)
{
    *flags &= ~BDRV_O_CACHE_MASK;

    if (!strcmp(mode, "off") || !strcmp(mode, "none")) {
        *writethrough = false;
        *flags |= BDRV_O_NOCACHE;
    } else if (!strcmp(mode, "directsync")) {
        *writethrough = true;
        *flags |= BDRV_O_NOCACHE;
    } else if (!strcmp(mode, "writeback")) {
        *writethrough = false;
    } else if (!strcmp(mode, "unsafe")) {
        *writethrough = false;
        *flags |= BDRV_O_NO_FLUSH;
    } else if (!strcmp(mode, "writethrough")) {
        *writethrough = true;
    } else {
        return -1;
    }

    return 0;
}

static char *bdrv_child_get_parent_desc(BdrvChild *c)
{
    BlockDriverState *parent = c->opaque;
    return g_strdup_printf("node '%s'", bdrv_get_node_name(parent));
}

static void GRAPH_RDLOCK bdrv_child_cb_drained_begin(BdrvChild *child)
{
    BlockDriverState *bs = child->opaque;
    bdrv_do_drained_begin_quiesce(bs, NULL);
}

static bool GRAPH_RDLOCK bdrv_child_cb_drained_poll(BdrvChild *child)
{
    BlockDriverState *bs = child->opaque;
    return bdrv_drain_poll(bs, NULL, false);
}

static void GRAPH_RDLOCK bdrv_child_cb_drained_end(BdrvChild *child)
{
    BlockDriverState *bs = child->opaque;
    bdrv_drained_end(bs);
}

static int bdrv_child_cb_inactivate(BdrvChild *child)
{
    BlockDriverState *bs = child->opaque;
    GLOBAL_STATE_CODE();
    assert(bs->open_flags & BDRV_O_INACTIVE);
    return 0;
}

static bool bdrv_child_cb_change_aio_ctx(BdrvChild *child, AioContext *ctx,
                                         GHashTable *visited, Transaction *tran,
                                         Error **errp)
{
    BlockDriverState *bs = child->opaque;
    return bdrv_change_aio_context(bs, ctx, visited, tran, errp);
}

static void GRAPH_WRLOCK bdrv_backing_attach(BdrvChild *c)
{
    BlockDriverState *parent = c->opaque;
    BlockDriverState *backing_hd = c->bs;

    GLOBAL_STATE_CODE();
    assert(!parent->backing_blocker);
    error_setg(&parent->backing_blocker,
               "node is used as backing hd of '%s'",
               bdrv_get_device_or_node_name(parent));

    bdrv_refresh_filename(backing_hd);

    parent->open_flags &= ~BDRV_O_NO_BACKING;

    bdrv_op_block_all(backing_hd, parent->backing_blocker);
    bdrv_op_unblock(backing_hd, BLOCK_OP_TYPE_COMMIT_TARGET,
                    parent->backing_blocker);
    bdrv_op_unblock(backing_hd, BLOCK_OP_TYPE_STREAM,
                    parent->backing_blocker);
    bdrv_op_unblock(backing_hd, BLOCK_OP_TYPE_BACKUP_SOURCE,
                    parent->backing_blocker);
    bdrv_op_unblock(backing_hd, BLOCK_OP_TYPE_BACKUP_TARGET,
                    parent->backing_blocker);
}

static void bdrv_backing_detach(BdrvChild *c)
{
    BlockDriverState *parent = c->opaque;

    GLOBAL_STATE_CODE();
    assert(parent->backing_blocker);
    bdrv_op_unblock_all(c->bs, parent->backing_blocker);
    error_free(parent->backing_blocker);
    parent->backing_blocker = NULL;
}

static int bdrv_backing_update_filename(BdrvChild *c, BlockDriverState *base,
                                        const char *filename,
                                        bool backing_mask_protocol,
                                        Error **errp)
{
    BlockDriverState *parent = c->opaque;
    bool read_only = bdrv_is_read_only(parent);
    int ret;
    const char *format_name;
    GLOBAL_STATE_CODE();

    if (read_only) {
        ret = bdrv_reopen_set_read_only(parent, false, errp);
        if (ret < 0) {
            return ret;
        }
    }

    if (base->drv) {
        if (backing_mask_protocol && base->drv->protocol_name) {
            format_name = "raw";
        } else {
            format_name = base->drv->format_name;
        }
    } else {
        format_name = "";
    }

    ret = bdrv_change_backing_file(parent, filename, format_name, false);
    if (ret < 0) {
        error_setg_errno(errp, -ret, "Could not update backing file link");
    }

    if (read_only) {
        bdrv_reopen_set_read_only(parent, true, NULL);
    }

    return ret;
}

static void bdrv_inherited_options(BdrvChildRole role, bool parent_is_format,
                                   int *child_flags, QDict *child_options,
                                   int parent_flags, QDict *parent_options)
{
    int flags = parent_flags;
    GLOBAL_STATE_CODE();


    if (!parent_is_format &&
        (role & BDRV_CHILD_DATA) &&
        !(role & (BDRV_CHILD_METADATA | BDRV_CHILD_FILTERED)))
    {
        flags &= ~BDRV_O_PROTOCOL;
    }

    if ((parent_is_format && !(role & BDRV_CHILD_COW)) ||
        (role & BDRV_CHILD_METADATA))
    {
        flags |= BDRV_O_PROTOCOL;
    }

    qdict_copy_default(child_options, parent_options, BDRV_OPT_CACHE_DIRECT);
    qdict_copy_default(child_options, parent_options, BDRV_OPT_CACHE_NO_FLUSH);
    qdict_copy_default(child_options, parent_options, BDRV_OPT_FORCE_SHARE);

    if (role & BDRV_CHILD_COW) {
        qdict_set_default_str(child_options, BDRV_OPT_READ_ONLY, "on");
        qdict_set_default_str(child_options, BDRV_OPT_AUTO_READ_ONLY, "off");
    } else {
        qdict_copy_default(child_options, parent_options, BDRV_OPT_READ_ONLY);
        qdict_copy_default(child_options, parent_options,
                           BDRV_OPT_AUTO_READ_ONLY);
    }

    qdict_set_default_str(child_options, BDRV_OPT_DISCARD, "unmap");

    flags &= ~(BDRV_O_SNAPSHOT | BDRV_O_NO_BACKING | BDRV_O_COPY_ON_READ);

    if (role & BDRV_CHILD_METADATA) {
        flags &= ~BDRV_O_NO_IO;
    }
    if (role & BDRV_CHILD_COW) {
        flags &= ~BDRV_O_TEMPORARY;
    }

    *child_flags = flags;
}

static void GRAPH_WRLOCK bdrv_child_cb_attach(BdrvChild *child)
{
    BlockDriverState *bs = child->opaque;

    assert_bdrv_graph_writable();
    QLIST_INSERT_HEAD(&bs->children, child, next);
    if (bs->drv->is_filter || (child->role & BDRV_CHILD_FILTERED)) {
        assert(!(child->role & BDRV_CHILD_COW));
        if (child->role & BDRV_CHILD_PRIMARY) {
            assert(child->role & BDRV_CHILD_FILTERED);
            assert(!bs->backing);
            assert(!bs->file);

            if (bs->drv->filtered_child_is_backing) {
                bs->backing = child;
            } else {
                bs->file = child;
            }
        } else {
            assert(!(child->role & BDRV_CHILD_FILTERED));
        }
    } else if (child->role & BDRV_CHILD_COW) {
        assert(bs->drv->supports_backing);
        assert(!(child->role & BDRV_CHILD_PRIMARY));
        assert(!bs->backing);
        bs->backing = child;
        bdrv_backing_attach(child);
    } else if (child->role & BDRV_CHILD_PRIMARY) {
        assert(!bs->file);
        bs->file = child;
    }
}

static void GRAPH_WRLOCK bdrv_child_cb_detach(BdrvChild *child)
{
    BlockDriverState *bs = child->opaque;

    if (child->role & BDRV_CHILD_COW) {
        bdrv_backing_detach(child);
    }

    assert_bdrv_graph_writable();
    QLIST_REMOVE(child, next);
    if (child == bs->backing) {
        assert(child != bs->file);
        bs->backing = NULL;
    } else if (child == bs->file) {
        bs->file = NULL;
    }
}

static int bdrv_child_cb_update_filename(BdrvChild *c, BlockDriverState *base,
                                         const char *filename,
                                         bool backing_mask_protocol,
                                         Error **errp)
{
    if (c->role & BDRV_CHILD_COW) {
        return bdrv_backing_update_filename(c, base, filename,
                                            backing_mask_protocol,
                                            errp);
    }
    return 0;
}

AioContext *child_of_bds_get_parent_aio_context(BdrvChild *c)
{
    BlockDriverState *bs = c->opaque;
    IO_CODE();

    return bdrv_get_aio_context(bs);
}

const BdrvChildClass child_of_bds = {
    .parent_is_bds   = true,
    .get_parent_desc = bdrv_child_get_parent_desc,
    .inherit_options = bdrv_inherited_options,
    .drained_begin   = bdrv_child_cb_drained_begin,
    .drained_poll    = bdrv_child_cb_drained_poll,
    .drained_end     = bdrv_child_cb_drained_end,
    .attach          = bdrv_child_cb_attach,
    .detach          = bdrv_child_cb_detach,
    .inactivate      = bdrv_child_cb_inactivate,
    .change_aio_ctx  = bdrv_child_cb_change_aio_ctx,
    .update_filename = bdrv_child_cb_update_filename,
    .get_parent_aio_context = child_of_bds_get_parent_aio_context,
};

AioContext *bdrv_child_get_parent_aio_context(BdrvChild *c)
{
    IO_CODE();
    return c->klass->get_parent_aio_context(c);
}

static int bdrv_open_flags(BlockDriverState *bs, int flags)
{
    int open_flags = flags;
    GLOBAL_STATE_CODE();

    open_flags &= ~(BDRV_O_SNAPSHOT | BDRV_O_NO_BACKING | BDRV_O_PROTOCOL);

    return open_flags;
}

static void update_flags_from_options(int *flags, QemuOpts *opts)
{
    GLOBAL_STATE_CODE();

    *flags &= ~(BDRV_O_CACHE_MASK | BDRV_O_RDWR | BDRV_O_AUTO_RDONLY);

    if (qemu_opt_get_bool_del(opts, BDRV_OPT_CACHE_NO_FLUSH, false)) {
        *flags |= BDRV_O_NO_FLUSH;
    }

    if (qemu_opt_get_bool_del(opts, BDRV_OPT_CACHE_DIRECT, false)) {
        *flags |= BDRV_O_NOCACHE;
    }

    if (!qemu_opt_get_bool_del(opts, BDRV_OPT_READ_ONLY, false)) {
        *flags |= BDRV_O_RDWR;
    }

    if (qemu_opt_get_bool_del(opts, BDRV_OPT_AUTO_READ_ONLY, false)) {
        *flags |= BDRV_O_AUTO_RDONLY;
    }

    if (!qemu_opt_get_bool_del(opts, BDRV_OPT_ACTIVE, true)) {
        *flags |= BDRV_O_INACTIVE;
    }
}

static void update_options_from_flags(QDict *options, int flags)
{
    GLOBAL_STATE_CODE();
    if (!qdict_haskey(options, BDRV_OPT_CACHE_DIRECT)) {
        qdict_put_bool(options, BDRV_OPT_CACHE_DIRECT, flags & BDRV_O_NOCACHE);
    }
    if (!qdict_haskey(options, BDRV_OPT_CACHE_NO_FLUSH)) {
        qdict_put_bool(options, BDRV_OPT_CACHE_NO_FLUSH,
                       flags & BDRV_O_NO_FLUSH);
    }
    if (!qdict_haskey(options, BDRV_OPT_READ_ONLY)) {
        qdict_put_bool(options, BDRV_OPT_READ_ONLY, !(flags & BDRV_O_RDWR));
    }
    if (!qdict_haskey(options, BDRV_OPT_AUTO_READ_ONLY)) {
        qdict_put_bool(options, BDRV_OPT_AUTO_READ_ONLY,
                       flags & BDRV_O_AUTO_RDONLY);
    }
}

static void bdrv_assign_node_name(BlockDriverState *bs,
                                  const char *node_name,
                                  Error **errp)
{
    char *gen_node_name = NULL;
    GLOBAL_STATE_CODE();

    if (!node_name) {
        node_name = gen_node_name = id_generate(ID_BLOCK);
    } else if (!id_wellformed(node_name)) {
        error_setg(errp, "Invalid node-name: '%s'", node_name);
        return;
    }

    if (blk_by_name(node_name)) {
        error_setg(errp, "node-name=%s is conflicting with a device id",
                   node_name);
        goto out;
    }

    if (bdrv_find_node(node_name)) {
        error_setg(errp, "Duplicate nodes with node-name='%s'", node_name);
        goto out;
    }

    if (strlen(node_name) >= sizeof(bs->node_name)) {
        error_setg(errp, "Node name too long");
        goto out;
    }

    pstrcpy(bs->node_name, sizeof(bs->node_name), node_name);
    QTAILQ_INSERT_TAIL(&graph_bdrv_states, bs, node_list);
out:
    g_free(gen_node_name);
}

static int no_coroutine_fn GRAPH_UNLOCKED
bdrv_open_driver(BlockDriverState *bs, BlockDriver *drv, const char *node_name,
                 QDict *options, int open_flags, Error **errp)
{
    Error *local_err = NULL;
    int i, ret;
    GLOBAL_STATE_CODE();

    bdrv_assign_node_name(bs, node_name, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        return -EINVAL;
    }

    bs->drv = drv;
    bs->opaque = g_malloc0(drv->instance_size);

    assert(!drv->bdrv_needs_filename || bs->filename[0]);
    if (drv->bdrv_open) {
        ret = drv->bdrv_open(bs, options, open_flags, &local_err);
    } else {
        ret = 0;
    }

    if (ret < 0) {
        if (local_err) {
            error_propagate(errp, local_err);
        } else if (bs->filename[0]) {
            error_setg_errno(errp, -ret, "Could not open '%s'", bs->filename);
        } else {
            error_setg_errno(errp, -ret, "Could not open image");
        }
        goto open_failed;
    }

    assert(!(bs->supported_read_flags & ~BDRV_REQ_MASK));
    assert(!(bs->supported_write_flags & ~BDRV_REQ_MASK));

    bs->supported_read_flags |= BDRV_REQ_REGISTERED_BUF;
    bs->supported_write_flags |= BDRV_REQ_REGISTERED_BUF;

    ret = bdrv_refresh_total_sectors(bs, bs->total_sectors);
    if (ret < 0) {
        error_setg_errno(errp, -ret, "Could not refresh total sector count");
        return ret;
    }

    bdrv_graph_rdlock_main_loop();
    bdrv_refresh_limits(bs, NULL, &local_err);
    bdrv_graph_rdunlock_main_loop();

    if (local_err) {
        error_propagate(errp, local_err);
        return -EINVAL;
    }

    assert(bdrv_opt_mem_align(bs) != 0);
    assert(bdrv_min_mem_align(bs) != 0);
    assert(is_power_of_2(bs->bl.request_alignment));

    for (i = 0; i < bs->quiesce_counter; i++) {
        if (drv->bdrv_drain_begin) {
            drv->bdrv_drain_begin(bs);
        }
    }

    return 0;
open_failed:
    bs->drv = NULL;

    bdrv_graph_wrlock();
    if (bs->file != NULL) {
        bdrv_unref_child(bs, bs->file);
        assert(!bs->file);
    }
    bdrv_graph_wrunlock();

    g_free(bs->opaque);
    bs->opaque = NULL;
    return ret;
}

BlockDriverState *bdrv_new_open_driver_opts(BlockDriver *drv,
                                            const char *node_name,
                                            QDict *options, int flags,
                                            Error **errp)
{
    BlockDriverState *bs;
    int ret;

    GLOBAL_STATE_CODE();

    bs = bdrv_new();
    bs->open_flags = flags;
    bs->options = options ?: qdict_new();
    bs->explicit_options = qdict_clone_shallow(bs->options);
    bs->opaque = NULL;

    update_options_from_flags(bs->options, flags);

    ret = bdrv_open_driver(bs, drv, node_name, bs->options, flags, errp);
    if (ret < 0) {
        qobject_unref(bs->explicit_options);
        bs->explicit_options = NULL;
        qobject_unref(bs->options);
        bs->options = NULL;
        bdrv_unref(bs);
        return NULL;
    }

    return bs;
}

BlockDriverState *bdrv_new_open_driver(BlockDriver *drv, const char *node_name,
                                       int flags, Error **errp)
{
    GLOBAL_STATE_CODE();
    return bdrv_new_open_driver_opts(drv, node_name, NULL, flags, errp);
}

QemuOptsList bdrv_runtime_opts = {
    .name = "bdrv_common",
    .head = QTAILQ_HEAD_INITIALIZER(bdrv_runtime_opts.head),
    .desc = {
        {
            .name = "node-name",
            .type = QEMU_OPT_STRING,
            .help = "Node name of the block device node",
        },
        {
            .name = "driver",
            .type = QEMU_OPT_STRING,
            .help = "Block driver to use for the node",
        },
        {
            .name = BDRV_OPT_CACHE_DIRECT,
            .type = QEMU_OPT_BOOL,
            .help = "Bypass software writeback cache on the host",
        },
        {
            .name = BDRV_OPT_CACHE_NO_FLUSH,
            .type = QEMU_OPT_BOOL,
            .help = "Ignore flush requests",
        },
        {
            .name = BDRV_OPT_ACTIVE,
            .type = QEMU_OPT_BOOL,
            .help = "Node is activated",
        },
        {
            .name = BDRV_OPT_READ_ONLY,
            .type = QEMU_OPT_BOOL,
            .help = "Node is opened in read-only mode",
        },
        {
            .name = BDRV_OPT_AUTO_READ_ONLY,
            .type = QEMU_OPT_BOOL,
            .help = "Node can become read-only if opening read-write fails",
        },
        {
            .name = "detect-zeroes",
            .type = QEMU_OPT_STRING,
            .help = "try to optimize zero writes (off, on, unmap)",
        },
        {
            .name = BDRV_OPT_DISCARD,
            .type = QEMU_OPT_STRING,
            .help = "discard operation (ignore/off, unmap/on)",
        },
        {
            .name = BDRV_OPT_FORCE_SHARE,
            .type = QEMU_OPT_BOOL,
            .help = "always accept other writers (default: off)",
        },
        { /* end of list */ }
    },
};

QemuOptsList bdrv_create_opts_simple = {
    .name = "simple-create-opts",
    .head = QTAILQ_HEAD_INITIALIZER(bdrv_create_opts_simple.head),
    .desc = {
        {
            .name = BLOCK_OPT_SIZE,
            .type = QEMU_OPT_SIZE,
            .help = "Virtual disk size"
        },
        {
            .name = BLOCK_OPT_PREALLOC,
            .type = QEMU_OPT_STRING,
            .help = "Preallocation mode (allowed values: off)"
        },
        { /* end of list */ }
    }
};

static int bdrv_open_common(BlockDriverState *bs, BlockBackend *file,
                            QDict *options, Error **errp)
{
    int ret, open_flags;
    const char *filename;
    const char *driver_name = NULL;
    const char *node_name = NULL;
    const char *discard;
    QemuOpts *opts;
    BlockDriver *drv;
    Error *local_err = NULL;
    bool ro;

    GLOBAL_STATE_CODE();

    bdrv_graph_rdlock_main_loop();
    assert(bs->file == NULL);
    assert(options != NULL && bs->options != options);
    bdrv_graph_rdunlock_main_loop();

    opts = qemu_opts_create(&bdrv_runtime_opts, NULL, 0, &error_abort);
    if (!qemu_opts_absorb_qdict(opts, options, errp)) {
        ret = -EINVAL;
        goto fail_opts;
    }

    update_flags_from_options(&bs->open_flags, opts);

    driver_name = qemu_opt_get(opts, "driver");
    drv = bdrv_find_format(driver_name);
    assert(drv != NULL);

    bs->force_share = qemu_opt_get_bool(opts, BDRV_OPT_FORCE_SHARE, false);

    if (bs->force_share && (bs->open_flags & BDRV_O_RDWR)) {
        error_setg(errp,
                   BDRV_OPT_FORCE_SHARE
                   "=on can only be used with read-only images");
        ret = -EINVAL;
        goto fail_opts;
    }

    if (file != NULL) {
        bdrv_graph_rdlock_main_loop();
        bdrv_refresh_filename(blk_bs(file));
        bdrv_graph_rdunlock_main_loop();

        filename = blk_bs(file)->filename;
    } else {
        filename = qdict_get_try_str(options, "filename");
    }

    if (drv->bdrv_needs_filename && (!filename || !filename[0])) {
        error_setg(errp, "The '%s' block driver requires a file name",
                   drv->format_name);
        ret = -EINVAL;
        goto fail_opts;
    }

    trace_bdrv_open_common(bs, filename ?: "", bs->open_flags,
                           drv->format_name);

    ro = bdrv_is_read_only(bs);

    if (use_bdrv_whitelist && !bdrv_is_whitelisted(drv, ro)) {
        if (!ro && bdrv_is_whitelisted(drv, true)) {
            bdrv_graph_rdlock_main_loop();
            ret = bdrv_apply_auto_read_only(bs, NULL, NULL);
            bdrv_graph_rdunlock_main_loop();
        } else {
            ret = -ENOTSUP;
        }
        if (ret < 0) {
            error_setg(errp,
                       !ro && bdrv_is_whitelisted(drv, true)
                       ? "Driver '%s' can only be used for read-only devices"
                       : "Driver '%s' is not whitelisted",
                       drv->format_name);
            goto fail_opts;
        }
    }

    assert(qatomic_read(&bs->copy_on_read) == 0);

    if (bs->open_flags & BDRV_O_COPY_ON_READ) {
        if (!ro) {
            bdrv_enable_copy_on_read(bs);
        } else {
            error_setg(errp, "Can't use copy-on-read on read-only device");
            ret = -EINVAL;
            goto fail_opts;
        }
    }

    discard = qemu_opt_get(opts, BDRV_OPT_DISCARD);
    if (discard != NULL) {
        if (bdrv_parse_discard_flags(discard, &bs->open_flags) != 0) {
            error_setg(errp, "Invalid discard option");
            ret = -EINVAL;
            goto fail_opts;
        }
    }

    bs->detect_zeroes =
        bdrv_parse_detect_zeroes(opts, bs->open_flags, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        ret = -EINVAL;
        goto fail_opts;
    }

    if (filename != NULL) {
        pstrcpy(bs->filename, sizeof(bs->filename), filename);
    } else {
        bs->filename[0] = '\0';
    }
    pstrcpy(bs->exact_filename, sizeof(bs->exact_filename), bs->filename);

    open_flags = bdrv_open_flags(bs, bs->open_flags);
    node_name = qemu_opt_get(opts, "node-name");

    assert(!drv->protocol_name || file == NULL);
    ret = bdrv_open_driver(bs, drv, node_name, options, open_flags, errp);
    if (ret < 0) {
        goto fail_opts;
    }

    qemu_opts_del(opts);
    return 0;

fail_opts:
    qemu_opts_del(opts);
    return ret;
}

static QDict *parse_json_filename(const char *filename, Error **errp)
{
    ERRP_GUARD();
    QObject *options_obj;
    QDict *options;
    int ret;
    GLOBAL_STATE_CODE();

    ret = strstart(filename, "json:", &filename);
    assert(ret);

    options_obj = qobject_from_json(filename, errp);
    if (!options_obj) {
        error_prepend(errp, "Could not parse the JSON options: ");
        return NULL;
    }

    options = qobject_to(QDict, options_obj);
    if (!options) {
        qobject_unref(options_obj);
        error_setg(errp, "Invalid JSON object given");
        return NULL;
    }

    qdict_flatten(options);

    return options;
}

static void parse_json_protocol(QDict *options, const char **pfilename,
                                Error **errp)
{
    QDict *json_options;
    Error *local_err = NULL;
    GLOBAL_STATE_CODE();

    if (!*pfilename || !g_str_has_prefix(*pfilename, "json:")) {
        return;
    }

    json_options = parse_json_filename(*pfilename, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        return;
    }

    qdict_join(options, json_options, false);
    qobject_unref(json_options);
    *pfilename = NULL;
}

static int bdrv_fill_options(QDict **options, const char *filename,
                             int *flags, bool allow_parse_filename,
                             Error **errp)
{
    const char *drvname;
    bool protocol = *flags & BDRV_O_PROTOCOL;
    bool parse_filename = false;
    BlockDriver *drv = NULL;
    Error *local_err = NULL;

    GLOBAL_STATE_CODE();

    drvname = qdict_get_try_str(*options, "driver");
    if (drvname) {
        drv = bdrv_find_format(drvname);
        if (!drv) {
            error_setg(errp, "Unknown driver '%s'", drvname);
            return -ENOENT;
        }
        protocol = drv->protocol_name;
    }

    if (protocol) {
        *flags |= BDRV_O_PROTOCOL;
    } else {
        *flags &= ~BDRV_O_PROTOCOL;
    }

    update_options_from_flags(*options, *flags);

    if (protocol && filename) {
        if (!qdict_haskey(*options, "filename")) {
            qdict_put_str(*options, "filename", filename);
            parse_filename = allow_parse_filename;
        } else {
            error_setg(errp, "Can't specify 'file' and 'filename' options at "
                             "the same time");
            return -EINVAL;
        }
    }

    filename = qdict_get_try_str(*options, "filename");

    if (!drvname && protocol) {
        if (filename) {
            drv = bdrv_find_protocol(filename, parse_filename, errp);
            if (!drv) {
                return -EINVAL;
            }

            drvname = drv->format_name;
            qdict_put_str(*options, "driver", drvname);
        } else {
            error_setg(errp, "Must specify either driver or file");
            return -EINVAL;
        }
    }

    assert(drv || !protocol);

    if (drv && drv->bdrv_parse_filename && parse_filename) {
        drv->bdrv_parse_filename(filename, *options, &local_err);
        if (local_err) {
            error_propagate(errp, local_err);
            return -EINVAL;
        }

        if (!drv->bdrv_needs_filename) {
            qdict_del(*options, "filename");
        }
    }

    return 0;
}

typedef struct BlockReopenQueueEntry {
     bool prepared;
     BDRVReopenState state;
     QTAILQ_ENTRY(BlockReopenQueueEntry) entry;
} BlockReopenQueueEntry;

static int bdrv_reopen_get_flags(BlockReopenQueue *q, BlockDriverState *bs)
{
    BlockReopenQueueEntry *entry;

    if (q != NULL) {
        QTAILQ_FOREACH(entry, q, entry) {
            if (entry->state.bs == bs) {
                return entry->state.flags;
            }
        }
    }

    return bs->open_flags;
}

static bool bdrv_is_writable_after_reopen(BlockDriverState *bs,
                                          BlockReopenQueue *q)
{
    int flags = bdrv_reopen_get_flags(q, bs);

    return (flags & (BDRV_O_RDWR | BDRV_O_INACTIVE)) == BDRV_O_RDWR;
}

bool bdrv_is_writable(BlockDriverState *bs)
{
    IO_CODE();
    return bdrv_is_writable_after_reopen(bs, NULL);
}

static char *bdrv_child_user_desc(BdrvChild *c)
{
    GLOBAL_STATE_CODE();
    return c->klass->get_parent_desc(c);
}

static bool bdrv_a_allow_b(BdrvChild *a, BdrvChild *b, Error **errp)
{
    const char *child_bs_name;
    g_autofree char *a_user = NULL;
    g_autofree char *b_user = NULL;
    g_autofree char *perms = NULL;

    assert(a->bs);
    assert(a->bs == b->bs);
    GLOBAL_STATE_CODE();

    if ((b->perm & a->shared_perm) == b->perm) {
        return true;
    }

    child_bs_name = bdrv_get_node_name(b->bs);
    a_user = bdrv_child_user_desc(a);
    b_user = bdrv_child_user_desc(b);
    perms = bdrv_perm_names(b->perm & ~a->shared_perm);

    error_setg(errp, "Permission conflict on node '%s': permissions '%s' are "
               "both required by %s (uses node '%s' as '%s' child) and "
               "unshared by %s (uses node '%s' as '%s' child).",
               child_bs_name, perms,
               b_user, child_bs_name, b->name,
               a_user, child_bs_name, a->name);

    return false;
}

static bool GRAPH_RDLOCK
bdrv_parent_perms_conflict(BlockDriverState *bs, Error **errp)
{
    BdrvChild *a, *b;
    GLOBAL_STATE_CODE();

    QLIST_FOREACH(a, &bs->parents, next_parent) {
        QLIST_FOREACH(b, &bs->parents, next_parent) {
            if (a == b) {
                continue;
            }

            if (!bdrv_a_allow_b(a, b, errp)) {
                return true;
            }
        }
    }

    return false;
}

static void GRAPH_RDLOCK
bdrv_child_perm(BlockDriverState *bs, BlockDriverState *child_bs,
                BdrvChild *c, BdrvChildRole role,
                BlockReopenQueue *reopen_queue,
                uint64_t parent_perm, uint64_t parent_shared,
                uint64_t *nperm, uint64_t *nshared)
{
    assert(bs->drv && bs->drv->bdrv_child_perm);
    GLOBAL_STATE_CODE();
    bs->drv->bdrv_child_perm(bs, c, role, reopen_queue,
                             parent_perm, parent_shared,
                             nperm, nshared);
    if (child_bs && child_bs->force_share) {
        *nshared = BLK_PERM_ALL;
    }
}

static GSList * GRAPH_RDLOCK
bdrv_topological_dfs(GSList *list, GHashTable *found, BlockDriverState *bs)
{
    BdrvChild *child;
    g_autoptr(GHashTable) local_found = NULL;

    GLOBAL_STATE_CODE();

    if (!found) {
        assert(!list);
        found = local_found = g_hash_table_new(NULL, NULL);
    }

    if (g_hash_table_contains(found, bs)) {
        return list;
    }
    g_hash_table_add(found, bs);

    QLIST_FOREACH(child, &bs->children, next) {
        list = bdrv_topological_dfs(list, found, child->bs);
    }

    return g_slist_prepend(list, bs);
}

typedef struct BdrvChildSetPermState {
    BdrvChild *child;
    uint64_t old_perm;
    uint64_t old_shared_perm;
} BdrvChildSetPermState;

static void bdrv_child_set_perm_abort(void *opaque)
{
    BdrvChildSetPermState *s = opaque;

    GLOBAL_STATE_CODE();

    s->child->perm = s->old_perm;
    s->child->shared_perm = s->old_shared_perm;
}

static TransactionActionDrv bdrv_child_set_pem_drv = {
    .abort = bdrv_child_set_perm_abort,
    .clean = g_free,
};

static void bdrv_child_set_perm(BdrvChild *c, uint64_t perm,
                                uint64_t shared, Transaction *tran)
{
    BdrvChildSetPermState *s = g_new(BdrvChildSetPermState, 1);
    GLOBAL_STATE_CODE();

    *s = (BdrvChildSetPermState) {
        .child = c,
        .old_perm = c->perm,
        .old_shared_perm = c->shared_perm,
    };

    c->perm = perm;
    c->shared_perm = shared;

    tran_add(tran, &bdrv_child_set_pem_drv, s);
}

static void GRAPH_RDLOCK bdrv_drv_set_perm_commit(void *opaque)
{
    BlockDriverState *bs = opaque;
    uint64_t cumulative_perms, cumulative_shared_perms;
    GLOBAL_STATE_CODE();

    if (bs->drv->bdrv_set_perm) {
        bdrv_get_cumulative_perm(bs, &cumulative_perms,
                                 &cumulative_shared_perms);
        bs->drv->bdrv_set_perm(bs, cumulative_perms, cumulative_shared_perms);
    }
}

static void GRAPH_RDLOCK bdrv_drv_set_perm_abort(void *opaque)
{
    BlockDriverState *bs = opaque;
    GLOBAL_STATE_CODE();

    if (bs->drv->bdrv_abort_perm_update) {
        bs->drv->bdrv_abort_perm_update(bs);
    }
}

TransactionActionDrv bdrv_drv_set_perm_drv = {
    .abort = bdrv_drv_set_perm_abort,
    .commit = bdrv_drv_set_perm_commit,
};

static int GRAPH_RDLOCK
bdrv_drv_set_perm(BlockDriverState *bs, uint64_t perm, uint64_t shared_perm,
                  Transaction *tran, Error **errp)
{
    GLOBAL_STATE_CODE();
    if (!bs->drv) {
        return 0;
    }

    if (bs->drv->bdrv_check_perm) {
        int ret = bs->drv->bdrv_check_perm(bs, perm, shared_perm, errp);
        if (ret < 0) {
            return ret;
        }
    }

    if (tran) {
        tran_add(tran, &bdrv_drv_set_perm_drv, bs);
    }

    return 0;
}

typedef struct BdrvReplaceChildState {
    BdrvChild *child;
    BlockDriverState *old_bs;
} BdrvReplaceChildState;

static void GRAPH_WRLOCK bdrv_replace_child_commit(void *opaque)
{
    BdrvReplaceChildState *s = opaque;
    GLOBAL_STATE_CODE();

    bdrv_schedule_unref(s->old_bs);
}

static void GRAPH_WRLOCK bdrv_replace_child_abort(void *opaque)
{
    BdrvReplaceChildState *s = opaque;
    BlockDriverState *new_bs = s->child->bs;

    GLOBAL_STATE_CODE();
    assert_bdrv_graph_writable();

    if (!s->child->bs) {
        bdrv_parent_drained_begin_single(s->child);
        assert(!bdrv_parent_drained_poll_single(s->child));
    }
    assert(s->child->quiesced_parent);
    bdrv_replace_child_noperm(s->child, s->old_bs);

    bdrv_unref(new_bs);
}

static TransactionActionDrv bdrv_replace_child_drv = {
    .commit = bdrv_replace_child_commit,
    .abort = bdrv_replace_child_abort,
    .clean = g_free,
};

static void GRAPH_WRLOCK
bdrv_replace_child_tran(BdrvChild *child, BlockDriverState *new_bs,
                        Transaction *tran)
{
    BdrvReplaceChildState *s = g_new(BdrvReplaceChildState, 1);

    assert(child->quiesced_parent);
    assert(!new_bs || new_bs->quiesce_counter);

    *s = (BdrvReplaceChildState) {
        .child = child,
        .old_bs = child->bs,
    };
    tran_add(tran, &bdrv_replace_child_drv, s);

    if (new_bs) {
        bdrv_ref(new_bs);
    }

    bdrv_replace_child_noperm(child, new_bs);
}

static int GRAPH_RDLOCK
bdrv_node_refresh_perm(BlockDriverState *bs, BlockReopenQueue *q,
                       Transaction *tran, Error **errp)
{
    BlockDriver *drv = bs->drv;
    BdrvChild *c;
    int ret;
    uint64_t cumulative_perms, cumulative_shared_perms;
    GLOBAL_STATE_CODE();

    bdrv_get_cumulative_perm(bs, &cumulative_perms, &cumulative_shared_perms);

    if ((cumulative_perms & (BLK_PERM_WRITE | BLK_PERM_WRITE_UNCHANGED)) &&
        !bdrv_is_writable_after_reopen(bs, q))
    {
        if (!bdrv_is_writable_after_reopen(bs, NULL)) {
            error_setg(errp, "Block node is read-only");
        } else {
            error_setg(errp, "Read-only block node '%s' cannot support "
                       "read-write users", bdrv_get_node_name(bs));
        }

        return -EPERM;
    }

    if ((cumulative_perms & (BLK_PERM_WRITE | BLK_PERM_WRITE_UNCHANGED)) &&
        !(cumulative_perms & BLK_PERM_RESIZE))
    {
        if ((bs->total_sectors * BDRV_SECTOR_SIZE) % bs->bl.request_alignment) {
            error_setg(errp, "Cannot get 'write' permission without 'resize': "
                             "Image size is not a multiple of request "
                             "alignment");
            return -EPERM;
        }
    }

    if (!drv) {
        return 0;
    }

    ret = bdrv_drv_set_perm(bs, cumulative_perms, cumulative_shared_perms, tran,
                            errp);
    if (ret < 0) {
        return ret;
    }

    if (!drv->bdrv_child_perm) {
        assert(QLIST_EMPTY(&bs->children));
        return 0;
    }

    QLIST_FOREACH(c, &bs->children, next) {
        uint64_t cur_perm, cur_shared;

        bdrv_child_perm(bs, c->bs, c, c->role, q,
                        cumulative_perms, cumulative_shared_perms,
                        &cur_perm, &cur_shared);
        bdrv_child_set_perm(c, cur_perm, cur_shared, tran);
    }

    return 0;
}

static int GRAPH_RDLOCK
bdrv_do_refresh_perms(GSList *list, BlockReopenQueue *q, Transaction *tran,
                      Error **errp)
{
    int ret;
    BlockDriverState *bs;
    GLOBAL_STATE_CODE();

    for ( ; list; list = list->next) {
        bs = list->data;

        if (bdrv_parent_perms_conflict(bs, errp)) {
            return -EINVAL;
        }

        ret = bdrv_node_refresh_perm(bs, q, tran, errp);
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}

static int GRAPH_RDLOCK
bdrv_list_refresh_perms(GSList *list, BlockReopenQueue *q, Transaction *tran,
                        Error **errp)
{
    g_autoptr(GHashTable) found = g_hash_table_new(NULL, NULL);
    g_autoptr(GSList) refresh_list = NULL;

    for ( ; list; list = list->next) {
        refresh_list = bdrv_topological_dfs(refresh_list, found, list->data);
    }

    return bdrv_do_refresh_perms(refresh_list, q, tran, errp);
}

void bdrv_get_cumulative_perm(BlockDriverState *bs, uint64_t *perm,
                              uint64_t *shared_perm)
{
    BdrvChild *c;
    uint64_t cumulative_perms = 0;
    uint64_t cumulative_shared_perms = BLK_PERM_ALL;

    GLOBAL_STATE_CODE();

    QLIST_FOREACH(c, &bs->parents, next_parent) {
        cumulative_perms |= c->perm;
        cumulative_shared_perms &= c->shared_perm;
    }

    *perm = cumulative_perms;
    *shared_perm = cumulative_shared_perms;
}

char *bdrv_perm_names(uint64_t perm)
{
    struct perm_name {
        uint64_t perm;
        const char *name;
    } permissions[] = {
        { BLK_PERM_CONSISTENT_READ, "consistent read" },
        { BLK_PERM_WRITE,           "write" },
        { BLK_PERM_WRITE_UNCHANGED, "write unchanged" },
        { BLK_PERM_RESIZE,          "resize" },
        { 0, NULL }
    };

    GString *result = g_string_sized_new(30);
    struct perm_name *p;

    for (p = permissions; p->name; p++) {
        if (perm & p->perm) {
            if (result->len > 0) {
                g_string_append(result, ", ");
            }
            g_string_append(result, p->name);
        }
    }

    return g_string_free(result, FALSE);
}


static int GRAPH_RDLOCK
bdrv_refresh_perms(BlockDriverState *bs, Transaction *tran, Error **errp)
{
    int ret;
    Transaction *local_tran = NULL;
    g_autoptr(GSList) list = bdrv_topological_dfs(NULL, NULL, bs);
    GLOBAL_STATE_CODE();

    if (!tran) {
        tran = local_tran = tran_new();
    }

    ret = bdrv_do_refresh_perms(list, NULL, tran, errp);

    if (local_tran) {
        tran_finalize(local_tran, ret);
    }

    return ret;
}

int bdrv_child_try_set_perm(BdrvChild *c, uint64_t perm, uint64_t shared,
                            Error **errp)
{
    Error *local_err = NULL;
    Transaction *tran = tran_new();
    int ret;

    GLOBAL_STATE_CODE();

    bdrv_child_set_perm(c, perm, shared, tran);

    ret = bdrv_refresh_perms(c->bs, tran, &local_err);

    tran_finalize(tran, ret);

    if (ret < 0) {
        if ((perm & ~c->perm) || (c->shared_perm & ~shared)) {
            error_propagate(errp, local_err);
        } else {
            error_free(local_err);
            ret = 0;
        }
    }

    return ret;
}

int bdrv_child_refresh_perms(BlockDriverState *bs, BdrvChild *c, Error **errp)
{
    uint64_t parent_perms, parent_shared;
    uint64_t perms, shared;

    GLOBAL_STATE_CODE();

    bdrv_get_cumulative_perm(bs, &parent_perms, &parent_shared);
    bdrv_child_perm(bs, c->bs, c, c->role, NULL,
                    parent_perms, parent_shared, &perms, &shared);

    return bdrv_child_try_set_perm(c, perms, shared, errp);
}

static void bdrv_filter_default_perms(BlockDriverState *bs, BdrvChild *c,
                                      BdrvChildRole role,
                                      BlockReopenQueue *reopen_queue,
                                      uint64_t perm, uint64_t shared,
                                      uint64_t *nperm, uint64_t *nshared)
{
    GLOBAL_STATE_CODE();
    *nperm = perm & DEFAULT_PERM_PASSTHROUGH;
    *nshared = (shared & DEFAULT_PERM_PASSTHROUGH) | DEFAULT_PERM_UNCHANGED;
}

static void bdrv_default_perms_for_cow(BlockDriverState *bs, BdrvChild *c,
                                       BdrvChildRole role,
                                       BlockReopenQueue *reopen_queue,
                                       uint64_t perm, uint64_t shared,
                                       uint64_t *nperm, uint64_t *nshared)
{
    assert(role & BDRV_CHILD_COW);
    GLOBAL_STATE_CODE();

    perm &= BLK_PERM_CONSISTENT_READ;

    if (shared & BLK_PERM_WRITE) {
        shared = BLK_PERM_WRITE | BLK_PERM_RESIZE;
    } else {
        shared = 0;
    }

    shared |= BLK_PERM_CONSISTENT_READ | BLK_PERM_WRITE_UNCHANGED;

    if (bs->open_flags & BDRV_O_INACTIVE) {
        shared |= BLK_PERM_WRITE | BLK_PERM_RESIZE;
    }

    *nperm = perm;
    *nshared = shared;
}

static void bdrv_default_perms_for_storage(BlockDriverState *bs, BdrvChild *c,
                                           BdrvChildRole role,
                                           BlockReopenQueue *reopen_queue,
                                           uint64_t perm, uint64_t shared,
                                           uint64_t *nperm, uint64_t *nshared)
{
    int flags;

    GLOBAL_STATE_CODE();
    assert(role & (BDRV_CHILD_METADATA | BDRV_CHILD_DATA));

    flags = bdrv_reopen_get_flags(reopen_queue, bs);

    bdrv_filter_default_perms(bs, c, role, reopen_queue,
                              perm, shared, &perm, &shared);

    if (role & BDRV_CHILD_METADATA) {
        if (bdrv_is_writable_after_reopen(bs, reopen_queue)) {
            perm |= BLK_PERM_WRITE | BLK_PERM_RESIZE;
        }

        if (!(flags & BDRV_O_NO_IO)) {
            perm |= BLK_PERM_CONSISTENT_READ;
        }
        shared &= ~(BLK_PERM_WRITE | BLK_PERM_RESIZE);
    }

    if (role & BDRV_CHILD_DATA) {

        shared &= ~BLK_PERM_RESIZE;

        if (perm & BLK_PERM_WRITE_UNCHANGED) {
            perm |= BLK_PERM_WRITE;
        }

        if (perm & BLK_PERM_WRITE) {
            perm |= BLK_PERM_RESIZE;
        }
    }

    if (bs->open_flags & BDRV_O_INACTIVE) {
        shared |= BLK_PERM_WRITE | BLK_PERM_RESIZE;
    }

    *nperm = perm;
    *nshared = shared;
}

void bdrv_default_perms(BlockDriverState *bs, BdrvChild *c,
                        BdrvChildRole role, BlockReopenQueue *reopen_queue,
                        uint64_t perm, uint64_t shared,
                        uint64_t *nperm, uint64_t *nshared)
{
    GLOBAL_STATE_CODE();
    if (role & BDRV_CHILD_FILTERED) {
        assert(!(role & (BDRV_CHILD_DATA | BDRV_CHILD_METADATA |
                         BDRV_CHILD_COW)));
        bdrv_filter_default_perms(bs, c, role, reopen_queue,
                                  perm, shared, nperm, nshared);
    } else if (role & BDRV_CHILD_COW) {
        assert(!(role & (BDRV_CHILD_DATA | BDRV_CHILD_METADATA)));
        bdrv_default_perms_for_cow(bs, c, role, reopen_queue,
                                   perm, shared, nperm, nshared);
    } else if (role & (BDRV_CHILD_METADATA | BDRV_CHILD_DATA)) {
        bdrv_default_perms_for_storage(bs, c, role, reopen_queue,
                                       perm, shared, nperm, nshared);
    } else {
        g_assert_not_reached();
    }
}

uint64_t bdrv_qapi_perm_to_blk_perm(BlockPermission qapi_perm)
{
    static const uint64_t permissions[] = {
        [BLOCK_PERMISSION_CONSISTENT_READ]  = BLK_PERM_CONSISTENT_READ,
        [BLOCK_PERMISSION_WRITE]            = BLK_PERM_WRITE,
        [BLOCK_PERMISSION_WRITE_UNCHANGED]  = BLK_PERM_WRITE_UNCHANGED,
        [BLOCK_PERMISSION_RESIZE]           = BLK_PERM_RESIZE,
    };

    QEMU_BUILD_BUG_ON(ARRAY_SIZE(permissions) != BLOCK_PERMISSION__MAX);
    QEMU_BUILD_BUG_ON(1UL << ARRAY_SIZE(permissions) != BLK_PERM_ALL + 1);

    assert(qapi_perm < BLOCK_PERMISSION__MAX);

    return permissions[qapi_perm];
}

static void GRAPH_WRLOCK
bdrv_replace_child_noperm(BdrvChild *child, BlockDriverState *new_bs)
{
    BlockDriverState *old_bs = child->bs;
    int new_bs_quiesce_counter;

    assert(!child->frozen);

    assert(!new_bs || child->quiesced_parent);
    assert(old_bs != new_bs);
    GLOBAL_STATE_CODE();

    if (old_bs && new_bs) {
        assert(bdrv_get_aio_context(old_bs) == bdrv_get_aio_context(new_bs));
    }

    if (old_bs) {
        if (child->klass->detach) {
            child->klass->detach(child);
        }
        QLIST_REMOVE(child, next_parent);
    }

    child->bs = new_bs;

    if (new_bs) {
        QLIST_INSERT_HEAD(&new_bs->parents, child, next_parent);
        if (child->klass->attach) {
            child->klass->attach(child);
        }
    }

    new_bs_quiesce_counter = (new_bs ? new_bs->quiesce_counter : 0);
    if (!new_bs_quiesce_counter && child->quiesced_parent) {
        bdrv_parent_drained_end_single(child);
    }
}

static void bdrv_child_free(BdrvChild *child)
{
    assert(!child->bs);
    GLOBAL_STATE_CODE();
    GRAPH_RDLOCK_GUARD_MAINLOOP();

    assert(!child->next.le_prev); /* not in children list */

    g_free(child->name);
    g_free(child);
}

typedef struct BdrvAttachChildCommonState {
    BdrvChild *child;
    AioContext *old_parent_ctx;
    AioContext *old_child_ctx;
} BdrvAttachChildCommonState;

static void GRAPH_WRLOCK bdrv_attach_child_common_abort(void *opaque)
{
    BdrvAttachChildCommonState *s = opaque;
    BlockDriverState *bs = s->child->bs;

    GLOBAL_STATE_CODE();
    assert_bdrv_graph_writable();

    bdrv_replace_child_noperm(s->child, NULL);

    if (bdrv_get_aio_context(bs) != s->old_child_ctx) {
        bdrv_try_change_aio_context(bs, s->old_child_ctx, NULL, &error_abort);
    }

    if (bdrv_child_get_parent_aio_context(s->child) != s->old_parent_ctx) {
        Transaction *tran;
        GHashTable *visited;
        bool ret;

        tran = tran_new();

        visited = g_hash_table_new(NULL, NULL);
        ret = s->child->klass->change_aio_ctx(s->child, s->old_parent_ctx,
                                              visited, tran, &error_abort);
        g_hash_table_destroy(visited);

        assert(ret == true);
        tran_commit(tran);
    }

    bdrv_schedule_unref(bs);
    bdrv_child_free(s->child);
}

static TransactionActionDrv bdrv_attach_child_common_drv = {
    .abort = bdrv_attach_child_common_abort,
    .clean = g_free,
};

static BdrvChild * GRAPH_WRLOCK
bdrv_attach_child_common(BlockDriverState *child_bs,
                         const char *child_name,
                         const BdrvChildClass *child_class,
                         BdrvChildRole child_role,
                         uint64_t perm, uint64_t shared_perm,
                         void *opaque,
                         Transaction *tran, Error **errp)
{
    BdrvChild *new_child;
    AioContext *parent_ctx;
    AioContext *child_ctx = bdrv_get_aio_context(child_bs);

    assert(child_class->get_parent_desc);
    GLOBAL_STATE_CODE();

    if (bdrv_is_inactive(child_bs) && (perm & ~BLK_PERM_CONSISTENT_READ)) {
        g_autofree char *perm_names = bdrv_perm_names(perm);
        error_setg(errp, "Permission '%s' unavailable on inactive node",
                   perm_names);
        return NULL;
    }

    new_child = g_new(BdrvChild, 1);
    *new_child = (BdrvChild) {
        .bs             = NULL,
        .name           = g_strdup(child_name),
        .klass          = child_class,
        .role           = child_role,
        .perm           = perm,
        .shared_perm    = shared_perm,
        .opaque         = opaque,
    };

    parent_ctx = bdrv_child_get_parent_aio_context(new_child);
    if (child_ctx != parent_ctx) {
        Error *local_err = NULL;
        int ret = bdrv_try_change_aio_context(child_bs, parent_ctx, NULL,
                                              &local_err);

        if (ret < 0 && child_class->change_aio_ctx) {
            Transaction *aio_ctx_tran = tran_new();
            GHashTable *visited = g_hash_table_new(NULL, NULL);
            bool ret_child;

            g_hash_table_add(visited, new_child);
            ret_child = child_class->change_aio_ctx(new_child, child_ctx,
                                                    visited, aio_ctx_tran,
                                                    NULL);
            if (ret_child == true) {
                error_free(local_err);
                ret = 0;
            }
            tran_finalize(aio_ctx_tran, ret_child == true ? 0 : -1);
            g_hash_table_destroy(visited);
        }

        if (ret < 0) {
            error_propagate(errp, local_err);
            bdrv_child_free(new_child);
            return NULL;
        }
    }

    bdrv_ref(child_bs);
    bdrv_parent_drained_begin_single(new_child);
    bdrv_replace_child_noperm(new_child, child_bs);

    BdrvAttachChildCommonState *s = g_new(BdrvAttachChildCommonState, 1);
    *s = (BdrvAttachChildCommonState) {
        .child = new_child,
        .old_parent_ctx = parent_ctx,
        .old_child_ctx = child_ctx,
    };
    tran_add(tran, &bdrv_attach_child_common_drv, s);

    return new_child;
}

static BdrvChild * GRAPH_WRLOCK
bdrv_attach_child_noperm(BlockDriverState *parent_bs,
                         BlockDriverState *child_bs,
                         const char *child_name,
                         const BdrvChildClass *child_class,
                         BdrvChildRole child_role,
                         Transaction *tran,
                         Error **errp)
{
    uint64_t perm, shared_perm;

    assert(parent_bs->drv);
    GLOBAL_STATE_CODE();

    if (bdrv_recurse_has_child(child_bs, parent_bs)) {
        error_setg(errp, "Making '%s' a %s child of '%s' would create a cycle",
                   child_bs->node_name, child_name, parent_bs->node_name);
        return NULL;
    }
    if (bdrv_is_inactive(child_bs) && !bdrv_is_inactive(parent_bs)) {
        error_setg(errp, "Inactive '%s' can't be a %s child of active '%s'",
                   child_bs->node_name, child_name, parent_bs->node_name);
        return NULL;
    }

    bdrv_get_cumulative_perm(parent_bs, &perm, &shared_perm);
    bdrv_child_perm(parent_bs, child_bs, NULL, child_role, NULL,
                    perm, shared_perm, &perm, &shared_perm);

    return bdrv_attach_child_common(child_bs, child_name, child_class,
                                    child_role, perm, shared_perm, parent_bs,
                                    tran, errp);
}

BdrvChild *bdrv_root_attach_child(BlockDriverState *child_bs,
                                  const char *child_name,
                                  const BdrvChildClass *child_class,
                                  BdrvChildRole child_role,
                                  uint64_t perm, uint64_t shared_perm,
                                  void *opaque, Error **errp)
{
    int ret;
    BdrvChild *child;
    Transaction *tran = tran_new();

    GLOBAL_STATE_CODE();

    child = bdrv_attach_child_common(child_bs, child_name, child_class,
                                   child_role, perm, shared_perm, opaque,
                                   tran, errp);
    if (!child) {
        ret = -EINVAL;
        goto out;
    }

    ret = bdrv_refresh_perms(child_bs, tran, errp);

out:
    tran_finalize(tran, ret);

    bdrv_schedule_unref(child_bs);

    return ret < 0 ? NULL : child;
}

BdrvChild *bdrv_attach_child(BlockDriverState *parent_bs,
                             BlockDriverState *child_bs,
                             const char *child_name,
                             const BdrvChildClass *child_class,
                             BdrvChildRole child_role,
                             Error **errp)
{
    int ret;
    BdrvChild *child;
    Transaction *tran = tran_new();

    GLOBAL_STATE_CODE();

    child = bdrv_attach_child_noperm(parent_bs, child_bs, child_name,
                                     child_class, child_role, tran, errp);
    if (!child) {
        ret = -EINVAL;
        goto out;
    }

    ret = bdrv_refresh_perms(parent_bs, tran, errp);
    if (ret < 0) {
        goto out;
    }

out:
    tran_finalize(tran, ret);

    bdrv_schedule_unref(child_bs);

    return ret < 0 ? NULL : child;
}

void bdrv_root_unref_child(BdrvChild *child)
{
    BlockDriverState *child_bs = child->bs;

    GLOBAL_STATE_CODE();
    bdrv_replace_child_noperm(child, NULL);
    bdrv_child_free(child);

    if (child_bs) {
        bdrv_refresh_perms(child_bs, NULL, NULL);

        bdrv_try_change_aio_context(child_bs, qemu_get_aio_context(), NULL,
                                    NULL);
    }

    bdrv_schedule_unref(child_bs);
}

typedef struct BdrvSetInheritsFrom {
    BlockDriverState *bs;
    BlockDriverState *old_inherits_from;
} BdrvSetInheritsFrom;

static void bdrv_set_inherits_from_abort(void *opaque)
{
    BdrvSetInheritsFrom *s = opaque;

    s->bs->inherits_from = s->old_inherits_from;
}

static TransactionActionDrv bdrv_set_inherits_from_drv = {
    .abort = bdrv_set_inherits_from_abort,
    .clean = g_free,
};

static void bdrv_set_inherits_from(BlockDriverState *bs,
                                   BlockDriverState *new_inherits_from,
                                   Transaction *tran)
{
    if (tran) {
        BdrvSetInheritsFrom *s = g_new(BdrvSetInheritsFrom, 1);

        *s = (BdrvSetInheritsFrom) {
            .bs = bs,
            .old_inherits_from = bs->inherits_from,
        };

        tran_add(tran, &bdrv_set_inherits_from_drv, s);
    }

    bs->inherits_from = new_inherits_from;
}

static void GRAPH_WRLOCK
bdrv_unset_inherits_from(BlockDriverState *root, BdrvChild *child,
                         Transaction *tran)
{
    BdrvChild *c;

    if (child->bs->inherits_from == root) {
        QLIST_FOREACH(c, &root->children, next) {
            if (c != child && c->bs == child->bs) {
                break;
            }
        }
        if (c == NULL) {
            bdrv_set_inherits_from(child->bs, NULL, tran);
        }
    }

    QLIST_FOREACH(c, &child->bs->children, next) {
        bdrv_unset_inherits_from(root, c, tran);
    }
}

void bdrv_unref_child(BlockDriverState *parent, BdrvChild *child)
{
    GLOBAL_STATE_CODE();
    if (child == NULL) {
        return;
    }

    bdrv_unset_inherits_from(parent, child, NULL);
    bdrv_root_unref_child(child);
}


static void GRAPH_RDLOCK
bdrv_parent_cb_change_media(BlockDriverState *bs, bool load)
{
    BdrvChild *c;
    GLOBAL_STATE_CODE();
    QLIST_FOREACH(c, &bs->parents, next_parent) {
        if (c->klass->change_media) {
            c->klass->change_media(c, load);
        }
    }
}

static bool bdrv_inherits_from_recursive(BlockDriverState *child,
                                         BlockDriverState *parent)
{
    while (child && child != parent) {
        child = child->inherits_from;
    }

    return child != NULL;
}

static BdrvChildRole bdrv_backing_role(BlockDriverState *bs)
{
    if (bs->drv && bs->drv->is_filter) {
        return BDRV_CHILD_FILTERED | BDRV_CHILD_PRIMARY;
    } else {
        return BDRV_CHILD_COW;
    }
}

static int GRAPH_WRLOCK
bdrv_set_file_or_backing_noperm(BlockDriverState *parent_bs,
                                BlockDriverState *child_bs,
                                bool is_backing,
                                Transaction *tran, Error **errp)
{
    bool update_inherits_from =
        bdrv_inherits_from_recursive(child_bs, parent_bs);
    BdrvChild *child = is_backing ? parent_bs->backing : parent_bs->file;
    BdrvChildRole role;

    GLOBAL_STATE_CODE();

    if (!parent_bs->drv) {
        error_setg(errp, "Node corrupted");
        return -EINVAL;
    }

    if (child && child->frozen) {
        error_setg(errp, "Cannot change frozen '%s' link from '%s' to '%s'",
                   child->name, parent_bs->node_name, child->bs->node_name);
        return -EPERM;
    }

    if (is_backing && !parent_bs->drv->is_filter &&
        !parent_bs->drv->supports_backing)
    {
        error_setg(errp, "Driver '%s' of node '%s' does not support backing "
                   "files", parent_bs->drv->format_name, parent_bs->node_name);
        return -EINVAL;
    }

    if (parent_bs->drv->is_filter) {
        role = BDRV_CHILD_FILTERED | BDRV_CHILD_PRIMARY;
    } else if (is_backing) {
        role = BDRV_CHILD_COW;
    } else {
        if (!child) {
            error_setg(errp, "Cannot set file child to format node without "
                       "file child");
            return -EINVAL;
        }
        role = child->role;
    }

    if (child) {
        assert(child->bs->quiesce_counter);
        bdrv_unset_inherits_from(parent_bs, child, tran);
        bdrv_remove_child(child, tran);
    }

    if (!child_bs) {
        goto out;
    }

    child = bdrv_attach_child_noperm(parent_bs, child_bs,
                                     is_backing ? "backing" : "file",
                                     &child_of_bds, role,
                                     tran, errp);
    if (!child) {
        return -EINVAL;
    }


    if (update_inherits_from) {
        bdrv_set_inherits_from(child_bs, parent_bs, tran);
    }

out:
    bdrv_refresh_limits(parent_bs, tran, NULL);

    return 0;
}

int bdrv_set_backing_hd_drained(BlockDriverState *bs,
                                BlockDriverState *backing_hd,
                                Error **errp)
{
    int ret;
    Transaction *tran = tran_new();

    GLOBAL_STATE_CODE();
    assert(bs->quiesce_counter > 0);
    if (bs->backing) {
        assert(bs->backing->bs->quiesce_counter > 0);
    }

    ret = bdrv_set_file_or_backing_noperm(bs, backing_hd, true, tran, errp);
    if (ret < 0) {
        goto out;
    }

    ret = bdrv_refresh_perms(bs, tran, errp);
out:
    tran_finalize(tran, ret);
    return ret;
}

int bdrv_set_backing_hd(BlockDriverState *bs, BlockDriverState *backing_hd,
                        Error **errp)
{
    BlockDriverState *drain_bs;
    int ret;
    GLOBAL_STATE_CODE();

    bdrv_graph_rdlock_main_loop();
    drain_bs = bs->backing ? bs->backing->bs : bs;
    bdrv_graph_rdunlock_main_loop();

    bdrv_ref(drain_bs);
    bdrv_drained_begin(drain_bs);
    bdrv_graph_wrlock();
    ret = bdrv_set_backing_hd_drained(bs, backing_hd, errp);
    bdrv_graph_wrunlock();
    bdrv_drained_end(drain_bs);
    bdrv_unref(drain_bs);

    return ret;
}

int bdrv_open_backing_file(BlockDriverState *bs, QDict *parent_options,
                           const char *bdref_key, Error **errp)
{
    ERRP_GUARD();
    char *backing_filename = NULL;
    char *bdref_key_dot;
    const char *reference = NULL;
    int ret = 0;
    bool implicit_backing = false;
    BlockDriverState *backing_hd;
    QDict *options;
    QDict *tmp_parent_options = NULL;
    Error *local_err = NULL;

    GLOBAL_STATE_CODE();
    GRAPH_RDLOCK_GUARD_MAINLOOP();

    if (bs->backing != NULL) {
        goto free_exit;
    }

    if (parent_options == NULL) {
        tmp_parent_options = qdict_new();
        parent_options = tmp_parent_options;
    }

    bs->open_flags &= ~BDRV_O_NO_BACKING;

    bdref_key_dot = g_strdup_printf("%s.", bdref_key);
    qdict_extract_subqdict(parent_options, &options, bdref_key_dot);
    g_free(bdref_key_dot);

    reference = qdict_get_try_str(parent_options, bdref_key);
    if (reference || qdict_haskey(options, "file.filename")) {
    } else if (bs->backing_file[0] == '\0' && qdict_size(options) == 0) {
        qobject_unref(options);
        goto free_exit;
    } else {
        if (qdict_size(options) == 0) {
            implicit_backing = !strcmp(bs->auto_backing_file, bs->backing_file);
        }

        backing_filename = bdrv_get_full_backing_filename(bs, &local_err);
        if (local_err) {
            ret = -EINVAL;
            error_propagate(errp, local_err);
            qobject_unref(options);
            goto free_exit;
        }
    }

    if (!bs->drv || !bs->drv->supports_backing) {
        ret = -EINVAL;
        error_setg(errp, "Driver doesn't support backing files");
        qobject_unref(options);
        goto free_exit;
    }

    if (!reference &&
        bs->backing_format[0] != '\0' && !qdict_haskey(options, "driver")) {
        qdict_put_str(options, "driver", bs->backing_format);
    }

    backing_hd = bdrv_open_inherit(backing_filename, reference, options, 0, bs,
                                   &child_of_bds, bdrv_backing_role(bs), true,
                                   errp);
    if (!backing_hd) {
        bs->open_flags |= BDRV_O_NO_BACKING;
        error_prepend(errp, "Could not open backing file: ");
        ret = -EINVAL;
        goto free_exit;
    }

    if (implicit_backing) {
        bdrv_refresh_filename(backing_hd);
        pstrcpy(bs->auto_backing_file, sizeof(bs->auto_backing_file),
                backing_hd->filename);
    }

    ret = bdrv_set_backing_hd(bs, backing_hd, errp);
    bdrv_unref(backing_hd);

    if (ret < 0) {
        goto free_exit;
    }

    qdict_del(parent_options, bdref_key);

free_exit:
    g_free(backing_filename);
    qobject_unref(tmp_parent_options);
    return ret;
}

static BlockDriverState *
bdrv_open_child_bs(const char *filename, QDict *options, const char *bdref_key,
                   BlockDriverState *parent, const BdrvChildClass *child_class,
                   BdrvChildRole child_role, bool allow_none,
                   bool parse_filename, Error **errp)
{
    BlockDriverState *bs = NULL;
    QDict *image_options;
    char *bdref_key_dot;
    const char *reference;

    assert(child_class != NULL);

    bdref_key_dot = g_strdup_printf("%s.", bdref_key);
    qdict_extract_subqdict(options, &image_options, bdref_key_dot);
    g_free(bdref_key_dot);

    reference = qdict_get_try_str(options, bdref_key);
    if (!filename && !reference && !qdict_size(image_options)) {
        if (!allow_none) {
            error_setg(errp, "A block device must be specified for \"%s\"",
                       bdref_key);
        }
        qobject_unref(image_options);
        goto done;
    }

    bs = bdrv_open_inherit(filename, reference, image_options, 0,
                           parent, child_class, child_role, parse_filename,
                           errp);
    if (!bs) {
        goto done;
    }

done:
    qdict_del(options, bdref_key);
    return bs;
}

static BdrvChild *bdrv_open_child_common(const char *filename,
                                         QDict *options, const char *bdref_key,
                                         BlockDriverState *parent,
                                         const BdrvChildClass *child_class,
                                         BdrvChildRole child_role,
                                         bool allow_none, bool parse_filename,
                                         Error **errp)
{
    BlockDriverState *bs;
    BdrvChild *child;

    GLOBAL_STATE_CODE();

    bs = bdrv_open_child_bs(filename, options, bdref_key, parent, child_class,
                            child_role, allow_none, parse_filename, errp);
    if (bs == NULL) {
        return NULL;
    }

    bdrv_graph_wrlock();
    child = bdrv_attach_child(parent, bs, bdref_key, child_class, child_role,
                              errp);
    bdrv_graph_wrunlock();

    return child;
}

BdrvChild *bdrv_open_child(const char *filename,
                           QDict *options, const char *bdref_key,
                           BlockDriverState *parent,
                           const BdrvChildClass *child_class,
                           BdrvChildRole child_role,
                           bool allow_none, Error **errp)
{
    return bdrv_open_child_common(filename, options, bdref_key, parent,
                                  child_class, child_role, allow_none, false,
                                  errp);
}

int bdrv_open_file_child(const char *filename,
                         QDict *options, const char *bdref_key,
                         BlockDriverState *parent, Error **errp)
{
    BdrvChildRole role;

    assert(!parent->drv->filtered_child_is_backing);
    role = parent->drv->is_filter ?
        (BDRV_CHILD_FILTERED | BDRV_CHILD_PRIMARY) : BDRV_CHILD_IMAGE;

    if (!bdrv_open_child_common(filename, options, bdref_key, parent,
                                &child_of_bds, role, false, true, errp))
    {
        return -EINVAL;
    }

    return 0;
}

BlockDriverState *bdrv_open_blockdev_ref(BlockdevRef *ref, Error **errp)
{
    BlockDriverState *bs = NULL;
    QObject *obj = NULL;
    QDict *qdict = NULL;
    const char *reference = NULL;
    Visitor *v = NULL;

    GLOBAL_STATE_CODE();

    if (ref->type == QTYPE_QSTRING) {
        reference = ref->u.reference;
    } else {
        BlockdevOptions *options = &ref->u.definition;
        assert(ref->type == QTYPE_QDICT);

        v = qobject_output_visitor_new(&obj);
        visit_type_BlockdevOptions(v, NULL, &options, &error_abort);
        visit_complete(v, &obj);

        qdict = qobject_to(QDict, obj);
        qdict_flatten(qdict);

        qdict_set_default_str(qdict, BDRV_OPT_CACHE_DIRECT, "off");
        qdict_set_default_str(qdict, BDRV_OPT_CACHE_NO_FLUSH, "off");
        qdict_set_default_str(qdict, BDRV_OPT_READ_ONLY, "off");
        qdict_set_default_str(qdict, BDRV_OPT_AUTO_READ_ONLY, "off");

    }

    bs = bdrv_open_inherit(NULL, reference, qdict, 0, NULL, NULL, 0, false,
                           errp);
    obj = NULL;
    qobject_unref(obj);
    visit_free(v);
    return bs;
}

static BlockDriverState * no_coroutine_fn
bdrv_open_inherit(const char *filename, const char *reference, QDict *options,
                  int flags, BlockDriverState *parent,
                  const BdrvChildClass *child_class, BdrvChildRole child_role,
                  bool parse_filename, Error **errp)
{
    int ret;
    BlockBackend *file = NULL;
    BlockDriverState *bs;
    BlockDriver *drv = NULL;
    BdrvChild *child;
    const char *drvname;
    const char *backing;
    Error *local_err = NULL;

    assert(!child_class || !flags);
    assert(!child_class == !parent);
    GLOBAL_STATE_CODE();
    assert(!qemu_in_coroutine());

    GRAPH_RDLOCK_GUARD_MAINLOOP();

    if (reference) {
        bool options_non_empty = options ? qdict_size(options) : false;
        qobject_unref(options);

        if (filename || options_non_empty) {
            error_setg(errp, "Cannot reference an existing block device with "
                       "additional options or a new filename");
            return NULL;
        }

        bs = bdrv_lookup_bs(reference, reference, errp);
        if (!bs) {
            return NULL;
        }

        bdrv_ref(bs);
        return bs;
    }

    bs = bdrv_new();

    if (options == NULL) {
        options = qdict_new();
    }

    if (parse_filename) {
        parse_json_protocol(options, &filename, &local_err);
        if (local_err) {
            goto fail;
        }
    }

    bs->explicit_options = qdict_clone_shallow(options);

    if (child_class) {
        bool parent_is_format;

        if (parent->drv) {
            parent_is_format = parent->drv->is_format;
        } else {
            parent_is_format = true;
        }

        bs->inherits_from = parent;
        child_class->inherit_options(child_role, parent_is_format,
                                     &flags, options,
                                     parent->open_flags, parent->options);
    }

    ret = bdrv_fill_options(&options, filename, &flags, parse_filename,
                            &local_err);
    if (ret < 0) {
        goto fail;
    }

    if (g_strcmp0(qdict_get_try_str(options, BDRV_OPT_READ_ONLY), "on") &&
        !qdict_get_try_bool(options, BDRV_OPT_READ_ONLY, false)) {
        flags |= (BDRV_O_RDWR | BDRV_O_ALLOW_RDWR);
    } else {
        flags &= ~BDRV_O_RDWR;
    }

    bs->open_flags = flags;
    bs->options = options;
    options = qdict_clone_shallow(options);

    drvname = qdict_get_try_str(options, "driver");
    if (drvname) {
        drv = bdrv_find_format(drvname);
        if (!drv) {
            error_setg(errp, "Unknown driver: '%s'", drvname);
            goto fail;
        }
    }

    assert(drvname || !(flags & BDRV_O_PROTOCOL));

    backing = qdict_get_try_str(options, "backing");
    if (qobject_to(QNull, qdict_get(options, "backing")) != NULL ||
        (backing && *backing == '\0'))
    {
        if (backing) {
            warn_report("Use of \"backing\": \"\" is deprecated; "
                        "use \"backing\": null instead");
        }
        flags |= BDRV_O_NO_BACKING;
        qdict_del(bs->explicit_options, "backing");
        qdict_del(bs->options, "backing");
        qdict_del(options, "backing");
    }

    if ((flags & BDRV_O_PROTOCOL) == 0) {
        BlockDriverState *file_bs;

        file_bs = bdrv_open_child_bs(filename, options, "file", bs,
                                     &child_of_bds, BDRV_CHILD_IMAGE,
                                     true, true, &local_err);
        if (local_err) {
            goto fail;
        }
        if (file_bs != NULL) {
            AioContext *ctx = bdrv_get_aio_context(file_bs);
            file = blk_new(ctx, 0, BLK_PERM_ALL);
            blk_insert_bs(file, file_bs, &local_err);
            bdrv_unref(file_bs);

            if (local_err) {
                goto fail;
            }

            qdict_put_str(options, "file", bdrv_get_node_name(file_bs));
        }
    }

    bs->probed = !drv;
    if (!drv && file) {
        ret = find_image_format(file, filename, &drv, &local_err);
        if (ret < 0) {
            goto fail;
        }
        qdict_put_str(bs->options, "driver", drv->format_name);
        qdict_put_str(options, "driver", drv->format_name);
    } else if (!drv) {
        error_setg(errp, "Must specify either driver or file");
        goto fail;
    }

    assert(!!(flags & BDRV_O_PROTOCOL) == !!drv->protocol_name);
    assert(!(flags & BDRV_O_PROTOCOL) || !file);

    ret = bdrv_open_common(bs, file, options, &local_err);
    if (ret < 0) {
        goto fail;
    }

    if (file) {
        blk_unref(file);
        file = NULL;
    }

    if ((flags & BDRV_O_NO_BACKING) == 0) {
        ret = bdrv_open_backing_file(bs, options, "backing", &local_err);
        if (ret < 0) {
            goto close_and_fail;
        }
    }

    QLIST_FOREACH(child, &bs->children, next) {
        char *child_key_dot;
        child_key_dot = g_strdup_printf("%s.", child->name);
        qdict_extract_subqdict(bs->explicit_options, NULL, child_key_dot);
        qdict_extract_subqdict(bs->options, NULL, child_key_dot);
        qdict_del(bs->explicit_options, child->name);
        qdict_del(bs->options, child->name);
        g_free(child_key_dot);
    }

    if (qdict_size(options) != 0) {
        const QDictEntry *entry = qdict_first(options);
        if (flags & BDRV_O_PROTOCOL) {
            error_setg(errp, "Block protocol '%s' doesn't support the option "
                       "'%s'", drv->format_name, entry->key);
        } else {
            error_setg(errp,
                       "Block format '%s' does not support the option '%s'",
                       drv->format_name, entry->key);
        }

        goto close_and_fail;
    }

    bdrv_parent_cb_change_media(bs, true);

    qobject_unref(options);
    options = NULL;

    return bs;

fail:
    blk_unref(file);
    qobject_unref(bs->explicit_options);
    qobject_unref(bs->options);
    qobject_unref(options);
    bs->options = NULL;
    bs->explicit_options = NULL;
    bdrv_unref(bs);
    error_propagate(errp, local_err);
    return NULL;

close_and_fail:
    bdrv_unref(bs);
    qobject_unref(options);
    error_propagate(errp, local_err);
    return NULL;
}

BlockDriverState *bdrv_open(const char *filename, const char *reference,
                            QDict *options, int flags, Error **errp)
{
    GLOBAL_STATE_CODE();

    return bdrv_open_inherit(filename, reference, options, flags, NULL,
                             NULL, 0, true, errp);
}

static bool is_str_in_list(const char *str, const char *const *list)
{
    if (str && list) {
        int i;
        for (i = 0; list[i] != NULL; i++) {
            if (!strcmp(str, list[i])) {
                return true;
            }
        }
    }
    return false;
}

static int bdrv_reset_options_allowed(BlockDriverState *bs,
                                      const QDict *new_opts, Error **errp)
{
    const QDictEntry *e;
    const char *const common_options[] = {
        "node-name", "discard", "cache.direct", "cache.no-flush",
        "read-only", "auto-read-only", "detect-zeroes", NULL
    };

    for (e = qdict_first(bs->options); e; e = qdict_next(bs->options, e)) {
        if (!qdict_haskey(new_opts, e->key) &&
            !is_str_in_list(e->key, common_options) &&
            !is_str_in_list(e->key, bs->drv->mutable_opts)) {
            error_setg(errp, "Option '%s' cannot be reset "
                       "to its default value", e->key);
            return -EINVAL;
        }
    }

    return 0;
}

static bool GRAPH_RDLOCK
bdrv_recurse_has_child(BlockDriverState *bs, BlockDriverState *child)
{
    BdrvChild *c;

    if (bs == child) {
        return true;
    }

    QLIST_FOREACH(c, &bs->children, next) {
        if (bdrv_recurse_has_child(c->bs, child)) {
            return true;
        }
    }

    return false;
}

static BlockReopenQueue * GRAPH_RDLOCK
bdrv_reopen_queue_child(BlockReopenQueue *bs_queue, BlockDriverState *bs,
                        QDict *options, const BdrvChildClass *klass,
                        BdrvChildRole role, bool parent_is_format,
                        QDict *parent_options, int parent_flags,
                        bool keep_old_opts)
{
    assert(bs != NULL);

    BlockReopenQueueEntry *bs_entry;
    BdrvChild *child;
    QDict *old_options, *explicit_options, *options_copy;
    int flags;
    QemuOpts *opts;

    GLOBAL_STATE_CODE();

    bdrv_drained_begin(bs);

    if (bs_queue == NULL) {
        bs_queue = g_new0(BlockReopenQueue, 1);
        QTAILQ_INIT(bs_queue);
    }

    if (!options) {
        options = qdict_new();
    }

    QTAILQ_FOREACH(bs_entry, bs_queue, entry) {
        if (bs == bs_entry->state.bs) {
            break;
        }
    }


    if (bs_entry || keep_old_opts) {
        old_options = qdict_clone_shallow(bs_entry ?
                                          bs_entry->state.explicit_options :
                                          bs->explicit_options);
        bdrv_join_options(bs, options, old_options);
        qobject_unref(old_options);
    }

    explicit_options = qdict_clone_shallow(options);

    if (parent_options) {
        flags = 0;
        klass->inherit_options(role, parent_is_format, &flags, options,
                               parent_flags, parent_options);
    } else {
        flags = bdrv_get_flags(bs);
    }

    if (keep_old_opts) {
        old_options = qdict_clone_shallow(bs->options);
        bdrv_join_options(bs, options, old_options);
        qobject_unref(old_options);
    }

    options_copy = qdict_clone_shallow(options);
    opts = qemu_opts_create(&bdrv_runtime_opts, NULL, 0, &error_abort);
    qemu_opts_absorb_qdict(opts, options_copy, NULL);
    update_flags_from_options(&flags, opts);
    qemu_opts_del(opts);
    qobject_unref(options_copy);

    flags &= ~BDRV_O_PROTOCOL;
    if (flags & BDRV_O_RDWR) {
        flags |= BDRV_O_ALLOW_RDWR;
    }

    if (!bs_entry) {
        bs_entry = g_new0(BlockReopenQueueEntry, 1);
        QTAILQ_INSERT_TAIL(bs_queue, bs_entry, entry);
    } else {
        qobject_unref(bs_entry->state.options);
        qobject_unref(bs_entry->state.explicit_options);
    }

    bs_entry->state.bs = bs;
    bs_entry->state.options = options;
    bs_entry->state.explicit_options = explicit_options;
    bs_entry->state.flags = flags;

    if (!keep_old_opts) {
        bs_entry->state.backing_missing =
            !qdict_haskey(options, "backing") &&
            !qdict_haskey(options, "backing.driver");
    }

    QLIST_FOREACH(child, &bs->children, next) {
        QDict *new_child_options = NULL;
        bool child_keep_old = keep_old_opts;

        if (child->bs->inherits_from != bs) {
            continue;
        }

        if (qdict_haskey(options, child->name)) {
            const char *childref = qdict_get_try_str(options, child->name);
            if (g_strcmp0(childref, child->bs->node_name)) {
                continue;
            }
            child_keep_old = true;
        } else {
            char *child_key_dot = g_strdup_printf("%s.", child->name);
            qdict_extract_subqdict(explicit_options, NULL, child_key_dot);
            qdict_extract_subqdict(options, &new_child_options, child_key_dot);
            g_free(child_key_dot);
        }

        bdrv_reopen_queue_child(bs_queue, child->bs, new_child_options,
                                child->klass, child->role, bs->drv->is_format,
                                options, flags, child_keep_old);
    }

    return bs_queue;
}

BlockReopenQueue *bdrv_reopen_queue(BlockReopenQueue *bs_queue,
                                    BlockDriverState *bs,
                                    QDict *options, bool keep_old_opts)
{
    GLOBAL_STATE_CODE();
    GRAPH_RDLOCK_GUARD_MAINLOOP();

    return bdrv_reopen_queue_child(bs_queue, bs, options, NULL, 0, false,
                                   NULL, 0, keep_old_opts);
}

void bdrv_reopen_queue_free(BlockReopenQueue *bs_queue)
{
    GLOBAL_STATE_CODE();
    if (bs_queue) {
        BlockReopenQueueEntry *bs_entry, *next;
        QTAILQ_FOREACH_SAFE(bs_entry, bs_queue, entry, next) {
            bdrv_drained_end(bs_entry->state.bs);
            qobject_unref(bs_entry->state.explicit_options);
            qobject_unref(bs_entry->state.options);
            g_free(bs_entry);
        }
        g_free(bs_queue);
    }
}

int bdrv_reopen_multiple(BlockReopenQueue *bs_queue, Error **errp)
{
    int ret = -1;
    BlockReopenQueueEntry *bs_entry, *next;
    Transaction *tran = tran_new();
    g_autoptr(GSList) refresh_list = NULL;

    assert(qemu_get_current_aio_context() == qemu_get_aio_context());
    assert(bs_queue != NULL);
    GLOBAL_STATE_CODE();

    QTAILQ_FOREACH(bs_entry, bs_queue, entry) {
        ret = bdrv_flush(bs_entry->state.bs);
        if (ret < 0) {
            error_setg_errno(errp, -ret, "Error flushing drive");
            goto abort;
        }
    }

    QTAILQ_FOREACH(bs_entry, bs_queue, entry) {
        assert(bs_entry->state.bs->quiesce_counter > 0);
        ret = bdrv_reopen_prepare(&bs_entry->state, bs_queue, tran, errp);
        if (ret < 0) {
            goto abort;
        }
        bs_entry->prepared = true;
    }

    QTAILQ_FOREACH(bs_entry, bs_queue, entry) {
        BDRVReopenState *state = &bs_entry->state;

        refresh_list = g_slist_prepend(refresh_list, state->bs);
        if (state->old_backing_bs) {
            refresh_list = g_slist_prepend(refresh_list, state->old_backing_bs);
        }
        if (state->old_file_bs) {
            refresh_list = g_slist_prepend(refresh_list, state->old_file_bs);
        }
    }

    bdrv_graph_rdlock_main_loop();
    ret = bdrv_list_refresh_perms(refresh_list, bs_queue, tran, errp);
    bdrv_graph_rdunlock_main_loop();

    if (ret < 0) {
        goto abort;
    }

    QTAILQ_FOREACH_REVERSE(bs_entry, bs_queue, entry) {
        bdrv_reopen_commit(&bs_entry->state);
    }

    bdrv_graph_wrlock();
    tran_commit(tran);
    bdrv_graph_wrunlock();

    QTAILQ_FOREACH_REVERSE(bs_entry, bs_queue, entry) {
        BlockDriverState *bs = bs_entry->state.bs;

        if (bs->drv->bdrv_reopen_commit_post) {
            bs->drv->bdrv_reopen_commit_post(&bs_entry->state);
        }
    }

    ret = 0;
    goto cleanup;

abort:
    bdrv_graph_wrlock();
    tran_abort(tran);
    bdrv_graph_wrunlock();

    QTAILQ_FOREACH_SAFE(bs_entry, bs_queue, entry, next) {
        if (bs_entry->prepared) {
            bdrv_reopen_abort(&bs_entry->state);
        }
    }

cleanup:
    bdrv_reopen_queue_free(bs_queue);

    return ret;
}

int bdrv_reopen(BlockDriverState *bs, QDict *opts, bool keep_old_opts,
                Error **errp)
{
    BlockReopenQueue *queue;

    GLOBAL_STATE_CODE();

    queue = bdrv_reopen_queue(NULL, bs, opts, keep_old_opts);

    return bdrv_reopen_multiple(queue, errp);
}

int bdrv_reopen_set_read_only(BlockDriverState *bs, bool read_only,
                              Error **errp)
{
    QDict *opts = qdict_new();

    GLOBAL_STATE_CODE();

    qdict_put_bool(opts, BDRV_OPT_READ_ONLY, read_only);

    return bdrv_reopen(bs, opts, true, errp);
}

static int GRAPH_UNLOCKED
bdrv_reopen_parse_file_or_backing(BDRVReopenState *reopen_state,
                                  bool is_backing, Transaction *tran,
                                  Error **errp)
{
    BlockDriverState *bs = reopen_state->bs;
    BlockDriverState *new_child_bs;
    BlockDriverState *old_child_bs;

    const char *child_name = is_backing ? "backing" : "file";
    QObject *value;
    const char *str;
    bool has_child;
    int ret;

    GLOBAL_STATE_CODE();

    value = qdict_get(reopen_state->options, child_name);
    if (value == NULL) {
        return 0;
    }

    bdrv_graph_rdlock_main_loop();

    switch (qobject_type(value)) {
    case QTYPE_QNULL:
        assert(is_backing); /* The 'file' option does not allow a null value */
        new_child_bs = NULL;
        break;
    case QTYPE_QSTRING:
        str = qstring_get_str(qobject_to(QString, value));
        new_child_bs = bdrv_lookup_bs(NULL, str, errp);
        if (new_child_bs == NULL) {
            ret = -EINVAL;
            goto out_rdlock;
        }

        has_child = bdrv_recurse_has_child(new_child_bs, bs);
        if (has_child) {
            error_setg(errp, "Making '%s' a %s child of '%s' would create a "
                       "cycle", str, child_name, bs->node_name);
            ret = -EINVAL;
            goto out_rdlock;
        }
        break;
    default:
        g_assert_not_reached();
    }

    old_child_bs = is_backing ? child_bs(bs->backing) : child_bs(bs->file);
    if (old_child_bs == new_child_bs) {
        ret = 0;
        goto out_rdlock;
    }

    if (old_child_bs) {
        if (bdrv_skip_implicit_filters(old_child_bs) == new_child_bs) {
            ret = 0;
            goto out_rdlock;
        }

        if (old_child_bs->implicit) {
            error_setg(errp, "Cannot replace implicit %s child of %s",
                       child_name, bs->node_name);
            ret = -EPERM;
            goto out_rdlock;
        }
    }

    if (bs->drv->is_filter && !old_child_bs) {
        error_setg(errp, "'%s' is a %s filter node that does not support a "
                   "%s child", bs->node_name, bs->drv->format_name, child_name);
        ret = -EINVAL;
        goto out_rdlock;
    }

    if (is_backing) {
        reopen_state->old_backing_bs = old_child_bs;
    } else {
        reopen_state->old_file_bs = old_child_bs;
    }

    if (old_child_bs) {
        bdrv_ref(old_child_bs);
        bdrv_drained_begin(old_child_bs);
    }

    bdrv_graph_rdunlock_main_loop();
    bdrv_graph_wrlock();

    ret = bdrv_set_file_or_backing_noperm(bs, new_child_bs, is_backing,
                                          tran, errp);

    bdrv_graph_wrunlock();

    if (old_child_bs) {
        bdrv_drained_end(old_child_bs);
        bdrv_unref(old_child_bs);
    }

    return ret;

out_rdlock:
    bdrv_graph_rdunlock_main_loop();
    return ret;
}

static int GRAPH_UNLOCKED
bdrv_reopen_prepare(BDRVReopenState *reopen_state, BlockReopenQueue *queue,
                    Transaction *change_child_tran, Error **errp)
{
    int ret = -1;
    int old_flags;
    Error *local_err = NULL;
    BlockDriver *drv;
    QemuOpts *opts;
    QDict *orig_reopen_opts;
    char *discard = NULL;
    bool read_only;
    bool drv_prepared = false;

    assert(reopen_state != NULL);
    assert(reopen_state->bs->drv != NULL);
    GLOBAL_STATE_CODE();
    drv = reopen_state->bs->drv;

    orig_reopen_opts = qdict_clone_shallow(reopen_state->options);

    opts = qemu_opts_create(&bdrv_runtime_opts, NULL, 0, &error_abort);
    if (!qemu_opts_absorb_qdict(opts, reopen_state->options, errp)) {
        ret = -EINVAL;
        goto error;
    }

    old_flags = reopen_state->flags;
    update_flags_from_options(&reopen_state->flags, opts);
    assert(old_flags == reopen_state->flags);

    discard = qemu_opt_get_del(opts, BDRV_OPT_DISCARD);
    if (discard != NULL) {
        if (bdrv_parse_discard_flags(discard, &reopen_state->flags) != 0) {
            error_setg(errp, "Invalid discard option");
            ret = -EINVAL;
            goto error;
        }
    }

    reopen_state->detect_zeroes =
        bdrv_parse_detect_zeroes(opts, reopen_state->flags, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        ret = -EINVAL;
        goto error;
    }

    qemu_opts_to_qdict(opts, reopen_state->options);

    read_only = !(reopen_state->flags & BDRV_O_RDWR);

    bdrv_graph_rdlock_main_loop();
    ret = bdrv_can_set_read_only(reopen_state->bs, read_only, true, &local_err);
    bdrv_graph_rdunlock_main_loop();
    if (local_err) {
        error_propagate(errp, local_err);
        goto error;
    }

    if (drv->bdrv_reopen_prepare) {
        ret = bdrv_reset_options_allowed(reopen_state->bs,
                                         reopen_state->options, errp);
        if (ret) {
            goto error;
        }

        ret = drv->bdrv_reopen_prepare(reopen_state, queue, &local_err);
        if (ret) {
            if (local_err != NULL) {
                error_propagate(errp, local_err);
            } else {
                bdrv_graph_rdlock_main_loop();
                bdrv_refresh_filename(reopen_state->bs);
                bdrv_graph_rdunlock_main_loop();
                error_setg(errp, "failed while preparing to reopen image '%s'",
                           reopen_state->bs->filename);
            }
            goto error;
        }
    } else {
        bdrv_graph_rdlock_main_loop();
        error_setg(errp, "Block format '%s' used by node '%s' "
                   "does not support reopening files", drv->format_name,
                   bdrv_get_device_or_node_name(reopen_state->bs));
        bdrv_graph_rdunlock_main_loop();
        ret = -1;
        goto error;
    }

    drv_prepared = true;

    bdrv_graph_rdlock_main_loop();
    if (drv->supports_backing && reopen_state->backing_missing &&
        (reopen_state->bs->backing || reopen_state->bs->backing_file[0])) {
        error_setg(errp, "backing is missing for '%s'",
                   reopen_state->bs->node_name);
        bdrv_graph_rdunlock_main_loop();
        ret = -EINVAL;
        goto error;
    }
    bdrv_graph_rdunlock_main_loop();

    ret = bdrv_reopen_parse_file_or_backing(reopen_state, true,
                                            change_child_tran, errp);
    if (ret < 0) {
        goto error;
    }
    qdict_del(reopen_state->options, "backing");

    ret = bdrv_reopen_parse_file_or_backing(reopen_state, false,
                                            change_child_tran, errp);
    if (ret < 0) {
        goto error;
    }
    qdict_del(reopen_state->options, "file");

    if (qdict_size(reopen_state->options)) {
        const QDictEntry *entry = qdict_first(reopen_state->options);

        GRAPH_RDLOCK_GUARD_MAINLOOP();

        do {
            QObject *new = entry->value;
            QObject *old = qdict_get(reopen_state->bs->options, entry->key);

            if (qobject_type(new) == QTYPE_QSTRING) {
                BdrvChild *child;
                QLIST_FOREACH(child, &reopen_state->bs->children, next) {
                    if (!strcmp(child->name, entry->key)) {
                        break;
                    }
                }

                if (child) {
                    if (!strcmp(child->bs->node_name,
                                qstring_get_str(qobject_to(QString, new)))) {
                        continue; /* Found child with this name, skip option */
                    }
                }
            }

            if (!qobject_is_equal(new, old)) {
                error_setg(errp, "Cannot change the option '%s'", entry->key);
                ret = -EINVAL;
                goto error;
            }
        } while ((entry = qdict_next(reopen_state->options, entry)));
    }

    ret = 0;

    qobject_unref(reopen_state->options);
    reopen_state->options = qobject_ref(orig_reopen_opts);

error:
    if (ret < 0 && drv_prepared) {
        if (drv->bdrv_reopen_abort) {
            drv->bdrv_reopen_abort(reopen_state);
        }
    }
    qemu_opts_del(opts);
    qobject_unref(orig_reopen_opts);
    g_free(discard);
    return ret;
}

static void GRAPH_UNLOCKED bdrv_reopen_commit(BDRVReopenState *reopen_state)
{
    BlockDriver *drv;
    BlockDriverState *bs;
    BdrvChild *child;

    assert(reopen_state != NULL);
    bs = reopen_state->bs;
    drv = bs->drv;
    assert(drv != NULL);
    GLOBAL_STATE_CODE();

    if (drv->bdrv_reopen_commit) {
        drv->bdrv_reopen_commit(reopen_state);
    }

    GRAPH_RDLOCK_GUARD_MAINLOOP();

    qobject_unref(bs->explicit_options);
    qobject_unref(bs->options);
    qobject_ref(reopen_state->explicit_options);
    qobject_ref(reopen_state->options);

    bs->explicit_options   = reopen_state->explicit_options;
    bs->options            = reopen_state->options;
    bs->open_flags         = reopen_state->flags;
    bs->detect_zeroes      = reopen_state->detect_zeroes;

    QLIST_FOREACH(child, &bs->children, next) {
        qdict_del(bs->explicit_options, child->name);
        qdict_del(bs->options, child->name);
    }
    qdict_del(bs->explicit_options, "backing");
    qdict_del(bs->options, "backing");

    bdrv_refresh_limits(bs, NULL, NULL);
    bdrv_refresh_total_sectors(bs, bs->total_sectors);
}

static void GRAPH_UNLOCKED bdrv_reopen_abort(BDRVReopenState *reopen_state)
{
    BlockDriver *drv;

    assert(reopen_state != NULL);
    drv = reopen_state->bs->drv;
    assert(drv != NULL);
    GLOBAL_STATE_CODE();

    if (drv->bdrv_reopen_abort) {
        drv->bdrv_reopen_abort(reopen_state);
    }
}


static void bdrv_close(BlockDriverState *bs)
{
    BdrvAioNotifier *ban, *ban_next;
    BdrvChild *child, *next;

    GLOBAL_STATE_CODE();
    assert(!bs->refcnt);

    bdrv_drained_begin(bs); /* complete I/O */
    bdrv_flush(bs);
    bdrv_drain(bs); /* in case flush left pending I/O */

    if (bs->drv) {
        if (bs->drv->bdrv_close) {
            bs->drv->bdrv_close(bs);
        }
        bs->drv = NULL;
    }

    bdrv_graph_wrlock();
    QLIST_FOREACH_SAFE(child, &bs->children, next, next) {
        bdrv_unref_child(bs, child);
    }

    assert(!bs->backing);
    assert(!bs->file);
    bdrv_graph_wrunlock();

    g_free(bs->opaque);
    bs->opaque = NULL;
    qatomic_set(&bs->copy_on_read, 0);
    bs->backing_file[0] = '\0';
    bs->backing_format[0] = '\0';
    bs->total_sectors = 0;
    bs->encrypted = false;
    bs->sg = false;
    qobject_unref(bs->options);
    qobject_unref(bs->explicit_options);
    bs->options = NULL;
    bs->explicit_options = NULL;
    qobject_unref(bs->full_open_options);
    bs->full_open_options = NULL;
    g_free(bs->block_status_cache);
    bs->block_status_cache = NULL;

    QLIST_FOREACH_SAFE(ban, &bs->aio_notifiers, list, ban_next) {
        g_free(ban);
    }
    QLIST_INIT(&bs->aio_notifiers);
    bdrv_drained_end(bs);

    if (bs->quiesce_counter) {
        bdrv_drain_all_end_quiesce(bs);
    }
}

void bdrv_close_all(void)
{
    GLOBAL_STATE_CODE();
    bdrv_drain_all();

    blk_remove_all_bs();
    blockdev_close_all_bdrv_states();

    assert(QTAILQ_EMPTY(&all_bdrv_states));
}

static bool GRAPH_RDLOCK should_update_child(BdrvChild *c, BlockDriverState *to)
{
    GQueue *queue;
    GHashTable *found;
    bool ret;

    if (c->klass->stay_at_node) {
        return false;
    }


    ret = true;
    found = g_hash_table_new(NULL, NULL);
    g_hash_table_add(found, to);
    queue = g_queue_new();
    g_queue_push_tail(queue, to);

    while (!g_queue_is_empty(queue)) {
        BlockDriverState *v = g_queue_pop_head(queue);
        BdrvChild *c2;

        QLIST_FOREACH(c2, &v->children, next) {
            if (c2 == c) {
                ret = false;
                break;
            }

            if (g_hash_table_contains(found, c2->bs)) {
                continue;
            }

            g_queue_push_tail(queue, c2->bs);
            g_hash_table_add(found, c2->bs);
        }
    }

    g_queue_free(queue);
    g_hash_table_destroy(found);

    return ret;
}

static void bdrv_remove_child_commit(void *opaque)
{
    GLOBAL_STATE_CODE();
    bdrv_child_free(opaque);
}

static TransactionActionDrv bdrv_remove_child_drv = {
    .commit = bdrv_remove_child_commit,
};

static void GRAPH_WRLOCK bdrv_remove_child(BdrvChild *child, Transaction *tran)
{
    if (!child) {
        return;
    }

    if (child->bs) {
        assert(child->quiesced_parent);
        bdrv_replace_child_tran(child, NULL, tran);
    }

    tran_add(tran, &bdrv_remove_child_drv, child);
}

static int GRAPH_WRLOCK
bdrv_replace_node_noperm(BlockDriverState *from,
                         BlockDriverState *to,
                         bool auto_skip, Transaction *tran,
                         Error **errp)
{
    BdrvChild *c, *next;

    GLOBAL_STATE_CODE();

    assert(from->quiesce_counter);
    assert(to->quiesce_counter);

    QLIST_FOREACH_SAFE(c, &from->parents, next_parent, next) {
        assert(c->bs == from);
        if (!should_update_child(c, to)) {
            if (auto_skip) {
                continue;
            }
            error_setg(errp, "Should not change '%s' link to '%s'",
                       c->name, from->node_name);
            return -EINVAL;
        }
        if (c->frozen) {
            error_setg(errp, "Cannot change '%s' link to '%s'",
                       c->name, from->node_name);
            return -EPERM;
        }
        bdrv_replace_child_tran(c, to, tran);
    }

    return 0;
}

static int GRAPH_WRLOCK
bdrv_replace_node_common(BlockDriverState *from, BlockDriverState *to,
                         bool auto_skip, bool detach_subchain, Error **errp)
{
    Transaction *tran = tran_new();
    g_autoptr(GSList) refresh_list = NULL;
    BlockDriverState *to_cow_parent = NULL;
    int ret;

    GLOBAL_STATE_CODE();

    assert(from->quiesce_counter);
    assert(to->quiesce_counter);
    assert(bdrv_get_aio_context(from) == bdrv_get_aio_context(to));

    if (detach_subchain) {
        assert(bdrv_chain_contains(from, to));
        assert(from != to);
        for (to_cow_parent = from;
             bdrv_filter_or_cow_bs(to_cow_parent) != to;
             to_cow_parent = bdrv_filter_or_cow_bs(to_cow_parent))
        {
            ;
        }
    }

    ret = bdrv_replace_node_noperm(from, to, auto_skip, tran, errp);
    if (ret < 0) {
        goto out;
    }

    if (detach_subchain) {
        bdrv_remove_child(bdrv_filter_or_cow_child(to_cow_parent), tran);
    }

    refresh_list = g_slist_prepend(refresh_list, to);
    refresh_list = g_slist_prepend(refresh_list, from);

    ret = bdrv_list_refresh_perms(refresh_list, NULL, tran, errp);
    if (ret < 0) {
        goto out;
    }

    ret = 0;

out:
    tran_finalize(tran, ret);
    return ret;
}

int bdrv_replace_node(BlockDriverState *from, BlockDriverState *to,
                      Error **errp)
{
    return bdrv_replace_node_common(from, to, true, false, errp);
}

int bdrv_drop_filter(BlockDriverState *bs, Error **errp)
{
    BlockDriverState *child_bs;
    int ret;

    GLOBAL_STATE_CODE();

    bdrv_graph_rdlock_main_loop();
    child_bs = bdrv_filter_or_cow_bs(bs);
    bdrv_graph_rdunlock_main_loop();

    bdrv_drained_begin(child_bs);
    bdrv_graph_wrlock();
    ret = bdrv_replace_node_common(bs, child_bs, true, true, errp);
    bdrv_graph_wrunlock();
    bdrv_drained_end(child_bs);

    return ret;
}

int bdrv_append(BlockDriverState *bs_new, BlockDriverState *bs_top,
                Error **errp)
{
    int ret;
    BdrvChild *child;
    Transaction *tran = tran_new();

    GLOBAL_STATE_CODE();

    bdrv_graph_rdlock_main_loop();
    assert(!bs_new->backing);
    bdrv_graph_rdunlock_main_loop();

    bdrv_drained_begin(bs_top);
    bdrv_drained_begin(bs_new);

    bdrv_graph_wrlock();

    child = bdrv_attach_child_noperm(bs_new, bs_top, "backing",
                                     &child_of_bds, bdrv_backing_role(bs_new),
                                     tran, errp);
    if (!child) {
        ret = -EINVAL;
        goto out;
    }

    ret = bdrv_replace_node_noperm(bs_top, bs_new, true, tran, errp);
    if (ret < 0) {
        goto out;
    }

    ret = bdrv_refresh_perms(bs_new, tran, errp);
out:
    tran_finalize(tran, ret);

    bdrv_refresh_limits(bs_top, NULL, NULL);
    bdrv_graph_wrunlock();

    bdrv_drained_end(bs_top);
    bdrv_drained_end(bs_new);

    return ret;
}

int bdrv_replace_child_bs(BdrvChild *child, BlockDriverState *new_bs,
                          Error **errp)
{
    int ret;
    Transaction *tran = tran_new();
    g_autoptr(GSList) refresh_list = NULL;
    BlockDriverState *old_bs = child->bs;

    GLOBAL_STATE_CODE();

    bdrv_ref(old_bs);
    bdrv_drained_begin(old_bs);
    bdrv_drained_begin(new_bs);
    bdrv_graph_wrlock();

    bdrv_replace_child_tran(child, new_bs, tran);

    refresh_list = g_slist_prepend(refresh_list, old_bs);
    refresh_list = g_slist_prepend(refresh_list, new_bs);

    ret = bdrv_list_refresh_perms(refresh_list, NULL, tran, errp);

    tran_finalize(tran, ret);

    bdrv_graph_wrunlock();
    bdrv_drained_end(old_bs);
    bdrv_drained_end(new_bs);
    bdrv_unref(old_bs);

    return ret;
}

static void bdrv_delete(BlockDriverState *bs)
{
    assert(bdrv_op_blocker_is_empty(bs));
    assert(!bs->refcnt);
    GLOBAL_STATE_CODE();

    if (bs->node_name[0] != '\0') {
        QTAILQ_REMOVE(&graph_bdrv_states, bs, node_list);
    }
    QTAILQ_REMOVE(&all_bdrv_states, bs, bs_list);

    bdrv_close(bs);

    qemu_mutex_destroy(&bs->reqs_lock);

    g_free(bs);
}


BlockDriverState *bdrv_insert_node(BlockDriverState *bs, QDict *options,
                                   int flags, Error **errp)
{
    ERRP_GUARD();
    int ret;
    AioContext *ctx = bdrv_get_aio_context(bs);
    BlockDriverState *new_node_bs = NULL;
    const char *drvname, *node_name;
    BlockDriver *drv;

    drvname = qdict_get_try_str(options, "driver");
    if (!drvname) {
        error_setg(errp, "driver is not specified");
        goto fail;
    }

    drv = bdrv_find_format(drvname);
    if (!drv) {
        error_setg(errp, "Unknown driver: '%s'", drvname);
        goto fail;
    }

    node_name = qdict_get_try_str(options, "node-name");

    GLOBAL_STATE_CODE();

    new_node_bs = bdrv_new_open_driver_opts(drv, node_name, options, flags,
                                            errp);
    assert(bdrv_get_aio_context(bs) == ctx);

    options = NULL; /* bdrv_new_open_driver() eats options */
    if (!new_node_bs) {
        error_prepend(errp, "Could not create node: ");
        goto fail;
    }

    bdrv_ref(bs);
    bdrv_drained_begin(bs);
    bdrv_drained_begin(new_node_bs);
    bdrv_graph_wrlock();
    ret = bdrv_replace_node(bs, new_node_bs, errp);
    bdrv_graph_wrunlock();
    bdrv_drained_end(new_node_bs);
    bdrv_drained_end(bs);
    bdrv_unref(bs);

    if (ret < 0) {
        error_prepend(errp, "Could not replace node: ");
        goto fail;
    }

    return new_node_bs;

fail:
    qobject_unref(options);
    bdrv_unref(new_node_bs);
    return NULL;
}

int coroutine_fn bdrv_co_check(BlockDriverState *bs,
                               BdrvCheckResult *res, BdrvCheckMode fix)
{
    IO_CODE();
    assert_bdrv_graph_readable();
    if (bs->drv == NULL) {
        return -ENOMEDIUM;
    }
    if (bs->drv->bdrv_co_check == NULL) {
        return -ENOTSUP;
    }

    memset(res, 0, sizeof(*res));
    return bs->drv->bdrv_co_check(bs, res, fix);
}

int coroutine_fn
bdrv_co_change_backing_file(BlockDriverState *bs, const char *backing_file,
                            const char *backing_fmt, bool require)
{
    BlockDriver *drv = bs->drv;
    int ret;

    IO_CODE();

    if (!drv) {
        return -ENOMEDIUM;
    }

    if (backing_fmt && !backing_file) {
        return -EINVAL;
    }

    if (require && backing_file && !backing_fmt) {
        return -EINVAL;
    }

    if (drv->bdrv_co_change_backing_file != NULL) {
        ret = drv->bdrv_co_change_backing_file(bs, backing_file, backing_fmt);
    } else {
        ret = -ENOTSUP;
    }

    if (ret == 0) {
        pstrcpy(bs->backing_file, sizeof(bs->backing_file), backing_file ?: "");
        pstrcpy(bs->backing_format, sizeof(bs->backing_format), backing_fmt ?: "");
        pstrcpy(bs->auto_backing_file, sizeof(bs->auto_backing_file),
                backing_file ?: "");
    }
    return ret;
}

BlockDriverState *bdrv_find_overlay(BlockDriverState *active,
                                    BlockDriverState *bs)
{

    GLOBAL_STATE_CODE();

    bs = bdrv_skip_filters(bs);
    active = bdrv_skip_filters(active);

    while (active) {
        BlockDriverState *next = bdrv_backing_chain_next(active);
        if (bs == next) {
            return active;
        }
        active = next;
    }

    return NULL;
}

BlockDriverState *bdrv_find_base(BlockDriverState *bs)
{
    GLOBAL_STATE_CODE();

    return bdrv_find_overlay(bs, NULL);
}

static bool GRAPH_RDLOCK
bdrv_is_backing_chain_frozen(BlockDriverState *bs, BlockDriverState *base,
                             Error **errp)
{
    BlockDriverState *i;
    BdrvChild *child;

    GLOBAL_STATE_CODE();

    for (i = bs; i != base; i = child_bs(child)) {
        child = bdrv_filter_or_cow_child(i);

        if (child && child->frozen) {
            error_setg(errp, "Cannot change '%s' link from '%s' to '%s'",
                       child->name, i->node_name, child->bs->node_name);
            return true;
        }
    }

    return false;
}

int bdrv_freeze_backing_chain(BlockDriverState *bs, BlockDriverState *base,
                              Error **errp)
{
    BlockDriverState *i;
    BdrvChild *child;

    GLOBAL_STATE_CODE();

    if (bdrv_is_backing_chain_frozen(bs, base, errp)) {
        return -EPERM;
    }

    for (i = bs; i != base; i = child_bs(child)) {
        child = bdrv_filter_or_cow_child(i);
        if (child && child->bs->never_freeze) {
            error_setg(errp, "Cannot freeze '%s' link to '%s'",
                       child->name, child->bs->node_name);
            return -EPERM;
        }
    }

    for (i = bs; i != base; i = child_bs(child)) {
        child = bdrv_filter_or_cow_child(i);
        if (child) {
            child->frozen = true;
        }
    }

    return 0;
}

void bdrv_unfreeze_backing_chain(BlockDriverState *bs, BlockDriverState *base)
{
    BlockDriverState *i;
    BdrvChild *child;

    GLOBAL_STATE_CODE();

    for (i = bs; i != base; i = child_bs(child)) {
        child = bdrv_filter_or_cow_child(i);
        if (child) {
            assert(child->frozen);
            child->frozen = false;
        }
    }
}

int bdrv_drop_intermediate(BlockDriverState *top, BlockDriverState *base,
                           const char *backing_file_str,
                           bool backing_mask_protocol)
{
    BlockDriverState *explicit_top = top;
    bool update_inherits_from;
    BdrvChild *c;
    Error *local_err = NULL;
    int ret = -EIO;
    g_autoptr(GSList) updated_children = NULL;
    GSList *p;

    GLOBAL_STATE_CODE();

    bdrv_ref(top);
    bdrv_drained_begin(base);
    bdrv_graph_wrlock();

    if (!top->drv || !base->drv) {
        goto exit_wrlock;
    }

    if (!bdrv_chain_contains(top, base)) {
        goto exit_wrlock;
    }

    explicit_top = bdrv_skip_implicit_filters(explicit_top);
    update_inherits_from = bdrv_inherits_from_recursive(base, explicit_top);

    if (!backing_file_str) {
        bdrv_refresh_filename(base);
        backing_file_str = base->filename;
    }

    QLIST_FOREACH(c, &top->parents, next_parent) {
        updated_children = g_slist_prepend(updated_children, c);
    }

    bdrv_replace_node_common(top, base, false, false, &local_err);
    bdrv_graph_wrunlock();

    if (local_err) {
        error_report_err(local_err);
        goto exit;
    }

    for (p = updated_children; p; p = p->next) {
        c = p->data;

        if (c->klass->update_filename) {
            ret = c->klass->update_filename(c, base, backing_file_str,
                                            backing_mask_protocol,
                                            &local_err);
            if (ret < 0) {
                error_report_err(local_err);
                goto exit;
            }
        }
    }

    if (update_inherits_from) {
        base->inherits_from = explicit_top->inherits_from;
    }

    ret = 0;
    goto exit;

exit_wrlock:
    bdrv_graph_wrunlock();
exit:
    bdrv_drained_end(base);
    bdrv_unref(top);
    return ret;
}

static int64_t coroutine_fn GRAPH_RDLOCK
bdrv_sum_allocated_file_size(BlockDriverState *bs)
{
    BdrvChild *child;
    int64_t child_size, sum = 0;

    QLIST_FOREACH(child, &bs->children, next) {
        if (child->role & (BDRV_CHILD_DATA | BDRV_CHILD_METADATA |
                           BDRV_CHILD_FILTERED))
        {
            child_size = bdrv_co_get_allocated_file_size(child->bs);
            if (child_size < 0) {
                return child_size;
            }
            sum += child_size;
        }
    }

    return sum;
}

int64_t coroutine_fn bdrv_co_get_allocated_file_size(BlockDriverState *bs)
{
    BlockDriver *drv = bs->drv;
    IO_CODE();
    assert_bdrv_graph_readable();

    if (!drv) {
        return -ENOMEDIUM;
    }
    if (drv->bdrv_co_get_allocated_file_size) {
        return drv->bdrv_co_get_allocated_file_size(bs);
    }

    if (drv->protocol_name) {
        return -ENOTSUP;
    } else if (drv->is_filter) {
        return bdrv_co_get_allocated_file_size(bdrv_filter_bs(bs));
    } else {
        return bdrv_sum_allocated_file_size(bs);
    }
}

BlockMeasureInfo *bdrv_measure(BlockDriver *drv, QemuOpts *opts,
                               BlockDriverState *in_bs, Error **errp)
{
    IO_CODE();
    if (!drv->bdrv_measure) {
        error_setg(errp, "Block driver '%s' does not support size measurement",
                   drv->format_name);
        return NULL;
    }

    return drv->bdrv_measure(opts, in_bs, errp);
}

int64_t coroutine_fn bdrv_co_nb_sectors(BlockDriverState *bs)
{
    BlockDriver *drv = bs->drv;
    IO_CODE();
    assert_bdrv_graph_readable();

    if (!drv)
        return -ENOMEDIUM;

    if (bs->bl.has_variable_length) {
        int ret = bdrv_co_refresh_total_sectors(bs, bs->total_sectors);
        if (ret < 0) {
            return ret;
        }
    }
    return bs->total_sectors;
}

int64_t coroutine_mixed_fn bdrv_nb_sectors(BlockDriverState *bs)
{
    BlockDriver *drv = bs->drv;
    IO_CODE();

    if (!drv)
        return -ENOMEDIUM;

    if (bs->bl.has_variable_length) {
        int ret = bdrv_refresh_total_sectors(bs, bs->total_sectors);
        if (ret < 0) {
            return ret;
        }
    }

    return bs->total_sectors;
}

int64_t coroutine_fn bdrv_co_getlength(BlockDriverState *bs)
{
    int64_t ret;
    IO_CODE();
    assert_bdrv_graph_readable();

    ret = bdrv_co_nb_sectors(bs);
    if (ret < 0) {
        return ret;
    }
    if (ret > INT64_MAX / BDRV_SECTOR_SIZE) {
        return -EFBIG;
    }
    return ret * BDRV_SECTOR_SIZE;
}

bool bdrv_is_sg(BlockDriverState *bs)
{
    IO_CODE();
    return bs->sg;
}

bool bdrv_supports_compressed_writes(BlockDriverState *bs)
{
    BlockDriverState *filtered;
    IO_CODE();

    if (!bs->drv || !block_driver_can_compress(bs->drv)) {
        return false;
    }

    filtered = bdrv_filter_bs(bs);
    if (filtered) {
        return bdrv_supports_compressed_writes(filtered);
    }

    return true;
}

const char *bdrv_get_format_name(BlockDriverState *bs)
{
    IO_CODE();
    return bs->drv ? bs->drv->format_name : NULL;
}

static int qsort_strcmp(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

void bdrv_iterate_format(void (*it)(void *opaque, const char *name),
                         void *opaque, bool read_only)
{
    BlockDriver *drv;
    int count = 0;
    int i;
    const char **formats = NULL;

    GLOBAL_STATE_CODE();

    QLIST_FOREACH(drv, &bdrv_drivers, list) {
        if (drv->format_name) {
            bool found = false;

            if (use_bdrv_whitelist && !bdrv_is_whitelisted(drv, read_only)) {
                continue;
            }

            i = count;
            while (formats && i && !found) {
                found = !strcmp(formats[--i], drv->format_name);
            }

            if (!found) {
                formats = g_renew(const char *, formats, count + 1);
                formats[count++] = drv->format_name;
            }
        }
    }

    for (i = 0; i < (int)ARRAY_SIZE(block_driver_modules); i++) {
        const char *format_name = block_driver_modules[i].format_name;

        if (format_name) {
            bool found = false;
            int j = count;

            if (use_bdrv_whitelist &&
                !bdrv_format_is_whitelisted(format_name, read_only)) {
                continue;
            }

            while (formats && j && !found) {
                found = !strcmp(formats[--j], format_name);
            }

            if (!found) {
                formats = g_renew(const char *, formats, count + 1);
                formats[count++] = format_name;
            }
        }
    }

    qsort(formats, count, sizeof(formats[0]), qsort_strcmp);

    for (i = 0; i < count; i++) {
        it(opaque, formats[i]);
    }

    g_free(formats);
}

BlockDriverState *bdrv_find_node(const char *node_name)
{
    BlockDriverState *bs;

    assert(node_name);
    GLOBAL_STATE_CODE();

    QTAILQ_FOREACH(bs, &graph_bdrv_states, node_list) {
        if (!strcmp(node_name, bs->node_name)) {
            return bs;
        }
    }
    return NULL;
}

BlockDriverState *bdrv_lookup_bs(const char *device,
                                 const char *node_name,
                                 Error **errp)
{
    BlockBackend *blk;
    BlockDriverState *bs;

    GLOBAL_STATE_CODE();

    if (device) {
        blk = blk_by_name(device);

        if (blk) {
            bs = blk_bs(blk);
            if (!bs) {
                error_setg(errp, "Device '%s' has no medium", device);
            }

            return bs;
        }
    }

    if (node_name) {
        bs = bdrv_find_node(node_name);

        if (bs) {
            return bs;
        }
    }

    error_setg(errp, "Cannot find device=\'%s\' nor node-name=\'%s\'",
                     device ? device : "",
                     node_name ? node_name : "");
    return NULL;
}

bool bdrv_chain_contains(BlockDriverState *top, BlockDriverState *base)
{

    GLOBAL_STATE_CODE();

    while (top && top != base) {
        top = bdrv_filter_or_cow_bs(top);
    }

    return top != NULL;
}

BlockDriverState *bdrv_next_node(BlockDriverState *bs)
{
    GLOBAL_STATE_CODE();
    if (!bs) {
        return QTAILQ_FIRST(&graph_bdrv_states);
    }
    return QTAILQ_NEXT(bs, node_list);
}

BlockDriverState *bdrv_next_all_states(BlockDriverState *bs)
{
    GLOBAL_STATE_CODE();
    if (!bs) {
        return QTAILQ_FIRST(&all_bdrv_states);
    }
    return QTAILQ_NEXT(bs, bs_list);
}

const char *bdrv_get_node_name(const BlockDriverState *bs)
{
    IO_CODE();
    return bs->node_name;
}

const char *bdrv_get_parent_name(const BlockDriverState *bs)
{
    BdrvChild *c;
    const char *name;
    IO_CODE();

    QLIST_FOREACH(c, &bs->parents, next_parent) {
        if (c->klass->get_name) {
            name = c->klass->get_name(c);
            if (name && *name) {
                return name;
            }
        }
    }

    return NULL;
}

const char *bdrv_get_device_name(const BlockDriverState *bs)
{
    IO_CODE();
    return bdrv_get_parent_name(bs) ?: "";
}

const char *bdrv_get_device_or_node_name(const BlockDriverState *bs)
{
    IO_CODE();
    return bdrv_get_parent_name(bs) ?: bs->node_name;
}

int bdrv_get_flags(BlockDriverState *bs)
{
    IO_CODE();
    return bs->open_flags;
}

int bdrv_has_zero_init_1(BlockDriverState *bs)
{
    GLOBAL_STATE_CODE();
    return 1;
}

int coroutine_mixed_fn bdrv_has_zero_init(BlockDriverState *bs)
{
    BlockDriverState *filtered;
    GLOBAL_STATE_CODE();

    if (!bs->drv) {
        return 0;
    }

    if (bdrv_cow_child(bs)) {
        return 0;
    }
    if (bs->drv->bdrv_has_zero_init) {
        return bs->drv->bdrv_has_zero_init(bs);
    }

    filtered = bdrv_filter_bs(bs);
    if (filtered) {
        return bdrv_has_zero_init(filtered);
    }

    return 0;
}

bool bdrv_can_write_zeroes_with_unmap(BlockDriverState *bs)
{
    IO_CODE();
    if (!(bs->open_flags & BDRV_O_UNMAP)) {
        return false;
    }

    return bs->supported_zero_flags & BDRV_REQ_MAY_UNMAP;
}

void bdrv_get_backing_filename(BlockDriverState *bs,
                               char *filename, int filename_size)
{
    IO_CODE();
    pstrcpy(filename, filename_size, bs->backing_file);
}

int coroutine_fn bdrv_co_get_info(BlockDriverState *bs, BlockDriverInfo *bdi)
{
    int ret;
    BlockDriver *drv = bs->drv;
    IO_CODE();
    assert_bdrv_graph_readable();

    if (!drv) {
        return -ENOMEDIUM;
    }
    if (!drv->bdrv_co_get_info) {
        BlockDriverState *filtered = bdrv_filter_bs(bs);
        if (filtered) {
            return bdrv_co_get_info(filtered, bdi);
        }
        return -ENOTSUP;
    }
    memset(bdi, 0, sizeof(*bdi));
    ret = drv->bdrv_co_get_info(bs, bdi);
    if (bdi->subcluster_size == 0) {
        bdi->subcluster_size = bdi->cluster_size;
    }
    if (ret < 0) {
        return ret;
    }

    if (bdi->cluster_size > BDRV_MAX_ALIGNMENT) {
        return -EINVAL;
    }

    return 0;
}

ImageInfoSpecific *bdrv_get_specific_info(BlockDriverState *bs,
                                          Error **errp)
{
    BlockDriver *drv = bs->drv;
    IO_CODE();
    if (drv && drv->bdrv_get_specific_info) {
        return drv->bdrv_get_specific_info(bs, errp);
    }
    return NULL;
}

BlockStatsSpecific *bdrv_get_specific_stats(BlockDriverState *bs)
{
    BlockDriver *drv = bs->drv;
    IO_CODE();
    if (!drv || !drv->bdrv_get_specific_stats) {
        return NULL;
    }
    return drv->bdrv_get_specific_stats(bs);
}

void coroutine_fn bdrv_co_debug_event(BlockDriverState *bs, BlkdebugEvent event)
{
    IO_CODE();
    assert_bdrv_graph_readable();

    if (!bs || !bs->drv || !bs->drv->bdrv_co_debug_event) {
        return;
    }

    bs->drv->bdrv_co_debug_event(bs, event);
}

static BlockDriverState * GRAPH_RDLOCK
bdrv_find_debug_node(BlockDriverState *bs)
{
    GLOBAL_STATE_CODE();
    while (bs && bs->drv && !bs->drv->bdrv_debug_breakpoint) {
        bs = bdrv_primary_bs(bs);
    }

    if (bs && bs->drv && bs->drv->bdrv_debug_breakpoint) {
        assert(bs->drv->bdrv_debug_remove_breakpoint);
        return bs;
    }

    return NULL;
}

int bdrv_debug_breakpoint(BlockDriverState *bs, const char *event,
                          const char *tag)
{
    GLOBAL_STATE_CODE();
    GRAPH_RDLOCK_GUARD_MAINLOOP();

    bs = bdrv_find_debug_node(bs);
    if (bs) {
        return bs->drv->bdrv_debug_breakpoint(bs, event, tag);
    }

    return -ENOTSUP;
}

int bdrv_debug_remove_breakpoint(BlockDriverState *bs, const char *tag)
{
    GLOBAL_STATE_CODE();
    GRAPH_RDLOCK_GUARD_MAINLOOP();

    bs = bdrv_find_debug_node(bs);
    if (bs) {
        return bs->drv->bdrv_debug_remove_breakpoint(bs, tag);
    }

    return -ENOTSUP;
}

int bdrv_debug_resume(BlockDriverState *bs, const char *tag)
{
    GLOBAL_STATE_CODE();
    GRAPH_RDLOCK_GUARD_MAINLOOP();

    while (bs && (!bs->drv || !bs->drv->bdrv_debug_resume)) {
        bs = bdrv_primary_bs(bs);
    }

    if (bs && bs->drv && bs->drv->bdrv_debug_resume) {
        return bs->drv->bdrv_debug_resume(bs, tag);
    }

    return -ENOTSUP;
}

bool bdrv_debug_is_suspended(BlockDriverState *bs, const char *tag)
{
    GLOBAL_STATE_CODE();
    GRAPH_RDLOCK_GUARD_MAINLOOP();

    while (bs && bs->drv && !bs->drv->bdrv_debug_is_suspended) {
        bs = bdrv_primary_bs(bs);
    }

    if (bs && bs->drv && bs->drv->bdrv_debug_is_suspended) {
        return bs->drv->bdrv_debug_is_suspended(bs, tag);
    }

    return false;
}

BlockDriverState *bdrv_find_backing_image(BlockDriverState *bs,
        const char *backing_file)
{
    char *filename_full = NULL;
    char *backing_file_full = NULL;
    char *filename_tmp = NULL;
    int is_protocol = 0;
    bool filenames_refreshed = false;
    BlockDriverState *curr_bs = NULL;
    BlockDriverState *retval = NULL;
    BlockDriverState *bs_below;

    GLOBAL_STATE_CODE();
    GRAPH_RDLOCK_GUARD_MAINLOOP();

    if (!bs || !bs->drv || !backing_file) {
        return NULL;
    }

    filename_full     = g_malloc(PATH_MAX);
    backing_file_full = g_malloc(PATH_MAX);

    is_protocol = path_has_protocol(backing_file);

    for (curr_bs = bdrv_skip_filters(bs);
         bdrv_cow_child(curr_bs) != NULL;
         curr_bs = bs_below)
    {
        bs_below = bdrv_backing_chain_next(curr_bs);

        if (bdrv_backing_overridden(curr_bs)) {

            if (!filenames_refreshed) {
                bdrv_refresh_filename(bs_below);
                filenames_refreshed = true;
            }

            if (strcmp(backing_file, bs_below->filename) == 0) {
                retval = bs_below;
                break;
            }
        } else if (is_protocol || path_has_protocol(curr_bs->backing_file)) {
            char *backing_file_full_ret;

            if (strcmp(backing_file, curr_bs->backing_file) == 0) {
                retval = bs_below;
                break;
            }
            backing_file_full_ret = bdrv_get_full_backing_filename(curr_bs,
                                                                   NULL);
            if (backing_file_full_ret) {
                bool equal = strcmp(backing_file, backing_file_full_ret) == 0;
                g_free(backing_file_full_ret);
                if (equal) {
                    retval = bs_below;
                    break;
                }
            }
        } else {
            filename_tmp = bdrv_make_absolute_filename(curr_bs, backing_file,
                                                       NULL);
            if (!filename_tmp || !realpath(filename_tmp, filename_full)) {
                g_free(filename_tmp);
                continue;
            }
            g_free(filename_tmp);

            filename_tmp = bdrv_get_full_backing_filename(curr_bs, NULL);
            if (!filename_tmp || !realpath(filename_tmp, backing_file_full)) {
                g_free(filename_tmp);
                continue;
            }
            g_free(filename_tmp);

            if (strcmp(backing_file_full, filename_full) == 0) {
                retval = bs_below;
                break;
            }
        }
    }

    g_free(filename_full);
    g_free(backing_file_full);
    return retval;
}

void bdrv_init(void)
{
#ifdef CONFIG_BDRV_WHITELIST_TOOLS
    use_bdrv_whitelist = 1;
#endif
    module_call_init(MODULE_INIT_BLOCK);
}

void bdrv_init_with_whitelist(void)
{
    use_bdrv_whitelist = 1;
    bdrv_init();
}

bool bdrv_is_inactive(BlockDriverState *bs) {
    return bs->open_flags & BDRV_O_INACTIVE;
}

int bdrv_activate(BlockDriverState *bs, Error **errp)
{
    BdrvChild *child, *parent;
    Error *local_err = NULL;
    int ret;

    GLOBAL_STATE_CODE();
    GRAPH_RDLOCK_GUARD_MAINLOOP();

    if (!bs->drv)  {
        return -ENOMEDIUM;
    }

    QLIST_FOREACH(child, &bs->children, next) {
        bdrv_activate(child->bs, &local_err);
        if (local_err) {
            error_propagate(errp, local_err);
            return -EINVAL;
        }
    }

    if (bs->open_flags & BDRV_O_INACTIVE) {
        bs->open_flags &= ~BDRV_O_INACTIVE;
        ret = bdrv_refresh_perms(bs, NULL, errp);
        if (ret < 0) {
            bs->open_flags |= BDRV_O_INACTIVE;
            return ret;
        }

        ret = bdrv_invalidate_cache(bs, errp);
        if (ret < 0) {
            bs->open_flags |= BDRV_O_INACTIVE;
            return ret;
        }

        ret = bdrv_refresh_total_sectors(bs, bs->total_sectors);
        if (ret < 0) {
            bs->open_flags |= BDRV_O_INACTIVE;
            error_setg_errno(errp, -ret, "Could not refresh total sector count");
            return ret;
        }
    }

    QLIST_FOREACH(parent, &bs->parents, next_parent) {
        if (parent->klass->activate) {
            parent->klass->activate(parent, &local_err);
            if (local_err) {
                bs->open_flags |= BDRV_O_INACTIVE;
                error_propagate(errp, local_err);
                return -EINVAL;
            }
        }
    }

    return 0;
}

int coroutine_fn bdrv_co_invalidate_cache(BlockDriverState *bs, Error **errp)
{
    Error *local_err = NULL;
    IO_CODE();

    assert(!(bs->open_flags & BDRV_O_INACTIVE));
    assert_bdrv_graph_readable();

    if (bs->drv->bdrv_co_invalidate_cache) {
        bs->drv->bdrv_co_invalidate_cache(bs, &local_err);
        if (local_err) {
            error_propagate(errp, local_err);
            return -EINVAL;
        }
    }

    return 0;
}

void bdrv_activate_all(Error **errp)
{
    BlockDriverState *bs;
    BdrvNextIterator it;

    GLOBAL_STATE_CODE();
    GRAPH_RDLOCK_GUARD_MAINLOOP();

    for (bs = bdrv_first(&it); bs; bs = bdrv_next(&it)) {
        int ret;

        ret = bdrv_activate(bs, errp);
        if (ret < 0) {
            bdrv_next_cleanup(&it);
            return;
        }
    }
}

static bool GRAPH_RDLOCK
bdrv_has_bds_parent(BlockDriverState *bs, bool only_active)
{
    BdrvChild *parent;
    GLOBAL_STATE_CODE();

    QLIST_FOREACH(parent, &bs->parents, next_parent) {
        if (parent->klass->parent_is_bds) {
            BlockDriverState *parent_bs = parent->opaque;
            if (!only_active || !(parent_bs->open_flags & BDRV_O_INACTIVE)) {
                return true;
            }
        }
    }

    return false;
}

static int GRAPH_RDLOCK
bdrv_inactivate_recurse(BlockDriverState *bs, bool top_level)
{
    BdrvChild *child, *parent;
    int ret;
    uint64_t cumulative_perms, cumulative_shared_perms;

    GLOBAL_STATE_CODE();

    if (!bs->drv) {
        return -ENOMEDIUM;
    }

    if (bdrv_has_bds_parent(bs, true)) {
        return 0;
    }

    if (bs->open_flags & BDRV_O_INACTIVE) {
        assert(top_level);
        return 0;
    }

    if (bs->drv->bdrv_inactivate) {
        ret = bs->drv->bdrv_inactivate(bs);
        if (ret < 0) {
            return ret;
        }
    }

    QLIST_FOREACH(parent, &bs->parents, next_parent) {
        if (parent->klass->inactivate) {
            ret = parent->klass->inactivate(parent);
            if (ret < 0) {
                return ret;
            }
        }
    }

    bdrv_get_cumulative_perm(bs, &cumulative_perms,
                             &cumulative_shared_perms);
    if (cumulative_perms & (BLK_PERM_WRITE | BLK_PERM_WRITE_UNCHANGED)) {
        return -EPERM;
    }

    bdrv_drained_begin(bs);
    bs->open_flags |= BDRV_O_INACTIVE;
    bdrv_drained_end(bs);

    bdrv_refresh_perms(bs, NULL, NULL);

    QLIST_FOREACH(child, &bs->children, next) {
        ret = bdrv_inactivate_recurse(child->bs, false);
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}

int bdrv_inactivate(BlockDriverState *bs, Error **errp)
{
    int ret;

    GLOBAL_STATE_CODE();
    GRAPH_RDLOCK_GUARD_MAINLOOP();

    if (bdrv_has_bds_parent(bs, true)) {
        error_setg(errp, "Node has active parent node");
        return -EPERM;
    }

    ret = bdrv_inactivate_recurse(bs, true);
    if (ret < 0) {
        error_setg_errno(errp, -ret, "Failed to inactivate node");
        return ret;
    }

    return 0;
}

int bdrv_inactivate_all(void)
{
    BlockDriverState *bs = NULL;
    BdrvNextIterator it;
    int ret = 0;

    GLOBAL_STATE_CODE();
    GRAPH_RDLOCK_GUARD_MAINLOOP();

    for (bs = bdrv_first(&it); bs; bs = bdrv_next(&it)) {
        if (bdrv_has_bds_parent(bs, false)) {
            continue;
        }
        ret = bdrv_inactivate_recurse(bs, true);
        if (ret < 0) {
            bdrv_next_cleanup(&it);
            break;
        }
    }

    return ret;
}


bool coroutine_fn bdrv_co_is_inserted(BlockDriverState *bs)
{
    BlockDriver *drv = bs->drv;
    BdrvChild *child;
    IO_CODE();
    assert_bdrv_graph_readable();

    if (!drv) {
        return false;
    }
    if (drv->bdrv_co_is_inserted) {
        return drv->bdrv_co_is_inserted(bs);
    }
    QLIST_FOREACH(child, &bs->children, next) {
        if (!bdrv_co_is_inserted(child->bs)) {
            return false;
        }
    }
    return true;
}

void coroutine_fn bdrv_co_eject(BlockDriverState *bs, bool eject_flag)
{
    BlockDriver *drv = bs->drv;
    IO_CODE();
    assert_bdrv_graph_readable();

    if (drv && drv->bdrv_co_eject) {
        drv->bdrv_co_eject(bs, eject_flag);
    }
}

void coroutine_fn bdrv_co_lock_medium(BlockDriverState *bs, bool locked)
{
    BlockDriver *drv = bs->drv;
    IO_CODE();
    assert_bdrv_graph_readable();
    trace_bdrv_lock_medium(bs, locked);

    if (drv && drv->bdrv_co_lock_medium) {
        drv->bdrv_co_lock_medium(bs, locked);
    }
}

void bdrv_ref(BlockDriverState *bs)
{
    GLOBAL_STATE_CODE();
    bs->refcnt++;
}

void bdrv_unref(BlockDriverState *bs)
{
    GLOBAL_STATE_CODE();
    if (!bs) {
        return;
    }
    assert(bs->refcnt > 0);
    if (--bs->refcnt == 0) {
        bdrv_delete(bs);
    }
}

static void bdrv_schedule_unref_bh(void *opaque)
{
    BlockDriverState *bs = opaque;

    bdrv_unref(bs);
}

void bdrv_schedule_unref(BlockDriverState *bs)
{
    if (!bs) {
        return;
    }
    aio_bh_schedule_oneshot(qemu_get_aio_context(), bdrv_schedule_unref_bh, bs);
}

struct BdrvOpBlocker {
    Error *reason;
    QLIST_ENTRY(BdrvOpBlocker) list;
};

bool bdrv_op_is_blocked(BlockDriverState *bs, BlockOpType op, Error **errp)
{
    BdrvOpBlocker *blocker;
    GLOBAL_STATE_CODE();

    assert((int) op >= 0 && op < BLOCK_OP_TYPE_MAX);
    if (!QLIST_EMPTY(&bs->op_blockers[op])) {
        blocker = QLIST_FIRST(&bs->op_blockers[op]);
        error_propagate_prepend(errp, error_copy(blocker->reason),
                                "Node '%s' is busy: ",
                                bdrv_get_device_or_node_name(bs));
        return true;
    }
    return false;
}

void bdrv_op_block(BlockDriverState *bs, BlockOpType op, Error *reason)
{
    BdrvOpBlocker *blocker;
    GLOBAL_STATE_CODE();
    assert((int) op >= 0 && op < BLOCK_OP_TYPE_MAX);

    blocker = g_new0(BdrvOpBlocker, 1);
    blocker->reason = reason;
    QLIST_INSERT_HEAD(&bs->op_blockers[op], blocker, list);
}

void bdrv_op_unblock(BlockDriverState *bs, BlockOpType op, Error *reason)
{
    BdrvOpBlocker *blocker, *next;
    GLOBAL_STATE_CODE();
    assert((int) op >= 0 && op < BLOCK_OP_TYPE_MAX);
    QLIST_FOREACH_SAFE(blocker, &bs->op_blockers[op], list, next) {
        if (blocker->reason == reason) {
            QLIST_REMOVE(blocker, list);
            g_free(blocker);
        }
    }
}

void bdrv_op_block_all(BlockDriverState *bs, Error *reason)
{
    int i;
    GLOBAL_STATE_CODE();
    for (i = 0; i < BLOCK_OP_TYPE_MAX; i++) {
        bdrv_op_block(bs, i, reason);
    }
}

void bdrv_op_unblock_all(BlockDriverState *bs, Error *reason)
{
    int i;
    GLOBAL_STATE_CODE();
    for (i = 0; i < BLOCK_OP_TYPE_MAX; i++) {
        bdrv_op_unblock(bs, i, reason);
    }
}

bool bdrv_op_blocker_is_empty(BlockDriverState *bs)
{
    int i;
    GLOBAL_STATE_CODE();
    for (i = 0; i < BLOCK_OP_TYPE_MAX; i++) {
        if (!QLIST_EMPTY(&bs->op_blockers[i])) {
            return false;
        }
    }
    return true;
}

void bdrv_img_create(const char *filename, const char *fmt,
                     const char *base_filename, const char *base_fmt,
                     char *options, uint64_t img_size, int flags, bool quiet,
                     Error **errp)
{
    QemuOptsList *create_opts = NULL;
    QemuOpts *opts = NULL;
    const char *backing_fmt, *backing_file;
    int64_t size;
    BlockDriver *drv, *proto_drv;
    Error *local_err = NULL;
    int ret = 0;

    GLOBAL_STATE_CODE();

    drv = bdrv_find_format(fmt);
    if (!drv) {
        error_setg(errp, "Unknown file format '%s'", fmt);
        return;
    }

    proto_drv = bdrv_find_protocol(filename, true, errp);
    if (!proto_drv) {
        return;
    }

    if (!drv->create_opts) {
        error_setg(errp, "Format driver '%s' does not support image creation",
                   drv->format_name);
        return;
    }

    if (!proto_drv->create_opts) {
        error_setg(errp, "Protocol driver '%s' does not support image creation",
                   proto_drv->format_name);
        return;
    }

    create_opts = qemu_opts_append(create_opts, drv->create_opts);
    create_opts = qemu_opts_append(create_opts, proto_drv->create_opts);

    opts = qemu_opts_create(create_opts, NULL, 0, &error_abort);

    if (options) {
        if (!qemu_opts_do_parse(opts, options, NULL, errp)) {
            goto out;
        }
    }

    if (!qemu_opt_get(opts, BLOCK_OPT_SIZE)) {
        qemu_opt_set_number(opts, BLOCK_OPT_SIZE, img_size, &error_abort);
    } else if (img_size != UINT64_C(-1)) {
        error_setg(errp, "The image size must be specified only once");
        goto out;
    }

    if (base_filename) {
        if (!qemu_opt_set(opts, BLOCK_OPT_BACKING_FILE, base_filename,
                          NULL)) {
            error_setg(errp, "Backing file not supported for file format '%s'",
                       fmt);
            goto out;
        }
    }

    if (base_fmt) {
        if (!qemu_opt_set(opts, BLOCK_OPT_BACKING_FMT, base_fmt, NULL)) {
            error_setg(errp, "Backing file format not supported for file "
                             "format '%s'", fmt);
            goto out;
        }
    }

    backing_file = qemu_opt_get(opts, BLOCK_OPT_BACKING_FILE);
    if (backing_file) {
        if (!strcmp(filename, backing_file)) {
            error_setg(errp, "Error: Trying to create an image with the "
                             "same filename as the backing file");
            goto out;
        }
        if (backing_file[0] == '\0') {
            error_setg(errp, "Expected backing file name, got empty string");
            goto out;
        }
    }

    backing_fmt = qemu_opt_get(opts, BLOCK_OPT_BACKING_FMT);

    size = qemu_opt_get_size(opts, BLOCK_OPT_SIZE, img_size);
    if (backing_file && !(flags & BDRV_O_NO_BACKING)) {
        BlockDriverState *bs;
        char *full_backing;
        int back_flags;
        QDict *backing_options = NULL;

        full_backing =
            bdrv_get_full_backing_filename_from_filename(filename, backing_file,
                                                         &local_err);
        if (local_err) {
            goto out;
        }
        assert(full_backing);

        back_flags = flags;
        back_flags &= ~(BDRV_O_RDWR | BDRV_O_SNAPSHOT | BDRV_O_NO_BACKING);
        back_flags |= BDRV_O_NO_IO;

        backing_options = qdict_new();
        if (backing_fmt) {
            qdict_put_str(backing_options, "driver", backing_fmt);
        }
        qdict_put_bool(backing_options, BDRV_OPT_FORCE_SHARE, true);

        bs = bdrv_open(full_backing, NULL, backing_options, back_flags,
                       &local_err);
        g_free(full_backing);
        if (!bs) {
            error_append_hint(&local_err, "Could not open backing image.\n");
            goto out;
        } else {
            if (!backing_fmt) {
                error_setg(&local_err,
                           "Backing file specified without backing format");
                error_append_hint(&local_err, "Detected format of %s.\n",
                                  bs->drv->format_name);
                goto out;
            }
            if (size == -1) {
                size = bdrv_getlength(bs);
                if (size < 0) {
                    error_setg_errno(errp, -size, "Could not get size of '%s'",
                                     backing_file);
                    bdrv_unref(bs);
                    goto out;
                }
                qemu_opt_set_number(opts, BLOCK_OPT_SIZE, size, &error_abort);
            }
            bdrv_unref(bs);
        }
    } else if (backing_file && !backing_fmt) {
        error_setg(&local_err,
                   "Backing file specified without backing format");
        goto out;
    }

    if (size == -1 &&
        !(!strcmp(fmt, "luks") &&
          qemu_opt_get_bool(opts, "detached-header", false))) {
        error_setg(errp, "Image creation needs a size parameter");
        goto out;
    }

    if (!quiet) {
        printf("Formatting '%s', fmt=%s ", filename, fmt);
        qemu_opts_print(opts, " ");
        puts("");
        fflush(stdout);
    }

    ret = bdrv_create(drv, filename, opts, &local_err);

    if (ret == -EFBIG) {
        const char *cluster_size_hint = "";
        if (qemu_opt_get_size(opts, BLOCK_OPT_CLUSTER_SIZE, 0)) {
            cluster_size_hint = " (try using a larger cluster size)";
        }
        error_setg(errp, "The image size is too large for file format '%s'"
                   "%s", fmt, cluster_size_hint);
        error_free(local_err);
        local_err = NULL;
    }

out:
    qemu_opts_del(opts);
    qemu_opts_free(create_opts);
    error_propagate(errp, local_err);
}

AioContext *bdrv_get_aio_context(BlockDriverState *bs)
{
    IO_CODE();
    return bs ? bs->aio_context : qemu_get_aio_context();
}

AioContext *coroutine_fn bdrv_co_enter(BlockDriverState *bs)
{
    Coroutine *self = qemu_coroutine_self();
    AioContext *old_ctx = qemu_coroutine_get_aio_context(self);
    AioContext *new_ctx;
    IO_CODE();

    bdrv_inc_in_flight(bs);

    new_ctx = bdrv_get_aio_context(bs);
    aio_co_reschedule_self(new_ctx);
    return old_ctx;
}

void coroutine_fn bdrv_co_leave(BlockDriverState *bs, AioContext *old_ctx)
{
    IO_CODE();
    aio_co_reschedule_self(old_ctx);
    bdrv_dec_in_flight(bs);
}

static void bdrv_do_remove_aio_context_notifier(BdrvAioNotifier *ban)
{
    GLOBAL_STATE_CODE();
    QLIST_REMOVE(ban, list);
    g_free(ban);
}

static void bdrv_detach_aio_context(BlockDriverState *bs)
{
    BdrvAioNotifier *baf, *baf_tmp;

    assert(!bs->walking_aio_notifiers);
    GLOBAL_STATE_CODE();
    bs->walking_aio_notifiers = true;
    QLIST_FOREACH_SAFE(baf, &bs->aio_notifiers, list, baf_tmp) {
        if (baf->deleted) {
            bdrv_do_remove_aio_context_notifier(baf);
        } else {
            baf->detach_aio_context(baf->opaque);
        }
    }
    bs->walking_aio_notifiers = false;

    if (bs->drv && bs->drv->bdrv_detach_aio_context) {
        bs->drv->bdrv_detach_aio_context(bs);
    }

    bs->aio_context = NULL;
}

static void bdrv_attach_aio_context(BlockDriverState *bs,
                                    AioContext *new_context)
{
    BdrvAioNotifier *ban, *ban_tmp;
    GLOBAL_STATE_CODE();

    bs->aio_context = new_context;

    if (bs->drv && bs->drv->bdrv_attach_aio_context) {
        bs->drv->bdrv_attach_aio_context(bs, new_context);
    }

    assert(!bs->walking_aio_notifiers);
    bs->walking_aio_notifiers = true;
    QLIST_FOREACH_SAFE(ban, &bs->aio_notifiers, list, ban_tmp) {
        if (ban->deleted) {
            bdrv_do_remove_aio_context_notifier(ban);
        } else {
            ban->attached_aio_context(new_context, ban->opaque);
        }
    }
    bs->walking_aio_notifiers = false;
}

typedef struct BdrvStateSetAioContext {
    AioContext *new_ctx;
    BlockDriverState *bs;
} BdrvStateSetAioContext;

static bool bdrv_parent_change_aio_context(BdrvChild *c, AioContext *ctx,
                                           GHashTable *visited,
                                           Transaction *tran,
                                           Error **errp)
{
    GLOBAL_STATE_CODE();
    if (g_hash_table_contains(visited, c)) {
        return true;
    }
    g_hash_table_add(visited, c);

    if (!c->klass->change_aio_ctx) {
        char *user = bdrv_child_user_desc(c);
        error_setg(errp, "Changing iothreads is not supported by %s", user);
        g_free(user);
        return false;
    }
    if (!c->klass->change_aio_ctx(c, ctx, visited, tran, errp)) {
        assert(!errp || *errp);
        return false;
    }
    return true;
}

bool bdrv_child_change_aio_context(BdrvChild *c, AioContext *ctx,
                                   GHashTable *visited, Transaction *tran,
                                   Error **errp)
{
    GLOBAL_STATE_CODE();
    if (g_hash_table_contains(visited, c)) {
        return true;
    }
    g_hash_table_add(visited, c);
    return bdrv_change_aio_context(c->bs, ctx, visited, tran, errp);
}

static void bdrv_set_aio_context_clean(void *opaque)
{
    BdrvStateSetAioContext *state = (BdrvStateSetAioContext *) opaque;
    BlockDriverState *bs = (BlockDriverState *) state->bs;

    bdrv_drained_end(bs);

    g_free(state);
}

static void bdrv_set_aio_context_commit(void *opaque)
{
    BdrvStateSetAioContext *state = (BdrvStateSetAioContext *) opaque;
    BlockDriverState *bs = (BlockDriverState *) state->bs;
    AioContext *new_context = state->new_ctx;

    bdrv_detach_aio_context(bs);
    bdrv_attach_aio_context(bs, new_context);
}

static TransactionActionDrv set_aio_context = {
    .commit = bdrv_set_aio_context_commit,
    .clean = bdrv_set_aio_context_clean,
};

static bool bdrv_change_aio_context(BlockDriverState *bs, AioContext *ctx,
                                    GHashTable *visited, Transaction *tran,
                                    Error **errp)
{
    BdrvChild *c;
    BdrvStateSetAioContext *state;

    GLOBAL_STATE_CODE();

    if (bdrv_get_aio_context(bs) == ctx) {
        return true;
    }

    bdrv_graph_rdlock_main_loop();
    QLIST_FOREACH(c, &bs->parents, next_parent) {
        if (!bdrv_parent_change_aio_context(c, ctx, visited, tran, errp)) {
            bdrv_graph_rdunlock_main_loop();
            return false;
        }
    }

    QLIST_FOREACH(c, &bs->children, next) {
        if (!bdrv_child_change_aio_context(c, ctx, visited, tran, errp)) {
            bdrv_graph_rdunlock_main_loop();
            return false;
        }
    }
    bdrv_graph_rdunlock_main_loop();

    state = g_new(BdrvStateSetAioContext, 1);
    *state = (BdrvStateSetAioContext) {
        .new_ctx = ctx,
        .bs = bs,
    };

    bdrv_drained_begin(bs);

    tran_add(tran, &set_aio_context, state);

    return true;
}

int bdrv_try_change_aio_context(BlockDriverState *bs, AioContext *ctx,
                                BdrvChild *ignore_child, Error **errp)
{
    Transaction *tran;
    GHashTable *visited;
    int ret;
    GLOBAL_STATE_CODE();

    tran = tran_new();
    visited = g_hash_table_new(NULL, NULL);
    if (ignore_child) {
        g_hash_table_add(visited, ignore_child);
    }
    ret = bdrv_change_aio_context(bs, ctx, visited, tran, errp);
    g_hash_table_destroy(visited);


    if (!ret) {
        tran_abort(tran);
        return -EPERM;
    }

    tran_commit(tran);
    return 0;
}

void bdrv_add_aio_context_notifier(BlockDriverState *bs,
        void (*attached_aio_context)(AioContext *new_context, void *opaque),
        void (*detach_aio_context)(void *opaque), void *opaque)
{
    BdrvAioNotifier *ban = g_new(BdrvAioNotifier, 1);
    *ban = (BdrvAioNotifier){
        .attached_aio_context = attached_aio_context,
        .detach_aio_context   = detach_aio_context,
        .opaque               = opaque
    };
    GLOBAL_STATE_CODE();

    QLIST_INSERT_HEAD(&bs->aio_notifiers, ban, list);
}

void bdrv_remove_aio_context_notifier(BlockDriverState *bs,
                                      void (*attached_aio_context)(AioContext *,
                                                                   void *),
                                      void (*detach_aio_context)(void *),
                                      void *opaque)
{
    BdrvAioNotifier *ban, *ban_next;
    GLOBAL_STATE_CODE();

    QLIST_FOREACH_SAFE(ban, &bs->aio_notifiers, list, ban_next) {
        if (ban->attached_aio_context == attached_aio_context &&
            ban->detach_aio_context   == detach_aio_context   &&
            ban->opaque               == opaque               &&
            ban->deleted              == false)
        {
            if (bs->walking_aio_notifiers) {
                ban->deleted = true;
            } else {
                bdrv_do_remove_aio_context_notifier(ban);
            }
            return;
        }
    }

    abort();
}

int bdrv_amend_options(BlockDriverState *bs, QemuOpts *opts,
                       BlockDriverAmendStatusCB *status_cb, void *cb_opaque,
                       bool force,
                       Error **errp)
{
    GLOBAL_STATE_CODE();
    if (!bs->drv) {
        error_setg(errp, "Node is ejected");
        return -ENOMEDIUM;
    }
    if (!bs->drv->bdrv_amend_options) {
        error_setg(errp, "Block driver '%s' does not support option amendment",
                   bs->drv->format_name);
        return -ENOTSUP;
    }
    return bs->drv->bdrv_amend_options(bs, opts, status_cb,
                                       cb_opaque, force, errp);
}

bool bdrv_recurse_can_replace(BlockDriverState *bs,
                              BlockDriverState *to_replace)
{
    BlockDriverState *filtered;

    GLOBAL_STATE_CODE();

    if (!bs || !bs->drv) {
        return false;
    }

    if (bs == to_replace) {
        return true;
    }

    if (bs->drv->bdrv_recurse_can_replace) {
        return bs->drv->bdrv_recurse_can_replace(bs, to_replace);
    }

    filtered = bdrv_filter_bs(bs);
    if (filtered) {
        return bdrv_recurse_can_replace(filtered, to_replace);
    }

    return false;
}

BlockDriverState *check_to_replace_node(BlockDriverState *parent_bs,
                                        const char *node_name, Error **errp)
{
    BlockDriverState *to_replace_bs = bdrv_find_node(node_name);

    GLOBAL_STATE_CODE();

    if (!to_replace_bs) {
        error_setg(errp, "Failed to find node with node-name='%s'", node_name);
        return NULL;
    }

    if (bdrv_op_is_blocked(to_replace_bs, BLOCK_OP_TYPE_REPLACE, errp)) {
        return NULL;
    }

    if (!bdrv_recurse_can_replace(parent_bs, to_replace_bs)) {
        error_setg(errp, "Cannot replace '%s' by a node mirrored from '%s', "
                   "because it cannot be guaranteed that doing so would not "
                   "lead to an abrupt change of visible data",
                   node_name, parent_bs->node_name);
        return NULL;
    }

    return to_replace_bs;
}

static const char *const *strong_options(BlockDriverState *bs,
                                         const char *const *curopt)
{
    static const char *const global_options[] = {
        "driver", "filename", NULL
    };

    if (!curopt) {
        return &global_options[0];
    }

    curopt++;
    if (curopt == &global_options[ARRAY_SIZE(global_options) - 1] && bs->drv) {
        curopt = bs->drv->strong_runtime_opts;
    }

    return (curopt && *curopt) ? curopt : NULL;
}

static bool append_strong_runtime_options(QDict *d, BlockDriverState *bs)
{
    bool found_any = false;
    const char *const *option_name = NULL;

    if (!bs->drv) {
        return false;
    }

    while ((option_name = strong_options(bs, option_name))) {
        bool option_given = false;

        assert(strlen(*option_name) > 0);
        if ((*option_name)[strlen(*option_name) - 1] != '.') {
            QObject *entry = qdict_get(bs->options, *option_name);
            if (!entry) {
                continue;
            }

            qdict_put_obj(d, *option_name, qobject_ref(entry));
            option_given = true;
        } else {
            const QDictEntry *entry;
            for (entry = qdict_first(bs->options); entry;
                 entry = qdict_next(bs->options, entry))
            {
                if (strstart(qdict_entry_key(entry), *option_name, NULL)) {
                    qdict_put_obj(d, qdict_entry_key(entry),
                                  qobject_ref(qdict_entry_value(entry)));
                    option_given = true;
                }
            }
        }

        if (!found_any && option_given &&
            strcmp(*option_name, "driver") && strcmp(*option_name, "filename"))
        {
            found_any = true;
        }
    }

    if (!qdict_haskey(d, "driver")) {
        qdict_put_str(d, "driver", bs->drv->format_name);
    }

    return found_any;
}

static bool GRAPH_RDLOCK bdrv_backing_overridden(BlockDriverState *bs)
{
    GLOBAL_STATE_CODE();
    if (bs->backing) {
        return strcmp(bs->auto_backing_file,
                      bs->backing->bs->filename);
    } else {
        return bs->auto_backing_file[0] != '\0';
    }
}

void bdrv_refresh_filename(BlockDriverState *bs)
{
    BlockDriver *drv = bs->drv;
    BdrvChild *child;
    BlockDriverState *primary_child_bs;
    QDict *opts;
    bool backing_overridden;
    bool generate_json_filename; /* Whether our default implementation should
                                    fill exact_filename (false) or not (true) */

    GLOBAL_STATE_CODE();

    if (!drv) {
        return;
    }

    QLIST_FOREACH(child, &bs->children, next) {
        bdrv_refresh_filename(child->bs);
    }

    if (bs->implicit) {
        child = QLIST_FIRST(&bs->children);
        assert(QLIST_NEXT(child, next) == NULL);

        pstrcpy(bs->exact_filename, sizeof(bs->exact_filename),
                child->bs->exact_filename);
        pstrcpy(bs->filename, sizeof(bs->filename), child->bs->filename);

        qobject_unref(bs->full_open_options);
        bs->full_open_options = qobject_ref(child->bs->full_open_options);

        return;
    }

    backing_overridden = bdrv_backing_overridden(bs);

    if (bs->open_flags & BDRV_O_NO_IO) {
        backing_overridden = false;
    }

    opts = qdict_new();
    generate_json_filename = append_strong_runtime_options(opts, bs);
    generate_json_filename |= backing_overridden;

    if (drv->bdrv_gather_child_options) {
        drv->bdrv_gather_child_options(bs, opts, backing_overridden);
    } else {
        QLIST_FOREACH(child, &bs->children, next) {
            if (child == bs->backing && !backing_overridden) {
                continue;
            }

            qdict_put(opts, child->name,
                      qobject_ref(child->bs->full_open_options));
        }

        if (backing_overridden && !bs->backing) {
            qdict_put_null(opts, "backing");
        }
    }

    qobject_unref(bs->full_open_options);
    bs->full_open_options = opts;

    primary_child_bs = bdrv_primary_bs(bs);

    if (drv->bdrv_refresh_filename) {
        bs->exact_filename[0] = '\0';

        drv->bdrv_refresh_filename(bs);
    } else if (primary_child_bs) {

        bs->exact_filename[0] = '\0';

        if (primary_child_bs->exact_filename[0] &&
            primary_child_bs->drv->protocol_name &&
            !drv->is_filter && !generate_json_filename)
        {
            strcpy(bs->exact_filename, primary_child_bs->exact_filename);
        }
    }

    if (bs->exact_filename[0]) {
        pstrcpy(bs->filename, sizeof(bs->filename), bs->exact_filename);
    } else {
        GString *json = qobject_to_json(QOBJECT(bs->full_open_options));
        if (snprintf(bs->filename, sizeof(bs->filename), "json:%s",
                     json->str) >= sizeof(bs->filename)) {
            strcpy(bs->filename + sizeof(bs->filename) - 4, "...");
        }
        g_string_free(json, true);
    }
}

char *bdrv_dirname(BlockDriverState *bs, Error **errp)
{
    BlockDriver *drv = bs->drv;
    BlockDriverState *child_bs;

    GLOBAL_STATE_CODE();

    if (!drv) {
        error_setg(errp, "Node '%s' is ejected", bs->node_name);
        return NULL;
    }

    if (drv->bdrv_dirname) {
        return drv->bdrv_dirname(bs, errp);
    }

    child_bs = bdrv_primary_bs(bs);
    if (child_bs) {
        return bdrv_dirname(child_bs, errp);
    }

    bdrv_refresh_filename(bs);
    if (bs->exact_filename[0] != '\0') {
        return path_combine(bs->exact_filename, "");
    }

    error_setg(errp, "Cannot generate a base directory for %s nodes",
               drv->format_name);
    return NULL;
}

void bdrv_add_child(BlockDriverState *parent_bs, BlockDriverState *child_bs,
                    Error **errp)
{
    GLOBAL_STATE_CODE();
    if (!parent_bs->drv || !parent_bs->drv->bdrv_add_child) {
        error_setg(errp, "The node %s does not support adding a child",
                   bdrv_get_device_or_node_name(parent_bs));
        return;
    }

    if (!parent_bs->drv->supports_zoned_children &&
        child_bs->bl.zoned == BLK_Z_HM) {
        error_setg(errp, "Cannot add a %s child to a %s parent",
                   child_bs->bl.zoned == BLK_Z_HM ? "zoned" : "non-zoned",
                   parent_bs->drv->supports_zoned_children ?
                   "support zoned children" : "not support zoned children");
        return;
    }

    if (!QLIST_EMPTY(&child_bs->parents)) {
        error_setg(errp, "The node %s already has a parent",
                   child_bs->node_name);
        return;
    }

    parent_bs->drv->bdrv_add_child(parent_bs, child_bs, errp);
}

void bdrv_del_child(BlockDriverState *parent_bs, BdrvChild *child, Error **errp)
{
    BdrvChild *tmp;

    GLOBAL_STATE_CODE();
    if (!parent_bs->drv || !parent_bs->drv->bdrv_del_child) {
        error_setg(errp, "The node %s does not support removing a child",
                   bdrv_get_device_or_node_name(parent_bs));
        return;
    }

    QLIST_FOREACH(tmp, &parent_bs->children, next) {
        if (tmp == child) {
            break;
        }
    }

    if (!tmp) {
        error_setg(errp, "The node %s does not have a child named %s",
                   bdrv_get_device_or_node_name(parent_bs),
                   bdrv_get_device_or_node_name(child->bs));
        return;
    }

    parent_bs->drv->bdrv_del_child(parent_bs, child, errp);
}

int bdrv_make_empty(BdrvChild *c, Error **errp)
{
    BlockDriver *drv = c->bs->drv;
    int ret;

    GLOBAL_STATE_CODE();
    assert(c->perm & (BLK_PERM_WRITE | BLK_PERM_WRITE_UNCHANGED));

    if (!drv->bdrv_make_empty) {
        error_setg(errp, "%s does not support emptying nodes",
                   drv->format_name);
        return -ENOTSUP;
    }

    ret = drv->bdrv_make_empty(c->bs);
    if (ret < 0) {
        error_setg_errno(errp, -ret, "Failed to empty %s",
                         c->bs->filename);
        return ret;
    }

    return 0;
}

BdrvChild *bdrv_cow_child(BlockDriverState *bs)
{
    IO_CODE();

    if (!bs || !bs->drv) {
        return NULL;
    }

    if (bs->drv->is_filter) {
        return NULL;
    }

    if (!bs->backing) {
        return NULL;
    }

    assert(bs->backing->role & BDRV_CHILD_COW);
    return bs->backing;
}

BdrvChild *bdrv_filter_child(BlockDriverState *bs)
{
    BdrvChild *c;
    IO_CODE();

    if (!bs || !bs->drv) {
        return NULL;
    }

    if (!bs->drv->is_filter) {
        return NULL;
    }

    assert(!(bs->backing && bs->file));

    c = bs->backing ?: bs->file;
    if (!c) {
        return NULL;
    }

    assert(c->role & BDRV_CHILD_FILTERED);
    return c;
}

BdrvChild *bdrv_filter_or_cow_child(BlockDriverState *bs)
{
    BdrvChild *cow_child = bdrv_cow_child(bs);
    BdrvChild *filter_child = bdrv_filter_child(bs);
    IO_CODE();

    assert(!(cow_child && filter_child));

    return cow_child ?: filter_child;
}

BdrvChild *bdrv_primary_child(BlockDriverState *bs)
{
    BdrvChild *c, *found = NULL;
    IO_CODE();

    QLIST_FOREACH(c, &bs->children, next) {
        if (c->role & BDRV_CHILD_PRIMARY) {
            assert(!found);
            found = c;
        }
    }

    return found;
}

static BlockDriverState * GRAPH_RDLOCK
bdrv_do_skip_filters(BlockDriverState *bs, bool stop_on_explicit_filter)
{
    BdrvChild *c;

    if (!bs) {
        return NULL;
    }

    while (!(stop_on_explicit_filter && !bs->implicit)) {
        c = bdrv_filter_child(bs);
        if (!c) {
            assert(!bs->drv || !bs->drv->is_filter);
            break;
        }
        bs = c->bs;
    }

    return bs;
}

BlockDriverState *bdrv_skip_implicit_filters(BlockDriverState *bs)
{
    GLOBAL_STATE_CODE();
    return bdrv_do_skip_filters(bs, true);
}

BlockDriverState *bdrv_skip_filters(BlockDriverState *bs)
{
    IO_CODE();
    return bdrv_do_skip_filters(bs, false);
}

BlockDriverState *bdrv_backing_chain_next(BlockDriverState *bs)
{
    IO_CODE();
    return bdrv_skip_filters(bdrv_cow_bs(bdrv_skip_filters(bs)));
}

static bool bdrv_bsc_range_overlaps_locked(BlockDriverState *bs,
                                           int64_t offset, int64_t bytes,
                                           int64_t *pnum)
{
    BdrvBlockStatusCache *bsc = qatomic_rcu_read(&bs->block_status_cache);
    bool overlaps;

    overlaps =
        qatomic_read(&bsc->valid) &&
        ranges_overlap(offset, bytes, bsc->data_start,
                       bsc->data_end - bsc->data_start);

    if (overlaps && pnum) {
        *pnum = bsc->data_end - offset;
    }

    return overlaps;
}

bool bdrv_bsc_is_data(BlockDriverState *bs, int64_t offset, int64_t *pnum)
{
    IO_CODE();
    RCU_READ_LOCK_GUARD();
    return bdrv_bsc_range_overlaps_locked(bs, offset, 1, pnum);
}

void bdrv_bsc_invalidate_range(BlockDriverState *bs,
                               int64_t offset, int64_t bytes)
{
    IO_CODE();
    RCU_READ_LOCK_GUARD();

    if (bdrv_bsc_range_overlaps_locked(bs, offset, bytes, NULL)) {
        qatomic_set(&bs->block_status_cache->valid, false);
    }
}

void bdrv_bsc_fill(BlockDriverState *bs, int64_t offset, int64_t bytes)
{
    BdrvBlockStatusCache *new_bsc = g_new(BdrvBlockStatusCache, 1);
    BdrvBlockStatusCache *old_bsc;
    IO_CODE();

    *new_bsc = (BdrvBlockStatusCache) {
        .valid = true,
        .data_start = offset,
        .data_end = offset + bytes,
    };

    QEMU_LOCK_GUARD(&bs->bsc_modify_lock);

    old_bsc = qatomic_rcu_read(&bs->block_status_cache);
    qatomic_rcu_set(&bs->block_status_cache, new_bsc);
    if (old_bsc) {
        g_free_rcu(old_bsc, rcu);
    }
}
