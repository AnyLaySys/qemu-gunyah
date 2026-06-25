specific_ss.add(when: 'CONFIG_SYSTEM_ONLY', if_true: [files(
  'arch_init.c',
  'ioport.c',
  'globals-target.c',
  'memory.c',
  'physmem.c',
)])

system_ss.add(files(
  'balloon.c',
  'bootdevice.c',
  'cpus.c',
  'cpu-timers.c',
  'datadir.c',
  'dma-helpers.c',
  'globals.c',
  'memory_mapping.c',
  'qdev-monitor.c',
  'qtest.c',
  'rtc.c',
  'runstate-action.c',
  'runstate-hmp-cmds.c',
  'runstate.c',
  'tpm-hmp-cmds.c',
  'vl.c',
  'watchpoint.c',
), sdl, libpmem, libdaxctl)

if have_tpm
  error('TPM support was removed from this trimmed build')
endif

if seccomp.found()
  error('seccomp support was removed from this trimmed build')
endif
system_ss.add(when: 'CONFIG_DEVICE_TREE',
              if_true: [fdt, files('device_tree.c')],
              if_false: files('device_tree-stub.c'))
if host_os == 'linux'
  system_ss.add(files('async-teardown.c'))
endif
