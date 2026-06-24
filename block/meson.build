block_ss.add(genh)
block_ss.add(files(
  'accounting.c',
  'aio_task.c',
  'amend.c',
  'backup.c',
  'blkdebug.c',
  'blklogwrites.c',
  'blkverify.c',
  'block-backend.c',
  'block-copy.c',
  'commit.c',
  'copy-before-write.c',
  'copy-on-read.c',
  'create.c',
  'crypto.c',
  'dirty-bitmap.c',
  'filter-compress.c',
  'graph-lock.c',
  'io.c',
  'mirror.c',
  'nbd.c',
  'null.c',
  'preallocate.c',
  'progress_meter.c',
  'qapi.c',
  'qcow2.c',
  'qcow2-bitmap.c',
  'qcow2-cache.c',
  'qcow2-cluster.c',
  'qcow2-refcount.c',
  'qcow2-snapshot.c',
  'qcow2-threads.c',
  'quorum.c',
  'raw-format.c',
  'reqlist.c',
  'snapshot.c',
  'snapshot-access.c',
  'throttle.c',
  'throttle-groups.c',
  'write-threshold.c',
), zstd, zlib)

system_ss.add(when: 'CONFIG_TCG', if_true: files('blkreplay.c'))
system_ss.add(files('block-ram-registrar.c'))

if get_option('cloop').allowed()
  block_ss.add(files('cloop.c'))
endif

block_ss.add(files('file-posix.c'), coref, iokit)
if host_os == 'linux'
  block_ss.add(files('nvme.c'))
endif

module_block_py = find_program('../scripts/modules/module_block.py')
module_block_h = custom_target('module_block.h',
                               output: 'module_block.h',
                               input: [],
                               command: [module_block_py, '@OUTPUT0@'])
block_ss.add(module_block_h)

wrapper_py = find_program('../scripts/block-coroutine-wrapper.py')
block_gen_c = custom_target('block-gen.c',
                            output: 'block-gen.c',
                            input: files(
                                      '../include/block/block-io.h',
                                      '../include/block/dirty-bitmap.h',
                                      '../include/block/block_int-io.h',
                                      '../include/block/block-global-state.h',
                                      '../include/system/block-backend-global-state.h',
                                      '../include/system/block-backend-io.h',
                                      'coroutines.h'
                                      ),
                            command: [wrapper_py, '@OUTPUT@', '@INPUT@'])
block_ss.add(block_gen_c)

block_ss.add(files('stream.c'))

system_ss.add(files('qapi-system.c'))

subdir('export')
subdir('monitor')
