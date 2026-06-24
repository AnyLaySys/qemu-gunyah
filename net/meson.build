system_ss.add(files(
  'announce.c',
  'checksum.c',
  'dump.c',
  'eth.c',
  'filter-buffer.c',
  'filter-mirror.c',
  'filter.c',
  'hub.c',
  'net-hmp-cmds.c',
  'net.c',
  'queue.c',
  'socket.c',
  'stream.c',
  'dgram.c',
  'util.c',
))

if get_option('replication').allowed() or \
    get_option('colo_proxy').allowed()
  system_ss.add(files('colo-compare.c'))
  system_ss.add(files('colo.c'))
else
  system_ss.add(files('colo-stubs.c'))
endif

if get_option('colo_proxy').allowed()
  system_ss.add(files('filter-rewriter.c'))
endif

system_ss.add(when: 'CONFIG_TCG', if_true: files('filter-replay.c'))

system_ss.add(when: slirp, if_true: files('slirp.c'))

if host_os == 'linux'
  system_ss.add(files('tap.c', 'tap-linux.c'))
else
  error('This trimmed build keeps only the Linux TAP backend')
endif
subdir('can')
