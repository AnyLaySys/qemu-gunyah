system_ss.add(when: 'CONFIG_VIRTIO_NET', if_true: files('net_rx_pkt.c'))
specific_ss.add(when: 'CONFIG_VIRTIO_NET', if_true: files('virtio-net.c'))

system_ss.add(files('vhost_net-stub.c'))
