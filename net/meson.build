system_ss.add(files(
  'announce.c',
  'checksum.c',
  'eth.c',
  'net-hmp-cmds.c',
  'net.c',
  'queue.c',
  'util.c',
))

system_ss.add(when: slirp, if_true: files('slirp.c'))
