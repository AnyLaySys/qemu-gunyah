
#ifndef MEMATTRS_H
#define MEMATTRS_H

typedef struct MemTxAttrs {
    unsigned int secure:1;
    unsigned int space:2;
    unsigned int user:1;
    unsigned int memory:1;
    unsigned int debug:1;
    unsigned int requester_id:16;

    unsigned int pid:8;

    bool unspecified;

    uint8_t _reserved1;
    uint16_t _reserved2;
} MemTxAttrs;

QEMU_BUILD_BUG_ON(sizeof(MemTxAttrs) > 8);

#define MEMTXATTRS_UNSPECIFIED ((MemTxAttrs) { .unspecified = true })

#define MEMTX_OK 0
#define MEMTX_ERROR             (1U << 0) /* device returned an error */
#define MEMTX_DECODE_ERROR      (1U << 1) /* nothing at that address */
#define MEMTX_ACCESS_ERROR      (1U << 2) /* access denied */
typedef uint32_t MemTxResult;

#endif
