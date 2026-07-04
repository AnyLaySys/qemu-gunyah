
#ifndef QEMU_NET_QUEUE_H
#define QEMU_NET_QUEUE_H


typedef struct NetPacket NetPacket;
typedef struct NetQueue NetQueue;

typedef void (NetPacketSent) (NetClientState *sender, ssize_t ret);

#define QEMU_NET_PACKET_FLAG_NONE  0
#define QEMU_NET_PACKET_FLAG_RAW  (1<<0)

typedef ssize_t (NetQueueDeliverFunc)(NetClientState *sender,
                                      unsigned flags,
                                      const struct iovec *iov,
                                      int iovcnt,
                                      void *opaque);

NetQueue *qemu_new_net_queue(NetQueueDeliverFunc *deliver, void *opaque);

void qemu_net_queue_append_iov(NetQueue *queue,
                               NetClientState *sender,
                               unsigned flags,
                               const struct iovec *iov,
                               int iovcnt,
                               NetPacketSent *sent_cb);

void qemu_del_net_queue(NetQueue *queue);

ssize_t qemu_net_queue_receive(NetQueue *queue,
                               const uint8_t *data,
                               size_t size);

ssize_t qemu_net_queue_send(NetQueue *queue,
                            NetClientState *sender,
                            unsigned flags,
                            const uint8_t *data,
                            size_t size,
                            NetPacketSent *sent_cb);

ssize_t qemu_net_queue_send_iov(NetQueue *queue,
                                NetClientState *sender,
                                unsigned flags,
                                const struct iovec *iov,
                                int iovcnt,
                                NetPacketSent *sent_cb);

void qemu_net_queue_purge(NetQueue *queue, NetClientState *from);
bool qemu_net_queue_flush(NetQueue *queue);

#endif /* QEMU_NET_QUEUE_H */
