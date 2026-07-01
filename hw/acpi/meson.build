acpi_ss = ss.source_set()
acpi_ss.add(files(
  'acpi_interface.c',
  'aml-build.c',
  'bios-linker-loader.c',
  'core.c',
  'utils.c',
))
acpi_ss.add(when: 'CONFIG_ACPI_CPU_HOTPLUG', if_true: files('cpu.c', 'cpu_hotplug.c'))
acpi_ss.add(when: 'CONFIG_ACPI_CPU_HOTPLUG', if_false: files('acpi-cpu-hotplug-stub.c'))
acpi_ss.add(when: 'CONFIG_ACPI_PCI', if_true: files('pci.c'))
acpi_ss.add(when: 'CONFIG_ACPI_VMGENID', if_true: files('vmgenid.c'))
acpi_ss.add(when: 'CONFIG_ACPI_VMCLOCK', if_true: files('vmclock.c'))
acpi_ss.add(when: 'CONFIG_ACPI_HW_REDUCED', if_true: files('generic_event_device.c'))
acpi_ss.add(when: 'CONFIG_ACPI_APEI', if_true: files('ghes.c'), if_false: files('ghes-stub.c'))
acpi_ss.add(files('acpi-pci-hotplug-stub.c'))
acpi_ss.add(when: 'CONFIG_IPMI', if_true: files('ipmi.c'), if_false: files('ipmi-stub.c'))

system_ss.add(when: 'CONFIG_ACPI', if_false: files('acpi-stub.c', 'aml-build-stub.c', 'ghes-stub.c', 'acpi_interface.c'))
system_ss.add_all(when: 'CONFIG_ACPI', if_true: acpi_ss)
system_ss.add(files('acpi-qmp-cmds.c'))
