system_ss.add(pixman)
specific_ss.add(when: ['CONFIG_SYSTEM_ONLY'], if_true: pixman)   # for the include path

system_ss.add(png)
system_ss.add(files(
  'console.c',
  'cursor.c',
  'dmabuf.c',
  'input-keymap.c',
  'input.c',
  'kbd-state.c',
  'qemu-pixman.c',
))
system_ss.add(files('console-vc-stubs.c'))
if is_android
  system_ss.add(opengl, android_native_window, files(
    'agl.c',
    'egl-helpers.c',
    'shader.c',
  ))
endif
if dbus_display
  error('DBus display was removed from this trimmed build')
endif

if host_os == 'linux'
  system_ss.add(files('udmabuf.c'))
endif
if cocoa.found()
  error('Cocoa UI was removed from this trimmed build')
endif

if dbus_display
  error('DBus UI was removed from this trimmed build')
endif

keymaps = [
  ['linux', 'qcode'],
  ['qcode', 'linux'],
  ['qnum', 'qcode'],
  ['usb', 'qcode'],
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
