system_ss.add([files(
  'cryptodev-builtin.c',
  'cryptodev-hmp-cmds.c',
  'cryptodev.c',
  'hostmem-ram.c',
  'hostmem.c',
  'rng-builtin.c',
  'rng-egd.c',
  'rng.c',
  'confidential-guest-support.c',
), numa])

# vectras android: removed hostmem-shm
if host_os != 'windows'
  system_ss.add(files('rng-random.c'))
  system_ss.add(files('hostmem-file.c'))
endif
if host_os == 'linux'
  system_ss.add(files('hostmem-memfd.c'))
  system_ss.add(files('host_iommu_device.c'))
endif
system_ss.add(when: 'CONFIG_VIRTIO_CRYPTO', if_true: files('cryptodev-vhost.c'))
system_ss.add(when: gio, if_true: files('dbus-vmstate.c'))
system_ss.add(when: 'CONFIG_SGX', if_true: files('hostmem-epc.c'))

system_ss.add(when: 'CONFIG_SPDM_SOCKET', if_true: files('spdm-socket.c'))
