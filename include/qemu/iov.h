
#ifndef IOV_H
#define IOV_H

size_t iov_size(const struct iovec *iov, const unsigned int iov_cnt);

size_t iov_from_buf_full(const struct iovec *iov, unsigned int iov_cnt,
                         size_t offset, const void *buf, size_t bytes);
size_t iov_to_buf_full(const struct iovec *iov, const unsigned int iov_cnt,
                       size_t offset, void *buf, size_t bytes);

static inline size_t
iov_from_buf(const struct iovec *iov, unsigned int iov_cnt,
             size_t offset, const void *buf, size_t bytes)
{
    if (__builtin_constant_p(bytes) && iov_cnt &&
        offset <= iov[0].iov_len && bytes <= iov[0].iov_len - offset) {
        memcpy(iov[0].iov_base + offset, buf, bytes);
        return bytes;
    } else {
        return iov_from_buf_full(iov, iov_cnt, offset, buf, bytes);
    }
}

static inline size_t
iov_to_buf(const struct iovec *iov, const unsigned int iov_cnt,
           size_t offset, void *buf, size_t bytes)
{
    if (__builtin_constant_p(bytes) && iov_cnt &&
        offset <= iov[0].iov_len && bytes <= iov[0].iov_len - offset) {
        memcpy(buf, iov[0].iov_base + offset, bytes);
        return bytes;
    } else {
        return iov_to_buf_full(iov, iov_cnt, offset, buf, bytes);
    }
}

size_t iov_memset(const struct iovec *iov, const unsigned int iov_cnt,
                  size_t offset, int fillc, size_t bytes);

ssize_t iov_send_recv_with_flags(int sockfd, int sockflags,
                                 const struct iovec *iov,
                                 unsigned iov_cnt, size_t offset,
                                 size_t bytes,
                                 bool do_send);

ssize_t iov_send_recv(int sockfd, const struct iovec *iov, unsigned iov_cnt,
                      size_t offset, size_t bytes, bool do_send);
#define iov_recv(sockfd, iov, iov_cnt, offset, bytes) \
  iov_send_recv(sockfd, iov, iov_cnt, offset, bytes, false)
#define iov_send(sockfd, iov, iov_cnt, offset, bytes) \
  iov_send_recv(sockfd, iov, iov_cnt, offset, bytes, true)

void iov_hexdump(const struct iovec *iov, const unsigned int iov_cnt,
                 FILE *fp, const char *prefix, size_t limit);

unsigned iov_copy(struct iovec *dst_iov, unsigned int dst_iov_cnt,
                 const struct iovec *iov, unsigned int iov_cnt,
                 size_t offset, size_t bytes);

size_t iov_discard_front(struct iovec **iov, unsigned int *iov_cnt,
                         size_t bytes);
size_t iov_discard_back(struct iovec *iov, unsigned int *iov_cnt,
                        size_t bytes);

typedef struct {
    struct iovec *modified_iov;
    struct iovec orig;
} IOVDiscardUndo;

void iov_discard_undo(IOVDiscardUndo *undo);

size_t iov_discard_front_undoable(struct iovec **iov, unsigned int *iov_cnt,
                                  size_t bytes, IOVDiscardUndo *undo);
size_t iov_discard_back_undoable(struct iovec *iov, unsigned int *iov_cnt,
                                 size_t bytes, IOVDiscardUndo *undo);

typedef struct QEMUIOVector {
    struct iovec *iov;
    int niov;

    union {
        struct {
            int nalloc;
            struct iovec local_iov;
        };
        struct {
            char __pad[sizeof(int) + offsetof(struct iovec, iov_len)];
            size_t size;
        };
    };
} QEMUIOVector;

QEMU_BUILD_BUG_ON(offsetof(QEMUIOVector, size) !=
                  offsetof(QEMUIOVector, local_iov.iov_len));

#define QEMU_IOVEC_INIT_BUF(self, buf, len)              \
{                                                        \
    .iov = &(self).local_iov,                            \
    .niov = 1,                                           \
    .nalloc = -1,                                        \
    .local_iov = {                                       \
        .iov_base = (void *)(buf), /* cast away const */ \
        .iov_len = (len),                                \
    },                                                   \
}

static inline void qemu_iovec_init_buf(QEMUIOVector *qiov,
                                       const void *buf, size_t len)
{
    *qiov = (QEMUIOVector) QEMU_IOVEC_INIT_BUF(*qiov, buf, len);
}

static inline void *qemu_iovec_buf(QEMUIOVector *qiov)
{
    assert(qiov->nalloc == -1 && qiov->iov == &qiov->local_iov);

    return qiov->local_iov.iov_base;
}

void qemu_iovec_init(QEMUIOVector *qiov, int alloc_hint);
void qemu_iovec_init_external(QEMUIOVector *qiov, struct iovec *iov, int niov);
void qemu_iovec_init_slice(QEMUIOVector *qiov, QEMUIOVector *source,
                           size_t offset, size_t len);
struct iovec *qemu_iovec_slice(QEMUIOVector *qiov,
                               size_t offset, size_t len,
                               size_t *head, size_t *tail, int *niov);
int qemu_iovec_subvec_niov(QEMUIOVector *qiov, size_t offset, size_t len);
void qemu_iovec_add(QEMUIOVector *qiov, void *base, size_t len);
void qemu_iovec_concat(QEMUIOVector *dst,
                       QEMUIOVector *src, size_t soffset, size_t sbytes);
size_t qemu_iovec_concat_iov(QEMUIOVector *dst,
                             struct iovec *src_iov, unsigned int src_cnt,
                             size_t soffset, size_t sbytes);
bool qemu_iovec_is_zero(QEMUIOVector *qiov, size_t qiov_offeset, size_t bytes);
void qemu_iovec_destroy(QEMUIOVector *qiov);
void qemu_iovec_reset(QEMUIOVector *qiov);
size_t qemu_iovec_to_buf(QEMUIOVector *qiov, size_t offset,
                         void *buf, size_t bytes);
size_t qemu_iovec_from_buf(QEMUIOVector *qiov, size_t offset,
                           const void *buf, size_t bytes);
size_t qemu_iovec_memset(QEMUIOVector *qiov, size_t offset,
                         int fillc, size_t bytes);
ssize_t qemu_iovec_compare(QEMUIOVector *a, QEMUIOVector *b);
void qemu_iovec_clone(QEMUIOVector *dest, const QEMUIOVector *src, void *buf);
void qemu_iovec_discard_back(QEMUIOVector *qiov, size_t bytes);

#endif
