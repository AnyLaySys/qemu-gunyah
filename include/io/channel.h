
#ifndef QIO_CHANNEL_H
#define QIO_CHANNEL_H

#include "qom/object.h"
#include "qemu/coroutine-core.h"
#include "block/aio.h"

#define TYPE_QIO_CHANNEL "qio-channel"
OBJECT_DECLARE_TYPE(QIOChannel, QIOChannelClass,
                    QIO_CHANNEL)


#define QIO_CHANNEL_ERR_BLOCK -2

#define QIO_CHANNEL_WRITE_FLAG_ZERO_COPY 0x1

#define QIO_CHANNEL_READ_FLAG_MSG_PEEK 0x1
#define QIO_CHANNEL_READ_FLAG_RELAXED_EOF 0x2

typedef enum QIOChannelFeature QIOChannelFeature;

enum QIOChannelFeature {
    QIO_CHANNEL_FEATURE_FD_PASS,
    QIO_CHANNEL_FEATURE_SHUTDOWN,
    QIO_CHANNEL_FEATURE_LISTEN,
    QIO_CHANNEL_FEATURE_WRITE_ZERO_COPY,
    QIO_CHANNEL_FEATURE_READ_MSG_PEEK,
    QIO_CHANNEL_FEATURE_SEEKABLE,
};


typedef enum QIOChannelShutdown QIOChannelShutdown;

enum QIOChannelShutdown {
    QIO_CHANNEL_SHUTDOWN_READ = 1,
    QIO_CHANNEL_SHUTDOWN_WRITE = 2,
    QIO_CHANNEL_SHUTDOWN_BOTH = 3,
};

typedef gboolean (*QIOChannelFunc)(QIOChannel *ioc,
                                   GIOCondition condition,
                                   gpointer data);


struct QIOChannel {
    Object parent;
    unsigned int features; /* bitmask of QIOChannelFeatures */
    char *name;
    AioContext *read_ctx;
    Coroutine *read_coroutine;
    AioContext *write_ctx;
    Coroutine *write_coroutine;
    bool follow_coroutine_ctx;
#ifdef _WIN32
    HANDLE event; /* For use with GSource on Win32 */
#endif
};

struct QIOChannelClass {
    ObjectClass parent;

    ssize_t (*io_writev)(QIOChannel *ioc,
                         const struct iovec *iov,
                         size_t niov,
                         int *fds,
                         size_t nfds,
                         int flags,
                         Error **errp);
    ssize_t (*io_readv)(QIOChannel *ioc,
                        const struct iovec *iov,
                        size_t niov,
                        int **fds,
                        size_t *nfds,
                        int flags,
                        Error **errp);
    int (*io_close)(QIOChannel *ioc,
                    Error **errp);
    GSource * (*io_create_watch)(QIOChannel *ioc,
                                 GIOCondition condition);
    int (*io_set_blocking)(QIOChannel *ioc,
                           bool enabled,
                           Error **errp);

    ssize_t (*io_pwritev)(QIOChannel *ioc,
                          const struct iovec *iov,
                          size_t niov,
                          off_t offset,
                          Error **errp);
    ssize_t (*io_preadv)(QIOChannel *ioc,
                         const struct iovec *iov,
                         size_t niov,
                         off_t offset,
                         Error **errp);
    int (*io_shutdown)(QIOChannel *ioc,
                       QIOChannelShutdown how,
                       Error **errp);
    void (*io_set_cork)(QIOChannel *ioc,
                        bool enabled);
    void (*io_set_delay)(QIOChannel *ioc,
                         bool enabled);
    off_t (*io_seek)(QIOChannel *ioc,
                     off_t offset,
                     int whence,
                     Error **errp);
    void (*io_set_aio_fd_handler)(QIOChannel *ioc,
                                  AioContext *read_ctx,
                                  IOHandler *io_read,
                                  AioContext *write_ctx,
                                  IOHandler *io_write,
                                  void *opaque);
    int (*io_flush)(QIOChannel *ioc,
                    Error **errp);
    int (*io_peerpid)(QIOChannel *ioc,
                       unsigned int *pid,
                       Error **errp);
};


bool qio_channel_has_feature(QIOChannel *ioc,
                             QIOChannelFeature feature);

void qio_channel_set_feature(QIOChannel *ioc,
                             QIOChannelFeature feature);

void qio_channel_set_name(QIOChannel *ioc,
                          const char *name);

ssize_t qio_channel_readv_full(QIOChannel *ioc,
                               const struct iovec *iov,
                               size_t niov,
                               int **fds,
                               size_t *nfds,
                               int flags,
                               Error **errp);


ssize_t qio_channel_writev_full(QIOChannel *ioc,
                                const struct iovec *iov,
                                size_t niov,
                                int *fds,
                                size_t nfds,
                                int flags,
                                Error **errp);

int coroutine_mixed_fn qio_channel_readv_all_eof(QIOChannel *ioc,
                                                 const struct iovec *iov,
                                                 size_t niov,
                                                 Error **errp);

int coroutine_mixed_fn qio_channel_readv_all(QIOChannel *ioc,
                                             const struct iovec *iov,
                                             size_t niov,
                                             Error **errp);


int coroutine_mixed_fn qio_channel_writev_all(QIOChannel *ioc,
                                              const struct iovec *iov,
                                              size_t niov,
                                              Error **errp);

ssize_t qio_channel_readv(QIOChannel *ioc,
                          const struct iovec *iov,
                          size_t niov,
                          Error **errp);

ssize_t qio_channel_writev(QIOChannel *ioc,
                           const struct iovec *iov,
                           size_t niov,
                           Error **errp);

ssize_t qio_channel_read(QIOChannel *ioc,
                         char *buf,
                         size_t buflen,
                         Error **errp);

ssize_t qio_channel_write(QIOChannel *ioc,
                          const char *buf,
                          size_t buflen,
                          Error **errp);

int coroutine_mixed_fn qio_channel_read_all_eof(QIOChannel *ioc,
                                                char *buf,
                                                size_t buflen,
                                                Error **errp);

int coroutine_mixed_fn qio_channel_read_all(QIOChannel *ioc,
                                            char *buf,
                                            size_t buflen,
                                            Error **errp);

int coroutine_mixed_fn qio_channel_write_all(QIOChannel *ioc,
                                             const char *buf,
                                             size_t buflen,
                                             Error **errp);

int qio_channel_set_blocking(QIOChannel *ioc,
                             bool enabled,
                             Error **errp);

void qio_channel_set_follow_coroutine_ctx(QIOChannel *ioc, bool enabled);

int qio_channel_close(QIOChannel *ioc,
                      Error **errp);

ssize_t qio_channel_pwritev(QIOChannel *ioc, const struct iovec *iov,
                            size_t niov, off_t offset, Error **errp);

ssize_t qio_channel_pwrite(QIOChannel *ioc, char *buf, size_t buflen,
                           off_t offset, Error **errp);

ssize_t qio_channel_preadv(QIOChannel *ioc, const struct iovec *iov,
                           size_t niov, off_t offset, Error **errp);

ssize_t qio_channel_pread(QIOChannel *ioc, char *buf, size_t buflen,
                          off_t offset, Error **errp);

int qio_channel_shutdown(QIOChannel *ioc,
                         QIOChannelShutdown how,
                         Error **errp);

void qio_channel_set_delay(QIOChannel *ioc,
                           bool enabled);

void qio_channel_set_cork(QIOChannel *ioc,
                          bool enabled);


off_t qio_channel_io_seek(QIOChannel *ioc,
                          off_t offset,
                          int whence,
                          Error **errp);


GSource *qio_channel_create_watch(QIOChannel *ioc,
                                  GIOCondition condition);

guint qio_channel_add_watch(QIOChannel *ioc,
                            GIOCondition condition,
                            QIOChannelFunc func,
                            gpointer user_data,
                            GDestroyNotify notify);

guint qio_channel_add_watch_full(QIOChannel *ioc,
                                 GIOCondition condition,
                                 QIOChannelFunc func,
                                 gpointer user_data,
                                 GDestroyNotify notify,
                                 GMainContext *context);

GSource *qio_channel_add_watch_source(QIOChannel *ioc,
                                      GIOCondition condition,
                                      QIOChannelFunc func,
                                      gpointer user_data,
                                      GDestroyNotify notify,
                                      GMainContext *context);

void coroutine_fn qio_channel_yield(QIOChannel *ioc,
                                    GIOCondition condition);

void qio_channel_wake_read(QIOChannel *ioc);

void qio_channel_wait(QIOChannel *ioc,
                      GIOCondition condition);

void qio_channel_set_aio_fd_handler(QIOChannel *ioc,
                                    AioContext *read_ctx,
                                    IOHandler *io_read,
                                    AioContext *write_ctx,
                                    IOHandler *io_write,
                                    void *opaque);


int coroutine_mixed_fn qio_channel_readv_full_all_eof(QIOChannel *ioc,
                                                      const struct iovec *iov,
                                                      size_t niov,
                                                      int **fds, size_t *nfds,
                                                      int flags,
                                                      Error **errp);


int coroutine_mixed_fn qio_channel_readv_full_all(QIOChannel *ioc,
                                                  const struct iovec *iov,
                                                  size_t niov,
                                                  int **fds, size_t *nfds,
                                                  Error **errp);


int coroutine_mixed_fn qio_channel_writev_full_all(QIOChannel *ioc,
                                                   const struct iovec *iov,
                                                   size_t niov,
                                                   int *fds, size_t nfds,
                                                   int flags, Error **errp);


int qio_channel_flush(QIOChannel *ioc,
                      Error **errp);

int qio_channel_get_peerpid(QIOChannel *ioc,
                             unsigned int *pid,
                             Error **errp);

#endif /* QIO_CHANNEL_H */
