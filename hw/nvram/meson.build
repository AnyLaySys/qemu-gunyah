system_ss.add(files('fw_cfg-interface.c'))
system_ss.add(files('fw_cfg.c'))

specific_ss.add(when: 'CONFIG_ACPI', if_true: files('fw_cfg-acpi.c'))
