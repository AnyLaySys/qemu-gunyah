#ifndef _LINUX_VIRTIO_PCIDEV_H
#define _LINUX_VIRTIO_PCIDEV_H
#include "standard-headers/linux/types.h"

enum virtio_pcidev_ops {
	VIRTIO_PCIDEV_OP_RESERVED = 0,
	VIRTIO_PCIDEV_OP_CFG_READ,
	VIRTIO_PCIDEV_OP_CFG_WRITE,
	VIRTIO_PCIDEV_OP_MMIO_READ,
	VIRTIO_PCIDEV_OP_MMIO_WRITE,
	VIRTIO_PCIDEV_OP_MMIO_MEMSET,
	VIRTIO_PCIDEV_OP_INT,
	VIRTIO_PCIDEV_OP_MSI,
	VIRTIO_PCIDEV_OP_PME,
};

struct virtio_pcidev_msg {
	uint8_t op;
	uint8_t bar;
	uint16_t reserved;
	uint32_t size;
	uint64_t addr;
	uint8_t data[];
};

#endif /* _LINUX_VIRTIO_PCIDEV_H */
