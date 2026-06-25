mem_ss = ss.source_set()
mem_ss.add(files('memory-device.c'))
mem_ss.add(when: 'CONFIG_DIMM', if_true: files('pc-dimm.c'))
mem_ss.add(when: 'CONFIG_NVDIMM', if_true: files('nvdimm.c'))

system_ss.add(when: 'CONFIG_MEM_DEVICE', if_false: files('memory-device-stubs.c'))
system_ss.add_all(when: 'CONFIG_MEM_DEVICE', if_true: mem_ss)
