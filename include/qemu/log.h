#ifndef QEMU_LOG_H
#define QEMU_LOG_H

#include "qemu/log-for-trace.h"


bool qemu_log_enabled(void);

bool qemu_log_separate(void);

#define CPU_LOG_TB_OUT_ASM (1 << 0)
#define CPU_LOG_TB_IN_ASM  (1 << 1)
#define CPU_LOG_TB_OP      (1 << 2)
#define CPU_LOG_TB_OP_OPT  (1 << 3)
#define CPU_LOG_INT        (1 << 4)
#define CPU_LOG_EXEC       (1 << 5)
#define CPU_LOG_PCALL      (1 << 6)
#define CPU_LOG_TB_CPU     (1 << 8)
#define CPU_LOG_RESET      (1 << 9)
#define LOG_UNIMP          (1 << 10)
#define LOG_GUEST_ERROR    (1 << 11)
#define CPU_LOG_MMU        (1 << 12)
#define CPU_LOG_TB_NOCHAIN (1 << 13)
#define CPU_LOG_PAGE       (1 << 14)
#define CPU_LOG_TB_OP_IND  (1 << 16)
#define CPU_LOG_TB_FPU     (1 << 17)
#define CPU_LOG_PLUGIN     (1 << 18)
#define LOG_STRACE         (1 << 19)
#define LOG_PER_THREAD     (1 << 20)
#define CPU_LOG_TB_VPU     (1 << 21)
#define LOG_TB_OP_PLUGIN   (1 << 22)
#define LOG_INVALID_MEM    (1 << 23)


FILE *qemu_log_trylock(void) G_GNUC_WARN_UNUSED_RESULT;
void qemu_log_unlock(FILE *fd);


#define qemu_log_mask(MASK, FMT, ...)                   \
    do {                                                \
        if (unlikely(qemu_loglevel_mask(MASK))) {       \
            qemu_log(FMT, ## __VA_ARGS__);              \
        }                                               \
    } while (0)

#define qemu_log_mask_and_addr(MASK, ADDR, FMT, ...)    \
    do {                                                \
        if (unlikely(qemu_loglevel_mask(MASK)) &&       \
                     qemu_log_in_addr_range(ADDR)) {    \
            qemu_log(FMT, ## __VA_ARGS__);              \
        }                                               \
    } while (0)


typedef struct QEMULogItem {
    int mask;
    const char *name;
    const char *help;
} QEMULogItem;

extern const QEMULogItem qemu_log_items[];

bool qemu_set_log(int log_flags, Error **errp);
bool qemu_set_log_filename(const char *filename, Error **errp);
bool qemu_set_log_filename_flags(const char *name, int flags, Error **errp);
void qemu_set_dfilter_ranges(const char *ranges, Error **errp);
bool qemu_log_in_addr_range(uint64_t addr);
int qemu_str_to_log_mask(const char *str);

void qemu_print_log_usage(FILE *f);

#endif
