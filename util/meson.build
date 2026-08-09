util_ss.add(files('osdep.c', 'cutils.c', 'unicode.c', 'qemu-timer-common.c'))
util_ss.add(files('thread-context.c'), numa)
if not config_host_data.get('CONFIG_ATOMIC64')
  error('atomic64 fallback was removed from this trimmed build')
endif
if host_os == 'windows'
  error('Windows util backend was removed from this trimmed build')
endif
util_ss.add(files('aio-posix.c'))
util_ss.add(files('fdmon-poll.c'))
if config_host_data.get('CONFIG_EPOLL_CREATE1')
  util_ss.add(files('fdmon-epoll.c'))
endif
util_ss.add(when: linux_io_uring, if_true: files('fdmon-io_uring.c'))
util_ss.add(files('compatfd.c'))
util_ss.add(files('event_notifier-posix.c'))
util_ss.add(files('mmap-alloc.c'))
util_ss.add(files('oslib-posix.c'))
util_ss.add(files('qemu-thread-posix.c'))
util_ss.add(files('memfd.c'))
util_ss.add(files('defer-call.c'))
util_ss.add(files('module.c'))
util_ss.add(files('bitmap.c', 'bitops.c'))
util_ss.add(files('cacheflush.c'))
util_ss.add(files('error.c', 'error-report.c'))
util_ss.add(files('qemu-print.c'))
util_ss.add(files('id.c'))
util_ss.add(files('qemu-config.c', 'notify.c'))
util_ss.add(files('qemu-option.c'))
util_ss.add(files('keyval.c'))
util_ss.add(files('uuid.c'))
util_ss.add(files('getauxval.c'))
util_ss.add(files('rcu.c'))
util_ss.add(files('log.c'))
util_ss.add(files('transactions.c'))
util_ss.add(files('guest-random.c'))
util_ss.add(files('memalign.c'))
util_ss.add(files('lockcnt.c'))

if have_user
  error('User-mode util support was removed from this trimmed build')
endif

if have_block or have_ga
  util_ss.add(files('aiocb.c', 'async.c'))
  util_ss.add(files('main-loop.c'))
  util_ss.add(files('qemu-coroutine.c', 'qemu-coroutine-lock.c'))
  util_ss.add(files(f'coroutine-@coroutine_backend@.c'))
  util_ss.add(files('thread-pool.c', 'qemu-timer.c'))
endif
if have_block or have_ga or have_user
  util_ss.add(files('qemu-sockets.c'))
endif
if have_block
  util_ss.add(files('aio-wait.c'))
  util_ss.add(files('bufferiszero.c'))
  util_ss.add(files('iov.c'))
  util_ss.add(files('block-helpers.c'))
  util_ss.add(files('qemu-coroutine-sleep.c'))
  util_ss.add(files('readline.c'))
  util_ss.add(files('timed-average.c'))
endif
