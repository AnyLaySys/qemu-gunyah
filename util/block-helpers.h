#ifndef BLOCK_HELPERS_H
#define BLOCK_HELPERS_H

#include "qemu/units.h"

#define MIN_BLOCK_SIZE          INT64_C(512)
#define MIN_BLOCK_SIZE_STR      "512 B"
#define MAX_BLOCK_SIZE          (2 * MiB)
#define MAX_BLOCK_SIZE_STR      "2 MiB"

bool check_block_size(const char *name, int64_t value, Error **errp);

#endif /* BLOCK_HELPERS_H */
