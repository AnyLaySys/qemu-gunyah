HXCOMM See docs/devel/docs.rst for the format of this file.
HXCOMM
HXCOMM Use DEFHEADING() to define headings in both help text and rST.
HXCOMM Text between SRST and ERST is copied to the rST version and
HXCOMM discarded from C version.
HXCOMM DEF(option, HAS_ARG/0, opt_enum, opt_help, arch_mask) is used to
HXCOMM construct option structures, enums and help message for specified
HXCOMM architectures.
HXCOMM HXCOMM can be used for comments, discarded from both rST and C.

DEFHEADING(Standard options:)

DEF("help", 0, QEMU_OPTION_h,
    "-h or -help     display this help and exit\n", QEMU_ARCH_ALL)
SRST
``-h``
    Display help and exit
ERST

DEF("version", 0, QEMU_OPTION_version,
    "-version        display version information and exit\n", QEMU_ARCH_ALL)
SRST
``-version``
    Display version information and exit
ERST

DEF("machine", HAS_ARG, QEMU_OPTION_machine, \
    "-machine [type=]name[,prop[=value][,...]]\n"
    "                selects emulated machine ('-machine help' for list)\n"
    "                property accel=accel1[:accel2[:...]] selects accelerator\n"
    "                vmport=on|off|auto controls emulation of vmport (default: auto)\n"
    "                dump-guest-core=on|off include guest memory in a core dump (default=on)\n"
    "                mem-merge=on|off controls memory merge support (default: on)\n"
    "                aes-key-wrap=on|off controls support for AES key wrapping (default=on)\n"
    "                dea-key-wrap=on|off controls support for DEA key wrapping (default=on)\n"
    "                suppress-vmdesc=on|off disables self-describing migration (default=off)\n"
    "                nvdimm=on|off controls NVDIMM support (default=off)\n"
    "                memory-encryption=@var{} memory encryption object to use (default=none)\n"
    "                hmat=on|off controls ACPI HMAT support (default=off)\n"
#ifdef CONFIG_POSIX
    "                aux-ram-share=on|off allocate auxiliary guest RAM as shared (default: off)\n"
#endif
    "                memory-backend='backend-id' specifies explicitly provided backend for main RAM (default=none)\n"
    "                smp-cache.0.cache=cachename,smp-cache.0.topology=topologylevel\n",
    QEMU_ARCH_ALL)
SRST
``-machine [type=]name[,prop=value[,...]]``
    Select the emulated machine by name. Use ``-machine help`` to list
    available machines.

    For architectures which aim to support live migration compatibility
    across releases, each release will introduce a new versioned machine
    type. For example, the 2.8.0 release introduced machine types
    "pc-i440fx-2.8" and "pc-q35-2.8" for the x86\_64/i686 architectures.

    To allow live migration of guests from QEMU version 2.8.0, to QEMU
    version 2.9.0, the 2.9.0 version must support the "pc-i440fx-2.8"
    and "pc-q35-2.8" machines too. To allow users live migrating VMs to
    skip multiple intermediate releases when upgrading, new releases of
    QEMU will support machine types from many previous versions.

    Supported machine properties are:

    ``accel=accels1[:accels2[:...]]``
    ``vmport=on|off|auto``
        Enables emulation of VMWare IO port, for vmmouse etc. auto says
        to select the value based on accel and i8042. For i8042=off the
        default is off otherwise the default is on.

    ``dump-guest-core=on|off``
        Include guest memory in a core dump. The default is on.

    ``mem-merge=on|off``
        Enables or disables memory merge support. This feature, when
        supported by the host, de-duplicates identical memory pages
        among VMs instances (enabled by default).

    ``aes-key-wrap=on|off``
        Enables or disables AES key wrapping support on s390-ccw hosts.
        This feature controls whether AES wrapping keys will be created
        to allow execution of AES cryptographic functions. The default
        is on.

    ``dea-key-wrap=on|off``
        Enables or disables DEA key wrapping support on s390-ccw hosts.
        This feature controls whether DEA wrapping keys will be created
        to allow execution of DEA cryptographic functions. The default
        is on.

    ``nvdimm=on|off``
        Enables or disables NVDIMM support. The default is off.

    ``memory-encryption=``
        Memory encryption object to use. The default is none.

    ``hmat=on|off``
        Enables or disables ACPI Heterogeneous Memory Attribute Table
        (HMAT) support. The default is off.

    ``aux-ram-share=on|off``
        Allocate auxiliary guest RAM as an anonymous file that is
        shareable with an external process.  This option applies to
        memory allocated as a side effect of creating various devices.
        It does not apply to memory-backend-objects, whether explicitly
        specified on the command line, or implicitly created by the -m
        command line option.  The default is off.

        To use the cpr-transfer migration mode, you must set aux-ram-share=on.

    ``memory-backend='id'``
        Allows to use a memory backend as main RAM.

        For example:
        ::

            -object memory-backend-file,id=pc.ram,size=512M,mem-path=/hugetlbfs,prealloc=on,share=on
            -machine memory-backend=pc.ram
            -m 512M

        Migration compatibility note:

        * as backend id one shall use value of 'default-ram-id', advertised by
          machine type (available via ``query-machines`` QMP command), if migration
          to/from old QEMU (<5.0) is expected.
        * for machine types 4.0 and older, user shall
          use ``x-use-canonical-path-for-ramblock-id=off`` backend option
          if migration to/from old QEMU (<5.0) is expected.

        For example:
        ::

            -object memory-backend-ram,id=pc.ram,size=512M,x-use-canonical-path-for-ramblock-id=off
            -machine memory-backend=pc.ram
            -m 512M

    ``smp-cache.0.cache=cachename,smp-cache.0.topology=topologylevel``
        Define cache properties for SMP system.

        ``cache=cachename`` specifies the cache that the properties will be
        applied on. This field is the combination of cache level and cache
        type. It supports ``l1d`` (L1 data cache), ``l1i`` (L1 instruction
        cache), ``l2`` (L2 unified cache) and ``l3`` (L3 unified cache).

        ``topology=topologylevel`` sets the cache topology level. It accepts
        CPU topology levels including ``core``, ``module``, ``cluster``, ``die``,
        ``socket``, ``book``, ``drawer`` and a special value ``default``. If
        ``default`` is set, then the cache topology will follow the architecture's
        default cache topology model. If another topology level is set, the cache
        will be shared at corresponding CPU topology level. For example,
        ``topology=core`` makes the cache shared by all threads within a core.
        The omitting cache will default to using the ``default`` level.

        The default cache topology model for an i386 PC machine is as follows:
        ``l1d``, ``l1i``, and ``l2`` caches are per ``core``, while the ``l3``
        cache is per ``die``.

        Example:

        ::

            -machine smp-cache.0.cache=l1d,smp-cache.0.topology=core,smp-cache.1.cache=l1i,smp-cache.1.topology=core
ERST

DEF("M", HAS_ARG, QEMU_OPTION_M,
    "                sgx-epc.0.memdev=memid,sgx-epc.0.node=numaid\n",
    QEMU_ARCH_ALL)

SRST
``sgx-epc.0.memdev=@var{memid},sgx-epc.0.node=@var{numaid}``
    Define an SGX EPC section.
ERST

DEF("cpu", HAS_ARG, QEMU_OPTION_cpu,
    "-cpu cpu        select CPU ('-cpu help' for list)\n", QEMU_ARCH_ALL)
SRST
``-cpu model``
    Select CPU model (``-cpu help`` for list and additional feature
    selection)
ERST

DEF("accel", HAS_ARG, QEMU_OPTION_accel,
    "-accel [accel=]accelerator[,prop[=value][,...]]\n"
    "                notify-vmexit=run|internal-error|disable,notify-window=n (enable notify VM exit and set notify window, x86 only)\n"
    , QEMU_ARCH_ALL)
SRST
``-accel name[,prop=value[,...]]``
    ``notify-vmexit=run|internal-error|disable,notify-window=n``
        Enables or disables notify VM exit support on x86 host and specify
        the corresponding notify window to trigger the VM exit if enabled.
        ``run`` option enables the feature. It does nothing and continue
        if the exit happens. ``internal-error`` option enables the feature.
        It raises a internal error. ``disable`` option doesn't enable the feature.
        This feature can mitigate the CPU stuck issue due to event windows don't
        open up for a specified of time (i.e. notify-window).
        Default: notify-vmexit=run,notify-window=0.

ERST

DEF("smp", HAS_ARG, QEMU_OPTION_smp,
    "-smp [[cpus=]n][,maxcpus=maxcpus][,drawers=drawers][,books=books][,sockets=sockets]\n"
    "               [,dies=dies][,clusters=clusters][,modules=modules][,cores=cores]\n"
    "               [,threads=threads]\n"
    "                set the number of initial CPUs to 'n' [default=1]\n"
    "                maxcpus= maximum number of total CPUs, including\n"
    "                offline CPUs for hotplug, etc\n"
    "                drawers= number of drawers on the machine board\n"
    "                books= number of books in one drawer\n"
    "                sockets= number of sockets in one book\n"
    "                dies= number of dies in one socket\n"
    "                clusters= number of clusters in one die\n"
    "                modules= number of modules in one cluster\n"
    "                cores= number of cores in one module\n"
    "                threads= number of threads in one core\n"
    "Note: Different machines may have different subsets of the CPU topology\n"
    "      parameters supported, so the actual meaning of the supported parameters\n"
    "      will vary accordingly. For example, for a machine type that supports a\n"
    "      three-level CPU hierarchy of sockets/cores/threads, the parameters will\n"
    "      sequentially mean as below:\n"
    "                sockets means the number of sockets on the machine board\n"
    "                cores means the number of cores in one socket\n"
    "                threads means the number of threads in one core\n"
    "      For a particular machine type board, an expected CPU topology hierarchy\n"
    "      can be defined through the supported sub-option. Unsupported parameters\n"
    "      can also be provided in addition to the sub-option, but their values\n"
    "      must be set as 1 in the purpose of correct parsing.\n",
    QEMU_ARCH_ALL)
SRST
``-smp [[cpus=]n][,maxcpus=maxcpus][,drawers=drawers][,books=books][,sockets=sockets][,dies=dies][,clusters=clusters][,modules=modules][,cores=cores][,threads=threads]``
    Simulate a SMP system with '\ ``n``\ ' CPUs initially present on
    the machine type board. On boards supporting CPU hotplug, the optional
    '\ ``maxcpus``\ ' parameter can be set to enable further CPUs to be
    added at runtime. When both parameters are omitted, the maximum number
    of CPUs will be calculated from the provided topology members and the
    initial CPU count will match the maximum number. When only one of them
    is given then the omitted one will be set to its counterpart's value.
    Both parameters may be specified, but the maximum number of CPUs must
    be equal to or greater than the initial CPU count. Product of the
    CPU topology hierarchy must be equal to the maximum number of CPUs.
    Both parameters are subject to an upper limit that is determined by
    the specific machine type chosen.

    To control reporting of CPU topology information, values of the topology
    parameters can be specified. Machines may only support a subset of the
    parameters and different machines may have different subsets supported
    which vary depending on capacity of the corresponding CPU targets. So
    for a particular machine type board, an expected topology hierarchy can
    be defined through the supported sub-option. Unsupported parameters can
    also be provided in addition to the sub-option, but their values must be
    set as 1 in the purpose of correct parsing.

    Either the initial CPU count, or at least one of the topology parameters
    must be specified. The specified parameters must be greater than zero,
    explicit configuration like "cpus=0" is not allowed. Values for any
    omitted parameters will be computed from those which are given.

    For example, the following sub-option defines a CPU topology hierarchy
    (2 sockets totally on the machine, 2 cores per socket, 2 threads per
    core) for a machine that only supports sockets/cores/threads.
    Some members of the option can be omitted but their values will be
    automatically computed:

    ::

        -smp 8,sockets=2,cores=2,threads=2,maxcpus=8

    The following sub-option defines a CPU topology hierarchy (2 sockets
    totally on the machine, 2 dies per socket, 2 modules per die, 2 cores per
    module, 2 threads per core) for PC machines which support sockets/dies
    /modules/cores/threads. Some members of the option can be omitted but
    their values will be automatically computed:

    ::

        -smp 32,sockets=2,dies=2,modules=2,cores=2,threads=2,maxcpus=32

    The following sub-option defines a CPU topology hierarchy (2 sockets
    totally on the machine, 2 clusters per socket, 2 cores per cluster,
    2 threads per core) for ARM virt machines which support sockets/clusters
    /cores/threads. Some members of the option can be omitted but their values
    will be automatically computed:

    ::

        -smp 16,sockets=2,clusters=2,cores=2,threads=2,maxcpus=16

    Historically preference was given to the coarsest topology parameters
    when computing missing values (ie sockets preferred over cores, which
    were preferred over threads), however, this behaviour is considered
    liable to change. Prior to 6.2 the preference was sockets over cores
    over threads. Since 6.2 the preference is cores over sockets over threads.

    For example, the following option defines a machine board with 2 sockets
    of 1 core before 6.2 and 1 socket of 2 cores after 6.2:

    ::

        -smp 2

    Note: The cluster topology will only be generated in ACPI and exposed
    to guest if it's explicitly specified in -smp.
ERST

DEF("numa", HAS_ARG, QEMU_OPTION_numa,
    "-numa node[,mem=size][,cpus=firstcpu[-lastcpu]][,nodeid=node][,initiator=node]\n"
    "-numa node[,memdev=id][,cpus=firstcpu[-lastcpu]][,nodeid=node][,initiator=node]\n"
    "-numa dist,src=source,dst=destination,val=distance\n"
    "-numa cpu,node-id=node[,socket-id=x][,core-id=y][,thread-id=z]\n"
    "-numa hmat-lb,initiator=node,target=node,hierarchy=memory|first-level|second-level|third-level,data-type=access-latency|read-latency|write-latency[,latency=lat][,bandwidth=bw]\n"
    "-numa hmat-cache,node-id=node,size=size,level=level[,associativity=none|direct|complex][,policy=none|write-back|write-through][,line=size]\n",
    QEMU_ARCH_ALL)
SRST
``-numa node[,mem=size][,cpus=firstcpu[-lastcpu]][,nodeid=node][,initiator=initiator]``
  \ 
``-numa node[,memdev=id][,cpus=firstcpu[-lastcpu]][,nodeid=node][,initiator=initiator]``
  \
``-numa dist,src=source,dst=destination,val=distance``
  \ 
``-numa cpu,node-id=node[,socket-id=x][,core-id=y][,thread-id=z]``
  \ 
``-numa hmat-lb,initiator=node,target=node,hierarchy=hierarchy,data-type=type[,latency=lat][,bandwidth=bw]``
  \ 
``-numa hmat-cache,node-id=node,size=size,level=level[,associativity=str][,policy=str][,line=size]``
    Define a NUMA node and assign RAM and VCPUs to it. Set the NUMA
    distance from a source node to a destination node. Set the ACPI
    Heterogeneous Memory Attributes for the given nodes.

    Legacy VCPU assignment uses '\ ``cpus``\ ' option where firstcpu and
    lastcpu are CPU indexes. Each '\ ``cpus``\ ' option represent a
    contiguous range of CPU indexes (or a single VCPU if lastcpu is
    omitted). A non-contiguous set of VCPUs can be represented by
    providing multiple '\ ``cpus``\ ' options. If '\ ``cpus``\ ' is
    omitted on all nodes, VCPUs are automatically split between them.

    For example, the following option assigns VCPUs 0, 1, 2 and 5 to a
    NUMA node:

    ::

        -numa node,cpus=0-2,cpus=5

    '\ ``cpu``\ ' option is a new alternative to '\ ``cpus``\ ' option
    which uses '\ ``socket-id|core-id|thread-id``\ ' properties to
    assign CPU objects to a node using topology layout properties of
    CPU. The set of properties is machine specific, and depends on used
    machine type/'\ ``smp``\ ' options. It could be queried with
    '\ ``hotpluggable-cpus``\ ' monitor command. '\ ``node-id``\ '
    property specifies node to which CPU object will be assigned, it's
    required for node to be declared with '\ ``node``\ ' option before
    it's used with '\ ``cpu``\ ' option.

    For example:

    ::

        -M pc \
        -smp 1,sockets=2,maxcpus=2 \
        -numa node,nodeid=0 -numa node,nodeid=1 \
        -numa cpu,node-id=0,socket-id=0 -numa cpu,node-id=1,socket-id=1

    '\ ``memdev``\ ' option assigns RAM from a given memory backend
    device to a node. It is recommended to use '\ ``memdev``\ ' option
    over legacy '\ ``mem``\ ' option. This is because '\ ``memdev``\ '
    option provides better performance and more control over the
    backend's RAM (e.g. '\ ``prealloc``\ ' parameter of
    '\ ``-memory-backend-ram``\ ' allows memory preallocation).

    For compatibility reasons, legacy '\ ``mem``\ ' option is
    supported in 5.0 and older machine types. Note that '\ ``mem``\ '
    and '\ ``memdev``\ ' are mutually exclusive. If one node uses
    '\ ``memdev``\ ', the rest nodes have to use '\ ``memdev``\ '
    option, and vice versa.

    Users must specify memory for all NUMA nodes by '\ ``memdev``\ '
    (or legacy '\ ``mem``\ ' if available). In QEMU 5.2, the support
    for '\ ``-numa node``\ ' without memory specified was removed.

    '\ ``initiator``\ ' is an additional option that points to an
    initiator NUMA node that has best performance (the lowest latency or
    largest bandwidth) to this NUMA node. Note that this option can be
    set only when the machine property 'hmat' is set to 'on'.

    Following example creates a machine with 2 NUMA nodes, node 0 has
    CPU. node 1 has only memory, and its initiator is node 0. Note that
    because node 0 has CPU, by default the initiator of node 0 is itself
    and must be itself.

    ::

        -machine hmat=on \
        -m 2G,slots=2,maxmem=4G \
        -object memory-backend-ram,size=1G,id=m0 \
        -object memory-backend-ram,size=1G,id=m1 \
        -numa node,nodeid=0,memdev=m0 \
        -numa node,nodeid=1,memdev=m1,initiator=0 \
        -smp 2,sockets=2,maxcpus=2  \
        -numa cpu,node-id=0,socket-id=0 \
        -numa cpu,node-id=0,socket-id=1

    source and destination are NUMA node IDs. distance is the NUMA
    distance from source to destination. The distance from a node to
    itself is always 10. If any pair of nodes is given a distance, then
    all pairs must be given distances. Although, when distances are only
    given in one direction for each pair of nodes, then the distances in
    the opposite directions are assumed to be the same. If, however, an
    asymmetrical pair of distances is given for even one node pair, then
    all node pairs must be provided distance values for both directions,
    even when they are symmetrical. When a node is unreachable from
    another node, set the pair's distance to 255.

    Note that the -``numa`` option doesn't allocate any of the specified
    resources, it just assigns existing resources to NUMA nodes. This
    means that one still has to use the ``-m``, ``-smp`` options to
    allocate RAM and VCPUs respectively.

    Use '\ ``hmat-lb``\ ' to set System Locality Latency and Bandwidth
    Information between initiator and target NUMA nodes in ACPI
    Heterogeneous Attribute Memory Table (HMAT). Initiator NUMA node can
    create memory requests, usually it has one or more processors.
    Target NUMA node contains addressable memory.

    In '\ ``hmat-lb``\ ' option, node are NUMA node IDs. hierarchy is
    the memory hierarchy of the target NUMA node: if hierarchy is
    'memory', the structure represents the memory performance; if
    hierarchy is 'first-level\|second-level\|third-level', this
    structure represents aggregated performance of memory side caches
    for each domain. type of 'data-type' is type of data represented by
    this structure instance: if 'hierarchy' is 'memory', 'data-type' is
    'access\|read\|write' latency or 'access\|read\|write' bandwidth of
    the target memory; if 'hierarchy' is
    'first-level\|second-level\|third-level', 'data-type' is
    'access\|read\|write' hit latency or 'access\|read\|write' hit
    bandwidth of the target memory side cache.

    lat is latency value in nanoseconds. bw is bandwidth value, the
    possible value and units are NUM[M\|G\|T], mean that the bandwidth
    value are NUM byte per second (or MB/s, GB/s or TB/s depending on
    used suffix). Note that if latency or bandwidth value is 0, means
    the corresponding latency or bandwidth information is not provided.

    In '\ ``hmat-cache``\ ' option, node-id is the NUMA-id of the memory
    belongs. size is the size of memory side cache in bytes. level is
    the cache level described in this structure, note that the cache
    level 0 should not be used with '\ ``hmat-cache``\ ' option.
    associativity is the cache associativity, the possible value is
    'none/direct(direct-mapped)/complex(complex cache indexing)'. policy
    is the write policy. line is the cache Line size in bytes.

    For example, the following options describe 2 NUMA nodes. Node 0 has
    2 cpus and a ram, node 1 has only a ram. The processors in node 0
    access memory in node 0 with access-latency 5 nanoseconds,
    access-bandwidth is 200 MB/s; The processors in NUMA node 0 access
    memory in NUMA node 1 with access-latency 10 nanoseconds,
    access-bandwidth is 100 MB/s. And for memory side cache information,
    NUMA node 0 and 1 both have 1 level memory cache, size is 10KB,
    policy is write-back, the cache Line size is 8 bytes:

    ::

        -machine hmat=on \
        -m 2G \
        -object memory-backend-ram,size=1G,id=m0 \
        -object memory-backend-ram,size=1G,id=m1 \
        -smp 2,sockets=2,maxcpus=2 \
        -numa node,nodeid=0,memdev=m0 \
        -numa node,nodeid=1,memdev=m1,initiator=0 \
        -numa cpu,node-id=0,socket-id=0 \
        -numa cpu,node-id=0,socket-id=1 \
        -numa hmat-lb,initiator=0,target=0,hierarchy=memory,data-type=access-latency,latency=5 \
        -numa hmat-lb,initiator=0,target=0,hierarchy=memory,data-type=access-bandwidth,bandwidth=200M \
        -numa hmat-lb,initiator=0,target=1,hierarchy=memory,data-type=access-latency,latency=10 \
        -numa hmat-lb,initiator=0,target=1,hierarchy=memory,data-type=access-bandwidth,bandwidth=100M \
        -numa hmat-cache,node-id=0,size=10K,level=1,associativity=direct,policy=write-back,line=8 \
        -numa hmat-cache,node-id=1,size=10K,level=1,associativity=direct,policy=write-back,line=8
ERST

DEF("add-fd", HAS_ARG, QEMU_OPTION_add_fd,
    "-add-fd fd=fd,set=set[,opaque=opaque]\n"
    "                Add 'fd' to fd 'set'\n", QEMU_ARCH_ALL)
SRST
``-add-fd fd=fd,set=set[,opaque=opaque]``
    Add a file descriptor to an fd set. Valid options are:

    ``fd=fd``
        This option defines the file descriptor of which a duplicate is
        added to fd set. The file descriptor cannot be stdin, stdout, or
        stderr.

    ``set=set``
        This option defines the ID of the fd set to add the file
        descriptor to.

    ``opaque=opaque``
        This option defines a free-form string that can be used to
        describe fd.

    You can open an image using pre-opened file descriptors from an fd
    set:

    .. parsed-literal::

        |qemu_system| \\
         -add-fd fd=3,set=2,opaque="rdwr:/path/to/file" \\
         -add-fd fd=4,set=2,opaque="rdonly:/path/to/file" \\
         -drive file=/dev/fdset/2,index=0,media=disk
ERST

DEF("set", HAS_ARG, QEMU_OPTION_set,
    "-set group.id.arg=value\n"
    "                set <arg> parameter for item <id> of type <group>\n"
    "                i.e. -set drive.$id.file=/path/to/image\n", QEMU_ARCH_ALL)
SRST
``-set group.id.arg=value``
    Set parameter arg for item id of type group
ERST

DEF("global", HAS_ARG, QEMU_OPTION_global,
    "-global driver.property=value\n"
    "-global driver=driver,property=property,value=value\n"
    "                set a global default for a driver property\n",
    QEMU_ARCH_ALL)
SRST
``-global driver.prop=value``
  \ 
``-global driver=driver,property=property,value=value``
    Set default value of driver's property prop to value.

    In particular, you can use this to set driver properties for devices
    which are created automatically by the machine model. To create a
    device which is not created automatically and set properties on it,
    use -``device``.

    -global driver.prop=value is shorthand for -global
    driver=driver,property=prop,value=value. The longhand syntax works
    even when driver contains a dot.
ERST

DEF("boot", HAS_ARG, QEMU_OPTION_boot,
    "-boot [order=drives][,once=drives][,menu=on|off]\n"
    "      [,splash=sp_name][,splash-time=sp_time][,reboot-timeout=rb_time][,strict=on|off]\n"
    "                'drives': hard disk (c), CD-ROM (d), network (n)\n"
    "                'sp_name': the file's name that would be passed to bios as logo picture, if menu=on\n"
    "                'sp_time': the period that splash picture last if menu=on, unit is ms\n"
    "                'rb_timeout': the timeout before guest reboot when boot failed, unit is ms\n",
    QEMU_ARCH_ALL)
SRST
``-boot [order=drives][,once=drives][,menu=on|off][,splash=sp_name][,splash-time=sp_time][,reboot-timeout=rb_timeout][,strict=on|off]``
    Specify boot order drives as a string of drive letters. Valid drive
    letters depend on the target architecture. The x86 PC uses: a, b
    (floppy 1 and 2), c (first hard disk), d (first CD-ROM), n-p
    (Etherboot from network adapter 1-4), hard disk boot is the default.
    To apply a particular boot order only on the first startup, specify
    it via ``once``. Note that the ``order`` or ``once`` parameter
    should not be used together with the ``bootindex`` property of
    devices, since the firmware implementations normally do not support
    both at the same time.

    Interactive boot menus/prompts can be enabled via ``menu=on`` as far
    as firmware/BIOS supports them. The default is non-interactive boot.

    A splash picture could be passed to bios, enabling user to show it
    as logo, when option splash=sp\_name is given and menu=on, If
    firmware/BIOS supports them. Currently Seabios for X86 system
    support it. limitation: The splash file could be a jpeg file or a
    BMP file in 24 BPP format(true color). The resolution should be
    supported by the SVGA mode, so the recommended is 320x240, 640x480,
    800x640.

    A timeout could be passed to bios, guest will pause for rb\_timeout
    ms when boot failed, then reboot. If rb\_timeout is '-1', guest will
    not reboot, qemu passes '-1' to bios by default. Currently Seabios
    for X86 system support it.

    Do strict boot via ``strict=on`` as far as firmware/BIOS supports
    it. This only effects when boot priority is changed by bootindex
    options. The default is non-strict boot.

    .. parsed-literal::

        # try to boot from network first, then from hard disk
        |qemu_system_x86| -boot order=nc
        # boot from CD-ROM first, switch back to default order after reboot
        |qemu_system_x86| -boot once=d
        # boot with a splash picture for 5 seconds.
        |qemu_system_x86| -boot menu=on,splash=/root/boot.bmp,splash-time=5000

    Note: The legacy format '-boot drives' is still supported but its
    use is discouraged as it may be removed from future versions.
ERST

DEF("m", HAS_ARG, QEMU_OPTION_m,
    "-m [size=]megs[,slots=n,maxmem=size]\n"
    "                configure guest RAM\n"
    "                size: initial amount of guest memory\n"
    "                slots: number of hotplug slots (default: none)\n"
    "                maxmem: maximum amount of guest memory (default: none)\n"
    "                Note: Some architectures might enforce a specific granularity\n",
    QEMU_ARCH_ALL)
SRST
``-m [size=]megs[,slots=n,maxmem=size]``
    Sets guest startup RAM size to megs megabytes. Default is 128 MiB.
    Optionally, a suffix of "M" or "G" can be used to signify a value in
    megabytes or gigabytes respectively. Optional pair slots, maxmem
    could be used to set amount of hotpluggable memory slots and maximum
    amount of memory. Note that maxmem must be aligned to the page size.

    For example, the following command-line sets the guest startup RAM
    size to 1GB, creates 3 slots to hotplug additional memory and sets
    the maximum memory the guest can reach to 4GB:

    .. parsed-literal::

        |qemu_system| -m 1G,slots=3,maxmem=4G

    If slots and maxmem are not specified, memory hotplug won't be
    enabled and the guest startup RAM will never increase.
ERST

DEF("mem-prealloc", 0, QEMU_OPTION_mem_prealloc,
    "-mem-prealloc   preallocate guest memory\n",
    QEMU_ARCH_ALL)
SRST
``-mem-prealloc``
    Preallocate guest memory.
ERST


DEF("audio", HAS_ARG, QEMU_OPTION_audio,
    "-audio [driver=]driver[,prop[=value][,...]]\n"
    "                specifies default audio backend when `audiodev` is not\n"
    "                used to create a machine or sound device;"
    "                options are the same as for -audiodev\n",
    QEMU_ARCH_ALL)
SRST
``-audio [driver=]driver[,prop[=value][,...]]``
    ``-audio`` configures a default audio backend used whenever the
    ``audiodev`` property is not set on a device or machine.  In
    particular, ``-audio none`` ensures that no audio is produced even
    for machines that have embedded sound hardware.

    The driver option is the same as with the corresponding ``-audiodev``
    option below.  Use ``driver=help`` to list the available drivers.

ERST

DEF("audiodev", HAS_ARG, QEMU_OPTION_audiodev,
    "-audiodev [driver=]driver,id=id[,prop[=value][,...]]\n"
    "                specifies the audio backend to use\n"
    "                Use ``-audiodev help`` to list the available drivers\n"
    "                id= identifier of the backend\n"
    "                timer-period= timer period in microseconds\n"
    "                in|out.mixing-engine= use mixing engine to mix streams inside QEMU\n"
    "                in|out.fixed-settings= use fixed settings for host audio\n"
    "                in|out.frequency= frequency to use with fixed settings\n"
    "                in|out.channels= number of channels to use with fixed settings\n"
    "                in|out.format= sample format to use with fixed settings\n"
    "                valid values: s8, s16, s32, u8, u16, u32, f32\n"
    "                in|out.voices= number of voices to use\n"
    "                in|out.buffer-length= length of buffer in microseconds\n"
    "-audiodev none,id=id,[,prop[=value][,...]]\n"
    "                dummy driver that discards all output\n"
#ifdef CONFIG_AUDIO_ALSA
    "-audiodev alsa,id=id[,prop[=value][,...]]\n"
    "                in|out.dev= name of the audio device to use\n"
    "                in|out.period-length= length of period in microseconds\n"
    "                in|out.try-poll= attempt to use poll mode\n"
    "                threshold= threshold (in microseconds) when playback starts\n"
#endif
#ifdef CONFIG_AUDIO_COREAUDIO
    "-audiodev coreaudio,id=id[,prop[=value][,...]]\n"
    "                in|out.buffer-count= number of buffers\n"
#endif
#ifdef CONFIG_AUDIO_DSOUND
    "-audiodev dsound,id=id[,prop[=value][,...]]\n"
    "                latency= add extra latency to playback in microseconds\n"
#endif
#ifdef CONFIG_AUDIO_OSS
    "-audiodev oss,id=id[,prop[=value][,...]]\n"
    "                in|out.dev= path of the audio device to use\n"
    "                in|out.buffer-count= number of buffers\n"
    "                in|out.try-poll= attempt to use poll mode\n"
    "                try-mmap= try using memory mapped access\n"
    "                exclusive= open device in exclusive mode\n"
    "                dsp-policy= set timing policy (0..10), -1 to use fragment mode\n"
#endif
#ifdef CONFIG_AUDIO_PA
    "-audiodev pa,id=id[,prop[=value][,...]]\n"
    "                server= PulseAudio server address\n"
    "                in|out.name= source/sink device name\n"
    "                in|out.latency= desired latency in microseconds\n"
#endif
#ifdef CONFIG_AUDIO_PIPEWIRE
    "-audiodev pipewire,id=id[,prop[=value][,...]]\n"
    "                in|out.name= source/sink device name\n"
    "                in|out.stream-name= name of pipewire stream\n"
    "                in|out.latency= desired latency in microseconds\n"
#endif
#ifdef CONFIG_AUDIO_SNDIO
    "-audiodev sndio,id=id[,prop[=value][,...]]\n"
#endif
#ifdef CONFIG_SPICE
    "-audiodev spice,id=id[,prop[=value][,...]]\n"
#endif
#ifdef CONFIG_DBUS_DISPLAY
    "-audiodev dbus,id=id[,prop[=value][,...]]\n"
#endif
    "-audiodev wav,id=id[,prop[=value][,...]]\n"
    "                path= path of wav file to record\n",
    QEMU_ARCH_ALL)
SRST
``-audiodev [driver=]driver,id=id[,prop[=value][,...]]``
    Adds a new audio backend driver identified by id. There are global
    and driver specific properties. Some values can be set differently
    for input and output, they're marked with ``in|out.``. You can set
    the input's property with ``in.prop`` and the output's property with
    ``out.prop``. For example:

    ::

        -audiodev alsa,id=example,in.frequency=44110,out.frequency=8000
        -audiodev alsa,id=example,out.channels=1 # leaves in.channels unspecified

    NOTE: parameter validation is known to be incomplete, in many cases
    specifying an invalid option causes QEMU to print an error message
    and continue emulation without sound.

    Valid global options are:

    ``id=identifier``
        Identifies the audio backend.

    ``timer-period=period``
        Sets the timer period used by the audio subsystem in
        microseconds. Default is 10000 (10 ms).

    ``in|out.mixing-engine=on|off``
        Use QEMU's mixing engine to mix all streams inside QEMU and
        convert audio formats when not supported by the backend. When
        off, fixed-settings must be off too. Note that disabling this
        option means that the selected backend must support multiple
        streams and the audio formats used by the virtual cards,
        otherwise you'll get no sound. It's not recommended to disable
        this option unless you want to use 5.1 or 7.1 audio, as mixing
        engine only supports mono and stereo audio. Default is on.

    ``in|out.fixed-settings=on|off``
        Use fixed settings for host audio. When off, it will change
        based on how the guest opens the sound card. In this case you
        must not specify frequency, channels or format. Default is on.

    ``in|out.frequency=frequency``
        Specify the frequency to use when using fixed-settings. Default
        is 44100Hz.

    ``in|out.channels=channels``
        Specify the number of channels to use when using fixed-settings.
        Default is 2 (stereo).

    ``in|out.format=format``
        Specify the sample format to use when using fixed-settings.
        Valid values are: ``s8``, ``s16``, ``s32``, ``u8``, ``u16``,
        ``u32``, ``f32``. Default is ``s16``.

    ``in|out.voices=voices``
        Specify the number of voices to use. Default is 1.

    ``in|out.buffer-length=usecs``
        Sets the size of the buffer in microseconds.

``-audiodev none,id=id[,prop[=value][,...]]``
    Creates a dummy backend that discards all outputs. This backend has
    no backend specific properties.

``-audiodev alsa,id=id[,prop[=value][,...]]``
    Creates backend using the ALSA. This backend is only available on
    Linux.

    ALSA specific options are:

    ``in|out.dev=device``
        Specify the ALSA device to use for input and/or output. Default
        is ``default``.

    ``in|out.period-length=usecs``
        Sets the period length in microseconds.

    ``in|out.try-poll=on|off``
        Attempt to use poll mode with the device. Default is on.

    ``threshold=threshold``
        Threshold (in microseconds) when playback starts. Default is 0.

``-audiodev coreaudio,id=id[,prop[=value][,...]]``
    Creates a backend using Apple's Core Audio. This backend is only
    available on Mac OS and only supports playback.

    Core Audio specific options are:

    ``in|out.buffer-count=count``
        Sets the count of the buffers.

``-audiodev dsound,id=id[,prop[=value][,...]]``
    Creates a backend using Microsoft's DirectSound. This backend is
    only available on Windows and only supports playback.

    DirectSound specific options are:

    ``latency=usecs``
        Add extra usecs microseconds latency to playback. Default is
        10000 (10 ms).

``-audiodev oss,id=id[,prop[=value][,...]]``
    Creates a backend using OSS. This backend is available on most
    Unix-like systems.

    OSS specific options are:

    ``in|out.dev=device``
        Specify the file name of the OSS device to use. Default is
        ``/dev/dsp``.

    ``in|out.buffer-count=count``
        Sets the count of the buffers.

    ``in|out.try-poll=on|of``
        Attempt to use poll mode with the device. Default is on.

    ``try-mmap=on|off``
        Try using memory mapped device access. Default is off.

    ``exclusive=on|off``
        Open the device in exclusive mode (vmix won't work in this
        case). Default is off.

    ``dsp-policy=policy``
        Sets the timing policy (between 0 and 10, where smaller number
        means smaller latency but higher CPU usage). Use -1 to use
        buffer sizes specified by ``buffer`` and ``buffer-count``. This
        option is ignored if you do not have OSS 4. Default is 5.

``-audiodev pa,id=id[,prop[=value][,...]]``
    Creates a backend using PulseAudio. This backend is available on
    most systems.

    PulseAudio specific options are:

    ``server=server``
        Sets the PulseAudio server to connect to.

    ``in|out.name=sink``
        Use the specified source/sink for recording/playback.

    ``in|out.latency=usecs``
        Desired latency in microseconds. The PulseAudio server will try
        to honor this value but actual latencies may be lower or higher.

``-audiodev pipewire,id=id[,prop[=value][,...]]``
    Creates a backend using PipeWire. This backend is available on
    most systems.

    PipeWire specific options are:

    ``in|out.latency=usecs``
        Desired latency in microseconds.

    ``in|out.name=sink``
        Use the specified source/sink for recording/playback.

    ``in|out.stream-name``
        Specify the name of pipewire stream.

``-audiodev sndio,id=id[,prop[=value][,...]]``
    Creates a backend using SNDIO. This backend is available on
    OpenBSD and most other Unix-like systems.

    Sndio specific options are:

    ``in|out.dev=device``
        Specify the sndio device to use for input and/or output. Default
        is ``default``.

    ``in|out.latency=usecs``
        Sets the desired period length in microseconds.

``-audiodev spice,id=id[,prop[=value][,...]]``
    Creates a backend that sends audio through SPICE. This backend
    requires ``-spice`` and automatically selected in that case, so
    usually you can ignore this option. This backend has no backend
    specific properties.

``-audiodev wav,id=id[,prop[=value][,...]]``
    Creates a backend that writes audio to a WAV file.

    Backend specific options are:

    ``path=path``
        Write recorded audio into the specified file. Default is
        ``qemu.wav``.
ERST

DEF("device", HAS_ARG, QEMU_OPTION_device,
    "-device driver[,prop[=value][,...]]\n"
    "                add device (based on driver)\n"
    "                prop=value,... sets driver properties\n"
    "                use '-device help' to print all possible drivers\n"
    "                use '-device driver,help' to print all possible properties\n",
    QEMU_ARCH_ALL)
SRST
``-device driver[,prop[=value][,...]]``
    Add device driver. prop=value sets driver properties. Valid
    properties depend on the driver. To get help on possible drivers and
    properties, use ``-device help`` and ``-device driver,help``.

``-device intel-iommu[,option=...]``
    This is only supported by ``-machine q35``, which will enable Intel VT-d
    emulation within the guest.  It supports below options:

    ``intremap=on|off`` (default: auto)
        This enables interrupt remapping feature.  It's required to enable
        complete x2apic.  Currently it only supports kvm kernel-irqchip modes
        ``off`` or ``split``, while full kernel-irqchip is not yet supported.
        The default value is "auto", which will be decided by the mode of
        kernel-irqchip.

    ``caching-mode=on|off`` (default: off)
        This enables caching mode for the VT-d emulated device.  When
        caching-mode is enabled, each guest DMA buffer mapping will generate an
        IOTLB invalidation from the guest IOMMU driver to the vIOMMU device in
        a synchronous way.

    ``device-iotlb=on|off`` (default: off)
        This enables device-iotlb capability for the emulated VT-d device.  So
        far virtio/vhost should be the only real user for this parameter,
        paired with ats=on configured for the device.

    ``aw-bits=39|48`` (default: 39)
        This decides the address width of IOVA address space.  The address
        space has 39 bits width for 3-level IOMMU page tables, and 48 bits for
        4-level IOMMU page tables.

    Please also refer to the wiki page for general scenarios of VT-d
    emulation in QEMU: https://wiki.qemu.org/Features/VT-d.

ERST

DEF("name", HAS_ARG, QEMU_OPTION_name,
    "-name string1[,process=string2][,debug-threads=on|off]\n"
    "                set the name of the guest\n"
    "                string1 sets the window title and string2 the process name\n"
    "                When debug-threads is enabled, individual threads are given a separate name\n"
    "                NOTE: The thread names are for debugging and not a stable API.\n",
    QEMU_ARCH_ALL)
SRST
``-name name``
    Sets the name of the guest and optionally the top visible process name
    in Linux. Naming of individual threads can also be enabled on Linux to
    aid debugging.
ERST

DEF("uuid", HAS_ARG, QEMU_OPTION_uuid,
    "-uuid %08x-%04x-%04x-%04x-%012x\n"
    "                specify machine UUID\n", QEMU_ARCH_ALL)
SRST
``-uuid uuid``
    Set system UUID.
ERST

DEFHEADING()

DEFHEADING(Block device options:)

SRST
The QEMU block device handling options have a long history and
have gone through several iterations as the feature set and complexity
of the block layer have grown. Many online guides to QEMU often
reference older and deprecated options, which can lead to confusion.

The most explicit way to describe disks is to use a combination of
``-device`` to specify the hardware device and ``-blockdev`` to
describe the backend. The device defines what the guest sees and the
backend describes how QEMU handles the data. It is the only guaranteed
stable interface for describing block devices and as such is
recommended for management tools and scripting.

The ``-drive`` option combines the device and backend into a single
command line option which is a more human friendly. There is however no
interface stability guarantee although some older board models still
need updating to work with the modern blockdev forms.

Older options like ``-hda`` are essentially macros which expand into
``-drive`` options for various drive interfaces. The original forms
bake in a lot of assumptions from the days when QEMU was emulating a
legacy PC, they are not recommended for modern configurations.

ERST

DEF("hda", HAS_ARG, QEMU_OPTION_hda,
    "-hda/-hdb file  use 'file' as hard disk 0/1 image\n", QEMU_ARCH_ALL)
DEF("hdb", HAS_ARG, QEMU_OPTION_hdb, "", QEMU_ARCH_ALL)
DEF("hdc", HAS_ARG, QEMU_OPTION_hdc,
    "-hdc/-hdd file  use 'file' as hard disk 2/3 image\n", QEMU_ARCH_ALL)
DEF("hdd", HAS_ARG, QEMU_OPTION_hdd, "", QEMU_ARCH_ALL)
SRST
``-hda file``
  \
``-hdb file``
  \ 
``-hdc file``
  \ 
``-hdd file``
    Use file as hard disk 0, 1, 2 or 3 image on the default bus of the
    emulated machine (this is for example the IDE bus on most x86 machines,
    but it can also be SCSI, virtio or something else on other target
    architectures). See also the :ref:`disk images` chapter in the System
    Emulation Users Guide.
ERST

DEF("cdrom", HAS_ARG, QEMU_OPTION_cdrom,
    "-cdrom file     use 'file' as CD-ROM image\n",
    QEMU_ARCH_ALL)
SRST
``-cdrom file``
    Use file as CD-ROM image on the default bus of the emulated machine
    (which is IDE1 master on x86, so you cannot use ``-hdc`` and ``-cdrom``
    at the same time there). On systems that support it, you can use the
    host CD-ROM by using ``/dev/cdrom`` as filename.
ERST

DEF("blockdev", HAS_ARG, QEMU_OPTION_blockdev,
    "-blockdev [driver=]driver[,node-name=N][,discard=ignore|unmap]\n"
    "          [,cache.direct=on|off][,cache.no-flush=on|off]\n"
    "          [,read-only=on|off][,auto-read-only=on|off]\n"
    "          [,force-share=on|off][,detect-zeroes=on|off|unmap]\n"
    "          [,driver specific parameters...]\n"
    "                configure a block backend\n", QEMU_ARCH_ALL)
SRST
``-blockdev option[,option[,option[,...]]]``
    Define a new block driver node. Some of the options apply to all
    block drivers, other options are only accepted for a specific block
    driver. See below for a list of generic options and options for the
    most common block drivers.

    Options that expect a reference to another node (e.g. ``file``) can
    be given in two ways. Either you specify the node name of an already
    existing node (file=node-name), or you define a new node inline,
    adding options for the referenced node after a dot
    (file.filename=path,file.aio=io_uring).

    A block driver node created with ``-blockdev`` can be used for a
    guest device by specifying its node name for the ``drive`` property
    in a ``-device`` argument that defines a block device.

    ``Valid options for any block driver node:``
        ``driver``
            Specifies the block driver to use for the given node.

        ``node-name``
            This defines the name of the block driver node by which it
            will be referenced later. The name must be unique, i.e. it
            must not match the name of a different block driver node, or
            (if you use ``-drive`` as well) the ID of a drive.

            If no node name is specified, it is automatically generated.
            The generated node name is not intended to be predictable
            and changes between QEMU invocations. For the top level, an
            explicit node name must be specified.

        ``read-only``
            Open the node read-only. Guest write attempts will fail.

            Note that some block drivers support only read-only access,
            either generally or in certain configurations. In this case,
            the default value ``read-only=off`` does not work and the
            option must be specified explicitly.

        ``auto-read-only``
            If ``auto-read-only=on`` is set, QEMU may fall back to
            read-only usage even when ``read-only=off`` is requested, or
            even switch between modes as needed, e.g. depending on
            whether the image file is writable or whether a writing user
            is attached to the node.

        ``force-share``
            Override the image locking system of QEMU by forcing the
            node to utilize weaker shared access for permissions where
            it would normally request exclusive access. When there is
            the potential for multiple instances to have the same file
            open (whether this invocation of QEMU is the first or the
            second instance), both instances must permit shared access
            for the second instance to succeed at opening the file.

            Enabling ``force-share=on`` requires ``read-only=on``.

        ``cache.direct``
            The host page cache can be avoided with ``cache.direct=on``.
            This will attempt to do disk IO directly to the guest's
            memory. QEMU may still perform an internal copy of the data.

        ``cache.no-flush``
            In case you don't care about data integrity over host
            failures, you can use ``cache.no-flush=on``. This option
            tells QEMU that it never needs to write any data to the disk
            but can instead keep things in cache. If anything goes
            wrong, like your host losing power, the disk storage getting
            disconnected accidentally, etc. your image will most
            probably be rendered unusable.

        ``discard=discard``
            discard is one of "ignore" (or "off") or "unmap" (or "on")
            and controls whether ``discard`` (also known as ``trim`` or
            ``unmap``) requests are ignored or passed to the filesystem.
            Some machine types may not support discard requests.

        ``detect-zeroes=detect-zeroes``
            detect-zeroes is "off", "on" or "unmap" and enables the
            automatic conversion of plain zero writes by the OS to
            driver specific optimized zero write commands. You may even
            choose "unmap" if discard is set to "unmap" to allow a zero
            write to be converted to an ``unmap`` operation.

    ``Driver-specific options for file``
        This is the protocol-level block driver for accessing regular
        files.

        ``filename``
            The path to the image file in the local filesystem

        ``aio``
            Specifies the AIO backend (threads/native/io_uring,
            default: threads)

        ``locking``
            Specifies whether the image file is protected with Linux OFD
            / POSIX locks. The default is to use the Linux Open File
            Descriptor API if available, otherwise no lock is applied.
            (auto/on/off, default: auto)

        Example:

        ::

            -blockdev driver=file,node-name=disk,filename=disk.img

    ``Driver-specific options for raw``
        This is the image format block driver for raw images. It is
        usually stacked on top of a protocol level block driver such as
        ``file``.

        ``file``
            Reference to or definition of the data source block driver
            node (e.g. a ``file`` driver node)

        Example 1:

        ::

            -blockdev driver=file,node-name=disk_file,filename=disk.img
            -blockdev driver=raw,node-name=disk,file=disk_file

        Example 2:

        ::

            -blockdev driver=raw,node-name=disk,file.driver=file,file.filename=disk.img

    ``Driver-specific options for other drivers``
        Please refer to the QAPI documentation of the ``blockdev-add``
        QMP command.
ERST

DEF("drive", HAS_ARG, QEMU_OPTION_drive,
    "-drive [file=file][,if=type][,bus=n][,unit=m][,media=d][,index=i]\n"
    "       [,cache=writethrough|writeback|none|directsync|unsafe][,format=f]\n"
    "       [,snapshot=on|off][,rerror=ignore|stop|report]\n"
    "       [,werror=ignore|stop|report|enospc][,id=name]\n"
    "       [,aio=io_uring]\n"
    "       [,readonly=on|off][,copy-on-read=on|off]\n"
    "       [,discard=ignore|unmap][,detect-zeroes=on|off|unmap]\n"
    "       [[,bps=b]|[[,bps_rd=r][,bps_wr=w]]]\n"
    "       [[,iops=i]|[[,iops_rd=r][,iops_wr=w]]]\n"
    "       [[,bps_max=bm]|[[,bps_rd_max=rm][,bps_wr_max=wm]]]\n"
    "       [[,iops_max=im]|[[,iops_rd_max=irm][,iops_wr_max=iwm]]]\n"
    "       [[,iops_size=is]]\n"
    "       [[,group=g]]\n"
    "                use 'file' as a drive image\n", QEMU_ARCH_ALL)
SRST
``-drive option[,option[,option[,...]]]``
    Define a new drive. This includes creating a block driver node (the
    backend) as well as a guest device, and is mostly a shortcut for
    defining the corresponding ``-blockdev`` and ``-device`` options.

    ``-drive`` accepts all options that are accepted by ``-blockdev``.
    In addition, it knows the following options:

    ``file=file``
        This option defines which disk image (see the :ref:`disk images`
        chapter in the System Emulation Users Guide) to use with this drive.
        If the filename contains comma, you must double it (for instance,
        "file=my,,file" to use file "my,file").

        Special files such as iSCSI devices can be specified using
        protocol specific URLs. See the section for "Device URL Syntax"
        for more information.

    ``if=interface``
        This option defines on which type on interface the drive is
        connected. Available types are: virtio, none.

    ``bus=bus,unit=unit``
        These options define where is connected the drive by defining
        the bus number and the unit id.

    ``index=index``
        This option defines where the drive is connected by using an
        index in the list of available connectors of a given interface
        type.

    ``media=media``
        This option defines the type of the media: disk or cdrom.

    ``snapshot=snapshot``
        snapshot is "on" or "off" and controls snapshot mode for the
        given drive (see ``-snapshot``).

    ``cache=cache``
        cache is "none", "writeback", "unsafe", "directsync" or
        "writethrough" and controls how the host cache is used to access
        block data. This is a shortcut that sets the ``cache.direct``
        and ``cache.no-flush`` options (as in ``-blockdev``), and
        additionally ``cache.writeback``, which provides a default for
        the ``write-cache`` option of block guest devices (as in
        ``-device``). The modes correspond to the following settings:

        =============  ===============   ============   ==============
        \              cache.writeback   cache.direct   cache.no-flush
        =============  ===============   ============   ==============
        writeback      on                off            off
        none           on                on             off
        writethrough   off               off            off
        directsync     off               on             off
        unsafe         on                off            on
        =============  ===============   ============   ==============

        The default mode is ``cache=writeback``.

    ``aio=aio``
        aio is "io_uring" and selects the Linux io_uring API.

    ``format=format``
        Specify which disk format will be used rather than detecting the
        format. Can be used to specify format=raw to avoid interpreting
        an untrusted format header.

    ``werror=action,rerror=action``
        Specify which action to take on write and read errors. Valid
        actions are: "ignore" (ignore the error and try to continue),
        "stop" (pause QEMU), "report" (report the error to the guest),
        "enospc" (pause QEMU only if the host disk is full; report the
        error to the guest otherwise). The default setting is
        ``werror=enospc`` and ``rerror=report``.

    ``copy-on-read=copy-on-read``
        copy-on-read is "on" or "off" and enables whether to copy read
        backing file sectors into the image file.

    ``bps=b,bps_rd=r,bps_wr=w``
        Specify bandwidth throttling limits in bytes per second, either
        for all request types or for reads or writes only. Small values
        can lead to timeouts or hangs inside the guest. A safe minimum
        for disks is 2 MB/s.

    ``bps_max=bm,bps_rd_max=rm,bps_wr_max=wm``
        Specify bursts in bytes per second, either for all request types
        or for reads or writes only. Bursts allow the guest I/O to spike
        above the limit temporarily.

    ``iops=i,iops_rd=r,iops_wr=w``
        Specify request rate limits in requests per second, either for
        all request types or for reads or writes only.

    ``iops_max=bm,iops_rd_max=rm,iops_wr_max=wm``
        Specify bursts in requests per second, either for all request
        types or for reads or writes only. Bursts allow the guest I/O to
        spike above the limit temporarily.

    ``iops_size=is``
        Let every is bytes of a request count as a new request for iops
        throttling purposes. Use this option to prevent guests from
        circumventing iops limits by sending fewer but larger requests.

    ``group=g``
        Join a throttling quota group with given name g. All drives that
        are members of the same group are accounted for together. Use
        this option to prevent guests from circumventing throttling
        limits by using many small disks instead of a single larger
        disk.

    By default, the ``cache.writeback=on`` mode is used. It will report
    data writes as completed as soon as the data is present in the host
    page cache. This is safe as long as your guest OS makes sure to
    correctly flush disk caches where needed. If your guest OS does not
    handle volatile disk write caches correctly and your host crashes or
    loses power, then the guest may experience data corruption.

    For such guests, you should consider using ``cache.writeback=off``.
    This means that the host page cache will be used to read and write
    data, but write notification will be sent to the guest only after
    QEMU has made sure to flush each write to the disk. Be aware that
    this has a major impact on performance.

    When using the ``-snapshot`` option, unsafe caching is always used.

    Copy-on-read avoids accessing the same backing file sectors
    repeatedly and is useful when the backing file is over a slow
    network. By default copy-on-read is off.

    You can open an image using pre-opened file descriptors from an fd
    set:

    .. parsed-literal::

        |qemu_system| \\
         -add-fd fd=3,set=2,opaque="rdwr:/path/to/file" \\
         -add-fd fd=4,set=2,opaque="rdonly:/path/to/file" \\
         -drive file=/dev/fdset/2,index=0,media=disk

ERST

DEF("snapshot", 0, QEMU_OPTION_snapshot,
    "-snapshot       write to temporary files instead of disk image files\n",
    QEMU_ARCH_ALL)
SRST
``-snapshot``
    Write to temporary files instead of disk image files. In this case,
    the raw disk image you use is not written back. You can however
    force the write back by pressing C-a s (see the :ref:`disk images`
    chapter in the System Emulation Users Guide).

    .. warning::
       snapshot is incompatible with ``-blockdev``.
       If you have mixed ``-blockdev`` and ``-drive`` declarations you
       can use the 'snapshot' property on your drive declarations
       instead of this global option.

ERST

DEFHEADING()

DEFHEADING(Display options:)

DEF("display", HAS_ARG, QEMU_OPTION_display,
#if defined(CONFIG_AGL)
    "-display agl\n"
#endif
    "-display none\n"
    "                select display backend type\n"
    "                The default display is equivalent to\n                "
#if defined(CONFIG_AGL)
            "\"-display agl\"\n"
#else
            "\"-display none\"\n"
#endif
    , QEMU_ARCH_ALL)
SRST
``-display type``
    Select type of display to use. Use ``-display help`` to list the available
    display types. Valid values for type are

    ``agl``
        Display video output through the Android native window backend.

    ``none``
        Do not display video output. The guest will still see an
        emulated graphics card, but its output will not be displayed to
        the QEMU user. This option differs from the -nographic option in
        that it only affects what is done with video output; -nographic
        also changes the destination of the serial and parallel port
        data.
ERST

DEF("nographic", 0, QEMU_OPTION_nographic,
    "-nographic      disable graphical output and redirect serial I/Os to console\n",
    QEMU_ARCH_ALL)
SRST
``-nographic``
    Normally, if QEMU is compiled with graphical window support, it
    displays output such as guest graphics, guest console, and the QEMU
    monitor in a window. With this option, you can totally disable
    graphical output so that QEMU is a simple command line application.
    The emulated serial port is redirected on the console and muxed with
    the monitor (unless redirected elsewhere explicitly). Therefore, you
    can still use QEMU to debug a Linux kernel with a serial console.
    Use C-a h for help on switching between the console and monitor.
ERST

#ifdef CONFIG_SPICE
DEF("spice", HAS_ARG, QEMU_OPTION_spice,
    "-spice [port=port][,tls-port=secured-port][,x509-dir=<dir>]\n"
    "       [,x509-key-file=<file>][,x509-key-password=<file>]\n"
    "       [,x509-cert-file=<file>][,x509-cacert-file=<file>]\n"
    "       [,x509-dh-key-file=<file>][,addr=addr]\n"
    "       [,ipv4=on|off][,ipv6=on|off][,unix=on|off]\n"
    "       [,tls-ciphers=<list>]\n"
    "       [,tls-channel=[main|display|cursor|inputs|record|playback]]\n"
    "       [,plaintext-channel=[main|display|cursor|inputs|record|playback]]\n"
    "       [,sasl=on|off][,disable-ticketing=on|off]\n"
    "       [,password-secret=<secret-id>]\n"
    "       [,image-compression=[auto_glz|auto_lz|quic|glz|lz|off]]\n"
    "       [,jpeg-wan-compression=[auto|never|always]]\n"
    "       [,zlib-glz-wan-compression=[auto|never|always]]\n"
    "       [,streaming-video=[off|all|filter]][,disable-copy-paste=on|off]\n"
    "       [,disable-agent-file-xfer=on|off][,agent-mouse=[on|off]]\n"
    "       [,playback-compression=[on|off]][,seamless-migration=[on|off]]\n"
    "       [,gl=[on|off]][,rendernode=<file>]\n"
    "                enable spice\n"
    "                at least one of {port, tls-port} is mandatory\n",
    QEMU_ARCH_ALL)
#endif
SRST
``-spice option[,option[,...]]``
    Enable the spice remote desktop protocol. Valid options are

    ``port=<nr>``
        Set the TCP port spice is listening on for plaintext channels.

    ``addr=<addr>``
        Set the IP address spice is listening on. Default is any
        address.

    ``ipv4=on|off``; \ ``ipv6=on|off``; \ ``unix=on|off``
        Force using the specified IP version.

    ``password-secret=<secret-id>``
        Set the ID of the ``secret`` object containing the password
        you need to authenticate.

    ``sasl=on|off``
        Require that the client use SASL to authenticate with the spice.
        The exact choice of authentication method used is controlled
        from the system / user's SASL configuration file for the 'qemu'
        service. This is typically found in /etc/sasl2/qemu.conf. If
        running QEMU as an unprivileged user, an environment variable
        SASL\_CONF\_PATH can be used to make it search alternate
        locations for the service config. While some SASL auth methods
        can also provide data encryption (eg GSSAPI), it is recommended
        that SASL always be combined with the 'tls' and 'x509' settings
        to enable use of SSL and server certificates. This ensures a
        data encryption preventing compromise of authentication
        credentials.

    ``disable-ticketing=on|off``
        Allow client connects without authentication.

    ``disable-copy-paste=on|off``
        Disable copy paste between the client and the guest.

    ``disable-agent-file-xfer=on|off``
        Disable spice-vdagent based file-xfer between the client and the
        guest.

    ``tls-port=<nr>``
        Set the TCP port spice is listening on for encrypted channels.

    ``x509-dir=<dir>``
        Set the x509 file directory.

    ``x509-key-file=<file>``; \ ``x509-key-password=<file>``; \ ``x509-cert-file=<file>``; \ ``x509-cacert-file=<file>``; \ ``x509-dh-key-file=<file>``
        The x509 file names can also be configured individually.

    ``tls-ciphers=<list>``
        Specify which ciphers to use.

    ``tls-channel=[main|display|cursor|inputs|record|playback]``; \ ``plaintext-channel=[main|display|cursor|inputs|record|playback]``
        Force specific channel to be used with or without TLS
        encryption. The options can be specified multiple times to
        configure multiple channels. The special name "default" can be
        used to set the default mode. For channels which are not
        explicitly forced into one mode the spice client is allowed to
        pick tls/plaintext as he pleases.

    ``image-compression=[auto_glz|auto_lz|quic|glz|lz|off]``
        Configure image compression (lossless). Default is auto\_glz.

    ``jpeg-wan-compression=[auto|never|always]``; \ ``zlib-glz-wan-compression=[auto|never|always]``
        Configure wan image compression (lossy for slow links). Default
        is auto.

    ``streaming-video=[off|all|filter]``
        Configure video stream detection. Default is off.

    ``agent-mouse=[on|off]``
        Enable/disable passing mouse events via vdagent. Default is on.

    ``playback-compression=[on|off]``
        Enable/disable audio stream compression (using celt 0.5.1).
        Default is on.

    ``seamless-migration=[on|off]``
        Enable/disable spice seamless migration. Default is off.

    ``gl=[on|off]``
        Enable/disable OpenGL context. Default is off.

    ``rendernode=<file>``
        DRM render node for OpenGL rendering. If not specified, it will
        pick the first available. (Since 2.9)
ERST

DEF("full-screen", 0, QEMU_OPTION_full_screen,
    "-full-screen    start in full screen\n", QEMU_ARCH_ALL)
SRST
``-full-screen``
    Start in full screen.
ERST

DEF("g", HAS_ARG, QEMU_OPTION_g ,
    "-g WxH[xDEPTH]  Set the initial graphical resolution and depth\n",
    QEMU_ARCH_PPC | QEMU_ARCH_SPARC | QEMU_ARCH_M68K)
SRST
``-g`` *width*\ ``x``\ *height*\ ``[x``\ *depth*\ ``]``
    Set the initial graphical resolution and depth (PPC, SPARC only).

    For PPC the default is 800x600x32.

    For SPARC with the TCX graphics device, the default is 1024x768x8
    with the option of 1024x768x24. For cgthree, the default is
    1024x768x8 with the option of 1152x900x8 for people who wish to use
    OBP.
ERST

ARCHHEADING(, QEMU_ARCH_I386)

ARCHHEADING(i386 target only:, QEMU_ARCH_I386)

DEF("win2k-hack", 0, QEMU_OPTION_win2k_hack,
    "-win2k-hack     use it when installing Windows 2000 to avoid a disk full bug\n",
    QEMU_ARCH_I386)
SRST
``-win2k-hack``
    Use it when installing Windows 2000 to avoid a disk full bug. After
    Windows 2000 is installed, you no longer need this option (this
    option slows down the IDE transfers).  Synonym of ``-global
    ide-device.win2k-install-hack=on``.
ERST

DEF("no-fd-bootchk", 0, QEMU_OPTION_no_fd_bootchk,
    "-no-fd-bootchk  disable boot signature checking for floppy disks\n",
    QEMU_ARCH_I386)
SRST
``-no-fd-bootchk``
    Disable boot signature checking for floppy disks in BIOS. May be
    needed to boot from old floppy disks.  Synonym of ``-m fd-bootchk=off``.
ERST

DEF("acpitable", HAS_ARG, QEMU_OPTION_acpitable,
    "-acpitable [sig=str][,rev=n][,oem_id=str][,oem_table_id=str][,oem_rev=n][,asl_compiler_id=str][,asl_compiler_rev=n][,{data|file}=file1[:file2]...]\n"
    "                ACPI table description\n", QEMU_ARCH_I386)
SRST
``-acpitable [sig=str][,rev=n][,oem_id=str][,oem_table_id=str][,oem_rev=n] [,asl_compiler_id=str][,asl_compiler_rev=n][,data=file1[:file2]...]``
    Add ACPI table with specified header fields and context from
    specified files. For file=, take whole ACPI table from the specified
    files, including all ACPI headers (possible overridden by other
    options). For data=, only data portion of the table is used, all
    header information is specified in the command line. If a SLIC table
    is supplied to QEMU, then the SLIC's oem\_id and oem\_table\_id
    fields will override the same in the RSDT and the FADT (a.k.a.
    FACP), in order to ensure the field matches required by the
    Microsoft SLIC spec and the ACPI spec.
ERST

DEFHEADING()

DEFHEADING(Network options:)

DEF("netdev", HAS_ARG, QEMU_OPTION_netdev,
    "-netdev tap,id=str,ifname=name,script=no,downscript=no\n"
    "                configure a TAP network backend with ID 'str'\n"
    , QEMU_ARCH_ALL)
DEF("nic", HAS_ARG, QEMU_OPTION_nic,
    "-nic tap[,option][,...][mac=macaddr]\n"
    "                initialize a default NIC and connect it to TAP networking\n"
    "-nic none       use it alone to have zero network devices\n",
    QEMU_ARCH_ALL)
DEF("net", HAS_ARG, QEMU_OPTION_net,
    "-net nic[,netdev=nd][,macaddr=mac][,model=type][,name=str][,addr=str][,vectors=v]\n"
    "                legacy NIC option; prefer -netdev tap + -device virtio-net-pci\n"
    "-net none       use it alone to have zero network devices\n",
    QEMU_ARCH_ALL)
SRST
``-nic user[,option][,...][,mac=macaddr][,model=mn]``
    Shortcut for configuring the default guest NIC hardware and the
    user-mode host network backend in one option. The guest NIC model can
    be set with ``model=modelname``; for this trimmed build, use the virtio
    PCI NIC path when adding devices explicitly.

``-nic none``
    Disable automatic network device creation.

``-netdev user,id=id[,option][,option][,...]``
    Configure user mode host networking, which requires no administrator
    privilege to run. Use it with ``virtio-net-pci`` via ``netdev=id``.

    ``id=id``
        Assign symbolic name for device options and monitor commands.

    ``ipv4=on|off and ipv6=on|off``
        Specify that either IPv4 or IPv6 must be enabled. If neither is
        specified both protocols are enabled.

    ``net=addr[/mask]``
        Set IP network address the guest will see. Optionally specify the
        netmask, either in the form a.b.c.d or as number of valid top-most
        bits. Default is 10.0.2.0/24.

    ``host=addr``
        Specify the guest-visible address of the host. Default is the 2nd IP
        in the guest network, i.e. x.x.x.2.

    ``restrict=on|off``
        Isolate the guest from the host and external network, except for
        explicitly configured forwarding rules.

    ``hostname=name``
        Client hostname reported by the built-in DHCP server.

    ``dhcpstart=addr``
        First DHCP address the built-in DHCP server can assign.

    ``dns=addr``; ``ipv6-dns=addr``; ``dnssearch=domain``; ``domainname=domain``
        DNS settings reported to the guest.

    ``tftp=dir``; ``tftp-server-name=name``; ``bootfile=file``
        Built-in TFTP and network boot settings.

    ``hostfwd=[tcp|udp]:[hostaddr]:hostport-[guestaddr]:guestport``
        Redirect incoming TCP or UDP host connections to the guest. This
        option can be given multiple times.

    ``guestfwd=[tcp]:server:port-dev``; ``guestfwd=[tcp]:server:port-cmd:command``
        Forward guest TCP connections to a character device or command.

    Example:

    .. parsed-literal::

        |qemu_system| -netdev user,id=usernet,hostfwd=tcp::2222-:22 \
            -device virtio-net-pci,netdev=usernet

``-net nic[,netdev=nd][,macaddr=mac][,model=type][,name=name][,addr=addr][,vectors=v]``
    Legacy option to configure or create a default NIC. Prefer explicit
    ``-netdev user`` plus ``-device virtio-net-pci,netdev=id``.

``-net none``
    Disable automatic network device creation.
ERST

DEFHEADING()

DEFHEADING(Character device options:)

DEF("chardev", HAS_ARG, QEMU_OPTION_chardev,
    "-chardev help\n"
    "-chardev null,id=id[,mux=on|off][,logfile=PATH][,logappend=on|off]\n"
    "-chardev socket,id=id[,host=host],port=port[,to=to][,ipv4=on|off][,ipv6=on|off][,nodelay=on|off]\n"
    "         [,server=on|off][,wait=on|off][,telnet=on|off][,websocket=on|off][,reconnect-ms=milliseconds][,mux=on|off]\n"
    "         [,logfile=PATH][,logappend=on|off][,tls-creds=ID] (tcp)\n"
    "-chardev socket,id=id,path=path[,server=on|off][,wait=on|off][,telnet=on|off][,websocket=on|off][,reconnect-ms=milliseconds]\n"
    "         [,mux=on|off][,logfile=PATH][,logappend=on|off][,abstract=on|off][,tight=on|off] (unix)\n"
    "-chardev udp,id=id[,host=host],port=port[,localaddr=localaddr]\n"
    "         [,localport=localport][,ipv4=on|off][,ipv6=on|off][,mux=on|off]\n"
    "         [,logfile=PATH][,logappend=on|off]\n"
    "-chardev vc,id=id[[,width=width][,height=height]][[,cols=cols][,rows=rows]]\n"
    "         [,mux=on|off][,logfile=PATH][,logappend=on|off]\n"
    "-chardev ringbuf,id=id[,size=size][,logfile=PATH][,logappend=on|off]\n"
    "-chardev file,id=id,path=path[,input-path=input-file][,mux=on|off][,logfile=PATH][,logappend=on|off]\n"
    "-chardev pipe,id=id,path=path[,mux=on|off][,logfile=PATH][,logappend=on|off]\n"
#ifdef _WIN32
    "-chardev console,id=id[,mux=on|off][,logfile=PATH][,logappend=on|off]\n"
    "-chardev serial,id=id,path=path[,mux=on|off][,logfile=PATH][,logappend=on|off]\n"
#else
    "-chardev pty,id=id[,path=path][,mux=on|off][,logfile=PATH][,logappend=on|off]\n"
    "-chardev stdio,id=id[,mux=on|off][,signal=on|off][,logfile=PATH][,logappend=on|off]\n"
#endif
#ifdef CONFIG_BRLAPI
    "-chardev braille,id=id[,mux=on|off][,logfile=PATH][,logappend=on|off]\n"
#endif
#if defined(__linux__) || defined(__sun__) || defined(__FreeBSD__) \
        || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
    "-chardev serial,id=id,path=path[,mux=on|off][,logfile=PATH][,logappend=on|off]\n"
#endif
#if defined(__linux__) || defined(__FreeBSD__) || defined(__DragonFly__)
    "-chardev parallel,id=id,path=path[,mux=on|off][,logfile=PATH][,logappend=on|off]\n"
#endif
#if defined(CONFIG_SPICE)
    "-chardev spicevmc,id=id,name=name[,debug=debug][,logfile=PATH][,logappend=on|off]\n"
    "-chardev spiceport,id=id,name=name[,debug=debug][,logfile=PATH][,logappend=on|off]\n"
#endif
    , QEMU_ARCH_ALL
)

SRST
The general form of a character device option is:

``-chardev backend,id=id[,mux=on|off][,options]``
    Backend is one of: ``null``, ``socket``, ``udp``, ``hub``,
    ``vc``, ``ringbuf``, ``file``, ``pipe``, ``console``, ``serial``,
    ``pty``, ``stdio``, ``braille``, ``parallel``,
    ``spicevmc``, ``spiceport``. The specific backend will determine the
    applicable options.

    Use ``-chardev help`` to print all available chardev backend types.

    All devices must have an id, which can be any string up to 127
    characters long. It is used to uniquely identify this device in
    other command line directives.

    A character device may be used in multiplexing mode by multiple
    front-ends. Specify ``mux=on`` to enable this mode. A multiplexer is
    a "1:N" device, and here the "1" end is your specified chardev
    backend, and the "N" end is the various parts of QEMU that can talk
    to a chardev. If you create a chardev with ``id=myid`` and
    ``mux=on``, QEMU will create a multiplexer with your specified ID,
    and you can then configure multiple front ends to use that chardev
    ID for their input/output. Up to four different front ends can be
    connected to a single multiplexed chardev. (Without multiplexing
    enabled, a chardev can only be used by a single front end.) For
    instance you could use this to allow a single stdio chardev to be
    used by two serial ports and the QEMU monitor:

    ::

        -chardev stdio,mux=on,id=char0 \
        -mon chardev=char0 \
        -serial chardev:char0 \
        -serial chardev:char0

    You can have more than one multiplexer in a system configuration;
    for instance you could have a TCP port multiplexed between UART 0
    and UART 1, and stdio multiplexed between the QEMU monitor and a
    parallel port:

    ::

        -chardev stdio,mux=on,id=char0 \
        -mon chardev=char0 \
        -parallel chardev:char0 \
        -chardev tcp,...,mux=on,id=char1 \
        -serial chardev:char1 \
        -serial chardev:char1

    When you're using a multiplexed character device, some escape
    sequences are interpreted in the input. See the chapter about
    :ref:`keys in the character backend multiplexer` in the
    System Emulation Users Guide for more details.

    Note that some other command line options may implicitly create
    multiplexed character backends; for instance ``-serial mon:stdio``
    creates a multiplexed stdio backend connected to the serial port and
    the QEMU monitor, and ``-nographic`` also multiplexes the console
    and the monitor to stdio.

    If you need to aggregate data in the opposite direction (where one
    QEMU frontend interface receives input and output from multiple
    backend chardev devices), please refer to the paragraph below
    regarding chardev ``hub`` aggregator device configuration.

    Every backend supports the ``logfile`` option, which supplies the
    path to a file to record all data transmitted via the backend. The
    ``logappend`` option controls whether the log file will be truncated
    or appended to when opened.

The available backends are:

``-chardev null,id=id``
    A void device. This device will not emit any data, and will drop any
    data it receives. The null backend does not take any options.

``-chardev socket,id=id[,TCP options or unix options][,server=on|off][,wait=on|off][,telnet=on|off][,websocket=on|off][,reconnect-ms=milliseconds][,tls-creds=id]``
    Create a two-way stream socket, which can be either a TCP or a unix
    socket. A unix socket will be created if ``path`` is specified.
    Behaviour is undefined if TCP options are specified for a unix
    socket.

    ``server=on|off`` specifies that the socket shall be a listening socket.

    ``wait=on|off`` specifies that QEMU should not block waiting for a client
    to connect to a listening socket.

    ``telnet=on|off`` specifies that traffic on the socket should interpret
    telnet escape sequences.

    ``websocket=on|off`` specifies that the socket uses WebSocket protocol for
    communication.

    ``reconnect-ms`` sets the timeout for reconnecting on non-server
    sockets when the remote end goes away. qemu will delay this many
    milliseconds and then attempt to reconnect. Zero disables reconnecting,
    and is the default.

    ``tls-creds`` requests enablement of the TLS protocol for
    encryption, and specifies the id of the TLS credentials to use for
    the handshake. The credentials must be previously created with the
    ``-object tls-creds`` argument.

    TCP and unix socket options are given below:

    ``TCP options: port=port[,host=host][,to=to][,ipv4=on|off][,ipv6=on|off][,nodelay=on|off]``
        ``host`` for a listening socket specifies the local address to
        be bound. For a connecting socket species the remote host to
        connect to. ``host`` is optional for listening sockets. If not
        specified it defaults to ``0.0.0.0``.

        ``port`` for a listening socket specifies the local port to be
        bound. For a connecting socket specifies the port on the remote
        host to connect to. ``port`` can be given as either a port
        number or a service name. ``port`` is required.

        ``to`` is only relevant to listening sockets. If it is
        specified, and ``port`` cannot be bound, QEMU will attempt to
        bind to subsequent ports up to and including ``to`` until it
        succeeds. ``to`` must be specified as a port number.

        ``ipv4=on|off`` and ``ipv6=on|off`` specify that either IPv4
        or IPv6 must be used. If neither is specified the socket may
        use either protocol.

        ``nodelay=on|off`` disables the Nagle algorithm.

    ``unix options: path=path[,abstract=on|off][,tight=on|off]``
        ``path`` specifies the local path of the unix socket. ``path``
        is required.
        ``abstract=on|off`` specifies the use of the abstract socket namespace,
        rather than the filesystem.  Optional, defaults to false.
        ``tight=on|off`` sets the socket length of abstract sockets to their minimum,
        rather than the full sun_path length.  Optional, defaults to true.

``-chardev udp,id=id[,host=host],port=port[,localaddr=localaddr][,localport=localport][,ipv4=on|off][,ipv6=on|off]``
    Sends all traffic from the guest to a remote host over UDP.

    ``host`` specifies the remote host to connect to. If not specified
    it defaults to ``localhost``.

    ``port`` specifies the port on the remote host to connect to.
    ``port`` is required.

    ``localaddr`` specifies the local address to bind to. If not
    specified it defaults to ``0.0.0.0``.

    ``localport`` specifies the local port to bind to. If not specified
    any available local port will be used.

    ``ipv4=on|off`` and ``ipv6=on|off`` specify that either IPv4 or IPv6 must be used.
    If neither is specified the device may use either protocol.

``-chardev hub,id=id,chardevs.0=id[,chardevs.N=id]``
    Explicitly create chardev backend hub device with the possibility
    to aggregate input from multiple backend devices and forward it to
    a single frontend device. Additionally, ``hub`` device takes the
    output from the frontend device and sends it back to all the
    connected backend devices. This allows for seamless interaction
    between different backend devices and a single frontend
    interface. Aggregation supported for up to 4 chardev
    devices. (Since 10.0)

    For example, the following is a use case of 2 backend devices:
    virtual console ``vc0`` and a pseudo TTY ``pty0`` connected to
    a single virtio hvc console frontend device with a hub ``hub0``
    help. Virtual console renders text to an image, while pty backend
    provides bidirectional communication to the virtio hvc console over
    the pseudo TTY file. The example configuration can be as follows:

    ::

       -chardev pty,path=/tmp/pty,id=pty0 \
       -chardev vc,id=vc0 \
       -chardev hub,id=hub0,chardevs.0=pty0,chardevs.1=vc0 \
       -device virtconsole,chardev=hub0

    Any TTY emulator can be used to control a single hvc console:

    ::

       # Start TTY emulator
       tio /tmp/pty

    Several frontend devices is not supported. Stacking of multiplexers
    and hub devices is not supported as well.

``-chardev vc,id=id[[,width=width][,height=height]][[,cols=cols][,rows=rows]]``
    Connect to a QEMU text console. ``vc`` may optionally be given a
    specific size.

    ``width`` and ``height`` specify the width and height respectively
    of the console, in pixels.

    ``cols`` and ``rows`` specify that the console be sized to fit a
    text console with the given dimensions.

``-chardev ringbuf,id=id[,size=size]``
    Create a ring buffer with fixed size ``size``. size must be a power
    of two and defaults to ``64K``.

``-chardev file,id=id,path=path[,input-path=input-path]``
    Log all traffic received from the guest to a file.

    ``path`` specifies the path of the file to be opened. This file will
    be created if it does not already exist, and overwritten if it does.
    ``path`` is required.

    If ``input-path`` is specified, this is the path of a second file
    which will be used for input. If ``input-path`` is not specified,
    no input will be available from the chardev.

    Note that ``input-path`` is not supported on Windows hosts.

``-chardev pipe,id=id,path=path``
    Create a two-way connection to the guest. The behaviour differs
    slightly between Windows hosts and other hosts:

    On Windows, a single duplex pipe will be created at
    ``\\.pipe\path``.

    On other hosts, 2 pipes will be created called ``path.in`` and
    ``path.out``. Data written to ``path.in`` will be received by the
    guest. Data written by the guest can be read from ``path.out``. QEMU
    will not create these fifos, and requires them to be present.

    ``path`` forms part of the pipe path as described above. ``path`` is
    required.

``-chardev console,id=id``
    Send traffic from the guest to QEMU's standard output. ``console``
    does not take any options.

    ``console`` is only available on Windows hosts.

``-chardev serial,id=id,path=path``
    Send traffic from the guest to a serial device on the host.

    On Unix hosts serial will actually accept any tty device, not only
    serial lines.

    ``path`` specifies the name of the serial device to open.

``-chardev pty,id=id[,path=path]``
    Create a new pseudo-terminal on the host and connect to it.

    ``pty`` is not available on Windows hosts.

    If ``path`` is specified, QEMU will create a symbolic link at
    that location which points to the new PTY device.

    This avoids having to make QMP or HMP monitor queries to find out
    what the new PTY device path is.

    Note that while QEMU will remove the symlink when it exits
    gracefully, it will not do so in case of crashes or on certain
    startup errors. It is recommended that the user checks and removes
    the symlink after QEMU terminates to account for this.

``-chardev stdio,id=id[,signal=on|off]``
    Connect to standard input and standard output of the QEMU process.

    ``signal`` controls if signals are enabled on the terminal, that
    includes exiting QEMU with the key sequence Control-c. This option
    is enabled by default, use ``signal=off`` to disable it.

``-chardev braille,id=id``
    Connect to a local BrlAPI server. ``braille`` does not take any
    options.

``-chardev parallel,id=id,path=path``
  \
    ``parallel`` is only available on Linux, FreeBSD and DragonFlyBSD
    hosts.

    Connect to a local parallel port.

    ``path`` specifies the path to the parallel port device. ``path`` is
    required.

``-chardev spicevmc,id=id,debug=debug,name=name``
    ``spicevmc`` is only available when spice support is built in.

    ``debug`` debug level for spicevmc

    ``name`` name of spice channel to connect to

    Connect to a spice virtual machine channel, such as vdiport.

``-chardev spiceport,id=id,debug=debug,name=name``
    ``spiceport`` is only available when spice support is built in.

    ``debug`` debug level for spicevmc

    ``name`` name of spice port to connect to

    Connect to a spice port, allowing a Spice client to handle the
    traffic identified by a name (preferably a fqdn).
ERST

DEFHEADING()

#ifdef CONFIG_TPM
DEFHEADING(TPM device options:)

DEF("tpmdev", HAS_ARG, QEMU_OPTION_tpmdev, \
    "-tpmdev passthrough,id=id[,path=path][,cancel-path=path]\n"
    "                use path to provide path to a character device; default is /dev/tpm0\n"
    "                use cancel-path to provide path to TPM's cancel sysfs entry; if\n"
    "                not provided it will be searched for in /sys/class/misc/tpm?/device\n"
    "-tpmdev emulator,id=id,chardev=dev\n"
    "                configure the TPM device using chardev backend\n",
    QEMU_ARCH_ALL)
SRST
The general form of a TPM device option is:

``-tpmdev backend,id=id[,options]``
    The specific backend type will determine the applicable options. The
    ``-tpmdev`` option creates the TPM backend and requires a
    ``-device`` option that specifies the TPM frontend interface model.

    Use ``-tpmdev help`` to print all available TPM backend types.

The available backends are:

``-tpmdev passthrough,id=id,path=path,cancel-path=cancel-path``
    (Linux-host only) Enable access to the host's TPM using the
    passthrough driver.

    ``path`` specifies the path to the host's TPM device, i.e., on a
    Linux host this would be ``/dev/tpm0``. ``path`` is optional and by
    default ``/dev/tpm0`` is used.

    ``cancel-path`` specifies the path to the host TPM device's sysfs
    entry allowing for cancellation of an ongoing TPM command.
    ``cancel-path`` is optional and by default QEMU will search for the
    sysfs entry to use.

    Some notes about using the host's TPM with the passthrough driver:

    The TPM device accessed by the passthrough driver must not be used
    by any other application on the host.

    Since the host's firmware (BIOS/UEFI) has already initialized the
    TPM, the VM's firmware (BIOS/UEFI) will not be able to initialize
    the TPM again and may therefore not show a TPM-specific menu that
    would otherwise allow the user to configure the TPM, e.g., allow the
    user to enable/disable or activate/deactivate the TPM. Further, if
    TPM ownership is released from within a VM then the host's TPM will
    get disabled and deactivated. To enable and activate the TPM again
    afterwards, the host has to be rebooted and the user is required to
    enter the firmware's menu to enable and activate the TPM. If the TPM
    is left disabled and/or deactivated most TPM commands will fail.

    To create a passthrough TPM use the following two options:

    ::

        -tpmdev passthrough,id=tpm0 -device tpm-tis,tpmdev=tpm0

    Note that the ``-tpmdev`` id is ``tpm0`` and is referenced by
    ``tpmdev=tpm0`` in the device option.

``-tpmdev emulator,id=id,chardev=dev``
    (Linux-host only) Enable access to a TPM emulator using Unix domain
    socket based chardev backend.

    ``chardev`` specifies the unique ID of a character device backend
    that provides connection to the software TPM server.

    To create a TPM emulator backend device with chardev socket backend:

    ::

        -chardev socket,id=chrtpm,path=/tmp/swtpm-sock -tpmdev emulator,id=tpm0,chardev=chrtpm -device tpm-tis,tpmdev=tpm0
ERST

DEFHEADING()

#endif

DEFHEADING(Boot Image or Kernel specific:)
SRST
There are broadly 4 ways you can boot a system with QEMU.

 - specify a firmware and let it control finding a kernel
 - specify a firmware and pass a hint to the kernel to boot
 - direct kernel image boot
 - manually load files into the guest's address space

The third method is useful for quickly testing kernels but as there is
no firmware to pass configuration information to the kernel the
hardware must either be probeable, the kernel built for the exact
configuration or passed some configuration data (e.g. a DTB blob)
which tells the kernel what drivers it needs. This exact details are
often hardware specific.

The final method is the most generic way of loading images into the
guest address space and used mostly for ``bare metal`` type
development where the reset vectors of the processor are taken into
account.

ERST

SRST

ERST

DEF("bios", HAS_ARG, QEMU_OPTION_bios, \
    "-bios file      set the filename for the BIOS\n", QEMU_ARCH_ALL)
SRST
``-bios file``
    Set the filename for the BIOS.
ERST

SRST

The kernel options were designed to work with Linux kernels although
other things (like hypervisors) can be packaged up as a kernel
executable image. The exact format of a executable image is usually
architecture specific.

The way in which the kernel is started (what address it is loaded at,
what if any information is passed to it via CPU registers, the state
of the hardware when it is started, and so on) is also architecture
specific. Typically it follows the specification laid down by the
Linux kernel for how kernels for that architecture must be started.

ERST

DEF("kernel", HAS_ARG, QEMU_OPTION_kernel, \
    "-kernel bzImage use 'bzImage' as kernel image\n", QEMU_ARCH_ALL)
SRST
``-kernel bzImage``
    Use bzImage as kernel image. The kernel can be either a Linux kernel
    or in multiboot format.
ERST

DEF("append", HAS_ARG, QEMU_OPTION_append, \
    "-append cmdline use 'cmdline' as kernel command line\n", QEMU_ARCH_ALL)
SRST
``-append cmdline``
    Use cmdline as kernel command line
ERST

DEF("initrd", HAS_ARG, QEMU_OPTION_initrd, \
           "-initrd file    use 'file' as initial ram disk\n", QEMU_ARCH_ALL)
SRST(initrd)

``-initrd file``
    Use file as initial ram disk.

``-initrd "file1 arg=foo,file2"``
    This syntax is only available with multiboot.

    Use file1 and file2 as modules and pass ``arg=foo`` as parameter to the
    first module. Commas can be provided in module parameters by doubling
    them on the command line to escape them:

``-initrd "bzImage earlyprintk=xen,,keep root=/dev/xvda1,initrd.img"``
    Multiboot only. Use bzImage as the first module with
    "``earlyprintk=xen,keep root=/dev/xvda1``" as its command line,
    and initrd.img as the second module.

ERST

DEF("dtb", HAS_ARG, QEMU_OPTION_dtb, \
    "-dtb    file    use 'file' as device tree image\n", QEMU_ARCH_ALL)
SRST
``-dtb file``
    Use file as a device tree binary (dtb) image and pass it to the
    kernel on boot.
ERST

SRST

Finally you can also manually load images directly into the address
space of the guest. This is most useful for developers who already
know the layout of their guest and take care to ensure something sane
will happen when the reset vector executes.

The generic loader can be invoked by using the loader device:

``-device loader,addr=<addr>,data=<data>,data-len=<data-len>[,data-be=<data-be>][,cpu-num=<cpu-num>]``

there is also the guest loader which operates in a similar way but
tweaks the DTB so a hypervisor loaded via ``-kernel`` can find where
the guest image is:

``-device guest-loader,addr=<addr>[,kernel=<path>,[bootargs=<arguments>]][,initrd=<path>]``

ERST

DEFHEADING()

DEFHEADING(Debug/Expert options:)

DEF("compat", HAS_ARG, QEMU_OPTION_compat,
    "-compat [deprecated-input=accept|reject|crash][,deprecated-output=accept|hide]\n"
    "                Policy for handling deprecated management interfaces\n"
    "-compat [unstable-input=accept|reject|crash][,unstable-output=accept|hide]\n"
    "                Policy for handling unstable management interfaces\n",
    QEMU_ARCH_ALL)
SRST
``-compat [deprecated-input=@var{input-policy}][,deprecated-output=@var{output-policy}]``
    Set policy for handling deprecated management interfaces (experimental):

    ``deprecated-input=accept`` (default)
        Accept deprecated commands and arguments
    ``deprecated-input=reject``
        Reject deprecated commands and arguments
    ``deprecated-input=crash``
        Crash on deprecated commands and arguments
    ``deprecated-output=accept`` (default)
        Emit deprecated command results and events
    ``deprecated-output=hide``
        Suppress deprecated command results and events

    Limitation: covers only syntactic aspects of QMP.

``-compat [unstable-input=@var{input-policy}][,unstable-output=@var{output-policy}]``
    Set policy for handling unstable management interfaces (experimental):

    ``unstable-input=accept`` (default)
        Accept unstable commands and arguments
    ``unstable-input=reject``
        Reject unstable commands and arguments
    ``unstable-input=crash``
        Crash on unstable commands and arguments
    ``unstable-output=accept`` (default)
        Emit unstable command results and events
    ``unstable-output=hide``
        Suppress unstable command results and events

    Limitation: covers only syntactic aspects of QMP.
ERST

DEF("fw_cfg", HAS_ARG, QEMU_OPTION_fwcfg,
    "-fw_cfg [name=]<name>,file=<file>\n"
    "                add named fw_cfg entry with contents from file\n"
    "-fw_cfg [name=]<name>,string=<str>\n"
    "                add named fw_cfg entry with contents from string\n",
    QEMU_ARCH_ALL)
SRST
``-fw_cfg [name=]name,file=file``
    Add named fw\_cfg entry with contents from file file.
    If the filename contains comma, you must double it (for instance,
    "file=my,,file" to use file "my,file").

``-fw_cfg [name=]name,string=str``
    Add named fw\_cfg entry with contents from string str.
    If the string contains comma, you must double it (for instance,
    "string=my,,string" to use file "my,string").

    The terminating NUL character of the contents of str will not be
    included as part of the fw\_cfg item data. To insert contents with
    embedded NUL characters, you have to use the file parameter.

    The fw\_cfg entries are passed by QEMU through to the guest.

    Example:

    ::

            -fw_cfg name=opt/com.mycompany/blob,file=./my_blob.bin

    creates an fw\_cfg entry named opt/com.mycompany/blob with contents
    from ./my\_blob.bin.
ERST

DEF("serial", HAS_ARG, QEMU_OPTION_serial, \
    "-serial dev     redirect the serial port to char device 'dev'\n",
    QEMU_ARCH_ALL)
SRST
``-serial dev``
    Redirect the virtual serial port to host character device dev. The
    default device is ``vc`` in graphical mode and ``stdio`` in non
    graphical mode.

    This option can be used several times to simulate multiple serial
    ports.

    You can use ``-serial none`` to suppress the creation of default
    serial devices.

    Available character devices are:

    ``vc[:WxH]``
        Virtual console. Optionally, a width and height can be given in
        pixel with

        ::

            vc:800x600

        It is also possible to specify width or height in characters:

        ::

            vc:80Cx24C

    ``pty[:path]``
        [Linux only] Pseudo TTY (a new PTY is automatically allocated).

        If ``path`` is specified, QEMU will create a symbolic link at
        that location which points to the new PTY device.

        This avoids having to make QMP or HMP monitor queries to find
        out what the new PTY device path is.

        Note that while QEMU will remove the symlink when it exits
        gracefully, it will not do so in case of crashes or on certain
        startup errors. It is recommended that the user checks and
        removes the symlink after QEMU terminates to account for this.

    ``none``
        No device is allocated. Note that for machine types which
        emulate systems where a serial device is always present in
        real hardware, this may be equivalent to the ``null`` option,
        in that the serial device is still present but all output
        is discarded. For boards where the number of serial ports is
        truly variable, this suppresses the creation of the device.

    ``null``
        A guest will see the UART or serial device as present in the
        machine, but all output is discarded, and there is no input.
        Conceptually equivalent to redirecting the output to ``/dev/null``.

    ``chardev:id``
        Use a named character device defined with the ``-chardev``
        option.

    ``/dev/XXX``
        [Linux only] Use host tty, e.g. ``/dev/ttyS0``. The host serial
        port parameters are set according to the emulated ones.

    ``/dev/parportN``
        [Linux only, parallel port only] Use host parallel port N.
        Currently SPP and EPP parallel port features can be used.

    ``file:filename``
        Write output to filename. No character can be read.

    ``stdio``
        [Unix only] standard input/output

    ``pipe:filename``
        name pipe filename

    ``COMn``
        [Windows only] Use host serial port n

    ``udp:[remote_host]:remote_port[@[src_ip]:src_port]``
        This implements UDP Net Console. When remote\_host or src\_ip
        are not specified they default to ``0.0.0.0``. When not using a
        specified src\_port a random port is automatically chosen.

        If you just want a simple readonly console you can use
        ``netcat`` or ``nc``, by starting QEMU with:
        ``-serial udp::4555`` and nc as: ``nc -u -l -p 4555``. Any time
        QEMU writes something to that port it will appear in the
        netconsole session.

        If you plan to send characters back via netconsole or you want
        to stop and start QEMU a lot of times, you should have QEMU use
        the same source port each time by using something like ``-serial
        udp::4555@:4556`` to QEMU. Another approach is to use a patched
        version of netcat which can listen to a TCP port and send and
        receive characters via udp. If you have a patched version of
        netcat which activates telnet remote echo and single char
        transfer, then you can use the following options to set up a
        netcat redirector to allow telnet on port 5555 to access the
        QEMU port.

        ``QEMU Options:``
            -serial udp::4555@:4556

        ``netcat options:``
            -u -P 4555 -L 0.0.0.0:4556 -t -p 5555 -I -T

        ``telnet options:``
            localhost 5555

    ``tcp:[host]:port[,server=on|off][,wait=on|off][,nodelay=on|off][,reconnect-ms=milliseconds]``
        The TCP Net Console has two modes of operation. It can send the
        serial I/O to a location or wait for a connection from a
        location. By default the TCP Net Console is sent to host at the
        port. If you use the ``server=on`` option QEMU will wait for a client
        socket application to connect to the port before continuing,
        unless the ``wait=on|off`` option was specified. The ``nodelay=on|off``
        option disables the Nagle buffering algorithm. The ``reconnect-ms``
        option only applies if ``server=no`` is set, if the connection goes
        down it will attempt to reconnect at the given interval. If host
        is omitted, 0.0.0.0 is assumed. Only one TCP connection at a
        time is accepted. You can use ``telnet=on`` to connect to the
        corresponding character device.

        ``Example to send tcp console to 192.168.0.2 port 4444``
            -serial tcp:192.168.0.2:4444

        ``Example to listen and wait on port 4444 for connection``
            -serial tcp::4444,server=on

        ``Example to not wait and listen on ip 192.168.0.100 port 4444``
            -serial tcp:192.168.0.100:4444,server=on,wait=off

    ``telnet:host:port[,server=on|off][,wait=on|off][,nodelay=on|off]``
        The telnet protocol is used instead of raw tcp sockets. The
        options work the same as if you had specified ``-serial tcp``.
        The difference is that the port acts like a telnet server or
        client using telnet option negotiation. This will also allow you
        to send the MAGIC\_SYSRQ sequence if you use a telnet that
        supports sending the break sequence. Typically in unix telnet
        you do it with Control-] and then type "send break" followed by
        pressing the enter key.

    ``websocket:host:port,server=on[,wait=on|off][,nodelay=on|off]``
        The WebSocket protocol is used instead of raw tcp socket. The
        port acts as a WebSocket server. Client mode is not supported.

    ``unix:path[,server=on|off][,wait=on|off][,reconnect-ms=milliseconds]``
        A unix domain socket is used instead of a tcp socket. The option
        works the same as if you had specified ``-serial tcp`` except
        the unix domain socket path is used for connections.

    ``mon:dev_string``
        This is a special option to allow the monitor to be multiplexed
        onto another serial port. The monitor is accessed with key
        sequence of Control-a and then pressing c. dev\_string should be
        any one of the serial devices specified above. An example to
        multiplex the monitor onto a telnet server listening on port
        4444 would be:

        ``-serial mon:telnet::4444,server=on,wait=off``

        When the monitor is multiplexed to stdio in this way, Ctrl+C
        will not terminate QEMU any more but will be passed to the guest
        instead.

    ``braille``
        Braille device. This will use BrlAPI to display the braille
        output on a real or fake device.

ERST

DEF("parallel", HAS_ARG, QEMU_OPTION_parallel, \
    "-parallel dev   redirect the parallel port to char device 'dev'\n",
    QEMU_ARCH_ALL)
SRST
``-parallel dev``
    Redirect the virtual parallel port to host device dev (same devices
    as the serial port). On Linux hosts, ``/dev/parportN`` can be used
    to use hardware devices connected on the corresponding host parallel
    port.

    This option can be used several times to simulate up to 3 parallel
    ports.

    Use ``-parallel none`` to disable all parallel ports.
ERST

DEF("monitor", HAS_ARG, QEMU_OPTION_monitor, \
    "-monitor dev    redirect the monitor to char device 'dev'\n",
    QEMU_ARCH_ALL)
SRST
``-monitor dev``
    Redirect the monitor to host device dev (same devices as the serial
    port). The default device is ``vc`` in graphical mode and ``stdio``
    in non graphical mode. Use ``-monitor none`` to disable the default
    monitor.
ERST
DEF("mon", HAS_ARG, QEMU_OPTION_mon, \
    "-mon [chardev=]name\n", QEMU_ARCH_ALL)
SRST
``-mon [chardev=]name``
    Set up an HMP monitor connected to the chardev ``name``.

    For example::

      -chardev socket,id=mon1,host=localhost,port=4444,server=on,wait=off \
      -mon chardev=mon1

    enables an HMP monitor on localhost port 4444.
ERST

DEF("pidfile", HAS_ARG, QEMU_OPTION_pidfile, \
    "-pidfile file   write PID to 'file'\n", QEMU_ARCH_ALL)
SRST
``-pidfile file``
    Store the QEMU process PID in file. It is useful if you launch QEMU
    from a script.
ERST

DEF("preconfig", 0, QEMU_OPTION_preconfig, \
    "--preconfig     pause QEMU before machine is initialized (experimental)\n",
    QEMU_ARCH_ALL)
SRST
``--preconfig``
    Pause QEMU for interactive configuration before the machine is
    created, which allows querying and configuring properties that will
    affect machine initialization. Use QMP command 'x-exit-preconfig' to
    exit the preconfig state and move to the next state (i.e. run guest
    if -S isn't used or pause the second time if -S is used). This
    option is experimental.
ERST

DEF("S", 0, QEMU_OPTION_S, \
    "-S              freeze CPU at startup (use 'c' to start execution)\n",
    QEMU_ARCH_ALL)
SRST
``-S``
    Do not start CPU at startup (you must type 'c' in the monitor).
ERST

DEF("overcommit", HAS_ARG, QEMU_OPTION_overcommit,
    "-overcommit [mem-lock=on|off|on-fault][cpu-pm=on|off]\n"
    "                run qemu with overcommit hints\n"
    "                mem-lock=on|off|on-fault controls memory lock support (default: off)\n"
    "                cpu-pm=on|off controls cpu power management (default: off)\n",
    QEMU_ARCH_ALL)
SRST
``-overcommit mem-lock=on|off|on-fault``
  \ 
``-overcommit cpu-pm=on|off``
    Run qemu with hints about host resource overcommit. The default is
    to assume that host overcommits all resources.

    Locking qemu and guest memory can be enabled via ``mem-lock=on``
    or ``mem-lock=on-fault`` (disabled by default). This works when
    host memory is not overcommitted and reduces the worst-case latency for
    guest. The on-fault option is better for reducing the memory footprint
    since it makes allocations lazy, but the pages still get locked in place
    once faulted by the guest or QEMU. Note that the two options are mutually
    exclusive.

    Guest ability to manage power state of host cpus (increasing latency
    for other processes on the same host cpu, but decreasing latency for
    guest) can be enabled via ``cpu-pm=on`` (disabled by default). This
    works best when host CPU is not overcommitted. When used, host
    estimates of CPU cycle and power utilization will be incorrect, not
    taking into account guest idle time.
ERST

DEF("d", HAS_ARG, QEMU_OPTION_d, \
    "-d item1,...    enable logging of specified items (use '-d help' for a list of log items)\n",
    QEMU_ARCH_ALL)
SRST
``-d item1[,...]``
    Enable logging of specified items. Use '-d help' for a list of log
    items.
ERST

DEF("D", HAS_ARG, QEMU_OPTION_D, \
    "-D logfile      output log to logfile (default stderr)\n",
    QEMU_ARCH_ALL)
SRST
``-D logfile``
    Output log in logfile instead of to stderr
ERST

DEF("seed", HAS_ARG, QEMU_OPTION_seed, \
    "-seed number       seed the pseudo-random number generator\n",
    QEMU_ARCH_ALL)
SRST
``-seed number``
    Force the guest to use a deterministic pseudo-random number
    generator, seeded with number. This does not affect crypto routines
    within the host.
ERST

DEF("L", HAS_ARG, QEMU_OPTION_L, \
    "-L path         set the directory for the BIOS, VGA BIOS and keymaps\n",
    QEMU_ARCH_ALL)
SRST
``-L  path``
    Set the directory for the BIOS, VGA BIOS and keymaps.

    To list all the data directories, use ``-L help``.
ERST

DEF("no-reboot", 0, QEMU_OPTION_no_reboot, \
    "-no-reboot      exit instead of rebooting\n", QEMU_ARCH_ALL)
SRST
``-no-reboot``
    Exit instead of rebooting.
ERST

DEF("no-shutdown", 0, QEMU_OPTION_no_shutdown, \
    "-no-shutdown    stop before shutdown\n", QEMU_ARCH_ALL)
SRST
``-no-shutdown``
    Don't exit QEMU on guest shutdown, but instead only stop the
    emulation. This allows for instance switching to monitor to commit
    changes to the disk image.
ERST

DEF("action", HAS_ARG, QEMU_OPTION_action,
    "-action reboot=reset|shutdown\n"
    "                   action when guest reboots [default=reset]\n"
    "-action shutdown=poweroff|pause\n"
    "                   action when guest shuts down [default=poweroff]\n"
    "-action panic=pause|shutdown|exit-failure|none\n"
    "                   action when guest panics [default=shutdown]\n",
    QEMU_ARCH_ALL)
SRST
``-action event=action``
    The action parameter serves to modify QEMU's default behavior when
    certain guest events occur. It provides a generic method for specifying the
    same behaviors that are modified by the ``-no-reboot`` and ``-no-shutdown``
    parameters.

    Examples:

    ``-action panic=none``
    ``-action reboot=shutdown,shutdown=pause``

ERST

#ifndef _WIN32
DEF("daemonize", 0, QEMU_OPTION_daemonize, \
    "-daemonize      daemonize QEMU after initializing\n", QEMU_ARCH_ALL)
#endif
SRST
``-daemonize``
    Daemonize the QEMU process after initialization. QEMU will not
    detach from standard IO until it is ready to receive connections on
    any of its devices. This option is a useful way for external
    programs to launch QEMU without having to cope with initialization
    race conditions.
ERST

DEF("rtc", HAS_ARG, QEMU_OPTION_rtc, \
    "-rtc [base=utc|localtime|<datetime>][,clock=host|rt|vm][,driftfix=none|slew]\n" \
    "                set the RTC base and clock, enable drift fix for clock ticks (x86 only)\n",
    QEMU_ARCH_ALL)

SRST
``-rtc [base=utc|localtime|datetime][,clock=host|rt|vm][,driftfix=none|slew]``
    Specify ``base`` as ``utc`` or ``localtime`` to let the RTC start at
    the current UTC or local time, respectively. ``localtime`` is
    required for correct date in MS-DOS or Windows. To start at a
    specific point in time, provide datetime in the format
    ``2006-06-17T16:01:21`` or ``2006-06-17``. The default base is UTC.

    By default the RTC is driven by the host system time. This allows
    using of the RTC as accurate reference clock inside the guest,
    specifically if the host time is smoothly following an accurate
    external reference clock, e.g. via NTP. If you want to isolate the
    guest time from the host, you can set ``clock`` to ``rt`` instead,
    which provides a host monotonic clock if host support it. To even
    prevent the RTC from progressing during suspension, you can set
    ``clock`` to ``vm`` (virtual clock).

    Enable ``driftfix`` (i386 targets only) if you experience time drift
    problems, specifically with Windows' ACPI HAL. This option will try
    to figure out how many timer interrupts were not processed by the
    Windows guest and will re-inject them.
ERST

DEF("echr", HAS_ARG, QEMU_OPTION_echr, \
    "-echr chr       set terminal escape character instead of ctrl-a\n",
    QEMU_ARCH_ALL)
SRST
``-echr numeric_ascii_value``
    Change the escape character used for switching to the monitor when
    using monitor and serial sharing. The default is ``0x01`` when using
    the ``-nographic`` option. ``0x01`` is equal to pressing
    ``Control-a``. You can select a different character from the ascii
    control keys where 1 through 26 map to Control-a through Control-z.
    For instance you could use the either of the following to change the
    escape character to Control-t.

    ``-echr 0x14``; \ ``-echr 20``

ERST

DEF("nodefaults", 0, QEMU_OPTION_nodefaults, \
    "-nodefaults     don't create default devices\n", QEMU_ARCH_ALL)
SRST
``-nodefaults``
    Don't create default devices. Normally, QEMU sets the default
    devices like serial port, parallel port, virtual console, monitor
    device, VGA adapter, floppy and CD-ROM drive and others. The
    ``-nodefaults`` option will disable all those default devices.
ERST

DEF("prom-env", HAS_ARG, QEMU_OPTION_prom_env,
    "-prom-env variable=value\n"
    "                set OpenBIOS nvram variables\n",
    QEMU_ARCH_PPC | QEMU_ARCH_SPARC)
SRST
``-prom-env variable=value``
    Set OpenBIOS nvram variable to given value (PPC, SPARC only).

    ::

        qemu-system-sparc -prom-env 'auto-boot?=false' \
         -prom-env 'boot-device=sd(0,2,0):d' -prom-env 'boot-args=linux single'

    ::

        qemu-system-ppc -prom-env 'auto-boot?=false' \
         -prom-env 'boot-device=hd:2,\yaboot' \
         -prom-env 'boot-args=conf=hd:2,\yaboot.conf'
ERST
DEF("readconfig", HAS_ARG, QEMU_OPTION_readconfig,
    "-readconfig <file>\n"
    "                read config file\n", QEMU_ARCH_ALL)
SRST
``-readconfig file``
    Read device configuration from file. This approach is useful when
    you want to spawn QEMU process with many command line options but
    you don't want to exceed the command line character limit.
ERST

DEF("no-user-config", 0, QEMU_OPTION_nouserconfig,
    "-no-user-config\n"
    "                do not load default user-provided config files at startup\n",
    QEMU_ARCH_ALL)
SRST
``-no-user-config``
    The ``-no-user-config`` option makes QEMU not load any of the
    user-provided config files on sysconfdir.
ERST

DEF("trace", HAS_ARG, QEMU_OPTION_trace,
    "-trace [[enable=]<pattern>][,events=<file>][,file=<file>]\n"
    "                specify tracing options\n",
    QEMU_ARCH_ALL)
SRST
``-trace [[enable=]pattern][,events=file][,file=file]``
  .. include:: ../qemu-option-trace.rst.inc

ERST
#ifdef CONFIG_POSIX
DEF("run-with", HAS_ARG, QEMU_OPTION_run_with,
    "-run-with [chroot=dir][user=username|uid:gid]\n"
    "                Set miscellaneous QEMU process lifecycle options:\n"
    "                chroot=dir chroot to dir just before starting the VM\n"
    "                user=username switch to the specified user before starting the VM\n"
    "                user=uid:gid ditto, but use specified user-ID and group-ID instead\n",
    QEMU_ARCH_ALL)
SRST
``-run-with [chroot=dir][user=username|uid:gid]``
    Set QEMU process lifecycle options.

    ``chroot=dir`` can be used for doing a chroot to the specified directory
    immediately before starting the guest execution. This is especially useful
    in combination with ``user=...``.

    ``user=username`` or ``user=uid:gid`` can be used to drop root privileges
    before starting guest execution. QEMU will use the ``setuid`` and ``setgid``
    system calls to switch to the specified identity.  Note that the
    ``user=username`` syntax will also apply the full set of supplementary
    groups for the user, whereas the ``user=uid:gid`` will use only the
    ``gid`` group.
ERST
#endif

DEF("msg", HAS_ARG, QEMU_OPTION_msg,
    "-msg [timestamp[=on|off]][,guest-name=[on|off]]\n"
    "                control error message format\n"
    "                timestamp=on enables timestamps (default: off)\n"
    "                guest-name=on enables guest name prefix but only if\n"
    "                              -name guest option is set (default: off)\n",
    QEMU_ARCH_ALL)
SRST
``-msg [timestamp[=on|off]][,guest-name[=on|off]]``
    Control error message format.

    ``timestamp=on|off``
        Prefix messages with a timestamp. Default is off.

    ``guest-name=on|off``
        Prefix messages with guest name but only if -name guest option is set
        otherwise the option is ignored. Default is off.
ERST

DEFHEADING()

DEFHEADING(Generic object creation:)

DEF("object", HAS_ARG, QEMU_OPTION_object,
    "-object TYPENAME[,PROP1=VALUE1,...]\n"
    "                create a new object of type TYPENAME setting properties\n"
    "                in the order they are specified.  Note that the 'id'\n"
    "                property must be set.  These objects are placed in the\n"
    "                '/objects' path.\n",
    QEMU_ARCH_ALL)
SRST
``-object typename[,prop1=value1,...]``
    Create a new object of type typename setting properties in the order
    they are specified. Note that the 'id' property must be set. These
    objects are placed in the '/objects' path.

    ``-object memory-backend-file,id=id,size=size,mem-path=dir,share=on|off,discard-data=on|off,merge=on|off,dump=on|off,prealloc=on|off,host-nodes=host-nodes,policy=default|preferred|bind|interleave,align=align,offset=offset,readonly=on|off,rom=on|off|auto``
        Creates a memory file backend object, which can be used to back
        the guest RAM with huge pages.

        The ``id`` parameter is a unique ID that will be used to
        reference this memory region in other parameters, e.g. ``-numa``,
        ``-device nvdimm``, etc.

        The ``size`` option provides the size of the memory region, and
        accepts common suffixes, e.g. ``500M``.

        The ``mem-path`` provides the path to either a shared memory or
        huge page filesystem mount.

        The ``share`` boolean option determines whether the memory
        region is marked as private to QEMU, or shared. The latter
        allows a co-operating external process to access the QEMU memory
        region.

        Setting share=on might affect the ability to configure NUMA
        bindings for the memory backend under some circumstances, see
        Documentation/vm/numa\_memory\_policy.txt on the Linux kernel
        source tree for additional details.

        Setting the ``discard-data`` boolean option to on indicates that
        file contents can be destroyed when QEMU exits, to avoid
        unnecessarily flushing data to the backing file. Note that
        ``discard-data`` is only an optimization, and QEMU might not
        discard file contents if it aborts unexpectedly or is terminated
        using SIGKILL.

        The ``merge`` boolean option enables memory merge, also known as
        MADV\_MERGEABLE, so that Kernel Samepage Merging will consider
        the pages for memory deduplication.

        Setting the ``dump`` boolean option to off excludes the memory
        from core dumps. This feature is also known as MADV\_DONTDUMP.

        The ``prealloc`` boolean option enables memory preallocation.

        The ``host-nodes`` option binds the memory range to a list of
        NUMA host nodes.

        The ``policy`` option sets the NUMA policy to one of the
        following values:

        ``default``
            default host policy

        ``preferred``
            prefer the given host node list for allocation

        ``bind``
            restrict memory allocation to the given host node list

        ``interleave``
            interleave memory allocations across the given host node
            list

        The ``align`` option specifies the base address alignment when
        QEMU mmap(2) ``mem-path``, and accepts common suffixes, eg
        ``2M``. Some backend store specified by ``mem-path`` requires an
        alignment different than the default one used by QEMU, eg the
        device DAX /dev/dax0.0 requires 2M alignment rather than 4K. In
        such cases, users can specify the required alignment via this
        option.

        The ``offset`` option specifies the offset into the target file
        that the region starts at. You can use this parameter to back
        multiple regions with a single file.

        The ``pmem`` option specifies whether the backing file specified
        by ``mem-path`` is in host persistent memory that can be
        accessed using the SNIA NVM programming model (e.g. Intel
        NVDIMM). If ``pmem`` is set to 'on', QEMU will take necessary
        operations to guarantee the persistence of its own writes to
        ``mem-path`` (e.g. in vNVDIMM label emulation and live
        migration). Also, we will map the backend-file with MAP\_SYNC
        flag, which ensures the file metadata is in sync for
        ``mem-path`` in case of host crash or a power failure. MAP\_SYNC
        requires support from both the host kernel (since Linux kernel
        4.15) and the filesystem of ``mem-path`` mounted with DAX
        option.

        The ``readonly`` option specifies whether the backing file is opened
        read-only or read-write (default).

        The ``rom`` option specifies whether to create Read Only Memory
        (ROM) that cannot be modified by the VM. Any write attempts to such
        ROM will be denied. Most use cases want proper RAM instead of ROM.
        However, selected use cases, like R/O NVDIMMs, can benefit from
        ROM. If set to ``on``, create ROM; if set to ``off``, create
        writable RAM; if set to ``auto`` (default), the value of the
        ``readonly`` option is used. This option is primarily helpful when
        we want to have writable RAM in configurations that would
        traditionally create ROM before the ``rom`` option was introduced:
        VM templating, where we want to open a file readonly
        (``readonly=on``) and mark the memory to be private for QEMU
        (``share=off``). For this use case, we need writable RAM instead
        of ROM, and want to also set ``rom=off``.

    ``-object memory-backend-ram,id=id,merge=on|off,dump=on|off,share=on|off,prealloc=on|off,size=size,host-nodes=host-nodes,policy=default|preferred|bind|interleave``
        Creates a memory backend object, which can be used to back the
        guest RAM. Memory backend objects offer more control than the
        ``-m`` option that is traditionally used to define guest RAM.
        Please refer to ``memory-backend-file`` for a description of the
        options.

    ``-object memory-backend-memfd,id=id,merge=on|off,dump=on|off,share=on|off,prealloc=on|off,size=size,host-nodes=host-nodes,policy=default|preferred|bind|interleave,seal=on|off,hugetlb=on|off,hugetlbsize=size``
        Creates an anonymous memory file backend object, which allows
        QEMU to share the memory with an external process (e.g. when
        using vhost-user). The memory is allocated with memfd and
        optional sealing. (Linux only)

        The ``seal`` option creates a sealed-file, that will block
        further resizing the memory ('on' by default).

        The ``hugetlb`` option specify the file to be created resides in
        the hugetlbfs filesystem (since Linux 4.14). Used in conjunction
        with the ``hugetlb`` option, the ``hugetlbsize`` option specify
        the hugetlb page size on systems that support multiple hugetlb
        page sizes (it must be a power of 2 value supported by the
        system).

        In some versions of Linux, the ``hugetlb`` option is
        incompatible with the ``seal`` option (requires at least Linux
        4.16).

        Please refer to ``memory-backend-file`` for a description of the
        other options.

        The ``share`` boolean option is on by default with memfd.

    ``-object memory-backend-shm,id=id,merge=on|off,dump=on|off,share=on|off,prealloc=on|off,size=size,host-nodes=host-nodes,policy=default|preferred|bind|interleave``
        Creates a POSIX shared memory backend object, which allows
        QEMU to share the memory with an external process (e.g. when
        using vhost-user).

        ``memory-backend-shm`` is a more portable and less featureful version
        of ``memory-backend-memfd``. It can then be used in any POSIX system,
        especially when memfd is not supported.

        Please refer to ``memory-backend-file`` for a description of the
        options.

        The ``share`` boolean option is on by default with shm. Setting it to
        off will cause a failure during allocation because it is not supported
        by this backend.

    ``-object iommufd,id=id[,fd=fd]``
        Creates an iommufd backend which allows control of DMA mapping
        through the ``/dev/iommu`` device.

        The ``id`` parameter is a unique ID which frontends will use to connect
        with the iommufd backend.

        The ``fd`` parameter is an optional pre-opened file descriptor
        resulting from ``/dev/iommu`` opening. Usually the iommufd is shared
        across all subsystems, bringing the benefit of centralized
        reference counting.

    ``-object tls-creds-anon,id=id,endpoint=endpoint,dir=/path/to/cred/dir,verify-peer=on|off``
        Creates a TLS anonymous credentials object, which can be used to
        provide TLS support on network backends. The ``id`` parameter is
        a unique ID which network backends will use to access the
        credentials. The ``endpoint`` is either ``server`` or ``client``
        depending on whether the QEMU network backend that uses the
        credentials will be acting as a client or as a server. If
        ``verify-peer`` is enabled (the default) then once the handshake
        is completed, the peer credentials will be verified, though this
        is a no-op for anonymous credentials.

        The dir parameter tells QEMU where to find the credential files.
        For server endpoints, this directory may contain a file
        dh-params.pem providing diffie-hellman parameters to use for the
        TLS server. If the file is missing, QEMU will generate a set of
        DH parameters at startup. This is a computationally expensive
        operation that consumes random pool entropy, so it is
        recommended that a persistent set of parameters be generated
        upfront and saved.

    ``-object tls-creds-psk,id=id,endpoint=endpoint,dir=/path/to/keys/dir[,username=username]``
        Creates a TLS Pre-Shared Keys (PSK) credentials object, which
        can be used to provide TLS support on network backends. The
        ``id`` parameter is a unique ID which network backends will use
        to access the credentials. The ``endpoint`` is either ``server``
        or ``client`` depending on whether the QEMU network backend that
        uses the credentials will be acting as a client or as a server.
        For clients only, ``username`` is the username which will be
        sent to the server. If omitted it defaults to "qemu".

        The dir parameter tells QEMU where to find the keys file. It is
        called "dir/keys.psk" and contains "username:key" pairs. This
        file can most easily be created using the GnuTLS ``psktool``
        program.

        For server endpoints, dir may also contain a file dh-params.pem
        providing diffie-hellman parameters to use for the TLS server.
        If the file is missing, QEMU will generate a set of DH
        parameters at startup. This is a computationally expensive
        operation that consumes random pool entropy, so it is
        recommended that a persistent set of parameters be generated up
        front and saved.

    ``-object tls-creds-x509,id=id,endpoint=endpoint,dir=/path/to/cred/dir,priority=priority,verify-peer=on|off,passwordid=id``
        Creates a TLS anonymous credentials object, which can be used to
        provide TLS support on network backends. The ``id`` parameter is
        a unique ID which network backends will use to access the
        credentials. The ``endpoint`` is either ``server`` or ``client``
        depending on whether the QEMU network backend that uses the
        credentials will be acting as a client or as a server. If
        ``verify-peer`` is enabled (the default) then once the handshake
        is completed, the peer credentials will be verified. With x509
        certificates, this implies that the clients must be provided
        with valid client certificates too.

        The dir parameter tells QEMU where to find the credential files.
        For server endpoints, this directory may contain a file
        dh-params.pem providing diffie-hellman parameters to use for the
        TLS server. If the file is missing, QEMU will generate a set of
        DH parameters at startup. This is a computationally expensive
        operation that consumes random pool entropy, so it is
        recommended that a persistent set of parameters be generated
        upfront and saved.

        For x509 certificate credentials the directory will contain
        further files providing the x509 certificates. The certificates
        must be stored in PEM format, in filenames ca-cert.pem,
        ca-crl.pem (optional), server-cert.pem (only servers),
        server-key.pem (only servers), client-cert.pem (only clients),
        and client-key.pem (only clients).

        For the server-key.pem and client-key.pem files which contain
        sensitive private keys, it is possible to use an encrypted
        version by providing the passwordid parameter. This provides the
        ID of a previously created ``secret`` object containing the
        password for decryption.

        The priority parameter allows to override the global default
        priority used by gnutls. This can be useful if the system
        administrator needs to use a weaker set of crypto priorities for
        QEMU without potentially forcing the weakness onto all
        applications. Or conversely if one wants wants a stronger
        default for QEMU than for all other applications, they can do
        this through this parameter. Its format is a gnutls priority
        string as described at
        https://gnutls.org/manual/html_node/Priority-Strings.html.

    ``-object tls-cipher-suites,id=id,priority=priority``
        Creates a TLS cipher suites object, which can be used to control
        the TLS cipher/protocol algorithms that applications are permitted
        to use.

        The ``id`` parameter is a unique ID which frontends will use to
        access the ordered list of permitted TLS cipher suites from the
        host.

        The ``priority`` parameter allows to override the global default
        priority used by gnutls. This can be useful if the system
        administrator needs to use a weaker set of crypto priorities for
        QEMU without potentially forcing the weakness onto all
        applications. Or conversely if one wants wants a stronger
        default for QEMU than for all other applications, they can do
        this through this parameter. Its format is a gnutls priority
        string as described at
        https://gnutls.org/manual/html_node/Priority-Strings.html.

        An example of use of this object is to control UEFI HTTPS Boot.
        The tls-cipher-suites object exposes the ordered list of permitted
        TLS cipher suites from the host side to the guest firmware, via
        fw_cfg. The list is represented as an array of IANA_TLS_CIPHER
        objects. The firmware uses the IANA_TLS_CIPHER array for configuring
        guest-side TLS.

        In the following example, the priority at which the host-side policy
        is retrieved is given by the ``priority`` property.
        Given that QEMU uses GNUTLS, ``priority=@SYSTEM`` may be used to
        refer to /etc/crypto-policies/back-ends/gnutls.config.

        .. parsed-literal::

             # |qemu_system| \\
                 -object tls-cipher-suites,id=mysuite0,priority=@SYSTEM \\
                 -fw_cfg name=etc/edk2/https/ciphers,gen_id=mysuite0

    ``-object secret,id=id,data=string,format=raw|base64[,keyid=secretid,iv=string]``
      \ 
    ``-object secret,id=id,file=filename,format=raw|base64[,keyid=secretid,iv=string]``
        Defines a secret to store a password, encryption key, or some
        other sensitive data. The sensitive data can either be passed
        directly via the data parameter, or indirectly via the file
        parameter. Using the data parameter is insecure unless the
        sensitive data is encrypted.

        The sensitive data can be provided in raw format (the default),
        or base64. When encoded as JSON, the raw format only supports
        valid UTF-8 characters, so base64 is recommended for sending
        binary data. QEMU will convert from which ever format is
        provided to the format it needs internally. eg, an RBD password
        can be provided in raw format, even though it will be base64
        encoded when passed onto the RBD sever.

        For added protection, it is possible to encrypt the data
        associated with a secret using the AES-256-CBC cipher. Use of
        encryption is indicated by providing the keyid and iv
        parameters. The keyid parameter provides the ID of a previously
        defined secret that contains the AES-256 decryption key. This
        key should be 32-bytes long and be base64 encoded. The iv
        parameter provides the random initialization vector used for
        encryption of this particular secret and should be a base64
        encrypted string of the 16-byte IV.

        The simplest (insecure) usage is to provide the secret inline

        .. parsed-literal::

             # |qemu_system| -object secret,id=sec0,data=letmein,format=raw

        The simplest secure usage is to provide the secret via a file

        # printf "letmein" > mypasswd.txt # QEMU\_SYSTEM\_MACRO -object
        secret,id=sec0,file=mypasswd.txt,format=raw

        For greater security, AES-256-CBC should be used. To illustrate
        usage, consider the openssl command line tool which can encrypt
        the data. Note that when encrypting, the plaintext must be
        padded to the cipher block size (32 bytes) using the standard
        PKCS#5/6 compatible padding algorithm.

        First a master key needs to be created in base64 encoding:

        ::

             # openssl rand -base64 32 > key.b64
             # KEY=$(base64 -d key.b64 | hexdump  -v -e '/1 "%02X"')

        Each secret to be encrypted needs to have a random
        initialization vector generated. These do not need to be kept
        secret

        ::

             # openssl rand -base64 16 > iv.b64
             # IV=$(base64 -d iv.b64 | hexdump  -v -e '/1 "%02X"')

        The secret to be defined can now be encrypted, in this case
        we're telling openssl to base64 encode the result, but it could
        be left as raw bytes if desired.

        ::

             # SECRET=$(printf "letmein" |
                        openssl enc -aes-256-cbc -a -K $KEY -iv $IV)

        When launching QEMU, create a master secret pointing to
        ``key.b64`` and specify that to be used to decrypt the user
        password. Pass the contents of ``iv.b64`` to the second secret

        .. parsed-literal::

             # |qemu_system| \\
                 -object secret,id=secmaster0,format=base64,file=key.b64 \\
                 -object secret,id=sec0,keyid=secmaster0,format=base64,\\
                     data=$SECRET,iv=$(<iv.b64)

    ``-object sev-guest,id=id,cbitpos=cbitpos,reduced-phys-bits=val,[sev-device=string,policy=policy,handle=handle,dh-cert-file=file,session-file=file,kernel-hashes=on|off]``
        Create a Secure Encrypted Virtualization (SEV) guest object,
        which can be used to provide the guest memory encryption support
        on AMD processors.

        When memory encryption is enabled, one of the physical address
        bit (aka the C-bit) is utilized to mark if a memory page is
        protected. The ``cbitpos`` is used to provide the C-bit
        position. The C-bit position is Host family dependent hence user
        must provide this value. On EPYC, the value should be 47.

        When memory encryption is enabled, we loose certain bits in
        physical address space. The ``reduced-phys-bits`` is used to
        provide the number of bits we loose in physical address space.
        Similar to C-bit, the value is Host family dependent. On EPYC,
        a guest will lose a maximum of 1 bit, so the value should be 1.

        The ``sev-device`` provides the device file to use for
        communicating with the SEV firmware running inside AMD Secure
        Processor. The default device is '/dev/sev'. If hardware
        supports memory encryption then /dev/sev devices are created by
        CCP driver.

        The ``policy`` provides the guest policy to be enforced by the
        SEV firmware and restrict what configuration and operational
        commands can be performed on this guest by the hypervisor. The
        policy should be provided by the guest owner and is bound to the
        guest and cannot be changed throughout the lifetime of the
        guest. The default is 0.

        If guest ``policy`` allows sharing the key with another SEV
        guest then ``handle`` can be use to provide handle of the guest
        from which to share the key.

        The ``dh-cert-file`` and ``session-file`` provides the guest
        owner's Public Diffie-Hillman key defined in SEV spec. The PDH
        and session parameters are used for establishing a cryptographic
        session with the guest owner to negotiate keys used for
        attestation. The file must be encoded in base64.

        The ``kernel-hashes`` adds the hashes of given kernel/initrd/
        cmdline to a designated guest firmware page for measured Linux
        boot with -kernel. The default is off. (Since 6.2)

        e.g to launch a SEV guest

        .. parsed-literal::

             # |qemu_system_x86| \\
                 ...... \\
                 -object sev-guest,id=sev0,cbitpos=47,reduced-phys-bits=1 \\
                 -machine ...,memory-encryption=sev0 \\
                 .....

    ``-object iothread,id=id,poll-max-ns=poll-max-ns,poll-grow=poll-grow,poll-shrink=poll-shrink,aio-max-batch=aio-max-batch``
        Creates a dedicated event loop thread that devices can be
        assigned to. This is known as an IOThread. By default device
        emulation happens in vCPU threads or the main event loop thread.
        This can become a scalability bottleneck. IOThreads allow device
        emulation and I/O to run on other host CPUs.

        The ``id`` parameter is a unique ID that will be used to
        reference this IOThread from ``-device ...,iothread=id``.
        Multiple devices can be assigned to an IOThread. Note that not
        all devices support an ``iothread`` parameter.

        The ``query-iothreads`` QMP command lists IOThreads and reports
        their thread IDs so that the user can configure host CPU
        pinning/affinity.

        IOThreads use an adaptive polling algorithm to reduce event loop
        latency. Instead of entering a blocking system call to monitor
        file descriptors and then pay the cost of being woken up when an
        event occurs, the polling algorithm spins waiting for events for
        a short time. The algorithm's default parameters are suitable
        for many cases but can be adjusted based on knowledge of the
        workload and/or host device latency.

        The ``poll-max-ns`` parameter is the maximum number of
        nanoseconds to busy wait for events. Polling can be disabled by
        setting this value to 0.

        The ``poll-grow`` parameter is the multiplier used to increase
        the polling time when the algorithm detects it is missing events
        due to not polling long enough.

        The ``poll-shrink`` parameter is the divisor used to decrease
        the polling time when the algorithm detects it is spending too
        long polling without encountering events.

        The ``aio-max-batch`` parameter is the maximum number of requests
        in a batch for the AIO engine, 0 means that the engine will use
        its default.

        The IOThread parameters can be modified at run-time using the
        ``qom-set`` command (where ``iothread1`` is the IOThread's
        ``id``):

        ::

            (qemu) qom-set /objects/iothread1 poll-max-ns 100000
ERST


HXCOMM This is the last statement. Insert new options before this line!

#undef DEF
#undef DEFHEADING
#undef ARCHHEADING
