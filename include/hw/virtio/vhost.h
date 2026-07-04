#ifndef VHOST_H
#define VHOST_H

#include "hw/virtio/vhost-backend.h"
#include "hw/virtio/virtio.h"
#include "exec/memory.h"

#define VHOST_F_DEVICE_IOTLB 63
#define VHOST_USER_F_PROTOCOL_FEATURES 30

#define VU_REALIZE_CONN_RETRIES 3


struct vhost_inflight {
    int fd;
    void *addr;
    uint64_t size;
    uint64_t offset;
    uint16_t queue_size;
};

struct vhost_virtqueue {
    int kick;
    int call;
    void *desc;
    void *avail;
    void *used;
    int num;
    unsigned long long desc_phys;
    unsigned desc_size;
    unsigned long long avail_phys;
    unsigned avail_size;
    unsigned long long used_phys;
    unsigned used_size;
    EventNotifier masked_notifier;
    EventNotifier error_notifier;
    EventNotifier masked_config_notifier;
    struct vhost_dev *dev;
};

typedef unsigned long vhost_log_chunk_t;
#define VHOST_LOG_PAGE 0x1000
#define VHOST_LOG_BITS (8 * sizeof(vhost_log_chunk_t))
#define VHOST_LOG_CHUNK (VHOST_LOG_PAGE * VHOST_LOG_BITS)
#define VHOST_INVALID_FEATURE_BIT   (0xff)
#define VHOST_QUEUE_NUM_CONFIG_INR 0

struct vhost_log {
    unsigned long long size;
    int refcnt;
    int fd;
    vhost_log_chunk_t *log;
};

struct vhost_dev;
struct vhost_iommu {
    struct vhost_dev *hdev;
    MemoryRegion *mr;
    hwaddr iommu_offset;
    IOMMUNotifier n;
    QLIST_ENTRY(vhost_iommu) iommu_next;
};

typedef struct VhostDevConfigOps {
    int (*vhost_dev_config_notifier)(struct vhost_dev *dev);
} VhostDevConfigOps;

struct vhost_memory;

struct vhost_dev {
    VirtIODevice *vdev;
    MemoryListener memory_listener;
    MemoryListener iommu_listener;
    struct vhost_memory *mem;
    int n_mem_sections;
    MemoryRegionSection *mem_sections;
    int n_tmp_sections;
    MemoryRegionSection *tmp_sections;
    struct vhost_virtqueue *vqs;
    unsigned int nvqs;
    int vq_index;
    int vq_index_end;
    int num_queues;
    uint64_t features;
    uint64_t acked_features;
    uint64_t backend_features;

    uint64_t protocol_features;

    uint64_t max_queues;
    uint64_t backend_cap;
    bool started;
    bool log_enabled;
    uint64_t log_size;
    Error *migration_blocker;
    const VhostOps *vhost_ops;
    void *opaque;
    struct vhost_log *log;
    QLIST_ENTRY(vhost_dev) entry;
    QLIST_ENTRY(vhost_dev) logdev_entry;
    QLIST_HEAD(, vhost_iommu) iommu_list;
    IOMMUNotifier n;
    const VhostDevConfigOps *config_ops;
};

extern const VhostOps kernel_ops;
extern const VhostOps user_ops;
extern const VhostOps vdpa_ops;

struct vhost_net {
    struct vhost_dev dev;
    struct vhost_virtqueue vqs[2];
    int backend;
    NetClientState *nc;
};

int vhost_dev_init(struct vhost_dev *hdev, void *opaque,
                   VhostBackendType backend_type,
                   uint32_t busyloop_timeout, Error **errp);

void vhost_dev_cleanup(struct vhost_dev *hdev);

void vhost_dev_disable_notifiers_nvqs(struct vhost_dev *hdev,
                                      VirtIODevice *vdev,
                                      unsigned int nvqs);

int vhost_dev_enable_notifiers(struct vhost_dev *hdev, VirtIODevice *vdev);

void vhost_dev_disable_notifiers(struct vhost_dev *hdev, VirtIODevice *vdev);
bool vhost_config_pending(struct vhost_dev *hdev);
void vhost_config_mask(struct vhost_dev *hdev, VirtIODevice *vdev, bool mask);

static inline bool vhost_dev_is_started(struct vhost_dev *hdev)
{
    return hdev->started;
}

int vhost_dev_start(struct vhost_dev *hdev, VirtIODevice *vdev, bool vrings);

void vhost_dev_stop(struct vhost_dev *hdev, VirtIODevice *vdev, bool vrings);


int vhost_dev_get_config(struct vhost_dev *hdev, uint8_t *config,
                         uint32_t config_len, Error **errp);

int vhost_dev_set_config(struct vhost_dev *dev, const uint8_t *data,
                         uint32_t offset, uint32_t size, uint32_t flags);

void vhost_dev_set_config_notifier(struct vhost_dev *dev,
                                   const VhostDevConfigOps *ops);


bool vhost_virtqueue_pending(struct vhost_dev *hdev, int n);

void vhost_virtqueue_mask(struct vhost_dev *hdev, VirtIODevice *vdev, int n,
                          bool mask);

uint64_t vhost_get_features(struct vhost_dev *hdev, const int *feature_bits,
                            uint64_t features);

void vhost_ack_features(struct vhost_dev *hdev, const int *feature_bits,
                        uint64_t features);
unsigned int vhost_get_max_memslots(void);
unsigned int vhost_get_free_memslots(void);

int vhost_net_set_backend(struct vhost_dev *hdev,
                          struct vhost_vring_file *file);

void vhost_toggle_device_iotlb(VirtIODevice *vdev);
int vhost_device_iotlb_miss(struct vhost_dev *dev, uint64_t iova, int write);

int vhost_virtqueue_start(struct vhost_dev *dev, struct VirtIODevice *vdev,
                          struct vhost_virtqueue *vq, unsigned idx);
void vhost_virtqueue_stop(struct vhost_dev *dev, struct VirtIODevice *vdev,
                          struct vhost_virtqueue *vq, unsigned idx);

void vhost_dev_reset_inflight(struct vhost_inflight *inflight);
void vhost_dev_free_inflight(struct vhost_inflight *inflight);
int vhost_dev_prepare_inflight(struct vhost_dev *hdev, VirtIODevice *vdev);
int vhost_dev_set_inflight(struct vhost_dev *dev,
                           struct vhost_inflight *inflight);
int vhost_dev_get_inflight(struct vhost_dev *dev, uint16_t queue_size,
                           struct vhost_inflight *inflight);
bool vhost_dev_has_iommu(struct vhost_dev *dev);

#ifdef CONFIG_VHOST
int vhost_reset_device(struct vhost_dev *hdev);
#else
static inline int vhost_reset_device(struct vhost_dev *hdev)
{
    return -ENOSYS;
}
#endif /* CONFIG_VHOST */

#ifdef CONFIG_VHOST
bool vhost_supports_device_state(struct vhost_dev *dev);
#else
static inline bool vhost_supports_device_state(struct vhost_dev *dev)
{
    return false;
}
#endif

int vhost_set_device_state_fd(struct vhost_dev *dev,
                              VhostDeviceStateDirection direction,
                              VhostDeviceStatePhase phase,
                              int fd,
                              int *reply_fd,
                              Error **errp);

int vhost_check_device_state(struct vhost_dev *dev, Error **errp);

#ifdef CONFIG_VHOST
int vhost_save_backend_state(struct vhost_dev *dev, QEMUFile *f, Error **errp);
#else
static inline int vhost_save_backend_state(struct vhost_dev *dev, QEMUFile *f,
                                           Error **errp)
{
    return -ENOSYS;
}
#endif

#ifdef CONFIG_VHOST
int vhost_load_backend_state(struct vhost_dev *dev, QEMUFile *f, Error **errp);
#else
static inline int vhost_load_backend_state(struct vhost_dev *dev, QEMUFile *f,
                                           Error **errp)
{
    return -ENOSYS;
}
#endif

#endif
