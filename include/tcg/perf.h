
#ifndef TCG_PERF_H
#define TCG_PERF_H

#if defined(CONFIG_TCG) && defined(CONFIG_LINUX)
void perf_enable_perfmap(void);

void perf_enable_jitdump(void);

void perf_report_prologue(const void *start, size_t size);

void perf_report_code(uint64_t guest_pc, TranslationBlock *tb,
                      const void *start);

void perf_exit(void);
#else
static inline void perf_enable_perfmap(void)
{
}

static inline void perf_enable_jitdump(void)
{
}

static inline void perf_report_prologue(const void *start, size_t size)
{
}

static inline void perf_report_code(uint64_t guest_pc, TranslationBlock *tb,
                                    const void *start)
{
}

static inline void perf_exit(void)
{
}
#endif

#endif
