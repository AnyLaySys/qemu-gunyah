
#ifndef _LINUX_VIRTIO_PMEM_H
#define _LINUX_VIRTIO_PMEM_H

#include "standard-headers/linux/types.h"
#include "standard-headers/linux/virtio_ids.h"
#include "standard-headers/linux/virtio_config.h"

#define VIRTIO_PMEM_F_SHMEM_REGION 0

#define VIRTIO_PMEM_SHMEM_REGION_ID 0

struct virtio_pmem_config {
	uint64_t start;
	uint64_t size;
};

#define VIRTIO_PMEM_REQ_TYPE_FLUSH      0

struct virtio_pmem_resp {
	uint32_t ret;
};

struct virtio_pmem_req {
	uint32_t type;
};

#endif
