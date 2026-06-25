block_ss.add(genh)
block_ss.add(files(
  'accounting.c',
  'aio_task.c',
  'block-backend.c',
  'graph-lock.c',
  'io.c',
  'null.c',
  'progress_meter.c',
  'crypto.c',
  'qcow2.c',
  'qcow2-cache.c',
  'qcow2-cluster.c',
  'qcow2-refcount.c',
  'qcow2-snapshot.c',
  'qcow2-threads.c',
  'raw-format.c',
  'reqlist.c',
), zstd, zlib)

system_ss.add(files('block-ram-registrar.c'))

block_ss.add(files('file-posix.c'), coref, iokit)

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
                                      '../include/block/block_int-io.h',
                                      '../include/block/block-global-state.h',
                                      '../include/system/block-backend-global-state.h',
                                      '../include/system/block-backend-io.h',
                                      'coroutines.h'
                                      ),
                            command: [wrapper_py, '@OUTPUT@', '@INPUT@'])
block_ss.add(block_gen_c)
