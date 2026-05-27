blobs = [
  'efi-virtio.rom',
]

if get_option('install_blobs')
  install_data(blobs, install_dir: qemu_datadir)
endif

subdir('keymaps')