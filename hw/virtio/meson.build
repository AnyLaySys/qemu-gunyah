system_virtio_ss = ss.source_set()
system_virtio_ss.add(files('virtio-bus.c'))
system_virtio_ss.add(files('iothread-vq-mapping.c'))
system_virtio_ss.add(when: 'CONFIG_VIRTIO_PCI', if_true: files('virtio-pci.c'))

specific_virtio_ss = ss.source_set()
specific_virtio_ss.add(files('virtio.c'))
specific_virtio_ss.add(files('virtio-config-io.c', 'virtio-qmp.c'))

if have_vhost
  system_virtio_ss.add(files('vhost.c'))
  specific_virtio_ss.add(files('vhost-backend.c', 'vhost-iova-tree.c'))
  if have_vhost_user
    # fixme - this really should be generic
    specific_virtio_ss.add(files('vhost-user.c'))
    system_virtio_ss.add(files('vhost-user-base.c'))

    # MMIO Stubs
    system_virtio_ss.add(files('vhost-user-device.c'))
    system_virtio_ss.add(when: 'CONFIG_VHOST_USER_GPIO', if_true: files('vhost-user-gpio.c'))
    system_virtio_ss.add(when: 'CONFIG_VHOST_USER_I2C', if_true: files('vhost-user-i2c.c'))
    system_virtio_ss.add(when: 'CONFIG_VHOST_USER_RNG', if_true: files('vhost-user-rng.c'))
    system_virtio_ss.add(when: 'CONFIG_VHOST_USER_SND', if_true: files('vhost-user-snd.c'))
    system_virtio_ss.add(when: 'CONFIG_VHOST_USER_INPUT', if_true: files('vhost-user-input.c'))

    # PCI Stubs
    system_virtio_ss.add(when: 'CONFIG_VIRTIO_PCI', if_true: files('vhost-user-device-pci.c'))
    system_virtio_ss.add(when: ['CONFIG_VIRTIO_PCI', 'CONFIG_VHOST_USER_GPIO'],
                         if_true: files('vhost-user-gpio-pci.c'))
    system_virtio_ss.add(when: ['CONFIG_VIRTIO_PCI', 'CONFIG_VHOST_USER_I2C'],
                         if_true: files('vhost-user-i2c-pci.c'))
    system_virtio_ss.add(when: ['CONFIG_VIRTIO_PCI', 'CONFIG_VHOST_USER_RNG'],
                         if_true: files('vhost-user-rng-pci.c'))
    system_virtio_ss.add(when: ['CONFIG_VIRTIO_PCI', 'CONFIG_VHOST_USER_SND'],
                         if_true: files('vhost-user-snd-pci.c'))
    system_virtio_ss.add(when: ['CONFIG_VIRTIO_PCI', 'CONFIG_VHOST_USER_INPUT'],
                         if_true: files('vhost-user-input-pci.c'))
  endif
  if have_vhost_vdpa
    system_virtio_ss.add(files('vhost-vdpa.c'))
    specific_virtio_ss.add(files('vhost-shadow-virtqueue.c'))
  endif
else
  system_virtio_ss.add(files('vhost-stub.c'))
endif

virtio_pci_ss = ss.source_set()
virtio_pci_ss.add(when: 'CONFIG_VIRTIO_INPUT', if_true: files('virtio-input-pci.c'))
virtio_pci_ss.add(when: 'CONFIG_VIRTIO_BLK', if_true: files('virtio-blk-pci.c'))
virtio_pci_ss.add(when: 'CONFIG_VIRTIO_NET', if_true: files('virtio-net-pci.c'))

specific_virtio_ss.add_all(when: 'CONFIG_VIRTIO_PCI', if_true: virtio_pci_ss)

system_ss.add_all(when: 'CONFIG_VIRTIO', if_true: system_virtio_ss)
system_ss.add(when: 'CONFIG_VIRTIO', if_false: files('vhost-stub.c'))
system_ss.add(when: 'CONFIG_VIRTIO', if_false: files('virtio-stub.c'))
system_ss.add(when: ['CONFIG_VIRTIO_MD', 'CONFIG_VIRTIO_PCI'],
              if_false: files('virtio-md-stubs.c'))

system_ss.add(files('virtio-hmp-cmds.c'))

specific_ss.add_all(when: 'CONFIG_VIRTIO', if_true: specific_virtio_ss)
system_ss.add(when: 'CONFIG_ACPI', if_true: files('virtio-acpi.c'))
