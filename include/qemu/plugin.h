#ifndef QEMU_PLUGIN_H
#define QEMU_PLUGIN_H

#include "qemu/config-file.h"
#include "qemu/qemu-plugin.h"
#include "qemu/error-report.h"
#include "qemu/queue.h"
#include "qemu/option.h"
#include "qemu/plugin-event.h"
#include "qemu/bitmap.h"
#include "exec/memopidx.h"
#include "hw/core/cpu.h"

struct qemu_plugin_desc;
typedef QTAILQ_HEAD(, qemu_plugin_desc) QemuPluginList;

static inline qemu_plugin_meminfo_t
make_plugin_meminfo(MemOpIdx oi, enum qemu_plugin_mem_rw rw)
{
    return oi | (rw << 16);
}

static inline enum qemu_plugin_mem_rw
get_plugin_meminfo_rw(qemu_plugin_meminfo_t i)
{
    return i >> 16;
}

#ifdef CONFIG_PLUGIN
extern QemuOptsList qemu_plugin_opts;

static inline void qemu_plugin_add_opts(void)
{
    qemu_add_opts(&qemu_plugin_opts);
}

void qemu_plugin_opt_parse(const char *optstr, QemuPluginList *head);
int qemu_plugin_load_list(QemuPluginList *head, Error **errp);

union qemu_plugin_cb_sig {
    qemu_plugin_simple_cb_t          simple;
    qemu_plugin_udata_cb_t           udata;
    qemu_plugin_vcpu_simple_cb_t     vcpu_simple;
    qemu_plugin_vcpu_udata_cb_t      vcpu_udata;
    qemu_plugin_vcpu_tb_trans_cb_t   vcpu_tb_trans;
    qemu_plugin_vcpu_mem_cb_t        vcpu_mem;
    qemu_plugin_vcpu_syscall_cb_t    vcpu_syscall;
    qemu_plugin_vcpu_syscall_ret_cb_t vcpu_syscall_ret;
    void *generic;
};

enum plugin_dyn_cb_type {
    PLUGIN_CB_REGULAR,
    PLUGIN_CB_COND,
    PLUGIN_CB_MEM_REGULAR,
    PLUGIN_CB_INLINE_ADD_U64,
    PLUGIN_CB_INLINE_STORE_U64,
};

struct qemu_plugin_regular_cb {
    union qemu_plugin_cb_sig f;
    TCGHelperInfo *info;
    void *userp;
    enum qemu_plugin_mem_rw rw;
};

struct qemu_plugin_inline_cb {
    qemu_plugin_u64 entry;
    uint64_t imm;
    enum qemu_plugin_mem_rw rw;
};

struct qemu_plugin_conditional_cb {
    union qemu_plugin_cb_sig f;
    TCGHelperInfo *info;
    void *userp;
    qemu_plugin_u64 entry;
    enum qemu_plugin_cond cond;
    uint64_t imm;
};

struct qemu_plugin_dyn_cb {
    enum plugin_dyn_cb_type type;
    union {
        struct qemu_plugin_regular_cb regular;
        struct qemu_plugin_conditional_cb cond;
        struct qemu_plugin_inline_cb inline_insn;
    };
};

struct qemu_plugin_insn {
    uint64_t vaddr;
    GArray *insn_cbs;
    GArray *mem_cbs;
    uint8_t len;
    bool calls_helpers;

    bool mem_helper;
};

struct qemu_plugin_scoreboard {
    GArray *data;
    QLIST_ENTRY(qemu_plugin_scoreboard) entry;
};

struct qemu_plugin_tb {
    GPtrArray *insns;
    size_t n;

    bool mem_helper;

    GArray *cbs;
};

struct CPUPluginState {
    DECLARE_BITMAP(event_mask, QEMU_PLUGIN_EV_MAX);
};

CPUPluginState *qemu_plugin_create_vcpu_state(void);

void qemu_plugin_vcpu_init_hook(CPUState *cpu);
void qemu_plugin_vcpu_exit_hook(CPUState *cpu);
void qemu_plugin_tb_trans_cb(CPUState *cpu, struct qemu_plugin_tb *tb);
void qemu_plugin_vcpu_idle_cb(CPUState *cpu);
void qemu_plugin_vcpu_resume_cb(CPUState *cpu);
void
qemu_plugin_vcpu_syscall(CPUState *cpu, int64_t num, uint64_t a1,
                         uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                         uint64_t a6, uint64_t a7, uint64_t a8);
void qemu_plugin_vcpu_syscall_ret(CPUState *cpu, int64_t num, int64_t ret);

void qemu_plugin_vcpu_mem_cb(CPUState *cpu, uint64_t vaddr,
                             uint64_t value_low,
                             uint64_t value_high,
                             MemOpIdx oi, enum qemu_plugin_mem_rw rw);

void qemu_plugin_flush_cb(void);

void qemu_plugin_atexit_cb(void);

void qemu_plugin_add_dyn_cb_arr(GArray *arr);

static inline void qemu_plugin_disable_mem_helpers(CPUState *cpu)
{
    cpu->neg.plugin_mem_cbs = NULL;
}

void qemu_plugin_user_exit(void);

void qemu_plugin_user_prefork_lock(void);

void qemu_plugin_user_postfork(bool is_child);

#else /* !CONFIG_PLUGIN */

static inline void qemu_plugin_add_opts(void)
{ }

static inline void qemu_plugin_opt_parse(const char *optstr,
                                         QemuPluginList *head)
{
    error_report("plugin interface not enabled in this build");
    exit(1);
}

static inline int qemu_plugin_load_list(QemuPluginList *head, Error **errp)
{
    return 0;
}

static inline void qemu_plugin_vcpu_init_hook(CPUState *cpu)
{ }

static inline void qemu_plugin_vcpu_exit_hook(CPUState *cpu)
{ }

static inline void qemu_plugin_tb_trans_cb(CPUState *cpu,
                                           struct qemu_plugin_tb *tb)
{ }

static inline void qemu_plugin_vcpu_idle_cb(CPUState *cpu)
{ }

static inline void qemu_plugin_vcpu_resume_cb(CPUState *cpu)
{ }

static inline void
qemu_plugin_vcpu_syscall(CPUState *cpu, int64_t num, uint64_t a1, uint64_t a2,
                         uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6,
                         uint64_t a7, uint64_t a8)
{ }

static inline
void qemu_plugin_vcpu_syscall_ret(CPUState *cpu, int64_t num, int64_t ret)
{ }

static inline void qemu_plugin_vcpu_mem_cb(CPUState *cpu, uint64_t vaddr,
                                           uint64_t value_low,
                                           uint64_t value_high,
                                           MemOpIdx oi,
                                           enum qemu_plugin_mem_rw rw)
{ }

static inline void qemu_plugin_flush_cb(void)
{ }

static inline void qemu_plugin_atexit_cb(void)
{ }

static inline
void qemu_plugin_add_dyn_cb_arr(GArray *arr)
{ }

static inline void qemu_plugin_disable_mem_helpers(CPUState *cpu)
{ }

static inline void qemu_plugin_user_exit(void)
{ }

static inline void qemu_plugin_user_prefork_lock(void)
{ }

static inline void qemu_plugin_user_postfork(bool is_child)
{ }

#endif /* !CONFIG_PLUGIN */

#endif /* QEMU_PLUGIN_H */
