/*
 * QEMU host block devices
 *
 * Copyright (c) 2003-2008 Fabrice Bellard
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * later.  See the COPYING file in the top-level directory.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 * Copyright (c) 2003-2008 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "system/block-backend.h"
#include "system/blockdev.h"
#include "hw/block/block.h"
#include "block/qdict.h"
#include "monitor/monitor.h"
#include "qemu/error-report.h"
#include "qemu/option.h"
#include "qemu/config-file.h"
#include "qapi/qapi-commands-block.h"
#include "qapi/qapi-visit-block-core.h"
#include "qobject/qdict.h"
#include "qobject/qnum.h"
#include "qobject/qstring.h"
#include "qapi/error.h"
#include "qapi/qmp/qerror.h"
#include "qobject/qlist.h"
#include "qapi/qobject-output-visitor.h"
#include "system/system.h"
#include "system/iothread.h"
#include "block/block_int.h"
#include "block/trace.h"
#include "system/runstate.h"
#include "system/replay.h"
#include "qemu/cutils.h"
#include "qemu/main-loop.h"

/* Protected by BQL */
QTAILQ_HEAD(, BlockDriverState) monitor_bdrv_states =
    QTAILQ_HEAD_INITIALIZER(monitor_bdrv_states);

void bdrv_set_monitor_owned(BlockDriverState *bs)
{
    GLOBAL_STATE_CODE();
    QTAILQ_INSERT_TAIL(&monitor_bdrv_states, bs, monitor_list);
}

static const char *const if_name[IF_COUNT] = {
    [IF_NONE] = "none",
    [IF_IDE] = "ide",
    [IF_SCSI] = "scsi",
    [IF_FLOPPY] = "floppy",
    [IF_PFLASH] = "pflash",
    [IF_MTD] = "mtd",
    [IF_SD] = "sd",
    [IF_VIRTIO] = "virtio",
    [IF_XEN] = "xen",
};

static int if_max_devs[IF_COUNT] = {
    /*
     * Do not change these numbers!  They govern how drive option
     * index maps to unit and bus.  That mapping is ABI.
     *
     * All controllers used to implement if=T drives need to support
     * if_max_devs[T] units, for any T with if_max_devs[T] != 0.
     * Otherwise, some index values map to "impossible" bus, unit
     * values.
     *
     * For instance, if you change [IF_SCSI] to 255, -drive
     * if=scsi,index=12 no longer means bus=1,unit=5, but
     * bus=0,unit=12.  With an lsi53c895a controller (7 units max),
     * the drive can't be set up.  Regression.
     */
    [IF_IDE] = 2,
    [IF_SCSI] = 7,
};

/**
 * Boards may call this to offer board-by-board overrides
 * of the default, global values.
 */
void override_max_devs(BlockInterfaceType type, int max_devs)
{
    BlockBackend *blk;
    DriveInfo *dinfo;

    GLOBAL_STATE_CODE();

    if (max_devs <= 0) {
        return;
    }

    for (blk = blk_next(NULL); blk; blk = blk_next(blk)) {
        dinfo = blk_legacy_dinfo(blk);
        if (dinfo->type == type) {
            fprintf(stderr, "Cannot override units-per-bus property of"
                    " the %s interface, because a drive of that type has"
                    " already been added.\n", if_name[type]);
            g_assert_not_reached();
        }
    }

    if_max_devs[type] = max_devs;
}

/*
 * We automatically delete the drive when a device using it gets
 * unplugged.  Questionable feature, but we can't just drop it.
 * Device models call blockdev_mark_auto_del() to schedule the
 * automatic deletion, and generic qdev code calls blockdev_auto_del()
 * when deletion is actually safe.
 */
void blockdev_mark_auto_del(BlockBackend *blk)
{
    DriveInfo *dinfo = blk_legacy_dinfo(blk);

    GLOBAL_STATE_CODE();

    if (!dinfo) {
        return;
    }

    dinfo->auto_del = 1;
}

void blockdev_auto_del(BlockBackend *blk)
{
    DriveInfo *dinfo = blk_legacy_dinfo(blk);
    GLOBAL_STATE_CODE();

    if (dinfo && dinfo->auto_del) {
        monitor_remove_blk(blk);
        blk_unref(blk);
    }
}

static int drive_index_to_bus_id(BlockInterfaceType type, int index)
{
    int max_devs = if_max_devs[type];
    return max_devs ? index / max_devs : 0;
}

static int drive_index_to_unit_id(BlockInterfaceType type, int index)
{
    int max_devs = if_max_devs[type];
    return max_devs ? index % max_devs : index;
}

QemuOpts *drive_add(BlockInterfaceType type, int index, const char *file,
                    const char *optstr)
{
    QemuOpts *opts;

    GLOBAL_STATE_CODE();

    opts = qemu_opts_parse_noisily(qemu_find_opts("drive"), optstr, false);
    if (!opts) {
        return NULL;
    }
    if (type != IF_DEFAULT) {
        qemu_opt_set(opts, "if", if_name[type], &error_abort);
    }
    if (index >= 0) {
        qemu_opt_set_number(opts, "index", index, &error_abort);
    }
    if (file)
        qemu_opt_set(opts, "file", file, &error_abort);
    return opts;
}

DriveInfo *drive_get(BlockInterfaceType type, int bus, int unit)
{
    BlockBackend *blk;
    DriveInfo *dinfo;

    GLOBAL_STATE_CODE();

    for (blk = blk_next(NULL); blk; blk = blk_next(blk)) {
        dinfo = blk_legacy_dinfo(blk);
        if (dinfo && dinfo->type == type
            && dinfo->bus == bus && dinfo->unit == unit) {
            return dinfo;
        }
    }

    return NULL;
}

/*
 * Check board claimed all -drive that are meant to be claimed.
 * Fatal error if any remain unclaimed.
 */
void drive_check_orphaned(void)
{
    BlockBackend *blk;
    DriveInfo *dinfo;
    Location loc;
    bool orphans = false;

    GLOBAL_STATE_CODE();

    for (blk = blk_next(NULL); blk; blk = blk_next(blk)) {
        dinfo = blk_legacy_dinfo(blk);
        /*
         * Ignore default drives, because we create certain default
         * drives unconditionally, then leave them unclaimed.  Not the
         * users fault.
         * Ignore IF_VIRTIO or IF_XEN, because it gets desugared into
         * -device, so we can leave failing to -device.
         * Ignore IF_NONE, because leaving unclaimed IF_NONE remains
         * available for device_add is a feature.
         */
        if (dinfo->is_default || dinfo->type == IF_VIRTIO
            || dinfo->type == IF_XEN || dinfo->type == IF_NONE) {
            continue;
        }
        if (!blk_get_attached_dev(blk)) {
            loc_push_none(&loc);
            qemu_opts_loc_restore(dinfo->opts);
            error_report("machine type does not support"
                         " if=%s,bus=%d,unit=%d",
                         if_name[dinfo->type], dinfo->bus, dinfo->unit);
            loc_pop(&loc);
            orphans = true;
        }
    }

    if (orphans) {
        exit(1);
    }
}

DriveInfo *drive_get_by_index(BlockInterfaceType type, int index)
{
    GLOBAL_STATE_CODE();
    return drive_get(type,
                     drive_index_to_bus_id(type, index),
                     drive_index_to_unit_id(type, index));
}

int drive_get_max_bus(BlockInterfaceType type)
{
    int max_bus;
    BlockBackend *blk;
    DriveInfo *dinfo;

    GLOBAL_STATE_CODE();

    max_bus = -1;
    for (blk = blk_next(NULL); blk; blk = blk_next(blk)) {
        dinfo = blk_legacy_dinfo(blk);
        if (dinfo && dinfo->type == type && dinfo->bus > max_bus) {
            max_bus = dinfo->bus;
        }
    }
    return max_bus;
}

typedef struct {
    QEMUBH *bh;
    BlockDriverState *bs;
} BDRVPutRefBH;

static int parse_block_error_action(const char *buf, bool is_read, Error **errp)
{
    if (!strcmp(buf, "ignore")) {
        return BLOCKDEV_ON_ERROR_IGNORE;
    } else if (!is_read && !strcmp(buf, "enospc")) {
        return BLOCKDEV_ON_ERROR_ENOSPC;
    } else if (!strcmp(buf, "stop")) {
        return BLOCKDEV_ON_ERROR_STOP;
    } else if (!strcmp(buf, "report")) {
        return BLOCKDEV_ON_ERROR_REPORT;
    } else {
        error_setg(errp, "'%s' invalid %s error action",
                   buf, is_read ? "read" : "write");
        return -1;
    }
}

static bool parse_stats_intervals(BlockAcctStats *stats, QList *intervals,
                                  Error **errp)
{
    const QListEntry *entry;
    for (entry = qlist_first(intervals); entry; entry = qlist_next(entry)) {
        switch (qobject_type(entry->value)) {

        case QTYPE_QSTRING: {
            uint64_t length;
            const char *str = qstring_get_str(qobject_to(QString,
                                                         entry->value));
            if (parse_uint_full(str, 10, &length) == 0 &&
                length > 0 && length <= UINT_MAX) {
                block_acct_add_interval(stats, (unsigned) length);
            } else {
                error_setg(errp, "Invalid interval length: %s", str);
                return false;
            }
            break;
        }

        case QTYPE_QNUM: {
            int64_t length = qnum_get_int(qobject_to(QNum, entry->value));

            if (length > 0 && length <= UINT_MAX) {
                block_acct_add_interval(stats, (unsigned) length);
            } else {
                error_setg(errp, "Invalid interval length: %" PRId64, length);
                return false;
            }
            break;
        }

        default:
            error_setg(errp, "The specification of stats-intervals is invalid");
            return false;
        }
    }
    return true;
}

typedef enum { MEDIA_DISK, MEDIA_CDROM } DriveMediaType;

/* All parameters but @opts are optional and may be set to NULL. */
static void extract_common_blockdev_options(QemuOpts *opts, int *bdrv_flags,
    BlockdevDetectZeroesOptions *detect_zeroes, Error **errp)
{
    Error *local_error = NULL;
    const char *aio;

    if (bdrv_flags) {
        if (qemu_opt_get_bool(opts, "copy-on-read", false)) {
            *bdrv_flags |= BDRV_O_COPY_ON_READ;
        }

        if ((aio = qemu_opt_get(opts, "aio")) != NULL) {
            if (bdrv_parse_aio(aio, bdrv_flags) < 0) {
                error_setg(errp, "invalid aio option");
                return;
            }
        }
    }

    if (detect_zeroes) {
        *detect_zeroes =
            qapi_enum_parse(&BlockdevDetectZeroesOptions_lookup,
                            qemu_opt_get(opts, "detect-zeroes"),
                            BLOCKDEV_DETECT_ZEROES_OPTIONS_OFF,
                            &local_error);
        if (local_error) {
            error_propagate(errp, local_error);
            return;
        }
    }
}

static OnOffAuto account_get_opt(QemuOpts *opts, const char *name)
{
    if (!qemu_opt_find(opts, name)) {
        return ON_OFF_AUTO_AUTO;
    }
    if (qemu_opt_get_bool(opts, name, true)) {
        return ON_OFF_AUTO_ON;
    }
    return ON_OFF_AUTO_OFF;
}

/* Takes the ownership of bs_opts */
static BlockBackend *blockdev_init(const char *file, QDict *bs_opts,
                                   Error **errp)
{
    const char *buf;
    int bdrv_flags = 0;
    int on_read_error, on_write_error;
    OnOffAuto account_invalid, account_failed;
    bool writethrough, read_only;
    BlockBackend *blk;
    BlockDriverState *bs;
    Error *error = NULL;
    QemuOpts *opts;
    QDict *interval_dict = NULL;
    QList *interval_list = NULL;
    const char *id;
    BlockdevDetectZeroesOptions detect_zeroes =
        BLOCKDEV_DETECT_ZEROES_OPTIONS_OFF;

    /* Check common options by copying from bs_opts to opts, all other options
     * stay in bs_opts for processing by bdrv_open(). */
    id = qdict_get_try_str(bs_opts, "id");
    opts = qemu_opts_create(&qemu_common_drive_opts, id, 1, errp);
    if (!opts) {
        goto err_no_opts;
    }

    if (!qemu_opts_absorb_qdict(opts, bs_opts, errp)) {
        goto early_err;
    }

    if (id) {
        qdict_del(bs_opts, "id");
    }

    /* extract parameters */
    account_invalid = account_get_opt(opts, "stats-account-invalid");
    account_failed = account_get_opt(opts, "stats-account-failed");

    writethrough = !qemu_opt_get_bool(opts, BDRV_OPT_CACHE_WB, true);

    id = qemu_opts_id(opts);

    qdict_extract_subqdict(bs_opts, &interval_dict, "stats-intervals.");
    qdict_array_split(interval_dict, &interval_list);

    if (qdict_size(interval_dict) != 0) {
        error_setg(errp, "Invalid option stats-intervals.%s",
                   qdict_first(interval_dict)->key);
        goto early_err;
    }

    extract_common_blockdev_options(opts, &bdrv_flags, &detect_zeroes, &error);
    if (error) {
        error_propagate(errp, error);
        goto early_err;
    }

    if ((buf = qemu_opt_get(opts, "format")) != NULL) {
        if (qdict_haskey(bs_opts, "driver")) {
            error_setg(errp, "Cannot specify both 'driver' and 'format'");
            goto early_err;
        }
        qdict_put_str(bs_opts, "driver", buf);
    }

    on_write_error = BLOCKDEV_ON_ERROR_ENOSPC;
    if ((buf = qemu_opt_get(opts, "werror")) != NULL) {
        on_write_error = parse_block_error_action(buf, 0, &error);
        if (error) {
            error_propagate(errp, error);
            goto early_err;
        }
    }

    on_read_error = BLOCKDEV_ON_ERROR_REPORT;
    if ((buf = qemu_opt_get(opts, "rerror")) != NULL) {
        on_read_error = parse_block_error_action(buf, 1, &error);
        if (error) {
            error_propagate(errp, error);
            goto early_err;
        }
    }

    read_only = qemu_opt_get_bool(opts, BDRV_OPT_READ_ONLY, false);

    /* init */
    if ((!file || !*file) && !qdict_size(bs_opts)) {
        BlockBackendRootState *blk_rs;

        blk = blk_new(qemu_get_aio_context(), 0, BLK_PERM_ALL);
        blk_rs = blk_get_root_state(blk);
        blk_rs->open_flags    = bdrv_flags | (read_only ? 0 : BDRV_O_RDWR);
        blk_rs->detect_zeroes = detect_zeroes;

        qobject_unref(bs_opts);
    } else {
        if (file && !*file) {
            file = NULL;
        }

        /* bdrv_open() defaults to the values in bdrv_flags (for compatibility
         * with other callers) rather than what we want as the real defaults.
         * Apply the defaults here instead. */
        qdict_set_default_str(bs_opts, BDRV_OPT_CACHE_DIRECT, "off");
        qdict_set_default_str(bs_opts, BDRV_OPT_CACHE_NO_FLUSH, "off");
        qdict_set_default_str(bs_opts, BDRV_OPT_READ_ONLY,
                              read_only ? "on" : "off");
        qdict_set_default_str(bs_opts, BDRV_OPT_AUTO_READ_ONLY, "on");
        assert((bdrv_flags & BDRV_O_CACHE_MASK) == 0);

        if (runstate_check(RUN_STATE_INMIGRATE)) {
            bdrv_flags |= BDRV_O_INACTIVE;
        }

        blk = blk_new_open(file, NULL, bs_opts, bdrv_flags, errp);
        if (!blk) {
            goto err_no_bs_opts;
        }
        bs = blk_bs(blk);

        bs->detect_zeroes = detect_zeroes;

        block_acct_setup(blk_get_stats(blk), account_invalid, account_failed);

        if (!parse_stats_intervals(blk_get_stats(blk), interval_list, errp)) {
            blk_unref(blk);
            blk = NULL;
            goto err_no_bs_opts;
        }
    }

    blk_set_enable_write_cache(blk, !writethrough);
    blk_set_on_error(blk, on_read_error, on_write_error);

    if (!monitor_add_blk(blk, id, errp)) {
        blk_unref(blk);
        blk = NULL;
        goto err_no_bs_opts;
    }

err_no_bs_opts:
    qemu_opts_del(opts);
    qobject_unref(interval_dict);
    qobject_unref(interval_list);
    return blk;

early_err:
    qemu_opts_del(opts);
    qobject_unref(interval_dict);
    qobject_unref(interval_list);
err_no_opts:
    qobject_unref(bs_opts);
    return NULL;
}

/* Takes the ownership of bs_opts */
BlockDriverState *bds_tree_init(QDict *bs_opts, Error **errp)
{
    int bdrv_flags = 0;

    GLOBAL_STATE_CODE();
    /* bdrv_open() defaults to the values in bdrv_flags (for compatibility
     * with other callers) rather than what we want as the real defaults.
     * Apply the defaults here instead. */
    qdict_set_default_str(bs_opts, BDRV_OPT_CACHE_DIRECT, "off");
    qdict_set_default_str(bs_opts, BDRV_OPT_CACHE_NO_FLUSH, "off");
    qdict_set_default_str(bs_opts, BDRV_OPT_READ_ONLY, "off");

    if (runstate_check(RUN_STATE_INMIGRATE)) {
        bdrv_flags |= BDRV_O_INACTIVE;
    }

    return bdrv_open(NULL, NULL, bs_opts, bdrv_flags, errp);
}

void blockdev_close_all_bdrv_states(void)
{
    BlockDriverState *bs, *next_bs;

    GLOBAL_STATE_CODE();
    QTAILQ_FOREACH_SAFE(bs, &monitor_bdrv_states, monitor_list, next_bs) {
        bdrv_unref(bs);
    }
}

/* Iterates over the list of monitor-owned BlockDriverStates */
BlockDriverState *bdrv_next_monitor_owned(BlockDriverState *bs)
{
    GLOBAL_STATE_CODE();
    return bs ? QTAILQ_NEXT(bs, monitor_list)
              : QTAILQ_FIRST(&monitor_bdrv_states);
}

static bool qemu_opt_rename(QemuOpts *opts, const char *from, const char *to,
                            Error **errp)
{
    const char *value;

    value = qemu_opt_get(opts, from);
    if (value) {
        if (qemu_opt_find(opts, to)) {
            error_setg(errp, "'%s' and its alias '%s' can't be used at the "
                       "same time", to, from);
            return false;
        }
    }

    /* rename all items in opts */
    while ((value = qemu_opt_get(opts, from))) {
        qemu_opt_set(opts, to, value, &error_abort);
        qemu_opt_unset(opts, from);
    }
    return true;
}

QemuOptsList qemu_legacy_drive_opts = {
    .name = "drive",
    .head = QTAILQ_HEAD_INITIALIZER(qemu_legacy_drive_opts.head),
    .desc = {
        {
            .name = "bus",
            .type = QEMU_OPT_NUMBER,
            .help = "bus number",
        },{
            .name = "unit",
            .type = QEMU_OPT_NUMBER,
            .help = "unit number (i.e. lun for scsi)",
        },{
            .name = "index",
            .type = QEMU_OPT_NUMBER,
            .help = "index number",
        },{
            .name = "media",
            .type = QEMU_OPT_STRING,
            .help = "media type (disk, cdrom)",
        },{
            .name = "if",
            .type = QEMU_OPT_STRING,
            .help = "interface (ide, scsi, sd, mtd, floppy, pflash, virtio)",
        },{
            .name = "file",
            .type = QEMU_OPT_STRING,
            .help = "file name",
        },

        /* Options that are passed on, but have special semantics with -drive */
        {
            .name = BDRV_OPT_READ_ONLY,
            .type = QEMU_OPT_BOOL,
            .help = "open drive file as read-only",
        },{
            .name = "rerror",
            .type = QEMU_OPT_STRING,
            .help = "read error action",
        },{
            .name = "werror",
            .type = QEMU_OPT_STRING,
            .help = "write error action",
        },{
            .name = "copy-on-read",
            .type = QEMU_OPT_BOOL,
            .help = "copy read data from backing file into image file",
        },

        { /* end of list */ }
    },
};

DriveInfo *drive_new(QemuOpts *all_opts, BlockInterfaceType block_default_type,
                     Error **errp)
{
    const char *value;
    BlockBackend *blk;
    DriveInfo *dinfo = NULL;
    QDict *bs_opts;
    QemuOpts *legacy_opts;
    DriveMediaType media = MEDIA_DISK;
    BlockInterfaceType type;
    int max_devs, bus_id, unit_id, index;
    const char *werror, *rerror;
    bool read_only = false;
    bool copy_on_read;
    const char *filename;
    int i;

    GLOBAL_STATE_CODE();

    /* Change legacy command line options into QMP ones */
    static const struct {
        const char *from;
        const char *to;
    } opt_renames[] = {
        { "readonly",       BDRV_OPT_READ_ONLY },
    };

    for (i = 0; i < ARRAY_SIZE(opt_renames); i++) {
        if (!qemu_opt_rename(all_opts, opt_renames[i].from,
                             opt_renames[i].to, errp)) {
            return NULL;
        }
    }

    value = qemu_opt_get(all_opts, "cache");
    if (value) {
        int flags = 0;
        bool writethrough;

        if (bdrv_parse_cache_mode(value, &flags, &writethrough) != 0) {
            error_setg(errp, "invalid cache option");
            return NULL;
        }

        /* Specific options take precedence */
        if (!qemu_opt_get(all_opts, BDRV_OPT_CACHE_WB)) {
            qemu_opt_set_bool(all_opts, BDRV_OPT_CACHE_WB,
                              !writethrough, &error_abort);
        }
        if (!qemu_opt_get(all_opts, BDRV_OPT_CACHE_DIRECT)) {
            qemu_opt_set_bool(all_opts, BDRV_OPT_CACHE_DIRECT,
                              !!(flags & BDRV_O_NOCACHE), &error_abort);
        }
        if (!qemu_opt_get(all_opts, BDRV_OPT_CACHE_NO_FLUSH)) {
            qemu_opt_set_bool(all_opts, BDRV_OPT_CACHE_NO_FLUSH,
                              !!(flags & BDRV_O_NO_FLUSH), &error_abort);
        }
        qemu_opt_unset(all_opts, "cache");
    }

    /* Get a QDict for processing the options */
    bs_opts = qdict_new();
    qemu_opts_to_qdict(all_opts, bs_opts);

    legacy_opts = qemu_opts_create(&qemu_legacy_drive_opts, NULL, 0,
                                   &error_abort);
    if (!qemu_opts_absorb_qdict(legacy_opts, bs_opts, errp)) {
        goto fail;
    }

    /* Media type */
    value = qemu_opt_get(legacy_opts, "media");
    if (value) {
        if (!strcmp(value, "disk")) {
            media = MEDIA_DISK;
        } else if (!strcmp(value, "cdrom")) {
            media = MEDIA_CDROM;
            read_only = true;
        } else {
            error_setg(errp, "'%s' invalid media", value);
            goto fail;
        }
    }

    /* copy-on-read is disabled with a warning for read-only devices */
    read_only |= qemu_opt_get_bool(legacy_opts, BDRV_OPT_READ_ONLY, false);
    copy_on_read = qemu_opt_get_bool(legacy_opts, "copy-on-read", false);

    if (read_only && copy_on_read) {
        warn_report("disabling copy-on-read on read-only drive");
        copy_on_read = false;
    }

    qdict_put_str(bs_opts, BDRV_OPT_READ_ONLY, read_only ? "on" : "off");
    qdict_put_str(bs_opts, "copy-on-read", copy_on_read ? "on" : "off");

    /* Controller type */
    value = qemu_opt_get(legacy_opts, "if");
    if (value) {
        for (type = 0;
             type < IF_COUNT && strcmp(value, if_name[type]);
             type++) {
        }
        if (type == IF_COUNT) {
            error_setg(errp, "unsupported bus type '%s'", value);
            goto fail;
        }
    } else {
        type = block_default_type;
    }

    /* Device address specified by bus/unit or index.
     * If none was specified, try to find the first free one. */
    bus_id  = qemu_opt_get_number(legacy_opts, "bus", 0);
    unit_id = qemu_opt_get_number(legacy_opts, "unit", -1);
    index   = qemu_opt_get_number(legacy_opts, "index", -1);

    max_devs = if_max_devs[type];

    if (index != -1) {
        if (bus_id != 0 || unit_id != -1) {
            error_setg(errp, "index cannot be used with bus and unit");
            goto fail;
        }
        bus_id = drive_index_to_bus_id(type, index);
        unit_id = drive_index_to_unit_id(type, index);
    }

    if (unit_id == -1) {
       unit_id = 0;
       while (drive_get(type, bus_id, unit_id) != NULL) {
           unit_id++;
           if (max_devs && unit_id >= max_devs) {
               unit_id -= max_devs;
               bus_id++;
           }
       }
    }

    if (max_devs && unit_id >= max_devs) {
        error_setg(errp, "unit %d too big (max is %d)", unit_id, max_devs - 1);
        goto fail;
    }

    if (drive_get(type, bus_id, unit_id) != NULL) {
        error_setg(errp, "drive with bus=%d, unit=%d (index=%d) exists",
                   bus_id, unit_id, index);
        goto fail;
    }

    /* no id supplied -> create one */
    if (qemu_opts_id(all_opts) == NULL) {
        char *new_id;
        const char *mediastr = "";
        if (type == IF_IDE || type == IF_SCSI) {
            mediastr = (media == MEDIA_CDROM) ? "-cd" : "-hd";
        }
        if (max_devs) {
            new_id = g_strdup_printf("%s%i%s%i", if_name[type], bus_id,
                                     mediastr, unit_id);
        } else {
            new_id = g_strdup_printf("%s%s%i", if_name[type],
                                     mediastr, unit_id);
        }
        qdict_put_str(bs_opts, "id", new_id);
        g_free(new_id);
    }

    /* Add virtio block device */
    if (type == IF_VIRTIO) {
        QemuOpts *devopts;
        devopts = qemu_opts_create(qemu_find_opts("device"), NULL, 0,
                                   &error_abort);
        qemu_opt_set(devopts, "driver", "virtio-blk", &error_abort);
        qemu_opt_set(devopts, "drive", qdict_get_str(bs_opts, "id"),
                     &error_abort);
    } else if (type == IF_XEN) {
        QemuOpts *devopts;
        devopts = qemu_opts_create(qemu_find_opts("device"), NULL, 0,
                                   &error_abort);
        qemu_opt_set(devopts, "driver",
                     (media == MEDIA_CDROM) ? "xen-cdrom" : "xen-disk",
                     &error_abort);
        qemu_opt_set(devopts, "drive", qdict_get_str(bs_opts, "id"),
                     &error_abort);
    }

    filename = qemu_opt_get(legacy_opts, "file");

    /* Check werror/rerror compatibility with if=... */
    werror = qemu_opt_get(legacy_opts, "werror");
    if (werror != NULL) {
        if (type != IF_IDE && type != IF_SCSI && type != IF_VIRTIO &&
            type != IF_NONE) {
            error_setg(errp, "werror is not supported by this bus type");
            goto fail;
        }
        qdict_put_str(bs_opts, "werror", werror);
    }

    rerror = qemu_opt_get(legacy_opts, "rerror");
    if (rerror != NULL) {
        if (type != IF_IDE && type != IF_VIRTIO && type != IF_SCSI &&
            type != IF_NONE) {
            error_setg(errp, "rerror is not supported by this bus type");
            goto fail;
        }
        qdict_put_str(bs_opts, "rerror", rerror);
    }

    /* Actual block device init: Functionality shared with blockdev-add */
    blk = blockdev_init(filename, bs_opts, errp);
    bs_opts = NULL;
    if (!blk) {
        goto fail;
    }

    /* Create legacy DriveInfo */
    dinfo = g_malloc0(sizeof(*dinfo));
    dinfo->opts = all_opts;

    dinfo->type = type;
    dinfo->bus = bus_id;
    dinfo->unit = unit_id;

    blk_set_legacy_dinfo(blk, dinfo);

    switch(type) {
    case IF_IDE:
    case IF_SCSI:
    case IF_XEN:
    case IF_NONE:
        dinfo->media_cd = media == MEDIA_CDROM;
        break;
    default:
        break;
    }

fail:
    qemu_opts_del(legacy_opts);
    qobject_unref(bs_opts);
    return dinfo;
}

static BlockDriverState *qmp_get_root_bs(const char *name, Error **errp)
{
    BlockDriverState *bs;

    GRAPH_RDLOCK_GUARD_MAINLOOP();

    bs = bdrv_lookup_bs(name, name, errp);
    if (bs == NULL) {
        return NULL;
    }

    if (!bdrv_is_root_node(bs)) {
        error_setg(errp, "Need a root block node");
        return NULL;
    }

    if (!bdrv_is_inserted(bs)) {
        error_setg(errp, "Device has no medium");
        bs = NULL;
    }

    return bs;
}

void qmp_blockdev_add(BlockdevOptions *options, Error **errp)
{
    BlockDriverState *bs;
    QObject *obj;
    Visitor *v = qobject_output_visitor_new(&obj);
    QDict *qdict;

    visit_type_BlockdevOptions(v, NULL, &options, &error_abort);
    visit_complete(v, &obj);
    qdict = qobject_to(QDict, obj);

    qdict_flatten(qdict);

    if (!qdict_get_try_str(qdict, "node-name")) {
        error_setg(errp, "'node-name' must be specified for the root node");
        goto fail;
    }

    bs = bds_tree_init(qdict, errp);
    if (!bs) {
        goto fail;
    }

    bdrv_set_monitor_owned(bs);

fail:
    visit_free(v);
}

QemuOptsList qemu_common_drive_opts = {
    .name = "drive",
    .head = QTAILQ_HEAD_INITIALIZER(qemu_common_drive_opts.head),
    .desc = {
        {
            .name = "aio",
            .type = QEMU_OPT_STRING,
            .help = "host AIO implementation (threads, native, io_uring)",
        },{
            .name = BDRV_OPT_CACHE_WB,
            .type = QEMU_OPT_BOOL,
            .help = "Enable writeback mode",
        },{
            .name = "format",
            .type = QEMU_OPT_STRING,
            .help = "disk format (raw, qcow2, ...)",
        },{
            .name = "rerror",
            .type = QEMU_OPT_STRING,
            .help = "read error action",
        },{
            .name = "werror",
            .type = QEMU_OPT_STRING,
            .help = "write error action",
        },{
            .name = BDRV_OPT_READ_ONLY,
            .type = QEMU_OPT_BOOL,
            .help = "open drive file as read-only",
        },

        {
            .name = "copy-on-read",
            .type = QEMU_OPT_BOOL,
            .help = "copy read data from backing file into image file",
        },{
            .name = "detect-zeroes",
            .type = QEMU_OPT_STRING,
            .help = "try to optimize zero writes (off, on, unmap)",
        },{
            .name = "stats-account-invalid",
            .type = QEMU_OPT_BOOL,
            .help = "whether to account for invalid I/O operations "
                    "in the statistics",
        },{
            .name = "stats-account-failed",
            .type = QEMU_OPT_BOOL,
            .help = "whether to account for failed I/O operations "
                    "in the statistics",
        },
        { /* end of list */ }
    },
};

QemuOptsList qemu_drive_opts = {
    .name = "drive",
    .head = QTAILQ_HEAD_INITIALIZER(qemu_drive_opts.head),
    .desc = {
        /*
         * no elements => accept any params
         * validation will happen later
         */
        { /* end of list */ }
    },
};
