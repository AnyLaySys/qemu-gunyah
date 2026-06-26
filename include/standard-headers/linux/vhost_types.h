#ifndef _LINUX_VHOST_TYPES_H
#define _LINUX_VHOST_TYPES_H


#include "standard-headers/linux/types.h"

#include "standard-headers/linux/virtio_config.h"
#include "standard-headers/linux/virtio_ring.h"

struct vhost_vring_state {
	unsigned int index;
	unsigned int num;
};

struct vhost_vring_file {
	unsigned int index;
	int fd; /* Pass -1 to unbind from file. */

};

struct vhost_vring_addr {
	unsigned int index;
	unsigned int flags;
#define VHOST_VRING_F_LOG 0

	uint64_t desc_user_addr;
	uint64_t used_user_addr;
	uint64_t avail_user_addr;
	uint64_t log_guest_addr;
};

struct vhost_worker_state {
	unsigned int worker_id;
};

struct vhost_vring_worker {
	unsigned int index;
	unsigned int worker_id;
};

struct vhost_iotlb_msg {
	uint64_t iova;
	uint64_t size;
	uint64_t uaddr;
#define VHOST_ACCESS_RO      0x1
#define VHOST_ACCESS_WO      0x2
#define VHOST_ACCESS_RW      0x3
	uint8_t perm;
#define VHOST_IOTLB_MISS           1
#define VHOST_IOTLB_UPDATE         2
#define VHOST_IOTLB_INVALIDATE     3
#define VHOST_IOTLB_ACCESS_FAIL    4
#define VHOST_IOTLB_BATCH_BEGIN    5
#define VHOST_IOTLB_BATCH_END      6
	uint8_t type;
};

#define VHOST_IOTLB_MSG 0x1
#define VHOST_IOTLB_MSG_V2 0x2

struct vhost_msg {
	int type;
	union {
		struct vhost_iotlb_msg iotlb;
		uint8_t padding[64];
	};
};

struct vhost_msg_v2 {
	uint32_t type;
	uint32_t asid;
	union {
		struct vhost_iotlb_msg iotlb;
		uint8_t padding[64];
	};
};

struct vhost_memory_region {
	uint64_t guest_phys_addr;
	uint64_t memory_size; /* bytes */
	uint64_t userspace_addr;
	uint64_t flags_padding; /* No flags are currently specified. */
};

#define VHOST_PAGE_SIZE 0x1000

struct vhost_memory {
	uint32_t nregions;
	uint32_t padding;
	struct vhost_memory_region regions[];
};



#define VHOST_SCSI_ABI_VERSION	1

struct vhost_scsi_target {
	int abi_version;
	char vhost_wwpn[224]; /* TRANSPORT_IQN_LEN */
	unsigned short vhost_tpgt;
	unsigned short reserved;
};


struct vhost_vdpa_config {
	uint32_t off;
	uint32_t len;
	uint8_t buf[];
};

struct vhost_vdpa_iova_range {
	uint64_t first;
	uint64_t last;
};

#define VHOST_F_LOG_ALL 26
#define VHOST_NET_F_VIRTIO_NET_HDR 27

#define VHOST_BACKEND_F_IOTLB_MSG_V2 0x1
#define VHOST_BACKEND_F_IOTLB_BATCH  0x2
#define VHOST_BACKEND_F_IOTLB_ASID  0x3
#define VHOST_BACKEND_F_SUSPEND  0x4
#define VHOST_BACKEND_F_RESUME  0x5
#define VHOST_BACKEND_F_ENABLE_AFTER_DRIVER_OK  0x6
#define VHOST_BACKEND_F_DESC_ASID    0x7
#define VHOST_BACKEND_F_IOTLB_PERSIST  0x8

#endif
