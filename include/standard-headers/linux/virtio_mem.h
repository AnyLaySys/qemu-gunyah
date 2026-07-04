
#ifndef _LINUX_VIRTIO_MEM_H
#define _LINUX_VIRTIO_MEM_H

#include "standard-headers/linux/types.h"
#include "standard-headers/linux/virtio_types.h"
#include "standard-headers/linux/virtio_ids.h"
#include "standard-headers/linux/virtio_config.h"



#define VIRTIO_MEM_F_ACPI_PXM		0
#define VIRTIO_MEM_F_UNPLUGGED_INACCESSIBLE	1
#define VIRTIO_MEM_F_PERSISTENT_SUSPEND		2



#define VIRTIO_MEM_REQ_PLUG			0
#define VIRTIO_MEM_REQ_UNPLUG			1
#define VIRTIO_MEM_REQ_UNPLUG_ALL		2
#define VIRTIO_MEM_REQ_STATE			3

struct virtio_mem_req_plug {
	__virtio64 addr;
	__virtio16 nb_blocks;
	__virtio16 padding[3];
};

struct virtio_mem_req_unplug {
	__virtio64 addr;
	__virtio16 nb_blocks;
	__virtio16 padding[3];
};

struct virtio_mem_req_state {
	__virtio64 addr;
	__virtio16 nb_blocks;
	__virtio16 padding[3];
};

struct virtio_mem_req {
	__virtio16 type;
	__virtio16 padding[3];

	union {
		struct virtio_mem_req_plug plug;
		struct virtio_mem_req_unplug unplug;
		struct virtio_mem_req_state state;
	} u;
};



#define VIRTIO_MEM_RESP_ACK			0
#define VIRTIO_MEM_RESP_NACK			1
#define VIRTIO_MEM_RESP_BUSY			2
#define VIRTIO_MEM_RESP_ERROR			3


#define VIRTIO_MEM_STATE_PLUGGED		0
#define VIRTIO_MEM_STATE_UNPLUGGED		1
#define VIRTIO_MEM_STATE_MIXED			2

struct virtio_mem_resp_state {
	__virtio16 state;
};

struct virtio_mem_resp {
	__virtio16 type;
	__virtio16 padding[3];

	union {
		struct virtio_mem_resp_state state;
	} u;
};


struct virtio_mem_config {
	uint64_t block_size;
	uint16_t node_id;
	uint8_t padding[6];
	uint64_t addr;
	uint64_t region_size;
	uint64_t usable_region_size;
	uint64_t plugged_size;
	uint64_t requested_size;
};

#endif /* _LINUX_VIRTIO_MEM_H */
