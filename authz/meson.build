authz_ss.add(genh)
authz_ss.add(files(
  'base.c',
  'list.c',
  'listfile.c',
  'simple.c',
))

if pam.found()
  error('PAM authorization support was removed from this trimmed build')
endif
