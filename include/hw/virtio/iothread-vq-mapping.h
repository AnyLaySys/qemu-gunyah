
#ifndef HW_VIRTIO_IOTHREAD_VQ_MAPPING_H
#define HW_VIRTIO_IOTHREAD_VQ_MAPPING_H

#include "qapi/error.h"
#include "qapi/qapi-types-virtio.h"

bool iothread_vq_mapping_apply(
        IOThreadVirtQueueMappingList *list,
        AioContext **vq_aio_context,
        uint16_t num_queues,
        Error **errp);

void iothread_vq_mapping_cleanup(IOThreadVirtQueueMappingList *list);

#endif /* HW_VIRTIO_IOTHREAD_VQ_MAPPING_H */
