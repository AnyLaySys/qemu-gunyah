arm_ss = ss.source_set()
arm_ss.add(files(
  'cpu.c',
  'debug_helper.c',
  'helper.c',
  'vfp_fpscr.c',
))
arm_ss.add(zlib)

arm_ss.add(when: 'CONFIG_GH', if_true: files('gunyah.c'))

arm_ss.add(when: 'TARGET_AARCH64', if_true: files(
  'cpu64.c',
))

arm_system_ss = ss.source_set()
arm_system_ss.add(files(
  'arm-powerctl.c',
  'arm-qmp-cmds.c',
  'cortex-regs.c',
  'machine.c',
  'ptw.c',
))

arm_user_ss = ss.source_set()

if 'CONFIG_HVF' in config_all_accel
  subdir('hvf')
endif

target_arch += {'arm': arm_ss}
target_system_arch += {'arm': arm_system_ss}
target_user_arch += {'arm': arm_user_ss}
