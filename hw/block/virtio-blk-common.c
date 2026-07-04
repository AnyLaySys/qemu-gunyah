
#include "qemu/osdep.h"

#include "standard-headers/linux/virtio_blk.h"
#include "hw/virtio/virtio.h"
#include "hw/virtio/virtio-blk-common.h"

#define VIRTIO_BLK_CFG_SIZE offsetof(struct virtio_blk_config, \
                                     max_discard_sectors)

static const VirtIOFeature feature_sizes[] = {
    {.flags = 1ULL << VIRTIO_BLK_F_DISCARD,
     .end = endof(struct virtio_blk_config, discard_sector_alignment)},
    {.flags = 1ULL << VIRTIO_BLK_F_WRITE_ZEROES,
     .end = endof(struct virtio_blk_config, write_zeroes_may_unmap)},
    {.flags = 1ULL << VIRTIO_BLK_F_ZONED,
     .end = endof(struct virtio_blk_config, zoned)},
    {}
};

const VirtIOConfigSizeParams virtio_blk_cfg_size_params = {
    .min_size = VIRTIO_BLK_CFG_SIZE,
    .max_size = sizeof(struct virtio_blk_config),
    .feature_sizes = feature_sizes
};
