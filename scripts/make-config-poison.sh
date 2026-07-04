#! /bin/sh

if test $# = 0; then
  exit 0
fi

exec sed -n \
  -e' /CONFIG_TCG/d' \
  -e '/CONFIG_USER_ONLY/d' \
  -e '/CONFIG_SOFTMMU/d' \
  -e '/^#define / {' \
  -e    's///' \
  -e    's/ .*//' \
  -e    's/^/#pragma GCC poison /p' \
  -e '}' "$@" | sort -u
