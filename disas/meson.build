common_ss.add(when: 'CONFIG_TCG', if_true: files(
    'disas-host.c',
    'disas-target.c',
    'objdump.c'
))
common_ss.add(files('disas-common.c'))
system_ss.add(files('disas-mon.c'))
if capstone.found()
  error('Capstone disassembler support was removed from this trimmed build')
endif
