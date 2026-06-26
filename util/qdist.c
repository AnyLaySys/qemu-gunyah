#include "qemu/osdep.h"
#include "qemu/qdist.h"

#include <math.h>
#ifndef NAN
#define NAN (0.0 / 0.0)
#endif

#define QDIST_EMPTY_STR "(empty)"

void qdist_init(struct qdist *dist)
{
    dist->entries = g_new(struct qdist_entry, 1);
    dist->size = 1;
    dist->n = 0;
}

void qdist_destroy(struct qdist *dist)
{
    g_free(dist->entries);
}

static inline int qdist_cmp_double(double a, double b)
{
    if (a > b) {
        return 1;
    } else if (a < b) {
        return -1;
    }
    return 0;
}

static int qdist_cmp(const void *ap, const void *bp)
{
    const struct qdist_entry *a = ap;
    const struct qdist_entry *b = bp;

    return qdist_cmp_double(a->x, b->x);
}

void qdist_add(struct qdist *dist, double x, long count)
{
    struct qdist_entry *entry = NULL;

    if (dist->n) {
        struct qdist_entry e;

        e.x = x;
        entry = bsearch(&e, dist->entries, dist->n, sizeof(e), qdist_cmp);
    }

    if (entry) {
        entry->count += count;
        return;
    }

    if (unlikely(dist->n == dist->size)) {
        dist->size *= 2;
        dist->entries = g_renew(struct qdist_entry, dist->entries, dist->size);
    }
    dist->n++;
    entry = &dist->entries[dist->n - 1];
    entry->x = x;
    entry->count = count;
    qsort(dist->entries, dist->n, sizeof(*entry), qdist_cmp);
}

void qdist_inc(struct qdist *dist, double x)
{
    qdist_add(dist, x, 1);
}

static const gunichar qdist_blocks[] = {
    0x2581,
    0x2582,
    0x2583,
    0x2584,
    0x2585,
    0x2586,
    0x2587,
    0x2588
};

#define QDIST_NR_BLOCK_CODES ARRAY_SIZE(qdist_blocks)

static char *qdist_pr_internal(const struct qdist *dist)
{
    double min, max;
    GString *s = g_string_new("");
    size_t i;

    if (dist->n == 1) {
        if (dist->entries[0].count) {
            g_string_append_unichar(s, qdist_blocks[QDIST_NR_BLOCK_CODES - 1]);
        } else {
            g_string_append_c(s, ' ');
        }
        goto out;
    }

    min = dist->entries[0].count;
    max = min;
    for (i = 0; i < dist->n; i++) {
        struct qdist_entry *e = &dist->entries[i];

        if (e->count < min) {
            min = e->count;
        }
        if (e->count > max) {
            max = e->count;
        }
    }

    for (i = 0; i < dist->n; i++) {
        struct qdist_entry *e = &dist->entries[i];
        int index;

        if (e->count) {
            index = (e->count - min) / (max - min) * (QDIST_NR_BLOCK_CODES - 1);
            g_string_append_unichar(s, qdist_blocks[index]);
        } else {
            g_string_append_c(s, ' ');
        }
    }
 out:
    return g_string_free(s, FALSE);
}

void qdist_bin__internal(struct qdist *to, const struct qdist *from, size_t n)
{
    double xmin, xmax;
    double step;
    size_t i, j;

    qdist_init(to);

    if (from->n == 0) {
        return;
    }
    if (n == 0 || from->n == 1) {
        n = from->n;
    }

    xmin = qdist_xmin(from);
    xmax = qdist_xmax(from);
    step = (xmax - xmin) / n;

    if (n == from->n) {
        for (i = 0; i < from->n; i++) {
            if (from->entries[i].x != xmin + i * step) {
                goto rebin;
            }
        }
        to->entries = g_renew(struct qdist_entry, to->entries, n);
        to->n = from->n;
        memcpy(to->entries, from->entries, sizeof(*to->entries) * to->n);
        return;
    }

 rebin:
    j = 0;
    for (i = 0; i < n; i++) {
        double x;
        double left, right;

        left = xmin + i * step;
        right = xmin + (i + 1) * step;

        x = left;
        qdist_add(to, x, 0);

        while (j < from->n && (from->entries[j].x < right || i == n - 1)) {
            struct qdist_entry *o = &from->entries[j];

            qdist_add(to, x, o->count);
            j++;
        }
    }
}

char *qdist_pr_plain(const struct qdist *dist, size_t n)
{
    struct qdist binned;
    char *ret;

    if (dist->n == 0) {
        return g_strdup(QDIST_EMPTY_STR);
    }
    qdist_bin__internal(&binned, dist, n);
    ret = qdist_pr_internal(&binned);
    qdist_destroy(&binned);
    return ret;
}

static char *qdist_pr_label(const struct qdist *dist, size_t n_bins,
                            uint32_t opt, bool is_left)
{
    const char *percent;
    const char *lparen;
    const char *rparen;
    GString *s;
    double x1, x2, step;
    double x;
    double n;
    int dec;

    s = g_string_new("");
    if (!(opt & QDIST_PR_LABELS)) {
        goto out;
    }

    dec = opt & QDIST_PR_NODECIMAL ? 0 : 1;
    percent = opt & QDIST_PR_PERCENT ? "%" : "";

    n = n_bins ? n_bins : dist->n;
    x = is_left ? qdist_xmin(dist) : qdist_xmax(dist);
    step = (qdist_xmax(dist) - qdist_xmin(dist)) / n;

    if (opt & QDIST_PR_100X) {
        x *= 100.0;
        step *= 100.0;
    }
    if (opt & QDIST_PR_NOBINRANGE) {
        lparen = rparen = "";
        x1 = x;
        x2 = x; /* unnecessary, but a dumb compiler might not figure it out */
    } else {
        lparen = "[";
        rparen = is_left ? ")" : "]";
        if (is_left) {
            x1 = x;
            x2 = x + step;
        } else {
            x1 = x - step;
            x2 = x;
        }
    }
    g_string_append_printf(s, "%s%.*f", lparen, dec, x1);
    if (!(opt & QDIST_PR_NOBINRANGE)) {
        g_string_append_printf(s, ",%.*f%s", dec, x2, rparen);
    }
    g_string_append(s, percent);
 out:
    return g_string_free(s, FALSE);
}

char *qdist_pr(const struct qdist *dist, size_t n_bins, uint32_t opt)
{
    const char *border = opt & QDIST_PR_BORDER ? "|" : "";
    char *llabel, *rlabel;
    char *hgram;
    GString *s;

    if (dist->n == 0) {
        return g_strdup(QDIST_EMPTY_STR);
    }

    s = g_string_new("");

    llabel = qdist_pr_label(dist, n_bins, opt, true);
    rlabel = qdist_pr_label(dist, n_bins, opt, false);
    hgram = qdist_pr_plain(dist, n_bins);
    g_string_append_printf(s, "%s%s%s%s%s",
                           llabel, border, hgram, border, rlabel);
    g_free(llabel);
    g_free(rlabel);
    g_free(hgram);

    return g_string_free(s, FALSE);
}

static inline double qdist_x(const struct qdist *dist, int index)
{
    if (dist->n == 0) {
        return NAN;
    }
    return dist->entries[index].x;
}

double qdist_xmin(const struct qdist *dist)
{
    return qdist_x(dist, 0);
}

double qdist_xmax(const struct qdist *dist)
{
    return qdist_x(dist, dist->n - 1);
}

size_t qdist_unique_entries(const struct qdist *dist)
{
    return dist->n;
}

unsigned long qdist_sample_count(const struct qdist *dist)
{
    unsigned long count = 0;
    size_t i;

    for (i = 0; i < dist->n; i++) {
        struct qdist_entry *e = &dist->entries[i];

        count += e->count;
    }
    return count;
}

static double qdist_pairwise_avg(const struct qdist *dist, size_t index,
                                 size_t n, unsigned long count)
{
    if (n <= 8) {
        size_t i;
        double ret = 0;

        for (i = 0; i < n; i++) {
            struct qdist_entry *e = &dist->entries[index + i];

            ret += e->x * e->count / count;
        }
        return ret;
    } else {
        size_t n2 = n / 2;

        return qdist_pairwise_avg(dist, index, n2, count) +
               qdist_pairwise_avg(dist, index + n2, n - n2, count);
    }
}

double qdist_avg(const struct qdist *dist)
{
    unsigned long count;

    count = qdist_sample_count(dist);
    if (!count) {
        return NAN;
    }
    return qdist_pairwise_avg(dist, 0, dist->n, count);
}
