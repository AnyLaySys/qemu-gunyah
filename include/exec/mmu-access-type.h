#ifndef EXEC_MMU_ACCESS_TYPE_H
#define EXEC_MMU_ACCESS_TYPE_H

typedef enum MMUAccessType {
    MMU_DATA_LOAD  = 0,
    MMU_DATA_STORE = 1,
    MMU_INST_FETCH = 2
#define MMU_ACCESS_COUNT 3
} MMUAccessType;

#endif
