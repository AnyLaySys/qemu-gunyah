#ifndef QEMU_TSAN_H
#define QEMU_TSAN_H

#ifdef CONFIG_TSAN
#define QEMU_TSAN_ANNOTATE_HAPPENS_BEFORE(addr) \
    AnnotateHappensBefore(__FILE__, __LINE__, (void *)(addr))
#define QEMU_TSAN_ANNOTATE_HAPPENS_AFTER(addr) \
    AnnotateHappensAfter(__FILE__, __LINE__, (void *)(addr))
#define QEMU_TSAN_ANNOTATE_THREAD_NAME(name) \
    AnnotateThreadName(__FILE__, __LINE__, (void *)(name))
#define QEMU_TSAN_ANNOTATE_IGNORE_READS_BEGIN() \
    AnnotateIgnoreReadsBegin(__FILE__, __LINE__)
#define QEMU_TSAN_ANNOTATE_IGNORE_READS_END() \
    AnnotateIgnoreReadsEnd(__FILE__, __LINE__)
#define QEMU_TSAN_ANNOTATE_IGNORE_WRITES_BEGIN() \
    AnnotateIgnoreWritesBegin(__FILE__, __LINE__)
#define QEMU_TSAN_ANNOTATE_IGNORE_WRITES_END() \
    AnnotateIgnoreWritesEnd(__FILE__, __LINE__)
#else
#define QEMU_TSAN_ANNOTATE_HAPPENS_BEFORE(addr)
#define QEMU_TSAN_ANNOTATE_HAPPENS_AFTER(addr)
#define QEMU_TSAN_ANNOTATE_THREAD_NAME(name)
#define QEMU_TSAN_ANNOTATE_IGNORE_READS_BEGIN()
#define QEMU_TSAN_ANNOTATE_IGNORE_READS_END()
#define QEMU_TSAN_ANNOTATE_IGNORE_WRITES_BEGIN()
#define QEMU_TSAN_ANNOTATE_IGNORE_WRITES_END()
#endif

void AnnotateHappensBefore(const char *f, int l, void *addr);
void AnnotateHappensAfter(const char *f, int l, void *addr);
void AnnotateThreadName(const char *f, int l, char *name);
void AnnotateIgnoreReadsBegin(const char *f, int l);
void AnnotateIgnoreReadsEnd(const char *f, int l);
void AnnotateIgnoreWritesBegin(const char *f, int l);
void AnnotateIgnoreWritesEnd(const char *f, int l);
#endif
