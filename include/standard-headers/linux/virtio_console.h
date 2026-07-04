#ifndef _LINUX_VIRTIO_CONSOLE_H
#define _LINUX_VIRTIO_CONSOLE_H
#include "standard-headers/linux/types.h"
#include "standard-headers/linux/virtio_types.h"
#include "standard-headers/linux/virtio_ids.h"
#include "standard-headers/linux/virtio_config.h"

#define VIRTIO_CONSOLE_F_SIZE	0	/* Does host provide console size? */
#define VIRTIO_CONSOLE_F_MULTIPORT 1	/* Does host provide multiple ports? */
#define VIRTIO_CONSOLE_F_EMERG_WRITE 2 /* Does host support emergency write? */

#define VIRTIO_CONSOLE_BAD_ID		(~(uint32_t)0)

struct virtio_console_config {
	__virtio16 cols;
	__virtio16 rows;
	__virtio32 max_nr_ports;
	__virtio32 emerg_wr;
} QEMU_PACKED;

struct virtio_console_control {
	__virtio32 id;		/* Port number */
	__virtio16 event;	/* The kind of control event (see below) */
	__virtio16 value;	/* Extra information for the key */
};

#define VIRTIO_CONSOLE_DEVICE_READY	0
#define VIRTIO_CONSOLE_PORT_ADD		1
#define VIRTIO_CONSOLE_PORT_REMOVE	2
#define VIRTIO_CONSOLE_PORT_READY	3
#define VIRTIO_CONSOLE_CONSOLE_PORT	4
#define VIRTIO_CONSOLE_RESIZE		5
#define VIRTIO_CONSOLE_PORT_OPEN	6
#define VIRTIO_CONSOLE_PORT_NAME	7


#endif /* _LINUX_VIRTIO_CONSOLE_H */
