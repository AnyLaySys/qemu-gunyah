
#ifndef HW_VIRTIO_VHOST_VDPA_H
#define HW_VIRTIO_VHOST_VDPA_H

#include <gmodule.h>

#include "hw/virtio/vhost-iova-tree.h"
#include "hw/virtio/vhost-shadow-virtqueue.h"
#include "hw/virtio/virtio.h"
#include "standard-headers/linux/vhost_types.h"

#define VHOST_VDPA_GUEST_PA_ASID 0

typedef struct VhostVDPAHostNotifier {
    MemoryRegion mr;
    void *addr;
} VhostVDPAHostNotifier;

typedef enum SVQTransitionState {
    SVQ_TSTATE_DISABLING = -1,
    SVQ_TSTATE_DONE,
    SVQ_TSTATE_ENABLING
} SVQTransitionState;

typedef struct vhost_vdpa_shared {
    int device_fd;
    MemoryListener listener;
    struct vhost_vdpa_iova_range iova_range;
    QLIST_HEAD(, vdpa_iommu) iommu_list;

    VhostIOVATree *iova_tree;

    uint64_t backend_cap;

    bool iotlb_batch_begin_sent;

    bool shadow_data;

    SVQTransitionState svq_switching;
} VhostVDPAShared;

typedef struct vhost_vdpa {
    int index;
    uint32_t address_space_id;
    uint64_t acked_features;
    bool shadow_vqs_enabled;
    bool suspended;
    VhostVDPAShared *shared;
    GPtrArray *shadow_vqs;
    const VhostShadowVirtqueueOps *shadow_vq_ops;
    void *shadow_vq_ops_opaque;
    struct vhost_dev *dev;
    Error *migration_blocker;
    VhostVDPAHostNotifier notifier[VIRTIO_QUEUE_MAX];
    IOMMUNotifier n;
} VhostVDPA;

int vhost_vdpa_get_iova_range(int fd, struct vhost_vdpa_iova_range *iova_range);
int vhost_vdpa_set_vring_ready(struct vhost_vdpa *v, unsigned idx);

int vhost_vdpa_dma_map(VhostVDPAShared *s, uint32_t asid, hwaddr iova,
                       hwaddr size, void *vaddr, bool readonly);
int vhost_vdpa_dma_unmap(VhostVDPAShared *s, uint32_t asid, hwaddr iova,
                         hwaddr size);

typedef struct vdpa_iommu {
    VhostVDPAShared *dev_shared;
    IOMMUMemoryRegion *iommu_mr;
    hwaddr iommu_offset;
    IOMMUNotifier n;
    QLIST_ENTRY(vdpa_iommu) iommu_next;
} VDPAIOMMUState;


#endif
