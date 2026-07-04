block_ss.add(genh)
block_ss.add(files(
  'accounting.c',
  'aio_task.c',
  'block-backend.c',
  'graph-lock.c',
  'io.c',
  'progress_meter.c',
  'raw-format.c',
  'reqlist.c',
))

system_ss.add(files('block-ram-registrar.c'))

block_ss.add(files('file-posix.c'), coref, iokit)
block_ss.add(when: linux_io_uring, if_true: files('io_uring.c'))

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
