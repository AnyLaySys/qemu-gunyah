HXCOMM See docs/devel/docs.rst for the format of this file.
HXCOMM
HXCOMM This file defines the contents of an array of HMPCommand structs
HXCOMM which specify the name, behaviour and help text for HMP commands.
HXCOMM Text between SRST and ERST is rST format documentation.
HXCOMM HXCOMM can be used for comments, discarded from both rST and C.


    {
        .name       = "help|?",
        .args_type  = "name:S?",
        .params     = "[cmd]",
        .help       = "show the help",
        .cmd        = hmp_help,
        .flags      = "p",
    },

SRST
``help`` or ``?`` [*cmd*]
  Show the help for all commands or just for command *cmd*.
ERST

    {
        .name       = "quit|q",
        .args_type  = "",
        .params     = "",
        .help       = "quit the emulator",
        .cmd        = hmp_quit,
        .flags      = "p",
    },

SRST
``quit`` or ``q``
  Quit the emulator.
ERST

    {
        .name       = "exit_preconfig",
        .args_type  = "",
        .params     = "",
        .help       = "exit the preconfig state",
        .cmd        = hmp_exit_preconfig,
        .flags      = "p",
    },

SRST
``exit_preconfig``
  This command makes QEMU exit the preconfig state and proceed with
  VM initialization using configuration data provided on the command line
  and via the QMP monitor during the preconfig state. The command is only
  available during the preconfig state (i.e. when the --preconfig command
  line option was in use).
ERST

#ifdef CONFIG_PIXMAN
    {
        .name       = "screendump",
        .args_type  = "filename:F,format:-fs,device:s?,head:i?",
        .params     = "filename [-f format] [device [head]]",
        .help       = "save screen from head 'head' of display device 'device'"
                      "in specified format 'format' as image 'filename'."
                      "Currently only 'png' and 'ppm' formats are supported.",
         .cmd        = hmp_screendump,
        .coroutine  = true,
    },

SRST
``screendump`` *filename*
  Save screen into PPM image *filename*.
ERST
#endif

    {
        .name       = "logfile",
        .args_type  = "filename:F",
        .params     = "filename",
        .help       = "output logs to 'filename'",
        .cmd        = hmp_logfile,
    },

SRST
``logfile`` *filename*
  Output logs to *filename*.
ERST

    {
        .name       = "trace-event",
        .args_type  = "name:s,option:b,vcpu:i?",
        .params     = "name on|off [vcpu]",
        .help       = "changes status of a specific trace event "
                      "(vcpu: vCPU to set, default is all)",
        .cmd = hmp_trace_event,
        .command_completion = trace_event_completion,
    },

SRST
``trace-event``
  changes status of a trace event
ERST

#if defined(CONFIG_TRACE_SIMPLE)
    {
        .name       = "trace-file",
        .args_type  = "op:s?,arg:F?",
        .params     = "on|off|flush|set [arg]",
        .help       = "open, close, or flush trace file, or set a new file name",
        .cmd        = hmp_trace_file,
    },

SRST
``trace-file on|off|flush``
  Open, close, or flush the trace file.  If no argument is given, the
  status of the trace file is displayed.
ERST
#endif

    {
        .name       = "log",
        .args_type  = "items:s",
        .params     = "item1[,...]",
        .help       = "activate logging of the specified items",
        .cmd        = hmp_log,
    },

SRST
``log`` *item1*\ [,...]
  Activate logging of the specified items.
ERST

    {
        .name       = "savevm",
        .args_type  = "name:s?",
        .params     = "tag",
        .help       = "save a VM snapshot. If no tag is provided, a new snapshot is created",
        .cmd        = hmp_savevm,
    },

SRST
``savevm`` *tag*
  Create a snapshot of the whole virtual machine. If *tag* is
  provided, it is used as human readable identifier. If there is already
  a snapshot with the same tag, it is replaced. More info at
  :ref:`vm_005fsnapshots`.

  Since 4.0, savevm stopped allowing the snapshot id to be set, accepting
  only *tag* as parameter.
ERST

    {
        .name       = "loadvm",
        .args_type  = "name:s",
        .params     = "tag",
        .help       = "restore a VM snapshot from its tag",
        .cmd        = hmp_loadvm,
        .command_completion = loadvm_completion,
    },

SRST
``loadvm`` *tag*
  Set the whole virtual machine to the snapshot identified by the tag
  *tag*.

  Since 4.0, loadvm stopped accepting snapshot id as parameter.
ERST

    {
        .name       = "delvm",
        .args_type  = "name:s",
        .params     = "tag",
        .help       = "delete a VM snapshot from its tag",
        .cmd        = hmp_delvm,
        .command_completion = delvm_completion,
    },

SRST
``delvm`` *tag*
  Delete the snapshot identified by *tag*.

  Since 4.0, delvm stopped deleting snapshots by snapshot id, accepting
  only *tag* as parameter.
ERST

    {
        .name       = "one-insn-per-tb",
        .args_type  = "option:s?",
        .params     = "[on|off]",
        .help       = "run emulation with one guest instruction per translation block",
        .cmd        = hmp_one_insn_per_tb,
    },

SRST
``one-insn-per-tb [off]``
  Run the emulation with one guest instruction per translation block.
  This slows down emulation a lot, but can be useful in some situations,
  such as when trying to analyse the logs produced by the ``-d`` option.
  This only has an effect when using TCG, not with KVM or other accelerators.

  If called with option off, the emulation returns to normal mode.
ERST

    {
        .name       = "stop|s",
        .args_type  = "",
        .params     = "",
        .help       = "stop emulation",
        .cmd        = hmp_stop,
    },

SRST
``stop`` or ``s``
  Stop emulation.
ERST

    {
        .name       = "cont|c",
        .args_type  = "",
        .params     = "",
        .help       = "resume emulation",
        .cmd        = hmp_cont,
    },

SRST
``cont`` or ``c``
  Resume emulation.
ERST

    {
        .name       = "system_wakeup",
        .args_type  = "",
        .params     = "",
        .help       = "wakeup guest from suspend",
        .cmd        = hmp_system_wakeup,
    },

SRST
``system_wakeup``
  Wakeup guest from suspend.
ERST

    {
        .name       = "gdbserver",
        .args_type  = "device:s?",
        .params     = "[device]",
        .help       = "start gdbserver on given device (default 'tcp::1234'), stop with 'none'",
        .cmd        = hmp_gdbserver,
    },

SRST
``gdbserver`` [*port*]
  Start gdbserver session (default *port*\=1234)
ERST

    {
        .name       = "x",
        .args_type  = "fmt:/,addr:l",
        .params     = "/fmt addr",
        .help       = "virtual memory dump starting at 'addr'",
        .cmd        = hmp_memory_dump,
    },

SRST
``x/``\ *fmt* *addr*
  Virtual memory dump starting at *addr*.
ERST

    {
        .name       = "xp",
        .args_type  = "fmt:/,addr:l",
        .params     = "/fmt addr",
        .help       = "physical memory dump starting at 'addr'",
        .cmd        = hmp_physical_memory_dump,
    },

SRST
``xp /``\ *fmt* *addr*
  Physical memory dump starting at *addr*.

  *fmt* is a format which tells the command how to format the
  data. Its syntax is: ``/{count}{format}{size}``

  *count*
    is the number of items to be dumped.
  *format*
    can be x (hex), d (signed decimal), u (unsigned decimal), o (octal),
    c (char) or i (asm instruction).
  *size*
    can be b (8 bits), h (16 bits), w (32 bits) or g (64 bits). On x86,
    ``h`` or ``w`` can be specified with the ``i`` format to
    respectively select 16 or 32 bit code instruction size.

  Examples:

  Dump 10 instructions at the current instruction pointer::

    (qemu) x/10i $eip
    0x90107063:  ret
    0x90107064:  sti
    0x90107065:  lea    0x0(%esi,1),%esi
    0x90107069:  lea    0x0(%edi,1),%edi
    0x90107070:  ret
    0x90107071:  jmp    0x90107080
    0x90107073:  nop
    0x90107074:  nop
    0x90107075:  nop
    0x90107076:  nop

  Dump 80 16 bit values at the start of the video memory::

    (qemu) xp/80hx 0xb8000
    0x000b8000: 0x0b50 0x0b6c 0x0b65 0x0b78 0x0b38 0x0b36 0x0b2f 0x0b42
    0x000b8010: 0x0b6f 0x0b63 0x0b68 0x0b73 0x0b20 0x0b56 0x0b47 0x0b41
    0x000b8020: 0x0b42 0x0b69 0x0b6f 0x0b73 0x0b20 0x0b63 0x0b75 0x0b72
    0x000b8030: 0x0b72 0x0b65 0x0b6e 0x0b74 0x0b2d 0x0b63 0x0b76 0x0b73
    0x000b8040: 0x0b20 0x0b30 0x0b35 0x0b20 0x0b4e 0x0b6f 0x0b76 0x0b20
    0x000b8050: 0x0b32 0x0b30 0x0b30 0x0b33 0x0720 0x0720 0x0720 0x0720
    0x000b8060: 0x0720 0x0720 0x0720 0x0720 0x0720 0x0720 0x0720 0x0720
    0x000b8070: 0x0720 0x0720 0x0720 0x0720 0x0720 0x0720 0x0720 0x0720
    0x000b8080: 0x0720 0x0720 0x0720 0x0720 0x0720 0x0720 0x0720 0x0720
    0x000b8090: 0x0720 0x0720 0x0720 0x0720 0x0720 0x0720 0x0720 0x0720

ERST

    {
        .name       = "gpa2hva",
        .args_type  = "addr:l",
        .params     = "addr",
        .help       = "print the host virtual address corresponding to a guest physical address",
        .cmd        = hmp_gpa2hva,
    },

SRST
``gpa2hva`` *addr*
  Print the host virtual address at which the guest's physical address *addr*
  is mapped.
ERST

#ifdef CONFIG_LINUX
    {
        .name       = "gpa2hpa",
        .args_type  = "addr:l",
        .params     = "addr",
        .help       = "print the host physical address corresponding to a guest physical address",
        .cmd        = hmp_gpa2hpa,
    },
#endif

SRST
``gpa2hpa`` *addr*
  Print the host physical address at which the guest's physical address *addr*
  is mapped.
ERST

    {
        .name       = "gva2gpa",
        .args_type  = "addr:l",
        .params     = "addr",
        .help       = "print the guest physical address corresponding to a guest virtual address",
        .cmd        = hmp_gva2gpa,
    },

SRST
``gva2gpa`` *addr*
  Print the guest physical address at which the guest's virtual address *addr*
  is mapped based on the mapping for the current CPU.
ERST

    {
        .name       = "print|p",
        .args_type  = "fmt:/,val:l",
        .params     = "/fmt expr",
        .help       = "print expression value (use $reg for CPU register access)",
        .cmd        = hmp_print,
    },

SRST
``print`` or ``p/``\ *fmt* *expr*
  Print expression value. Only the *format* part of *fmt* is
  used.
ERST

    {
        .name       = "i",
        .args_type  = "fmt:/,addr:i,index:i.",
        .params     = "/fmt addr",
        .help       = "I/O port read",
        .cmd        = hmp_ioport_read,
    },

SRST
``i/``\ *fmt* *addr* [.\ *index*\ ]
  Read I/O port.
ERST

    {
        .name       = "o",
        .args_type  = "fmt:/,addr:i,val:i",
        .params     = "/fmt addr value",
        .help       = "I/O port write",
        .cmd        = hmp_ioport_write,
    },

SRST
``o/``\ *fmt* *addr* *val*
  Write to I/O port.
ERST

    {
        .name       = "sendkey",
        .args_type  = "keys:s,hold-time:i?",
        .params     = "keys [hold_ms]",
        .help       = "send keys to the VM (e.g. 'sendkey ctrl-alt-f1', default hold time=100 ms)",
        .cmd        = hmp_sendkey,
        .command_completion = sendkey_completion,
    },

SRST
``sendkey`` *keys*
  Send *keys* to the guest. *keys* could be the name of the
  key or the raw value in hexadecimal format. Use ``-`` to press
  several keys simultaneously. Example::

    sendkey ctrl-alt-f1

  This command is useful to send keys that your graphical user interface
  intercepts at low level, such as ``ctrl-alt-f1`` in X Window.
ERST
    {
        .name       = "sync-profile",
        .args_type  = "op:s?",
        .params     = "[on|off|reset]",
        .help       = "enable, disable or reset synchronization profiling. "
                      "With no arguments, prints whether profiling is on or off.",
        .cmd        = hmp_sync_profile,
    },

SRST
``sync-profile [on|off|reset]``
  Enable, disable or reset synchronization profiling. With no arguments, prints
  whether profiling is on or off.
ERST

    {
        .name       = "system_reset",
        .args_type  = "",
        .params     = "",
        .help       = "reset the system",
        .cmd        = hmp_system_reset,
    },

SRST
``system_reset``
  Reset the system.
ERST

    {
        .name       = "system_powerdown",
        .args_type  = "",
        .params     = "",
        .help       = "send system power down event",
        .cmd        = hmp_system_powerdown,
    },

SRST
``system_powerdown``
  Power down the system (if supported).
ERST

    {
        .name       = "sum",
        .args_type  = "start:i,size:i",
        .params     = "addr size",
        .help       = "compute the checksum of a memory region",
        .cmd        = hmp_sum,
    },

SRST
``sum`` *addr* *size*
  Compute the checksum of a memory region.
ERST

    {
        .name       = "device_add",
        .args_type  = "device:O",
        .params     = "driver[,prop=value][,...]",
        .help       = "add device, like -device on the command line",
        .cmd        = hmp_device_add,
        .command_completion = device_add_completion,
    },

SRST
``device_add`` *config*
  Add device.
ERST

    {
        .name       = "device_del",
        .args_type  = "id:s",
        .params     = "device",
        .help       = "remove device",
        .cmd        = hmp_device_del,
        .command_completion = device_del_completion,
    },

SRST
``device_del`` *id*
  Remove device *id*. *id* may be a short ID
  or a QOM object path.
ERST

    {
        .name       = "cpu",
        .args_type  = "index:i",
        .params     = "index",
        .help       = "set the default CPU",
        .cmd        = hmp_cpu,
    },

SRST
``cpu`` *index*
  Set the default CPU.
ERST

    {
        .name       = "mouse_move",
        .args_type  = "dx_str:s,dy_str:s,dz_str:s?",
        .params     = "dx dy [dz]",
        .help       = "send mouse move events",
        .cmd        = hmp_mouse_move,
    },

SRST
``mouse_move`` *dx* *dy* [*dz*]
  Move the active mouse to the specified coordinates *dx* *dy*
  with optional scroll axis *dz*.
ERST

    {
        .name       = "mouse_button",
        .args_type  = "button_state:i",
        .params     = "state",
        .help       = "change mouse button state (1=L, 2=M, 4=R)",
        .cmd        = hmp_mouse_button,
    },

SRST
``mouse_button`` *val*
  Change the active mouse button state *val* (1=L, 2=M, 4=R).
ERST

    {
        .name       = "mouse_set",
        .args_type  = "index:i",
        .params     = "index",
        .help       = "set which mouse device receives events",
        .cmd        = hmp_mouse_set,
    },

SRST
``mouse_set`` *index*
  Set which mouse device receives events at given *index*, index
  can be obtained with::

    info mice

ERST

    {
        .name       = "memsave",
        .args_type  = "val:l,size:i,filename:s",
        .params     = "addr size file",
        .help       = "save to disk virtual memory dump starting at 'addr' of size 'size'",
        .cmd        = hmp_memsave,
    },

SRST
``memsave`` *addr* *size* *file*
  save to disk virtual memory dump starting at *addr* of size *size*.
ERST

    {
        .name       = "pmemsave",
        .args_type  = "val:l,size:i,filename:s",
        .params     = "addr size file",
        .help       = "save to disk physical memory dump starting at 'addr' of size 'size'",
        .cmd        = hmp_pmemsave,
    },

SRST
``pmemsave`` *addr* *size* *file*
  save to disk physical memory dump starting at *addr* of size *size*.
ERST

    {
        .name       = "boot_set",
        .args_type  = "bootdevice:s",
        .params     = "bootdevice",
        .help       = "define new values for the boot device list",
        .cmd        = hmp_boot_set,
    },

SRST
``boot_set`` *bootdevicelist*
  Define new values for the boot device list. Those values will override
  the values specified on the command line through the ``-boot`` option.

  The values that can be specified here depend on the machine type, but are
  the same that can be specified in the ``-boot`` command line option.
ERST

    {
        .name       = "nmi",
        .args_type  = "",
        .params     = "",
        .help       = "inject an NMI",
        .cmd        = hmp_nmi,
    },
SRST
``nmi`` *cpu*
  Inject an NMI on the default CPU (x86/s390) or all CPUs (ppc64).
ERST

    {
        .name       = "ringbuf_write",
        .args_type  = "device:s,data:s",
        .params     = "device data",
        .help       = "Write to a ring buffer character device",
        .cmd        = hmp_ringbuf_write,
        .command_completion = ringbuf_write_completion,
    },

SRST
``ringbuf_write`` *device* *data*
  Write *data* to ring buffer character device *device*.
  *data* must be a UTF-8 string.
ERST

    {
        .name       = "ringbuf_read",
        .args_type  = "device:s,size:i",
        .params     = "device size",
        .help       = "Read from a ring buffer character device",
        .cmd        = hmp_ringbuf_read,
        .command_completion = ringbuf_write_completion,
    },

SRST
``ringbuf_read`` *device*
  Read and print up to *size* bytes from ring buffer character
  device *device*.
  Certain non-printable characters are printed ``\uXXXX``, where ``XXXX`` is the
  character code in hexadecimal.  Character ``\`` is printed ``\\``.
  Bug: can screw up when the buffer contains invalid UTF-8 sequences,
  NUL characters, after the ring buffer lost data, and when reading
  stops because the size limit is reached.
ERST

    {
        .name       = "announce_self",
        .args_type  = "interfaces:s?,id:s?",
        .params     = "[interfaces] [id]",
        .help       = "Trigger GARP/RARP announcements",
        .cmd        = hmp_announce_self,
    },

SRST
``announce_self``
  Trigger a round of GARP/RARP broadcasts; this is useful for explicitly
  updating the network infrastructure after a reconfiguration or some forms
  of migration. The timings of the round are set by the migration announce
  parameters. An optional comma separated *interfaces* list restricts the
  announce to the named set of interfaces. An optional *id* can be used to
  start a separate announce timer and to change the parameters of it later.
ERST

    {
        .name       = "migrate",
        .args_type  = "detach:-d,resume:-r,uri:s",
        .params     = "[-d] [-r] uri",
        .help       = "migrate to URI (using -d to not wait for completion)"
		      "\n\t\t\t -r to resume a paused postcopy migration",
        .cmd        = hmp_migrate,
    },


SRST
``migrate [-d] [-r]`` *uri*
  Migrate the VM to *uri*.

  ``-d``
    Start the migration process, but do not wait for its completion.  To
    query an ongoing migration process, use "info migrate".
  ``-r``
    Resume a paused postcopy migration.
ERST

    {
        .name       = "migrate_cancel",
        .args_type  = "",
        .params     = "",
        .help       = "cancel the current VM migration",
        .cmd        = hmp_migrate_cancel,
    },

SRST
``migrate_cancel``
  Cancel the current VM migration.
ERST

    {
        .name       = "migrate_continue",
        .args_type  = "state:s",
        .params     = "state",
        .help       = "Continue migration from the given paused state",
        .cmd        = hmp_migrate_continue,
    },
SRST
``migrate_continue`` *state*
  Continue migration from the paused state *state*
ERST

    {
        .name       = "migrate_incoming",
        .args_type  = "uri:s",
        .params     = "uri",
        .help       = "Continue an incoming migration from an -incoming defer",
        .cmd        = hmp_migrate_incoming,
    },

SRST
``migrate_incoming`` *uri*
  Continue an incoming migration using the *uri* (that has the same syntax
  as the ``-incoming`` option).
ERST

    {
        .name       = "migrate_recover",
        .args_type  = "uri:s",
        .params     = "uri",
        .help       = "Continue a paused incoming postcopy migration",
        .cmd        = hmp_migrate_recover,
    },

SRST
``migrate_recover`` *uri*
  Continue a paused incoming postcopy migration using the *uri*.
ERST

    {
        .name       = "migrate_pause",
        .args_type  = "",
        .params     = "",
        .help       = "Pause an ongoing migration (postcopy-only)",
        .cmd        = hmp_migrate_pause,
    },

SRST
``migrate_pause``
  Pause an ongoing migration.  Currently it only supports postcopy.
ERST

    {
        .name       = "migrate_set_capability",
        .args_type  = "capability:s,state:b",
        .params     = "capability state",
        .help       = "Enable/Disable the usage of a capability for migration",
        .cmd        = hmp_migrate_set_capability,
        .command_completion = migrate_set_capability_completion,
    },

SRST
``migrate_set_capability`` *capability* *state*
  Enable/Disable the usage of a capability *capability* for migration.
ERST

    {
        .name       = "migrate_set_parameter",
        .args_type  = "parameter:s,value:s",
        .params     = "parameter value",
        .help       = "Set the parameter for migration",
        .cmd        = hmp_migrate_set_parameter,
        .command_completion = migrate_set_parameter_completion,
    },

SRST
``migrate_set_parameter`` *parameter* *value*
  Set the parameter *parameter* for migration.
ERST

    {
        .name       = "migrate_start_postcopy",
        .args_type  = "",
        .params     = "",
        .help       = "Followup to a migration command to switch the migration"
                      " to postcopy mode. The postcopy-ram capability must "
                      "be set on both source and destination before the "
                      "original migration command .",
        .cmd        = hmp_migrate_start_postcopy,
    },

SRST
``migrate_start_postcopy``
  Switch in-progress migration to postcopy mode. Ignored after the end of
  migration (or once already in postcopy).
ERST

#ifdef CONFIG_REPLICATION
    {
        .name       = "x_colo_lost_heartbeat",
        .args_type  = "",
        .params     = "",
        .help       = "Tell COLO that heartbeat is lost,\n\t\t\t"
                      "a failover or takeover is needed.",
        .cmd = hmp_x_colo_lost_heartbeat,
    },
#endif

SRST
``x_colo_lost_heartbeat``
  Tell COLO that heartbeat is lost, a failover or takeover is needed.
ERST

    {
        .name       = "client_migrate_info",
        .args_type  = "protocol:s,hostname:s,port:i?,tls-port:i?,cert-subject:s?",
        .params     = "protocol hostname port tls-port cert-subject",
        .help       = "set migration information for remote display",
        .cmd        = hmp_client_migrate_info,
    },

SRST
``client_migrate_info`` *protocol* *hostname* *port* *tls-port* *cert-subject*
  Set migration information for remote display.  This makes the server
  ask the client to automatically reconnect using the new parameters
  once migration finished successfully.  Only implemented for SPICE.
ERST

    {
        .name       = "dump-guest-memory",
        .args_type  = "paging:-p,detach:-d,windmp:-w,zlib:-z,lzo:-l,snappy:-s,raw:-R,filename:F,begin:l?,length:l?",
        .params     = "[-p] [-d] [-z|-l|-s|-w] [-R] filename [begin length]",
        .help       = "dump guest memory into file 'filename'.\n\t\t\t"
                      "-p: do paging to get guest's memory mapping.\n\t\t\t"
                      "-d: return immediately (do not wait for completion).\n\t\t\t"
                      "-z: dump in kdump-compressed format, with zlib compression.\n\t\t\t"
                      "-l: dump in kdump-compressed format, with lzo compression.\n\t\t\t"
                      "-s: dump in kdump-compressed format, with snappy compression.\n\t\t\t"
                      "-R: when using kdump (-z, -l, -s), use raw rather than makedumpfile-flattened\n\t\t\t"
                      "    format\n\t\t\t"
                      "-w: dump in Windows crashdump format (can be used instead of ELF-dump converting),\n\t\t\t"
                      "    for Windows x86 and x64 guests with vmcoreinfo driver only.\n\t\t\t"
                      "begin: the starting physical address.\n\t\t\t"
                      "length: the memory size, in bytes.",
        .cmd        = hmp_dump_guest_memory,
    },

SRST
``dump-guest-memory [-p]`` *filename* *begin* *length*
  \ 
``dump-guest-memory [-z|-l|-s|-w]`` *filename*
  Dump guest memory to *protocol*. The file can be processed with crash or
  gdb. Without ``-z|-l|-s|-w``, the dump format is ELF.

  ``-p``
    do paging to get guest's memory mapping.
  ``-z``
    dump in kdump-compressed format, with zlib compression.
  ``-l``
    dump in kdump-compressed format, with lzo compression.
  ``-s``
    dump in kdump-compressed format, with snappy compression.
  ``-R``
    when using kdump (-z, -l, -s), use raw rather than makedumpfile-flattened
    format
  ``-w``
    dump in Windows crashdump format (can be used instead of ELF-dump converting),
    for Windows x64 guests with vmcoreinfo driver only
  *filename*
    dump file name.
  *begin*
    the starting physical address. It's optional, and should be
    specified together with *length*.
  *length*
    the memory size, in bytes. It's optional, and should be specified
    together with *begin*.

ERST

#if defined(TARGET_S390X)
    {
        .name       = "dump-skeys",
        .args_type  = "filename:F",
        .params     = "",
        .help       = "Save guest storage keys into file 'filename'.\n",
        .cmd        = hmp_dump_skeys,
    },
#endif

SRST
``dump-skeys`` *filename*
  Save guest storage keys to a file.
ERST

#if defined(TARGET_S390X)
    {
        .name       = "migration_mode",
        .args_type  = "mode:i",
        .params     = "mode",
        .help       = "Enables or disables migration mode\n",
        .cmd        = hmp_migrationmode,
    },
#endif

SRST
``migration_mode`` *mode*
  Enables or disables migration mode.
ERST

    {
        .name       = "pcie_aer_inject_error",
        .args_type  = "advisory_non_fatal:-a,correctable:-c,"
	              "id:s,error_status:s,"
	              "header0:i?,header1:i?,header2:i?,header3:i?,"
	              "prefix0:i?,prefix1:i?,prefix2:i?,prefix3:i?",
        .params     = "[-a] [-c] id "
                      "<error_status> [<tlp header> [<tlp header prefix>]]",
        .help       = "inject pcie aer error\n\t\t\t"
	              " -a for advisory non fatal error\n\t\t\t"
	              " -c for correctable error\n\t\t\t"
                      "<id> = qdev device id\n\t\t\t"
                      "<error_status> = error string or 32bit\n\t\t\t"
                      "<tlp header> = 32bit x 4\n\t\t\t"
                      "<tlp header prefix> = 32bit x 4",
        .cmd        = hmp_pcie_aer_inject_error,
    },

SRST
``pcie_aer_inject_error``
  Inject PCIe AER error
ERST

    {
        .name       = "netdev_add",
        .args_type  = "netdev:O",
        .params     = "[user],id=str[,prop=value][,...]",
        .help       = "add host network device",
        .cmd        = hmp_netdev_add,
        .command_completion = netdev_add_completion,
        .flags      = "p",
    },

SRST
``netdev_add``
  Add host network device.
ERST

    {
        .name       = "netdev_del",
        .args_type  = "id:s",
        .params     = "id",
        .help       = "remove host network device",
        .cmd        = hmp_netdev_del,
        .command_completion = netdev_del_completion,
        .flags      = "p",
    },

SRST
``netdev_del``
  Remove host network device.
ERST

    {
        .name       = "object_add",
        .args_type  = "object:S",
        .params     = "[qom-type=]type,id=str[,prop=value][,...]",
        .help       = "create QOM object",
        .cmd        = hmp_object_add,
        .command_completion = object_add_completion,
        .flags      = "p",
    },

SRST
``object_add``
  Create QOM object.
ERST

    {
        .name       = "object_del",
        .args_type  = "id:s",
        .params     = "id",
        .help       = "destroy QOM object",
        .cmd        = hmp_object_del,
        .command_completion = object_del_completion,
        .flags      = "p",
    },

SRST
``object_del``
  Destroy QOM object.
ERST

#ifdef CONFIG_SLIRP
    {
        .name       = "hostfwd_add",
        .args_type  = "arg1:s,arg2:s?",
        .params     = "[netdev_id] [tcp|udp]:[hostaddr]:hostport-[guestaddr]:guestport",
        .help       = "redirect TCP or UDP connections from host to guest (requires -net user)",
        .cmd        = hmp_hostfwd_add,
    },
#endif
SRST
``hostfwd_add``
  Redirect TCP or UDP connections from host to guest (requires -net user).
ERST

#ifdef CONFIG_SLIRP
    {
        .name       = "hostfwd_remove",
        .args_type  = "arg1:s,arg2:s?",
        .params     = "[netdev_id] [tcp|udp]:[hostaddr]:hostport",
        .help       = "remove host-to-guest TCP or UDP redirection",
        .cmd        = hmp_hostfwd_remove,
    },

#endif
SRST
``hostfwd_remove``
  Remove host-to-guest TCP or UDP redirection.
ERST

    {
        .name       = "balloon",
        .args_type  = "value:M",
        .params     = "target",
        .help       = "request VM to change its memory allocation (in MB)",
        .cmd        = hmp_balloon,
    },

SRST
``balloon`` *value*
  Request VM to change its memory allocation to *value* (in MB).
ERST

    {
        .name       = "set_link",
        .args_type  = "name:s,up:b",
        .params     = "name on|off",
        .help       = "change the link status of a network adapter",
        .cmd        = hmp_set_link,
        .command_completion = set_link_completion,
    },

SRST
``set_link`` *name* ``[on|off]``
  Switch link *name* on (i.e. up) or off (i.e. down).
ERST

    {
        .name       = "watchdog_action",
        .args_type  = "action:s",
        .params     = "[reset|shutdown|poweroff|pause|debug|none|inject-nmi]",
        .help       = "change watchdog action",
        .cmd        = hmp_watchdog_action,
        .command_completion = watchdog_action_completion,
    },

SRST
``watchdog_action``
  Change watchdog action.
ERST

#if defined(TARGET_I386)

    {
        .name       = "mce",
        .args_type  = "broadcast:-b,cpu_index:i,bank:i,status:l,mcg_status:l,addr:l,misc:l",
        .params     = "[-b] cpu bank status mcgstatus addr misc",
        .help       = "inject a MCE on the given CPU [and broadcast to other CPUs with -b option]",
        .cmd        = hmp_mce,
    },

#endif
SRST
``mce`` *cpu* *bank* *status* *mcgstatus* *addr* *misc*
  Inject an MCE on the given CPU (x86 only).
ERST

#ifdef CONFIG_POSIX
    {
        .name       = "getfd",
        .args_type  = "fdname:s",
        .params     = "getfd name",
        .help       = "receive a file descriptor via SCM rights and assign it a name",
        .cmd        = hmp_getfd,
        .flags      = "p",
    },

SRST
``getfd`` *fdname*
  If a file descriptor is passed alongside this command using the SCM_RIGHTS
  mechanism on unix sockets, it is stored using the name *fdname* for
  later use by other monitor commands.
ERST
#endif

    {
        .name       = "closefd",
        .args_type  = "fdname:s",
        .params     = "closefd name",
        .help       = "close a file descriptor previously passed via SCM rights",
        .cmd        = hmp_closefd,
        .flags      = "p",
    },

SRST
``closefd`` *fdname*
  Close the file descriptor previously assigned to *fdname* using the
  ``getfd`` command. This is only needed if the file descriptor was never
  used by another monitor command.
ERST

    {
        .name       = "set_password",
        .args_type  = "protocol:s,password:s,display:-ds,connected:s?",
        .params     = "protocol password [-d display] [action-if-connected]",
        .help       = "set spice password",
        .cmd        = hmp_set_password,
    },

SRST
``set_password spice password [ action-if-connected ]``
  Change spice password.  *action-if-connected* specifies
  what should happen in case a connection is established: *fail* makes
  the password change fail.  *disconnect* changes the password and
  disconnects the client.  *keep* changes the password and keeps the
  connection up.  *keep* is the default.
ERST

    {
        .name       = "expire_password",
        .args_type  = "protocol:s,time:s,display:-ds",
        .params     = "protocol time [-d display]",
        .help       = "set spice password expire-time",
        .cmd        = hmp_expire_password,
    },

SRST
``expire_password spice expire-time``
  Specify when a password for spice becomes invalid.
  *expire-time* accepts:

  ``now``
    Invalidate password instantly.
  ``never``
    Password stays valid forever.
  ``+``\ *nsec*
    Password stays valid for *nsec* seconds starting now.
  *nsec*
    Password is invalidated at the given time.  *nsec* are the seconds
    passed since 1970, i.e. unix epoch.

ERST

    {
        .name       = "chardev-add",
        .args_type  = "args:s",
        .params     = "args",
        .help       = "add chardev",
        .cmd        = hmp_chardev_add,
        .command_completion = chardev_add_completion,
    },

SRST
``chardev-add`` *args*
  chardev-add accepts the same parameters as the -chardev command line switch.
ERST

    {
        .name       = "chardev-change",
        .args_type  = "id:s,args:s",
        .params     = "id args",
        .help       = "change chardev",
        .cmd        = hmp_chardev_change,
    },

SRST
``chardev-change`` *args*
  chardev-change accepts existing chardev *id* and then the same arguments
  as the -chardev command line switch (except for "id").
ERST

    {
        .name       = "chardev-remove",
        .args_type  = "id:s",
        .params     = "id",
        .help       = "remove chardev",
        .cmd        = hmp_chardev_remove,
        .command_completion = chardev_remove_completion,
    },

SRST
``chardev-remove`` *id*
  Removes the chardev *id*.
ERST

    {
        .name       = "chardev-send-break",
        .args_type  = "id:s",
        .params     = "id",
        .help       = "send a break on chardev",
        .cmd        = hmp_chardev_send_break,
        .command_completion = chardev_remove_completion,
    },

SRST
``chardev-send-break`` *id*
  Send a break on the chardev *id*.
ERST

    {
        .name       = "qom-list",
        .args_type  = "path:s?",
        .params     = "path",
        .help       = "list QOM properties",
        .cmd        = hmp_qom_list,
        .flags      = "p",
    },

SRST
``qom-list`` [*path*]
  Print QOM properties of object at location *path*
ERST

    {
        .name       = "qom-get",
        .args_type  = "path:s,property:s",
        .params     = "path property",
        .help       = "print QOM property",
        .cmd        = hmp_qom_get,
        .flags      = "p",
    },

SRST
``qom-get`` *path* *property*
  Print QOM property *property* of object at location *path*
ERST

    {
        .name       = "qom-set",
        .args_type  = "json:-j,path:s,property:s,value:S",
        .params     = "[-j] path property value",
        .help       = "set QOM property.\n\t\t\t"
                      "-j: the value is specified in json format.",
        .cmd        = hmp_qom_set,
        .flags      = "p",
    },

SRST
``qom-set`` *path* *property* *value*
  Set QOM property *property* of object at location *path* to value *value*
ERST

    {
        .name       = "replay_break",
        .args_type  = "icount:l",
        .params     = "icount",
        .help       = "set breakpoint at the specified instruction count",
        .cmd        = hmp_replay_break,
    },

SRST
``replay_break`` *icount*
  Set replay breakpoint at instruction count *icount*.
  Execution stops when the specified instruction is reached.
  There can be at most one breakpoint. When breakpoint is set, any prior
  one is removed.  The breakpoint may be set only in replay mode and only
  "in the future", i.e. at instruction counts greater than the current one.
  The current instruction count can be observed with ``info replay``.
ERST

    {
        .name       = "replay_delete_break",
        .args_type  = "",
        .params     = "",
        .help       = "remove replay breakpoint",
        .cmd        = hmp_replay_delete_break,
    },

SRST
``replay_delete_break``
  Remove replay breakpoint which was previously set with ``replay_break``.
  The command is ignored when there are no replay breakpoints.
ERST

    {
        .name       = "replay_seek",
        .args_type  = "icount:l",
        .params     = "icount",
        .help       = "replay execution to the specified instruction count",
        .cmd        = hmp_replay_seek,
    },

SRST
``replay_seek`` *icount*
  Automatically proceed to the instruction count *icount*, when
  replaying the execution. The command automatically loads nearest
  snapshot and replays the execution to find the desired instruction.
  When there is no preceding snapshot or the execution is not replayed,
  then the command fails.
  *icount* for the reference may be observed with ``info replay`` command.
ERST

    {
        .name       = "calc_dirty_rate",
        .args_type  = "dirty_ring:-r,dirty_bitmap:-b,second:l,sample_pages_per_GB:l?",
        .params     = "[-r] [-b] second [sample_pages_per_GB]",
        .help       = "start a round of guest dirty rate measurement (using -r to"
                      "\n\t\t\t specify dirty ring as the method of calculation and"
                      "\n\t\t\t -b to specify dirty bitmap as method of calculation)",
        .cmd        = hmp_calc_dirty_rate,
    },

SRST
``calc_dirty_rate`` *second*
  Start a round of dirty rate measurement with the period specified in *second*.
  The result of the dirty rate measurement may be observed with ``info
  dirty_rate`` command.
ERST

    {
        .name       = "set_vcpu_dirty_limit",
        .args_type  = "dirty_rate:l,cpu_index:l?",
        .params     = "dirty_rate [cpu_index]",
        .help       = "set dirty page rate limit, use cpu_index to set limit"
                      "\n\t\t\t\t\t on a specified virtual cpu",
        .cmd        = hmp_set_vcpu_dirty_limit,
    },

SRST
``set_vcpu_dirty_limit``
  Set dirty page rate limit on virtual CPU, the information about all the
  virtual CPU dirty limit status can be observed with ``info vcpu_dirty_limit``
  command.
ERST

    {
        .name       = "cancel_vcpu_dirty_limit",
        .args_type  = "cpu_index:l?",
        .params     = "[cpu_index]",
        .help       = "cancel dirty page rate limit, use cpu_index to cancel"
                      "\n\t\t\t\t\t limit on a specified virtual cpu",
        .cmd        = hmp_cancel_vcpu_dirty_limit,
    },

SRST
``cancel_vcpu_dirty_limit``
  Cancel dirty page rate limit on virtual CPU, the information about all the
  virtual CPU dirty limit status can be observed with ``info vcpu_dirty_limit``
  command.
ERST

    {
        .name       = "info",
        .args_type  = "item:s?",
        .params     = "[subcommand]",
        .help       = "show various information about the system state",
        .cmd        = hmp_info_help,
        .sub_table  = hmp_info_cmds,
        .flags      = "p",
    },

#if defined(CONFIG_FDT)
    {
        .name       = "dumpdtb",
        .args_type  = "filename:F",
        .params     = "filename",
        .help       = "dump the FDT in dtb format to 'filename'",
        .cmd        = hmp_dumpdtb,
    },

SRST
``dumpdtb`` *filename*
  Dump the FDT in dtb format to *filename*.
ERST
#endif

#if defined(CONFIG_XEN_EMU)
    {
        .name       = "xen-event-inject",
        .args_type  = "port:i",
        .params     = "port",
        .help       = "inject event channel",
        .cmd        = hmp_xen_event_inject,
    },

SRST
``xen-event-inject`` *port*
  Notify guest via event channel on port *port*.
ERST


    {
        .name       = "xen-event-list",
        .args_type  = "",
        .params     = "",
        .help       = "list event channel state",
        .cmd        = hmp_xen_event_list,
    },

SRST
``xen-event-list``
  List event channels in the guest
ERST
#endif
