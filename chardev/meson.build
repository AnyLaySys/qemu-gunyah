chardev_ss.add(files(
  'char-fe.c',
  'char-file.c',
  'char-io.c',
  'char-mux.c',
  'char-hub.c',
  'char-null.c',
  'char-pipe.c',
  'char-ringbuf.c',
  'char-serial.c',
  'char-socket.c',
  'char-stdio.c',
  'char-udp.c',
  'char.c',
))
chardev_ss.add(files(
    'char-fd.c',
    'char-parallel.c',
    'char-pty.c',
  ), util)

chardev_ss = chardev_ss.apply({})

system_ss.add(files(
    'char-hmp-cmds.c',
    'msmouse.c',
    'wctablet.c',
    'testdev.c'))
