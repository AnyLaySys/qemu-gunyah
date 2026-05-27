system_stubs_ss = ss.source_set()
system_stubs_ss.add(when: 'CONFIG_TCG', if_false: files('tcg-stub.c'))
system_stubs_ss.add(when: 'CONFIG_GUNYAH', if_false: files('gunyah-stub.c'))

specific_ss.add_all(when: ['CONFIG_SYSTEM_ONLY'], if_true: system_stubs_ss)
