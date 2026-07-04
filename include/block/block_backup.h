
#ifndef BLOCK_BACKUP_H
#define BLOCK_BACKUP_H

#include "block/blockjob.h"

void backup_do_checkpoint(BlockJob *job, Error **errp);

#endif
