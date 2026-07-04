system_ss.add(files('soundhw.c'))
system_ss.add(when: ['CONFIG_VIRTIO_SND', 'CONFIG_VIRTIO'], if_true: files('virtio-snd.c'))
system_ss.add(when: ['CONFIG_VIRTIO_SND', 'CONFIG_VIRTIO', 'CONFIG_VIRTIO_PCI'], if_true: files('virtio-snd-pci.c'))
