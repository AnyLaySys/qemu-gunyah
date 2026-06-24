system_ss.add(files('audio.c'))
system_ss.add(files(
  'audio-hmp-cmds.c',
  'mixeng.c',
  'noaudio.c',
  'wavaudio.c',
  'wavcapture.c',
))

system_ss.add(when: aaudio, if_true: files('aaudioaudio.c'))
