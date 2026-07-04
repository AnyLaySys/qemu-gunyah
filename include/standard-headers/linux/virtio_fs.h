
#ifndef _LINUX_VIRTIO_FS_H
#define _LINUX_VIRTIO_FS_H

#include "standard-headers/linux/types.h"
#include "standard-headers/linux/virtio_ids.h"
#include "standard-headers/linux/virtio_config.h"
#include "standard-headers/linux/virtio_types.h"

struct virtio_fs_config {
	uint8_t tag[36];

	uint32_t num_request_queues;
} QEMU_PACKED;

#define VIRTIO_FS_SHMCAP_ID_CACHE 0

#endif /* _LINUX_VIRTIO_FS_H */
