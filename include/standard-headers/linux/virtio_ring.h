#ifndef _LINUX_VIRTIO_RING_H
#define _LINUX_VIRTIO_RING_H
#include <stdint.h>
#include "standard-headers/linux/types.h"
#include "standard-headers/linux/virtio_types.h"

#define VRING_DESC_F_NEXT	1
#define VRING_DESC_F_WRITE	2
#define VRING_DESC_F_INDIRECT	4

#define VRING_PACKED_DESC_F_AVAIL	7
#define VRING_PACKED_DESC_F_USED	15

#define VRING_USED_F_NO_NOTIFY	1
#define VRING_AVAIL_F_NO_INTERRUPT	1

#define VRING_PACKED_EVENT_FLAG_ENABLE	0x0
#define VRING_PACKED_EVENT_FLAG_DISABLE	0x1
#define VRING_PACKED_EVENT_FLAG_DESC	0x2

#define VRING_PACKED_EVENT_F_WRAP_CTR	15

#define VIRTIO_RING_F_INDIRECT_DESC	28

#define VIRTIO_RING_F_EVENT_IDX		29

#define VRING_AVAIL_ALIGN_SIZE 2
#define VRING_USED_ALIGN_SIZE 4
#define VRING_DESC_ALIGN_SIZE 16

struct vring_desc {
	__virtio64 addr;
	__virtio32 len;
	__virtio16 flags;
	__virtio16 next;
};

struct vring_avail {
	__virtio16 flags;
	__virtio16 idx;
	__virtio16 ring[];
};

struct vring_used_elem {
	__virtio32 id;
	__virtio32 len;
};

typedef struct vring_used_elem __attribute__((aligned(VRING_USED_ALIGN_SIZE)))
	vring_used_elem_t;

struct vring_used {
	__virtio16 flags;
	__virtio16 idx;
	vring_used_elem_t ring[];
};

typedef struct vring_desc __attribute__((aligned(VRING_DESC_ALIGN_SIZE)))
	vring_desc_t;
typedef struct vring_avail __attribute__((aligned(VRING_AVAIL_ALIGN_SIZE)))
	vring_avail_t;
typedef struct vring_used __attribute__((aligned(VRING_USED_ALIGN_SIZE)))
	vring_used_t;

struct vring {
	unsigned int num;

	vring_desc_t *desc;

	vring_avail_t *avail;

	vring_used_t *used;
};

#ifndef VIRTIO_RING_NO_LEGACY

#define vring_used_event(vr) ((vr)->avail->ring[(vr)->num])
#define vring_avail_event(vr) (*(__virtio16 *)&(vr)->used->ring[(vr)->num])

static inline void vring_init(struct vring *vr, unsigned int num, void *p,
			      unsigned long align)
{
	vr->num = num;
	vr->desc = p;
	vr->avail = (struct vring_avail *)((char *)p + num * sizeof(struct vring_desc));
	vr->used = (void *)(((uintptr_t)&vr->avail->ring[num] + sizeof(__virtio16)
		+ align-1) & ~(align - 1));
}

static inline unsigned vring_size(unsigned int num, unsigned long align)
{
	return ((sizeof(struct vring_desc) * num + sizeof(__virtio16) * (3 + num)
		 + align - 1) & ~(align - 1))
		+ sizeof(__virtio16) * 3 + sizeof(struct vring_used_elem) * num;
}

#endif /* VIRTIO_RING_NO_LEGACY */

static inline int vring_need_event(uint16_t event_idx, uint16_t new_idx, uint16_t old)
{
	return (uint16_t)(new_idx - event_idx - 1) < (uint16_t)(new_idx - old);
}

struct vring_packed_desc_event {
	uint16_t off_wrap;
	uint16_t flags;
};

struct vring_packed_desc {
	uint64_t addr;
	uint32_t len;
	uint16_t id;
	uint16_t flags;
};

#endif /* _LINUX_VIRTIO_RING_H */
