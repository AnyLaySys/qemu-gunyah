crypto_ss.add(genh)
crypto_ss.add(files(
  'afsplit.c',
  'akcipher.c',
  'block-luks.c',
  'block-qcow.c',
  'block.c',
  'cipher.c',
  'der.c',
  'hash.c',
  'hmac.c',
  'ivgen-essiv.c',
  'ivgen-plain.c',
  'ivgen-plain64.c',
  'ivgen.c',
  'pbkdf.c',
  'secret_common.c',
  'secret.c',
  'tlscreds.c',
  'tlscredsanon.c',
  'tlscredspsk.c',
  'tlscredsx509.c',
  'tlssession.c',
  'rsakey.c',
))

crypto_ss.add(files('hash-glib.c', 'hmac-glib.c', 'pbkdf-stub.c'))

if have_keyring
  crypto_ss.add(files('secret_keyring.c'))
endif
util_ss.add(files(
  'aes.c',
  'clmul.c',
  'init.c',
  'sm4.c',
))

util_ss.add(files('random-platform.c'))
