#ifndef VMNET_INT_H
#define VMNET_INT_H

#include "vmnet_int.h"
#include "clients.h"

#include <vmnet/vmnet.h>
#include <dispatch/dispatch.h>

#define VMNET_PACKETS_LIMIT 200

typedef struct VmnetState {
    NetClientState nc;
    interface_ref vmnet_if;

    uint64_t mtu;
    uint64_t max_packet_size;

    dispatch_queue_t if_queue;

    QEMUBH *send_bh;

    struct vmpktdesc packets_buf[VMNET_PACKETS_LIMIT];
    int packets_send_current_pos;
    int packets_send_end_pos;

    struct iovec iov_buf[VMNET_PACKETS_LIMIT];

    VMChangeStateEntry *change;
} VmnetState;

const char *vmnet_status_map_str(vmnet_return_t status);

int vmnet_if_create(NetClientState *nc,
                    xpc_object_t if_desc,
                    Error **errp);

ssize_t vmnet_receive_common(NetClientState *nc,
                             const uint8_t *buf,
                             size_t size);

void vmnet_cleanup_common(NetClientState *nc);

#endif /* VMNET_INT_H */
