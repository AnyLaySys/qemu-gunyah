
#ifndef QEMU_QEMU_PLUGIN_H
#define QEMU_QEMU_PLUGIN_H

#include <glib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

#if defined _WIN32 || defined __CYGWIN__
  #ifdef CONFIG_PLUGIN
    #define QEMU_PLUGIN_EXPORT __declspec(dllimport)
    #define QEMU_PLUGIN_API __declspec(dllexport)
  #else
    #define QEMU_PLUGIN_EXPORT __declspec(dllexport)
    #define QEMU_PLUGIN_API __declspec(dllimport)
  #endif
  #define QEMU_PLUGIN_LOCAL
#else
  #define QEMU_PLUGIN_EXPORT __attribute__((visibility("default")))
  #define QEMU_PLUGIN_LOCAL  __attribute__((visibility("hidden")))
  #define QEMU_PLUGIN_API
#endif

typedef uint64_t qemu_plugin_id_t;


extern QEMU_PLUGIN_EXPORT int qemu_plugin_version;

#define QEMU_PLUGIN_VERSION 4

typedef struct qemu_info_t {
    const char *target_name;
    struct {
        int min;
        int cur;
    } version;
    bool system_emulation;
    union {
        struct {
            int smp_vcpus;
            int max_vcpus;
        } system;
    };
} qemu_info_t;

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info,
                                           int argc, char **argv);

typedef void (*qemu_plugin_simple_cb_t)(qemu_plugin_id_t id);

typedef void (*qemu_plugin_udata_cb_t)(qemu_plugin_id_t id, void *userdata);

typedef void (*qemu_plugin_vcpu_simple_cb_t)(qemu_plugin_id_t id,
                                             unsigned int vcpu_index);

typedef void (*qemu_plugin_vcpu_udata_cb_t)(unsigned int vcpu_index,
                                            void *userdata);

QEMU_PLUGIN_API
void qemu_plugin_uninstall(qemu_plugin_id_t id, qemu_plugin_simple_cb_t cb);

QEMU_PLUGIN_API
void qemu_plugin_reset(qemu_plugin_id_t id, qemu_plugin_simple_cb_t cb);

QEMU_PLUGIN_API
void qemu_plugin_register_vcpu_init_cb(qemu_plugin_id_t id,
                                       qemu_plugin_vcpu_simple_cb_t cb);

QEMU_PLUGIN_API
void qemu_plugin_register_vcpu_exit_cb(qemu_plugin_id_t id,
                                       qemu_plugin_vcpu_simple_cb_t cb);

QEMU_PLUGIN_API
void qemu_plugin_register_vcpu_idle_cb(qemu_plugin_id_t id,
                                       qemu_plugin_vcpu_simple_cb_t cb);

QEMU_PLUGIN_API
void qemu_plugin_register_vcpu_resume_cb(qemu_plugin_id_t id,
                                         qemu_plugin_vcpu_simple_cb_t cb);

struct qemu_plugin_tb;
struct qemu_plugin_insn;
struct qemu_plugin_scoreboard;

typedef struct {
    struct qemu_plugin_scoreboard *score;
    size_t offset;
} qemu_plugin_u64;

enum qemu_plugin_cb_flags {
    QEMU_PLUGIN_CB_NO_REGS,
    QEMU_PLUGIN_CB_R_REGS,
    QEMU_PLUGIN_CB_RW_REGS,
};

enum qemu_plugin_mem_rw {
    QEMU_PLUGIN_MEM_R = 1,
    QEMU_PLUGIN_MEM_W,
    QEMU_PLUGIN_MEM_RW,
};

enum qemu_plugin_mem_value_type {
    QEMU_PLUGIN_MEM_VALUE_U8,
    QEMU_PLUGIN_MEM_VALUE_U16,
    QEMU_PLUGIN_MEM_VALUE_U32,
    QEMU_PLUGIN_MEM_VALUE_U64,
    QEMU_PLUGIN_MEM_VALUE_U128,
};

typedef struct {
    enum qemu_plugin_mem_value_type type;
    union {
        uint8_t u8;
        uint16_t u16;
        uint32_t u32;
        uint64_t u64;
        struct {
            uint64_t low;
            uint64_t high;
        } u128;
    } data;
} qemu_plugin_mem_value;

enum qemu_plugin_cond {
    QEMU_PLUGIN_COND_NEVER,
    QEMU_PLUGIN_COND_ALWAYS,
    QEMU_PLUGIN_COND_EQ,
    QEMU_PLUGIN_COND_NE,
    QEMU_PLUGIN_COND_LT,
    QEMU_PLUGIN_COND_LE,
    QEMU_PLUGIN_COND_GT,
    QEMU_PLUGIN_COND_GE,
};

typedef void (*qemu_plugin_vcpu_tb_trans_cb_t)(qemu_plugin_id_t id,
                                               struct qemu_plugin_tb *tb);

QEMU_PLUGIN_API
void qemu_plugin_register_vcpu_tb_trans_cb(qemu_plugin_id_t id,
                                           qemu_plugin_vcpu_tb_trans_cb_t cb);

QEMU_PLUGIN_API
void qemu_plugin_register_vcpu_tb_exec_cb(struct qemu_plugin_tb *tb,
                                          qemu_plugin_vcpu_udata_cb_t cb,
                                          enum qemu_plugin_cb_flags flags,
                                          void *userdata);

QEMU_PLUGIN_API
void qemu_plugin_register_vcpu_tb_exec_cond_cb(struct qemu_plugin_tb *tb,
                                               qemu_plugin_vcpu_udata_cb_t cb,
                                               enum qemu_plugin_cb_flags flags,
                                               enum qemu_plugin_cond cond,
                                               qemu_plugin_u64 entry,
                                               uint64_t imm,
                                               void *userdata);


enum qemu_plugin_op {
    QEMU_PLUGIN_INLINE_ADD_U64,
    QEMU_PLUGIN_INLINE_STORE_U64,
};

QEMU_PLUGIN_API
void qemu_plugin_register_vcpu_tb_exec_inline_per_vcpu(
    struct qemu_plugin_tb *tb,
    enum qemu_plugin_op op,
    qemu_plugin_u64 entry,
    uint64_t imm);

QEMU_PLUGIN_API
void qemu_plugin_register_vcpu_insn_exec_cb(struct qemu_plugin_insn *insn,
                                            qemu_plugin_vcpu_udata_cb_t cb,
                                            enum qemu_plugin_cb_flags flags,
                                            void *userdata);

QEMU_PLUGIN_API
void qemu_plugin_register_vcpu_insn_exec_cond_cb(
    struct qemu_plugin_insn *insn,
    qemu_plugin_vcpu_udata_cb_t cb,
    enum qemu_plugin_cb_flags flags,
    enum qemu_plugin_cond cond,
    qemu_plugin_u64 entry,
    uint64_t imm,
    void *userdata);

QEMU_PLUGIN_API
void qemu_plugin_register_vcpu_insn_exec_inline_per_vcpu(
    struct qemu_plugin_insn *insn,
    enum qemu_plugin_op op,
    qemu_plugin_u64 entry,
    uint64_t imm);

QEMU_PLUGIN_API
size_t qemu_plugin_tb_n_insns(const struct qemu_plugin_tb *tb);

QEMU_PLUGIN_API
uint64_t qemu_plugin_tb_vaddr(const struct qemu_plugin_tb *tb);

QEMU_PLUGIN_API
struct qemu_plugin_insn *
qemu_plugin_tb_get_insn(const struct qemu_plugin_tb *tb, size_t idx);

QEMU_PLUGIN_API
size_t qemu_plugin_insn_data(const struct qemu_plugin_insn *insn,
                             void *dest, size_t len);

QEMU_PLUGIN_API
size_t qemu_plugin_insn_size(const struct qemu_plugin_insn *insn);

QEMU_PLUGIN_API
uint64_t qemu_plugin_insn_vaddr(const struct qemu_plugin_insn *insn);

QEMU_PLUGIN_API
void *qemu_plugin_insn_haddr(const struct qemu_plugin_insn *insn);

typedef uint32_t qemu_plugin_meminfo_t;
struct qemu_plugin_hwaddr;

QEMU_PLUGIN_API
unsigned int qemu_plugin_mem_size_shift(qemu_plugin_meminfo_t info);
QEMU_PLUGIN_API
bool qemu_plugin_mem_is_sign_extended(qemu_plugin_meminfo_t info);
QEMU_PLUGIN_API
bool qemu_plugin_mem_is_big_endian(qemu_plugin_meminfo_t info);
QEMU_PLUGIN_API
bool qemu_plugin_mem_is_store(qemu_plugin_meminfo_t info);

QEMU_PLUGIN_API
qemu_plugin_mem_value qemu_plugin_mem_get_value(qemu_plugin_meminfo_t info);

QEMU_PLUGIN_API
struct qemu_plugin_hwaddr *qemu_plugin_get_hwaddr(qemu_plugin_meminfo_t info,
                                                  uint64_t vaddr);


QEMU_PLUGIN_API
bool qemu_plugin_hwaddr_is_io(const struct qemu_plugin_hwaddr *haddr);

QEMU_PLUGIN_API
uint64_t qemu_plugin_hwaddr_phys_addr(const struct qemu_plugin_hwaddr *haddr);

QEMU_PLUGIN_API
const char *qemu_plugin_hwaddr_device_name(const struct qemu_plugin_hwaddr *h);

typedef void (*qemu_plugin_vcpu_mem_cb_t) (unsigned int vcpu_index,
                                           qemu_plugin_meminfo_t info,
                                           uint64_t vaddr,
                                           void *userdata);

QEMU_PLUGIN_API
void qemu_plugin_register_vcpu_mem_cb(struct qemu_plugin_insn *insn,
                                      qemu_plugin_vcpu_mem_cb_t cb,
                                      enum qemu_plugin_cb_flags flags,
                                      enum qemu_plugin_mem_rw rw,
                                      void *userdata);

QEMU_PLUGIN_API
void qemu_plugin_register_vcpu_mem_inline_per_vcpu(
    struct qemu_plugin_insn *insn,
    enum qemu_plugin_mem_rw rw,
    enum qemu_plugin_op op,
    qemu_plugin_u64 entry,
    uint64_t imm);

QEMU_PLUGIN_API
const void *qemu_plugin_request_time_control(void);

QEMU_PLUGIN_API
void qemu_plugin_update_ns(const void *handle, int64_t time);

typedef void
(*qemu_plugin_vcpu_syscall_cb_t)(qemu_plugin_id_t id, unsigned int vcpu_index,
                                 int64_t num, uint64_t a1, uint64_t a2,
                                 uint64_t a3, uint64_t a4, uint64_t a5,
                                 uint64_t a6, uint64_t a7, uint64_t a8);

QEMU_PLUGIN_API
void qemu_plugin_register_vcpu_syscall_cb(qemu_plugin_id_t id,
                                          qemu_plugin_vcpu_syscall_cb_t cb);

typedef void
(*qemu_plugin_vcpu_syscall_ret_cb_t)(qemu_plugin_id_t id, unsigned int vcpu_idx,
                                     int64_t num, int64_t ret);

QEMU_PLUGIN_API
void
qemu_plugin_register_vcpu_syscall_ret_cb(qemu_plugin_id_t id,
                                         qemu_plugin_vcpu_syscall_ret_cb_t cb);



QEMU_PLUGIN_API
char *qemu_plugin_insn_disas(const struct qemu_plugin_insn *insn);

QEMU_PLUGIN_API
const char *qemu_plugin_insn_symbol(const struct qemu_plugin_insn *insn);

QEMU_PLUGIN_API
void qemu_plugin_vcpu_for_each(qemu_plugin_id_t id,
                               qemu_plugin_vcpu_simple_cb_t cb);

QEMU_PLUGIN_API
void qemu_plugin_register_flush_cb(qemu_plugin_id_t id,
                                   qemu_plugin_simple_cb_t cb);

QEMU_PLUGIN_API
void qemu_plugin_register_atexit_cb(qemu_plugin_id_t id,
                                    qemu_plugin_udata_cb_t cb, void *userdata);

QEMU_PLUGIN_API
int qemu_plugin_num_vcpus(void);

QEMU_PLUGIN_API
void qemu_plugin_outs(const char *string);

QEMU_PLUGIN_API
bool qemu_plugin_bool_parse(const char *name, const char *val, bool *ret);

QEMU_PLUGIN_API
const char *qemu_plugin_path_to_binary(void);

QEMU_PLUGIN_API
uint64_t qemu_plugin_start_code(void);

QEMU_PLUGIN_API
uint64_t qemu_plugin_end_code(void);

QEMU_PLUGIN_API
uint64_t qemu_plugin_entry_code(void);

struct qemu_plugin_register;

typedef struct {
    struct qemu_plugin_register *handle;
    const char *name;
    const char *feature;
} qemu_plugin_reg_descriptor;

QEMU_PLUGIN_API
GArray *qemu_plugin_get_registers(void);

QEMU_PLUGIN_API
bool qemu_plugin_read_memory_vaddr(uint64_t addr,
                                   GByteArray *data, size_t len);

QEMU_PLUGIN_API
int qemu_plugin_read_register(struct qemu_plugin_register *handle,
                              GByteArray *buf);

QEMU_PLUGIN_API
struct qemu_plugin_scoreboard *qemu_plugin_scoreboard_new(size_t element_size);

QEMU_PLUGIN_API
void qemu_plugin_scoreboard_free(struct qemu_plugin_scoreboard *score);

QEMU_PLUGIN_API
void *qemu_plugin_scoreboard_find(struct qemu_plugin_scoreboard *score,
                                  unsigned int vcpu_index);

#define qemu_plugin_scoreboard_u64(score) \
    (qemu_plugin_u64) {score, 0}
#define qemu_plugin_scoreboard_u64_in_struct(score, type, member) \
    (qemu_plugin_u64) {score, offsetof(type, member)}

QEMU_PLUGIN_API
void qemu_plugin_u64_add(qemu_plugin_u64 entry, unsigned int vcpu_index,
                         uint64_t added);

QEMU_PLUGIN_API
uint64_t qemu_plugin_u64_get(qemu_plugin_u64 entry, unsigned int vcpu_index);

QEMU_PLUGIN_API
void qemu_plugin_u64_set(qemu_plugin_u64 entry, unsigned int vcpu_index,
                         uint64_t val);

QEMU_PLUGIN_API
uint64_t qemu_plugin_u64_sum(qemu_plugin_u64 entry);

#endif /* QEMU_QEMU_PLUGIN_H */
