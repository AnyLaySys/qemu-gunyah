#ifndef QEMU_QDIST_H
#define QEMU_QDIST_H

#include "qemu/bitops.h"

struct qdist_entry {
    double x;
    unsigned long count;
};

struct qdist {
    struct qdist_entry *entries;
    size_t n;
    size_t size;
};

#define QDIST_PR_BORDER     BIT(0)
#define QDIST_PR_LABELS     BIT(1)
#define QDIST_PR_NODECIMAL  BIT(2)
#define QDIST_PR_PERCENT    BIT(3)
#define QDIST_PR_100X       BIT(4)
#define QDIST_PR_NOBINRANGE BIT(5)

void qdist_init(struct qdist *dist);
void qdist_destroy(struct qdist *dist);

void qdist_add(struct qdist *dist, double x, long count);
void qdist_inc(struct qdist *dist, double x);
double qdist_xmin(const struct qdist *dist);
double qdist_xmax(const struct qdist *dist);
double qdist_avg(const struct qdist *dist);
unsigned long qdist_sample_count(const struct qdist *dist);
size_t qdist_unique_entries(const struct qdist *dist);

char *qdist_pr_plain(const struct qdist *dist, size_t n_groups);

char *qdist_pr(const struct qdist *dist, size_t n_groups, uint32_t opt);

void qdist_bin__internal(struct qdist *to, const struct qdist *from, size_t n);

#endif /* QEMU_QDIST_H */
