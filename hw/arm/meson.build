arm_ss = ss.source_set()
arm_ss.add(files('boot.c'))
arm_ss.add(when: 'CONFIG_ARM_VIRT', if_true: files('virt.c'))
arm_ss.add(when: 'CONFIG_ACPI', if_true: files('virt-acpi-build.c'))

hw_arch += {'arm': arm_ss}
