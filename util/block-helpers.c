
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "block-helpers.h"

bool check_block_size(const char *name, int64_t value, Error **errp)
{
    if (!value) {
        return true;
    }

    if (value < MIN_BLOCK_SIZE || value > MAX_BLOCK_SIZE
        || (value & (value - 1))) {
        error_setg(errp,
                   "parameter %s must be a power of 2 between %" PRId64
                   " and %" PRId64,
                   name, MIN_BLOCK_SIZE, MAX_BLOCK_SIZE);
        return false;
    }
    return true;
}
