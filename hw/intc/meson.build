system_ss.add(files('intc.c'))
system_ss.add(when: 'CONFIG_ARM_GIC', if_true: files(
  'arm_gic.c',
  'arm_gic_common.c',
  'arm_gicv2m.c',
  'arm_gicv3_common.c',
  'arm_gicv3_its_common.c',
))
system_ss.add(when: 'CONFIG_ARM_GICV3', if_true: files(
  'arm_gicv3.c',
  'arm_gicv3_dist.c',
  'arm_gicv3_its.c',
  'arm_gicv3_redist.c',
))
system_ss.add(when: 'CONFIG_IOAPIC', if_true: files('ioapic_common.c'), if_false: files('ioapic-stub.c'))
specific_ss.add(when: 'CONFIG_ARM_GIC', if_true: files('arm_gicv3_cpuif_common.c'))
specific_ss.add(when: ['CONFIG_ARM_GIC', 'CONFIG_GH', 'TARGET_AARCH64'], if_true: files('arm_gicv3_gunyah.c'))
