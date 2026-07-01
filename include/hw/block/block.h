
#ifndef HW_BLOCK_H
#define HW_BLOCK_H

#include "exec/hwaddr.h"
#include "qapi/qapi-types-block-core.h"
#include "hw/qdev-properties-system.h"


typedef struct BlockConf {
    BlockBackend *blk;
    OnOffAuto backend_defaults;
    uint32_t physical_block_size;
    uint32_t logical_block_size;
    uint32_t min_io_size;
    uint32_t opt_io_size;
    int32_t bootindex;
    uint32_t discard_granularity;
    OnOffAuto wce;
    bool share_rw;
    OnOffAuto account_invalid, account_failed;
    BlockdevOnError rerror;
    BlockdevOnError werror;
} BlockConf;

static inline unsigned int get_physical_block_exp(BlockConf *conf)
{
    unsigned int exp = 0, size;

    for (size = conf->physical_block_size;
        size > conf->logical_block_size;
        size >>= 1) {
        exp++;
    }

    return exp;
}

#define DEFINE_BLOCK_PROPERTIES_BASE(_state, _conf)                     \
    DEFINE_PROP_ON_OFF_AUTO("backend_defaults", _state,                 \
                            _conf.backend_defaults, ON_OFF_AUTO_AUTO),  \
    DEFINE_PROP_BLOCKSIZE("logical_block_size", _state,                 \
                          _conf.logical_block_size),                    \
    DEFINE_PROP_BLOCKSIZE("physical_block_size", _state,                \
                          _conf.physical_block_size),                   \
    DEFINE_PROP_SIZE32("min_io_size", _state, _conf.min_io_size, 0),    \
    DEFINE_PROP_SIZE32("opt_io_size", _state, _conf.opt_io_size, 0),    \
    DEFINE_PROP_SIZE32("discard_granularity", _state,                   \
                       _conf.discard_granularity, -1),                  \
    DEFINE_PROP_ON_OFF_AUTO("write-cache", _state, _conf.wce,           \
                            ON_OFF_AUTO_AUTO),                          \
    DEFINE_PROP_BOOL("share-rw", _state, _conf.share_rw, false),        \
    DEFINE_PROP_ON_OFF_AUTO("account-invalid", _state,                  \
                            _conf.account_invalid, ON_OFF_AUTO_AUTO),   \
    DEFINE_PROP_ON_OFF_AUTO("account-failed", _state,                   \
                            _conf.account_failed, ON_OFF_AUTO_AUTO)

#define DEFINE_BLOCK_PROPERTIES(_state, _conf)                          \
    DEFINE_PROP_DRIVE("drive", _state, _conf.blk),                      \
    DEFINE_BLOCK_PROPERTIES_BASE(_state, _conf)

#define DEFINE_BLOCK_ERROR_PROPERTIES(_state, _conf)                    \
    DEFINE_PROP_BLOCKDEV_ON_ERROR("rerror", _state, _conf.rerror,       \
                                  BLOCKDEV_ON_ERROR_AUTO),              \
    DEFINE_PROP_BLOCKDEV_ON_ERROR("werror", _state, _conf.werror,       \
                                  BLOCKDEV_ON_ERROR_AUTO)


bool blk_check_size_and_read_all(BlockBackend *blk, DeviceState *dev,
                                 void *buf, hwaddr size, Error **errp);


bool blkconf_blocksizes(BlockConf *conf, Error **errp);
bool blkconf_apply_backend_options(BlockConf *conf, bool readonly,
                                   bool resizable, Error **errp);

#endif
