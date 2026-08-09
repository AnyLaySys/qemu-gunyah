system_ss.add(files(
  'block.c',
  'hd-geometry.c'
))
specific_ss.add(when: 'CONFIG_VIRTIO_BLK', if_true: files('virtio-blk.c', 'virtio-blk-common.c'))
