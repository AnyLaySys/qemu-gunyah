
#ifndef _LINUX_VIRTIO_I2C_H
#define _LINUX_VIRTIO_I2C_H

#include "standard-headers/linux/const.h"
#include "standard-headers/linux/types.h"

#define VIRTIO_I2C_F_ZERO_LENGTH_REQUEST	0

#define VIRTIO_I2C_FLAGS_FAIL_NEXT	_BITUL(0)

#define VIRTIO_I2C_FLAGS_M_RD		_BITUL(1)

struct virtio_i2c_out_hdr {
	uint16_t addr;
	uint16_t padding;
	uint32_t flags;
};

struct virtio_i2c_in_hdr {
	uint8_t status;
};

#define VIRTIO_I2C_MSG_OK	0
#define VIRTIO_I2C_MSG_ERR	1

#endif /* _LINUX_VIRTIO_I2C_H */
