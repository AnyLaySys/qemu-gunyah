system_ss.add(files('control-target.c'))

foreach item : [ '.' ] + trace_events_subdirs + qapi_trace_events
  if item in qapi_trace_events
    trace_events_file = item
    group_name = item.full_path().split('/')[-1].underscorify()
  else
    trace_events_file = meson.project_source_root() / item / 'trace-events'
    group_name = item == '.' ? 'root' : item.underscorify()
  endif
  group = '--group=' + group_name
  fmt = '@0@-' + group_name + '.@1@'

  trace_h = custom_target(fmt.format('trace', 'h'),
                          output: fmt.format('trace', 'h'),
                          input: trace_events_file,
                          command: [ tracetool, group, '--format=h', '@INPUT@', '@OUTPUT@' ],
                          depend_files: tracetool_depends)
  genh += trace_h
  trace_c = custom_target(fmt.format('trace', 'c'),
                          output: fmt.format('trace', 'c'),
                          input: trace_events_file,
                          command: [ tracetool, group, '--format=c', '@INPUT@', '@OUTPUT@' ],
                          depend_files: tracetool_depends)
  trace_ss.add(trace_h, trace_c)
endforeach
trace_ss.add(files('control.c'))
if have_system or have_tools or have_ga
  trace_ss.add(files('qmp.c'))
endif
