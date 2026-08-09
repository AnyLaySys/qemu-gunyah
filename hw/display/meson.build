hw_display_modules = {}

system_ss.add(when: 'CONFIG_EDID', if_true: files('edid-generate.c'))
system_ss.add(when: 'CONFIG_VIRTIO', if_true: files('virtio-dmabuf.c'))

if config_all_devices.has_key('CONFIG_VIRTIO_PCI')
  virtio_gpu_gl_pci_ss = ss.source_set()
  virtio_gpu_gl_pci_ss.add(when: [opengl, virgl, 'CONFIG_VIRTIO_GPU',
                                  'CONFIG_VIRTIO_PCI'],
                           if_true: [files('virtio-gpu-base.c',
                                           'virtio-gpu.c',
                                           'virtio-gpu-gl-pci.c',
                                           'virtio-gpu-udmabuf.c',
                                           'virtio-gpu-gl.c'), pixman])
  hw_display_modules += {'virtio-gpu-gl-pci': virtio_gpu_gl_pci_ss}
endif

modules += { 'hw-display': hw_display_modules }
