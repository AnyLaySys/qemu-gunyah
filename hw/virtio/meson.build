system_virtio_ss = ss.source_set()
system_virtio_ss.add(files('virtio-bus.c'))
system_virtio_ss.add(files('iothread-vq-mapping.c'))
system_virtio_ss.add(when: 'CONFIG_VIRTIO_PCI', if_true: files('virtio-pci.c'))

specific_virtio_ss = ss.source_set()
specific_virtio_ss.add(files('virtio.c'))
specific_virtio_ss.add(files('virtio-config-io.c'))

system_virtio_ss.add(files('vhost-stub.c'))

virtio_pci_ss = ss.source_set()
virtio_pci_ss.add(when: 'CONFIG_VIRTIO_INPUT', if_true: files('virtio-input-pci.c'))
virtio_pci_ss.add(when: 'CONFIG_VIRTIO_BLK', if_true: files('virtio-blk-pci.c'))
virtio_pci_ss.add(when: 'CONFIG_VIRTIO_NET', if_true: files('virtio-net-pci.c'))

specific_virtio_ss.add_all(when: 'CONFIG_VIRTIO_PCI', if_true: virtio_pci_ss)

system_ss.add_all(when: 'CONFIG_VIRTIO', if_true: system_virtio_ss)
system_ss.add(when: 'CONFIG_VIRTIO', if_false: files('vhost-stub.c'))

specific_ss.add_all(when: 'CONFIG_VIRTIO', if_true: specific_virtio_ss)
