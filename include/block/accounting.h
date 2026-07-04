#ifndef BLOCK_ACCOUNTING_H
#define BLOCK_ACCOUNTING_H

#include "qemu/timed-average.h"
#include "qemu/thread.h"
#include "qapi/qapi-types-common.h"

typedef struct BlockAcctTimedStats BlockAcctTimedStats;
typedef struct BlockAcctStats BlockAcctStats;

enum BlockAcctType {
    BLOCK_ACCT_NONE = 0,
    BLOCK_ACCT_READ,
    BLOCK_ACCT_WRITE,
    BLOCK_ACCT_FLUSH,
    BLOCK_ACCT_ZONE_APPEND,
    BLOCK_ACCT_UNMAP,
    BLOCK_MAX_IOTYPE,
};

struct BlockAcctTimedStats {
    BlockAcctStats *stats;
    TimedAverage latency[BLOCK_MAX_IOTYPE];
    unsigned interval_length; /* in seconds */
    QSLIST_ENTRY(BlockAcctTimedStats) entries;
};

typedef struct BlockLatencyHistogram {
    int nbins;
    uint64_t *boundaries; /* @nbins-1 numbers here
                             (all boundaries, except 0 and +inf) */
    uint64_t *bins;
} BlockLatencyHistogram;

struct BlockAcctStats {
    QemuMutex lock;
    uint64_t nr_bytes[BLOCK_MAX_IOTYPE];
    uint64_t nr_ops[BLOCK_MAX_IOTYPE];
    uint64_t invalid_ops[BLOCK_MAX_IOTYPE];
    uint64_t failed_ops[BLOCK_MAX_IOTYPE];
    uint64_t total_time_ns[BLOCK_MAX_IOTYPE];
    uint64_t merged[BLOCK_MAX_IOTYPE];
    int64_t last_access_time_ns;
    QSLIST_HEAD(, BlockAcctTimedStats) intervals;
    bool account_invalid;
    bool account_failed;
    BlockLatencyHistogram latency_histogram[BLOCK_MAX_IOTYPE];
};

typedef struct BlockAcctCookie {
    int64_t bytes;
    int64_t start_time_ns;
    enum BlockAcctType type;
} BlockAcctCookie;

void block_acct_init(BlockAcctStats *stats);
void block_acct_setup(BlockAcctStats *stats, enum OnOffAuto account_invalid,
                      enum OnOffAuto account_failed);
void block_acct_cleanup(BlockAcctStats *stats);
void block_acct_add_interval(BlockAcctStats *stats, unsigned interval_length);
BlockAcctTimedStats *block_acct_interval_next(BlockAcctStats *stats,
                                              BlockAcctTimedStats *s);
void block_acct_start(BlockAcctStats *stats, BlockAcctCookie *cookie,
                      int64_t bytes, enum BlockAcctType type);
void block_acct_done(BlockAcctStats *stats, BlockAcctCookie *cookie);
void block_acct_failed(BlockAcctStats *stats, BlockAcctCookie *cookie);
void block_acct_invalid(BlockAcctStats *stats, enum BlockAcctType type);
void block_acct_merge_done(BlockAcctStats *stats, enum BlockAcctType type,
                           int num_requests);
int64_t block_acct_idle_time_ns(BlockAcctStats *stats);
double block_acct_queue_depth(BlockAcctTimedStats *stats,
                              enum BlockAcctType type);
int block_latency_histogram_set(BlockAcctStats *stats, enum BlockAcctType type,
                                uint64List *boundaries);
void block_latency_histograms_clear(BlockAcctStats *stats);

#endif
