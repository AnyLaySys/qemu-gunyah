system_ss.add(pixman)
specific_ss.add(when: ['CONFIG_SYSTEM_ONLY'], if_true: pixman)   # for the include path
specific_ss.add(when: ['CONFIG_SYSTEM_ONLY'], if_true: opengl)   # for the include path

system_ss.add(png)
system_ss.add(files(
  'console.c',
  'cursor.c',
  'dmabuf.c',
  'input-keymap.c',
  'input-legacy.c',
  'input.c',
  'kbd-state.c',
  'keymaps.c',
  'qemu-pixman.c',
  'util.c',
))
system_ss.add(files('console-vc-stubs.c'))
if dbus_display
  error('DBus display was removed from this trimmed build')
endif

if host_os == 'linux'
  system_ss.add(files('udmabuf.c'))
endif
if cocoa.found()
  error('Cocoa UI was removed from this trimmed build')
endif

ui_modules = {}

system_ss.add(opengl)
if opengl.found()
  opengl_ss = ss.source_set()
  opengl_ss.add(pixman)
  opengl_ss.add(when: [opengl],
                if_true: files('shader.c', 'console-gl.c',
                               'egl-helpers.c', 'egl-context.c'))
  ui_modules += {'opengl' : opengl_ss}
endif

if dbus_display
  error('DBus UI was removed from this trimmed build')
endif

if sdl.found()
  sdl_ss = ss.source_set()
  sdl_ss.add(sdl, pixman, glib, files(
    'sdl2-2d.c',
    'sdl2-input.c',
    'sdl2.c',
  ))
  sdl_ss.add(when: opengl, if_true: files('sdl2-gl.c'))
  ui_modules += {'sdl' : sdl_ss}
endif

keymaps = [
  ['atset1', 'qcode'],
  ['linux', 'qcode'],
  ['qcode', 'atset1'],
  ['qcode', 'atset2'],
  ['qcode', 'atset3'],
  ['qcode', 'linux'],
  ['qcode', 'qnum'],
  ['qcode', 'sun'],
  ['qnum', 'qcode'],
  ['usb', 'qcode'],
  ['win32', 'qcode'],
  ['x11', 'qcode'],
  ['xorgevdev', 'qcode'],
  ['xorgkbd', 'qcode'],
  ['xorgxquartz', 'qcode'],
  ['xorgxwin', 'qcode'],
  ['osx', 'qcode'],
]

if have_system or xkbcommon.found()
  keycodemapdb_proj = subproject('keycodemapdb', required: true)
  foreach e : keymaps
    output = 'input-keymap-@0@-to-@1@.c.inc'.format(e[0], e[1])
    genh += custom_target(output,
                  output: output,
                  capture: true,
                  input: keycodemapdb_proj.get_variable('keymaps_csv'),
                  command: [python, keycodemapdb_proj.get_variable('keymap_gen').full_path(),
                            'code-map', '--lang', 'glib2',
                            '--varname', 'qemu_input_map_@0@_to_@1@'.format(e[0], e[1]),
                            '@INPUT0@', e[0], e[1]])
  endforeach
endif

modules += {'ui': ui_modules}
