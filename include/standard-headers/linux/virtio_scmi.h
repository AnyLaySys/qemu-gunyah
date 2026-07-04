
#ifndef _LINUX_VIRTIO_SCMI_H
#define _LINUX_VIRTIO_SCMI_H

#include "standard-headers/linux/virtio_types.h"

#define VIRTIO_SCMI_F_P2A_CHANNELS 0

#define VIRTIO_SCMI_F_SHARED_MEMORY 1


#define VIRTIO_SCMI_VQ_TX 0 /* cmdq */
#define VIRTIO_SCMI_VQ_RX 1 /* eventq */
#define VIRTIO_SCMI_VQ_MAX_CNT 2

#endif /* _LINUX_VIRTIO_SCMI_H */
