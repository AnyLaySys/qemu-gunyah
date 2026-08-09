
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/cutils.h"
#include "qemu/error-report.h"
#include "block/block-io.h"
#include "block/block_int.h"
#include "qemu/module.h"
#include "qemu/option.h"
#include "qemu/units.h"
#include "qemu/memalign.h"
#include "trace.h"
#include "block/thread-pool.h"
#include "qemu/iov.h"
#include "block/raw-aio.h"
#include "qobject/qdict.h"
#include "qobject/qstring.h"

#if defined(__APPLE__) && (__MACH__)
#include <sys/ioctl.h>
#if defined(HAVE_HOST_BLOCK_DEVICE)
#include <paths.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOMediaBSDClient.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IODVDMedia.h>
#include <CoreFoundation/CoreFoundation.h>
#endif /* defined(HAVE_HOST_BLOCK_DEVICE) */
#endif

#ifdef __sun__
#define _POSIX_PTHREAD_SEMANTICS 1
#include <sys/dkio.h>
#endif
#ifdef __linux__
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/syscall.h>
#include <sys/vfs.h>
#if defined(CONFIG_BLKZONED)
#include <linux/blkzoned.h>
#endif
#include <linux/cdrom.h>
#include <linux/fd.h>
#include <linux/fs.h>
#include <linux/hdreg.h>
#include <linux/magic.h>
#include <scsi/sg.h>
#ifdef __s390__
#include <asm/dasd.h>
#endif
#ifndef FS_NOCOW_FL
#define FS_NOCOW_FL                     0x00800000 /* Do not cow file */
#endif
#endif
#if defined(CONFIG_FALLOCATE_PUNCH_HOLE) || defined(CONFIG_FALLOCATE_ZERO_RANGE)
#include <linux/falloc.h>
#endif
#if defined (__FreeBSD__) || defined(__FreeBSD_kernel__)
#include <sys/disk.h>
#include <sys/cdio.h>
#endif

#ifdef __OpenBSD__
#include <sys/ioctl.h>
#include <sys/disklabel.h>
#include <sys/dkio.h>
#endif

#ifdef __NetBSD__
#include <sys/ioctl.h>
#include <sys/disklabel.h>
#include <sys/dkio.h>
#include <sys/disk.h>
#endif

#ifdef __DragonFly__
#include <sys/ioctl.h>
#include <sys/diskslice.h>
#endif

#ifndef O_DSYNC
#ifdef O_SYNC
#define O_DSYNC O_SYNC
#elif defined(O_FSYNC)
#define O_DSYNC O_FSYNC
#endif
#endif

#ifndef O_DIRECT
#define O_DIRECT O_DSYNC
#endif

#define FTYPE_FILE   0
#define FTYPE_CD     1

#define MAX_BLOCKSIZE	4096

#define RAW_LOCK_PERM_BASE             100
#define RAW_LOCK_SHARED_BASE           200

typedef struct BDRVRawState {
    int fd;
    bool use_lock;
    int type;
    int open_flags;
    size_t buf_align;

    uint64_t perm;
    uint64_t shared_perm;

    uint64_t locked_perm;
    uint64_t locked_shared_perm;

    int perm_change_fd;
    int perm_change_flags;
    BDRVReopenState *reopen_state;

    bool has_discard:1;
    bool has_write_zeroes:1;
    bool use_linux_io_uring:1;
    int page_cache_inconsistent; /* errno from fdatasync failure */
    bool has_fallocate;
    bool needs_alignment;
    bool force_alignment;
    bool drop_cache;
    bool check_cache_dropped;
    struct {
        uint64_t discard_nb_ok;
        uint64_t discard_nb_failed;
        uint64_t discard_bytes_ok;
    } stats;
} BDRVRawState;

typedef struct BDRVRawReopenState {
    int open_flags;
    bool drop_cache;
    bool check_cache_dropped;
} BDRVRawReopenState;

static int fd_open(BlockDriverState *bs)
{
    BDRVRawState *s = bs->opaque;

    if (s->fd >= 0) {
        return 0;
    }
    return -EIO;
}

static int64_t raw_getlength(BlockDriverState *bs);
static int coroutine_fn raw_co_flush_to_disk(BlockDriverState *bs);

typedef struct RawPosixAIOData {
    BlockDriverState *bs;
    int aio_type;
    int aio_fildes;

    off_t aio_offset;
    uint64_t aio_nbytes;

    union {
        struct {
            struct iovec *iov;
            int niov;
        } io;
        struct {
            uint64_t cmd;
            void *buf;
        } ioctl;
        struct {
            int aio_fd2;
            off_t aio_offset2;
        } copy_range;
        struct {
            PreallocMode prealloc;
            Error **errp;
        } truncate;
        struct {
            unsigned int *nr_zones;
            BlockZoneDescriptor *zones;
        } zone_report;
        struct {
            unsigned long op;
        } zone_mgmt;
    };
} RawPosixAIOData;

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
static int cdrom_reopen(BlockDriverState *bs);
#endif

#define raw_lock_error_setg_errno(errp, err, fmt, ...)                  \
    do {                                                                \
        if ((err) == EAGAIN || (err) == EACCES) {                       \
            error_setg((errp), (fmt), ## __VA_ARGS__);                  \
        } else {                                                        \
            error_setg_errno((errp), (err), (fmt), ## __VA_ARGS__);     \
        }                                                               \
    } while (0)

#if defined(__NetBSD__)
static int raw_normalize_devicepath(const char **filename, Error **errp)
{
    static char namebuf[PATH_MAX];
    const char *dp, *fname;
    struct stat sb;

    fname = *filename;
    dp = strrchr(fname, '/');
    if (lstat(fname, &sb) < 0) {
        error_setg_file_open(errp, errno, fname);
        return -errno;
    }

    if (!S_ISBLK(sb.st_mode)) {
        return 0;
    }

    if (dp == NULL) {
        snprintf(namebuf, PATH_MAX, "r%s", fname);
    } else {
        snprintf(namebuf, PATH_MAX, "%.*s/r%s",
            (int)(dp - fname), fname, dp + 1);
    }
    *filename = namebuf;
    warn_report("%s is a block device, using %s", fname, *filename);

    return 0;
}
#else
static int raw_normalize_devicepath(const char **filename, Error **errp)
{
    return 0;
}
#endif

static int probe_logical_blocksize(int fd, unsigned int *sector_size_p)
{
    unsigned int sector_size;
    bool success = false;
    int i;

    errno = ENOTSUP;
    static const unsigned long ioctl_list[] = {
#ifdef BLKSSZGET
        BLKSSZGET,
#endif
#ifdef DKIOCGETBLOCKSIZE
        DKIOCGETBLOCKSIZE,
#endif
#ifdef DIOCGSECTORSIZE
        DIOCGSECTORSIZE,
#endif
    };

    for (i = 0; i < (int)ARRAY_SIZE(ioctl_list); i++) {
        if (ioctl(fd, ioctl_list[i], &sector_size) >= 0) {
            *sector_size_p = sector_size;
            success = true;
        }
    }

    return success ? 0 : -errno;
}

static int probe_physical_blocksize(int fd, unsigned int *blk_size)
{
#ifdef BLKPBSZGET
    if (ioctl(fd, BLKPBSZGET, blk_size) < 0) {
        return -errno;
    }
    return 0;
#else
    return -ENOTSUP;
#endif
}

static bool dio_byte_aligned(int fd)
{
#ifdef __linux__
    struct statfs buf;
    int ret;

    ret = fstatfs(fd, &buf);
    if (ret == 0 && buf.f_type == NFS_SUPER_MAGIC) {
        return true;
    }
#endif
    return false;
}

static bool raw_needs_alignment(BlockDriverState *bs)
{
    BDRVRawState *s = bs->opaque;

    if ((bs->open_flags & BDRV_O_NOCACHE) != 0 && !dio_byte_aligned(s->fd)) {
        return true;
    }

    return s->force_alignment;
}

static bool raw_is_io_aligned(int fd, void *buf, size_t len)
{
    ssize_t ret = pread(fd, buf, len, 0);

    if (ret >= 0) {
        return true;
    }

#ifdef __linux__
    if (errno != EINVAL) {
        return true;
    }
#endif

    return false;
}

static void raw_probe_alignment(BlockDriverState *bs, int fd, Error **errp)
{
    BDRVRawState *s = bs->opaque;
    char *buf;
    size_t max_align = MAX(MAX_BLOCKSIZE, qemu_real_host_page_size());
    size_t alignments[] = {1, 512, 1024, 2048, 4096};

    if (bdrv_is_sg(bs) || !s->needs_alignment) {
        bs->bl.request_alignment = 1;
        s->buf_align = 1;
        return;
    }

    bs->bl.request_alignment = 0;
    s->buf_align = 0;
    if (probe_logical_blocksize(fd, &bs->bl.request_alignment) < 0) {
        bs->bl.request_alignment = 0;
    }

#ifdef __linux__
    struct xfs_dioattr {
        uint32_t d_mem;
        uint32_t d_miniosz;
        uint32_t d_maxiosz;
    } da;
    if (ioctl(fd, _IOR('X', 30, struct xfs_dioattr), &da) >= 0) {
        bs->bl.request_alignment = da.d_miniosz;
    }
#endif


    if (!bs->bl.request_alignment) {
        int i;
        size_t align;
        buf = qemu_memalign(max_align, max_align);
        for (i = 0; i < ARRAY_SIZE(alignments); i++) {
            align = alignments[i];
            if (raw_is_io_aligned(fd, buf, align)) {
                bs->bl.request_alignment = (align != 1) ? align : max_align;
                break;
            }
        }
        qemu_vfree(buf);
    }

    if (!s->buf_align) {
        int i;
        size_t align;
        buf = qemu_memalign(max_align, 2 * max_align);
        for (i = 0; i < ARRAY_SIZE(alignments); i++) {
            align = alignments[i];
            if (raw_is_io_aligned(fd, buf + align, max_align)) {
                s->buf_align = (align != 1) ? align : bs->bl.request_alignment;
                break;
            }
        }
        qemu_vfree(buf);
    }

    if (!s->buf_align || !bs->bl.request_alignment) {
        error_setg(errp, "Could not find working O_DIRECT alignment");
        error_append_hint(errp, "Try cache.direct=off\n");
    }
}

static int check_hdev_writable(int fd)
{
#if defined(BLKROGET)
    struct stat st;
    int readonly = 0;

    if (fstat(fd, &st)) {
        return -errno;
    }

    if (!S_ISBLK(st.st_mode)) {
        return 0;
    }

    if (ioctl(fd, BLKROGET, &readonly) < 0) {
        return -errno;
    }

    if (readonly) {
        return -EACCES;
    }
#endif /* defined(BLKROGET) */
    return 0;
}

static void raw_parse_flags(int bdrv_flags, int *open_flags, bool has_writers)
{
    bool read_write = false;
    assert(open_flags != NULL);

    *open_flags |= O_BINARY;
    *open_flags &= ~O_ACCMODE;

    if (bdrv_flags & BDRV_O_AUTO_RDONLY) {
        read_write = has_writers;
    } else if (bdrv_flags & BDRV_O_RDWR) {
        read_write = true;
    }

    if (read_write) {
        *open_flags |= O_RDWR;
    } else {
        *open_flags |= O_RDONLY;
    }

    if ((bdrv_flags & BDRV_O_NOCACHE)) {
        *open_flags |= O_DIRECT;
    }
}

static void raw_parse_filename(const char *filename, QDict *options,
                               Error **errp)
{
    bdrv_parse_filename_strip_prefix(filename, "file:", options);
}

static QemuOptsList raw_runtime_opts = {
    .name = "raw",
    .head = QTAILQ_HEAD_INITIALIZER(raw_runtime_opts.head),
    .desc = {
        {
            .name = "filename",
            .type = QEMU_OPT_STRING,
            .help = "File name of the image",
        },
        {
            .name = "aio",
            .type = QEMU_OPT_STRING,
            .help = "host AIO implementation (io_uring)",
        },
        {
            .name = "locking",
            .type = QEMU_OPT_STRING,
            .help = "file locking mode (on/off/auto, default: auto)",
        },
#if defined(__linux__)
        {
            .name = "drop-cache",
            .type = QEMU_OPT_BOOL,
            .help = "invalidate page cache during live migration (default: on)",
        },
#endif
        {
            .name = "x-check-cache-dropped",
            .type = QEMU_OPT_BOOL,
            .help = "check that page cache was dropped on live migration (default: off)"
        },
        { /* end of list */ }
    },
};

static const char *const mutable_opts[] = { "x-check-cache-dropped", NULL };

static int raw_open_common(BlockDriverState *bs, QDict *options,
                           int bdrv_flags, int open_flags,
                           bool device, Error **errp)
{
    BDRVRawState *s = bs->opaque;
    QemuOpts *opts;
    Error *local_err = NULL;
    const char *filename = NULL;
    BlockdevAioOptions aio, aio_default;
    int fd, ret;
    struct stat st;
    OnOffAuto locking;

    opts = qemu_opts_create(&raw_runtime_opts, NULL, 0, &error_abort);
    if (!qemu_opts_absorb_qdict(opts, options, errp)) {
        ret = -EINVAL;
        goto fail;
    }

    filename = qemu_opt_get(opts, "filename");

    ret = raw_normalize_devicepath(&filename, errp);
    if (ret != 0) {
        goto fail;
    }

    aio_default = BLOCKDEV_AIO_OPTIONS_IO_URING;

    aio = qapi_enum_parse(&BlockdevAioOptions_lookup,
                          qemu_opt_get(opts, "aio"),
                          aio_default, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        ret = -EINVAL;
        goto fail;
    }

    s->use_linux_io_uring = (aio == BLOCKDEV_AIO_OPTIONS_IO_URING);

    locking = qapi_enum_parse(&OnOffAuto_lookup,
                              qemu_opt_get(opts, "locking"),
                              ON_OFF_AUTO_AUTO, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        ret = -EINVAL;
        goto fail;
    }
    switch (locking) {
    case ON_OFF_AUTO_ON:
        s->use_lock = true;
        if (!qemu_has_ofd_lock()) {
            warn_report("File lock requested but OFD locking syscall is "
                        "unavailable, falling back to POSIX file locks");
            error_printf("Due to the implementation, locks can be lost "
                         "unexpectedly.\n");
        }
        break;
    case ON_OFF_AUTO_OFF:
        s->use_lock = false;
        break;
    case ON_OFF_AUTO_AUTO:
        s->use_lock = qemu_has_ofd_lock();
        break;
    default:
        abort();
    }

    s->drop_cache = qemu_opt_get_bool(opts, "drop-cache", true);
    s->check_cache_dropped = qemu_opt_get_bool(opts, "x-check-cache-dropped",
                                               false);

    s->open_flags = open_flags;
    raw_parse_flags(bdrv_flags, &s->open_flags, false);

    s->fd = -1;
    fd = qemu_open(filename, s->open_flags, errp);
    ret = fd < 0 ? -errno : 0;

    if (ret < 0) {
        if (ret == -EROFS) {
            ret = -EACCES;
        }
        goto fail;
    }
    s->fd = fd;

    if (s->open_flags & O_RDWR) {
        ret = check_hdev_writable(s->fd);
        if (ret < 0) {
            error_setg_errno(errp, -ret, "The device is not writable");
            goto fail;
        }
    }

    s->perm = 0;
    s->shared_perm = BLK_PERM_ALL;

#ifndef CONFIG_LINUX_IO_URING
    if (s->use_linux_io_uring) {
        error_setg(errp, "aio=io_uring was specified, but is not supported "
                         "in this build.");
        ret = -EINVAL;
        goto fail;
    }
#endif /* !defined(CONFIG_LINUX_IO_URING) */

    s->has_discard = true;
    s->has_write_zeroes = true;

    if (fstat(s->fd, &st) < 0) {
        ret = -errno;
        error_setg_errno(errp, errno, "Could not stat file");
        goto fail;
    }

    if (!device) {
        if (!S_ISREG(st.st_mode)) {
            error_setg(errp, "'%s' driver requires '%s' to be a regular file",
                       bs->drv->format_name, bs->filename);
            ret = -EINVAL;
            goto fail;
        } else {
            s->has_fallocate = true;
        }
    } else {
        if (!(S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode))) {
            error_setg(errp, "'%s' driver requires '%s' to be either "
                       "a character or block device",
                       bs->drv->format_name, bs->filename);
            ret = -EINVAL;
            goto fail;
        }
    }
#ifdef CONFIG_BLKZONED
    if ((bs->bl.zoned != BLK_Z_NONE) &&
        (!(s->open_flags & O_DIRECT))) {
        error_setg(errp, "The driver supports zoned devices, and it requires "
                         "cache.direct=on, which was not specified.");
        return -EINVAL; /* No host kernel page cache */
    }
#endif

    if (S_ISBLK(st.st_mode)) {
#ifdef __linux__
        if (!(bs->open_flags & BDRV_O_NOCACHE)) {
            s->has_write_zeroes = false;
        }
#endif
    }
#ifdef __FreeBSD__
    if (S_ISCHR(st.st_mode)) {
        s->force_alignment = true;
    }
#endif
    s->needs_alignment = raw_needs_alignment(bs);

    bs->supported_write_flags = BDRV_REQ_FUA;
    if (s->use_linux_io_uring && !luring_has_fua()) {
        bs->supported_write_flags &= ~BDRV_REQ_FUA;
    }

    bs->supported_zero_flags = BDRV_REQ_MAY_UNMAP | BDRV_REQ_NO_FALLBACK;
    if (S_ISREG(st.st_mode)) {
        bs->supported_truncate_flags = BDRV_REQ_ZERO_WRITE;
    }
    ret = 0;
fail:
    if (ret < 0 && s->fd != -1) {
        qemu_close(s->fd);
    }
    if (filename && (bdrv_flags & BDRV_O_TEMPORARY)) {
        unlink(filename);
    }
    qemu_opts_del(opts);
    return ret;
}

static int raw_open(BlockDriverState *bs, QDict *options, int flags,
                    Error **errp)
{
    BDRVRawState *s = bs->opaque;

    s->type = FTYPE_FILE;
    return raw_open_common(bs, options, flags, 0, false, errp);
}

typedef enum {
    RAW_PL_PREPARE,
    RAW_PL_COMMIT,
    RAW_PL_ABORT,
} RawPermLockOp;

#define PERM_FOREACH(i) \
    for ((i) = 0; (1ULL << (i)) <= BLK_PERM_ALL; i++)

static int raw_apply_lock_bytes(BDRVRawState *s, int fd,
                                uint64_t perm_lock_bits,
                                uint64_t shared_perm_lock_bits,
                                bool unlock, Error **errp)
{
    int ret;
    int i;
    uint64_t locked_perm, locked_shared_perm;

    if (s) {
        locked_perm = s->locked_perm;
        locked_shared_perm = s->locked_shared_perm;
    } else {
        if (unlock) {
            locked_perm = BLK_PERM_ALL;
            locked_shared_perm = BLK_PERM_ALL;
        } else {
            locked_perm = 0;
            locked_shared_perm = 0;
        }
    }

    PERM_FOREACH(i) {
        int off = RAW_LOCK_PERM_BASE + i;
        uint64_t bit = (1ULL << i);
        if ((perm_lock_bits & bit) && !(locked_perm & bit)) {
            ret = qemu_lock_fd(fd, off, 1, false);
            if (ret) {
                raw_lock_error_setg_errno(errp, -ret, "Failed to lock byte %d",
                                          off);
                return ret;
            } else if (s) {
                s->locked_perm |= bit;
            }
        } else if (unlock && (locked_perm & bit) && !(perm_lock_bits & bit)) {
            ret = qemu_unlock_fd(fd, off, 1);
            if (ret) {
                error_setg_errno(errp, -ret, "Failed to unlock byte %d", off);
                return ret;
            } else if (s) {
                s->locked_perm &= ~bit;
            }
        }
    }
    PERM_FOREACH(i) {
        int off = RAW_LOCK_SHARED_BASE + i;
        uint64_t bit = (1ULL << i);
        if ((shared_perm_lock_bits & bit) && !(locked_shared_perm & bit)) {
            ret = qemu_lock_fd(fd, off, 1, false);
            if (ret) {
                raw_lock_error_setg_errno(errp, -ret, "Failed to lock byte %d",
                                          off);
                return ret;
            } else if (s) {
                s->locked_shared_perm |= bit;
            }
        } else if (unlock && (locked_shared_perm & bit) &&
                   !(shared_perm_lock_bits & bit)) {
            ret = qemu_unlock_fd(fd, off, 1);
            if (ret) {
                error_setg_errno(errp, -ret, "Failed to unlock byte %d", off);
                return ret;
            } else if (s) {
                s->locked_shared_perm &= ~bit;
            }
        }
    }
    return 0;
}

static int raw_check_lock_bytes(int fd, uint64_t perm, uint64_t shared_perm,
                                Error **errp)
{
    int ret;
    int i;

    PERM_FOREACH(i) {
        int off = RAW_LOCK_SHARED_BASE + i;
        uint64_t p = 1ULL << i;
        if (perm & p) {
            ret = qemu_lock_fd_test(fd, off, 1, true);
            if (ret) {
                char *perm_name = bdrv_perm_names(p);

                raw_lock_error_setg_errno(errp, -ret,
                                          "Failed to get \"%s\" lock",
                                          perm_name);
                g_free(perm_name);
                return ret;
            }
        }
    }
    PERM_FOREACH(i) {
        int off = RAW_LOCK_PERM_BASE + i;
        uint64_t p = 1ULL << i;
        if (!(shared_perm & p)) {
            ret = qemu_lock_fd_test(fd, off, 1, true);
            if (ret) {
                char *perm_name = bdrv_perm_names(p);

                raw_lock_error_setg_errno(errp, -ret,
                                          "Failed to get shared \"%s\" lock",
                                          perm_name);
                g_free(perm_name);
                return ret;
            }
        }
    }
    return 0;
}

static int raw_handle_perm_lock(BlockDriverState *bs,
                                RawPermLockOp op,
                                uint64_t new_perm, uint64_t new_shared,
                                Error **errp)
{
    BDRVRawState *s = bs->opaque;
    int ret = 0;
    Error *local_err = NULL;

    if (!s->use_lock) {
        return 0;
    }

    if (bdrv_get_flags(bs) & BDRV_O_INACTIVE) {
        return 0;
    }

    switch (op) {
    case RAW_PL_PREPARE:
        if ((s->perm | new_perm) == s->perm &&
            (s->shared_perm & new_shared) == s->shared_perm)
        {
            return 0;
        }
        ret = raw_apply_lock_bytes(s, s->fd, s->perm | new_perm,
                                   ~s->shared_perm | ~new_shared,
                                   false, errp);
        if (!ret) {
            ret = raw_check_lock_bytes(s->fd, new_perm, new_shared, errp);
            if (!ret) {
                return 0;
            }
            error_append_hint(errp,
                              "Is another process using the image [%s]?\n",
                              bs->filename);
        }
    case RAW_PL_ABORT:
        raw_apply_lock_bytes(s, s->fd, s->perm, ~s->shared_perm,
                             true, &local_err);
        if (local_err) {
            warn_report_err(local_err);
        }
        break;
    case RAW_PL_COMMIT:
        raw_apply_lock_bytes(s, s->fd, new_perm, ~new_shared,
                             true, &local_err);
        if (local_err) {
            warn_report_err(local_err);
        }
        break;
    }
    return ret;
}

static int fcntl_setfl(int fd, int flag)
{
    int flags;

    flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        return -errno;
    }
    if (fcntl(fd, F_SETFL, flags | flag) == -1) {
        return -errno;
    }
    return 0;
}

static int raw_reconfigure_getfd(BlockDriverState *bs, int flags,
                                 int *open_flags, uint64_t perm, Error **errp)
{
    BDRVRawState *s = bs->opaque;
    int fd = -1;
    int ret;
    bool has_writers = perm &
        (BLK_PERM_WRITE | BLK_PERM_WRITE_UNCHANGED | BLK_PERM_RESIZE);
    int fcntl_flags = O_APPEND | O_NONBLOCK;
#ifdef O_NOATIME
    fcntl_flags |= O_NOATIME;
#endif

    *open_flags = 0;
    if (s->type == FTYPE_CD) {
        *open_flags |= O_NONBLOCK;
    }

    raw_parse_flags(flags, open_flags, has_writers);

#ifdef O_ASYNC
    assert((s->open_flags & O_ASYNC) == 0);
#endif

    if (*open_flags == s->open_flags) {
        return s->fd;
    }

    if ((*open_flags & ~fcntl_flags) == (s->open_flags & ~fcntl_flags)) {
        fd = qemu_dup(s->fd);
        if (fd >= 0) {
            ret = fcntl_setfl(fd, *open_flags);
            if (ret) {
                qemu_close(fd);
                fd = -1;
            }
        }
    }

    if (fd == -1) {
        const char *normalized_filename = bs->filename;
        ret = raw_normalize_devicepath(&normalized_filename, errp);
        if (ret >= 0) {
            fd = qemu_open(normalized_filename, *open_flags, errp);
            if (fd == -1) {
                return -1;
            }
        }
    }

    if (fd != -1 && (*open_flags & O_RDWR)) {
        ret = check_hdev_writable(fd);
        if (ret < 0) {
            qemu_close(fd);
            error_setg_errno(errp, -ret, "The device is not writable");
            return -1;
        }
    }

    return fd;
}

static int raw_reopen_prepare(BDRVReopenState *state,
                              BlockReopenQueue *queue, Error **errp)
{
    BDRVRawState *s;
    BDRVRawReopenState *rs;
    QemuOpts *opts;
    int ret;

    assert(state != NULL);
    assert(state->bs != NULL);

    s = state->bs->opaque;

    state->opaque = g_new0(BDRVRawReopenState, 1);
    rs = state->opaque;

    opts = qemu_opts_create(&raw_runtime_opts, NULL, 0, &error_abort);
    if (!qemu_opts_absorb_qdict(opts, state->options, errp)) {
        ret = -EINVAL;
        goto out;
    }

    rs->drop_cache = qemu_opt_get_bool_del(opts, "drop-cache", true);
    rs->check_cache_dropped =
        qemu_opt_get_bool_del(opts, "x-check-cache-dropped", false);

    qemu_opts_to_qdict(opts, state->options);


    s->reopen_state = state;
    ret = 0;

out:
    qemu_opts_del(opts);
    return ret;
}

static void raw_reopen_commit(BDRVReopenState *state)
{
    BDRVRawReopenState *rs = state->opaque;
    BDRVRawState *s = state->bs->opaque;

    s->drop_cache = rs->drop_cache;
    s->check_cache_dropped = rs->check_cache_dropped;
    s->open_flags = rs->open_flags;
    g_free(state->opaque);
    state->opaque = NULL;

    assert(s->reopen_state == state);
    s->reopen_state = NULL;
}


static void raw_reopen_abort(BDRVReopenState *state)
{
    BDRVRawReopenState *rs = state->opaque;
    BDRVRawState *s = state->bs->opaque;

    if (rs == NULL) {
        return;
    }

    g_free(state->opaque);
    state->opaque = NULL;

    assert(s->reopen_state == state);
    s->reopen_state = NULL;
}

static int hdev_get_max_hw_transfer(int fd, struct stat *st)
{
#ifdef BLKSECTGET
    if (S_ISBLK(st->st_mode)) {
        unsigned short max_sectors = 0;
        if (ioctl(fd, BLKSECTGET, &max_sectors) == 0) {
            return max_sectors * 512;
        }
    } else {
        int max_bytes = 0;
        if (ioctl(fd, BLKSECTGET, &max_bytes) == 0) {
            return max_bytes;
        }
    }
    return -errno;
#else
    return -ENOSYS;
#endif
}

#ifdef CONFIG_LINUX
static int get_sysfs_str_val(struct stat *st, const char *attribute,
                             char **val) {
    g_autofree char *sysfspath = NULL;
    size_t len;

    if (!S_ISBLK(st->st_mode)) {
        return -ENOTSUP;
    }

    sysfspath = g_strdup_printf("/sys/dev/block/%u:%u/queue/%s",
                                major(st->st_rdev), minor(st->st_rdev),
                                attribute);
    if (!g_file_get_contents(sysfspath, val, &len, NULL)) {
        return -ENOENT;
    }

    char *p;
    p = *val;
    if (*(p + len - 1) == '\n') {
        *(p + len - 1) = '\0';
    }
    return 0;
}
#endif

#if defined(CONFIG_BLKZONED)
static int get_sysfs_zoned_model(struct stat *st, BlockZoneModel *zoned)
{
    g_autofree char *val = NULL;
    int ret;

    ret = get_sysfs_str_val(st, "zoned", &val);
    if (ret < 0) {
        return ret;
    }

    if (strcmp(val, "host-managed") == 0) {
        *zoned = BLK_Z_HM;
    } else if (strcmp(val, "host-aware") == 0) {
        *zoned = BLK_Z_HA;
    } else if (strcmp(val, "none") == 0) {
        *zoned = BLK_Z_NONE;
    } else {
        return -ENOTSUP;
    }
    return 0;
}
#endif /* defined(CONFIG_BLKZONED) */

#ifdef CONFIG_LINUX
static long get_sysfs_long_val(struct stat *st, const char *attribute)
{
    g_autofree char *str = NULL;
    const char *end;
    long val;
    int ret;

    ret = get_sysfs_str_val(st, attribute, &str);
    if (ret < 0) {
        return ret;
    }

    ret = qemu_strtol(str, &end, 10, &val);
    if (ret == 0 && end && *end == '\0') {
        ret = val;
    }
    return ret;
}
#endif

static int hdev_get_max_segments(int fd, struct stat *st)
{
#ifdef CONFIG_LINUX
    int ret;

    if (S_ISCHR(st->st_mode)) {
        if (ioctl(fd, SG_GET_SG_TABLESIZE, &ret) == 0) {
            return ret;
        }
        return -ENOTSUP;
    }
    return get_sysfs_long_val(st, "max_segments");
#else
    return -ENOTSUP;
#endif
}

#if defined(CONFIG_BLKZONED)
static int get_zones_wp(BlockDriverState *bs, int fd, int64_t offset,
                        unsigned int nrz, bool reset_all)
{
    struct blk_zone *blkz;
    size_t rep_size;
    uint64_t sector = offset >> BDRV_SECTOR_BITS;
    BlockZoneWps *wps = bs->wps;
    unsigned int j = offset / bs->bl.zone_size;
    unsigned int n = 0, i = 0;
    int ret;
    rep_size = sizeof(struct blk_zone_report) + nrz * sizeof(struct blk_zone);
    g_autofree struct blk_zone_report *rep = NULL;

    rep = g_malloc(rep_size);
    blkz = (struct blk_zone *)(rep + 1);
    while (n < nrz) {
        memset(rep, 0, rep_size);
        rep->sector = sector;
        rep->nr_zones = nrz - n;

        do {
            ret = ioctl(fd, BLKREPORTZONE, rep);
        } while (ret != 0 && errno == EINTR);
        if (ret != 0) {
            error_report("%d: ioctl BLKREPORTZONE at %" PRId64 " failed %d",
                    fd, offset, errno);
            return -errno;
        }

        if (!rep->nr_zones) {
            break;
        }

        for (i = 0; i < rep->nr_zones; ++i, ++n, ++j) {
            if (blkz[i].type == BLK_ZONE_TYPE_CONVENTIONAL) {
                wps->wp[j] |= 1ULL << 63;
            } else {
                switch(blkz[i].cond) {
                case BLK_ZONE_COND_FULL:
                case BLK_ZONE_COND_READONLY:
                    wps->wp[j] = (blkz[i].start + blkz[i].len) << BDRV_SECTOR_BITS;
                    break;
                case BLK_ZONE_COND_OFFLINE:
                    wps->wp[j] = (blkz[i].start) << BDRV_SECTOR_BITS;
                    break;
                default:
                    if (reset_all) {
                        wps->wp[j] = blkz[i].start << BDRV_SECTOR_BITS;
                    } else {
                        wps->wp[j] = blkz[i].wp << BDRV_SECTOR_BITS;
                    }
                    break;
                }
            }
        }
        sector = blkz[i - 1].start + blkz[i - 1].len;
    }

    return 0;
}

static void update_zones_wp(BlockDriverState *bs, int fd, int64_t offset,
                            unsigned int nrz)
{
    if (get_zones_wp(bs, fd, offset, nrz, 0) < 0) {
        error_report("update zone wp failed");
    }
}

static void raw_refresh_zoned_limits(BlockDriverState *bs, struct stat *st,
                                     Error **errp)
{
    BDRVRawState *s = bs->opaque;
    BlockZoneModel zoned = BLK_Z_NONE;
    int ret;

    ret = get_sysfs_zoned_model(st, &zoned);
    if (ret < 0 || zoned == BLK_Z_NONE) {
        goto no_zoned;
    }
    bs->bl.zoned = zoned;

    ret = get_sysfs_long_val(st, "max_open_zones");
    if (ret >= 0) {
        bs->bl.max_open_zones = ret;
    }

    ret = get_sysfs_long_val(st, "max_active_zones");
    if (ret >= 0) {
        bs->bl.max_active_zones = ret;
    }

    ret = get_sysfs_long_val(st, "chunk_sectors");
    if (ret < 0) {
        error_setg_errno(errp, -ret, "Unable to read chunk_sectors "
                                     "sysfs attribute");
        goto no_zoned;
    } else if (!ret) {
        error_setg(errp, "Read 0 from chunk_sectors sysfs attribute");
        goto no_zoned;
    }
    bs->bl.zone_size = ret << BDRV_SECTOR_BITS;

    ret = get_sysfs_long_val(st, "nr_zones");
    if (ret < 0) {
        error_setg_errno(errp, -ret, "Unable to read nr_zones "
                                     "sysfs attribute");
        goto no_zoned;
    } else if (!ret) {
        error_setg(errp, "Read 0 from nr_zones sysfs attribute");
        goto no_zoned;
    }
    bs->bl.nr_zones = ret;

    ret = get_sysfs_long_val(st, "zone_append_max_bytes");
    if (ret > 0) {
        bs->bl.max_append_sectors = ret >> BDRV_SECTOR_BITS;
    }

    ret = get_sysfs_long_val(st, "physical_block_size");
    if (ret >= 0) {
        bs->bl.write_granularity = ret;
    }

    g_free(bs->wps);
    bs->wps = g_malloc(sizeof(BlockZoneWps) +
            sizeof(int64_t) * bs->bl.nr_zones);
    ret = get_zones_wp(bs, s->fd, 0, bs->bl.nr_zones, 0);
    if (ret < 0) {
        error_setg_errno(errp, -ret, "report wps failed");
        goto no_zoned;
    }
    qemu_co_mutex_init(&bs->wps->colock);
    return;

no_zoned:
    bs->bl.zoned = BLK_Z_NONE;
    g_free(bs->wps);
    bs->wps = NULL;
}
#else /* !defined(CONFIG_BLKZONED) */
static void raw_refresh_zoned_limits(BlockDriverState *bs, struct stat *st,
                                     Error **errp)
{
    bs->bl.zoned = BLK_Z_NONE;
}
#endif /* !defined(CONFIG_BLKZONED) */

static void raw_refresh_limits(BlockDriverState *bs, Error **errp)
{
    BDRVRawState *s = bs->opaque;
    struct stat st;

    s->needs_alignment = raw_needs_alignment(bs);
    raw_probe_alignment(bs, s->fd, errp);

    bs->bl.min_mem_alignment = s->buf_align;
    bs->bl.opt_mem_alignment = MAX(s->buf_align, qemu_real_host_page_size());

    if (fstat(s->fd, &st)) {
        return;
    }

#if defined(__APPLE__) && (__MACH__)
    struct statfs buf;

    if (!fstatfs(s->fd, &buf)) {
        bs->bl.opt_transfer = buf.f_iosize;
        bs->bl.pdiscard_alignment = buf.f_bsize;
    }
#endif

    if (bdrv_is_sg(bs) || S_ISBLK(st.st_mode)) {
        int ret = hdev_get_max_hw_transfer(s->fd, &st);

        if (ret > 0 && ret <= BDRV_REQUEST_MAX_BYTES) {
            bs->bl.max_hw_transfer = ret;
        }

        ret = hdev_get_max_segments(s->fd, &st);
        if (ret > 0) {
            bs->bl.max_hw_iov = ret;
        }
    }

    raw_refresh_zoned_limits(bs, &st, errp);
}

static int check_for_dasd(int fd)
{
#ifdef BIODASDINFO2
    struct dasd_information2_t info = {0};

    return ioctl(fd, BIODASDINFO2, &info);
#else
    return -1;
#endif
}

static int hdev_probe_blocksizes(BlockDriverState *bs, BlockSizes *bsz)
{
    BDRVRawState *s = bs->opaque;
    int ret;

    if (check_for_dasd(s->fd) < 0) {
        if (bs->bl.zoned == BLK_Z_NONE) {
            return -ENOTSUP;
        }
    }
    ret = probe_logical_blocksize(s->fd, &bsz->log);
    if (ret < 0) {
        return ret;
    }
    return probe_physical_blocksize(s->fd, &bsz->phys);
}

#ifdef __linux__
static int hdev_probe_geometry(BlockDriverState *bs, HDGeometry *geo)
{
    BDRVRawState *s = bs->opaque;
    struct hd_geometry ioctl_geo = {0};

    if (check_for_dasd(s->fd) < 0) {
        return -ENOTSUP;
    }
    if (ioctl(s->fd, HDIO_GETGEO, &ioctl_geo) < 0) {
        return -errno;
    }
    if (!ioctl_geo.heads || !ioctl_geo.sectors || !ioctl_geo.cylinders) {
        return -ENOTSUP;
    }
    if (ioctl_geo.start != 0) {
        return -ENOTSUP;
    }
    geo->heads = ioctl_geo.heads;
    geo->sectors = ioctl_geo.sectors;
    geo->cylinders = ioctl_geo.cylinders;

    return 0;
}
#else /* __linux__ */
static int hdev_probe_geometry(BlockDriverState *bs, HDGeometry *geo)
{
    return -ENOTSUP;
}
#endif

#if defined(__linux__)
static int handle_aiocb_ioctl(void *opaque)
{
    RawPosixAIOData *aiocb = opaque;
    int ret;

    ret = RETRY_ON_EINTR(
        ioctl(aiocb->aio_fildes, aiocb->ioctl.cmd, aiocb->ioctl.buf)
    );
    if (ret == -1) {
        return -errno;
    }

    return 0;
}
#endif /* linux */

#if defined(CONFIG_FALLOCATE) || defined(BLKZEROOUT) || defined(BLKDISCARD)
static int translate_err(int err)
{
    if (err == -ENODEV || err == -ENOSYS || err == -EOPNOTSUPP ||
        err == -ENOTTY) {
        err = -ENOTSUP;
    }
    return err;
}
#endif

#ifdef CONFIG_FALLOCATE
static int do_fallocate(int fd, int mode, off_t offset, off_t len)
{
    do {
        if (fallocate(fd, mode, offset, len) == 0) {
            return 0;
        }
    } while (errno == EINTR);
    return translate_err(-errno);
}
#endif

static ssize_t handle_aiocb_write_zeroes_block(RawPosixAIOData *aiocb)
{
    int ret = -ENOTSUP;
    BDRVRawState *s = aiocb->bs->opaque;

    if (!s->has_write_zeroes) {
        return -ENOTSUP;
    }

#ifdef BLKZEROOUT
    if (!(aiocb->aio_type & QEMU_AIO_NO_FALLBACK)) {
        do {
            uint64_t range[2] = { aiocb->aio_offset, aiocb->aio_nbytes };
            if (ioctl(aiocb->aio_fildes, BLKZEROOUT, range) == 0) {
                return 0;
            }
        } while (errno == EINTR);

        ret = translate_err(-errno);
        if (ret == -ENOTSUP) {
            s->has_write_zeroes = false;
        }
    }
#endif

    return ret;
}

static int handle_aiocb_write_zeroes(void *opaque)
{
    RawPosixAIOData *aiocb = opaque;
#ifdef CONFIG_FALLOCATE
    BDRVRawState *s = aiocb->bs->opaque;
    int64_t len;
#endif

    if (aiocb->aio_type & QEMU_AIO_BLKDEV) {
        return handle_aiocb_write_zeroes_block(aiocb);
    }

#ifdef CONFIG_FALLOCATE_ZERO_RANGE
    if (s->has_write_zeroes) {
        int ret = do_fallocate(s->fd, FALLOC_FL_ZERO_RANGE,
                               aiocb->aio_offset, aiocb->aio_nbytes);
        if (ret == -ENOTSUP) {
            s->has_write_zeroes = false;
        } else if (ret == 0 || ret != -EINVAL) {
            return ret;
        }
    }
#endif

#ifdef CONFIG_FALLOCATE_PUNCH_HOLE
    if (s->has_discard && s->has_fallocate) {
        int ret = do_fallocate(s->fd,
                               FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
                               aiocb->aio_offset, aiocb->aio_nbytes);
        if (ret == 0) {
            ret = do_fallocate(s->fd, 0, aiocb->aio_offset, aiocb->aio_nbytes);
            if (ret == 0 || ret != -ENOTSUP) {
                return ret;
            }
            s->has_fallocate = false;
        } else if (ret == -EINVAL) {
            warn_report_once("Your file system is misbehaving: "
                             "fallocate(FALLOC_FL_PUNCH_HOLE) returned EINVAL. "
                             "Please report this bug to your file system "
                             "vendor.");
        } else if (ret != -ENOTSUP) {
            return ret;
        } else {
            s->has_discard = false;
        }
    }
#endif

#ifdef CONFIG_FALLOCATE
    len = raw_getlength(aiocb->bs);
    if (s->has_fallocate && len >= 0 && aiocb->aio_offset >= len) {
        int ret = do_fallocate(s->fd, 0, aiocb->aio_offset, aiocb->aio_nbytes);
        if (ret == 0 || ret != -ENOTSUP) {
            return ret;
        }
        s->has_fallocate = false;
    }
#endif

    return -ENOTSUP;
}

static int handle_aiocb_write_zeroes_unmap(void *opaque)
{
    RawPosixAIOData *aiocb = opaque;
    BDRVRawState *s G_GNUC_UNUSED = aiocb->bs->opaque;


#ifdef CONFIG_FALLOCATE_PUNCH_HOLE
    int ret = do_fallocate(s->fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
                           aiocb->aio_offset, aiocb->aio_nbytes);
    switch (ret) {
    case -ENOTSUP:
    case -EINVAL:
    case -EBUSY:
        break;
    default:
        return ret;
    }
#endif

    return handle_aiocb_write_zeroes(aiocb);
}

#ifndef HAVE_COPY_FILE_RANGE
static off_t copy_file_range(int in_fd, off_t *in_off, int out_fd,
                             off_t *out_off, size_t len, unsigned int flags)
{
#ifdef __NR_copy_file_range
    return syscall(__NR_copy_file_range, in_fd, in_off, out_fd,
                   out_off, len, flags);
#else
    errno = ENOSYS;
    return -1;
#endif
}
#endif

#if defined(CONFIG_BLKZONED)
static inline int parse_zone(struct BlockZoneDescriptor *zone,
                              const struct blk_zone *blkz) {
    zone->start = blkz->start << BDRV_SECTOR_BITS;
    zone->length = blkz->len << BDRV_SECTOR_BITS;
    zone->wp = blkz->wp << BDRV_SECTOR_BITS;

#ifdef HAVE_BLK_ZONE_REP_CAPACITY
    zone->cap = blkz->capacity << BDRV_SECTOR_BITS;
#else
    zone->cap = blkz->len << BDRV_SECTOR_BITS;
#endif

    switch (blkz->type) {
    case BLK_ZONE_TYPE_SEQWRITE_REQ:
        zone->type = BLK_ZT_SWR;
        break;
    case BLK_ZONE_TYPE_SEQWRITE_PREF:
        zone->type = BLK_ZT_SWP;
        break;
    case BLK_ZONE_TYPE_CONVENTIONAL:
        zone->type = BLK_ZT_CONV;
        break;
    default:
        error_report("Unsupported zone type: 0x%x", blkz->type);
        return -ENOTSUP;
    }

    switch (blkz->cond) {
    case BLK_ZONE_COND_NOT_WP:
        zone->state = BLK_ZS_NOT_WP;
        break;
    case BLK_ZONE_COND_EMPTY:
        zone->state = BLK_ZS_EMPTY;
        break;
    case BLK_ZONE_COND_IMP_OPEN:
        zone->state = BLK_ZS_IOPEN;
        break;
    case BLK_ZONE_COND_EXP_OPEN:
        zone->state = BLK_ZS_EOPEN;
        break;
    case BLK_ZONE_COND_CLOSED:
        zone->state = BLK_ZS_CLOSED;
        break;
    case BLK_ZONE_COND_READONLY:
        zone->state = BLK_ZS_RDONLY;
        break;
    case BLK_ZONE_COND_FULL:
        zone->state = BLK_ZS_FULL;
        break;
    case BLK_ZONE_COND_OFFLINE:
        zone->state = BLK_ZS_OFFLINE;
        break;
    default:
        error_report("Unsupported zone state: 0x%x", blkz->cond);
        return -ENOTSUP;
    }
    return 0;
}
#endif

#if defined(CONFIG_BLKZONED)
static int handle_aiocb_zone_report(void *opaque)
{
    RawPosixAIOData *aiocb = opaque;
    int fd = aiocb->aio_fildes;
    unsigned int *nr_zones = aiocb->zone_report.nr_zones;
    BlockZoneDescriptor *zones = aiocb->zone_report.zones;
    uint64_t sector = aiocb->aio_offset / 512;

    struct blk_zone *blkz;
    size_t rep_size;
    unsigned int nrz;
    int ret;
    unsigned int n = 0, i = 0;

    nrz = *nr_zones;
    rep_size = sizeof(struct blk_zone_report) + nrz * sizeof(struct blk_zone);
    g_autofree struct blk_zone_report *rep = NULL;
    rep = g_malloc(rep_size);

    blkz = (struct blk_zone *)(rep + 1);
    while (n < nrz) {
        memset(rep, 0, rep_size);
        rep->sector = sector;
        rep->nr_zones = nrz - n;

        do {
            ret = ioctl(fd, BLKREPORTZONE, rep);
        } while (ret != 0 && errno == EINTR);
        if (ret != 0) {
            error_report("%d: ioctl BLKREPORTZONE at %" PRId64 " failed %d",
                         fd, sector, errno);
            return -errno;
        }

        if (!rep->nr_zones) {
            break;
        }

        for (i = 0; i < rep->nr_zones; i++, n++) {
            ret = parse_zone(&zones[n], &blkz[i]);
            if (ret != 0) {
                return ret;
            }

            sector = blkz[i].start + blkz[i].len;
        }
    }

    *nr_zones = n;
    return 0;
}
#endif

#if defined(CONFIG_BLKZONED)
static int handle_aiocb_zone_mgmt(void *opaque)
{
    RawPosixAIOData *aiocb = opaque;
    int fd = aiocb->aio_fildes;
    uint64_t sector = aiocb->aio_offset / 512;
    int64_t nr_sectors = aiocb->aio_nbytes / 512;
    struct blk_zone_range range;
    int ret;

    range.sector = sector;
    range.nr_sectors = nr_sectors;
    do {
        ret = ioctl(fd, aiocb->zone_mgmt.op, &range);
    } while (ret != 0 && errno == EINTR);

    return ret < 0 ? -errno : ret;
}
#endif

static int handle_aiocb_copy_range(void *opaque)
{
    RawPosixAIOData *aiocb = opaque;
    uint64_t bytes = aiocb->aio_nbytes;
    off_t in_off = aiocb->aio_offset;
    off_t out_off = aiocb->copy_range.aio_offset2;

    while (bytes) {
        ssize_t ret = copy_file_range(aiocb->aio_fildes, &in_off,
                                      aiocb->copy_range.aio_fd2, &out_off,
                                      bytes, 0);
        trace_file_copy_file_range(aiocb->bs, aiocb->aio_fildes, in_off,
                                   aiocb->copy_range.aio_fd2, out_off, bytes,
                                   0, ret);
        if (ret == 0) {
            return -ENOSPC;
        }
        if (ret < 0) {
            switch (errno) {
            case ENOSYS:
                return -ENOTSUP;
            case EINTR:
                continue;
            default:
                return -errno;
            }
        }
        bytes -= ret;
    }
    return 0;
}

static int handle_aiocb_discard(void *opaque)
{
    RawPosixAIOData *aiocb = opaque;
    int ret = -ENOTSUP;
    BDRVRawState *s = aiocb->bs->opaque;

    if (!s->has_discard) {
        return -ENOTSUP;
    }

    if (aiocb->aio_type & QEMU_AIO_BLKDEV) {
#ifdef BLKDISCARD
        do {
            uint64_t range[2] = { aiocb->aio_offset, aiocb->aio_nbytes };
            if (ioctl(aiocb->aio_fildes, BLKDISCARD, range) == 0) {
                return 0;
            }
        } while (errno == EINTR);

        ret = translate_err(-errno);
#endif
    } else {
#ifdef CONFIG_FALLOCATE_PUNCH_HOLE
        ret = do_fallocate(s->fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
                           aiocb->aio_offset, aiocb->aio_nbytes);
        ret = translate_err(ret);
#elif defined(__APPLE__) && (__MACH__)
        fpunchhole_t fpunchhole;
        fpunchhole.fp_flags = 0;
        fpunchhole.reserved = 0;
        fpunchhole.fp_offset = aiocb->aio_offset;
        fpunchhole.fp_length = aiocb->aio_nbytes;
        if (fcntl(s->fd, F_PUNCHHOLE, &fpunchhole) == -1) {
            ret = errno == ENODEV ? -ENOTSUP : -errno;
        } else {
            ret = 0;
        }
#endif
    }

    if (ret == -ENOTSUP) {
        s->has_discard = false;
    }
    return ret;
}

static int allocate_first_block(int fd, size_t max_size)
{
    size_t write_size = (max_size < MAX_BLOCKSIZE)
        ? BDRV_SECTOR_SIZE
        : MAX_BLOCKSIZE;
    size_t max_align = MAX(MAX_BLOCKSIZE, qemu_real_host_page_size());
    void *buf;
    ssize_t n;
    int ret;

    buf = qemu_memalign(max_align, write_size);
    memset(buf, 0, write_size);

    n = RETRY_ON_EINTR(pwrite(fd, buf, write_size, 0));

    ret = (n == -1) ? -errno : 0;

    qemu_vfree(buf);
    return ret;
}

static int handle_aiocb_truncate(void *opaque)
{
    RawPosixAIOData *aiocb = opaque;
    int result = 0;
    int64_t current_length = 0;
    char *buf = NULL;
    struct stat st;
    int fd = aiocb->aio_fildes;
    int64_t offset = aiocb->aio_offset;
    PreallocMode prealloc = aiocb->truncate.prealloc;
    Error **errp = aiocb->truncate.errp;

    if (fstat(fd, &st) < 0) {
        result = -errno;
        error_setg_errno(errp, -result, "Could not stat file");
        return result;
    }

    current_length = st.st_size;
    if (current_length > offset && prealloc != PREALLOC_MODE_OFF) {
        error_setg(errp, "Cannot use preallocation for shrinking files");
        return -ENOTSUP;
    }

    switch (prealloc) {
#ifdef CONFIG_POSIX_FALLOCATE
    case PREALLOC_MODE_FALLOC:
        if (offset != current_length) {
            result = -posix_fallocate(fd, current_length,
                                      offset - current_length);
            if (result != 0) {
                error_setg_errno(errp, -result,
                                 "Could not preallocate new data");
            } else if (current_length == 0) {
                allocate_first_block(fd, offset);
            }
        } else {
            result = 0;
        }
        goto out;
#endif
    case PREALLOC_MODE_FULL:
    {
        int64_t num = 0, left = offset - current_length;
        off_t seek_result;

        if (ftruncate(fd, offset) != 0) {
            result = -errno;
            error_setg_errno(errp, -result, "Could not resize file");
            goto out;
        }

        buf = g_malloc0(65536);

        seek_result = lseek(fd, current_length, SEEK_SET);
        if (seek_result < 0) {
            result = -errno;
            error_setg_errno(errp, -result,
                             "Failed to seek to the old end of file");
            goto out;
        }

        while (left > 0) {
            num = MIN(left, 65536);
            result = write(fd, buf, num);
            if (result < 0) {
                if (errno == EINTR) {
                    continue;
                }
                result = -errno;
                error_setg_errno(errp, -result,
                                 "Could not write zeros for preallocation");
                goto out;
            }
            left -= result;
        }
        if (result >= 0) {
            result = fsync(fd);
            if (result < 0) {
                result = -errno;
                error_setg_errno(errp, -result,
                                 "Could not flush file to disk");
                goto out;
            }
        }
        goto out;
    }
    case PREALLOC_MODE_OFF:
        if (ftruncate(fd, offset) != 0) {
            result = -errno;
            error_setg_errno(errp, -result, "Could not resize file");
        } else if (current_length == 0 && offset > current_length) {
            allocate_first_block(fd, offset);
        }
        return result;
    default:
        result = -ENOTSUP;
        error_setg(errp, "Unsupported preallocation mode: %s",
                   PreallocMode_str(prealloc));
        return result;
    }

out:
    if (result < 0) {
        if (ftruncate(fd, current_length) < 0) {
            error_report("Failed to restore old file length: %s",
                         strerror(errno));
        }
    }

    g_free(buf);
    return result;
}

static int coroutine_fn raw_thread_pool_submit(ThreadPoolFunc func, void *arg)
{
    return thread_pool_submit_co(func, arg);
}

static bool bdrv_qiov_is_aligned(BlockDriverState *bs, QEMUIOVector *qiov)
{
    int i;
    size_t alignment = bdrv_min_mem_align(bs);
    size_t len = bs->bl.request_alignment;
    IO_CODE();

    for (i = 0; i < qiov->niov; i++) {
        if ((uintptr_t) qiov->iov[i].iov_base % alignment) {
            return false;
        }
        if (qiov->iov[i].iov_len % len) {
            return false;
        }
    }

    return true;
}

#ifdef CONFIG_LINUX_IO_URING
static inline bool raw_check_linux_io_uring(BDRVRawState *s)
{
    Error *local_err = NULL;
    AioContext *ctx;

    if (!s->use_linux_io_uring) {
        return false;
    }

    ctx = qemu_get_current_aio_context();
    if (unlikely(!aio_setup_linux_io_uring(ctx, &local_err))) {
        error_reportf_err(local_err, "Unable to use linux io_uring: ");
        s->use_linux_io_uring = false;
        return false;
    }
    return true;
}
#endif

static int coroutine_fn raw_co_prw(BlockDriverState *bs, int64_t *offset_ptr,
                                   uint64_t bytes, QEMUIOVector *qiov, int type,
                                   int flags)
{
    BDRVRawState *s = bs->opaque;
    int ret;
    uint64_t offset = *offset_ptr;

    if (fd_open(bs) < 0)
        return -EIO;
#if defined(CONFIG_BLKZONED)
    if ((type & (QEMU_AIO_WRITE | QEMU_AIO_ZONE_APPEND)) &&
        bs->bl.zoned != BLK_Z_NONE) {
        qemu_co_mutex_lock(&bs->wps->colock);
        if (type & QEMU_AIO_ZONE_APPEND) {
            int index = offset / bs->bl.zone_size;
            offset = bs->wps->wp[index];
        }
    }
#endif

    if (s->needs_alignment && !bdrv_qiov_is_aligned(bs, qiov)) {
        ret = -EINVAL;
        goto out;
#ifdef CONFIG_LINUX_IO_URING
    } else if (raw_check_linux_io_uring(s)) {
        assert(qiov->size == bytes);
        ret = luring_co_submit(bs, s->fd, offset, qiov, type, flags);
        goto out;
    } else {
        ret = -EIO;
        goto out;
#else
    } else {
        ret = -ENOTSUP;
        goto out;
#endif
    }

out:
#if defined(CONFIG_BLKZONED)
    if ((type & (QEMU_AIO_WRITE | QEMU_AIO_ZONE_APPEND)) &&
        bs->bl.zoned != BLK_Z_NONE) {
        BlockZoneWps *wps = bs->wps;
        if (ret == 0) {
            uint64_t *wp = &wps->wp[offset / bs->bl.zone_size];
            if (!BDRV_ZT_IS_CONV(*wp)) {
                if (type & QEMU_AIO_ZONE_APPEND) {
                    *offset_ptr = *wp;
                    trace_zbd_zone_append_complete(bs, *offset_ptr
                        >> BDRV_SECTOR_BITS);
                }
                if (offset + bytes > *wp) {
                    *wp = offset + bytes;
                }
            }
        } else {
            update_zones_wp(bs, s->fd, offset, 1);
        }

        qemu_co_mutex_unlock(&wps->colock);
    }
#endif
    return ret;
}

static int coroutine_fn raw_co_preadv(BlockDriverState *bs, int64_t offset,
                                      int64_t bytes, QEMUIOVector *qiov,
                                      BdrvRequestFlags flags)
{
    return raw_co_prw(bs, &offset, bytes, qiov, QEMU_AIO_READ, flags);
}

static int coroutine_fn raw_co_pwritev(BlockDriverState *bs, int64_t offset,
                                       int64_t bytes, QEMUIOVector *qiov,
                                       BdrvRequestFlags flags)
{
    return raw_co_prw(bs, &offset, bytes, qiov, QEMU_AIO_WRITE, flags);
}

static int coroutine_fn raw_co_flush_to_disk(BlockDriverState *bs)
{
    BDRVRawState *s = bs->opaque;
    int ret;

    ret = fd_open(bs);
    if (ret < 0) {
        return ret;
    }

#ifdef CONFIG_LINUX_IO_URING
    if (raw_check_linux_io_uring(s)) {
        return luring_co_submit(bs, s->fd, 0, NULL, QEMU_AIO_FLUSH, 0);
    }
    return -EIO;
#else
    return -ENOTSUP;
#endif
}

static void raw_close(BlockDriverState *bs)
{
    BDRVRawState *s = bs->opaque;

    if (s->fd >= 0) {
#if defined(CONFIG_BLKZONED)
        g_free(bs->wps);
#endif
        qemu_close(s->fd);
        s->fd = -1;
    }
}

static int coroutine_fn
raw_regular_truncate(BlockDriverState *bs, int fd, int64_t offset,
                     PreallocMode prealloc, Error **errp)
{
    RawPosixAIOData acb;

    acb = (RawPosixAIOData) {
        .bs             = bs,
        .aio_fildes     = fd,
        .aio_type       = QEMU_AIO_TRUNCATE,
        .aio_offset     = offset,
        .truncate       = {
            .prealloc       = prealloc,
            .errp           = errp,
        },
    };

    return raw_thread_pool_submit(handle_aiocb_truncate, &acb);
}

static int coroutine_fn raw_co_truncate(BlockDriverState *bs, int64_t offset,
                                        bool exact, PreallocMode prealloc,
                                        BdrvRequestFlags flags, Error **errp)
{
    BDRVRawState *s = bs->opaque;
    struct stat st;
    int ret;

    if (fstat(s->fd, &st)) {
        ret = -errno;
        error_setg_errno(errp, -ret, "Failed to fstat() the file");
        return ret;
    }

    if (S_ISREG(st.st_mode)) {
        return raw_regular_truncate(bs, s->fd, offset, prealloc, errp);
    }

    if (prealloc != PREALLOC_MODE_OFF) {
        error_setg(errp, "Preallocation mode '%s' unsupported for this "
                   "non-regular file", PreallocMode_str(prealloc));
        return -ENOTSUP;
    }

    if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode)) {
        int64_t cur_length = raw_getlength(bs);

        if (offset != cur_length && exact) {
            error_setg(errp, "Cannot resize device files");
            return -ENOTSUP;
        } else if (offset > cur_length) {
            error_setg(errp, "Cannot grow device files");
            return -EINVAL;
        }
    } else {
        error_setg(errp, "Resizing this file is not supported");
        return -ENOTSUP;
    }

    return 0;
}

#ifdef __OpenBSD__
static int64_t raw_getlength(BlockDriverState *bs)
{
    BDRVRawState *s = bs->opaque;
    int fd = s->fd;
    struct stat st;

    if (fstat(fd, &st))
        return -errno;
    if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode)) {
        struct disklabel dl;

        if (ioctl(fd, DIOCGDINFO, &dl))
            return -errno;
        return (uint64_t)dl.d_secsize *
            dl.d_partitions[DISKPART(st.st_rdev)].p_size;
    } else
        return st.st_size;
}
#elif defined(__NetBSD__)
static int64_t raw_getlength(BlockDriverState *bs)
{
    BDRVRawState *s = bs->opaque;
    int fd = s->fd;
    struct stat st;

    if (fstat(fd, &st))
        return -errno;
    if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode)) {
        struct dkwedge_info dkw;

        if (ioctl(fd, DIOCGWEDGEINFO, &dkw) != -1) {
            return dkw.dkw_size * 512;
        } else {
            struct disklabel dl;

            if (ioctl(fd, DIOCGDINFO, &dl))
                return -errno;
            return (uint64_t)dl.d_secsize *
                dl.d_partitions[DISKPART(st.st_rdev)].p_size;
        }
    } else
        return st.st_size;
}
#elif defined(__sun__)
static int64_t raw_getlength(BlockDriverState *bs)
{
    BDRVRawState *s = bs->opaque;
    struct dk_minfo minfo;
    int ret;
    int64_t size;

    ret = fd_open(bs);
    if (ret < 0) {
        return ret;
    }

    ret = ioctl(s->fd, DKIOCGMEDIAINFO, &minfo);
    if (ret != -1) {
        return minfo.dki_lbsize * minfo.dki_capacity;
    }

    size = lseek(s->fd, 0, SEEK_END);
    if (size < 0) {
        return -errno;
    }
    return size;
}
#elif defined(CONFIG_BSD)
static int64_t raw_getlength(BlockDriverState *bs)
{
    BDRVRawState *s = bs->opaque;
    int fd = s->fd;
    int64_t size;
    struct stat sb;
#if defined (__FreeBSD__) || defined(__FreeBSD_kernel__)
    int reopened = 0;
#endif
    int ret;

    ret = fd_open(bs);
    if (ret < 0)
        return ret;

#if defined (__FreeBSD__) || defined(__FreeBSD_kernel__)
again:
#endif
    if (!fstat(fd, &sb) && (S_IFCHR & sb.st_mode)) {
        size = 0;
#ifdef DIOCGMEDIASIZE
        if (ioctl(fd, DIOCGMEDIASIZE, (off_t *)&size)) {
            size = 0;
        }
#endif
#ifdef DIOCGPART
        if (size == 0) {
            struct partinfo pi;
            if (ioctl(fd, DIOCGPART, &pi) == 0) {
                size = pi.media_size;
            }
        }
#endif
#if defined(DKIOCGETBLOCKCOUNT) && defined(DKIOCGETBLOCKSIZE)
        if (size == 0) {
            uint64_t sectors = 0;
            uint32_t sector_size = 0;

            if (ioctl(fd, DKIOCGETBLOCKCOUNT, &sectors) == 0
               && ioctl(fd, DKIOCGETBLOCKSIZE, &sector_size) == 0) {
                size = sectors * sector_size;
            }
        }
#endif
        if (size == 0) {
            size = lseek(fd, 0LL, SEEK_END);
        }
        if (size < 0) {
            return -errno;
        }
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
        switch(s->type) {
        case FTYPE_CD:
            if (size == 2048LL * (unsigned)-1)
                size = 0;
            if (size <= 0 && !reopened && cdrom_reopen(bs) >= 0) {
                reopened = 1;
                goto again;
            }
        }
#endif
    } else {
        size = lseek(fd, 0, SEEK_END);
        if (size < 0) {
            return -errno;
        }
    }
    return size;
}
#else
static int64_t raw_getlength(BlockDriverState *bs)
{
    BDRVRawState *s = bs->opaque;
    int ret;
    int64_t size;

    ret = fd_open(bs);
    if (ret < 0) {
        return ret;
    }

    size = lseek(s->fd, 0, SEEK_END);
    if (size < 0) {
        return -errno;
    }
    return size;
}
#endif

static int64_t coroutine_fn raw_co_getlength(BlockDriverState *bs)
{
    return raw_getlength(bs);
}

static int64_t coroutine_fn raw_co_get_allocated_file_size(BlockDriverState *bs)
{
    struct stat st;
    BDRVRawState *s = bs->opaque;

    if (fstat(s->fd, &st) < 0) {
        return -errno;
    }
    return (int64_t)st.st_blocks * 512;
}

static int coroutine_fn
raw_co_create(BlockdevCreateOptions *options, Error **errp)
{
    BlockdevCreateOptionsFile *file_opts;
    Error *local_err = NULL;
    int fd;
    uint64_t perm, shared;
    int result = 0;

    assert(options->driver == BLOCKDEV_DRIVER_FILE);
    file_opts = &options->u.file;

    if (!file_opts->has_nocow) {
        file_opts->nocow = false;
    }
    if (!file_opts->has_preallocation) {
        file_opts->preallocation = PREALLOC_MODE_OFF;
    }
    if (!file_opts->has_extent_size_hint) {
        file_opts->extent_size_hint = 1 * MiB;
    }
    if (file_opts->extent_size_hint > UINT32_MAX) {
        result = -EINVAL;
        error_setg(errp, "Extent size hint is too large");
        goto out;
    }

    fd = qemu_create(file_opts->filename, O_RDWR | O_BINARY, 0644, errp);
    if (fd < 0) {
        result = -errno;
        goto out;
    }

    perm = BLK_PERM_WRITE | BLK_PERM_RESIZE;
    shared = BLK_PERM_ALL & ~BLK_PERM_RESIZE;

    result = raw_apply_lock_bytes(NULL, fd, perm, ~shared, false, errp);
    if (result < 0) {
        goto out_close;
    }

    result = raw_check_lock_bytes(fd, perm, shared, errp);
    if (result < 0) {
        error_append_hint(errp,
                          "Is another process using the image [%s]?\n",
                          file_opts->filename);
        goto out_unlock;
    }

    result = raw_regular_truncate(NULL, fd, 0, PREALLOC_MODE_OFF, errp);
    if (result < 0) {
        goto out_unlock;
    }

    if (file_opts->nocow) {
#ifdef __linux__
        int attr;
        if (ioctl(fd, FS_IOC_GETFLAGS, &attr) == 0) {
            attr |= FS_NOCOW_FL;
            ioctl(fd, FS_IOC_SETFLAGS, &attr);
        }
#endif
    }
#ifdef FS_IOC_FSSETXATTR
    {
        struct fsxattr attr;
        result = ioctl(fd, FS_IOC_FSGETXATTR, &attr);
        if (result == 0) {
            attr.fsx_xflags |= FS_XFLAG_EXTSIZE;
            attr.fsx_extsize = file_opts->extent_size_hint;
            result = ioctl(fd, FS_IOC_FSSETXATTR, &attr);
        }
        if (result < 0 && file_opts->has_extent_size_hint &&
            file_opts->extent_size_hint)
        {
            warn_report("Failed to set extent size hint: %s",
                        strerror(errno));
        }
    }
#endif

    result = raw_regular_truncate(NULL, fd, file_opts->size,
                                  file_opts->preallocation, errp);
    if (result < 0) {
        goto out_unlock;
    }

out_unlock:
    raw_apply_lock_bytes(NULL, fd, 0, 0, true, &local_err);
    if (local_err) {
        warn_report_err(local_err);
    }

out_close:
    if (qemu_close(fd) != 0 && result == 0) {
        result = -errno;
        error_setg_errno(errp, -result, "Could not close the new file");
    }
out:
    return result;
}

static int coroutine_fn GRAPH_RDLOCK
raw_co_create_opts(BlockDriver *drv, const char *filename,
                   QemuOpts *opts, Error **errp)
{
    BlockdevCreateOptions options;
    int64_t total_size = 0;
    int64_t extent_size_hint = 0;
    bool has_extent_size_hint = false;
    bool nocow = false;
    PreallocMode prealloc;
    char *buf = NULL;
    Error *local_err = NULL;

    strstart(filename, "file:", &filename);

    total_size = ROUND_UP(qemu_opt_get_size_del(opts, BLOCK_OPT_SIZE, 0),
                          BDRV_SECTOR_SIZE);
    if (qemu_opt_get(opts, BLOCK_OPT_EXTENT_SIZE_HINT)) {
        has_extent_size_hint = true;
        extent_size_hint =
            qemu_opt_get_size_del(opts, BLOCK_OPT_EXTENT_SIZE_HINT, -1);
    }
    nocow = qemu_opt_get_bool(opts, BLOCK_OPT_NOCOW, false);
    buf = qemu_opt_get_del(opts, BLOCK_OPT_PREALLOC);
    prealloc = qapi_enum_parse(&PreallocMode_lookup, buf,
                               PREALLOC_MODE_OFF, &local_err);
    g_free(buf);
    if (local_err) {
        error_propagate(errp, local_err);
        return -EINVAL;
    }

    options = (BlockdevCreateOptions) {
        .driver     = BLOCKDEV_DRIVER_FILE,
        .u.file     = {
            .filename           = (char *) filename,
            .size               = total_size,
            .has_preallocation  = true,
            .preallocation      = prealloc,
            .has_nocow          = true,
            .nocow              = nocow,
            .has_extent_size_hint = has_extent_size_hint,
            .extent_size_hint   = extent_size_hint,
        },
    };
    return raw_co_create(&options, errp);
}

static int coroutine_fn raw_co_delete_file(BlockDriverState *bs,
                                           Error **errp)
{
    struct stat st;
    int ret;

    if (!(stat(bs->filename, &st) == 0) || !S_ISREG(st.st_mode)) {
        error_setg_errno(errp, ENOENT, "%s is not a regular file",
                         bs->filename);
        return -ENOENT;
    }

    ret = unlink(bs->filename);
    if (ret < 0) {
        ret = -errno;
        error_setg_errno(errp, -ret, "Error when deleting file %s",
                         bs->filename);
    }

    return ret;
}

static int find_allocation(BlockDriverState *bs, off_t start,
                           off_t *data, off_t *hole)
{
#if defined SEEK_HOLE && defined SEEK_DATA
    BDRVRawState *s = bs->opaque;
    off_t offs;

    offs = lseek(s->fd, start, SEEK_DATA);
    if (offs < 0) {
        return -errno;          /* D3 or D4 */
    }

    if (offs < start) {
        return -EIO;
    }

    if (offs > start) {
        *hole = start;
        *data = offs;
        return 0;
    }


    offs = lseek(s->fd, start, SEEK_HOLE);
    if (offs < 0) {
        return -errno;          /* D1 and (H3 or H4) */
    }

    if (offs < start) {
        return -EIO;
    }

    if (offs > start) {
        *data = start;
        *hole = offs;
        return 0;
    }

    return -EBUSY;
#else
    return -ENOTSUP;
#endif
}

static int coroutine_fn raw_co_block_status(BlockDriverState *bs,
                                            bool want_zero,
                                            int64_t offset,
                                            int64_t bytes, int64_t *pnum,
                                            int64_t *map,
                                            BlockDriverState **file)
{
    off_t data = 0, hole = 0;
    int ret;

    assert(QEMU_IS_ALIGNED(offset | bytes, bs->bl.request_alignment));

    ret = fd_open(bs);
    if (ret < 0) {
        return ret;
    }

    if (!want_zero) {
        *pnum = bytes;
        *map = offset;
        *file = bs;
        return BDRV_BLOCK_DATA | BDRV_BLOCK_OFFSET_VALID;
    }

    ret = find_allocation(bs, offset, &data, &hole);
    if (ret == -ENXIO) {
        *pnum = bytes;
        ret = BDRV_BLOCK_ZERO;
    } else if (ret < 0) {
        *pnum = bytes;
        ret = BDRV_BLOCK_DATA;
    } else if (data == offset) {
        *pnum = hole - offset;

        if (!QEMU_IS_ALIGNED(*pnum, bs->bl.request_alignment)) {
            int64_t file_length = raw_getlength(bs);
            if (file_length > 0) {
                assert(hole == file_length);
            }
            *pnum = ROUND_UP(*pnum, bs->bl.request_alignment);
        }

        ret = BDRV_BLOCK_DATA;
    } else {
        assert(hole == offset);
        *pnum = data - offset;
        ret = BDRV_BLOCK_ZERO;
    }
    *map = offset;
    *file = bs;
    return ret | BDRV_BLOCK_OFFSET_VALID;
}

#if defined(__linux__)
static void check_cache_dropped(BlockDriverState *bs, Error **errp)
{
    const size_t window_size = 128 * 1024 * 1024;
    BDRVRawState *s = bs->opaque;
    void *window = NULL;
    size_t length = 0;
    unsigned char *vec;
    size_t page_size;
    off_t offset;
    off_t end;

    page_size = sysconf(_SC_PAGESIZE);
    vec = g_malloc(DIV_ROUND_UP(window_size, page_size));

    end = raw_getlength(bs);

    for (offset = 0; offset < end; offset += window_size) {
        void *new_window;
        size_t new_length;
        size_t vec_end;
        size_t i;
        int ret;

        new_length = MIN(end - offset, window_size);
        if (new_length != length) {
            munmap(window, length);
            window = NULL;
            length = 0;
        }

        new_window = mmap(window, new_length, PROT_NONE, MAP_PRIVATE,
                          s->fd, offset);
        if (new_window == MAP_FAILED) {
            error_setg_errno(errp, errno, "mmap failed");
            break;
        }

        window = new_window;
        length = new_length;

        ret = mincore(window, length, vec);
        if (ret < 0) {
            error_setg_errno(errp, errno, "mincore failed");
            break;
        }

        vec_end = DIV_ROUND_UP(length, page_size);
        for (i = 0; i < vec_end; i++) {
            if (vec[i] & 0x1) {
                break;
            }
        }
        if (i < vec_end) {
            error_setg(errp, "page cache still in use!");
            break;
        }
    }

    if (window) {
        munmap(window, length);
    }

    g_free(vec);
}
#endif /* __linux__ */

static void coroutine_fn GRAPH_RDLOCK
raw_co_invalidate_cache(BlockDriverState *bs, Error **errp)
{
    BDRVRawState *s = bs->opaque;
    int ret;

    ret = fd_open(bs);
    if (ret < 0) {
        error_setg_errno(errp, -ret, "The file descriptor is not open");
        return;
    }

    if (!s->drop_cache) {
        return;
    }

    if (s->open_flags & O_DIRECT) {
        return; /* No host kernel page cache */
    }

#if defined(__linux__)
    ret = bdrv_co_flush(bs);
    if (ret < 0) {
        error_setg_errno(errp, -ret, "flush failed");
        return;
    }

    ret = posix_fadvise(s->fd, 0, 0, POSIX_FADV_DONTNEED);
    if (ret != 0) { /* the return value is a positive errno */
        error_setg_errno(errp, ret, "fadvise failed");
        return;
    }

    if (s->check_cache_dropped) {
        check_cache_dropped(bs, errp);
    }
#else /* __linux__ */
#endif /* !__linux__ */
}

static void raw_account_discard(BDRVRawState *s, uint64_t nbytes, int ret)
{
    if (ret) {
        s->stats.discard_nb_failed++;
    } else {
        s->stats.discard_nb_ok++;
        s->stats.discard_bytes_ok += nbytes;
    }
}

#if defined(CONFIG_BLKZONED)
static int coroutine_fn raw_co_zone_report(BlockDriverState *bs, int64_t offset,
                                           unsigned int *nr_zones,
                                           BlockZoneDescriptor *zones) {
    BDRVRawState *s = bs->opaque;
    RawPosixAIOData acb = (RawPosixAIOData) {
        .bs         = bs,
        .aio_fildes = s->fd,
        .aio_type   = QEMU_AIO_ZONE_REPORT,
        .aio_offset = offset,
        .zone_report    = {
            .nr_zones       = nr_zones,
            .zones          = zones,
        },
    };

    trace_zbd_zone_report(bs, *nr_zones, offset >> BDRV_SECTOR_BITS);
    return raw_thread_pool_submit(handle_aiocb_zone_report, &acb);
}
#endif

#if defined(CONFIG_BLKZONED)
static int coroutine_fn raw_co_zone_mgmt(BlockDriverState *bs, BlockZoneOp op,
        int64_t offset, int64_t len) {
    BDRVRawState *s = bs->opaque;
    RawPosixAIOData acb;
    int64_t zone_size, zone_size_mask;
    const char *op_name;
    unsigned long zo;
    int ret;
    BlockZoneWps *wps = bs->wps;
    int64_t capacity = bs->total_sectors << BDRV_SECTOR_BITS;

    zone_size = bs->bl.zone_size;
    zone_size_mask = zone_size - 1;
    if (offset & zone_size_mask) {
        error_report("sector offset %" PRId64 " is not aligned to zone size "
                     "%" PRId64 "", offset / 512, zone_size / 512);
        return -EINVAL;
    }

    if (((offset + len) < capacity && len & zone_size_mask) ||
        offset + len > capacity) {
        error_report("number of sectors %" PRId64 " is not aligned to zone size"
                      " %" PRId64 "", len / 512, zone_size / 512);
        return -EINVAL;
    }

    uint32_t i = offset / bs->bl.zone_size;
    uint32_t nrz = len / bs->bl.zone_size;
    uint64_t *wp = &wps->wp[i];
    if (BDRV_ZT_IS_CONV(*wp) && len != capacity) {
        error_report("zone mgmt operations are not allowed for conventional zones");
        return -EIO;
    }

    switch (op) {
    case BLK_ZO_OPEN:
        op_name = "BLKOPENZONE";
        zo = BLKOPENZONE;
        break;
    case BLK_ZO_CLOSE:
        op_name = "BLKCLOSEZONE";
        zo = BLKCLOSEZONE;
        break;
    case BLK_ZO_FINISH:
        op_name = "BLKFINISHZONE";
        zo = BLKFINISHZONE;
        break;
    case BLK_ZO_RESET:
        op_name = "BLKRESETZONE";
        zo = BLKRESETZONE;
        break;
    default:
        error_report("Unsupported zone op: 0x%x", op);
        return -ENOTSUP;
    }

    acb = (RawPosixAIOData) {
        .bs             = bs,
        .aio_fildes     = s->fd,
        .aio_type       = QEMU_AIO_ZONE_MGMT,
        .aio_offset     = offset,
        .aio_nbytes     = len,
        .zone_mgmt  = {
            .op = zo,
        },
    };

    trace_zbd_zone_mgmt(bs, op_name, offset >> BDRV_SECTOR_BITS,
                        len >> BDRV_SECTOR_BITS);
    ret = raw_thread_pool_submit(handle_aiocb_zone_mgmt, &acb);
    if (ret != 0) {
        update_zones_wp(bs, s->fd, offset, nrz);
        error_report("ioctl %s failed %d", op_name, ret);
        return ret;
    }

    if (zo == BLKRESETZONE && len == capacity) {
        ret = get_zones_wp(bs, s->fd, 0, bs->bl.nr_zones, 1);
        if (ret < 0) {
            error_report("reporting single wp failed");
            return ret;
        }
    } else if (zo == BLKRESETZONE) {
        for (unsigned int j = 0; j < nrz; ++j) {
            wp[j] = offset + j * zone_size;
        }
    } else if (zo == BLKFINISHZONE) {
        for (unsigned int j = 0; j < nrz; ++j) {
            wp[j] = MIN(offset + (j + 1) * zone_size, offset + len);
        }
    }

    return ret;
}
#endif

#if defined(CONFIG_BLKZONED)
static int coroutine_fn raw_co_zone_append(BlockDriverState *bs,
                                           int64_t *offset,
                                           QEMUIOVector *qiov,
                                           BdrvRequestFlags flags) {
    assert(flags == 0);
    int64_t zone_size_mask = bs->bl.zone_size - 1;
    int64_t iov_len = 0;
    int64_t len = 0;

    if (*offset & zone_size_mask) {
        error_report("sector offset %" PRId64 " is not aligned to zone size "
                     "%" PRId32 "", *offset / 512, bs->bl.zone_size / 512);
        return -EINVAL;
    }

    int64_t wg = bs->bl.write_granularity;
    int64_t wg_mask = wg - 1;
    for (int i = 0; i < qiov->niov; i++) {
        iov_len = qiov->iov[i].iov_len;
        if (iov_len & wg_mask) {
            error_report("len of IOVector[%d] %" PRId64 " is not aligned to "
                         "block size %" PRId64 "", i, iov_len, wg);
            return -EINVAL;
        }
        len += iov_len;
    }

    trace_zbd_zone_append(bs, *offset >> BDRV_SECTOR_BITS);
    return raw_co_prw(bs, offset, len, qiov, QEMU_AIO_ZONE_APPEND, 0);
}
#endif

static coroutine_fn int
raw_do_pdiscard(BlockDriverState *bs, int64_t offset, int64_t bytes,
                bool blkdev)
{
    BDRVRawState *s = bs->opaque;
    RawPosixAIOData acb;
    int ret;

    acb = (RawPosixAIOData) {
        .bs             = bs,
        .aio_fildes     = s->fd,
        .aio_type       = QEMU_AIO_DISCARD,
        .aio_offset     = offset,
        .aio_nbytes     = bytes,
    };

    if (blkdev) {
        acb.aio_type |= QEMU_AIO_BLKDEV;
    }

    ret = raw_thread_pool_submit(handle_aiocb_discard, &acb);
    raw_account_discard(s, bytes, ret);
    return ret;
}

static coroutine_fn int
raw_co_pdiscard(BlockDriverState *bs, int64_t offset, int64_t bytes)
{
    return raw_do_pdiscard(bs, offset, bytes, false);
}

static int coroutine_fn
raw_do_pwrite_zeroes(BlockDriverState *bs, int64_t offset, int64_t bytes,
                     BdrvRequestFlags flags, bool blkdev)
{
    BDRVRawState *s = bs->opaque;
    RawPosixAIOData acb;
    ThreadPoolFunc *handler;

#ifdef CONFIG_FALLOCATE
    if (offset + bytes > bs->total_sectors * BDRV_SECTOR_SIZE) {
        BdrvTrackedRequest *req;


        req = bdrv_co_get_self_request(bs);
        assert(req);
        assert(req->type == BDRV_TRACKED_WRITE);
        assert(req->offset <= offset);
        assert(req->offset + req->bytes >= offset + bytes);

        req->bytes = BDRV_MAX_LENGTH - req->offset;

        bdrv_check_request(req->offset, req->bytes, &error_abort);

        bdrv_make_request_serialising(req, bs->bl.request_alignment);
    }
#endif

    acb = (RawPosixAIOData) {
        .bs             = bs,
        .aio_fildes     = s->fd,
        .aio_type       = QEMU_AIO_WRITE_ZEROES,
        .aio_offset     = offset,
        .aio_nbytes     = bytes,
    };

    if (blkdev) {
        acb.aio_type |= QEMU_AIO_BLKDEV;
    }
    if (flags & BDRV_REQ_NO_FALLBACK) {
        acb.aio_type |= QEMU_AIO_NO_FALLBACK;
    }

    if (flags & BDRV_REQ_MAY_UNMAP) {
        acb.aio_type |= QEMU_AIO_DISCARD;
        handler = handle_aiocb_write_zeroes_unmap;
    } else {
        handler = handle_aiocb_write_zeroes;
    }

    return raw_thread_pool_submit(handler, &acb);
}

static int coroutine_fn raw_co_pwrite_zeroes(
    BlockDriverState *bs, int64_t offset,
    int64_t bytes, BdrvRequestFlags flags)
{
    return raw_do_pwrite_zeroes(bs, offset, bytes, flags, false);
}

static int coroutine_fn
raw_co_get_info(BlockDriverState *bs, BlockDriverInfo *bdi)
{
    return 0;
}

static ImageInfoSpecific *raw_get_specific_info(BlockDriverState *bs,
                                                Error **errp)
{
    ImageInfoSpecificFile *file_info = g_new0(ImageInfoSpecificFile, 1);
    ImageInfoSpecific *spec_info = g_new(ImageInfoSpecific, 1);

    *spec_info = (ImageInfoSpecific){
        .type = IMAGE_INFO_SPECIFIC_KIND_FILE,
        .u.file.data = file_info,
    };

#ifdef FS_IOC_FSGETXATTR
    {
        BDRVRawState *s = bs->opaque;
        struct fsxattr attr;
        int ret;

        ret = ioctl(s->fd, FS_IOC_FSGETXATTR, &attr);
        if (!ret && attr.fsx_extsize != 0) {
            file_info->has_extent_size_hint = true;
            file_info->extent_size_hint = attr.fsx_extsize;
        }
    }
#endif

    return spec_info;
}

static BlockStatsSpecificFile get_blockstats_specific_file(BlockDriverState *bs)
{
    BDRVRawState *s = bs->opaque;
    return (BlockStatsSpecificFile) {
        .discard_nb_ok = s->stats.discard_nb_ok,
        .discard_nb_failed = s->stats.discard_nb_failed,
        .discard_bytes_ok = s->stats.discard_bytes_ok,
    };
}

static BlockStatsSpecific *raw_get_specific_stats(BlockDriverState *bs)
{
    BlockStatsSpecific *stats = g_new(BlockStatsSpecific, 1);

    stats->driver = BLOCKDEV_DRIVER_FILE;
    stats->u.file = get_blockstats_specific_file(bs);

    return stats;
}

#if defined(HAVE_HOST_BLOCK_DEVICE)
static BlockStatsSpecific *hdev_get_specific_stats(BlockDriverState *bs)
{
    BlockStatsSpecific *stats = g_new(BlockStatsSpecific, 1);

    stats->driver = BLOCKDEV_DRIVER_HOST_DEVICE;
    stats->u.host_device = get_blockstats_specific_file(bs);

    return stats;
}
#endif /* HAVE_HOST_BLOCK_DEVICE */

static QemuOptsList raw_create_opts = {
    .name = "raw-create-opts",
    .head = QTAILQ_HEAD_INITIALIZER(raw_create_opts.head),
    .desc = {
        {
            .name = BLOCK_OPT_SIZE,
            .type = QEMU_OPT_SIZE,
            .help = "Virtual disk size"
        },
        {
            .name = BLOCK_OPT_NOCOW,
            .type = QEMU_OPT_BOOL,
            .help = "Turn off copy-on-write (valid only on btrfs)"
        },
        {
            .name = BLOCK_OPT_PREALLOC,
            .type = QEMU_OPT_STRING,
            .help = "Preallocation mode (allowed values: off"
#ifdef CONFIG_POSIX_FALLOCATE
                    ", falloc"
#endif
                    ", full)"
        },
        {
            .name = BLOCK_OPT_EXTENT_SIZE_HINT,
            .type = QEMU_OPT_SIZE,
            .help = "Extent size hint for the image file, 0 to disable"
        },
        { /* end of list */ }
    }
};

static int raw_check_perm(BlockDriverState *bs, uint64_t perm, uint64_t shared,
                          Error **errp)
{
    BDRVRawState *s = bs->opaque;
    int input_flags = s->reopen_state ? s->reopen_state->flags : bs->open_flags;
    int open_flags;
    int ret;

    ret = raw_reconfigure_getfd(bs, input_flags, &open_flags, perm, errp);
    if (ret < 0) {
        return ret;
    } else if (ret != s->fd) {
        Error *local_err = NULL;

        raw_probe_alignment(bs, ret, &local_err);
        if (local_err) {
            error_propagate(errp, local_err);
            return -EINVAL;
        }

        s->perm_change_fd = ret;
        s->perm_change_flags = open_flags;
    }

    ret = raw_handle_perm_lock(bs, RAW_PL_PREPARE, perm, shared, errp);
    if (ret < 0) {
        goto fail;
    }

    if (s->perm_change_fd && s->use_lock) {
        ret = raw_apply_lock_bytes(NULL, s->perm_change_fd, perm, ~shared,
                                   false, errp);
        if (ret < 0) {
            raw_handle_perm_lock(bs, RAW_PL_ABORT, 0, 0, NULL);
            goto fail;
        }
    }
    return 0;

fail:
    if (s->perm_change_fd) {
        qemu_close(s->perm_change_fd);
    }
    s->perm_change_fd = 0;
    return ret;
}

static void raw_set_perm(BlockDriverState *bs, uint64_t perm, uint64_t shared)
{
    BDRVRawState *s = bs->opaque;

    if (s->perm_change_fd && s->fd != s->perm_change_fd) {
        qemu_close(s->fd);
        s->fd = s->perm_change_fd;
        s->open_flags = s->perm_change_flags;
    }
    s->perm_change_fd = 0;

    raw_handle_perm_lock(bs, RAW_PL_COMMIT, perm, shared, NULL);
    s->perm = perm;
    s->shared_perm = shared;
}

static void raw_abort_perm_update(BlockDriverState *bs)
{
    BDRVRawState *s = bs->opaque;

    if (s->perm_change_fd) {
        qemu_close(s->perm_change_fd);
    }
    s->perm_change_fd = 0;

    raw_handle_perm_lock(bs, RAW_PL_ABORT, 0, 0, NULL);
}

static int coroutine_fn GRAPH_RDLOCK raw_co_copy_range_from(
        BlockDriverState *bs, BdrvChild *src, int64_t src_offset,
        BdrvChild *dst, int64_t dst_offset, int64_t bytes,
        BdrvRequestFlags read_flags, BdrvRequestFlags write_flags)
{
    return bdrv_co_copy_range_to(src, src_offset, dst, dst_offset, bytes,
                                 read_flags, write_flags);
}

static int coroutine_fn GRAPH_RDLOCK
raw_co_copy_range_to(BlockDriverState *bs,
                     BdrvChild *src, int64_t src_offset,
                     BdrvChild *dst, int64_t dst_offset,
                     int64_t bytes, BdrvRequestFlags read_flags,
                     BdrvRequestFlags write_flags)
{
    RawPosixAIOData acb;
    BDRVRawState *s = bs->opaque;
    BDRVRawState *src_s;

    assert(dst->bs == bs);
    if (src->bs->drv->bdrv_co_copy_range_to != raw_co_copy_range_to) {
        return -ENOTSUP;
    }

    src_s = src->bs->opaque;
    if (fd_open(src->bs) < 0 || fd_open(dst->bs) < 0) {
        return -EIO;
    }

    acb = (RawPosixAIOData) {
        .bs             = bs,
        .aio_type       = QEMU_AIO_COPY_RANGE,
        .aio_fildes     = src_s->fd,
        .aio_offset     = src_offset,
        .aio_nbytes     = bytes,
        .copy_range     = {
            .aio_fd2        = s->fd,
            .aio_offset2    = dst_offset,
        },
    };

    return raw_thread_pool_submit(handle_aiocb_copy_range, &acb);
}

BlockDriver bdrv_file = {
    .format_name = "file",
    .protocol_name = "file",
    .instance_size = sizeof(BDRVRawState),
    .bdrv_needs_filename = true,
    .bdrv_probe = NULL, /* no probe for protocols */
    .bdrv_parse_filename = raw_parse_filename,
    .bdrv_open      = raw_open,
    .bdrv_reopen_prepare = raw_reopen_prepare,
    .bdrv_reopen_commit = raw_reopen_commit,
    .bdrv_reopen_abort = raw_reopen_abort,
    .bdrv_close = raw_close,
    .bdrv_co_create = raw_co_create,
    .bdrv_co_create_opts = raw_co_create_opts,
    .bdrv_has_zero_init = bdrv_has_zero_init_1,
    .bdrv_co_block_status = raw_co_block_status,
    .bdrv_co_invalidate_cache = raw_co_invalidate_cache,
    .bdrv_co_pwrite_zeroes = raw_co_pwrite_zeroes,
    .bdrv_co_delete_file = raw_co_delete_file,

    .bdrv_co_preadv         = raw_co_preadv,
    .bdrv_co_pwritev        = raw_co_pwritev,
    .bdrv_co_flush_to_disk  = raw_co_flush_to_disk,
    .bdrv_co_pdiscard       = raw_co_pdiscard,
    .bdrv_co_copy_range_from = raw_co_copy_range_from,
    .bdrv_co_copy_range_to  = raw_co_copy_range_to,
    .bdrv_refresh_limits = raw_refresh_limits,

    .bdrv_co_truncate                   = raw_co_truncate,
    .bdrv_co_getlength                  = raw_co_getlength,
    .bdrv_co_get_info                   = raw_co_get_info,
    .bdrv_get_specific_info             = raw_get_specific_info,
    .bdrv_co_get_allocated_file_size    = raw_co_get_allocated_file_size,
    .bdrv_get_specific_stats = raw_get_specific_stats,
    .bdrv_check_perm = raw_check_perm,
    .bdrv_set_perm   = raw_set_perm,
    .bdrv_abort_perm_update = raw_abort_perm_update,
    .create_opts = &raw_create_opts,
    .mutable_opts = mutable_opts,
};


#if defined(HAVE_HOST_BLOCK_DEVICE)

#if defined(__APPLE__) && defined(__MACH__)
static kern_return_t GetBSDPath(io_iterator_t mediaIterator, char *bsdPath,
                                CFIndex maxPathSize, int flags);

static char *FindEjectableOpticalMedia(io_iterator_t *mediaIterator)
{
    kern_return_t kernResult = KERN_FAILURE;
    mach_port_t mainPort;
    CFMutableDictionaryRef  classesToMatch;
    const char *matching_array[] = {kIODVDMediaClass, kIOCDMediaClass};
    char *mediaType = NULL;

    kernResult = IOMainPort(MACH_PORT_NULL, &mainPort);
    if ( KERN_SUCCESS != kernResult ) {
        printf("IOMainPort returned %d\n", kernResult);
    }

    int index;
    for (index = 0; index < ARRAY_SIZE(matching_array); index++) {
        classesToMatch = IOServiceMatching(matching_array[index]);
        if (classesToMatch == NULL) {
            error_report("IOServiceMatching returned NULL for %s",
                         matching_array[index]);
            continue;
        }
        CFDictionarySetValue(classesToMatch, CFSTR(kIOMediaEjectableKey),
                             kCFBooleanTrue);
        kernResult = IOServiceGetMatchingServices(mainPort, classesToMatch,
                                                  mediaIterator);
        if (kernResult != KERN_SUCCESS) {
            error_report("Note: IOServiceGetMatchingServices returned %d",
                         kernResult);
            continue;
        }

        if (*mediaIterator != 0) {
            trace_file_FindEjectableOpticalMedia(matching_array[index]);
            mediaType = g_strdup(matching_array[index]);
            break;
        }
    }
    return mediaType;
}

kern_return_t GetBSDPath(io_iterator_t mediaIterator, char *bsdPath,
                         CFIndex maxPathSize, int flags)
{
    io_object_t     nextMedia;
    kern_return_t   kernResult = KERN_FAILURE;
    *bsdPath = '\0';
    nextMedia = IOIteratorNext( mediaIterator );
    if ( nextMedia )
    {
        CFTypeRef   bsdPathAsCFString;
    bsdPathAsCFString = IORegistryEntryCreateCFProperty( nextMedia, CFSTR( kIOBSDNameKey ), kCFAllocatorDefault, 0 );
        if ( bsdPathAsCFString ) {
            size_t devPathLength;
            strcpy( bsdPath, _PATH_DEV );
            if (flags & BDRV_O_NOCACHE) {
                strcat(bsdPath, "r");
            }
            devPathLength = strlen( bsdPath );
            if ( CFStringGetCString( bsdPathAsCFString, bsdPath + devPathLength, maxPathSize - devPathLength, kCFStringEncodingASCII ) ) {
                kernResult = KERN_SUCCESS;
            }
            CFRelease( bsdPathAsCFString );
        }
        IOObjectRelease( nextMedia );
    }

    return kernResult;
}

static bool setup_cdrom(char *bsd_path, Error **errp)
{
    int index, num_of_test_partitions = 2, fd;
    char test_partition[MAXPATHLEN];
    bool partition_found = false;

    for (index = 0; index < num_of_test_partitions; index++) {
        snprintf(test_partition, sizeof(test_partition), "%ss%d", bsd_path,
                 index);
        fd = qemu_open(test_partition, O_RDONLY | O_BINARY | O_LARGEFILE, NULL);
        if (fd >= 0) {
            partition_found = true;
            qemu_close(fd);
            break;
        }
    }

    if (partition_found == false) {
        error_setg(errp, "Failed to find a working partition on disc");
    } else {
        trace_file_setup_cdrom(test_partition);
        pstrcpy(bsd_path, MAXPATHLEN, test_partition);
    }
    return partition_found;
}

static void print_unmounting_directions(const char *file_name)
{
    error_report("If device %s is mounted on the desktop, unmount"
                 " it first before using it in QEMU", file_name);
    error_report("Command to unmount device: diskutil unmountDisk %s",
                 file_name);
    error_report("Command to mount device: diskutil mountDisk %s", file_name);
}

#endif /* defined(__APPLE__) && defined(__MACH__) */

static int hdev_probe_device(const char *filename)
{
    struct stat st;

    if (strstart(filename, "/dev/cdrom", NULL))
        return 50;

    if (stat(filename, &st) >= 0 &&
            (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode))) {
        return 100;
    }

    return 0;
}

static void hdev_parse_filename(const char *filename, QDict *options,
                                Error **errp)
{
    bdrv_parse_filename_strip_prefix(filename, "host_device:", options);
}

static bool hdev_is_sg(BlockDriverState *bs)
{

#if defined(__linux__)

    BDRVRawState *s = bs->opaque;
    struct stat st;
    struct sg_scsi_id scsiid;
    int sg_version;
    int ret;

    if (stat(bs->filename, &st) < 0 || !S_ISCHR(st.st_mode)) {
        return false;
    }

    ret = ioctl(s->fd, SG_GET_VERSION_NUM, &sg_version);
    if (ret < 0) {
        return false;
    }

    ret = ioctl(s->fd, SG_GET_SCSI_ID, &scsiid);
    if (ret >= 0) {
        trace_file_hdev_is_sg(scsiid.scsi_type, sg_version);
        return true;
    }

#endif

    return false;
}

static int hdev_open(BlockDriverState *bs, QDict *options, int flags,
                     Error **errp)
{
    BDRVRawState *s = bs->opaque;
    int ret;

#if defined(__APPLE__) && defined(__MACH__)
    const char *filename = qdict_get_str(options, "filename");
    char bsd_path[MAXPATHLEN] = "";
    bool error_occurred = false;

    if (strcmp(filename, "/dev/cdrom") == 0) {
        char *mediaType = NULL;
        kern_return_t ret_val;
        io_iterator_t mediaIterator = 0;

        mediaType = FindEjectableOpticalMedia(&mediaIterator);
        if (mediaType == NULL) {
            error_setg(errp, "Please make sure your CD/DVD is in the optical"
                       " drive");
            error_occurred = true;
            goto hdev_open_Mac_error;
        }

        ret_val = GetBSDPath(mediaIterator, bsd_path, sizeof(bsd_path), flags);
        if (ret_val != KERN_SUCCESS) {
            error_setg(errp, "Could not get BSD path for optical drive");
            error_occurred = true;
            goto hdev_open_Mac_error;
        }

        if (bsd_path[0] == '\0') {
            error_setg(errp, "Failed to obtain bsd path for optical drive");
            error_occurred = true;
            goto hdev_open_Mac_error;
        }

        if (strncmp(mediaType, kIOCDMediaClass, 9) == 0 &&
            setup_cdrom(bsd_path, errp) == false) {
            print_unmounting_directions(bsd_path);
            error_occurred = true;
            goto hdev_open_Mac_error;
        }

        qdict_put_str(options, "filename", bsd_path);

hdev_open_Mac_error:
        g_free(mediaType);
        if (mediaIterator) {
            IOObjectRelease(mediaIterator);
        }
        if (error_occurred) {
            return -ENOENT;
        }
    }
#endif /* defined(__APPLE__) && defined(__MACH__) */

    s->type = FTYPE_FILE;

    ret = raw_open_common(bs, options, flags, 0, true, errp);
    if (ret < 0) {
#if defined(__APPLE__) && defined(__MACH__)
        if (*bsd_path) {
            filename = bsd_path;
        }
        if (strncmp(filename, "/dev/", 5) == 0) {
            print_unmounting_directions(filename);
        }
#endif /* defined(__APPLE__) && defined(__MACH__) */
        return ret;
    }

    bs->sg = hdev_is_sg(bs);

    return ret;
}

#if defined(__linux__)
static int coroutine_fn
hdev_co_ioctl(BlockDriverState *bs, unsigned long int req, void *buf)
{
    BDRVRawState *s = bs->opaque;
    RawPosixAIOData acb;
    int ret;

    ret = fd_open(bs);
    if (ret < 0) {
        return ret;
    }

    acb = (RawPosixAIOData) {
        .bs         = bs,
        .aio_type   = QEMU_AIO_IOCTL,
        .aio_fildes = s->fd,
        .aio_offset = 0,
        .ioctl      = {
            .buf        = buf,
            .cmd        = req,
        },
    };

    return raw_thread_pool_submit(handle_aiocb_ioctl, &acb);
}
#endif /* linux */

static coroutine_fn int
hdev_co_pdiscard(BlockDriverState *bs, int64_t offset, int64_t bytes)
{
    BDRVRawState *s = bs->opaque;
    int ret;

    ret = fd_open(bs);
    if (ret < 0) {
        raw_account_discard(s, bytes, ret);
        return ret;
    }
    return raw_do_pdiscard(bs, offset, bytes, true);
}

static coroutine_fn int hdev_co_pwrite_zeroes(BlockDriverState *bs,
    int64_t offset, int64_t bytes, BdrvRequestFlags flags)
{
    int rc;

    rc = fd_open(bs);
    if (rc < 0) {
        return rc;
    }

    return raw_do_pwrite_zeroes(bs, offset, bytes, flags, true);
}

static BlockDriver bdrv_host_device = {
    .format_name        = "host_device",
    .protocol_name        = "host_device",
    .instance_size      = sizeof(BDRVRawState),
    .bdrv_needs_filename = true,
    .bdrv_probe_device  = hdev_probe_device,
    .bdrv_parse_filename = hdev_parse_filename,
    .bdrv_open          = hdev_open,
    .bdrv_close         = raw_close,
    .bdrv_reopen_prepare = raw_reopen_prepare,
    .bdrv_reopen_commit  = raw_reopen_commit,
    .bdrv_reopen_abort   = raw_reopen_abort,
    .bdrv_co_create_opts = bdrv_co_create_opts_simple,
    .create_opts         = &bdrv_create_opts_simple,
    .mutable_opts        = mutable_opts,
    .bdrv_co_invalidate_cache = raw_co_invalidate_cache,
    .bdrv_co_pwrite_zeroes = hdev_co_pwrite_zeroes,

    .bdrv_co_preadv         = raw_co_preadv,
    .bdrv_co_pwritev        = raw_co_pwritev,
    .bdrv_co_flush_to_disk  = raw_co_flush_to_disk,
    .bdrv_co_pdiscard       = hdev_co_pdiscard,
    .bdrv_co_copy_range_from = raw_co_copy_range_from,
    .bdrv_co_copy_range_to  = raw_co_copy_range_to,
    .bdrv_refresh_limits = raw_refresh_limits,

    .bdrv_co_truncate                   = raw_co_truncate,
    .bdrv_co_getlength                  = raw_co_getlength,
    .bdrv_co_get_info                   = raw_co_get_info,
    .bdrv_get_specific_info             = raw_get_specific_info,
    .bdrv_co_get_allocated_file_size    = raw_co_get_allocated_file_size,
    .bdrv_get_specific_stats = hdev_get_specific_stats,
    .bdrv_check_perm = raw_check_perm,
    .bdrv_set_perm   = raw_set_perm,
    .bdrv_abort_perm_update = raw_abort_perm_update,
    .bdrv_probe_blocksizes = hdev_probe_blocksizes,
    .bdrv_probe_geometry = hdev_probe_geometry,

#ifdef __linux__
    .bdrv_co_ioctl          = hdev_co_ioctl,
#endif

#if defined(CONFIG_BLKZONED)
    .bdrv_co_zone_report = raw_co_zone_report,
    .bdrv_co_zone_mgmt = raw_co_zone_mgmt,
    .bdrv_co_zone_append = raw_co_zone_append,
#endif
};

#if defined(__linux__) || defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
static void cdrom_parse_filename(const char *filename, QDict *options,
                                 Error **errp)
{
    bdrv_parse_filename_strip_prefix(filename, "host_cdrom:", options);
}

static void cdrom_refresh_limits(BlockDriverState *bs, Error **errp)
{
    bs->bl.has_variable_length = true;
    raw_refresh_limits(bs, errp);
}
#endif

#ifdef __linux__
static int cdrom_open(BlockDriverState *bs, QDict *options, int flags,
                      Error **errp)
{
    BDRVRawState *s = bs->opaque;

    s->type = FTYPE_CD;

    return raw_open_common(bs, options, flags, O_NONBLOCK, true, errp);
}

static int cdrom_probe_device(const char *filename)
{
    int fd, ret;
    int prio = 0;
    struct stat st;

    fd = qemu_open(filename, O_RDONLY | O_NONBLOCK, NULL);
    if (fd < 0) {
        goto out;
    }
    ret = fstat(fd, &st);
    if (ret == -1 || !S_ISBLK(st.st_mode)) {
        goto outc;
    }

    ret = ioctl(fd, CDROM_DRIVE_STATUS, CDSL_CURRENT);
    if (ret >= 0)
        prio = 100;

outc:
    qemu_close(fd);
out:
    return prio;
}

static bool coroutine_fn cdrom_co_is_inserted(BlockDriverState *bs)
{
    BDRVRawState *s = bs->opaque;
    int ret;

    ret = ioctl(s->fd, CDROM_DRIVE_STATUS, CDSL_CURRENT);
    return ret == CDS_DISC_OK;
}

static void coroutine_fn cdrom_co_eject(BlockDriverState *bs, bool eject_flag)
{
    BDRVRawState *s = bs->opaque;

    if (eject_flag) {
        if (ioctl(s->fd, CDROMEJECT, NULL) < 0)
            perror("CDROMEJECT");
    } else {
        if (ioctl(s->fd, CDROMCLOSETRAY, NULL) < 0)
            perror("CDROMEJECT");
    }
}

static void coroutine_fn cdrom_co_lock_medium(BlockDriverState *bs, bool locked)
{
    BDRVRawState *s = bs->opaque;

    if (ioctl(s->fd, CDROM_LOCKDOOR, locked) < 0) {
    }
}

static BlockDriver bdrv_host_cdrom = {
    .format_name        = "host_cdrom",
    .protocol_name      = "host_cdrom",
    .instance_size      = sizeof(BDRVRawState),
    .bdrv_needs_filename = true,
    .bdrv_probe_device	= cdrom_probe_device,
    .bdrv_parse_filename = cdrom_parse_filename,
    .bdrv_open          = cdrom_open,
    .bdrv_close         = raw_close,
    .bdrv_reopen_prepare = raw_reopen_prepare,
    .bdrv_reopen_commit  = raw_reopen_commit,
    .bdrv_reopen_abort   = raw_reopen_abort,
    .bdrv_co_create_opts = bdrv_co_create_opts_simple,
    .create_opts         = &bdrv_create_opts_simple,
    .mutable_opts        = mutable_opts,
    .bdrv_co_invalidate_cache = raw_co_invalidate_cache,

    .bdrv_co_preadv         = raw_co_preadv,
    .bdrv_co_pwritev        = raw_co_pwritev,
    .bdrv_co_flush_to_disk  = raw_co_flush_to_disk,
    .bdrv_refresh_limits    = cdrom_refresh_limits,

    .bdrv_co_truncate                   = raw_co_truncate,
    .bdrv_co_getlength                  = raw_co_getlength,
    .bdrv_co_get_allocated_file_size    = raw_co_get_allocated_file_size,

    .bdrv_co_is_inserted    = cdrom_co_is_inserted,
    .bdrv_co_eject          = cdrom_co_eject,
    .bdrv_co_lock_medium    = cdrom_co_lock_medium,

    .bdrv_co_ioctl      = hdev_co_ioctl,
};
#endif /* __linux__ */

#if defined (__FreeBSD__) || defined(__FreeBSD_kernel__)
static int cdrom_open(BlockDriverState *bs, QDict *options, int flags,
                      Error **errp)
{
    BDRVRawState *s = bs->opaque;
    int ret;

    s->type = FTYPE_CD;

    ret = raw_open_common(bs, options, flags, 0, true, errp);
    if (ret) {
        return ret;
    }

    ioctl(s->fd, CDIOCALLOW);
    return 0;
}

static int cdrom_probe_device(const char *filename)
{
    if (strstart(filename, "/dev/cd", NULL) ||
            strstart(filename, "/dev/acd", NULL))
        return 100;
    return 0;
}

static int cdrom_reopen(BlockDriverState *bs)
{
    BDRVRawState *s = bs->opaque;
    int fd;

    if (s->fd >= 0)
        qemu_close(s->fd);
    fd = qemu_open(bs->filename, s->open_flags, NULL);
    if (fd < 0) {
        s->fd = -1;
        return -EIO;
    }
    s->fd = fd;

    ioctl(s->fd, CDIOCALLOW);
    return 0;
}

static bool coroutine_fn cdrom_co_is_inserted(BlockDriverState *bs)
{
    return raw_getlength(bs) > 0;
}

static void coroutine_fn cdrom_co_eject(BlockDriverState *bs, bool eject_flag)
{
    BDRVRawState *s = bs->opaque;

    if (s->fd < 0)
        return;

    (void) ioctl(s->fd, CDIOCALLOW);

    if (eject_flag) {
        if (ioctl(s->fd, CDIOCEJECT) < 0)
            perror("CDIOCEJECT");
    } else {
        if (ioctl(s->fd, CDIOCCLOSE) < 0)
            perror("CDIOCCLOSE");
    }

    cdrom_reopen(bs);
}

static void coroutine_fn cdrom_co_lock_medium(BlockDriverState *bs, bool locked)
{
    BDRVRawState *s = bs->opaque;

    if (s->fd < 0)
        return;
    if (ioctl(s->fd, (locked ? CDIOCPREVENT : CDIOCALLOW)) < 0) {
    }
}

static BlockDriver bdrv_host_cdrom = {
    .format_name        = "host_cdrom",
    .protocol_name      = "host_cdrom",
    .instance_size      = sizeof(BDRVRawState),
    .bdrv_needs_filename = true,
    .bdrv_probe_device	= cdrom_probe_device,
    .bdrv_parse_filename = cdrom_parse_filename,
    .bdrv_open          = cdrom_open,
    .bdrv_close         = raw_close,
    .bdrv_reopen_prepare = raw_reopen_prepare,
    .bdrv_reopen_commit  = raw_reopen_commit,
    .bdrv_reopen_abort   = raw_reopen_abort,
    .bdrv_co_create_opts = bdrv_co_create_opts_simple,
    .create_opts         = &bdrv_create_opts_simple,
    .mutable_opts       = mutable_opts,

    .bdrv_co_preadv         = raw_co_preadv,
    .bdrv_co_pwritev        = raw_co_pwritev,
    .bdrv_co_flush_to_disk  = raw_co_flush_to_disk,
    .bdrv_refresh_limits    = cdrom_refresh_limits,

    .bdrv_co_truncate                   = raw_co_truncate,
    .bdrv_co_getlength                  = raw_co_getlength,
    .bdrv_co_get_allocated_file_size    = raw_co_get_allocated_file_size,

    .bdrv_co_is_inserted     = cdrom_co_is_inserted,
    .bdrv_co_eject           = cdrom_co_eject,
    .bdrv_co_lock_medium     = cdrom_co_lock_medium,
};
#endif /* __FreeBSD__ */

#endif /* HAVE_HOST_BLOCK_DEVICE */

static void bdrv_file_init(void)
{
    bdrv_register(&bdrv_file);
#if defined(HAVE_HOST_BLOCK_DEVICE)
    bdrv_register(&bdrv_host_device);
#ifdef __linux__
    bdrv_register(&bdrv_host_cdrom);
#endif
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
    bdrv_register(&bdrv_host_cdrom);
#endif
#endif /* HAVE_HOST_BLOCK_DEVICE */
}

block_init(bdrv_file_init);
