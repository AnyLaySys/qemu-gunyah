system_ss.add([files(
  'hostmem-ram.c',
  'hostmem.c',
  'confidential-guest-support.c',
), numa])

system_ss.add(when: 'CONFIG_SGX', if_true: files('hostmem-epc.c'))
