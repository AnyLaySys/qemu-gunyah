
#ifndef QEMU_MODULE_H
#define QEMU_MODULE_H


#define DSO_STAMP_FUN         glue(qemu_stamp, CONFIG_STAMP)
#define DSO_STAMP_FUN_STR     stringify(DSO_STAMP_FUN)

#ifdef BUILD_DSO
void DSO_STAMP_FUN(void);
void qemu_module_dummy(void);

#define module_init(function, type)                                         \
static void __attribute__((constructor)) do_qemu_init_ ## function(void)    \
{                                                                           \
    register_dso_module_init(function, type);                               \
}
#else
#define module_init(function, type)                                         \
static void __attribute__((constructor)) do_qemu_init_ ## function(void)    \
{                                                                           \
    register_module_init(function, type);                                   \
}
#endif

typedef enum {
    MODULE_INIT_MIGRATION,
    MODULE_INIT_BLOCK,
    MODULE_INIT_OPTS,
    MODULE_INIT_QOM,
    MODULE_INIT_TRACE,
    MODULE_INIT_XEN_BACKEND,
    MODULE_INIT_LIBQOS,
    MODULE_INIT_FUZZ_TARGET,
    MODULE_INIT_MAX
} module_init_type;

#define block_init(function) module_init(function, MODULE_INIT_BLOCK)
#define opts_init(function) module_init(function, MODULE_INIT_OPTS)
#define type_init(function) module_init(function, MODULE_INIT_QOM)
#define trace_init(function) module_init(function, MODULE_INIT_TRACE)
#define xen_backend_init(function) module_init(function, \
                                               MODULE_INIT_XEN_BACKEND)
#define libqos_init(function) module_init(function, MODULE_INIT_LIBQOS)
#define fuzz_target_init(function) module_init(function, \
                                               MODULE_INIT_FUZZ_TARGET)
#define migration_init(function) module_init(function, MODULE_INIT_MIGRATION)
#define block_module_load(lib, errp) module_load("block-", lib, errp)
#define ui_module_load(lib, errp) module_load("ui-", lib, errp)
#define audio_module_load(lib, errp) module_load("audio-", lib, errp)

void register_module_init(void (*fn)(void), module_init_type type);
void register_dso_module_init(void (*fn)(void), module_init_type type);

void module_call_init(module_init_type type);

int module_load(const char *prefix, const char *name, Error **errp);

int module_load_qom(const char *type, Error **errp);
void module_load_qom_all(void);
void module_allow_arch(const char *arch);

#ifdef QEMU_MODINFO
# define modinfo(kind, value) \
    MODINFO_START kind value MODINFO_END
#else
# define modinfo(kind, value)
#endif

#define module_obj(name) modinfo(obj, name)

#define module_dep(name) modinfo(dep, name)

#define module_arch(name) modinfo(arch, name)

#define module_opts(name) modinfo(opts, name)

#define module_kconfig(name) modinfo(kconfig, name)

typedef struct QemuModinfo QemuModinfo;
struct QemuModinfo {
    const char *name;
    const char *arch;
    const char **objs;
    const char **deps;
    const char **opts;
};
extern const QemuModinfo qemu_modinfo[];
void module_init_info(const QemuModinfo *info);

#endif
