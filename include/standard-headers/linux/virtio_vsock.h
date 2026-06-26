
#ifndef _LINUX_VIRTIO_VSOCK_H
#define _LINUX_VIRTIO_VSOCK_H

#include "standard-headers/linux/types.h"
#include "standard-headers/linux/virtio_ids.h"
#include "standard-headers/linux/virtio_config.h"

#define VIRTIO_VSOCK_F_SEQPACKET	1	/* SOCK_SEQPACKET supported */

struct virtio_vsock_config {
	uint64_t guest_cid;
} QEMU_PACKED;

enum virtio_vsock_event_id {
	VIRTIO_VSOCK_EVENT_TRANSPORT_RESET = 0,
};

struct virtio_vsock_event {
	uint32_t id;
} QEMU_PACKED;

struct virtio_vsock_hdr {
	uint64_t	src_cid;
	uint64_t	dst_cid;
	uint32_t	src_port;
	uint32_t	dst_port;
	uint32_t	len;
	uint16_t	type;		/* enum virtio_vsock_type */
	uint16_t	op;		/* enum virtio_vsock_op */
	uint32_t	flags;
	uint32_t	buf_alloc;
	uint32_t	fwd_cnt;
} QEMU_PACKED;

enum virtio_vsock_type {
	VIRTIO_VSOCK_TYPE_STREAM = 1,
	VIRTIO_VSOCK_TYPE_SEQPACKET = 2,
};

enum virtio_vsock_op {
	VIRTIO_VSOCK_OP_INVALID = 0,

	VIRTIO_VSOCK_OP_REQUEST = 1,
	VIRTIO_VSOCK_OP_RESPONSE = 2,
	VIRTIO_VSOCK_OP_RST = 3,
	VIRTIO_VSOCK_OP_SHUTDOWN = 4,

	VIRTIO_VSOCK_OP_RW = 5,

	VIRTIO_VSOCK_OP_CREDIT_UPDATE = 6,
	VIRTIO_VSOCK_OP_CREDIT_REQUEST = 7,
};

enum virtio_vsock_shutdown {
	VIRTIO_VSOCK_SHUTDOWN_RCV = 1,
	VIRTIO_VSOCK_SHUTDOWN_SEND = 2,
};

enum virtio_vsock_rw {
	VIRTIO_VSOCK_SEQ_EOM = 1,
	VIRTIO_VSOCK_SEQ_EOR = 2,
};

#endif /* _LINUX_VIRTIO_VSOCK_H */
