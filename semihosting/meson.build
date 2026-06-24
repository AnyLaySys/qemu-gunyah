common_ss.add(when: ['CONFIG_SEMIHOSTING', 'CONFIG_SYSTEM_ONLY'], if_false: files('stubs-all.c'))
system_ss.add(when: ['CONFIG_SEMIHOSTING'], if_true: files(
  'config.c',
  'console.c',
), if_false: files(
  'stubs-system.c',
))

if config_all_devices.has_key('CONFIG_SEMIHOSTING')
  error('Semihosting syscall support was removed from this trimmed build')
endif
