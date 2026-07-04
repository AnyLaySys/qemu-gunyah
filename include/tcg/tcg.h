
#ifndef TCG_H
#define TCG_H

#include "exec/memop.h"
#include "exec/memopidx.h"
#include "qemu/bitops.h"
#include "qemu/plugin.h"
#include "qemu/queue.h"
#include "tcg/tcg-mo.h"
#include "tcg-target-reg-bits.h"
#include "tcg-target.h"
#include "tcg/tcg-cond.h"
#include "tcg/debug-assert.h"

#define MAX_OP_PER_INSTR 266

#define CPU_TEMP_BUF_NLONGS 128
#define TCG_STATIC_FRAME_SIZE  (CPU_TEMP_BUF_NLONGS * sizeof(long))

#if TCG_TARGET_REG_BITS == 32
typedef int32_t tcg_target_long;
typedef uint32_t tcg_target_ulong;
#define TCG_PRIlx PRIx32
#define TCG_PRIld PRId32
#elif TCG_TARGET_REG_BITS == 64
typedef int64_t tcg_target_long;
typedef uint64_t tcg_target_ulong;
#define TCG_PRIlx PRIx64
#define TCG_PRIld PRId64
#else
#error unsupported
#endif

#if TCG_TARGET_NB_REGS <= 32
typedef uint32_t TCGRegSet;
#elif TCG_TARGET_NB_REGS <= 64
typedef uint64_t TCGRegSet;
#else
#error unsupported
#endif

typedef enum TCGOpcode {
#define DEF(name, oargs, iargs, cargs, flags) INDEX_op_ ## name,
#include "tcg/tcg-opc.h"
#undef DEF
    NB_OPS,
} TCGOpcode;

#define tcg_regset_set_reg(d, r)   ((d) |= (TCGRegSet)1 << (r))
#define tcg_regset_reset_reg(d, r) ((d) &= ~((TCGRegSet)1 << (r)))
#define tcg_regset_test_reg(d, r)  (((d) >> (r)) & 1)

#ifndef TCG_TARGET_INSN_UNIT_SIZE
# error "Missing TCG_TARGET_INSN_UNIT_SIZE"
#elif TCG_TARGET_INSN_UNIT_SIZE == 1
typedef uint8_t tcg_insn_unit;
#elif TCG_TARGET_INSN_UNIT_SIZE == 2
typedef uint16_t tcg_insn_unit;
#elif TCG_TARGET_INSN_UNIT_SIZE == 4
typedef uint32_t tcg_insn_unit;
#elif TCG_TARGET_INSN_UNIT_SIZE == 8
typedef uint64_t tcg_insn_unit;
#else
#endif

typedef struct TCGRelocation TCGRelocation;
struct TCGRelocation {
    QSIMPLEQ_ENTRY(TCGRelocation) next;
    tcg_insn_unit *ptr;
    intptr_t addend;
    int type;
};

typedef struct TCGOp TCGOp;
typedef struct TCGLabelUse TCGLabelUse;
struct TCGLabelUse {
    QSIMPLEQ_ENTRY(TCGLabelUse) next;
    TCGOp *op;
};

typedef struct TCGLabel TCGLabel;
struct TCGLabel {
    bool present;
    bool has_value;
    uint16_t id;
    union {
        uintptr_t value;
        const tcg_insn_unit *value_ptr;
    } u;
    QSIMPLEQ_HEAD(, TCGLabelUse) branches;
    QSIMPLEQ_HEAD(, TCGRelocation) relocs;
    QSIMPLEQ_ENTRY(TCGLabel) next;
};

typedef struct TCGPool {
    struct TCGPool *next;
    int size;
    uint8_t data[] __attribute__ ((aligned));
} TCGPool;

#define TCG_POOL_CHUNK_SIZE 32768

#define TCG_MAX_TEMPS 512
#define TCG_MAX_INSNS 512

#define TCG_STATIC_CALL_ARGS_SIZE 128

typedef enum TCGType {
    TCG_TYPE_I32,
    TCG_TYPE_I64,
    TCG_TYPE_I128,

    TCG_TYPE_V64,
    TCG_TYPE_V128,
    TCG_TYPE_V256,

#define TCG_TYPE_COUNT  (TCG_TYPE_V256 + 1)

#if TCG_TARGET_REG_BITS == 32
    TCG_TYPE_REG = TCG_TYPE_I32,
#else
    TCG_TYPE_REG = TCG_TYPE_I64,
#endif

#if UINTPTR_MAX == UINT32_MAX
    TCG_TYPE_PTR = TCG_TYPE_I32,
#else
    TCG_TYPE_PTR = TCG_TYPE_I64,
#endif
} TCGType;

static inline int tcg_type_size(TCGType t)
{
    unsigned i = t;
    if (i >= TCG_TYPE_V64) {
        tcg_debug_assert(i < TCG_TYPE_COUNT);
        i -= TCG_TYPE_V64 - 1;
    }
    return 4 << i;
}

typedef tcg_target_ulong TCGArg;


typedef struct TCGv_i32_d *TCGv_i32;
typedef struct TCGv_i64_d *TCGv_i64;
typedef struct TCGv_i128_d *TCGv_i128;
typedef struct TCGv_ptr_d *TCGv_ptr;
typedef struct TCGv_vec_d *TCGv_vec;
typedef TCGv_ptr TCGv_env;

#define TCG_CALL_NO_READ_GLOBALS    0x0001
#define TCG_CALL_NO_WRITE_GLOBALS   0x0002
#define TCG_CALL_NO_SIDE_EFFECTS    0x0004
#define TCG_CALL_NO_RETURN          0x0008

#define TCG_CALL_NO_RWG         TCG_CALL_NO_READ_GLOBALS
#define TCG_CALL_NO_WG          TCG_CALL_NO_WRITE_GLOBALS
#define TCG_CALL_NO_SE          TCG_CALL_NO_SIDE_EFFECTS
#define TCG_CALL_NO_RWG_SE      (TCG_CALL_NO_RWG | TCG_CALL_NO_SE)
#define TCG_CALL_NO_WG_SE       (TCG_CALL_NO_WG | TCG_CALL_NO_SE)

enum {
    TCG_BSWAP_IZ = 1,
    TCG_BSWAP_OZ = 2,
    TCG_BSWAP_OS = 4,
};

typedef enum TCGTempVal {
    TEMP_VAL_DEAD,
    TEMP_VAL_REG,
    TEMP_VAL_MEM,
    TEMP_VAL_CONST,
} TCGTempVal;

typedef enum TCGTempKind {
    TEMP_EBB,
    TEMP_TB,
    TEMP_GLOBAL,
    TEMP_FIXED,
    TEMP_CONST,
} TCGTempKind;

typedef struct TCGTemp {
    TCGReg reg:8;
    TCGTempVal val_type:8;
    TCGType base_type:8;
    TCGType type:8;
    TCGTempKind kind:3;
    unsigned int indirect_reg:1;
    unsigned int indirect_base:1;
    unsigned int mem_coherent:1;
    unsigned int mem_allocated:1;
    unsigned int temp_allocated:1;
    unsigned int temp_subindex:2;

    int64_t val;
    struct TCGTemp *mem_base;
    intptr_t mem_offset;
    const char *name;

    uintptr_t state;
    void *state_ptr;
} TCGTemp;

typedef struct TCGContext TCGContext;

typedef struct TCGTempSet {
    unsigned long l[BITS_TO_LONGS(TCG_MAX_TEMPS)];
} TCGTempSet;

#define DEAD_ARG  (1 << 4)
#define SYNC_ARG  (1 << 0)
typedef uint32_t TCGLifeData;

struct TCGOp {
    TCGOpcode opc   : 8;
    unsigned nargs  : 8;

    unsigned param1 : 8;
    unsigned param2 : 8;

    TCGLifeData life;

    QTAILQ_ENTRY(TCGOp) link;

    TCGRegSet output_pref[2];

    TCGArg args[];
};

#define TCGOP_CALLI(X)    (X)->param1
#define TCGOP_CALLO(X)    (X)->param2

#define TCGOP_TYPE(X)     (X)->param1
#define TCGOP_FLAGS(X)    (X)->param2
#define TCGOP_VECE(X)     (X)->param2

QEMU_BUILD_BUG_ON(NB_OPS > (1 << 8));

static inline TCGRegSet output_pref(const TCGOp *op, unsigned i)
{
    return i < ARRAY_SIZE(op->output_pref) ? op->output_pref[i] : 0;
}

struct TCGContext {
    uint8_t *pool_cur, *pool_end;
    TCGPool *pool_first, *pool_current, *pool_first_large;
    int nb_labels;
    int nb_globals;
    int nb_temps;
    int nb_indirects;
    int nb_ops;
    TCGType addr_type;            /* TCG_TYPE_I32 or TCG_TYPE_I64 */

    int page_mask;
    uint8_t page_bits;
    uint8_t tlb_dyn_max_bits;
    uint8_t insn_start_words;
    TCGBar guest_mo;

    TCGRegSet reserved_regs;
    intptr_t current_frame_offset;
    intptr_t frame_start;
    intptr_t frame_end;
    TCGTemp *frame_temp;

    TranslationBlock *gen_tb;     /* tb for which code is being generated */
    tcg_insn_unit *code_buf;      /* pointer for start of tb */
    tcg_insn_unit *code_ptr;      /* pointer for running end of tb */

#ifdef CONFIG_DEBUG_TCG
    int goto_tb_issue_mask;
    const TCGOpcode *vecop_list;
#endif

    void *code_gen_buffer;
    size_t code_gen_buffer_size;
    void *code_gen_ptr;
    void *data_gen_ptr;

    void *code_gen_highwater;

    CPUState *cpu;                      /* *_trans */

    QSIMPLEQ_HEAD(, TCGLabelQemuLdst) ldst_labels;
    struct TCGLabelPoolData *pool_labels;

    TCGLabel *exitreq_label;

#ifdef CONFIG_PLUGIN
    struct qemu_plugin_tb *plugin_tb;
    const struct DisasContextBase *plugin_db;

    struct qemu_plugin_insn *plugin_insn;
#endif

#ifdef __riscv
    MemOp riscv_cur_vsew;
    TCGType riscv_cur_type;
#endif

    GHashTable *const_table[TCG_TYPE_COUNT];
    TCGTempSet free_temps[TCG_TYPE_COUNT];
    TCGTemp temps[TCG_MAX_TEMPS]; /* globals first, temps after */

    QTAILQ_HEAD(, TCGOp) ops, free_ops;
    QSIMPLEQ_HEAD(, TCGLabel) labels;

    TCGOp *emit_before_op;

    TCGTemp *reg_to_temp[TCG_TARGET_NB_REGS];

    uint16_t gen_insn_end_off[TCG_MAX_INSNS];
    uint64_t *gen_insn_data;

    sigjmp_buf jmp_trans;
};

static inline bool temp_readonly(TCGTemp *ts)
{
    return ts->kind >= TEMP_FIXED;
}

#ifdef CONFIG_USER_ONLY
extern bool tcg_use_softmmu;
#else
#define tcg_use_softmmu  true
#endif

#ifdef __ANDROID__
TCGContext **android_tcg_ctx_ptr(void);
#define tcg_ctx (*android_tcg_ctx_ptr())
#else
extern __thread TCGContext *tcg_ctx;
#endif
extern const void *tcg_code_gen_epilogue;
extern uintptr_t tcg_splitwx_diff;
extern TCGv_env tcg_env;

bool in_code_gen_buffer(const void *p);

#ifdef CONFIG_DEBUG_TCG
const void *tcg_splitwx_to_rx(void *rw);
void *tcg_splitwx_to_rw(const void *rx);
#else
static inline const void *tcg_splitwx_to_rx(void *rw)
{
    return rw ? rw + tcg_splitwx_diff : NULL;
}

static inline void *tcg_splitwx_to_rw(const void *rx)
{
    return rx ? (void *)rx - tcg_splitwx_diff : NULL;
}
#endif

static inline TCGArg temp_arg(TCGTemp *ts)
{
    return (uintptr_t)ts;
}

static inline TCGTemp *arg_temp(TCGArg a)
{
    return (TCGTemp *)(uintptr_t)a;
}

#ifdef CONFIG_DEBUG_TCG
size_t temp_idx(TCGTemp *ts);
TCGTemp *tcgv_i32_temp(TCGv_i32 v);
#else
static inline size_t temp_idx(TCGTemp *ts)
{
    return ts - tcg_ctx->temps;
}

static inline TCGTemp *tcgv_i32_temp(TCGv_i32 v)
{
    return (void *)tcg_ctx + (uintptr_t)v;
}
#endif

static inline TCGTemp *tcgv_i64_temp(TCGv_i64 v)
{
    return tcgv_i32_temp((TCGv_i32)v);
}

static inline TCGTemp *tcgv_i128_temp(TCGv_i128 v)
{
    return tcgv_i32_temp((TCGv_i32)v);
}

static inline TCGTemp *tcgv_ptr_temp(TCGv_ptr v)
{
    return tcgv_i32_temp((TCGv_i32)v);
}

static inline TCGTemp *tcgv_vec_temp(TCGv_vec v)
{
    return tcgv_i32_temp((TCGv_i32)v);
}

static inline TCGArg tcgv_i32_arg(TCGv_i32 v)
{
    return temp_arg(tcgv_i32_temp(v));
}

static inline TCGArg tcgv_i64_arg(TCGv_i64 v)
{
    return temp_arg(tcgv_i64_temp(v));
}

static inline TCGArg tcgv_i128_arg(TCGv_i128 v)
{
    return temp_arg(tcgv_i128_temp(v));
}

static inline TCGArg tcgv_ptr_arg(TCGv_ptr v)
{
    return temp_arg(tcgv_ptr_temp(v));
}

static inline TCGArg tcgv_vec_arg(TCGv_vec v)
{
    return temp_arg(tcgv_vec_temp(v));
}

static inline TCGv_i32 temp_tcgv_i32(TCGTemp *t)
{
    (void)temp_idx(t); /* trigger embedded assert */
    return (TCGv_i32)((void *)t - (void *)tcg_ctx);
}

static inline TCGv_i64 temp_tcgv_i64(TCGTemp *t)
{
    return (TCGv_i64)temp_tcgv_i32(t);
}

static inline TCGv_i128 temp_tcgv_i128(TCGTemp *t)
{
    return (TCGv_i128)temp_tcgv_i32(t);
}

static inline TCGv_ptr temp_tcgv_ptr(TCGTemp *t)
{
    return (TCGv_ptr)temp_tcgv_i32(t);
}

static inline TCGv_vec temp_tcgv_vec(TCGTemp *t)
{
    return (TCGv_vec)temp_tcgv_i32(t);
}

static inline TCGArg tcg_get_insn_param(TCGOp *op, int arg)
{
    return op->args[arg];
}

static inline void tcg_set_insn_param(TCGOp *op, int arg, TCGArg v)
{
    op->args[arg] = v;
}

static inline uint64_t tcg_get_insn_start_param(TCGOp *op, int arg)
{
    if (TCG_TARGET_REG_BITS == 64) {
        return tcg_get_insn_param(op, arg);
    } else {
        return deposit64(tcg_get_insn_param(op, arg * 2), 32, 32,
                         tcg_get_insn_param(op, arg * 2 + 1));
    }
}

static inline void tcg_set_insn_start_param(TCGOp *op, int arg, uint64_t v)
{
    if (TCG_TARGET_REG_BITS == 64) {
        tcg_set_insn_param(op, arg, v);
    } else {
        tcg_set_insn_param(op, arg * 2, v);
        tcg_set_insn_param(op, arg * 2 + 1, v >> 32);
    }
}

static inline TCGOp *tcg_last_op(void)
{
    return QTAILQ_LAST(&tcg_ctx->ops);
}

static inline bool tcg_op_buf_full(void)
{
    return tcg_ctx->nb_ops >= 4000;
}


void *tcg_malloc_internal(TCGContext *s, int size);
void tcg_pool_reset(TCGContext *s);
TranslationBlock *tcg_tb_alloc(TCGContext *s);

void tcg_region_reset_all(void);

size_t tcg_code_size(void);
size_t tcg_code_capacity(void);

void tcg_tb_insert(TranslationBlock *tb);

void tcg_tb_remove(TranslationBlock *tb);

TranslationBlock *tcg_tb_lookup(uintptr_t tc_ptr);

void tcg_tb_foreach(GTraverseFunc func, gpointer user_data);

size_t tcg_nb_tbs(void);

static inline void *tcg_malloc(int size)
{
    TCGContext *s = tcg_ctx;
    uint8_t *ptr, *ptr_end;

    size = QEMU_ALIGN_UP(size, 8);

    ptr = s->pool_cur;
    ptr_end = ptr + size;
    if (unlikely(ptr_end > s->pool_end)) {
        return tcg_malloc_internal(tcg_ctx, size);
    } else {
        s->pool_cur = ptr_end;
        return ptr;
    }
}

void tcg_func_start(TCGContext *s);

int tcg_gen_code(TCGContext *s, TranslationBlock *tb, uint64_t pc_start);

void tb_target_set_jmp_target(const TranslationBlock *, int,
                              uintptr_t, uintptr_t);

void tcg_set_frame(TCGContext *s, TCGReg reg, intptr_t start, intptr_t size);

#define TCG_CT_CONST      1  /* any constant of register size */
#define TCG_CT_REG_ZERO   2  /* zero, in TCG_REG_ZERO */

typedef struct TCGArgConstraint {
    unsigned ct : 16;
    unsigned alias_index : 4;
    unsigned sort_index : 4;
    unsigned pair_index : 4;
    unsigned pair : 2;  /* 0: none, 1: first, 2: second, 3: second alias */
    bool oalias : 1;
    bool ialias : 1;
    bool newreg : 1;
    TCGRegSet regs;
} TCGArgConstraint;

#define TCG_MAX_OP_ARGS 16

enum {
    TCG_OPF_BB_EXIT      = 0x01,
    TCG_OPF_BB_END       = 0x02,
    TCG_OPF_CALL_CLOBBER = 0x04,
    TCG_OPF_SIDE_EFFECTS = 0x08,
    TCG_OPF_NOT_PRESENT  = 0x20,
    TCG_OPF_VECTOR       = 0x40,
    TCG_OPF_COND_BRANCH  = 0x80
};

typedef struct TCGOpDef {
    const char *name;
    uint8_t nb_oargs, nb_iargs, nb_cargs, nb_args;
    uint8_t flags;
} TCGOpDef;

extern const TCGOpDef tcg_op_defs[];
extern const size_t tcg_op_defs_max;

bool tcg_op_supported(TCGOpcode op, TCGType type, unsigned flags);
bool tcg_op_deposit_valid(TCGType type, unsigned ofs, unsigned len);

void tcg_gen_call0(void *func, TCGHelperInfo *, TCGTemp *ret);
void tcg_gen_call1(void *func, TCGHelperInfo *, TCGTemp *ret, TCGTemp *);
void tcg_gen_call2(void *func, TCGHelperInfo *, TCGTemp *ret,
                   TCGTemp *, TCGTemp *);
void tcg_gen_call3(void *func, TCGHelperInfo *, TCGTemp *ret,
                   TCGTemp *, TCGTemp *, TCGTemp *);
void tcg_gen_call4(void *func, TCGHelperInfo *, TCGTemp *ret,
                   TCGTemp *, TCGTemp *, TCGTemp *, TCGTemp *);
void tcg_gen_call5(void *func, TCGHelperInfo *, TCGTemp *ret,
                   TCGTemp *, TCGTemp *, TCGTemp *, TCGTemp *, TCGTemp *);
void tcg_gen_call6(void *func, TCGHelperInfo *, TCGTemp *ret,
                   TCGTemp *, TCGTemp *, TCGTemp *, TCGTemp *,
                   TCGTemp *, TCGTemp *);
void tcg_gen_call7(void *func, TCGHelperInfo *, TCGTemp *ret,
                   TCGTemp *, TCGTemp *, TCGTemp *, TCGTemp *,
                   TCGTemp *, TCGTemp *, TCGTemp *);

TCGOp *tcg_emit_op(TCGOpcode opc, unsigned nargs);
void tcg_op_remove(TCGContext *s, TCGOp *op);

void tcg_remove_ops_after(TCGOp *op);

void tcg_optimize(TCGContext *s);

TCGLabel *gen_new_label(void);


static inline TCGArg label_arg(TCGLabel *l)
{
    return (uintptr_t)l;
}


static inline TCGLabel *arg_label(TCGArg i)
{
    return (TCGLabel *)(uintptr_t)i;
}


static inline ptrdiff_t tcg_ptr_byte_diff(const void *a, const void *b)
{
    return a - b;
}


static inline ptrdiff_t tcg_pcrel_diff(TCGContext *s, const void *target)
{
    return tcg_ptr_byte_diff(target, tcg_splitwx_to_rx(s->code_ptr));
}

static inline ptrdiff_t tcg_tbrel_diff(TCGContext *s, const void *target)
{
    return tcg_ptr_byte_diff(target, tcg_splitwx_to_rx(s->code_buf));
}


static inline size_t tcg_current_code_size(TCGContext *s)
{
    return tcg_ptr_byte_diff(s->code_ptr, s->code_buf);
}

#define TB_EXIT_MASK      3
#define TB_EXIT_IDX0      0
#define TB_EXIT_IDX1      1
#define TB_EXIT_IDXMAX    1
#define TB_EXIT_REQUESTED 3

#ifdef CONFIG_TCG_INTERPRETER
uintptr_t tcg_qemu_tb_exec(CPUArchState *env, const void *tb_ptr);
#else
typedef uintptr_t tcg_prologue_fn(CPUArchState *env, const void *tb_ptr);
extern tcg_prologue_fn *tcg_qemu_tb_exec;
#endif

void tcg_register_jit(const void *buf, size_t buf_size);

int tcg_can_emit_vec_op(TCGOpcode, TCGType, unsigned);

void tcg_expand_vec_op(TCGOpcode, TCGType, unsigned, TCGArg, ...);

uint64_t dup_const(unsigned vece, uint64_t c);

#define dup_const(VECE, C)                                         \
    (__builtin_constant_p(VECE)                                    \
     ? (  (VECE) == MO_8  ? 0x0101010101010101ull * (uint8_t)(C)   \
        : (VECE) == MO_16 ? 0x0001000100010001ull * (uint16_t)(C)  \
        : (VECE) == MO_32 ? 0x0000000100000001ull * (uint32_t)(C)  \
        : (VECE) == MO_64 ? (uint64_t)(C)                          \
        : (qemu_build_not_reached_always(), 0))                    \
     : dup_const(VECE, C))

static inline const TCGOpcode *tcg_swap_vecop_list(const TCGOpcode *n)
{
#ifdef CONFIG_DEBUG_TCG
    const TCGOpcode *o = tcg_ctx->vecop_list;
    tcg_ctx->vecop_list = n;
    return o;
#else
    return NULL;
#endif
}

bool tcg_can_emit_vecop_list(const TCGOpcode *, TCGType, unsigned);
void tcg_dump_ops(TCGContext *s, FILE *f, bool have_prefs);

#endif /* TCG_H */
