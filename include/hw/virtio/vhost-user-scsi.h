
#ifndef VHOST_USER_SCSI_H
#define VHOST_USER_SCSI_H

#include "hw/virtio/virtio-scsi.h"
#include "hw/virtio/vhost.h"
#include "hw/virtio/vhost-user.h"
#include "hw/virtio/vhost-scsi-common.h"
#include "qom/object.h"

#define TYPE_VHOST_USER_SCSI "vhost-user-scsi"
OBJECT_DECLARE_SIMPLE_TYPE(VHostUserSCSI, VHOST_USER_SCSI)

struct VHostUserSCSI {
    VHostSCSICommon parent_obj;

    bool connected;
    bool started_vu;

    VhostUserState vhost_user;
    struct vhost_virtqueue *vhost_vqs;
};

#endif /* VHOST_USER_SCSI_H */
