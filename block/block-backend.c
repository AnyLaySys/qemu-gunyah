
#include "qemu/osdep.h"
#include "system/block-backend.h"
#include "block/block_int.h"
#include "block/coroutines.h"
#include "hw/qdev-core.h"
#include "system/blockdev.h"
#include "system/runstate.h"
#include "qapi/error.h"
#include "qapi/qapi-events-block.h"
#include "qemu/id.h"
#include "qemu/main-loop.h"
#include "qemu/option.h"
#include "trace.h"

#define COROUTINE_POOL_RESERVATION 64

#define NOT_DONE 0x7fffffff /* used while emulated sync operation in progress */

typedef struct BlockBackendAioNotifier {
    void (*attached_aio_context)(AioContext *new_context, void *opaque);
    void (*detach_aio_context)(void *opaque);
    void *opaque;
    QLIST_ENTRY(BlockBackendAioNotifier) list;
} BlockBackendAioNotifier;

struct BlockBackend {
    char *name;
    int refcnt;
    BdrvChild *root;
    AioContext *ctx; /* access with atomic operations only */
    DriveInfo *legacy_dinfo;    /* null unless created by drive_new() */
    QTAILQ_ENTRY(BlockBackend) link;         /* for block_backends */
    QTAILQ_ENTRY(BlockBackend) monitor_link; /* for monitor_block_backends */

    DeviceState *dev;           /* attached device model, if any */
    const BlockDevOps *dev_ops;
    void *dev_opaque;

    BlockBackendRootState root_state;

    bool enable_write_cache;

    BlockAcctStats stats;

    BlockdevOnError on_read_error, on_write_error;
    bool iostatus_enabled;
    BlockDeviceIoStatus iostatus;

    uint64_t perm;
    uint64_t shared_perm;
    bool disable_perm;

    bool allow_aio_context_change;
    bool allow_write_beyond_eof;

    NotifierList remove_bs_notifiers, insert_bs_notifiers;
    QLIST_HEAD(, BlockBackendAioNotifier) aio_notifiers;

    int quiesce_counter; /* atomic: written under BQL, read by other threads */
    QemuMutex queued_requests_lock; /* protects queued_requests */
    CoQueue queued_requests;
    bool disable_request_queuing; /* atomic */

    VMChangeStateEntry *vmsh;
    bool force_allow_inactivate;

    unsigned int in_flight;
};

typedef struct BlockBackendAIOCB {
    BlockAIOCB common;
    BlockBackend *blk;
    int ret;
} BlockBackendAIOCB;

static const AIOCBInfo block_backend_aiocb_info = {
    .aiocb_size = sizeof(BlockBackendAIOCB),
};

static void drive_info_del(DriveInfo *dinfo);
static BlockBackend *bdrv_first_blk(BlockDriverState *bs);

static QTAILQ_HEAD(, BlockBackend) block_backends =
    QTAILQ_HEAD_INITIALIZER(block_backends);

static QTAILQ_HEAD(, BlockBackend) monitor_block_backends =
    QTAILQ_HEAD_INITIALIZER(monitor_block_backends);

static int coroutine_mixed_fn GRAPH_RDLOCK
blk_set_perm_locked(BlockBackend *blk, uint64_t perm, uint64_t shared_perm,
                    Error **errp);

static void blk_root_inherit_options(BdrvChildRole role, bool parent_is_format,
                                     int *child_flags, QDict *child_options,
                                     int parent_flags, QDict *parent_options)
{
    abort();
}
static void blk_root_drained_begin(BdrvChild *child);
static bool blk_root_drained_poll(BdrvChild *child);
static void blk_root_drained_end(BdrvChild *child);

static void blk_root_change_media(BdrvChild *child, bool load);
static void blk_root_resize(BdrvChild *child);

static bool blk_root_change_aio_ctx(BdrvChild *child, AioContext *ctx,
                                    GHashTable *visited, Transaction *tran,
                                    Error **errp);

static char *blk_root_get_parent_desc(BdrvChild *child)
{
    BlockBackend *blk = child->opaque;
    g_autofree char *dev_id = NULL;

    if (blk->name) {
        return g_strdup_printf("block device '%s'", blk->name);
    }

    dev_id = blk_get_attached_dev_id(blk);
    if (*dev_id) {
        return g_strdup_printf("block device '%s'", dev_id);
    } else {
        return g_strdup("an unnamed block device");
    }
}

static const char *blk_root_get_name(BdrvChild *child)
{
    return blk_name(child->opaque);
}

static void GRAPH_RDLOCK blk_root_activate(BdrvChild *child, Error **errp)
{
    BlockBackend *blk = child->opaque;
    Error *local_err = NULL;
    uint64_t saved_shared_perm;

    if (!blk->disable_perm) {
        return;
    }

    blk->disable_perm = false;

    saved_shared_perm = blk->shared_perm;

    blk_set_perm_locked(blk, blk->perm, BLK_PERM_ALL, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        blk->disable_perm = true;
        return;
    }
    blk->shared_perm = saved_shared_perm;

    blk_set_perm_locked(blk, blk->perm, blk->shared_perm, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        blk->disable_perm = true;
        return;
    }
}

void blk_set_force_allow_inactivate(BlockBackend *blk)
{
    GLOBAL_STATE_CODE();
    blk->force_allow_inactivate = true;
}

static bool blk_can_inactivate(BlockBackend *blk)
{
    if (blk->dev || blk_name(blk)[0]) {
        return true;
    }

    if (!(blk->perm & ~BLK_PERM_CONSISTENT_READ)) {
        return true;
    }

    return blk->force_allow_inactivate;
}

static int GRAPH_RDLOCK blk_root_inactivate(BdrvChild *child)
{
    BlockBackend *blk = child->opaque;

    if (blk->disable_perm) {
        return 0;
    }

    if (!blk_can_inactivate(blk)) {
        return -EPERM;
    }

    blk->disable_perm = true;
    if (blk->root) {
        bdrv_child_try_set_perm(blk->root, 0, BLK_PERM_ALL, &error_abort);
    }

    return 0;
}

static void blk_root_attach(BdrvChild *child)
{
    BlockBackend *blk = child->opaque;
    BlockBackendAioNotifier *notifier;

    trace_blk_root_attach(child, blk, child->bs);

    QLIST_FOREACH(notifier, &blk->aio_notifiers, list) {
        bdrv_add_aio_context_notifier(child->bs,
                notifier->attached_aio_context,
                notifier->detach_aio_context,
                notifier->opaque);
    }
}

static void blk_root_detach(BdrvChild *child)
{
    BlockBackend *blk = child->opaque;
    BlockBackendAioNotifier *notifier;

    trace_blk_root_detach(child, blk, child->bs);

    QLIST_FOREACH(notifier, &blk->aio_notifiers, list) {
        bdrv_remove_aio_context_notifier(child->bs,
                notifier->attached_aio_context,
                notifier->detach_aio_context,
                notifier->opaque);
    }
}

static AioContext *blk_root_get_parent_aio_context(BdrvChild *c)
{
    BlockBackend *blk = c->opaque;
    IO_CODE();

    return blk_get_aio_context(blk);
}

static const BdrvChildClass child_root = {
    .inherit_options    = blk_root_inherit_options,

    .change_media       = blk_root_change_media,
    .resize             = blk_root_resize,
    .get_name           = blk_root_get_name,
    .get_parent_desc    = blk_root_get_parent_desc,

    .drained_begin      = blk_root_drained_begin,
    .drained_poll       = blk_root_drained_poll,
    .drained_end        = blk_root_drained_end,

    .activate           = blk_root_activate,
    .inactivate         = blk_root_inactivate,

    .attach             = blk_root_attach,
    .detach             = blk_root_detach,

    .change_aio_ctx     = blk_root_change_aio_ctx,

    .get_parent_aio_context = blk_root_get_parent_aio_context,
};

BlockBackend *blk_new(AioContext *ctx, uint64_t perm, uint64_t shared_perm)
{
    BlockBackend *blk;

    GLOBAL_STATE_CODE();

    blk = g_new0(BlockBackend, 1);
    blk->refcnt = 1;
    blk->ctx = ctx;
    blk->perm = perm;
    blk->shared_perm = shared_perm;
    blk_set_enable_write_cache(blk, true);

    blk->on_read_error = BLOCKDEV_ON_ERROR_REPORT;
    blk->on_write_error = BLOCKDEV_ON_ERROR_ENOSPC;

    block_acct_init(&blk->stats);

    qemu_mutex_init(&blk->queued_requests_lock);
    qemu_co_queue_init(&blk->queued_requests);
    notifier_list_init(&blk->remove_bs_notifiers);
    notifier_list_init(&blk->insert_bs_notifiers);
    QLIST_INIT(&blk->aio_notifiers);

    QTAILQ_INSERT_TAIL(&block_backends, blk, link);
    return blk;
}

BlockBackend *blk_new_with_bs(BlockDriverState *bs, uint64_t perm,
                              uint64_t shared_perm, Error **errp)
{
    BlockBackend *blk = blk_new(bdrv_get_aio_context(bs), perm, shared_perm);

    GLOBAL_STATE_CODE();

    if (blk_insert_bs(blk, bs, errp) < 0) {
        blk_unref(blk);
        return NULL;
    }
    return blk;
}

BlockBackend *blk_new_open(const char *filename, const char *reference,
                           QDict *options, int flags, Error **errp)
{
    BlockBackend *blk;
    BlockDriverState *bs;
    uint64_t perm = 0;
    uint64_t shared = BLK_PERM_ALL;

    GLOBAL_STATE_CODE();

    if ((flags & BDRV_O_NO_IO) == 0) {
        perm |= BLK_PERM_CONSISTENT_READ;
        if (flags & BDRV_O_RDWR) {
            perm |= BLK_PERM_WRITE;
        }
    }
    if (flags & BDRV_O_RESIZE) {
        perm |= BLK_PERM_RESIZE;
    }
    if (flags & BDRV_O_NO_SHARE) {
        shared = BLK_PERM_CONSISTENT_READ | BLK_PERM_WRITE_UNCHANGED;
    }

    bs = bdrv_open(filename, reference, options, flags, errp);
    if (!bs) {
        return NULL;
    }

    blk = blk_new(bdrv_get_aio_context(bs), perm, shared);
    blk->perm = perm;
    blk->shared_perm = shared;

    blk_insert_bs(blk, bs, errp);
    bdrv_unref(bs);

    if (!blk->root) {
        blk_unref(blk);
        return NULL;
    }

    return blk;
}

static void blk_delete(BlockBackend *blk)
{
    assert(!blk->refcnt);
    assert(!blk->name);
    assert(!blk->dev);
    if (blk->root) {
        blk_remove_bs(blk);
    }
    if (blk->vmsh) {
        qemu_del_vm_change_state_handler(blk->vmsh);
        blk->vmsh = NULL;
    }
    assert(QLIST_EMPTY(&blk->remove_bs_notifiers.notifiers));
    assert(QLIST_EMPTY(&blk->insert_bs_notifiers.notifiers));
    assert(QLIST_EMPTY(&blk->aio_notifiers));
    assert(qemu_co_queue_empty(&blk->queued_requests));
    qemu_mutex_destroy(&blk->queued_requests_lock);
    QTAILQ_REMOVE(&block_backends, blk, link);
    drive_info_del(blk->legacy_dinfo);
    block_acct_cleanup(&blk->stats);
    g_free(blk);
}

static void drive_info_del(DriveInfo *dinfo)
{
    if (!dinfo) {
        return;
    }
    qemu_opts_del(dinfo->opts);
    g_free(dinfo);
}

int blk_get_refcnt(BlockBackend *blk)
{
    GLOBAL_STATE_CODE();
    return blk ? blk->refcnt : 0;
}

void blk_ref(BlockBackend *blk)
{
    assert(blk->refcnt > 0);
    GLOBAL_STATE_CODE();
    blk->refcnt++;
}

void blk_unref(BlockBackend *blk)
{
    GLOBAL_STATE_CODE();
    if (blk) {
        assert(blk->refcnt > 0);
        if (blk->refcnt > 1) {
            blk->refcnt--;
        } else {
            blk_drain(blk);
            assert(blk->refcnt == 1);
            blk->refcnt = 0;
            blk_delete(blk);
        }
    }
}

BlockBackend *blk_all_next(BlockBackend *blk)
{
    GLOBAL_STATE_CODE();
    return blk ? QTAILQ_NEXT(blk, link)
               : QTAILQ_FIRST(&block_backends);
}

void blk_remove_all_bs(void)
{
    BlockBackend *blk = NULL;

    GLOBAL_STATE_CODE();

    while ((blk = blk_all_next(blk)) != NULL) {
        if (blk->root) {
            blk_remove_bs(blk);
        }
    }
}

BlockBackend *blk_next(BlockBackend *blk)
{
    GLOBAL_STATE_CODE();
    return blk ? QTAILQ_NEXT(blk, monitor_link)
               : QTAILQ_FIRST(&monitor_block_backends);
}

BlockDriverState *bdrv_next(BdrvNextIterator *it)
{
    BlockDriverState *bs, *old_bs;

    assert(qemu_get_current_aio_context() == qemu_get_aio_context());

    old_bs = it->bs;

    if (it->phase == BDRV_NEXT_BACKEND_ROOTS) {
        BlockBackend *old_blk = it->blk;

        do {
            it->blk = blk_all_next(it->blk);
            bs = it->blk ? blk_bs(it->blk) : NULL;
        } while (it->blk && (bs == NULL || bdrv_first_blk(bs) != it->blk));

        if (it->blk) {
            blk_ref(it->blk);
        }
        blk_unref(old_blk);

        if (bs) {
            bdrv_ref(bs);
            bdrv_unref(old_bs);
            it->bs = bs;
            return bs;
        }
        it->phase = BDRV_NEXT_MONITOR_OWNED;
    }

    do {
        it->bs = bdrv_next_monitor_owned(it->bs);
        bs = it->bs;
    } while (bs && bdrv_has_blk(bs));

    if (bs) {
        bdrv_ref(bs);
    }
    bdrv_unref(old_bs);

    return bs;
}

static void bdrv_next_reset(BdrvNextIterator *it)
{
    *it = (BdrvNextIterator) {
        .phase = BDRV_NEXT_BACKEND_ROOTS,
    };
}

BlockDriverState *bdrv_first(BdrvNextIterator *it)
{
    GLOBAL_STATE_CODE();
    bdrv_next_reset(it);
    return bdrv_next(it);
}

void bdrv_next_cleanup(BdrvNextIterator *it)
{
    assert(qemu_get_current_aio_context() == qemu_get_aio_context());

    bdrv_unref(it->bs);

    if (it->phase == BDRV_NEXT_BACKEND_ROOTS && it->blk) {
        blk_unref(it->blk);
    }

    bdrv_next_reset(it);
}

bool monitor_add_blk(BlockBackend *blk, const char *name, Error **errp)
{
    assert(!blk->name);
    assert(name && name[0]);
    GLOBAL_STATE_CODE();

    if (!id_wellformed(name)) {
        error_setg(errp, "Invalid device name");
        return false;
    }
    if (blk_by_name(name)) {
        error_setg(errp, "Device with id '%s' already exists", name);
        return false;
    }
    if (bdrv_find_node(name)) {
        error_setg(errp,
                   "Device name '%s' conflicts with an existing node name",
                   name);
        return false;
    }

    blk->name = g_strdup(name);
    QTAILQ_INSERT_TAIL(&monitor_block_backends, blk, monitor_link);
    return true;
}

void monitor_remove_blk(BlockBackend *blk)
{
    GLOBAL_STATE_CODE();

    if (!blk->name) {
        return;
    }

    QTAILQ_REMOVE(&monitor_block_backends, blk, monitor_link);
    g_free(blk->name);
    blk->name = NULL;
}

const char *blk_name(const BlockBackend *blk)
{
    IO_CODE();
    return blk->name ?: "";
}

BlockBackend *blk_by_name(const char *name)
{
    BlockBackend *blk = NULL;

    GLOBAL_STATE_CODE();
    assert(name);
    while ((blk = blk_next(blk)) != NULL) {
        if (!strcmp(name, blk->name)) {
            return blk;
        }
    }
    return NULL;
}

BlockDriverState *blk_bs(BlockBackend *blk)
{
    IO_CODE();
    return blk->root ? blk->root->bs : NULL;
}

static BlockBackend * GRAPH_RDLOCK bdrv_first_blk(BlockDriverState *bs)
{
    BdrvChild *child;

    GLOBAL_STATE_CODE();
    assert_bdrv_graph_readable();

    QLIST_FOREACH(child, &bs->parents, next_parent) {
        if (child->klass == &child_root) {
            return child->opaque;
        }
    }

    return NULL;
}

bool bdrv_has_blk(BlockDriverState *bs)
{
    GLOBAL_STATE_CODE();
    return bdrv_first_blk(bs) != NULL;
}

bool bdrv_is_root_node(BlockDriverState *bs)
{
    BdrvChild *c;

    GLOBAL_STATE_CODE();
    assert_bdrv_graph_readable();

    QLIST_FOREACH(c, &bs->parents, next_parent) {
        if (c->klass != &child_root) {
            return false;
        }
    }

    return true;
}

DriveInfo *blk_legacy_dinfo(BlockBackend *blk)
{
    GLOBAL_STATE_CODE();
    return blk->legacy_dinfo;
}

DriveInfo *blk_set_legacy_dinfo(BlockBackend *blk, DriveInfo *dinfo)
{
    assert(!blk->legacy_dinfo);
    GLOBAL_STATE_CODE();
    return blk->legacy_dinfo = dinfo;
}

BlockBackend *blk_by_legacy_dinfo(DriveInfo *dinfo)
{
    BlockBackend *blk = NULL;
    GLOBAL_STATE_CODE();

    while ((blk = blk_next(blk)) != NULL) {
        if (blk->legacy_dinfo == dinfo) {
            return blk;
        }
    }
    abort();
}

void blk_remove_bs(BlockBackend *blk)
{
    BdrvChild *root;

    GLOBAL_STATE_CODE();

    notifier_list_notify(&blk->remove_bs_notifiers, blk);

    blk_update_root_state(blk);

    blk_drain(blk);
    root = blk->root;
    blk->root = NULL;

    bdrv_graph_wrlock();
    bdrv_root_unref_child(root);
    bdrv_graph_wrunlock();
}

int blk_insert_bs(BlockBackend *blk, BlockDriverState *bs, Error **errp)
{
    uint64_t perm, shared_perm;

    GLOBAL_STATE_CODE();
    bdrv_ref(bs);
    bdrv_graph_wrlock();

    if ((bs->open_flags & BDRV_O_INACTIVE) && blk_can_inactivate(blk)) {
        blk->disable_perm = true;
        perm = 0;
        shared_perm = BLK_PERM_ALL;
    } else {
        perm = blk->perm;
        shared_perm = blk->shared_perm;
    }

    blk->root = bdrv_root_attach_child(bs, "root", &child_root,
                                       BDRV_CHILD_FILTERED | BDRV_CHILD_PRIMARY,
                                       perm, shared_perm, blk, errp);
    bdrv_graph_wrunlock();
    if (blk->root == NULL) {
        return -EPERM;
    }

    notifier_list_notify(&blk->insert_bs_notifiers, blk);

    return 0;
}

int blk_replace_bs(BlockBackend *blk, BlockDriverState *new_bs, Error **errp)
{
    GLOBAL_STATE_CODE();
    return bdrv_replace_child_bs(blk->root, new_bs, errp);
}

static int coroutine_mixed_fn GRAPH_RDLOCK
blk_set_perm_locked(BlockBackend *blk, uint64_t perm, uint64_t shared_perm,
                    Error **errp)
{
    int ret;
    GLOBAL_STATE_CODE();

    if (blk->root && !blk->disable_perm) {
        ret = bdrv_child_try_set_perm(blk->root, perm, shared_perm, errp);
        if (ret < 0) {
            return ret;
        }
    }

    blk->perm = perm;
    blk->shared_perm = shared_perm;

    return 0;
}

int blk_set_perm(BlockBackend *blk, uint64_t perm, uint64_t shared_perm,
                 Error **errp)
{
    GLOBAL_STATE_CODE();
    GRAPH_RDLOCK_GUARD_MAINLOOP();

    return blk_set_perm_locked(blk, perm, shared_perm, errp);
}

void blk_get_perm(BlockBackend *blk, uint64_t *perm, uint64_t *shared_perm)
{
    GLOBAL_STATE_CODE();
    *perm = blk->perm;
    *shared_perm = blk->shared_perm;
}

int blk_attach_dev(BlockBackend *blk, DeviceState *dev)
{
    GLOBAL_STATE_CODE();
    if (blk->dev) {
        return -EBUSY;
    }

    blk_ref(blk);
    blk->dev = dev;
    blk_iostatus_reset(blk);

    return 0;
}

void blk_detach_dev(BlockBackend *blk, DeviceState *dev)
{
    assert(blk->dev == dev);
    GLOBAL_STATE_CODE();
    blk->dev = NULL;
    blk->dev_ops = NULL;
    blk->dev_opaque = NULL;
    blk_set_perm(blk, 0, BLK_PERM_ALL, &error_abort);
    blk_unref(blk);
}

DeviceState *blk_get_attached_dev(BlockBackend *blk)
{
    GLOBAL_STATE_CODE();
    return blk->dev;
}

static char *blk_get_attached_dev_id_or_path(BlockBackend *blk, bool want_id)
{
    DeviceState *dev = blk->dev;
    IO_CODE();

    if (!dev) {
        return g_strdup("");
    } else if (want_id && dev->id) {
        return g_strdup(dev->id);
    }

    return object_get_canonical_path(OBJECT(dev)) ?: g_strdup("");
}

char *blk_get_attached_dev_id(BlockBackend *blk)
{
    return blk_get_attached_dev_id_or_path(blk, true);
}

static char *blk_get_attached_dev_path(BlockBackend *blk)
{
    return blk_get_attached_dev_id_or_path(blk, false);
}

BlockBackend *blk_by_dev(void *dev)
{
    BlockBackend *blk = NULL;

    GLOBAL_STATE_CODE();

    assert(dev != NULL);
    while ((blk = blk_all_next(blk)) != NULL) {
        if (blk->dev == dev) {
            return blk;
        }
    }
    return NULL;
}

void blk_set_dev_ops(BlockBackend *blk, const BlockDevOps *ops,
                     void *opaque)
{
    GLOBAL_STATE_CODE();
    blk->dev_ops = ops;
    blk->dev_opaque = opaque;

    if (qatomic_read(&blk->quiesce_counter) && ops && ops->drained_begin) {
        ops->drained_begin(opaque);
    }
}

void blk_dev_change_media_cb(BlockBackend *blk, bool load, Error **errp)
{
    GLOBAL_STATE_CODE();
    if (blk->dev_ops && blk->dev_ops->change_media_cb) {
        bool tray_was_open, tray_is_open;
        Error *local_err = NULL;

        tray_was_open = blk_dev_is_tray_open(blk);
        blk->dev_ops->change_media_cb(blk->dev_opaque, load, &local_err);
        if (local_err) {
            assert(load == true);
            error_propagate(errp, local_err);
            return;
        }
        tray_is_open = blk_dev_is_tray_open(blk);

        if (tray_was_open != tray_is_open) {
            char *id = blk_get_attached_dev_id(blk);
            qapi_event_send_device_tray_moved(blk_name(blk), id, tray_is_open);
            g_free(id);
        }
    }
}

static void blk_root_change_media(BdrvChild *child, bool load)
{
    blk_dev_change_media_cb(child->opaque, load, NULL);
}

bool blk_dev_has_removable_media(BlockBackend *blk)
{
    GLOBAL_STATE_CODE();
    return !blk->dev || (blk->dev_ops && blk->dev_ops->change_media_cb);
}

bool blk_dev_has_tray(BlockBackend *blk)
{
    IO_CODE();
    return blk->dev_ops && blk->dev_ops->is_tray_open;
}

void blk_dev_eject_request(BlockBackend *blk, bool force)
{
    GLOBAL_STATE_CODE();
    if (blk->dev_ops && blk->dev_ops->eject_request_cb) {
        blk->dev_ops->eject_request_cb(blk->dev_opaque, force);
    }
}

bool blk_dev_is_tray_open(BlockBackend *blk)
{
    IO_CODE();
    if (blk_dev_has_tray(blk)) {
        return blk->dev_ops->is_tray_open(blk->dev_opaque);
    }
    return false;
}

bool blk_dev_is_medium_locked(BlockBackend *blk)
{
    GLOBAL_STATE_CODE();
    if (blk->dev_ops && blk->dev_ops->is_medium_locked) {
        return blk->dev_ops->is_medium_locked(blk->dev_opaque);
    }
    return false;
}

static void blk_root_resize(BdrvChild *child)
{
    BlockBackend *blk = child->opaque;

    if (blk->dev_ops && blk->dev_ops->resize_cb) {
        blk->dev_ops->resize_cb(blk->dev_opaque);
    }
}

void blk_iostatus_enable(BlockBackend *blk)
{
    GLOBAL_STATE_CODE();
    blk->iostatus_enabled = true;
    blk->iostatus = BLOCK_DEVICE_IO_STATUS_OK;
}

bool blk_iostatus_is_enabled(const BlockBackend *blk)
{
    IO_CODE();
    return (blk->iostatus_enabled &&
           (blk->on_write_error == BLOCKDEV_ON_ERROR_ENOSPC ||
            blk->on_write_error == BLOCKDEV_ON_ERROR_STOP   ||
            blk->on_read_error == BLOCKDEV_ON_ERROR_STOP));
}

BlockDeviceIoStatus blk_iostatus(const BlockBackend *blk)
{
    GLOBAL_STATE_CODE();
    return blk->iostatus;
}

void blk_iostatus_reset(BlockBackend *blk)
{
    GLOBAL_STATE_CODE();
    if (blk_iostatus_is_enabled(blk)) {
        blk->iostatus = BLOCK_DEVICE_IO_STATUS_OK;
    }
}

void blk_iostatus_set_err(BlockBackend *blk, int error)
{
    IO_CODE();
    assert(blk_iostatus_is_enabled(blk));
    if (blk->iostatus == BLOCK_DEVICE_IO_STATUS_OK) {
        blk->iostatus = error == ENOSPC ? BLOCK_DEVICE_IO_STATUS_NOSPACE :
                                          BLOCK_DEVICE_IO_STATUS_FAILED;
    }
}

void blk_set_allow_write_beyond_eof(BlockBackend *blk, bool allow)
{
    IO_CODE();
    blk->allow_write_beyond_eof = allow;
}

void blk_set_allow_aio_context_change(BlockBackend *blk, bool allow)
{
    IO_CODE();
    blk->allow_aio_context_change = allow;
}

void blk_set_disable_request_queuing(BlockBackend *blk, bool disable)
{
    IO_CODE();
    qatomic_set(&blk->disable_request_queuing, disable);
}

static int coroutine_fn GRAPH_RDLOCK
blk_check_byte_request(BlockBackend *blk, int64_t offset, int64_t bytes)
{
    int64_t len;

    if (bytes < 0) {
        return -EIO;
    }

    if (!blk_co_is_available(blk)) {
        return -ENOMEDIUM;
    }

    if (offset < 0) {
        return -EIO;
    }

    if (!blk->allow_write_beyond_eof) {
        len = bdrv_co_getlength(blk_bs(blk));
        if (len < 0) {
            return len;
        }

        if (offset > len || len - offset < bytes) {
            return -EIO;
        }
    }

    return 0;
}

bool blk_in_drain(BlockBackend *blk)
{
    GLOBAL_STATE_CODE(); /* change to IO_OR_GS_CODE(), if necessary */
    return qatomic_read(&blk->quiesce_counter);
}

static void coroutine_fn blk_wait_while_drained(BlockBackend *blk)
{
    assert(blk->in_flight > 0);

    if (qatomic_read(&blk->quiesce_counter) &&
        !qatomic_read(&blk->disable_request_queuing)) {
        qemu_mutex_lock(&blk->queued_requests_lock);
        blk_dec_in_flight(blk);
        qemu_co_queue_wait(&blk->queued_requests, &blk->queued_requests_lock);
        blk_inc_in_flight(blk);
        qemu_mutex_unlock(&blk->queued_requests_lock);
    }
}

static int coroutine_fn
blk_co_do_preadv_part(BlockBackend *blk, int64_t offset, int64_t bytes,
                      QEMUIOVector *qiov, size_t qiov_offset,
                      BdrvRequestFlags flags)
{
    int ret;
    BlockDriverState *bs;
    IO_CODE();

    blk_wait_while_drained(blk);
    GRAPH_RDLOCK_GUARD();

    bs = blk_bs(blk);
    trace_blk_co_preadv(blk, bs, offset, bytes, flags);

    ret = blk_check_byte_request(blk, offset, bytes);
    if (ret < 0) {
        return ret;
    }

    bdrv_inc_in_flight(bs);

    ret = bdrv_co_preadv_part(blk->root, offset, bytes, qiov, qiov_offset,
                              flags);
    bdrv_dec_in_flight(bs);
    return ret;
}

int coroutine_fn blk_co_pread(BlockBackend *blk, int64_t offset, int64_t bytes,
                              void *buf, BdrvRequestFlags flags)
{
    QEMUIOVector qiov = QEMU_IOVEC_INIT_BUF(qiov, buf, bytes);
    IO_OR_GS_CODE();

    assert(bytes <= SIZE_MAX);

    return blk_co_preadv(blk, offset, bytes, &qiov, flags);
}

int coroutine_fn blk_co_preadv(BlockBackend *blk, int64_t offset,
                               int64_t bytes, QEMUIOVector *qiov,
                               BdrvRequestFlags flags)
{
    int ret;
    IO_OR_GS_CODE();

    blk_inc_in_flight(blk);
    ret = blk_co_do_preadv_part(blk, offset, bytes, qiov, 0, flags);
    blk_dec_in_flight(blk);

    return ret;
}

int coroutine_fn blk_co_preadv_part(BlockBackend *blk, int64_t offset,
                                    int64_t bytes, QEMUIOVector *qiov,
                                    size_t qiov_offset, BdrvRequestFlags flags)
{
    int ret;
    IO_OR_GS_CODE();

    blk_inc_in_flight(blk);
    ret = blk_co_do_preadv_part(blk, offset, bytes, qiov, qiov_offset, flags);
    blk_dec_in_flight(blk);

    return ret;
}

static int coroutine_fn
blk_co_do_pwritev_part(BlockBackend *blk, int64_t offset, int64_t bytes,
                       QEMUIOVector *qiov, size_t qiov_offset,
                       BdrvRequestFlags flags)
{
    int ret;
    BlockDriverState *bs;
    IO_CODE();

    blk_wait_while_drained(blk);
    GRAPH_RDLOCK_GUARD();

    bs = blk_bs(blk);
    trace_blk_co_pwritev(blk, bs, offset, bytes, flags);

    ret = blk_check_byte_request(blk, offset, bytes);
    if (ret < 0) {
        return ret;
    }

    bdrv_inc_in_flight(bs);
    if (!blk->enable_write_cache) {
        flags |= BDRV_REQ_FUA;
    }

    ret = bdrv_co_pwritev_part(blk->root, offset, bytes, qiov, qiov_offset,
                               flags);
    bdrv_dec_in_flight(bs);
    return ret;
}

int coroutine_fn blk_co_pwritev_part(BlockBackend *blk, int64_t offset,
                                     int64_t bytes,
                                     QEMUIOVector *qiov, size_t qiov_offset,
                                     BdrvRequestFlags flags)
{
    int ret;
    IO_OR_GS_CODE();

    blk_inc_in_flight(blk);
    ret = blk_co_do_pwritev_part(blk, offset, bytes, qiov, qiov_offset, flags);
    blk_dec_in_flight(blk);

    return ret;
}

int coroutine_fn blk_co_pwrite(BlockBackend *blk, int64_t offset, int64_t bytes,
                               const void *buf, BdrvRequestFlags flags)
{
    QEMUIOVector qiov = QEMU_IOVEC_INIT_BUF(qiov, buf, bytes);
    IO_OR_GS_CODE();

    assert(bytes <= SIZE_MAX);

    return blk_co_pwritev(blk, offset, bytes, &qiov, flags);
}

int coroutine_fn blk_co_pwritev(BlockBackend *blk, int64_t offset,
                                int64_t bytes, QEMUIOVector *qiov,
                                BdrvRequestFlags flags)
{
    IO_OR_GS_CODE();
    return blk_co_pwritev_part(blk, offset, bytes, qiov, 0, flags);
}

int coroutine_fn blk_co_block_status_above(BlockBackend *blk,
                                           BlockDriverState *base,
                                           int64_t offset, int64_t bytes,
                                           int64_t *pnum, int64_t *map,
                                           BlockDriverState **file)
{
    IO_CODE();
    GRAPH_RDLOCK_GUARD();
    return bdrv_co_block_status_above(blk_bs(blk), base, offset, bytes, pnum,
                                      map, file);
}

int coroutine_fn blk_co_is_allocated_above(BlockBackend *blk,
                                           BlockDriverState *base,
                                           bool include_base, int64_t offset,
                                           int64_t bytes, int64_t *pnum)
{
    IO_CODE();
    GRAPH_RDLOCK_GUARD();
    return bdrv_co_is_allocated_above(blk_bs(blk), base, include_base, offset,
                                      bytes, pnum);
}

typedef struct BlkRwCo {
    BlockBackend *blk;
    int64_t offset;
    void *iobuf;
    int ret;
    BdrvRequestFlags flags;
} BlkRwCo;

int blk_make_zero(BlockBackend *blk, BdrvRequestFlags flags)
{
    GLOBAL_STATE_CODE();
    return bdrv_make_zero(blk->root, flags);
}

void blk_inc_in_flight(BlockBackend *blk)
{
    IO_CODE();
    qatomic_inc(&blk->in_flight);
}

void blk_dec_in_flight(BlockBackend *blk)
{
    IO_CODE();
    qatomic_dec(&blk->in_flight);
    aio_wait_kick();
}

static void error_callback_bh(void *opaque)
{
    struct BlockBackendAIOCB *acb = opaque;

    blk_dec_in_flight(acb->blk);
    acb->common.cb(acb->common.opaque, acb->ret);
    qemu_aio_unref(acb);
}

BlockAIOCB *blk_abort_aio_request(BlockBackend *blk,
                                  BlockCompletionFunc *cb,
                                  void *opaque, int ret)
{
    struct BlockBackendAIOCB *acb;
    IO_CODE();

    blk_inc_in_flight(blk);
    acb = blk_aio_get(&block_backend_aiocb_info, blk, cb, opaque);
    acb->blk = blk;
    acb->ret = ret;

    aio_bh_schedule_oneshot(qemu_get_current_aio_context(),
                                     error_callback_bh, acb);
    return &acb->common;
}

typedef struct BlkAioEmAIOCB {
    BlockAIOCB common;
    BlkRwCo rwco;
    int64_t bytes;
    bool has_returned;
} BlkAioEmAIOCB;

static const AIOCBInfo blk_aio_em_aiocb_info = {
    .aiocb_size         = sizeof(BlkAioEmAIOCB),
};

static void blk_aio_complete(BlkAioEmAIOCB *acb)
{
    if (acb->has_returned) {
        acb->common.cb(acb->common.opaque, acb->rwco.ret);
        blk_dec_in_flight(acb->rwco.blk);
        qemu_aio_unref(acb);
    }
}

static void blk_aio_complete_bh(void *opaque)
{
    BlkAioEmAIOCB *acb = opaque;
    assert(acb->has_returned);
    blk_aio_complete(acb);
}

static BlockAIOCB *blk_aio_prwv(BlockBackend *blk, int64_t offset,
                                int64_t bytes,
                                void *iobuf, CoroutineEntry co_entry,
                                BdrvRequestFlags flags,
                                BlockCompletionFunc *cb, void *opaque)
{
    BlkAioEmAIOCB *acb;
    Coroutine *co;

    blk_inc_in_flight(blk);
    acb = blk_aio_get(&blk_aio_em_aiocb_info, blk, cb, opaque);
    acb->rwco = (BlkRwCo) {
        .blk    = blk,
        .offset = offset,
        .iobuf  = iobuf,
        .flags  = flags,
        .ret    = NOT_DONE,
    };
    acb->bytes = bytes;
    acb->has_returned = false;

    co = qemu_coroutine_create(co_entry, acb);
    aio_co_enter(qemu_get_current_aio_context(), co);

    acb->has_returned = true;
    if (acb->rwco.ret != NOT_DONE) {
        aio_bh_schedule_oneshot(qemu_get_current_aio_context(),
                                         blk_aio_complete_bh, acb);
    }

    return &acb->common;
}

static void coroutine_fn blk_aio_read_entry(void *opaque)
{
    BlkAioEmAIOCB *acb = opaque;
    BlkRwCo *rwco = &acb->rwco;
    QEMUIOVector *qiov = rwco->iobuf;

    assert(qiov->size == acb->bytes);
    rwco->ret = blk_co_do_preadv_part(rwco->blk, rwco->offset, acb->bytes, qiov,
                                      0, rwco->flags);
    blk_aio_complete(acb);
}

static void coroutine_fn blk_aio_write_entry(void *opaque)
{
    BlkAioEmAIOCB *acb = opaque;
    BlkRwCo *rwco = &acb->rwco;
    QEMUIOVector *qiov = rwco->iobuf;

    assert(!qiov || qiov->size == acb->bytes);
    rwco->ret = blk_co_do_pwritev_part(rwco->blk, rwco->offset, acb->bytes,
                                       qiov, 0, rwco->flags);
    blk_aio_complete(acb);
}

BlockAIOCB *blk_aio_pwrite_zeroes(BlockBackend *blk, int64_t offset,
                                  int64_t bytes, BdrvRequestFlags flags,
                                  BlockCompletionFunc *cb, void *opaque)
{
    IO_CODE();
    return blk_aio_prwv(blk, offset, bytes, NULL, blk_aio_write_entry,
                        flags | BDRV_REQ_ZERO_WRITE, cb, opaque);
}

int64_t coroutine_fn blk_co_getlength(BlockBackend *blk)
{
    IO_CODE();
    GRAPH_RDLOCK_GUARD();

    if (!blk_co_is_available(blk)) {
        return -ENOMEDIUM;
    }

    return bdrv_co_getlength(blk_bs(blk));
}

int64_t coroutine_fn blk_co_nb_sectors(BlockBackend *blk)
{
    BlockDriverState *bs = blk_bs(blk);

    IO_CODE();
    GRAPH_RDLOCK_GUARD();

    if (!bs) {
        return -ENOMEDIUM;
    } else {
        return bdrv_co_nb_sectors(bs);
    }
}

int64_t coroutine_mixed_fn blk_nb_sectors(BlockBackend *blk)
{
    BlockDriverState *bs = blk_bs(blk);

    IO_CODE();

    if (!bs) {
        return -ENOMEDIUM;
    } else {
        return bdrv_nb_sectors(bs);
    }
}

void coroutine_fn blk_co_get_geometry(BlockBackend *blk,
                                      uint64_t *nb_sectors_ptr)
{
    int64_t ret = blk_co_nb_sectors(blk);
    *nb_sectors_ptr = ret < 0 ? 0 : ret;
}

void coroutine_mixed_fn blk_get_geometry(BlockBackend *blk,
                                         uint64_t *nb_sectors_ptr)
{
    int64_t ret = blk_nb_sectors(blk);
    *nb_sectors_ptr = ret < 0 ? 0 : ret;
}

BlockAIOCB *blk_aio_preadv(BlockBackend *blk, int64_t offset,
                           QEMUIOVector *qiov, BdrvRequestFlags flags,
                           BlockCompletionFunc *cb, void *opaque)
{
    IO_CODE();
    assert((uint64_t)qiov->size <= INT64_MAX);
    return blk_aio_prwv(blk, offset, qiov->size, qiov,
                        blk_aio_read_entry, flags, cb, opaque);
}

BlockAIOCB *blk_aio_pwritev(BlockBackend *blk, int64_t offset,
                            QEMUIOVector *qiov, BdrvRequestFlags flags,
                            BlockCompletionFunc *cb, void *opaque)
{
    IO_CODE();
    assert((uint64_t)qiov->size <= INT64_MAX);
    return blk_aio_prwv(blk, offset, qiov->size, qiov,
                        blk_aio_write_entry, flags, cb, opaque);
}

void blk_aio_cancel(BlockAIOCB *acb)
{
    GLOBAL_STATE_CODE();
    bdrv_aio_cancel(acb);
}

void blk_aio_cancel_async(BlockAIOCB *acb)
{
    IO_CODE();
    bdrv_aio_cancel_async(acb);
}

static int coroutine_fn
blk_co_do_ioctl(BlockBackend *blk, unsigned long int req, void *buf)
{
    IO_CODE();

    blk_wait_while_drained(blk);
    GRAPH_RDLOCK_GUARD();

    if (!blk_co_is_available(blk)) {
        return -ENOMEDIUM;
    }

    return bdrv_co_ioctl(blk_bs(blk), req, buf);
}

int coroutine_fn blk_co_ioctl(BlockBackend *blk, unsigned long int req,
                              void *buf)
{
    int ret;
    IO_OR_GS_CODE();

    blk_inc_in_flight(blk);
    ret = blk_co_do_ioctl(blk, req, buf);
    blk_dec_in_flight(blk);

    return ret;
}

static void coroutine_fn blk_aio_ioctl_entry(void *opaque)
{
    BlkAioEmAIOCB *acb = opaque;
    BlkRwCo *rwco = &acb->rwco;

    rwco->ret = blk_co_do_ioctl(rwco->blk, rwco->offset, rwco->iobuf);

    blk_aio_complete(acb);
}

BlockAIOCB *blk_aio_ioctl(BlockBackend *blk, unsigned long int req, void *buf,
                          BlockCompletionFunc *cb, void *opaque)
{
    IO_CODE();
    return blk_aio_prwv(blk, req, 0, buf, blk_aio_ioctl_entry, 0, cb, opaque);
}

static int coroutine_fn
blk_co_do_pdiscard(BlockBackend *blk, int64_t offset, int64_t bytes)
{
    int ret;
    IO_CODE();

    blk_wait_while_drained(blk);
    GRAPH_RDLOCK_GUARD();

    ret = blk_check_byte_request(blk, offset, bytes);
    if (ret < 0) {
        return ret;
    }

    return bdrv_co_pdiscard(blk->root, offset, bytes);
}

static void coroutine_fn blk_aio_pdiscard_entry(void *opaque)
{
    BlkAioEmAIOCB *acb = opaque;
    BlkRwCo *rwco = &acb->rwco;

    rwco->ret = blk_co_do_pdiscard(rwco->blk, rwco->offset, acb->bytes);
    blk_aio_complete(acb);
}

BlockAIOCB *blk_aio_pdiscard(BlockBackend *blk,
                             int64_t offset, int64_t bytes,
                             BlockCompletionFunc *cb, void *opaque)
{
    IO_CODE();
    return blk_aio_prwv(blk, offset, bytes, NULL, blk_aio_pdiscard_entry, 0,
                        cb, opaque);
}

int coroutine_fn blk_co_pdiscard(BlockBackend *blk, int64_t offset,
                                 int64_t bytes)
{
    int ret;
    IO_OR_GS_CODE();

    blk_inc_in_flight(blk);
    ret = blk_co_do_pdiscard(blk, offset, bytes);
    blk_dec_in_flight(blk);

    return ret;
}

static int coroutine_fn blk_co_do_flush(BlockBackend *blk)
{
    IO_CODE();
    blk_wait_while_drained(blk);
    GRAPH_RDLOCK_GUARD();

    if (!blk_co_is_available(blk)) {
        return -ENOMEDIUM;
    }

    return bdrv_co_flush(blk_bs(blk));
}

static void coroutine_fn blk_aio_flush_entry(void *opaque)
{
    BlkAioEmAIOCB *acb = opaque;
    BlkRwCo *rwco = &acb->rwco;

    rwco->ret = blk_co_do_flush(rwco->blk);
    blk_aio_complete(acb);
}

BlockAIOCB *blk_aio_flush(BlockBackend *blk,
                          BlockCompletionFunc *cb, void *opaque)
{
    IO_CODE();
    return blk_aio_prwv(blk, 0, 0, NULL, blk_aio_flush_entry, 0, cb, opaque);
}

int coroutine_fn blk_co_flush(BlockBackend *blk)
{
    int ret;
    IO_OR_GS_CODE();

    blk_inc_in_flight(blk);
    ret = blk_co_do_flush(blk);
    blk_dec_in_flight(blk);

    return ret;
}

static void coroutine_fn blk_aio_zone_report_entry(void *opaque)
{
    BlkAioEmAIOCB *acb = opaque;
    BlkRwCo *rwco = &acb->rwco;

    rwco->ret = blk_co_zone_report(rwco->blk, rwco->offset,
                                   (unsigned int*)(uintptr_t)acb->bytes,
                                   rwco->iobuf);
    blk_aio_complete(acb);
}

BlockAIOCB *blk_aio_zone_report(BlockBackend *blk, int64_t offset,
                                unsigned int *nr_zones,
                                BlockZoneDescriptor  *zones,
                                BlockCompletionFunc *cb, void *opaque)
{
    BlkAioEmAIOCB *acb;
    Coroutine *co;
    IO_CODE();

    blk_inc_in_flight(blk);
    acb = blk_aio_get(&blk_aio_em_aiocb_info, blk, cb, opaque);
    acb->rwco = (BlkRwCo) {
        .blk    = blk,
        .offset = offset,
        .iobuf  = zones,
        .ret    = NOT_DONE,
    };
    acb->bytes = (int64_t)(uintptr_t)nr_zones,
    acb->has_returned = false;

    co = qemu_coroutine_create(blk_aio_zone_report_entry, acb);
    aio_co_enter(qemu_get_current_aio_context(), co);

    acb->has_returned = true;
    if (acb->rwco.ret != NOT_DONE) {
        aio_bh_schedule_oneshot(qemu_get_current_aio_context(),
                                         blk_aio_complete_bh, acb);
    }

    return &acb->common;
}

static void coroutine_fn blk_aio_zone_mgmt_entry(void *opaque)
{
    BlkAioEmAIOCB *acb = opaque;
    BlkRwCo *rwco = &acb->rwco;

    rwco->ret = blk_co_zone_mgmt(rwco->blk,
                                 (BlockZoneOp)(uintptr_t)rwco->iobuf,
                                 rwco->offset, acb->bytes);
    blk_aio_complete(acb);
}

BlockAIOCB *blk_aio_zone_mgmt(BlockBackend *blk, BlockZoneOp op,
                              int64_t offset, int64_t len,
                              BlockCompletionFunc *cb, void *opaque) {
    BlkAioEmAIOCB *acb;
    Coroutine *co;
    IO_CODE();

    blk_inc_in_flight(blk);
    acb = blk_aio_get(&blk_aio_em_aiocb_info, blk, cb, opaque);
    acb->rwco = (BlkRwCo) {
        .blk    = blk,
        .offset = offset,
        .iobuf  = (void *)(uintptr_t)op,
        .ret    = NOT_DONE,
    };
    acb->bytes = len;
    acb->has_returned = false;

    co = qemu_coroutine_create(blk_aio_zone_mgmt_entry, acb);
    aio_co_enter(qemu_get_current_aio_context(), co);

    acb->has_returned = true;
    if (acb->rwco.ret != NOT_DONE) {
        aio_bh_schedule_oneshot(qemu_get_current_aio_context(),
                                         blk_aio_complete_bh, acb);
    }

    return &acb->common;
}

static void coroutine_fn blk_aio_zone_append_entry(void *opaque)
{
    BlkAioEmAIOCB *acb = opaque;
    BlkRwCo *rwco = &acb->rwco;

    rwco->ret = blk_co_zone_append(rwco->blk, (int64_t *)(uintptr_t)acb->bytes,
                                   rwco->iobuf, rwco->flags);
    blk_aio_complete(acb);
}

BlockAIOCB *blk_aio_zone_append(BlockBackend *blk, int64_t *offset,
                                QEMUIOVector *qiov, BdrvRequestFlags flags,
                                BlockCompletionFunc *cb, void *opaque) {
    BlkAioEmAIOCB *acb;
    Coroutine *co;
    IO_CODE();

    blk_inc_in_flight(blk);
    acb = blk_aio_get(&blk_aio_em_aiocb_info, blk, cb, opaque);
    acb->rwco = (BlkRwCo) {
        .blk    = blk,
        .ret    = NOT_DONE,
        .flags  = flags,
        .iobuf  = qiov,
    };
    acb->bytes = (int64_t)(uintptr_t)offset;
    acb->has_returned = false;

    co = qemu_coroutine_create(blk_aio_zone_append_entry, acb);
    aio_co_enter(qemu_get_current_aio_context(), co);
    acb->has_returned = true;
    if (acb->rwco.ret != NOT_DONE) {
        aio_bh_schedule_oneshot(qemu_get_current_aio_context(),
                                         blk_aio_complete_bh, acb);
    }

    return &acb->common;
}

int coroutine_fn blk_co_zone_report(BlockBackend *blk, int64_t offset,
                                    unsigned int *nr_zones,
                                    BlockZoneDescriptor *zones)
{
    int ret;
    IO_CODE();

    blk_inc_in_flight(blk); /* increase before waiting */
    blk_wait_while_drained(blk);
    GRAPH_RDLOCK_GUARD();
    if (!blk_is_available(blk)) {
        blk_dec_in_flight(blk);
        return -ENOMEDIUM;
    }
    ret = bdrv_co_zone_report(blk_bs(blk), offset, nr_zones, zones);
    blk_dec_in_flight(blk);
    return ret;
}

int coroutine_fn blk_co_zone_mgmt(BlockBackend *blk, BlockZoneOp op,
        int64_t offset, int64_t len)
{
    int ret;
    IO_CODE();

    blk_inc_in_flight(blk);
    blk_wait_while_drained(blk);
    GRAPH_RDLOCK_GUARD();

    ret = blk_check_byte_request(blk, offset, len);
    if (ret < 0) {
        blk_dec_in_flight(blk);
        return ret;
    }

    ret = bdrv_co_zone_mgmt(blk_bs(blk), op, offset, len);
    blk_dec_in_flight(blk);
    return ret;
}

int coroutine_fn blk_co_zone_append(BlockBackend *blk, int64_t *offset,
        QEMUIOVector *qiov, BdrvRequestFlags flags)
{
    int ret;
    IO_CODE();

    blk_inc_in_flight(blk);
    blk_wait_while_drained(blk);
    GRAPH_RDLOCK_GUARD();
    if (!blk_is_available(blk)) {
        blk_dec_in_flight(blk);
        return -ENOMEDIUM;
    }

    ret = bdrv_co_zone_append(blk_bs(blk), offset, qiov, flags);
    blk_dec_in_flight(blk);
    return ret;
}

void blk_drain(BlockBackend *blk)
{
    BlockDriverState *bs = blk_bs(blk);
    GLOBAL_STATE_CODE();

    if (bs) {
        bdrv_ref(bs);
        bdrv_drained_begin(bs);
    }

    AIO_WAIT_WHILE(blk_get_aio_context(blk),
                   qatomic_read(&blk->in_flight) > 0);

    if (bs) {
        bdrv_drained_end(bs);
        bdrv_unref(bs);
    }
}

void blk_drain_all(void)
{
    BlockBackend *blk = NULL;

    GLOBAL_STATE_CODE();

    bdrv_drain_all_begin();

    while ((blk = blk_all_next(blk)) != NULL) {
        AIO_WAIT_WHILE_UNLOCKED(NULL, qatomic_read(&blk->in_flight) > 0);
    }

    bdrv_drain_all_end();
}

void blk_set_on_error(BlockBackend *blk, BlockdevOnError on_read_error,
                      BlockdevOnError on_write_error)
{
    GLOBAL_STATE_CODE();
    blk->on_read_error = on_read_error;
    blk->on_write_error = on_write_error;
}

BlockdevOnError blk_get_on_error(BlockBackend *blk, bool is_read)
{
    IO_CODE();
    return is_read ? blk->on_read_error : blk->on_write_error;
}

BlockErrorAction blk_get_error_action(BlockBackend *blk, bool is_read,
                                      int error)
{
    BlockdevOnError on_err = blk_get_on_error(blk, is_read);
    IO_CODE();

    switch (on_err) {
    case BLOCKDEV_ON_ERROR_ENOSPC:
        return (error == ENOSPC) ?
               BLOCK_ERROR_ACTION_STOP : BLOCK_ERROR_ACTION_REPORT;
    case BLOCKDEV_ON_ERROR_STOP:
        return BLOCK_ERROR_ACTION_STOP;
    case BLOCKDEV_ON_ERROR_REPORT:
        return BLOCK_ERROR_ACTION_REPORT;
    case BLOCKDEV_ON_ERROR_IGNORE:
        return BLOCK_ERROR_ACTION_IGNORE;
    case BLOCKDEV_ON_ERROR_AUTO:
    default:
        abort();
    }
}

static void send_qmp_error_event(BlockBackend *blk,
                                 BlockErrorAction action,
                                 bool is_read, int error)
{
    IoOperationType optype;
    BlockDriverState *bs = blk_bs(blk);
    g_autofree char *path = blk_get_attached_dev_path(blk);

    optype = is_read ? IO_OPERATION_TYPE_READ : IO_OPERATION_TYPE_WRITE;
    qapi_event_send_block_io_error(path, blk_name(blk),
                                   bs ? bdrv_get_node_name(bs) : NULL, optype,
                                   action, blk_iostatus_is_enabled(blk),
                                   error == ENOSPC, strerror(error));
}

void blk_error_action(BlockBackend *blk, BlockErrorAction action,
                      bool is_read, int error)
{
    assert(error >= 0);
    IO_CODE();

    if (action == BLOCK_ERROR_ACTION_STOP) {
        blk_iostatus_set_err(blk, error);

        qemu_system_vmstop_request_prepare();
        send_qmp_error_event(blk, action, is_read, error);
        qemu_system_vmstop_request(RUN_STATE_IO_ERROR);
    } else {
        send_qmp_error_event(blk, action, is_read, error);
    }
}

bool blk_supports_write_perm(BlockBackend *blk)
{
    BlockDriverState *bs = blk_bs(blk);
    GLOBAL_STATE_CODE();

    if (bs) {
        return !bdrv_is_read_only(bs);
    } else {
        return blk->root_state.open_flags & BDRV_O_RDWR;
    }
}

bool blk_is_writable(BlockBackend *blk)
{
    IO_CODE();
    return blk->perm & BLK_PERM_WRITE;
}

bool blk_is_sg(BlockBackend *blk)
{
    BlockDriverState *bs = blk_bs(blk);
    GLOBAL_STATE_CODE();

    if (!bs) {
        return false;
    }

    return bdrv_is_sg(bs);
}

bool blk_enable_write_cache(BlockBackend *blk)
{
    IO_CODE();
    return blk->enable_write_cache;
}

void blk_set_enable_write_cache(BlockBackend *blk, bool wce)
{
    IO_CODE();
    blk->enable_write_cache = wce;
}

bool coroutine_fn blk_co_is_inserted(BlockBackend *blk)
{
    BlockDriverState *bs = blk_bs(blk);
    IO_CODE();
    assert_bdrv_graph_readable();

    return bs && bdrv_co_is_inserted(bs);
}

bool coroutine_fn blk_co_is_available(BlockBackend *blk)
{
    IO_CODE();
    return blk_co_is_inserted(blk) && !blk_dev_is_tray_open(blk);
}

void coroutine_fn blk_co_lock_medium(BlockBackend *blk, bool locked)
{
    BlockDriverState *bs = blk_bs(blk);
    IO_CODE();
    GRAPH_RDLOCK_GUARD();

    if (bs) {
        bdrv_co_lock_medium(bs, locked);
    }
}

void coroutine_fn blk_co_eject(BlockBackend *blk, bool eject_flag)
{
    BlockDriverState *bs = blk_bs(blk);
    char *id;
    IO_CODE();
    GRAPH_RDLOCK_GUARD();

    if (bs) {
        bdrv_co_eject(bs, eject_flag);
    }

    id = blk_get_attached_dev_id(blk);
    qapi_event_send_device_tray_moved(blk_name(blk), id,
                                      eject_flag);
    g_free(id);
}

int blk_get_flags(BlockBackend *blk)
{
    BlockDriverState *bs = blk_bs(blk);
    GLOBAL_STATE_CODE();

    if (bs) {
        return bdrv_get_flags(bs);
    } else {
        return blk->root_state.open_flags;
    }
}

uint32_t blk_get_request_alignment(BlockBackend *blk)
{
    BlockDriverState *bs = blk_bs(blk);
    IO_CODE();
    return bs ? bs->bl.request_alignment : BDRV_SECTOR_SIZE;
}

uint64_t blk_get_max_hw_transfer(BlockBackend *blk)
{
    BlockDriverState *bs = blk_bs(blk);
    uint64_t max = INT_MAX;
    IO_CODE();

    if (bs) {
        max = MIN_NON_ZERO(max, bs->bl.max_hw_transfer);
        max = MIN_NON_ZERO(max, bs->bl.max_transfer);
    }
    return ROUND_DOWN(max, blk_get_request_alignment(blk));
}

uint32_t blk_get_max_transfer(BlockBackend *blk)
{
    BlockDriverState *bs = blk_bs(blk);
    uint32_t max = INT_MAX;
    IO_CODE();

    if (bs) {
        max = MIN_NON_ZERO(max, bs->bl.max_transfer);
    }
    return ROUND_DOWN(max, blk_get_request_alignment(blk));
}

int blk_get_max_hw_iov(BlockBackend *blk)
{
    IO_CODE();
    return MIN_NON_ZERO(blk->root->bs->bl.max_hw_iov,
                        blk->root->bs->bl.max_iov);
}

int blk_get_max_iov(BlockBackend *blk)
{
    IO_CODE();
    return blk->root->bs->bl.max_iov;
}

void *blk_try_blockalign(BlockBackend *blk, size_t size)
{
    IO_CODE();
    return qemu_try_blockalign(blk ? blk_bs(blk) : NULL, size);
}

void *blk_blockalign(BlockBackend *blk, size_t size)
{
    IO_CODE();
    return qemu_blockalign(blk ? blk_bs(blk) : NULL, size);
}


AioContext *blk_get_aio_context(BlockBackend *blk)
{
    IO_CODE();

    if (!blk) {
        return qemu_get_aio_context();
    }

    return qatomic_read(&blk->ctx);
}

int blk_set_aio_context(BlockBackend *blk, AioContext *new_context,
                        Error **errp)
{
    bool old_allow_change;
    BlockDriverState *bs = blk_bs(blk);
    int ret;

    GLOBAL_STATE_CODE();

    if (!bs) {
        qatomic_set(&blk->ctx, new_context);
        return 0;
    }

    bdrv_ref(bs);

    old_allow_change = blk->allow_aio_context_change;
    blk->allow_aio_context_change = true;

    ret = bdrv_try_change_aio_context(bs, new_context, NULL, errp);

    blk->allow_aio_context_change = old_allow_change;

    bdrv_unref(bs);
    return ret;
}

typedef struct BdrvStateBlkRootContext {
    AioContext *new_ctx;
    BlockBackend *blk;
} BdrvStateBlkRootContext;

static void blk_root_set_aio_ctx_commit(void *opaque)
{
    BdrvStateBlkRootContext *s = opaque;
    BlockBackend *blk = s->blk;
    AioContext *new_context = s->new_ctx;

    qatomic_set(&blk->ctx, new_context);
}

static TransactionActionDrv set_blk_root_context = {
    .commit = blk_root_set_aio_ctx_commit,
    .clean = g_free,
};

static bool blk_root_change_aio_ctx(BdrvChild *child, AioContext *ctx,
                                    GHashTable *visited, Transaction *tran,
                                    Error **errp)
{
    BlockBackend *blk = child->opaque;
    BdrvStateBlkRootContext *s;

    if (!blk->allow_aio_context_change) {
        if (!blk->name || blk->dev) {
            error_setg(errp, "Cannot change iothread of active block backend");
            return false;
        }
    }

    s = g_new(BdrvStateBlkRootContext, 1);
    *s = (BdrvStateBlkRootContext) {
        .new_ctx = ctx,
        .blk = blk,
    };

    tran_add(tran, &set_blk_root_context, s);
    return true;
}

void blk_add_aio_context_notifier(BlockBackend *blk,
        void (*attached_aio_context)(AioContext *new_context, void *opaque),
        void (*detach_aio_context)(void *opaque), void *opaque)
{
    BlockBackendAioNotifier *notifier;
    BlockDriverState *bs = blk_bs(blk);
    GLOBAL_STATE_CODE();

    notifier = g_new(BlockBackendAioNotifier, 1);
    notifier->attached_aio_context = attached_aio_context;
    notifier->detach_aio_context = detach_aio_context;
    notifier->opaque = opaque;
    QLIST_INSERT_HEAD(&blk->aio_notifiers, notifier, list);

    if (bs) {
        bdrv_add_aio_context_notifier(bs, attached_aio_context,
                                      detach_aio_context, opaque);
    }
}

void blk_remove_aio_context_notifier(BlockBackend *blk,
                                     void (*attached_aio_context)(AioContext *,
                                                                  void *),
                                     void (*detach_aio_context)(void *),
                                     void *opaque)
{
    BlockBackendAioNotifier *notifier;
    BlockDriverState *bs = blk_bs(blk);

    GLOBAL_STATE_CODE();

    if (bs) {
        bdrv_remove_aio_context_notifier(bs, attached_aio_context,
                                         detach_aio_context, opaque);
    }

    QLIST_FOREACH(notifier, &blk->aio_notifiers, list) {
        if (notifier->attached_aio_context == attached_aio_context &&
            notifier->detach_aio_context == detach_aio_context &&
            notifier->opaque == opaque) {
            QLIST_REMOVE(notifier, list);
            g_free(notifier);
            return;
        }
    }

    abort();
}

void blk_add_remove_bs_notifier(BlockBackend *blk, Notifier *notify)
{
    GLOBAL_STATE_CODE();
    notifier_list_add(&blk->remove_bs_notifiers, notify);
}

BlockAcctStats *blk_get_stats(BlockBackend *blk)
{
    IO_CODE();
    return &blk->stats;
}

void *blk_aio_get(const AIOCBInfo *aiocb_info, BlockBackend *blk,
                  BlockCompletionFunc *cb, void *opaque)
{
    IO_CODE();
    return qemu_aio_get(aiocb_info, blk_bs(blk), cb, opaque);
}

int coroutine_fn blk_co_pwrite_zeroes(BlockBackend *blk, int64_t offset,
                                      int64_t bytes, BdrvRequestFlags flags)
{
    IO_OR_GS_CODE();
    return blk_co_pwritev(blk, offset, bytes, NULL,
                          flags | BDRV_REQ_ZERO_WRITE);
}

int coroutine_fn blk_co_pwrite_compressed(BlockBackend *blk, int64_t offset,
                                          int64_t bytes, const void *buf)
{
    QEMUIOVector qiov = QEMU_IOVEC_INIT_BUF(qiov, buf, bytes);
    IO_OR_GS_CODE();
    return blk_co_pwritev_part(blk, offset, bytes, &qiov, 0,
                               BDRV_REQ_WRITE_COMPRESSED);
}

int coroutine_fn blk_co_truncate(BlockBackend *blk, int64_t offset, bool exact,
                                 PreallocMode prealloc, BdrvRequestFlags flags,
                                 Error **errp)
{
    IO_OR_GS_CODE();
    GRAPH_RDLOCK_GUARD();
    if (!blk_co_is_available(blk)) {
        error_setg(errp, "No medium inserted");
        return -ENOMEDIUM;
    }

    return bdrv_co_truncate(blk->root, offset, exact, prealloc, flags, errp);
}

int blk_save_vmstate(BlockBackend *blk, const uint8_t *buf,
                     int64_t pos, int size)
{
    int ret;
    GLOBAL_STATE_CODE();

    if (!blk_is_available(blk)) {
        return -ENOMEDIUM;
    }

    ret = bdrv_save_vmstate(blk_bs(blk), buf, pos, size);
    if (ret < 0) {
        return ret;
    }

    if (ret == size && !blk->enable_write_cache) {
        ret = bdrv_flush(blk_bs(blk));
    }

    return ret < 0 ? ret : size;
}

int blk_load_vmstate(BlockBackend *blk, uint8_t *buf, int64_t pos, int size)
{
    GLOBAL_STATE_CODE();
    if (!blk_is_available(blk)) {
        return -ENOMEDIUM;
    }

    return bdrv_load_vmstate(blk_bs(blk), buf, pos, size);
}

int blk_probe_blocksizes(BlockBackend *blk, BlockSizes *bsz)
{
    GLOBAL_STATE_CODE();
    GRAPH_RDLOCK_GUARD_MAINLOOP();

    if (!blk_is_available(blk)) {
        return -ENOMEDIUM;
    }

    return bdrv_probe_blocksizes(blk_bs(blk), bsz);
}

int blk_probe_geometry(BlockBackend *blk, HDGeometry *geo)
{
    GLOBAL_STATE_CODE();
    if (!blk_is_available(blk)) {
        return -ENOMEDIUM;
    }

    return bdrv_probe_geometry(blk_bs(blk), geo);
}

void blk_update_root_state(BlockBackend *blk)
{
    GLOBAL_STATE_CODE();
    assert(blk->root);

    blk->root_state.open_flags    = blk->root->bs->open_flags;
    blk->root_state.detect_zeroes = blk->root->bs->detect_zeroes;
}

bool blk_get_detect_zeroes_from_root_state(BlockBackend *blk)
{
    GLOBAL_STATE_CODE();
    return blk->root_state.detect_zeroes;
}

int blk_get_open_flags_from_root_state(BlockBackend *blk)
{
    GLOBAL_STATE_CODE();
    return blk->root_state.open_flags;
}

BlockBackendRootState *blk_get_root_state(BlockBackend *blk)
{
    GLOBAL_STATE_CODE();
    return &blk->root_state;
}

static void blk_root_drained_begin(BdrvChild *child)
{
    BlockBackend *blk = child->opaque;

    if (qatomic_fetch_inc(&blk->quiesce_counter) == 0) {
        if (blk->dev_ops && blk->dev_ops->drained_begin) {
            blk->dev_ops->drained_begin(blk->dev_opaque);
        }
    }

}

static bool blk_root_drained_poll(BdrvChild *child)
{
    BlockBackend *blk = child->opaque;
    bool busy = false;
    assert(qatomic_read(&blk->quiesce_counter));

    if (blk->dev_ops && blk->dev_ops->drained_poll) {
        busy = blk->dev_ops->drained_poll(blk->dev_opaque);
    }
    return busy || !!blk->in_flight;
}

static void blk_root_drained_end(BdrvChild *child)
{
    BlockBackend *blk = child->opaque;
    assert(qatomic_read(&blk->quiesce_counter));

    if (qatomic_fetch_dec(&blk->quiesce_counter) == 1) {
        if (blk->dev_ops && blk->dev_ops->drained_end) {
            blk->dev_ops->drained_end(blk->dev_opaque);
        }
        qemu_mutex_lock(&blk->queued_requests_lock);
        while (qemu_co_enter_next(&blk->queued_requests,
                                  &blk->queued_requests_lock)) {
        }
        qemu_mutex_unlock(&blk->queued_requests_lock);
    }
}

bool blk_register_buf(BlockBackend *blk, void *host, size_t size, Error **errp)
{
    BlockDriverState *bs = blk_bs(blk);

    GLOBAL_STATE_CODE();

    if (bs) {
        return bdrv_register_buf(bs, host, size, errp);
    }
    return true;
}

void blk_unregister_buf(BlockBackend *blk, void *host, size_t size)
{
    BlockDriverState *bs = blk_bs(blk);

    GLOBAL_STATE_CODE();

    if (bs) {
        bdrv_unregister_buf(bs, host, size);
    }
}

int coroutine_fn blk_co_copy_range(BlockBackend *blk_in, int64_t off_in,
                                   BlockBackend *blk_out, int64_t off_out,
                                   int64_t bytes, BdrvRequestFlags read_flags,
                                   BdrvRequestFlags write_flags)
{
    int r;
    IO_CODE();
    GRAPH_RDLOCK_GUARD();

    r = blk_check_byte_request(blk_in, off_in, bytes);
    if (r) {
        return r;
    }
    r = blk_check_byte_request(blk_out, off_out, bytes);
    if (r) {
        return r;
    }

    return bdrv_co_copy_range(blk_in->root, off_in,
                              blk_out->root, off_out,
                              bytes, read_flags, write_flags);
}

const BdrvChild *blk_root(BlockBackend *blk)
{
    GLOBAL_STATE_CODE();
    return blk->root;
}

int blk_make_empty(BlockBackend *blk, Error **errp)
{
    GLOBAL_STATE_CODE();
    GRAPH_RDLOCK_GUARD_MAINLOOP();

    if (!blk_is_available(blk)) {
        error_setg(errp, "No medium inserted");
        return -ENOMEDIUM;
    }

    return bdrv_make_empty(blk->root, errp);
}
