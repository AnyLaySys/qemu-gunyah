project('qemu', ['c'], meson_version: '>=1.5.0',
        default_options: ['warning_level=1', 'c_std=gnu11', 'cpp_std=gnu++11', 'b_colorout=auto',
                          'b_staticpic=false', 'stdsplit=false', 'optimization=2', 'b_pie=true'],
        version: files('VERSION'))

meson.add_devenv({ 'MESON_BUILD_ROOT' : meson.project_build_root() })

add_test_setup('quick', exclude_suites: ['slow', 'thorough'], is_default: true,
               env: ['RUST_BACKTRACE=1'])
add_test_setup('slow', exclude_suites: ['thorough'],
               env: ['G_TEST_SLOW=1', 'SPEED=slow', 'RUST_BACKTRACE=1'])
add_test_setup('thorough',
               env: ['G_TEST_SLOW=1', 'SPEED=thorough', 'RUST_BACKTRACE=1'])

meson.add_postconf_script(find_program('scripts/symlink-install-tree.py'))


not_found = dependency('', required: false)
keyval = import('keyval')
rust = import('rust')
ss = import('sourceset')
fs = import('fs')

host_os = host_machine.system()
config_host = keyval.load(meson.current_build_dir() / 'config-host.mak')

tmpdir = meson.current_build_dir() / 'meson-private/temp'

if get_option('qemu_suffix').startswith('/')
  error('qemu_suffix cannot start with a /')
endif

qemu_confdir = get_option('sysconfdir') / get_option('qemu_suffix')
qemu_datadir = get_option('datadir') / get_option('qemu_suffix')
qemu_docdir = get_option('docdir') / get_option('qemu_suffix')
qemu_moddir = get_option('libdir') / get_option('qemu_suffix')

genh = []
qapi_trace_events = []

bsd_oses = ['gnu/kfreebsd', 'freebsd', 'netbsd', 'openbsd', 'dragonfly', 'darwin']
supported_oses = ['windows', 'freebsd', 'netbsd', 'openbsd', 'darwin', 'sunos', 'linux']
supported_cpus = ['ppc', 'ppc64', 's390x', 'riscv32', 'riscv64', 'x86', 'x86_64',
  'arm', 'aarch64', 'loongarch64', 'mips', 'mips64', 'sparc64']

cpu = host_machine.cpu_family()

target_dirs = config_host['TARGET_DIRS'].split()

have_linux_user = false
have_bsd_user = false
have_system = false
foreach target : target_dirs
  have_linux_user = have_linux_user or target.endswith('linux-user')
  have_bsd_user = have_bsd_user or target.endswith('bsd-user')
  have_system = have_system or target.endswith('-softmmu')
endforeach
have_user = have_linux_user or have_bsd_user


sh = find_program('sh')
python = import('python').find_installation()

cc = meson.get_compiler('c')
all_languages = ['c']
if host_os == 'windows' and add_languages('cpp', required: false, native: false)
  all_languages += ['cpp']
  cxx = meson.get_compiler('cpp')
endif
if host_os == 'darwin' and \
   add_languages('objc', required: true, native: false)
  all_languages += ['objc']
  objc = meson.get_compiler('objc')
endif

have_rust = add_languages('rust', native: false,
    required: get_option('rust').disable_auto_if(not have_system))
have_rust = have_rust and add_languages('rust', native: true,
    required: get_option('rust').disable_auto_if(not have_system))
if have_rust
  rustc = meson.get_compiler('rust')
  if rustc.version().version_compare('<1.63.0')
    if get_option('rust').enabled()
      error('rustc version ' + rustc.version() + ' is unsupported. Please upgrade to at least 1.63.0')
    else
      warning('rustc version ' + rustc.version() + ' is unsupported, disabling Rust compilation.')
      message('Please upgrade to at least 1.63.0 to use Rust.')
      have_rust = false
    endif
  endif
endif

if have_rust
  bindgen = find_program('bindgen', required: get_option('rust'))
  if not bindgen.found() or bindgen.version().version_compare('<0.60.0')
    if get_option('rust').enabled()
      error('bindgen version ' + bindgen.version() + ' is unsupported. You can install a new version with "cargo install bindgen-cli"')
    else
      if bindgen.found()
        warning('bindgen version ' + bindgen.version() + ' is unsupported, disabling Rust compilation.')
      else
        warning('bindgen not found, disabling Rust compilation.')
      endif
      message('To use Rust you can install a new version with "cargo install bindgen-cli"')
      have_rust = false
    endif
  endif
endif

if have_rust
  rustc_args = [find_program('scripts/rust/rustc_args.py'),
    '--rustc-version', rustc.version(),
    '--workspace', meson.project_source_root() / 'rust']
  if get_option('strict_rust_lints')
    rustc_args += ['--strict-lints']
  endif

  rustfmt = find_program('rustfmt', required: false)

  rustc_lint_args = run_command(rustc_args, '--lints',
     capture: true, check: true).stdout().strip().splitlines()

  add_project_arguments(rustc_lint_args +
      ['--cfg', 'MESON', '-C', 'default-linker-libraries'],
      native: false, language: 'rust')
  add_project_arguments(rustc_lint_args + ['--cfg', 'MESON'],
      native: true, language: 'rust')
endif

dtrace = not_found
stap = not_found
if 'dtrace' in get_option('trace_backends')
  dtrace = find_program('dtrace', required: true)
  stap = find_program('stap', required: false)
  if stap.found()
    add_global_arguments('-DSTAP_SDT_V2',
                         native: false, language: all_languages)
  endif
endif

if get_option('iasl') == ''
  iasl = find_program('iasl', required: false)
else
  iasl = find_program(get_option('iasl'), required: true)
endif

edk2_targets = [ 'arm-softmmu', 'aarch64-softmmu', 'i386-softmmu', 'x86_64-softmmu', 'riscv64-softmmu', 'loongarch64-softmmu' ]
unpack_edk2_blobs = false
foreach target : edk2_targets
  if target in target_dirs
    bzip2 = find_program('bzip2', required: get_option('install_blobs'))
    unpack_edk2_blobs = bzip2.found()
    break
  endif
endforeach


if get_option('fuzzing') and get_option('fuzzing_engine') == '' and \
    not cc.links('''
          #include <stdint.h>
          #include <sys/types.h>
          int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size);
          int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) { return 0; }
        ''',
        args: ['-Werror', '-fsanitize=fuzzer'])
  error('Your compiler does not support -fsanitize=fuzzer')
endif

if 'ftrace' in get_option('trace_backends') and host_os != 'linux'
  error('ftrace is supported only on Linux')
endif
if 'syslog' in get_option('trace_backends') and not cc.compiles('''
    #include <syslog.h>
    int main(void) {
        openlog("qemu", LOG_PID, LOG_DAEMON);
        syslog(LOG_INFO, "configure");
        return 0;
    }''')
  error('syslog is not supported on this system')
endif

multiprocess_allowed = get_option('multiprocess') \
  .require(host_os == 'linux', error_message: 'Multiprocess QEMU is supported only on Linux') \
  .allowed()

have_tools = get_option('tools') \
  .disable_auto_if(not have_system) \
  .allowed()
have_ga = get_option('guest_agent') \
  .disable_auto_if(not have_system and not have_tools) \
  .require(host_os in ['sunos', 'linux', 'windows', 'freebsd', 'netbsd', 'openbsd'],
           error_message: 'unsupported OS for QEMU guest agent') \
  .allowed()
have_block = have_system or have_tools

enable_modules = get_option('modules') \
  .require(host_os != 'windows',
           error_message: 'Modules are not available for Windows') \
  .require(not get_option('prefer_static'),
           error_message: 'Modules are incompatible with static linking') \
  .allowed()


if cpu not in supported_cpus
  host_arch = 'unknown'
elif cpu == 'x86'
  host_arch = 'i386'
elif cpu == 'mips64'
  host_arch = 'mips'
elif cpu in ['riscv32', 'riscv64']
  host_arch = 'riscv'
else
  host_arch = cpu
endif

accelerator_targets = {}

if cpu == 'aarch64'
  accelerator_targets += {
    'CONFIG_GH': ['aarch64-softmmu']
  }
endif


foreach lang : all_languages
  compiler = meson.get_compiler(lang)
  if compiler.get_id() == 'gcc' and compiler.version().version_compare('>=7.4')
  elif compiler.get_id() == 'clang' and compiler.compiles('''
      #ifdef __apple_build_version__
      # if __clang_major__ < 15 || (__clang_major__ == 15 && __clang_minor__ < 0)
      #  error You need at least XCode Clang v15.0 to compile QEMU
      # endif
      #else
      # if __clang_major__ < 10 || (__clang_major__ == 10 && __clang_minor__ < 0)
      #  error You need at least Clang v10.0 to compile QEMU
      # endif
      #endif''')
  else
    error('You either need GCC v7.4 or Clang v10.0 (or XCode Clang v15.0) to compile QEMU')
  endif
endforeach


qemu_common_flags = [
  '-D_GNU_SOURCE', '-D_FILE_OFFSET_BITS=64', '-D_LARGEFILE_SOURCE',
  '-fno-strict-aliasing', '-fno-common', '-fwrapv' ]
qemu_cflags = []
qemu_ldflags = []
qemu_ldflags += ['-llog']

if host_os == 'darwin'
  if compiler.get_id() == 'gcc'
    qemu_common_flags += '-DOS_OBJECT_USE_OBJC=0'
  endif
elif host_os == 'sunos'
  qemu_common_flags += '-D_XOPEN_SOURCE=600'
  qemu_common_flags += '-D__EXTENSIONS__'
elif host_os == 'haiku'
  qemu_common_flags += ['-DB_USE_POSITIVE_POSIX_ERRORS', '-D_BSD_SOURCE', '-fPIC']
elif host_os == 'windows'
  if compiler.get_id() == 'clang' and compiler.get_linker_id() != 'ld.lld'
    error('On windows, you need to use lld with clang - use msys2 clang64/clangarm64 env')
  endif
endif


qemu_isa_flags = []

if host_arch == 'i386' and not cc.links('''
  static int sfaa(int *ptr)
  {
    return __sync_fetch_and_and(ptr, 0);
  }

  int main(void)
  {
    int val = 42;
    val = __sync_val_compare_and_swap(&val, 0, 1);
    sfaa(&val);
    return val;
  }''')
  qemu_isa_flags += ['-march=i486']
endif

if host_arch in ['i386', 'x86_64']
  if get_option('x86_version') == '0' and host_arch == 'x86_64'
    error('x86_64-v1 required for x86-64 hosts')
  endif

  if get_option('x86_version') >= '1'
    if host_arch == 'i386'
      qemu_common_flags = ['-mfpmath=sse'] + qemu_common_flags
    else
      qemu_isa_flags += ['-mcx16']
    endif
  endif
  if get_option('x86_version') >= '2'
    qemu_isa_flags += ['-mpopcnt']
    qemu_isa_flags += cc.get_supported_arguments('-mneeded')
  endif
  if get_option('x86_version') >= '3'
    qemu_isa_flags += ['-mmovbe', '-mabm', '-mbmi', '-mbmi2', '-mfma', '-mf16c']
  endif

  if get_option('x86_version') == '1'
    qemu_isa_flags += ['-msse2']
  elif get_option('x86_version') == '2'
    qemu_isa_flags += ['-msse4.2']
  elif get_option('x86_version') == '3'
    qemu_isa_flags += ['-mavx2']
  elif get_option('x86_version') == '4'
    qemu_isa_flags += ['-mavx512f', '-mavx512bw', '-mavx512cd', '-mavx512dq', '-mavx512vl']
  endif
endif

qemu_common_flags = qemu_isa_flags + qemu_common_flags

if get_option('prefer_static')
  qemu_ldflags += get_option('b_pie') ? '-static-pie' : '-static'
endif

if not get_option('b_pie')
  qemu_common_flags += cc.get_supported_arguments('-fno-pie', '-no-pie')
endif

if not get_option('stack_protector').disabled()
  stack_protector_probe = '''
    int main(int argc, char *argv[])
    {
      char arr[64], *p = arr, *c = argv[argc - 1];
      while (*c) {
          *p++ = *c++;
      }
      return 0;
    }'''
  have_stack_protector = false
  foreach arg : ['-fstack-protector-strong', '-fstack-protector-all']
    if cc.compiles(stack_protector_probe, args: ['-Werror', arg]) and \
       cc.links(stack_protector_probe, args: ['-Werror', arg])
      have_stack_protector = true
      qemu_cflags += arg
      qemu_ldflags += arg
      break
    endif
  endforeach
  get_option('stack_protector') \
    .require(have_stack_protector, error_message: 'Stack protector not supported')
endif

coroutine_backend = get_option('coroutine_backend')
ucontext_probe = '''
  #include <ucontext.h>
  #ifdef __stub_makecontext
  #error Ignoring glibc stub makecontext which will always fail
  #endif
  int main(void) { makecontext(0, 0, 0); return 0; }'''

supported_backends = []
if host_os == 'windows'
  supported_backends += ['windows']
else
  supported_backends += ['ucontext']
  supported_backends += ['sigaltstack']
endif

if coroutine_backend == 'auto'
  coroutine_backend = supported_backends[0]
elif coroutine_backend not in supported_backends
  error('"@0@" backend requested but not available'.format(coroutine_backend))
endif

safe_stack_probe = '''
  int main(void)
  {
  #if defined(__has_feature)
  #if __has_feature(safe_stack)
  #error SafeStack Enabled
  #endif
  #endif
      return 0;
  }'''
if get_option('safe_stack') != not cc.compiles(safe_stack_probe)
  safe_stack_arg = get_option('safe_stack') ? '-fsanitize=safe-stack' : '-fno-sanitize=safe-stack'
  if get_option('safe_stack') != not cc.compiles(safe_stack_probe, args: safe_stack_arg)
    error(get_option('safe_stack') \
          ? 'SafeStack not supported by your compiler' \
          : 'Cannot disable SafeStack')
  endif
  qemu_cflags += safe_stack_arg
  qemu_ldflags += safe_stack_arg
endif
if get_option('safe_stack') and coroutine_backend != 'ucontext'
  error('SafeStack is only supported with the ucontext coroutine backend')
endif

if get_option('asan')
  if cc.has_argument('-fsanitize=address')
    qemu_cflags = ['-fsanitize=address'] + qemu_cflags
    qemu_ldflags = ['-fsanitize=address'] + qemu_ldflags
  else
    error('Your compiler does not support -fsanitize=address')
  endif
endif

if get_option('ubsan')
  if cc.links('int main(int argc, char **argv) { return argc + 1; }',
              args: [qemu_ldflags, '-fsanitize=undefined'])
    qemu_cflags += ['-fsanitize=undefined']
    qemu_ldflags += ['-fsanitize=undefined']

    if cc.has_argument('-fno-sanitize=function')
      qemu_cflags += ['-fno-sanitize=function']
    endif
  else
    error('Your compiler does not support -fsanitize=undefined')
  endif
endif

if get_option('tsan')
  if get_option('asan') or get_option('ubsan')
    error('TSAN is not supported with other sanitizers')
  endif
  if not cc.has_function('__tsan_create_fiber',
                         args: '-fsanitize=thread',
                         prefix: '#include <sanitizer/tsan_interface.h>')
    error('Cannot enable TSAN due to missing fiber annotation interface')
  endif
  tsan_warn_suppress = []
  if cc.has_argument('-Wno-tsan')
    tsan_warn_suppress = ['-Wno-tsan']
  endif
  qemu_cflags = ['-fsanitize=thread'] + tsan_warn_suppress + qemu_cflags
  qemu_ldflags = ['-fsanitize=thread'] + qemu_ldflags
endif

qemu_ldflags += cc.get_supported_link_arguments('-Wl,-z,relro', '-Wl,-z,now')

if host_os == 'windows'
  qemu_ldflags += cc.get_supported_link_arguments('-Wl,--no-seh', '-Wl,--nxcompat')
  qemu_ldflags += cc.get_supported_link_arguments('-Wl,--dynamicbase', '-Wl,--high-entropy-va')
endif

if get_option('fuzzing')
  configure_file(output: 'instrumentation-filter',
                 input: 'scripts/oss-fuzz/instrumentation-filter-template',
                 copy: true)

  if cc.compiles('int main () { return 0; }',
                  name: '-fsanitize-coverage-allowlist=/dev/null',
                 args: ['-fsanitize-coverage-allowlist=/dev/null',
                        '-fsanitize-coverage=trace-pc'] )
    qemu_common_flags += ['-fsanitize-coverage-allowlist=instrumentation-filter']
  endif

  if get_option('fuzzing_engine') == ''
    qemu_common_flags += ['-fsanitize=fuzzer-no-link']
    qemu_ldflags += ['-fsanitize=fuzzer-no-link']
    fuzz_exe_ldflags = ['-fsanitize=fuzzer']
  else
    fuzz_exe_ldflags = get_option('fuzzing_engine').split()
  endif
endif

if get_option('cfi')
  cfi_flags=[]
  if not get_option('b_lto')
    error('Selected Control-Flow Integrity but LTO is disabled')
  endif
  if enable_modules
    error('Selected Control-Flow Integrity is not compatible with modules')
  endif
  if cc.compiles('int main () { return 0; }', name: '-fsanitize=cfi-icall',
                 args: ['-flto', '-fsanitize=cfi-icall'] )
    cfi_flags += '-fsanitize=cfi-icall'
  else
    error('-fsanitize=cfi-icall is not supported by the compiler')
  endif
  if cc.compiles('int main () { return 0; }',
                 name: '-fsanitize-cfi-icall-generalize-pointers',
                 args: ['-flto', '-fsanitize=cfi-icall',
                        '-fsanitize-cfi-icall-generalize-pointers'] )
    cfi_flags += '-fsanitize-cfi-icall-generalize-pointers'
  else
    error('-fsanitize-cfi-icall-generalize-pointers is not supported by the compiler')
  endif
  if get_option('cfi_debug')
    if cc.compiles('int main () { return 0; }',
                   name: '-fno-sanitize-trap=cfi-icall',
                   args: ['-flto', '-fsanitize=cfi-icall',
                          '-fno-sanitize-trap=cfi-icall'] )
      cfi_flags += '-fno-sanitize-trap=cfi-icall'
    else
      error('-fno-sanitize-trap=cfi-icall is not supported by the compiler')
    endif
  endif
  add_global_arguments(cfi_flags, native: false, language: all_languages)
  add_global_link_arguments(cfi_flags, native: false, language: all_languages)
endif


hardening_flags = [
    '-ftrivial-auto-var-init=zero',
]

if host_os != 'openbsd' and \
   cc.compiles('extern struct { void (*cb)(void); } s; void f(void) { s.cb(); }',
               name: '-fzero-call-used-regs=used-gpr',
               args: ['-O2', '-fzero-call-used-regs=used-gpr'])
    hardening_flags += '-fzero-call-used-regs=used-gpr'
endif

qemu_common_flags += cc.get_supported_arguments(hardening_flags)

add_global_arguments(qemu_common_flags, native: false, language: all_languages)
add_global_link_arguments(qemu_ldflags, native: false, language: all_languages)

warn_flags = [
  '-Wempty-body',
  '-Wendif-labels',
  '-Wexpansion-to-defined',
  '-Wformat-security',
  '-Wformat-y2k',
  '-Wignored-qualifiers',
  '-Wimplicit-fallthrough=2',
  '-Winit-self',
  '-Wmissing-format-attribute',
  '-Wmissing-prototypes',
  '-Wnested-externs',
  '-Wold-style-declaration',
  '-Wold-style-definition',
  '-Wredundant-decls',
  '-Wshadow=local',
  '-Wstrict-prototypes',
  '-Wtype-limits',
  '-Wundef',
  '-Wvla',
  '-Wwrite-strings',

  '-Wno-gnu-variable-sized-type-not-at-end',
  '-Wno-initializer-overrides',
  '-Wno-missing-include-dirs',
  '-Wno-psabi',
  '-Wno-shift-negative-value',
  '-Wno-string-plus-int',
  '-Wno-tautological-type-limit-compare',
  '-Wno-typedef-redefinition',
]

if host_os != 'darwin'
  tsa_has_cleanup = cc.compiles('''
    struct __attribute__((capability("mutex"))) mutex {};
    void lock(struct mutex *m) __attribute__((acquire_capability(m)));
    void unlock(struct mutex *m) __attribute__((release_capability(m)));

    void test(void) {
      struct mutex __attribute__((cleanup(unlock))) m;
      lock(&m);
    }
  ''', args: ['-Wthread-safety', '-Werror'])
  if tsa_has_cleanup
    warn_flags += ['-Wthread-safety']
  endif
endif

qemu_cxxflags = []
if 'cpp' in all_languages
  qemu_cxxflags = ['-D__STDC_LIMIT_MACROS', '-D__STDC_CONSTANT_MACROS', '-D__STDC_FORMAT_MACROS'] + qemu_cflags
endif

add_project_arguments(qemu_cflags, native: false, language: 'c')
add_project_arguments(cc.get_supported_arguments(warn_flags), native: false, language: 'c')
if 'cpp' in all_languages
  add_project_arguments(qemu_cxxflags, native: false, language: 'cpp')
  add_project_arguments(cxx.get_supported_arguments(warn_flags), native: false, language: 'cpp')
endif
if 'objc' in all_languages
  add_project_arguments(objc.get_supported_arguments(warn_flags), native: false, language: 'objc')
endif
if host_os == 'linux'
  add_project_arguments('-isystem', meson.current_source_dir() / 'linux-headers',
                        '-isystem', 'linux-headers',
                        language: all_languages)
endif

add_project_arguments('-iquote', '.',
                      '-iquote', meson.current_source_dir(),
                      '-iquote', meson.current_source_dir() / 'include',
                      language: all_languages)

host_include = meson.current_source_dir() / 'host/include/'
if fs.is_dir(host_include / host_arch)
  add_project_arguments('-iquote', host_include / host_arch,
                        language: all_languages)
endif
add_project_arguments('-iquote', host_include / 'generic',
                      language: all_languages)

sparse = find_program('cgcc', required: get_option('sparse'))
if sparse.found()
  run_target('sparse',
             command: [find_program('scripts/check_sparse.py'),
                       'compile_commands.json', sparse.full_path(), '-Wbitwise',
                       '-Wno-transparent-union', '-Wno-old-initializer',
                       '-Wno-non-pointer-null'])
endif


libm = cc.find_library('m', required: false)
threads = dependency('threads')
util = cc.find_library('util', required: false)
winmm = []
socket = []
version_res = []
coref = []
iokit = []
pvg = not_found
emulator_link_args = []
midl = not_found
widl = not_found
pathcch = not_found
host_dsosuf = '.so'
if host_os == 'windows'
  midl = find_program('midl', required: false)
  widl = find_program('widl', required: false)
  pathcch = cc.find_library('pathcch')
  socket = cc.find_library('ws2_32')
  winmm = cc.find_library('winmm')

  win = import('windows')
  version_res = win.compile_resources('version.rc',
                                      depend_files: files('pc-bios/qemu-nsis.ico'),
                                      include_directories: include_directories('.'))
  host_dsosuf = '.dll'
elif host_os == 'darwin'
  coref = dependency('appleframeworks', modules: 'CoreFoundation')
  iokit = dependency('appleframeworks', modules: 'IOKit', required: false)
  host_dsosuf = '.dylib'
  pvg = dependency('appleframeworks', modules: ['ParavirtualizedGraphics', 'Metal'],
                   required: get_option('pvg'))
elif host_os == 'sunos'
  socket = [cc.find_library('socket'),
            cc.find_library('nsl'),
            cc.find_library('resolv')]
elif host_os == 'haiku'
  socket = [cc.find_library('posix_error_mapper'),
            cc.find_library('network'),
            cc.find_library('bsd')]
elif host_os == 'openbsd'
  if get_option('tcg').allowed() and target_dirs.length() > 0
    emulator_link_args = cc.get_supported_link_arguments('-Wl,-z,wxneeded')
  endif
endif


accelerators = []
if get_option('gunyah').allowed() and host_os == 'linux'
  accelerators += 'CONFIG_GH'
endif
tcg_arch = host_arch
if get_option('tcg').allowed()
  if host_arch == 'unknown'
    if not get_option('tcg_interpreter')
      error('Unsupported CPU @0@, try --enable-tcg-interpreter'.format(cpu))
    endif
  elif get_option('tcg_interpreter')
    warning('Use of the TCG interpreter is not recommended on this host')
    warning('architecture. There is a native TCG execution backend available')
    warning('which provides substantially better performance and reliability.')
    warning('It is strongly recommended to remove the --enable-tcg-interpreter')
    warning('configuration option on this architecture to use the native')
    warning('backend.')
  endif
  if get_option('tcg_interpreter')
    tcg_arch = 'tci'
  elif host_arch == 'x86_64'
    tcg_arch = 'i386'
  elif host_arch == 'ppc64'
    tcg_arch = 'ppc'
  endif
  add_project_arguments('-iquote', meson.current_source_dir() / 'tcg' / tcg_arch,
                        language: all_languages)

  accelerators += 'CONFIG_TCG'
endif

if 'CONFIG_GH' not in accelerators and get_option('gunyah').enabled()
  error('Gunyah not available on this platform')
endif
xen = not_found
if get_option('xen').enabled()
  error('Xen support was removed from this trimmed build')
endif
have_xen_pci_passthrough = get_option('xen_pci_passthrough') \
  .require(xen.found(),
           error_message: 'Xen PCI passthrough requested but Xen not enabled') \
  .require(host_os == 'linux',
           error_message: 'Xen PCI passthrough not available on this platform') \
  .require(cpu == 'x86'  or cpu == 'x86_64',
           error_message: 'Xen PCI passthrough not available on this platform') \
  .allowed()


glib_req_ver = '>=2.66.0'
glib_pc = dependency('glib-2.0', version: glib_req_ver, required: true,
                    method: 'pkg-config')
glib_cflags = []
if enable_modules
  gmodule = dependency('gmodule-export-2.0', version: glib_req_ver, required: true,
                       method: 'pkg-config')
elif get_option('plugins')
  gmodule = dependency('gmodule-no-export-2.0', version: glib_req_ver, required: true,
                       method: 'pkg-config')
else
  gmodule = not_found
endif

if host_os == 'windows' and get_option('prefer_static')
  glib_cflags += ['-DGLIB_STATIC_COMPILATION']
endif


if not cc.compiles('''
  #include <glib.h>
  #include <unistd.h>

  #define QEMU_BUILD_BUG_ON(x) \
  typedef char qemu_build_bug_on[(x)?-1:1] __attribute__((unused));

  int main(void) {
     QEMU_BUILD_BUG_ON(sizeof(size_t) != GLIB_SIZEOF_SIZE_T);
     return 0;
  }''', dependencies: glib_pc, args: glib_cflags)
  error('''sizeof(size_t) doesn't match GLIB_SIZEOF_SIZE_T.
        You probably need to set PKG_CONFIG_LIBDIR" to point
        to the right pkg-config files for your build target.''')
endif

glib = declare_dependency(dependencies: [glib_pc, gmodule],
                          compile_args: glib_cflags,
                          version: glib_pc.version())

glib_has_gslice = glib.version().version_compare('<2.75.3')
glib_has_aligned_alloc = glib.version().version_compare('>=2.72.0')

meson.override_dependency('glib-2.0', glib)

add_project_dependencies(glib.partial_dependency(compile_args: true, includes: true),
                         native: false, language: all_languages)

gio = not_found
gdbus_codegen = not_found
gdbus_codegen_error = '@0@ requires gdbus-codegen, please install libgio'
if not get_option('gio').auto() or have_system
  gio = dependency('gio-2.0', required: get_option('gio'),
                   method: 'pkg-config')
  if gio.found() and not cc.links('''
    #include <gio/gio.h>
    int main(void)
    {
      g_dbus_proxy_new_sync(0, 0, 0, 0, 0, 0, 0, 0);
      return 0;
    }''', dependencies: [glib, gio])
    if get_option('gio').enabled()
      error('The installed libgio is broken for static linking')
    endif
    gio = not_found
  endif
  if gio.found()
    gdbus_codegen = find_program('gdbus-codegen',
                                 required: get_option('gio'))
    gio_unix = dependency('gio-unix-2.0', required: get_option('gio'),
                          method: 'pkg-config')
    gio = declare_dependency(dependencies: [gio, gio_unix],
                             version: gio.version())
  endif
endif
if gdbus_codegen.found() and get_option('cfi')
  gdbus_codegen = not_found
  gdbus_codegen_error = '@0@ uses gdbus-codegen, which does not support control flow integrity'
endif

xml_pp = find_program('scripts/xml-preprocess.py')

lttng = not_found
if 'ust' in get_option('trace_backends')
  lttng = dependency('lttng-ust', required: true, version: '>= 2.1',
                     method: 'pkg-config')
endif
pixman = not_found
if not get_option('pixman').auto() or have_system or have_tools
  pixman = dependency('pixman-1', required: get_option('pixman'), version:'>=0.21.8',
                      method: 'pkg-config')
endif

zlib = dependency('zlib', required: true)

linux_io_uring_test = '''
  #include <liburing.h>
  #include <linux/errqueue.h>

  int main(void) { return 0; }'''

linux_io_uring = not_found
if not get_option('linux_io_uring').auto() or have_block
  linux_io_uring = dependency('liburing', version: '>=0.3',
                              required: get_option('linux_io_uring'),
                              method: 'pkg-config')
  if not cc.links(linux_io_uring_test)
    linux_io_uring = not_found
  endif
endif

libattr_test = '''
  #include <stddef.h>
  #include <sys/types.h>
  #ifdef CONFIG_LIBATTR
  #include <attr/xattr.h>
  #else
  #include <sys/xattr.h>
  #endif
  int main(void) { getxattr(NULL, NULL, NULL, 0); setxattr(NULL, NULL, NULL, 0, 0); return 0; }'''

libattr = not_found
have_old_libattr = false
if get_option('attr').allowed()
  if cc.links(libattr_test)
    libattr = declare_dependency()
  else
    libattr = cc.find_library('attr', has_headers: ['attr/xattr.h'],
                              required: get_option('attr'))
    if libattr.found() and not \
      cc.links(libattr_test, dependencies: libattr, args: '-DCONFIG_LIBATTR')
      libattr = not_found
      if get_option('attr').enabled()
        error('could not link libattr')
      else
        warning('could not link libattr, disabling')
      endif
    else
      have_old_libattr = libattr.found()
    endif
  endif
endif

cocoa = dependency('appleframeworks',
                   modules: ['Cocoa', 'CoreVideo', 'QuartzCore'],
                   required: get_option('cocoa'))

seccomp = not_found
seccomp_has_sysrawrc = false
if not get_option('seccomp').auto() or have_system or have_tools
  seccomp = dependency('libseccomp', version: '>=2.3.0',
                       required: get_option('seccomp'),
                       method: 'pkg-config')
  if seccomp.found()
    seccomp_has_sysrawrc = cc.has_header_symbol('seccomp.h',
                                                'SCMP_FLTATR_API_SYSRAWRC',
                                                dependencies: seccomp)
  endif
endif

libcap_ng = not_found
if not get_option('cap_ng').auto() or have_system or have_tools
  libcap_ng = cc.find_library('cap-ng', has_headers: ['cap-ng.h'],
                              required: get_option('cap_ng'))
endif
if libcap_ng.found() and not cc.links('''
   #include <cap-ng.h>
   int main(void)
   {
     capng_capability_to_name(CAPNG_EFFECTIVE);
     return 0;
   }''', dependencies: libcap_ng)
  libcap_ng = not_found
  if get_option('cap_ng').enabled()
    error('could not link libcap-ng')
  else
    warning('could not link libcap-ng, disabling')
  endif
endif

if get_option('xkbcommon').auto() and not have_system and not have_tools
  xkbcommon = not_found
else
  xkbcommon = dependency('xkbcommon', required: get_option('xkbcommon'),
                         method: 'pkg-config')
endif

vde = not_found
if not get_option('vde').auto() or have_system or have_tools
  vde = cc.find_library('vdeplug', has_headers: ['libvdeplug.h'],
                           required: get_option('vde'))
endif
if vde.found() and not cc.links('''
   #include <libvdeplug.h>
   int main(void)
   {
     struct vde_open_args a = {0, 0, 0};
     char s[] = "";
     vde_open(s, s, &a);
     return 0;
   }''', dependencies: vde)
  vde = not_found
  if get_option('cap_ng').enabled()
    error('could not link libvdeplug')
  else
    warning('could not link libvdeplug, disabling')
  endif
endif

pulse = not_found
if not get_option('pa').auto() or (host_os == 'linux' and have_system)
  pulse = dependency('libpulse', required: get_option('pa'),
                     method: 'pkg-config')
endif
alsa = not_found
if not get_option('alsa').auto() or (host_os == 'linux' and have_system)
  alsa = dependency('alsa', required: get_option('alsa'),
                    method: 'pkg-config')
endif
jack = not_found
if not get_option('jack').auto() or have_system
  jack = dependency('jack', required: get_option('jack'),
                    method: 'pkg-config')
endif
pipewire = not_found
if not get_option('pipewire').auto() or (host_os == 'linux' and have_system)
  pipewire = dependency('libpipewire-0.3', version: '>=0.3.60',
                    required: get_option('pipewire'),
                    method: 'pkg-config')
endif
sndio = not_found
if not get_option('sndio').auto() or have_system
  sndio = dependency('sndio', required: get_option('sndio'),
                    method: 'pkg-config')
endif

aaudio = not_found
if host_os == 'linux' and have_system
  aaudio = cc.find_library('aaudio', required: false)
endif

spice_protocol = not_found
if not get_option('spice_protocol').auto() or have_system
  spice_protocol = dependency('spice-protocol', version: '>=0.14.0',
                              required: get_option('spice_protocol'),
                              method: 'pkg-config')
endif
spice = not_found
if get_option('spice') \
             .disable_auto_if(not have_system) \
             .require(pixman.found(),
                      error_message: 'cannot enable SPICE if pixman is not available') \
             .allowed()
  spice = dependency('spice-server', version: '>=0.14.0',
                     required: get_option('spice'),
                     method: 'pkg-config')
endif
spice_headers = spice.partial_dependency(compile_args: true, includes: true)

rt = cc.find_library('rt', required: false)

zstd = not_found
if not get_option('zstd').auto() or have_block
  zstd = dependency('libzstd', version: '>=1.4.0',
                    required: get_option('zstd'),
                    method: 'pkg-config')
endif
qpl = not_found
if not get_option('qpl').auto() or have_system
  qpl = dependency('qpl', version: '>=1.5.0',
                    required: get_option('qpl'),
                    method: 'pkg-config')
endif
uadk = not_found
if not get_option('uadk').auto() or have_system
  libwd = dependency('libwd', version: '>=2.6',
                      required: get_option('uadk'),
                      method: 'pkg-config')
  libwd_comp = dependency('libwd_comp', version: '>=2.6',
                           required: get_option('uadk'),
                           method: 'pkg-config')
  if libwd.found() and libwd_comp.found()
     uadk = declare_dependency(dependencies: [libwd, libwd_comp])
  endif
endif

qatzip = not_found
if not get_option('qatzip').auto() or have_system
  qatzip = dependency('qatzip', version: '>=1.1.2',
                      required: get_option('qatzip'),
                      method: 'pkg-config')
endif

virgl = not_found

if not get_option('virglrenderer').auto() or have_system
  virgl = dependency('virglrenderer',
                     method: 'pkg-config',
                     required: get_option('virglrenderer'))
endif
blkio = not_found
if not get_option('blkio').auto() or have_block
  blkio = dependency('blkio',
                     method: 'pkg-config',
                     required: get_option('blkio'))
endif
curl = not_found
if not get_option('curl').auto() or have_block
  curl = dependency('libcurl', version: '>=7.29.0',
                    method: 'pkg-config',
                    required: get_option('curl'))
endif
libudev = not_found
if host_os == 'linux' and (have_system or have_tools)
  libudev = dependency('libudev',
                       method: 'pkg-config',
                       required: get_option('libudev'))
endif

iconv = not_found
curses = not_found
if have_system and get_option('curses').allowed()
  curses_test = '''
    #ifdef __APPLE__
    #define _XOPEN_SOURCE_EXTENDED 1
    #endif
    #include <locale.h>
    #include <curses.h>
    #include <wchar.h>
    int main(void) {
      wchar_t wch = L'w';
      setlocale(LC_ALL, "");
      resize_term(0, 0);
      addwstr(L"wide chars\n");
      addnwstr(&wch, 1);
      add_wch(WACS_DEGREE);
      return 0;
    }'''

  curses_dep_list = host_os == 'windows' ? ['ncurses', 'ncursesw'] : ['ncursesw']
  curses = dependency(curses_dep_list,
                      required: false,
                      method: 'pkg-config')
  msg = get_option('curses').enabled() ? 'curses library not found' : ''
  curses_compile_args = ['-DNCURSES_WIDECHAR=1']
  if curses.found()
    if cc.links(curses_test, args: curses_compile_args, dependencies: [curses])
      curses = declare_dependency(compile_args: curses_compile_args, dependencies: [curses],
                                  version: curses.version())
    else
      msg = 'curses package not usable'
      curses = not_found
    endif
  endif
  if not curses.found()
    has_curses_h = cc.has_header('curses.h', args: curses_compile_args)
    if host_os != 'windows' and not has_curses_h
      message('Trying with /usr/include/ncursesw')
      curses_compile_args += ['-I/usr/include/ncursesw']
      has_curses_h = cc.has_header('curses.h', args: curses_compile_args)
    endif
    if has_curses_h
      curses_libname_list = (host_os == 'windows' ? ['pdcurses'] : ['ncursesw', 'cursesw'])
      foreach curses_libname : curses_libname_list
        libcurses = cc.find_library(curses_libname,
                                    required: false)
        if libcurses.found()
          if cc.links(curses_test, args: curses_compile_args, dependencies: libcurses)
            curses = declare_dependency(compile_args: curses_compile_args,
                                        dependencies: [libcurses])
            break
          else
            msg = 'curses library not usable'
          endif
        endif
      endforeach
    endif
  endif
  if get_option('iconv').allowed()
    foreach link_args : [ ['-liconv'], [] ]
      if cc.links('''
        #include <iconv.h>
        int main(void) {
          iconv_t conv = iconv_open("WCHAR_T", "UCS-2");
          return conv != (iconv_t) -1;
        }''', args: link_args, dependencies: glib)
        iconv = declare_dependency(link_args: link_args, dependencies: glib)
        break
      endif
    endforeach
  endif
  if curses.found() and not iconv.found()
    if get_option('iconv').enabled()
      error('iconv not available')
    endif
    msg = 'iconv required for curses UI but not available'
    curses = not_found
  endif
  if not curses.found() and msg != ''
    if get_option('curses').enabled()
      error(msg)
    else
      warning(msg + ', disabling')
    endif
  endif
endif

brlapi = not_found
if not get_option('brlapi').auto() or have_system
  brlapi = cc.find_library('brlapi', has_headers: ['brlapi.h'],
                         required: get_option('brlapi'))
  if brlapi.found() and not cc.links('''
     #include <brlapi.h>
     #include <stddef.h>
     int main(void) { return brlapi__openConnection (NULL, NULL, NULL); }''', dependencies: brlapi)
    brlapi = not_found
    if get_option('brlapi').enabled()
      error('could not link brlapi')
    else
      warning('could not link brlapi, disabling')
    endif
  endif
endif

sdl = not_found
if not get_option('sdl').auto() or have_system
  sdl = dependency('sdl2', required: get_option('sdl'))
endif
if sdl.found()
  if not cc.compiles('''
                     #include <SDL.h>
                     #include <SDL_syswm.h>
                     int main(int argc, char *argv[]) { return 0; }
                     ''', dependencies: sdl, args: '-Werror=undef')
    sdl = declare_dependency(compile_args: '-Wno-undef',
                             dependencies: sdl,
                             version: sdl.version())
  endif
endif

libbzip2 = not_found
if not get_option('bzip2').auto() or have_block
  libbzip2 = cc.find_library('bz2', has_headers: ['bzlib.h'],
                             required: get_option('bzip2'))
  if libbzip2.found() and not cc.links('''
     #include <bzlib.h>
     int main(void) { BZ2_bzlibVersion(); return 0; }''', dependencies: libbzip2)
    libbzip2 = not_found
    if get_option('bzip2').enabled()
      error('could not link libbzip2')
    else
      warning('could not link libbzip2, disabling')
    endif
  endif
endif

liblzfse = not_found
if not get_option('lzfse').auto() or have_block
  liblzfse = cc.find_library('lzfse', has_headers: ['lzfse.h'],
                             required: get_option('lzfse'))
endif
if liblzfse.found() and not cc.links('''
   #include <lzfse.h>
   int main(void) { lzfse_decode_scratch_size(); return 0; }''', dependencies: liblzfse)
  liblzfse = not_found
  if get_option('lzfse').enabled()
    error('could not link liblzfse')
  else
    warning('could not link liblzfse, disabling')
  endif
endif

oss = not_found
if get_option('oss').allowed() and have_system
  if not cc.has_header('sys/soundcard.h')
  elif host_os == 'netbsd'
    oss = cc.find_library('ossaudio', required: get_option('oss'))
  else
    oss = declare_dependency()
  endif

  if not oss.found()
    if get_option('oss').enabled()
      error('OSS not found')
    endif
  endif
endif
dsound = not_found
if not get_option('dsound').auto() or (host_os == 'windows' and have_system)
  if cc.has_header('dsound.h')
    dsound = declare_dependency(link_args: ['-lole32', '-ldxguid'])
  endif

  if not dsound.found()
    if get_option('dsound').enabled()
      error('DirectSound not found')
    endif
  endif
endif

coreaudio = not_found
if not get_option('coreaudio').auto() or (host_os == 'darwin' and have_system)
  coreaudio = dependency('appleframeworks', modules: 'CoreAudio',
                         required: get_option('coreaudio'))
endif

opengl = not_found
if not get_option('opengl').auto() or have_system
  epoxy = dependency('epoxy', method: 'pkg-config',
                      required: get_option('opengl'))
  if cc.has_header('epoxy/egl.h', dependencies: epoxy)
    opengl = epoxy
  elif get_option('opengl').enabled()
    error('epoxy/egl.h not found')
  endif
endif
gbm = not_found
if (have_system or have_tools) and (virgl.found() or opengl.found())
  gbm = dependency('gbm', method: 'pkg-config', required: false)
endif

libcbor = not_found
if not get_option('libcbor').auto() or have_system
  libcbor = dependency('libcbor', version: '>=0.7.0',
                       required: get_option('libcbor'))
endif

gnutls = not_found
gnutls_crypto = not_found
if get_option('gnutls').enabled() or (get_option('gnutls').auto() and have_system)
  gnutls_crypto = dependency('gnutls', version: '>=3.6.14',
                             method: 'pkg-config',
                             required: false)
  if gnutls_crypto.found()
    gnutls = gnutls_crypto
  else
    gnutls = dependency('gnutls', version: '>=3.5.18',
                        method: 'pkg-config',
                        required: get_option('gnutls'))
  endif
endif

gcrypt = not_found
nettle = not_found
hogweed = not_found
crypto_sm4 = not_found
crypto_sm3 = not_found
xts = 'none'

if get_option('nettle').enabled() and get_option('gcrypt').enabled()
  error('Only one of gcrypt & nettle can be enabled')
endif

if get_option('nettle').enabled() or get_option('gcrypt').enabled()
  gnutls_crypto = not_found
endif

if not gnutls_crypto.found()
  if (not get_option('gcrypt').auto() or have_system) and not get_option('nettle').enabled()
    gcrypt = dependency('libgcrypt', version: '>=1.8',
                        required: get_option('gcrypt'))
    if gcrypt.found() and get_option('prefer_static')
      gcrypt = declare_dependency(dependencies:
        [gcrypt,
         cc.find_library('gpg-error', required: true)],
        version: gcrypt.version())
    endif
    crypto_sm4 = gcrypt
    if gcrypt.found() and not cc.links('''
      #include <gcrypt.h>
      int main(void) {
        gcry_cipher_hd_t handler;
        gcry_cipher_open(&handler, GCRY_CIPHER_SM4, GCRY_CIPHER_MODE_ECB, 0);
        return 0;
      }''', dependencies: gcrypt)
      crypto_sm4 = not_found
    endif
    crypto_sm3 = gcrypt
    if gcrypt.found() and not cc.links('''
      #include <gcrypt.h>
      int main(void) {
        gcry_md_hd_t handler;
        gcry_md_open(&handler, GCRY_MD_SM3, 0);
        return 0;
      }''', dependencies: gcrypt)
      crypto_sm3 = not_found
    endif
  endif
  if (not get_option('nettle').auto() or have_system) and not gcrypt.found()
    nettle = dependency('nettle', version: '>=3.4',
                        method: 'pkg-config',
                        required: get_option('nettle'))
    if nettle.found() and not cc.has_header('nettle/xts.h', dependencies: nettle)
      xts = 'private'
    endif
    crypto_sm4 = nettle
    if nettle.found() and not cc.links('''
      #include <nettle/sm4.h>
      int main(void) {
        struct sm4_ctx ctx;
        unsigned char key[16] = {0};
        sm4_set_encrypt_key(&ctx, key);
        return 0;
      }''', dependencies: nettle)
      crypto_sm4 = not_found
    endif
    crypto_sm3 = nettle
    if nettle.found() and not cc.links('''
      #include <nettle/sm3.h>
      #include <nettle/hmac.h>
      int main(void) {
      struct sm3_ctx ctx;
      struct hmac_sm3_ctx hmac_ctx;
      unsigned char data[64] = {0};
      unsigned char output[32];

      // SM3 hash function test
      sm3_init(&ctx);
      sm3_update(&ctx, 64, data);
      sm3_digest(&ctx, 32, data);

      // HMAC-SM3 test
      hmac_sm3_set_key(&hmac_ctx, 32, data);
      hmac_sm3_update(&hmac_ctx, 64, data);
      hmac_sm3_digest(&hmac_ctx, 32, output);

      return 0;
      }''', dependencies: nettle)
      crypto_sm3 = not_found
    endif
  endif
endif

capstone = not_found
if not get_option('capstone').auto() or have_system or have_user
  capstone = dependency('capstone', version: '>=3.0.5',
                        method: 'pkg-config',
                        required: get_option('capstone'))

  if capstone.found() and not cc.compiles('#include <capstone.h>',
                                          dependencies: [capstone])
    capstone = not_found
    if get_option('capstone').enabled()
      error('capstone requested, but it does not appear to work')
    endif
  endif
endif

gmp = dependency('gmp', required: false, method: 'pkg-config')
if nettle.found() and gmp.found()
  hogweed = dependency('hogweed', version: '>=3.4',
                       method: 'pkg-config',
                       required: get_option('nettle'))
endif


x11 = not_found
png = not_found
if get_option('png').allowed() and have_system
   png = dependency('libpng', version: '>=1.6.34', required: get_option('png'),
                    method: 'pkg-config')
endif
snappy = not_found
if not get_option('snappy').auto() or have_system
  snappy = cc.find_library('snappy', has_headers: ['snappy-c.h'],
                           required: get_option('snappy'))
endif
if snappy.found() and not cc.links('''
   #include <snappy-c.h>
   int main(void) { snappy_max_compressed_length(4096); return 0; }''', dependencies: snappy)
  snappy = not_found
  if get_option('snappy').enabled()
    error('could not link libsnappy')
  else
    warning('could not link libsnappy, disabling')
  endif
endif

lzo = not_found
if not get_option('lzo').auto() or have_system
  lzo = cc.find_library('lzo2', has_headers: ['lzo/lzo1x.h'],
                        required: get_option('lzo'))
endif
if lzo.found() and not cc.links('''
   #include <lzo/lzo1x.h>
   int main(void) { lzo_version(); return 0; }''', dependencies: lzo)
  lzo = not_found
  if get_option('lzo').enabled()
    error('could not link liblzo2')
  else
    warning('could not link liblzo2, disabling')
  endif
endif

fdt = not_found
fdt_opt = get_option('fdt')
if fdt_opt == 'enabled' and get_option('wrap_mode') == 'nodownload'
  fdt_opt = 'system'
endif
if fdt_opt in ['enabled', 'system'] or (fdt_opt == 'auto' and have_system)
  fdt = cc.find_library('fdt', required: fdt_opt == 'system')
  if fdt.found() and cc.links('''
     #include <libfdt.h>
     #include <libfdt_env.h>
     int main(void) { fdt_find_max_phandle(NULL, NULL); return 0; }''',
       dependencies: fdt)
    fdt_opt = 'system'
  elif fdt_opt != 'system'
    fdt_opt = get_option('wrap_mode') == 'nodownload' ? 'disabled' : 'internal'
    fdt = not_found
  else
    error('system libfdt is too old (1.5.1 or newer required)')
  endif
endif
if fdt_opt == 'internal'
  assert(not fdt.found())
  libfdt_proj = subproject('dtc', required: true,
                           default_options: ['tools=false',  'yaml=disabled',
                                             'python=disabled', 'default_library=static'])
  fdt = libfdt_proj.get_variable('libfdt_dep')
endif

cacard = not_found
if not get_option('smartcard').auto() or have_system
  cacard = dependency('libcacard', required: get_option('smartcard'),
                      version: '>=2.5.1', method: 'pkg-config')
endif
u2f = not_found
if not get_option('u2f').auto() or have_system
  u2f = dependency('u2f-emu', required: get_option('u2f'),
                   method: 'pkg-config')
endif
libusb = not_found
if not get_option('libusb').auto() or have_system
  libusb = dependency('libusb-1.0', required: get_option('libusb'),
                      version: '>=1.0.13', method: 'pkg-config')
endif

libpmem = not_found
if not get_option('libpmem').auto() or have_system
  libpmem = dependency('libpmem', required: get_option('libpmem'),
                       method: 'pkg-config')
endif
libdaxctl = not_found
if not get_option('libdaxctl').auto() or have_system
  libdaxctl = dependency('libdaxctl', required: get_option('libdaxctl'),
                         version: '>=57', method: 'pkg-config')
endif
tasn1 = not_found
if gnutls.found()
  tasn1 = dependency('libtasn1',
                     required: false,
                     method: 'pkg-config')
endif
has_gettid = cc.has_function('gettid')


malloc = []
if get_option('malloc') == 'system'
  has_malloc_trim = \
    get_option('malloc_trim').allowed() and \
    cc.has_function('malloc_trim', prefix: '#include <malloc.h>')
else
  has_malloc_trim = false
  malloc = cc.find_library(get_option('malloc'), required: true)
endif
if not has_malloc_trim and get_option('malloc_trim').enabled()
  if get_option('malloc') == 'system'
    error('malloc_trim not available on this platform.')
  else
    error('malloc_trim not available with non-libc memory allocator')
  endif
endif

osdep_prefix = '''
  #ifndef _GNU_SOURCE
  #define _GNU_SOURCE
  #endif

  #include <stddef.h>
  #include <sys/types.h>

  #include <string.h>
  #include <limits.h>
  /* Put unistd.h before time.h as that triggers localtime_r/gmtime_r
   * function availability on recentish Mingw-w64 platforms. */
  #include <unistd.h>
  #include <time.h>
  #include <errno.h>
  #include <fcntl.h>
'''

libdw = not_found
if not get_option('libdw').auto() or \
        (not get_option('prefer_static') and (have_system or have_user))
    libdw = dependency('libdw',
                       method: 'pkg-config',
                       required: get_option('libdw'))
endif


config_host_data = configuration_data()

config_host_data.set('CONFIG_HAVE_RUST', have_rust)
audio_drivers_selected = []
if have_system
  audio_drivers_available = {
    'alsa': false,
    'coreaudio': false,
    'dsound': false,
    'jack': false,
    'oss': false,
    'pa': false,
    'pipewire': false,
    'sdl': false,
    'sndio': false,
    'aaudio': aaudio.found(),
  }
  foreach k, v: audio_drivers_available
    config_host_data.set('CONFIG_AUDIO_' + k.to_upper(), v)
  endforeach

  audio_drivers_priority = \
    [ 'aaudio' ]
  audio_drivers_default = []
  foreach k: audio_drivers_priority
    if audio_drivers_available[k]
      audio_drivers_default += k
    endif
  endforeach

  foreach k: get_option('audio_drv_list')
    if k == 'default'
      audio_drivers_selected += audio_drivers_default
    elif not audio_drivers_available[k]
      error('Audio driver "@0@" not available.'.format(k))
    else
      audio_drivers_selected += k
    endif
  endforeach
endif
config_host_data.set('CONFIG_AUDIO_DRIVERS',
                     '"' + '", "'.join(audio_drivers_selected) + '", ')

have_host_block_device = (host_os != 'darwin' or
    cc.has_header('IOKit/storage/IOMedia.h'))

dbus_display = get_option('dbus_display') \
  .require(gio.version().version_compare('>=2.64'),
           error_message: '-display dbus requires glib>=2.64') \
  .require(gdbus_codegen.found(),
           error_message: gdbus_codegen_error.format('-display dbus')) \
  .allowed()

have_virtfs = get_option('virtfs') \
    .require(host_os == 'linux' or host_os == 'darwin',
             error_message: 'virtio-9p (virtfs) requires Linux or macOS') \
    .require(host_os == 'linux' or cc.has_function('pthread_fchdir_np'),
             error_message: 'virtio-9p (virtfs) on macOS requires the presence of pthread_fchdir_np') \
    .require(host_os == 'darwin' or libattr.found(),
             error_message: 'virtio-9p (virtfs) on Linux requires libattr-devel') \
    .disable_auto_if(not have_tools and not have_system) \
    .allowed()

qga_fsfreeze = false
qga_fstrim = false
if host_os == 'linux'
    if cc.has_header_symbol('linux/fs.h', 'FIFREEZE')
        qga_fsfreeze = true
    endif
    if cc.has_header_symbol('linux/fs.h', 'FITRIM')
        qga_fstrim = true
    endif
elif host_os == 'freebsd' and cc.has_header_symbol('ufs/ffs/fs.h', 'UFSSUSPEND')
    qga_fsfreeze = true
endif

if get_option('block_drv_ro_whitelist') == ''
  config_host_data.set('CONFIG_BDRV_RO_WHITELIST', '')
else
  config_host_data.set('CONFIG_BDRV_RO_WHITELIST',
        '"' + get_option('block_drv_ro_whitelist').replace(',', '", "') + '", ')
endif
if get_option('block_drv_rw_whitelist') == ''
  config_host_data.set('CONFIG_BDRV_RW_WHITELIST', '')
else
  config_host_data.set('CONFIG_BDRV_RW_WHITELIST',
        '"' + get_option('block_drv_rw_whitelist').replace(',', '", "') + '", ')
endif

foreach k : get_option('trace_backends')
  config_host_data.set('CONFIG_TRACE_' + k.to_upper(), true)
endforeach
config_host_data.set_quoted('CONFIG_TRACE_FILE', get_option('trace_file'))
config_host_data.set_quoted('CONFIG_TLS_PRIORITY', get_option('tls_priority'))
if iasl.found()
  config_host_data.set_quoted('CONFIG_IASL', iasl.full_path())
endif
config_host_data.set_quoted('CONFIG_BINDIR', get_option('prefix') / get_option('bindir'))
config_host_data.set_quoted('CONFIG_PREFIX', get_option('prefix'))
config_host_data.set_quoted('CONFIG_QEMU_CONFDIR', get_option('prefix') / qemu_confdir)
config_host_data.set_quoted('CONFIG_QEMU_DATADIR', get_option('prefix') / qemu_datadir)

qemu_firmwarepath = ''
foreach k : get_option('qemu_firmwarepath')
  qemu_firmwarepath += '"' + get_option('prefix') / k + '", '
endforeach
config_host_data.set('CONFIG_QEMU_FIRMWAREPATH', qemu_firmwarepath)

config_host_data.set_quoted('CONFIG_QEMU_HELPERDIR', get_option('prefix') / get_option('libexecdir'))
config_host_data.set_quoted('CONFIG_QEMU_LOCALEDIR', get_option('prefix') / get_option('localedir'))
config_host_data.set_quoted('CONFIG_QEMU_LOCALSTATEDIR', get_option('prefix') / get_option('localstatedir'))
config_host_data.set_quoted('CONFIG_QEMU_MODDIR', get_option('prefix') / qemu_moddir)
config_host_data.set_quoted('CONFIG_SYSCONFDIR', get_option('prefix') / get_option('sysconfdir'))

if enable_modules
  config_host_data.set('CONFIG_STAMP', run_command(
      meson.current_source_dir() / 'scripts/qemu-stamp.py',
      meson.project_version(), get_option('pkgversion'), '--',
      meson.current_source_dir() / 'configure',
      capture: true, check: true).stdout().strip())
endif

config_host_data.set('HOST_' + host_arch.to_upper(), 1)

if get_option('module_upgrades') and not enable_modules
  error('Cannot enable module-upgrades as modules are not enabled')
endif
config_host_data.set('CONFIG_MODULE_UPGRADES', get_option('module_upgrades'))

config_host_data.set('CONFIG_ATTR', libattr.found())
config_host_data.set('CONFIG_BDRV_WHITELIST_TOOLS', get_option('block_drv_whitelist_in_tools'))
config_host_data.set('CONFIG_BRLAPI', brlapi.found())
config_host_data.set('CONFIG_BSD', host_os in bsd_oses)
config_host_data.set('CONFIG_FREEBSD', host_os == 'freebsd')
config_host_data.set('CONFIG_CAPSTONE', capstone.found())
config_host_data.set('CONFIG_COCOA', cocoa.found())
config_host_data.set('CONFIG_DARWIN', host_os == 'darwin')
config_host_data.set('CONFIG_FDT', fdt.found())
config_host_data.set('CONFIG_FUZZ', get_option('fuzzing'))
config_host_data.set('CONFIG_GCOV', get_option('b_coverage'))
config_host_data.set('CONFIG_LIBUDEV', libudev.found())
config_host_data.set('CONFIG_LINUX', host_os == 'linux')
config_host_data.set('CONFIG_POSIX', host_os != 'windows')
config_host_data.set('CONFIG_WIN32', host_os == 'windows')
config_host_data.set('CONFIG_LZO', lzo.found())
config_host_data.set('CONFIG_BLKIO', blkio.found())
if blkio.found()
  config_host_data.set('CONFIG_BLKIO_WRITE_ZEROS_FUA',
                       blkio.version().version_compare('>=1.4.0'))
endif
config_host_data.set('CONFIG_CURL', curl.found())
config_host_data.set('CONFIG_CURSES', curses.found())
config_host_data.set('CONFIG_GBM', gbm.found())
config_host_data.set('CONFIG_GIO', gio.found())
config_host_data.set('CONFIG_HEXAGON_IDEF_PARSER', get_option('hexagon_idef_parser'))
config_host_data.set('CONFIG_LIBATTR', have_old_libattr)
config_host_data.set('CONFIG_LIBCAP_NG', libcap_ng.found())
config_host_data.set('CONFIG_LIBDAXCTL', libdaxctl.found())
config_host_data.set('CONFIG_LINUX_IO_URING', linux_io_uring.found())
config_host_data.set('CONFIG_LIBPMEM', libpmem.found())
config_host_data.set('CONFIG_MODULES', enable_modules)
config_host_data.set('CONFIG_OPENGL', opengl.found())
config_host_data.set('CONFIG_PLUGIN', get_option('plugins'))
config_host_data.set('CONFIG_RELOCATABLE', get_option('relocatable'))
config_host_data.set('CONFIG_SAFESTACK', get_option('safe_stack'))
config_host_data.set('CONFIG_SDL', sdl.found())
config_host_data.set('CONFIG_SECCOMP', seccomp.found())
if seccomp.found()
  config_host_data.set('CONFIG_SECCOMP_SYSRAWRC', seccomp_has_sysrawrc)
endif
config_host_data.set('CONFIG_PIXMAN', pixman.found())
config_host_data.set('CONFIG_SNAPPY', snappy.found())
config_host_data.set('CONFIG_SOLARIS', host_os == 'sunos')
if get_option('tcg').allowed()
  config_host_data.set('CONFIG_TCG', 1)
  config_host_data.set('CONFIG_TCG_INTERPRETER', tcg_arch == 'tci')
endif
config_host_data.set('CONFIG_TSAN', get_option('tsan'))
config_host_data.set('CONFIG_USB_LIBUSB', libusb.found())
config_host_data.set('CONFIG_VDE', vde.found())
config_host_data.set('CONFIG_PNG', png.found())
if virgl.found()
  config_host_data.set('VIRGL_VERSION_MAJOR', virgl.version().split('.')[0])
endif
config_host_data.set('CONFIG_VIRTFS', have_virtfs)
config_host_data.set('CONFIG_XKBCOMMON', xkbcommon.found())
config_host_data.set('CONFIG_GETTID', has_gettid)
config_host_data.set('CONFIG_GNUTLS', gnutls.found())
config_host_data.set('CONFIG_GNUTLS_CRYPTO', gnutls_crypto.found())
config_host_data.set('CONFIG_TASN1', tasn1.found())
config_host_data.set('CONFIG_GCRYPT', gcrypt.found())
config_host_data.set('CONFIG_NETTLE', nettle.found())
config_host_data.set('CONFIG_CRYPTO_SM4', crypto_sm4.found())
config_host_data.set('CONFIG_CRYPTO_SM3', crypto_sm3.found())
config_host_data.set('CONFIG_HOGWEED', hogweed.found())
config_host_data.set('CONFIG_QEMU_PRIVATE_XTS', xts == 'private')
config_host_data.set('CONFIG_MALLOC_TRIM', has_malloc_trim)
config_host_data.set('CONFIG_ZSTD', zstd.found())
config_host_data.set('CONFIG_QPL', qpl.found())
config_host_data.set('CONFIG_UADK', uadk.found())
config_host_data.set('CONFIG_QATZIP', qatzip.found())
config_host_data.set('CONFIG_SPICE_PROTOCOL', spice_protocol.found())
if spice_protocol.found()
config_host_data.set('CONFIG_SPICE_PROTOCOL_MAJOR', spice_protocol.version().split('.')[0])
config_host_data.set('CONFIG_SPICE_PROTOCOL_MINOR', spice_protocol.version().split('.')[1])
config_host_data.set('CONFIG_SPICE_PROTOCOL_MICRO', spice_protocol.version().split('.')[2])
endif
config_host_data.set('CONFIG_SPICE', spice.found())
config_host_data.set('CONFIG_X11', x11.found())
config_host_data.set('CONFIG_DBUS_DISPLAY', dbus_display)
config_host_data.set('CONFIG_CFI', get_option('cfi'))
config_host_data.set('CONFIG_XEN_BACKEND', xen.found())
config_host_data.set('CONFIG_LIBDW', libdw.found())
if xen.found()
  xen_version = xen.version().split('.') + ['0', '0']
  xen_ctrl_version = xen_version[0] + \
    ('0' + xen_version[1]).substring(-2) + \
    ('0' + xen_version[2]).substring(-2)
  config_host_data.set('CONFIG_XEN_CTRL_INTERFACE_VERSION', xen_ctrl_version)
endif
config_host_data.set('QEMU_VERSION', '"@0@"'.format(meson.project_version()))
config_host_data.set('QEMU_VERSION_MAJOR', meson.project_version().split('.')[0])
config_host_data.set('QEMU_VERSION_MINOR', meson.project_version().split('.')[1])
config_host_data.set('QEMU_VERSION_MICRO', meson.project_version().split('.')[2])

config_host_data.set_quoted('CONFIG_HOST_DSOSUF', host_dsosuf)
config_host_data.set('HAVE_HOST_BLOCK_DEVICE', have_host_block_device)

have_coroutine_pool = get_option('coroutine_pool')
if get_option('debug_stack_usage') and have_coroutine_pool
  message('Disabling coroutine pool to measure stack usage')
  have_coroutine_pool = false
endif
config_host_data.set('CONFIG_COROUTINE_POOL', have_coroutine_pool)
config_host_data.set('CONFIG_DEBUG_GRAPH_LOCK', get_option('debug_graph_lock'))
config_host_data.set('CONFIG_DEBUG_MUTEX', get_option('debug_mutex'))
config_host_data.set('CONFIG_DEBUG_STACK_USAGE', get_option('debug_stack_usage'))
config_host_data.set('CONFIG_DEBUG_TCG', get_option('debug_tcg'))
config_host_data.set('CONFIG_DEBUG_REMAP', get_option('debug_remap'))
config_host_data.set('CONFIG_QOM_CAST_DEBUG', get_option('qom_cast_debug'))
config_host_data.set('CONFIG_REPLICATION', get_option('replication').allowed())
config_host_data.set('CONFIG_FSFREEZE', qga_fsfreeze)
config_host_data.set('CONFIG_FSTRIM', qga_fstrim)

config_host_data.set('CONFIG_EPOLL', cc.has_header('sys/epoll.h'))
config_host_data.set('CONFIG_LINUX_MAGIC_H', cc.has_header('linux/magic.h'))
config_host_data.set('CONFIG_VALGRIND_H', cc.has_header('valgrind/valgrind.h'))
config_host_data.set('HAVE_BTRFS_H', cc.has_header('linux/btrfs.h'))
config_host_data.set('HAVE_DRM_H', cc.has_header('libdrm/drm.h'))
config_host_data.set('HAVE_OPENAT2_H', cc.has_header('linux/openat2.h'))
config_host_data.set('HAVE_PTY_H', cc.has_header('pty.h'))
config_host_data.set('HAVE_SYS_DISK_H', cc.has_header('sys/disk.h'))
config_host_data.set('HAVE_SYS_IOCCOM_H', cc.has_header('sys/ioccom.h'))
config_host_data.set('HAVE_SYS_KCOV_H', cc.has_header('sys/kcov.h'))
if host_os == 'windows'
  config_host_data.set('HAVE_AFUNIX_H', cc.has_header('afunix.h'))
endif

config_host_data.set('CONFIG_CLOSE_RANGE', cc.has_function('close_range'))
config_host_data.set('CONFIG_ACCEPT4', cc.has_function('accept4'))
config_host_data.set('CONFIG_CLOCK_ADJTIME', cc.has_function('clock_adjtime'))
config_host_data.set('CONFIG_DUP3', cc.has_function('dup3'))
config_host_data.set('CONFIG_FALLOCATE', cc.has_function('fallocate'))
config_host_data.set('CONFIG_POSIX_FALLOCATE', cc.has_function('posix_fallocate'))
config_host_data.set('CONFIG_SCHED_GETCPU', cc.has_function('sched_getcpu', prefix: '#include <sched.h>'))
config_host_data.set('CONFIG_POSIX_MEMALIGN', cc.has_function('posix_memalign', prefix: '#include <stdlib.h>'))
config_host_data.set('CONFIG_ALIGNED_MALLOC', cc.has_function('_aligned_malloc'))
config_host_data.set('CONFIG_VALLOC', cc.has_function('valloc'))
config_host_data.set('CONFIG_MEMALIGN', cc.has_function('memalign'))
config_host_data.set('CONFIG_PPOLL', cc.has_function('ppoll'))
config_host_data.set('CONFIG_PREADV', cc.has_function('preadv', prefix: '#include <sys/uio.h>'))
config_host_data.set('CONFIG_PTHREAD_FCHDIR_NP', cc.has_function('pthread_fchdir_np'))
config_host_data.set('CONFIG_SENDFILE', cc.has_function('sendfile'))
config_host_data.set('CONFIG_SETNS', cc.has_function('setns') and cc.has_function('unshare'))
config_host_data.set('CONFIG_SYNCFS', cc.has_function('syncfs'))
config_host_data.set('CONFIG_SYNC_FILE_RANGE', cc.has_function('sync_file_range'))
config_host_data.set('CONFIG_TIMERFD', cc.has_function('timerfd_create'))
config_host_data.set('CONFIG_GETLOADAVG', cc.has_function('getloadavg'))
config_host_data.set('HAVE_COPY_FILE_RANGE', cc.has_function('copy_file_range'))
config_host_data.set('HAVE_GETIFADDRS', cc.has_function('getifaddrs'))
config_host_data.set('HAVE_GLIB_WITH_SLICE_ALLOCATOR', glib_has_gslice)
config_host_data.set('HAVE_GLIB_WITH_ALIGNED_ALLOC', glib_has_aligned_alloc)
config_host_data.set('HAVE_OPENPTY', cc.has_function('openpty', dependencies: util))
config_host_data.set('HAVE_STRCHRNUL', cc.has_function('strchrnul', prefix: osdep_prefix))
config_host_data.set('HAVE_SYSTEM_FUNCTION', cc.has_function('system', prefix: '#include <stdlib.h>'))
have_asan_fiber = false
if get_option('asan') and \
   not cc.has_function('__sanitizer_start_switch_fiber',
                         args: '-fsanitize=address',
                         prefix: '#include <sanitizer/asan_interface.h>')
  warning('Missing ASAN due to missing fiber annotation interface')
  warning('Without code annotation, the report may be inferior.')
else
  have_asan_fiber = true
endif
config_host_data.set('CONFIG_ASAN_IFACE_FIBER', have_asan_fiber)

have_inotify_init = cc.has_header_symbol('sys/inotify.h', 'inotify_init')
have_inotify_init1 = cc.has_header_symbol('sys/inotify.h', 'inotify_init1')
inotify = not_found
if (have_inotify_init or have_inotify_init1) and host_os == 'freebsd'
  inotify = cc.find_library('inotify')
  if have_inotify_init
    have_inotify_init = inotify.found()
  endif
  if have_inotify_init1
    have_inotify_init1 = inotify.found()
  endif
endif
config_host_data.set('CONFIG_INOTIFY', have_inotify_init)
config_host_data.set('CONFIG_INOTIFY1', have_inotify_init1)

config_host_data.set('CONFIG_BLKZONED',
                     cc.has_header_symbol('linux/blkzoned.h', 'BLKOPENZONE'))
config_host_data.set('CONFIG_EPOLL_CREATE1',
                     cc.has_header_symbol('sys/epoll.h', 'epoll_create1'))
config_host_data.set('CONFIG_FALLOCATE_PUNCH_HOLE',
                     cc.has_header_symbol('linux/falloc.h', 'FALLOC_FL_PUNCH_HOLE') and
                     cc.has_header_symbol('linux/falloc.h', 'FALLOC_FL_KEEP_SIZE'))
config_host_data.set('CONFIG_FALLOCATE_ZERO_RANGE',
                     cc.has_header_symbol('linux/falloc.h', 'FALLOC_FL_ZERO_RANGE'))
config_host_data.set('CONFIG_FIEMAP',
                     cc.has_header('linux/fiemap.h') and
                     cc.has_header_symbol('linux/fs.h', 'FS_IOC_FIEMAP'))
config_host_data.set('CONFIG_GETCPU',
                     cc.has_header_symbol('sched.h', 'getcpu', prefix: osdep_prefix))
config_host_data.set('CONFIG_GETRANDOM',
                     cc.has_function('getrandom') and
                     cc.has_header_symbol('sys/random.h', 'GRND_NONBLOCK'))
config_host_data.set('CONFIG_PRCTL_PR_SET_TIMERSLACK',
                     cc.has_header_symbol('sys/prctl.h', 'PR_SET_TIMERSLACK'))
config_host_data.set('CONFIG_RTNETLINK',
                     cc.has_header_symbol('linux/rtnetlink.h', 'IFLA_PROTO_DOWN'))
config_host_data.set('CONFIG_SYSMACROS',
                     cc.has_header_symbol('sys/sysmacros.h', 'makedev'))
config_host_data.set('HAVE_OPTRESET',
                     cc.has_header_symbol('getopt.h', 'optreset'))
config_host_data.set('HAVE_IPPROTO_MPTCP',
                     cc.has_header_symbol('netinet/in.h', 'IPPROTO_MPTCP'))
if linux_io_uring.found()
  config_host_data.set('HAVE_IO_URING_PREP_WRITEV2',
                       cc.has_header_symbol('liburing.h', 'io_uring_prep_writev2'))
endif

config_host_data.set('HAVE_SIGEV_NOTIFY_THREAD_ID',
                     cc.has_member('struct sigevent', 'sigev_notify_thread_id',
                                   prefix: '#include <signal.h>'))
config_host_data.set('HAVE_STRUCT_STAT_ST_ATIM',
                     cc.has_member('struct stat', 'st_atim',
                                   prefix: '#include <sys/stat.h>'))
config_host_data.set('HAVE_BLK_ZONE_REP_CAPACITY',
                     cc.has_member('struct blk_zone', 'capacity',
                                   prefix: '#include <linux/blkzoned.h>'))

config_host_data.set('CONFIG_IOVEC',
                     cc.has_type('struct iovec',
                                 prefix: '#include <sys/uio.h>'))
config_host_data.set('HAVE_UTMPX',
                     cc.has_type('struct utmpx',
                                 prefix: '#include <utmpx.h>'))

config_host_data.set('CONFIG_EVENTFD', cc.links('''
  #include <sys/eventfd.h>
  int main(void) { return eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC); }'''))
config_host_data.set('CONFIG_FDATASYNC', cc.links(osdep_prefix + '''
  int main(void) {
  #if defined(_POSIX_SYNCHRONIZED_IO) && _POSIX_SYNCHRONIZED_IO > 0
  return fdatasync(0);
  #else
  #error Not supported
  #endif
  }'''))

has_madvise = cc.links(osdep_prefix + '''
  #include <sys/mman.h>
  int main(void) { return madvise(NULL, 0, MADV_DONTNEED); }''')
missing_madvise_proto = false
if has_madvise
  missing_madvise_proto = cc.links(osdep_prefix + '''>
    #include <sys/mman.h>
    extern int madvise(int);
    int main(void) { return madvise(0); }''')
endif
config_host_data.set('CONFIG_MADVISE', has_madvise)
config_host_data.set('HAVE_MADVISE_WITHOUT_PROTOTYPE', missing_madvise_proto)

config_host_data.set('CONFIG_MEMFD', cc.links(osdep_prefix + '''
  #include <sys/mman.h>
  int main(void) { return memfd_create("foo", MFD_ALLOW_SEALING); }'''))
config_host_data.set('CONFIG_OPEN_BY_HANDLE', cc.links(osdep_prefix + '''
  #if !defined(AT_EMPTY_PATH)
  # error missing definition
  #else
  int main(void) { struct file_handle fh; return open_by_handle_at(0, &fh, 0); }
  #endif'''))

if host_os != 'darwin'
  config_host_data.set('CONFIG_POSIX_MADVISE', cc.links(osdep_prefix + '''
    #include <sys/mman.h>
    int main(void) { return posix_madvise(NULL, 0, POSIX_MADV_DONTNEED); }'''))
endif

config_host_data.set('CONFIG_PTHREAD_SETNAME_NP_W_TID', cc.links(osdep_prefix + '''
  #include <pthread.h>

  static void *f(void *p) { return NULL; }
  int main(void)
  {
    pthread_t thread;
    pthread_create(&thread, 0, f, 0);
    pthread_setname_np(thread, "QEMU");
    return 0;
  }''', dependencies: threads))
config_host_data.set('CONFIG_PTHREAD_SETNAME_NP_WO_TID', cc.links(osdep_prefix + '''
  #include <pthread.h>

  static void *f(void *p) { pthread_setname_np("QEMU"); return NULL; }
  int main(void)
  {
    pthread_t thread;
    pthread_create(&thread, 0, f, 0);
    return 0;
  }''', dependencies: threads))
config_host_data.set('CONFIG_PTHREAD_SET_NAME_NP', cc.links(osdep_prefix + '''
  #include <pthread.h>
  #include <pthread_np.h>

  static void *f(void *p) { return NULL; }
  int main(void)
  {
    pthread_t thread;
    pthread_create(&thread, 0, f, 0);
    pthread_set_name_np(thread, "QEMU");
    return 0;
  }''', dependencies: threads))
config_host_data.set('CONFIG_PTHREAD_CONDATTR_SETCLOCK', cc.links(osdep_prefix + '''
  #include <pthread.h>

  int main(void)
  {
    pthread_condattr_t attr
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    return 0;
  }''', dependencies: threads))
config_host_data.set('CONFIG_PTHREAD_AFFINITY_NP', cc.links(osdep_prefix + '''
  #include <pthread.h>

  static void *f(void *p) { return NULL; }
  int main(void)
  {
    int setsize = CPU_ALLOC_SIZE(64);
    pthread_t thread;
    cpu_set_t *cpuset;
    pthread_create(&thread, 0, f, 0);
    cpuset = CPU_ALLOC(64);
    CPU_ZERO_S(setsize, cpuset);
    pthread_setaffinity_np(thread, setsize, cpuset);
    pthread_getaffinity_np(thread, setsize, cpuset);
    CPU_FREE(cpuset);
    return 0;
  }''', dependencies: threads))
config_host_data.set('CONFIG_SIGNALFD', cc.links(osdep_prefix + '''
  #include <sys/signalfd.h>
  int main(void) { return signalfd(-1, NULL, SFD_CLOEXEC); }'''))
config_host_data.set('CONFIG_SPLICE', cc.links(osdep_prefix + '''
  int main(void)
  {
    int len, fd = 0;
    len = tee(STDIN_FILENO, STDOUT_FILENO, INT_MAX, SPLICE_F_NONBLOCK);
    splice(STDIN_FILENO, NULL, fd, NULL, len, SPLICE_F_MOVE);
    return 0;
  }'''))

config_host_data.set('HAVE_MLOCKALL', cc.links(osdep_prefix + '''
  #include <sys/mman.h>
  int main(void) {
    return mlockall(MCL_FUTURE);
  }'''))

config_host_data.set('HAVE_MLOCK_ONFAULT', cc.links(osdep_prefix + '''
  #include <sys/mman.h>
  int main(void) {
      return mlockall(MCL_FUTURE | MCL_ONFAULT);
  }'''))

have_l2tpv3 = false
if get_option('l2tpv3').allowed() and have_system
  have_l2tpv3 = cc.has_type('struct mmsghdr',
    prefix: osdep_prefix + '''
      #include <sys/socket.h>
      #include <linux/ip.h>''')
endif
config_host_data.set('CONFIG_L2TPV3', have_l2tpv3)

config_host_data.set('HAVE_FSXATTR', cc.links('''
  #include <linux/fs.h>
  struct fsxattr foo;
  int main(void) {
    return 0;
  }'''))

config_host_data.set('HAVE_BROKEN_SIZE_MAX', not cc.compiles('''
    #include <stdint.h>
    #include <stdio.h>
    int main(void) {
        return printf("%zu", SIZE_MAX);
    }''', args: ['-Werror']))

config_host_data.set('CONFIG_ATOMIC64', cc.links('''
  #include <stdint.h>
  int main(void)
  {
    uint64_t x = 0, y = 0;
    y = __atomic_load_n(&x, __ATOMIC_RELAXED);
    __atomic_store_n(&x, y, __ATOMIC_RELAXED);
    __atomic_compare_exchange_n(&x, &y, x, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
    __atomic_exchange_n(&x, y, __ATOMIC_RELAXED);
    __atomic_fetch_add(&x, y, __ATOMIC_RELAXED);
    return 0;
  }''', args: qemu_isa_flags))

has_int128_type = cc.compiles('''
  __int128_t a;
  __uint128_t b;
  int main(void) { b = a; }''')
config_host_data.set('CONFIG_INT128_TYPE', has_int128_type)

has_int128 = has_int128_type and cc.links('''
  __int128_t a;
  __uint128_t b;
  int main (void) {
    a = a + b;
    b = a * b;
    a = a * a;
    return 0;
  }''')
config_host_data.set('CONFIG_INT128', has_int128)

if has_int128_type
  atomic_test_128 = '''
    int main(int ac, char **av) {
      __uint128_t *p = __builtin_assume_aligned(av[ac - 1], 16);
      p[1] = __atomic_load_n(&p[0], __ATOMIC_RELAXED);
      __atomic_store_n(&p[2], p[3], __ATOMIC_RELAXED);
      __atomic_compare_exchange_n(&p[4], &p[5], p[6], 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
      return 0;
    }'''
  has_atomic128 = cc.links(atomic_test_128, args: qemu_isa_flags)

  config_host_data.set('CONFIG_ATOMIC128', has_atomic128)

  if not has_atomic128
    has_atomic128_opt = cc.links('__attribute__((optimize("O1")))' + atomic_test_128,
                                 args: qemu_isa_flags)
    config_host_data.set('CONFIG_ATOMIC128_OPT', has_atomic128_opt)

    if not has_atomic128_opt
      config_host_data.set('CONFIG_CMPXCHG128', cc.links('''
        int main(void)
        {
          __uint128_t x = 0, y = 0;
          __sync_val_compare_and_swap_16(&x, y, x);
          return 0;
        }
      ''', args: qemu_isa_flags))
    endif
  endif
endif

config_host_data.set('CONFIG_GETAUXVAL', cc.links(osdep_prefix + '''
  #include <sys/auxv.h>
  int main(void) {
    return getauxval(AT_HWCAP) == 0;
  }'''))

config_host_data.set('CONFIG_ELF_AUX_INFO', cc.links(osdep_prefix + '''
  #include <sys/auxv.h>
  int main(void) {
    unsigned long hwcap = 0;
    elf_aux_info(AT_HWCAP, &hwcap, sizeof(hwcap));
    return hwcap;
  }'''))

config_host_data.set('CONFIG_USBFS', have_linux_user and cc.compiles('''
  #include <linux/usbdevice_fs.h>

  #ifndef USBDEVFS_GET_CAPABILITIES
  #error "USBDEVFS_GET_CAPABILITIES undefined"
  #endif

  #ifndef USBDEVFS_DISCONNECT_CLAIM
  #error "USBDEVFS_DISCONNECT_CLAIM undefined"
  #endif

  int main(void) { return 0; }'''))

have_cpuid_h = cc.links('''
  #include <cpuid.h>
  int main(void) {
    unsigned a, b, c, d;
    unsigned max = __get_cpuid_max(0, 0);

    if (max >= 1) {
        __cpuid(1, a, b, c, d);
    }

    if (max >= 7) {
        __cpuid_count(7, 0, a, b, c, d);
    }

    return 0;
  }''')
config_host_data.set('CONFIG_CPUID_H', have_cpuid_h)

config_host_data.set('CONFIG_ASM_HWPROBE_H',
                     cc.has_header_symbol('asm/hwprobe.h',
                                          'RISCV_HWPROBE_EXT_ZBA'))

config_host_data.set('CONFIG_AVX2_OPT', get_option('avx2') \
  .require(have_cpuid_h, error_message: 'cpuid.h not available, cannot enable AVX2') \
  .require(cc.links('''
    #include <cpuid.h>
    #include <immintrin.h>
    static int __attribute__((target("avx2"))) bar(void *a) {
      __m256i x = *(__m256i *)a;
      return _mm256_testz_si256(x, x);
    }
    int main(int argc, char *argv[]) { return bar(argv[argc - 1]); }
  '''), error_message: 'AVX2 not available').allowed())

config_host_data.set('CONFIG_AVX512BW_OPT', get_option('avx512bw') \
  .require(have_cpuid_h, error_message: 'cpuid.h not available, cannot enable AVX512BW') \
  .require(cc.links('''
    #include <cpuid.h>
    #include <immintrin.h>
    static int __attribute__((target("avx512bw"))) bar(void *a) {
      __m512i *x = a;
      __m512i res= _mm512_abs_epi8(*x);
      return res[1];
    }
    int main(int argc, char *argv[]) { return bar(argv[0]); }
  '''), error_message: 'AVX512BW not available').allowed())

config_host_data.set('CONFIG_ARM_AES_BUILTIN', cc.compiles('''
    #include <arm_neon.h>
    #ifndef __ARM_FEATURE_AES
    __attribute__((target("+crypto")))
    #endif
    void foo(uint8x16_t *p) { *p = vaesmcq_u8(*p); }
  '''))

if get_option('membarrier').disabled()
  have_membarrier = false
elif host_os == 'windows'
  have_membarrier = true
elif host_os == 'linux'
  have_membarrier = cc.compiles('''
    #include <linux/membarrier.h>
    #include <sys/syscall.h>
    #include <unistd.h>
    #include <stdlib.h>
    int main(void) {
        syscall(__NR_membarrier, MEMBARRIER_CMD_QUERY, 0);
        syscall(__NR_membarrier, MEMBARRIER_CMD_SHARED, 0);
        exit(0);
    }''')
endif
config_host_data.set('CONFIG_MEMBARRIER', get_option('membarrier') \
  .require(have_membarrier, error_message: 'membarrier system call not available') \
  .allowed())

have_afalg = get_option('crypto_afalg') \
  .require(cc.compiles(osdep_prefix + '''
    #include <sys/socket.h>
    #include <linux/if_alg.h>
    int main(void) {
      int sock;
      sock = socket(AF_ALG, SOCK_SEQPACKET, 0);
      return sock;
    }
  '''), error_message: 'AF_ALG requested but could not be detected').allowed()
config_host_data.set('CONFIG_AF_ALG', have_afalg)

have_vss = false
have_vss_sdk = false # old xp/2003 SDK
if host_os == 'windows' and 'cpp' in all_languages
  have_vss = cxx.compiles('''
    #define __MIDL_user_allocate_free_DEFINED__
    #include <vss.h>
    int main(void) { return VSS_CTX_BACKUP; }''')
  have_vss_sdk = cxx.has_header('vscoordint.h')
endif
config_host_data.set('HAVE_VSS_SDK', have_vss_sdk)

if host_os == 'windows'
  config_host_data.set('HAVE__LOCK_FILE', cc.links('''
    #include <stdio.h>
    int main(void) {
      _lock_file(NULL);
      _unlock_file(NULL);
      return 0;
    }''', name: '_lock_file and _unlock_file'))
endif

if host_os == 'windows'
  mingw_has_setjmp_longjmp = cc.links('''
    #include <setjmp.h>
    int main(void) {
      /*
       * These functions are not available in setjmp header, but may be
       * available at link time, from libmingwex.a.
       */
      extern int __mingw_setjmp(jmp_buf);
      extern void __attribute__((noreturn)) __mingw_longjmp(jmp_buf, int);
      jmp_buf env;
      __mingw_setjmp(env);
      __mingw_longjmp(env, 0);
    }
  ''', name: 'mingw setjmp and longjmp')

  if cpu == 'aarch64' and not mingw_has_setjmp_longjmp
    error('mingw must provide setjmp/longjmp for windows-arm64')
  endif
endif

host_long_bits = cc.sizeof('void *') * 8


minikconf = find_program('scripts/minikconf.py')

config_all_accel = {}
config_all_devices = {}
config_devices_mak_list = []
config_devices_h = {}
config_target_h = {}
config_target_mak = {}

disassemblers = {
  'alpha' : ['CONFIG_ALPHA_DIS'],
  'avr' : ['CONFIG_AVR_DIS'],
  'hexagon' : ['CONFIG_HEXAGON_DIS'],
  'hppa' : ['CONFIG_HPPA_DIS'],
  'i386' : ['CONFIG_I386_DIS'],
  'x86_64' : ['CONFIG_I386_DIS'],
  'm68k' : ['CONFIG_M68K_DIS'],
  'microblaze' : ['CONFIG_MICROBLAZE_DIS'],
  'mips' : ['CONFIG_MIPS_DIS'],
  'or1k' : ['CONFIG_OPENRISC_DIS'],
  'ppc' : ['CONFIG_PPC_DIS'],
  'riscv' : ['CONFIG_RISCV_DIS'],
  'rx' : ['CONFIG_RX_DIS'],
  's390' : ['CONFIG_S390_DIS'],
  'sh4' : ['CONFIG_SH4_DIS'],
  'sparc' : ['CONFIG_SPARC_DIS'],
  'xtensa' : ['CONFIG_XTENSA_DIS'],
  'loongarch' : ['CONFIG_LOONGARCH_DIS'],
}

have_ivshmem = config_host_data.get('CONFIG_EVENTFD')
host_kconfig = \
  (get_option('fuzzing') ? ['CONFIG_FUZZ=y'] : []) + \
  (pixman.found() ? ['CONFIG_PIXMAN=y'] : []) + \
  (spice.found() ? ['CONFIG_SPICE=y'] : []) + \
  (have_ivshmem ? ['CONFIG_IVSHMEM=y'] : []) + \
  (opengl.found() ? ['CONFIG_OPENGL=y'] : []) + \
  (libcbor.found() ? ['CONFIG_LIBCBOR=y'] : []) + \
  (gnutls.found() ? ['CONFIG_GNUTLS=y'] : []) + \
  (x11.found() ? ['CONFIG_X11=y'] : []) + \
  (fdt.found() ? ['CONFIG_FDT=y'] : []) + \
  (have_virtfs ? ['CONFIG_VIRTFS=y'] : []) + \
  (host_os == 'linux' ? ['CONFIG_LINUX=y'] : []) + \
  (multiprocess_allowed ? ['CONFIG_MULTIPROCESS_ALLOWED=y'] : []) + \
  (have_rust ? ['CONFIG_HAVE_RUST=y'] : [])

ignored = [ 'TARGET_XML_FILES', 'TARGET_ABI_DIR', 'TARGET_ARCH' ]

default_targets = 'CONFIG_DEFAULT_TARGETS' in config_host
actual_target_dirs = []
fdt_required = []
foreach target : target_dirs
  config_target = { 'TARGET_NAME': target.split('-')[0] }
  if target.endswith('linux-user')
    if host_os != 'linux'
      if default_targets
        continue
      endif
      error('Target @0@ is only available on a Linux host'.format(target))
    endif
    config_target += { 'CONFIG_LINUX_USER': 'y' }
  elif target.endswith('bsd-user')
    if host_os not in bsd_oses
      if default_targets
        continue
      endif
      error('Target @0@ is only available on a BSD host'.format(target))
    endif
    config_target += { 'CONFIG_BSD_USER': 'y' }
  elif target.endswith('softmmu')
    config_target += { 'CONFIG_SYSTEM_ONLY': 'y' }
    config_target += { 'CONFIG_SOFTMMU': 'y' }
  endif
  if target.endswith('-user')
    config_target += {
      'CONFIG_USER_ONLY': 'y',
      'CONFIG_QEMU_INTERP_PREFIX':
        get_option('interp_prefix').replace('%M', config_target['TARGET_NAME']),
      'CONFIG_QEMU_RTSIG_MAP': get_option('rtsig_map'),
    }
  endif

  config_target += keyval.load('configs/targets' / target + '.mak')

  target_kconfig = []
  foreach sym: accelerators
    if host_long_bits < config_target['TARGET_LONG_BITS'].to_int()
      continue
    endif
    if sym == 'CONFIG_TCG' or target in accelerator_targets.get(sym, [])
      config_target += { sym: 'y' }
      config_all_accel += { sym: 'y' }
      target_kconfig += [ sym + '=y' ]
    endif
  endforeach
  if target_kconfig.length() == 0
    if default_targets
      continue
    endif
    error('No accelerator available for target @0@'.format(target))
  endif

  if 'TARGET_NEED_FDT' in config_target and not fdt.found()
    if default_targets
      warning('Disabling ' + target + ' due to missing libfdt')
    else
      fdt_required += target
    endif
    continue
  endif

  actual_target_dirs += target

  config_target += { 'TARGET_' + config_target['TARGET_ARCH'].to_upper(): 'y' }
  if 'TARGET_BASE_ARCH' not in config_target
    config_target += {'TARGET_BASE_ARCH': config_target['TARGET_ARCH']}
  endif
  if 'TARGET_ABI_DIR' not in config_target
    config_target += {'TARGET_ABI_DIR': config_target['TARGET_ARCH']}
  endif
  if 'TARGET_BIG_ENDIAN' not in config_target
    config_target += {'TARGET_BIG_ENDIAN': 'n'}
  endif

  foreach k, v: disassemblers
    if host_arch.startswith(k) or config_target['TARGET_BASE_ARCH'].startswith(k)
      foreach sym: v
        config_target += { sym: 'y' }
      endforeach
    endif
  endforeach

  config_target_data = configuration_data()
  foreach k, v: config_target
    if not k.startswith('TARGET_') and not k.startswith('CONFIG_')
    elif ignored.contains(k)
    elif k == 'TARGET_BASE_ARCH'
      config_target_data.set('TARGET_' + v.to_upper(), 1)
    elif k == 'TARGET_NAME' or k == 'CONFIG_QEMU_INTERP_PREFIX'
      config_target_data.set_quoted(k, v)
    elif v == 'y'
      config_target_data.set(k, 1)
    elif v == 'n'
      config_target_data.set(k, 0)
    else
      config_target_data.set(k, v)
    endif
  endforeach
  config_target_data.set('QEMU_ARCH',
                         'QEMU_ARCH_' + config_target['TARGET_BASE_ARCH'].to_upper())
  config_target_h += {target: configure_file(output: target + '-config-target.h',
                                               configuration: config_target_data)}

  if target.endswith('-softmmu')
    target_kconfig += 'CONFIG_' + config_target['TARGET_ARCH'].to_upper() + '=y'
    target_kconfig += 'CONFIG_TARGET_BIG_ENDIAN=' + config_target['TARGET_BIG_ENDIAN']

    config_input = meson.get_external_property(target, 'default')
    config_devices_mak = target + '-config-devices.mak'
    config_devices_mak = configure_file(
      input: ['configs/devices' / target / config_input + '.mak', 'Kconfig'],
      output: config_devices_mak,
      depfile: config_devices_mak + '.d',
      capture: true,
      command: [minikconf,
                get_option('default_devices') ? '--defconfig' : '--allnoconfig',
                config_devices_mak, '@DEPFILE@', '@INPUT@',
                host_kconfig, target_kconfig])

    config_devices_data = configuration_data()
    config_devices = keyval.load(config_devices_mak)
    foreach k, v: config_devices
      config_devices_data.set(k, 1)
    endforeach
    config_devices_mak_list += config_devices_mak
    config_devices_h += {target: configure_file(output: target + '-config-devices.h',
                                                configuration: config_devices_data)}
    config_target += config_devices
    config_all_devices += config_devices
  endif
  config_target_mak += {target: config_target}
endforeach
target_dirs = actual_target_dirs

target_configs_h = []
foreach target: target_dirs
  target_configs_h += config_target_h[target]
  target_configs_h += config_devices_h.get(target, [])
endforeach
genh += custom_target('config-poison.h',
                      input: [target_configs_h],
                      output: 'config-poison.h',
                      capture: true,
                      command: [find_program('scripts/make-config-poison.sh'),
                                target_configs_h])

if fdt_required.length() > 0
  error('fdt disabled but required by targets ' + ', '.join(fdt_required))
endif


config_host_h = configure_file(output: 'config-host.h', configuration: config_host_data)
genh += config_host_h

hxtool = find_program('scripts/hxtool')
qapi_gen = find_program('scripts/qapi-gen.py')
qapi_gen_depends = [ meson.current_source_dir() / 'scripts/qapi/__init__.py',
                     meson.current_source_dir() / 'scripts/qapi/commands.py',
                     meson.current_source_dir() / 'scripts/qapi/common.py',
                     meson.current_source_dir() / 'scripts/qapi/error.py',
                     meson.current_source_dir() / 'scripts/qapi/events.py',
                     meson.current_source_dir() / 'scripts/qapi/expr.py',
                     meson.current_source_dir() / 'scripts/qapi/gen.py',
                     meson.current_source_dir() / 'scripts/qapi/introspect.py',
                     meson.current_source_dir() / 'scripts/qapi/main.py',
                     meson.current_source_dir() / 'scripts/qapi/parser.py',
                     meson.current_source_dir() / 'scripts/qapi/schema.py',
                     meson.current_source_dir() / 'scripts/qapi/source.py',
                     meson.current_source_dir() / 'scripts/qapi/types.py',
                     meson.current_source_dir() / 'scripts/qapi/features.py',
                     meson.current_source_dir() / 'scripts/qapi/visit.py',
                     meson.current_source_dir() / 'scripts/qapi-gen.py'
]

tracetool = [
  python, files('scripts/tracetool.py'),
   '--backend=' + ','.join(get_option('trace_backends'))
]
tracetool_depends = files(
  'scripts/tracetool/backend/log.py',
  'scripts/tracetool/backend/__init__.py',
  'scripts/tracetool/backend/dtrace.py',
  'scripts/tracetool/backend/ftrace.py',
  'scripts/tracetool/backend/simple.py',
  'scripts/tracetool/backend/syslog.py',
  'scripts/tracetool/backend/ust.py',
  'scripts/tracetool/format/ust_events_c.py',
  'scripts/tracetool/format/ust_events_h.py',
  'scripts/tracetool/format/__init__.py',
  'scripts/tracetool/format/d.py',
  'scripts/tracetool/format/simpletrace_stap.py',
  'scripts/tracetool/format/c.py',
  'scripts/tracetool/format/h.py',
  'scripts/tracetool/format/log_stap.py',
  'scripts/tracetool/format/stap.py',
  'scripts/tracetool/__init__.py',
)

qemu_version_cmd = [find_program('scripts/qemu-version.sh'),
                    meson.current_source_dir(),
                    get_option('pkgversion'), meson.project_version()]
qemu_version = custom_target('qemu-version.h',
                             output: 'qemu-version.h',
                             command: qemu_version_cmd,
                             capture: true,
                             build_by_default: true,
                             build_always_stale: true)
genh += qemu_version

hxdep = []
hx_headers = [
  ['qemu-options.hx', 'qemu-options.def'],
]
foreach d : hx_headers
  hxdep += custom_target(d[1],
                input: files(d[0]),
                output: d[1],
                capture: true,
                command: [hxtool, '-h', '@INPUT0@'])
endforeach
genh += hxdep


trace_events_subdirs = [
  'qapi',
  'qom',
  'monitor',
  'util',
]
if have_linux_user
  trace_events_subdirs += [ 'linux-user' ]
endif
if have_bsd_user
  trace_events_subdirs += [ 'bsd-user' ]
endif
if have_block
  trace_events_subdirs += [
    'block',
    'chardev',
    'io',
  ]
endif
if have_system
  trace_events_subdirs += [
    'audio',
    'backends',
    'hw/acpi',
    'hw/arm',
    'hw/audio',
    'hw/block',
    'hw/char',
    'hw/display',
    'hw/input',
    'hw/intc',
    'hw/mem',
    'hw/misc',
    'hw/net',
    'hw/nvram',
    'hw/pci',
    'hw/pci-host',
    'hw/rtc',
    'hw/uefi',
    'hw/virtio',
    'hw/gpio',
    'state',
    'net',
    'system',
    'ui',
  ]
endif
if have_system or have_user
  trace_events_subdirs += [
    'hw/core',
    'target/arm',
  ]
  if get_option('tcg').allowed()
    trace_events_subdirs += ['accel/tcg']
  endif
endif


blockdev_ss = ss.source_set()
block_ss = ss.source_set()
chardev_ss = ss.source_set()
common_ss = ss.source_set()
hwcore_ss = ss.source_set()
io_ss = ss.source_set()
qmp_ss = ss.source_set()
qom_ss = ss.source_set()
system_ss = ss.source_set()
specific_fuzz_ss = ss.source_set()
specific_ss = ss.source_set()
rust_devices_ss = ss.source_set()
stub_ss = ss.source_set()
trace_ss = ss.source_set()
user_ss = ss.source_set()
util_ss = ss.source_set()

modules = {}
target_modules = {}
plugin_modules = []
hw_arch = {}
target_arch = {}
target_system_arch = {}
target_user_arch = {}

subdir('qapi')
subdir('qobject')
subdir('stubs')
subdir('trace')
subdir('util')
subdir('qom')
subdir('ui')
if have_system
  subdir('hw')
else
  subdir('hw/core')
endif

if enable_modules
  error('Loadable modules were removed from this trimmed build')
endif

qom_ss = qom_ss.apply({})
libqom = static_library('qom', qom_ss.sources() + genh,
                        dependencies: [qom_ss.dependencies()],
                        build_by_default: false)
qom = declare_dependency(objects: libqom.extract_all_objects(recursive: false),
                         dependencies: qom_ss.dependencies())

event_loop_base = files('event-loop-base.c')
event_loop_base = static_library('event-loop-base',
                                 sources: event_loop_base + genh,
                                 build_by_default: false)
event_loop_base = declare_dependency(objects: event_loop_base.extract_all_objects(recursive: false),
                                     dependencies: [qom])

stub_ss = stub_ss.apply({})

util_ss.add_all(trace_ss)
util_ss = util_ss.apply({})
libqemuutil = static_library('qemuutil',
                             build_by_default: false,
                             sources: util_ss.sources() + stub_ss.sources() + genh,
                             dependencies: [util_ss.dependencies(), libm, threads, glib, socket, malloc])
qemuutil_deps = [event_loop_base]
if host_os != 'windows'
  qemuutil_deps += [rt]
endif
qemuutil = declare_dependency(link_with: libqemuutil,
                              sources: genh + version_res,
                              dependencies: qemuutil_deps)

if have_system or have_user
  decodetree = generator(find_program('scripts/decodetree.py'),
                         output: 'decode-@BASENAME@.c.inc',
                         arguments: ['@INPUT@', '@EXTRA_ARGS@', '-o', '@OUTPUT@'])
  subdir('target')
endif

subdir('audio')
subdir('io')
subdir('chardev')

if have_block
  block_ss.add(files(
    'block.c',
    'job.c',
  ))
  if config_host_data.get('CONFIG_REPLICATION')
    error('Block replication was removed from this trimmed build')
  endif

  subdir('block')

  blockdev_ss.add(files(
    'blockdev.c',
    'iothread.c',
    'job-qmp.c',
  ))

  blockdev_ss.add(files('os-posix.c'))
endif

common_ss.add(files('cpu-common.c'))
specific_ss.add(files('cpu-target.c'))

subdir('system')

pagevary = files('page-vary-common.c')
if get_option('b_lto')
  pagevary_flags = ['-fno-lto']
  if get_option('cfi')
    pagevary_flags += '-fno-sanitize=cfi-icall'
  endif
  pagevary = static_library('page-vary-common', sources: pagevary + genh,
                            c_args: pagevary_flags)
  pagevary = declare_dependency(link_with: pagevary)
endif
common_ss.add(pagevary)
specific_ss.add(files('page-target.c', 'page-vary-target.c'))

subdir('backends')
subdir('disas')
subdir('state')
subdir('monitor')
subdir('net')
if get_option('tcg').allowed()
  subdir('tcg')
endif
subdir('fpu')
subdir('accel')
if get_option('plugins')
  subdir('plugins')
endif

if get_option('plugins') and 'CONFIG_TCG' in config_all_accel
  subdir('contrib/plugins')
endif

common_user_inc = []

if have_user
  subdir('common-user')
  subdir('bsd-user')
  subdir('linux-user')
endif

modinfo_collect = find_program('scripts/modinfo-collect.py')
modinfo_generate = find_program('scripts/modinfo-generate.py')
modinfo_files = []

block_mods = []
system_mods = []
emulator_modules = []
foreach d, list : modules
  if not (d == 'block' ? have_block : have_system)
    continue
  endif

  foreach m, module_ss : list
    if enable_modules
      module_ss.add(modulecommon)
      module_ss = module_ss.apply(config_all_devices, strict: false)
      sl = static_library(d + '-' + m, [genh, module_ss.sources()],
                          dependencies: module_ss.dependencies(), pic: true)
      if d == 'block'
        block_mods += sl
      else
        system_mods += sl
      endif
      emulator_modules += shared_module(sl.name(),
                    name_prefix: '',
                    objects: sl.extract_all_objects(recursive: false),
                    dependencies: module_ss.dependencies(),
                    install: true,
                    install_dir: qemu_moddir)
      if module_ss.sources() != []
        modinfo_files += custom_target(d + '-' + m + '.modinfo',
                                       output: d + '-' + m + '.modinfo',
                                       input: module_ss.sources() + genh,
                                       capture: true,
                                       command: [modinfo_collect, module_ss.sources()])
      endif
    else
      if d == 'block'
        block_ss.add_all(module_ss)
      else
        system_ss.add_all(module_ss)
      endif
    endif
  endforeach
endforeach

foreach d, list : target_modules
  foreach m, module_ss : list
    if enable_modules
      module_ss.add(modulecommon)
      foreach target : target_dirs
        if target.endswith('-softmmu')
          config_target = config_target_mak[target]
          target_inc = [include_directories('target' / config_target['TARGET_BASE_ARCH'])]
          c_args = ['-DCOMPILING_PER_TARGET',
                    '-DCONFIG_TARGET="@0@-config-target.h"'.format(target),
                    '-DCONFIG_DEVICES="@0@-config-devices.h"'.format(target)]
          target_module_ss = module_ss.apply(config_target, strict: false)
          if target_module_ss.sources() != []
            module_name = d + '-' + m + '-' + config_target['TARGET_NAME']
            sl = static_library(module_name,
                                [genh, target_module_ss.sources()],
                                dependencies: target_module_ss.dependencies(),
                                include_directories: target_inc,
                                c_args: c_args,
                                pic: true)
            system_mods += sl
            emulator_modules += shared_module(sl.name(),
                    name_prefix: '',
                    objects: sl.extract_all_objects(recursive: false),
                    dependencies: target_module_ss.dependencies(),
                    install: true,
                    install_dir: qemu_moddir)
            modinfo_files += custom_target(module_name + '.modinfo',
                                           output: module_name + '.modinfo',
                                           input: target_module_ss.sources() + genh,
                                           capture: true,
                                           command: [modinfo_collect, '--target', target, target_module_ss.sources()])
          endif
        endif
      endforeach
    else
      specific_ss.add_all(module_ss)
    endif
  endforeach
endforeach

if enable_modules
  foreach target : target_dirs
    if target.endswith('-softmmu')
      config_target = config_target_mak[target]
      config_devices_mak = target + '-config-devices.mak'
      modinfo_src = custom_target('modinfo-' + target + '.c',
                                  output: 'modinfo-' + target + '.c',
                                  input: modinfo_files,
                                  command: [modinfo_generate, '--devices', config_devices_mak, '@INPUT@'],
                                  capture: true)

      modinfo_lib = static_library('modinfo-' + target + '.c', modinfo_src)
      modinfo_dep = declare_dependency(link_with: modinfo_lib)

      arch = config_target['TARGET_NAME'] == 'sparc64' ? 'sparc64' : config_target['TARGET_BASE_ARCH']
      hw_arch[arch].add(modinfo_dep)
    endif
  endforeach

  if emulator_modules.length() > 0
    alias_target('modules', emulator_modules)
  endif
endif

nm = find_program('nm')
undefsym = find_program('scripts/undefsym.py')
block_syms = custom_target('block.syms', output: 'block.syms',
                             input: [libqemuutil, block_mods],
                             capture: true,
                             command: [undefsym, nm, '@INPUT@'])
qemu_syms = custom_target('qemu.syms', output: 'qemu.syms',
                             input: [libqemuutil, system_mods],
                             capture: true,
                             command: [undefsym, nm, '@INPUT@'])

io_ss = io_ss.apply({})
libio = static_library('io', io_ss.sources() + genh,
                       dependencies: [io_ss.dependencies()],
                       link_with: libqemuutil,
                       build_by_default: false)

io = declare_dependency(objects: libio.extract_all_objects(recursive: false),
                        dependencies: [io_ss.dependencies(), qom])

libstate = static_library('state', sources: state_files + genh,
                              build_by_default: false)
state_dep = declare_dependency(objects: libstate.extract_all_objects(recursive: false),
                               dependencies: [qom, io])
system_ss.add(state_dep)

block_ss = block_ss.apply({})
libblock = static_library('block', block_ss.sources() + genh,
                          dependencies: block_ss.dependencies(),
                          build_by_default: false)

block = declare_dependency(objects: libblock.extract_all_objects(recursive: false),
                           dependencies: [block_ss.dependencies(), io])

blockdev_ss = blockdev_ss.apply({})
libblockdev = static_library('blockdev', blockdev_ss.sources() + genh,
                             dependencies: blockdev_ss.dependencies(),
                             build_by_default: false)

blockdev = declare_dependency(objects: libblockdev.extract_all_objects(recursive: false),
                              dependencies: [blockdev_ss.dependencies(), block, event_loop_base])

qmp_ss = qmp_ss.apply({})
libqmp = static_library('qmp', qmp_ss.sources() + genh,
                        dependencies: qmp_ss.dependencies(),
                        build_by_default: false)

qmp = declare_dependency(objects: libqmp.extract_all_objects(recursive: false),
                         dependencies: qmp_ss.dependencies())

libchardev = static_library('chardev', chardev_ss.sources() + genh,
                            dependencies: chardev_ss.dependencies(),
                            build_by_default: false)

chardev = declare_dependency(objects: libchardev.extract_all_objects(recursive: false),
                             dependencies: [chardev_ss.dependencies(), io])

hwcore_ss = hwcore_ss.apply({})
libhwcore = static_library('hwcore', sources: hwcore_ss.sources() + genh,
                           build_by_default: false)
hwcore = declare_dependency(objects: libhwcore.extract_all_objects(recursive: false))
common_ss.add(hwcore)


system_ss.add(blockdev, chardev, io, qmp)
common_ss.add(qom, qemuutil)

common_ss.add_all(when: 'CONFIG_SYSTEM_ONLY', if_true: [system_ss])
common_ss.add_all(when: 'CONFIG_USER_ONLY', if_true: user_ss)

common_all = static_library('common',
                            build_by_default: false,
                            sources: common_ss.all_sources() + genh,
                            include_directories: common_user_inc,
                            implicit_include_directories: false,
                            dependencies: common_ss.all_dependencies())

if have_rust
  bindgen_args = [
    '--disable-header-comment',
    '--raw-line', '// @generated',
    '--ctypes-prefix', 'std::os::raw',
    '--generate-block',
    '--impl-debug',
    '--no-doc-comments',
    '--with-derive-default',
    '--no-layout-tests',
    '--no-prepend-enum-name',
    '--allowlist-file', meson.project_source_root() + '/include/.*',
    '--allowlist-file', meson.project_source_root() + '/.*',
    '--allowlist-file', meson.project_build_root() + '/.*'
    ]
  if not rustfmt.found()
    if bindgen.version().version_compare('<0.65.0')
      bindgen_args += ['--no-rustfmt-bindings']
    else
      bindgen_args += ['--formatter', 'none']
    endif
  endif
  if bindgen.version().version_compare('>=0.66.0')
    bindgen_args += ['--rust-target', '1.59']
  endif
  if bindgen.version().version_compare('<0.61.0')
    bindgen_args += ['--size_t-is-usize']
  else
    bindgen_args += ['--merge-extern-blocks']
  endif
  c_enums = [
    'DeviceCategory',
    'GpioPolarity',
    'MachineInitPhase',
    'MigrationPolicy',
    'MigrationPriority',
    'QEMUChrEvent',
    'QEMUClockType',
    'ResetType',
    'device_endian',
    'module_init_type',
  ]
  foreach enum : c_enums
    bindgen_args += ['--rustified-enum', enum]
  endforeach
  c_bitfields = [
    'ClockEvent',
    'VMStateFlags',
  ]
  foreach enum : c_bitfields
    bindgen_args += ['--bitfield-enum', enum]
  endforeach

  bindings_rs = rust.bindgen(
    input: 'rust/wrapper.h',
    dependencies: common_ss.all_dependencies(),
    output: 'bindings.inc.rs',
    include_directories: include_directories('.', 'include'),
    bindgen_version: ['>=0.60.0'],
    args: bindgen_args,
    )
  subdir('rust')
endif


if have_rust
  rust_root_crate = find_program('scripts/rust/rust_root_crate.sh')
endif

if host_os == 'darwin'
  entitlement = find_program('scripts/entitlement.sh')
endif

traceable = []
emulators = {}
foreach target : target_dirs
  config_target = config_target_mak[target]
  target_name = config_target['TARGET_NAME']
  target_base_arch = config_target['TARGET_BASE_ARCH']
  arch_srcs = [config_target_h[target]]
  arch_deps = []
  c_args = ['-DCOMPILING_PER_TARGET',
            '-DCONFIG_TARGET="@0@-config-target.h"'.format(target),
  ]
  link_args = emulator_link_args

  target_inc = [include_directories('target' / config_target['TARGET_BASE_ARCH'])]
  if host_os == 'linux'
    target_inc += include_directories('linux-headers', is_system: true)
  endif
  if target.endswith('-softmmu')
    target_type='system'
    t = target_system_arch[target_base_arch].apply(config_target, strict: false)
    arch_srcs += t.sources()
    arch_deps += t.dependencies()

    hw_dir = target_name == 'sparc64' ? 'sparc64' : target_base_arch
    if hw_arch.has_key(hw_dir)
      hw = hw_arch[hw_dir].apply(config_target, strict: false)
      arch_srcs += hw.sources()
      arch_deps += hw.dependencies()
    endif

    c_args += ['-DCONFIG_DEVICES="@0@-config-devices.h"'.format(target)]
    arch_srcs += config_devices_h[target]
    link_args += ['@block.syms', '@qemu.syms']
  else
    abi = config_target['TARGET_ABI_DIR']
    target_type='user'
    target_inc += common_user_inc
    if target_base_arch in target_user_arch
      t = target_user_arch[target_base_arch].apply(config_target, strict: false)
      arch_srcs += t.sources()
      arch_deps += t.dependencies()
    endif
    if 'CONFIG_LINUX_USER' in config_target
      base_dir = 'linux-user'
    endif
    if 'CONFIG_BSD_USER' in config_target
      base_dir = 'bsd-user'
      target_inc += include_directories('bsd-user/' / host_os)
      target_inc += include_directories('bsd-user/host/' / host_arch)
      dir = base_dir / abi
      arch_srcs += files(dir / 'signal.c', dir / 'target_arch_cpu.c')
    endif
    target_inc += include_directories(
      base_dir,
      base_dir / abi,
    )
    if 'CONFIG_LINUX_USER' in config_target
      dir = base_dir / abi
      arch_srcs += files(dir / 'signal.c', dir / 'cpu_loop.c')
      if config_target.has_key('TARGET_SYSTBL_ABI')
        arch_srcs += \
          syscall_nr_generators[abi].process(base_dir / abi / config_target['TARGET_SYSTBL'],
                                             extra_args : config_target['TARGET_SYSTBL_ABI'])
      endif
    endif
  endif

  t = target_arch[target_base_arch].apply(config_target, strict: false)
  arch_srcs += t.sources()
  arch_deps += t.dependencies()

  target_common = common_ss.apply(config_target, strict: false)
  objects = common_all.extract_objects(target_common.sources())
  arch_deps += target_common.dependencies()

  target_specific = specific_ss.apply(config_target, strict: false)
  arch_srcs += target_specific.sources()
  arch_deps += target_specific.dependencies()

  if have_rust and target_type == 'system'
    target_rust = rust_devices_ss.apply(config_target, strict: false)
    crates = []
    foreach dep : target_rust.dependencies()
      crates += dep.get_variable('crate')
    endforeach
    if crates.length() > 0
      rlib_rs = custom_target('rust_' + target.underscorify() + '.rs',
                              output: 'rust_' + target.underscorify() + '.rs',
                              command: [rust_root_crate, crates],
                              capture: true,
                              build_by_default: true,
                              build_always_stale: true)
      rlib = static_library('rust_' + target.underscorify(),
                            rlib_rs,
                            dependencies: target_rust.dependencies(),
                            override_options: ['rust_std=2021', 'build.rust_std=2021'],
                            rust_abi: 'c')
      arch_deps += declare_dependency(link_whole: [rlib])
    endif
  endif

  lib_deps = []
  foreach dep : arch_deps
    lib_deps += dep.partial_dependency(compile_args: true, includes: true)
  endforeach

  lib = static_library('qemu-' + target,
                 sources: arch_srcs + genh,
                 dependencies: lib_deps,
                 objects: objects,
                 include_directories: target_inc,
                 c_args: c_args,
                 build_by_default: false)

  if target.endswith('-softmmu')
    execs = [{
      'name': 'qemu-system-' + target_name,
      'win_subsystem': 'console',
      'sources': files('system/main.c'),
      'dependencies': [sdl]
    }]
    if host_os == 'windows' and sdl.found()
      execs += [{
        'name': 'qemu-system-' + target_name + 'w',
        'win_subsystem': 'windows',
        'sources': files('system/main.c'),
        'dependencies': [sdl]
      }]
    endif
    if get_option('fuzzing')
      specific_fuzz = specific_fuzz_ss.apply(config_target, strict: false)
      execs += [{
        'name': 'qemu-fuzz-' + target_name,
        'win_subsystem': 'console',
        'sources': specific_fuzz.sources(),
        'dependencies': specific_fuzz.dependencies(),
      }]
    endif
  else
    execs = [{
      'name': 'qemu-' + target_name,
      'win_subsystem': 'console',
      'sources': [],
      'dependencies': []
    }]
  endif
  foreach exe: execs
    exe_name = exe['name']
    if host_os == 'darwin'
      exe_name += '-unsigned'
    endif

    emulator = executable(exe_name, exe['sources'],
               install: true,
               c_args: c_args,
               dependencies: arch_deps + exe['dependencies'],
               objects: lib.extract_all_objects(recursive: true),
               link_depends: [block_syms, qemu_syms],
               link_args: link_args,
               win_subsystem: exe['win_subsystem'])

    if host_os == 'darwin'
      icon = 'pc-bios/qemu.rsrc'
      build_input = [emulator, files(icon)]
      install_input = [
        get_option('bindir') / exe_name,
        meson.current_source_dir() / icon
      ]
      emulators += {exe['name'] : custom_target(exe['name'],
                   input: build_input,
                   output: exe['name'],
                   command: [entitlement, '@OUTPUT@', '@INPUT@'])
      }

      meson.add_install_script(entitlement, '--install',
                               get_option('bindir') / exe['name'],
                               install_input)
    else
      emulators += {exe['name']: emulator}
    endif

    traceable += [{
      'exe': exe['name'],
      'probe-prefix': 'qemu.' + target_type + '.' + target_name,
    }]

  endforeach
endforeach


if get_option('plugins')
  install_headers('include/qemu/qemu-plugin.h')
  if host_os == 'windows'
    install_data(win32_qemu_plugin_api_lib, install_dir: 'lib')
  endif
endif

if have_ga
  subdir('qga')
endif

qemu_keymap = []

if have_tools
  error('QEMU tools were removed from this trimmed build')
endif

if stap.found()
  foreach t: traceable
    foreach stp: [
      {'ext': '.stp-build', 'fmt': 'stap', 'bin': meson.current_build_dir() / t['exe'], 'install': false},
      {'ext': '.stp', 'fmt': 'stap', 'bin': get_option('prefix') / get_option('bindir') / t['exe'], 'install': true},
      {'ext': '-simpletrace.stp', 'fmt': 'simpletrace-stap', 'bin': '', 'install': true},
      {'ext': '-log.stp', 'fmt': 'log-stap', 'bin': '', 'install': true},
    ]
      cmd = [
        tracetool, '--group=all', '--format=' + stp['fmt'],
        '--binary=' + stp['bin'],
        '--probe-prefix=' + t['probe-prefix'],
        '@INPUT@', '@OUTPUT@'
      ]

      custom_target(t['exe'] + stp['ext'],
                    input: trace_events_all,
                    output: t['exe'] + stp['ext'],
                    install: stp['install'],
                    install_dir: get_option('datadir') / 'systemtap/tapset',
                    command: cmd,
                    depend_files: tracetool_depends)
    endforeach
  endforeach
endif

subdir('scripts')
if host_machine.system() == 'windows'
  nsis_cmd = [
    find_program('scripts/nsis.py'),
    '@OUTPUT@',
    get_option('prefix'),
    meson.current_source_dir(),
    glib_pc.get_variable('bindir'),
    host_machine.cpu(),
    '--',
    '-DDISPLAYVERSION=' + meson.project_version(),
  ]
  if get_option('docs').enabled()
    nsis_cmd += '-DCONFIG_DOCUMENTATION=y'
  endif
  nsis = custom_target('nsis',
                       output: 'qemu-setup-' + meson.project_version() + '.exe',
                       input: files('qemu.nsi'),
                       build_always_stale: true,
                       command: nsis_cmd + ['@INPUT@'])
  alias_target('installer', nsis)
endif


summary_info = {}
summary_info += {'Build directory':   meson.current_build_dir()}
summary_info += {'Source path':       meson.current_source_dir()}
summary_info += {'Download dependencies': get_option('wrap_mode') != 'nodownload'}
summary(summary_info, bool_yn: true, section: 'Build environment')

summary_info = {}
summary_info += {'python':            '@0@ (version: @1@)'.format(python.full_path(), python.language_version())}

if config_host.has_key('GDB')
  summary_info += {'gdb':             config_host['GDB']}
endif
summary_info += {'iasl':              iasl}
summary_info += {'genisoimage':       config_host['GENISOIMAGE']}
if host_os == 'windows' and have_ga
  summary_info += {'wixl':            wixl}
endif
summary(summary_info, bool_yn: true, section: 'Host binaries')

summary_info = {}
summary_info += {'host CPU':          cpu}
summary_info += {'host endianness':   build_machine.endian()}
summary_info += {'C compiler':        ' '.join(meson.get_compiler('c').cmd_array())}
summary_info += {'Host C compiler':   ' '.join(meson.get_compiler('c', native: true).cmd_array())}
if 'cpp' in all_languages
  summary_info += {'C++ compiler':    ' '.join(meson.get_compiler('cpp').cmd_array())}
else
  summary_info += {'C++ compiler':      false}
endif
if 'objc' in all_languages
  summary_info += {'Objective-C compiler': ' '.join(meson.get_compiler('objc').cmd_array())}
else
  summary_info += {'Objective-C compiler': false}
endif
summary_info += {'Rust support':      have_rust}
if have_rust
  summary_info += {'Rust target':     config_host['RUST_TARGET_TRIPLE']}
  summary_info += {'rustc':           ' '.join(rustc.cmd_array())}
  summary_info += {'rustc version':   rustc.version()}
  summary_info += {'bindgen':         bindgen.full_path()}
  summary_info += {'bindgen version': bindgen.version()}
endif
option_cflags = (get_option('debug') ? ['-g'] : [])
if get_option('optimization') != 'plain'
  option_cflags += ['-O' + get_option('optimization')]
endif
summary_info += {'CFLAGS':            ' '.join(get_option('c_args') + option_cflags)}
if 'cpp' in all_languages
  summary_info += {'CXXFLAGS':        ' '.join(get_option('cpp_args') + option_cflags)}
endif
if 'objc' in all_languages
  summary_info += {'OBJCFLAGS':       ' '.join(get_option('objc_args') + option_cflags)}
endif
link_args = get_option('c_link_args')
if link_args.length() > 0
  summary_info += {'LDFLAGS':         ' '.join(link_args)}
endif
summary_info += {'QEMU_CFLAGS':       ' '.join(qemu_common_flags + qemu_cflags)}
if 'cpp' in all_languages
  summary_info += {'QEMU_CXXFLAGS':     ' '.join(qemu_common_flags + qemu_cxxflags)}
endif
if 'objc' in all_languages
  summary_info += {'QEMU_OBJCFLAGS':    ' '.join(qemu_common_flags)}
endif
summary_info += {'QEMU_LDFLAGS':      ' '.join(qemu_ldflags)}
summary_info += {'link-time optimization (LTO)': get_option('b_lto')}
summary_info += {'PIE':               get_option('b_pie')}
summary_info += {'static build':      get_option('prefer_static')}
summary_info += {'malloc trim support': has_malloc_trim}
summary_info += {'membarrier':        have_membarrier}
summary_info += {'debug graph lock':  get_option('debug_graph_lock')}
summary_info += {'debug stack usage': get_option('debug_stack_usage')}
summary_info += {'mutex debugging':   get_option('debug_mutex')}
summary_info += {'memory allocator':  get_option('malloc')}
summary_info += {'avx2 optimization': config_host_data.get('CONFIG_AVX2_OPT')}
summary_info += {'avx512bw optimization': config_host_data.get('CONFIG_AVX512BW_OPT')}
summary_info += {'gcov':              get_option('b_coverage')}
summary_info += {'thread sanitizer':  get_option('tsan')}
summary_info += {'CFI support':       get_option('cfi')}
if get_option('cfi')
  summary_info += {'CFI debug support': get_option('cfi_debug')}
endif
summary_info += {'strip binaries':    get_option('strip')}
summary_info += {'sparse':            sparse}
summary_info += {'mingw32 support':   host_os == 'windows'}
summary(summary_info, bool_yn: true, section: 'Compilation')

summary_info = {}
if have_system
  summary_info += {'GH support':        config_all_accel.has_key('CONFIG_GH')}
  summary_info += {'Xen support':       xen.found()}
  if xen.found()
    summary_info += {'xen ctrl version':  xen.version()}
  endif
  summary_info += {'Xen emulation':     config_all_devices.has_key('CONFIG_XEN_EMU')}
endif
summary_info += {'TCG support':       config_all_accel.has_key('CONFIG_TCG')}
if config_all_accel.has_key('CONFIG_TCG')
  if get_option('tcg_interpreter')
    summary_info += {'TCG backend':   'TCI (TCG with bytecode interpreter, slow)'}
  else
    summary_info += {'TCG backend':   'native (@0@)'.format(cpu)}
  endif
  summary_info += {'TCG plugins':       get_option('plugins')}
  summary_info += {'TCG debug enabled': get_option('debug_tcg')}
  if have_linux_user or have_bsd_user
    summary_info += {'syscall buffer debugging support': get_option('debug_remap')}
  endif
endif
summary_info += {'target list':       ' '.join(target_dirs)}
if have_system
  summary_info += {'default devices':   get_option('default_devices')}
  summary_info += {'out of process emulation': multiprocess_allowed}
endif
summary(summary_info, bool_yn: true, section: 'Targets and accelerators')

summary_info = {}
summary_info += {'coroutine backend': coroutine_backend}
summary_info += {'coroutine pool':    have_coroutine_pool}
if have_block
  summary_info += {'Block whitelist (rw)': get_option('block_drv_rw_whitelist')}
  summary_info += {'Block whitelist (ro)': get_option('block_drv_ro_whitelist')}
  summary_info += {'Use block whitelist in tools': get_option('block_drv_whitelist_in_tools')}
  summary_info += {'VirtFS (9P) support':    have_virtfs}
  summary_info += {'replication support': config_host_data.get('CONFIG_REPLICATION')}
endif
summary(summary_info, bool_yn: true, section: 'Block layer support')

summary_info = {}
summary_info += {'vde support':       vde}
summary_info += {'l2tpv3 support':    have_l2tpv3}
summary(summary_info, bool_yn: true, section: 'Network backends')

summary_info = {}
if host_os == 'darwin'
  summary_info += {'Cocoa support':           cocoa}
endif
summary_info += {'SDL support':       sdl}
summary_info += {'pixman':            pixman}
summary_info += {'PNG support':       png}
summary_info += {'spice protocol support': spice_protocol}
if spice_protocol.found()
  summary_info += {'  spice server support': spice}
endif
summary_info += {'curses support':    curses}
summary_info += {'brlapi support':    brlapi}
summary(summary_info, bool_yn: true, section: 'User interface')

summary_info = {}
summary_info += {'VirGL support':     virgl}
summary(summary_info, bool_yn: true, section: 'Graphics backends')

summary_info = {}
if host_os not in ['darwin', 'haiku', 'windows']
  summary_info += {'OSS support':     oss}
  summary_info += {'sndio support':   sndio}
elif host_os == 'darwin'
  summary_info += {'CoreAudio support': coreaudio}
elif host_os == 'windows'
  summary_info += {'DirectSound support': dsound}
endif
if host_os == 'linux'
  summary_info += {'ALSA support':    alsa}
  summary_info += {'PulseAudio support': pulse}
endif
summary_info += {'PipeWire support':  pipewire}
summary_info += {'JACK support':      jack}
summary(summary_info, bool_yn: true, section: 'Audio backends')

summary_info = {}
summary_info += {'TLS priority':      get_option('tls_priority')}
summary_info += {'GNUTLS support':    gnutls}
if gnutls.found()
  summary_info += {'  GNUTLS crypto':   gnutls_crypto.found()}
endif
summary_info += {'libgcrypt':         gcrypt}
summary_info += {'nettle':            nettle}
if nettle.found()
   summary_info += {'  XTS':             xts != 'private'}
endif
summary_info += {'SM4 ALG support':   crypto_sm4}
summary_info += {'SM3 ALG support':   crypto_sm3}
summary_info += {'AF_ALG support':    have_afalg}
summary_info += {'rng-none':          get_option('rng_none')}
summary(summary_info, bool_yn: true, section: 'Crypto')

summary_info = {}
summary_info += {'libtasn1':          tasn1}
summary_info += {'iconv support':     iconv}
summary_info += {'blkio support':     blkio}
summary_info += {'curl support':      curl}
summary_info += {'Linux io_uring support': linux_io_uring}
summary_info += {'ATTR/XATTR support': libattr}
summary_info += {'fdt support':       fdt_opt == 'internal' ? 'internal' : fdt}
summary_info += {'libcap-ng support': libcap_ng}
summary_info += {'smartcard support': cacard}
summary_info += {'U2F support':       u2f}
summary_info += {'libusb':            libusb}
summary_info += {'OpenGL support (epoxy)': opengl}
summary_info += {'GBM':               gbm}
if host_os == 'windows'
  if have_ga
    summary_info += {'QGA VSS support':   have_qga_vss}
  endif
endif
summary_info += {'seccomp support':   seccomp}
summary_info += {'lzo support':       lzo}
summary_info += {'snappy support':    snappy}
summary_info += {'bzip2 support':     libbzip2}
summary_info += {'lzfse support':     liblzfse}
summary_info += {'zstd support':      zstd}
summary_info += {'Query Processing Library support': qpl}
summary_info += {'UADK Library support': uadk}
summary_info += {'qatzip support':    qatzip}
summary_info += {'capstone':          capstone}
summary_info += {'libpmem support':   libpmem}
summary_info += {'libdaxctl support': libdaxctl}
summary_info += {'libcbor support':   libcbor}
summary_info += {'libudev':           libudev}
summary_info += {'libdw':             libdw}
if host_os == 'freebsd'
  summary_info += {'libinotify-kqueue': inotify}
endif
if host_os == 'darwin'
  summary_info += {'ParavirtualizedGraphics support': pvg}
endif
summary(summary_info, bool_yn: true, section: 'Dependencies')

summary_info = {}
summary_info += {'Documentation':     get_option('docs').enabled()}
summary_info += {'system-mode emulation': have_system}
summary_info += {'user-mode emulation': have_user}
summary_info += {'block layer':       have_block}
summary_info += {'Install blobs':     get_option('install_blobs')}
summary_info += {'module support':    enable_modules}
if enable_modules
  summary_info += {'alternative module path': get_option('module_upgrades')}
endif
summary_info += {'fuzzing support':   get_option('fuzzing')}
if have_system
  summary_info += {'Audio drivers':     ' '.join(audio_drivers_selected)}
endif
summary_info += {'Trace backends':    ','.join(get_option('trace_backends'))}
if 'simple' in get_option('trace_backends')
  summary_info += {'Trace output file': get_option('trace_file') + '-<pid>'}
endif
summary_info += {'D-Bus display':     dbus_display}
summary_info += {'QOM debugging':     get_option('qom_cast_debug')}
summary_info += {'Relocatable install': get_option('relocatable')}
summary_info += {'build guest agent': have_ga}
summary(summary_info, bool_yn: true, section: 'Configurable features')

summary_info = {}
summary_info += {'Install prefix':    get_option('prefix')}
summary_info += {'BIOS directory':    qemu_datadir}
pathsep = host_os == 'windows' ? ';' : ':'
summary_info += {'firmware path':     pathsep.join(get_option('qemu_firmwarepath'))}
summary_info += {'binary directory':  get_option('prefix') / get_option('bindir')}
summary_info += {'library directory': get_option('prefix') / get_option('libdir')}
summary_info += {'module directory':  qemu_moddir}
summary_info += {'libexec directory': get_option('prefix') / get_option('libexecdir')}
summary_info += {'include directory': get_option('prefix') / get_option('includedir')}
summary_info += {'config directory':  get_option('prefix') / get_option('sysconfdir')}
if host_os != 'windows'
  summary_info += {'local state directory': get_option('prefix') / get_option('localstatedir')}
  summary_info += {'Manual directory':      get_option('prefix') / get_option('mandir')}
else
  summary_info += {'local state directory': 'queried at runtime'}
endif
summary_info += {'Doc directory':     get_option('prefix') / get_option('docdir')}
summary(summary_info, bool_yn: true, section: 'Directories')


if host_arch == 'unknown'
  message()
  warning('UNSUPPORTED HOST CPU')
  message()
  message('Support for CPU host architecture ' + cpu + ' is not currently')
  message('maintained. The QEMU project does not guarantee that QEMU will')
  message('compile or work on this host CPU. You can help by volunteering')
  message('to maintain it and providing a build host for our continuous')
  message('integration setup.')
  if get_option('tcg').allowed() and target_dirs.length() > 0
    message()
    message('configure has succeeded and you can continue to build, but')
    message('QEMU will use a slow interpreter to emulate the target CPU.')
  endif
elif host_long_bits < 64
  message()
  warning('DEPRECATED HOST CPU')
  message()
  message('Support for 32-bit CPU host architecture ' + cpu + ' is going')
  message('to be dropped in a future QEMU release.')
endif

if not supported_oses.contains(host_os)
  message()
  warning('UNSUPPORTED HOST OS')
  message()
  message('Support for host OS ' + host_os + 'is not currently maintained.')
  message('configure has succeeded and you can continue to build, but')
  message('the QEMU project does not guarantee that QEMU will compile or')
  message('work on this operating system. You can help by volunteering')
  message('to maintain it and providing a build host for our continuous')
  message('integration setup. This will ensure that future versions of QEMU')
  message('will keep working on ' + host_os + '.')
endif

if host_arch == 'unknown' or not supported_oses.contains(host_os)
  message()
  message('If you want to help supporting QEMU on this platform, please')
  message('contact the developers at qemu-devel@nongnu.org.')
endif

actually_reloc = get_option('relocatable')
if get_option('relocatable') and \
  not (get_option('prefix') / get_option('bindir')).startswith(get_option('prefix') / '')
  message()
  warning('bindir not included within prefix, the installation will not be relocatable.')
  actually_reloc = false
endif
if not actually_reloc and (host_os == 'windows' or get_option('relocatable'))
  if host_os == 'windows'
    message()
    warning('Windows installs should usually be relocatable.')
  endif
  message()
  message('QEMU will have to be installed under ' + get_option('prefix') + '.')
  message('Use --disable-relocatable to remove this warning.')
endif
