gunyah_ss = ss.source_set()
gunyah_ss.add(files(
  'gunyah.c',
  'gunyah-accel-ops.c',
  'gunyah-report.c',
))
specific_ss.add_all(when: 'CONFIG_GH', if_true: gunyah_ss)
