#ifndef _LINUX_VIRTIO_9P_H
#define _LINUX_VIRTIO_9P_H
#include "standard-headers/linux/virtio_types.h"
#include "standard-headers/linux/virtio_ids.h"
#include "standard-headers/linux/virtio_config.h"


#define VIRTIO_9P_MOUNT_TAG 0

struct virtio_9p_config {
	__virtio16 tag_len;
	uint8_t tag[];
} QEMU_PACKED;

#endif /* _LINUX_VIRTIO_9P_H */
