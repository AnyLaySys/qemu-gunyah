specific_ss.add(files('accel-target.c'))
system_ss.add(files('accel-system.c', 'accel-blocker.c'))
if get_option('tcg').allowed()
subdir('tcg')
endif
if have_system
subdir('gunyah')
subdir('stubs')
endif
