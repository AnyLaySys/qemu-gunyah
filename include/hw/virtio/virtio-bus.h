
#ifndef VIRTIO_BUS_H
#define VIRTIO_BUS_H

#include "hw/qdev-core.h"
#include "hw/virtio/virtio.h"
#include "qom/object.h"

#define TYPE_VIRTIO_BUS "virtio-bus"
typedef struct VirtioBusClass VirtioBusClass;
typedef struct VirtioBusState VirtioBusState;
DECLARE_OBJ_CHECKERS(VirtioBusState, VirtioBusClass,
                     VIRTIO_BUS, TYPE_VIRTIO_BUS)


struct VirtioBusClass {
    BusClass parent;
    void (*notify)(DeviceState *d, uint16_t vector);
    void (*save_config)(DeviceState *d, QEMUFile *f);
    void (*save_queue)(DeviceState *d, int n, QEMUFile *f);
    void (*save_extra_state)(DeviceState *d, QEMUFile *f);
    int (*load_config)(DeviceState *d, QEMUFile *f);
    int (*load_queue)(DeviceState *d, int n, QEMUFile *f);
    int (*load_done)(DeviceState *d, QEMUFile *f);
    int (*load_extra_state)(DeviceState *d, QEMUFile *f);
    bool (*has_extra_state)(DeviceState *d);
    bool (*query_guest_notifiers)(DeviceState *d);
    int (*set_guest_notifiers)(DeviceState *d, int nvqs, bool assign);
    int (*set_host_notifier_mr)(DeviceState *d, int n,
                                MemoryRegion *mr, bool assign);
    void (*vmstate_change)(DeviceState *d, bool running);
    void (*pre_plugged)(DeviceState *d, Error **errp);
    void (*device_plugged)(DeviceState *d, Error **errp);
    void (*device_unplugged)(DeviceState *d);
    int (*query_nvectors)(DeviceState *d);
    bool (*ioeventfd_enabled)(DeviceState *d);
    int (*ioeventfd_assign)(DeviceState *d, EventNotifier *notifier,
                            int n, bool assign);
    bool (*queue_enabled)(DeviceState *d, int n);
    bool has_variable_vring_alignment;
    AddressSpace *(*get_dma_as)(DeviceState *d);
    bool (*iommu_enabled)(DeviceState *d);
};

struct VirtioBusState {
    BusState parent_obj;

    bool ioeventfd_started;

    int ioeventfd_grabbed;
};

void virtio_bus_device_plugged(VirtIODevice *vdev, Error **errp);
void virtio_bus_reset(VirtioBusState *bus);
void virtio_bus_device_unplugged(VirtIODevice *bus);
uint16_t virtio_bus_get_vdev_id(VirtioBusState *bus);
size_t virtio_bus_get_vdev_config_len(VirtioBusState *bus);
uint32_t virtio_bus_get_vdev_bad_features(VirtioBusState *bus);
void virtio_bus_get_vdev_config(VirtioBusState *bus, uint8_t *config);
void virtio_bus_set_vdev_config(VirtioBusState *bus, uint8_t *config);

static inline VirtIODevice *virtio_bus_get_device(VirtioBusState *bus)
{
    BusState *qbus = &bus->parent_obj;
    BusChild *kid = QTAILQ_FIRST(&qbus->children);
    DeviceState *qdev = kid ? kid->child : NULL;

    return (VirtIODevice *)qdev;
}

bool virtio_bus_ioeventfd_enabled(VirtioBusState *bus);
int virtio_bus_start_ioeventfd(VirtioBusState *bus);
void virtio_bus_stop_ioeventfd(VirtioBusState *bus);
int virtio_bus_grab_ioeventfd(VirtioBusState *bus);
void virtio_bus_release_ioeventfd(VirtioBusState *bus);
int virtio_bus_set_host_notifier(VirtioBusState *bus, int n, bool assign);
void virtio_bus_cleanup_host_notifier(VirtioBusState *bus, int n);
bool virtio_bus_device_iommu_enabled(VirtIODevice *vdev);
#endif /* VIRTIO_BUS_H */
