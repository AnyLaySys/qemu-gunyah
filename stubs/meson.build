
stub_ss.add(files('cpu-get-clock.c'))
stub_ss.add(files('crypto-stub.c'))
stub_ss.add(files('error-printf.c'))
stub_ss.add(files('fdset.c'))
stub_ss.add(files('iothread-lock.c'))
stub_ss.add(files('is-daemonized.c'))
stub_ss.add(files('monitor-core.c'))
stub_ss.add(files('trace-control.c'))

if have_block
  stub_ss.add(files('bdrv-next-monitor-owned.c'))
  stub_ss.add(files('blockdev-close-all-bdrv-states.c'))
  stub_ss.add(files('change-state-handler.c'))
  stub_ss.add(files('get-vm-name.c'))
  stub_ss.add(files('iothread-lock-block.c'))
  stub_ss.add(files('physmem.c'))
  stub_ss.add(files('ram-block.c'))
  stub_ss.add(files('runstate-check.c'))
  stub_ss.add(files('uuid.c'))
endif

if have_block or have_ga
  stub_ss.add(files('cpus-virtual-clock.c'))
  stub_ss.add(files('graph-lock.c'))
  stub_ss.add(files('qemu-timer-notify-cb.c'))

  stub_ss.add(files('monitor-internal.c'))
endif

if have_block or have_user
  stub_ss.add(files('vm-stop.c'))
endif


if have_system
  stub_ss.add(files('cmos.c'))
  stub_ss.add(files('fw_cfg.c'))
  stub_ss.add(files('target-get-monitor-def.c'))
  stub_ss.add(files('target-monitor-defs.c'))
  stub_ss.add(files('win32-kbd-hook.c'))
endif

if have_system or have_user
  stub_ss.add(files('hotplug-stubs.c'))
  stub_ss.add(files('sysbus.c'))
endif
