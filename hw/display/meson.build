hw_display_modules = {}

system_ss.add(when: 'CONFIG_DDC', if_true: files('i2c-ddc.c'))
system_ss.add(when: 'CONFIG_EDID', if_true: files('edid-generate.c', 'edid-region.c'))
system_ss.add(when: 'CONFIG_VIRTIO', if_true: files('virtio-dmabuf.c'))

if config_all_devices.has_key('CONFIG_VIRTIO_PCI')
  virtio_gpu_pci_ss = ss.source_set()
  virtio_gpu_pci_ss.add(when: ['CONFIG_VIRTIO_GPU', 'CONFIG_VIRTIO_PCI'],
                        if_true: [files('virtio-gpu-base.c',
                                        'virtio-gpu.c',
                                        'virtio-gpu-pci.c',
                                        'virtio-gpu-udmabuf.c'), pixman])
  hw_display_modules += {'virtio-gpu-pci': virtio_gpu_pci_ss}
endif

modules += { 'hw-display': hw_display_modules }
