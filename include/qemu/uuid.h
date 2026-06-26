
#ifndef QEMU_UUID_H
#define QEMU_UUID_H



typedef struct {
    union {
        unsigned char data[16];
        struct {
            uint32_t time_low;
            uint16_t time_mid;
            uint16_t time_high_and_version;
            uint8_t  clock_seq_and_reserved;
            uint8_t  clock_seq_low;
            uint8_t  node[6];
        } fields;
    };
} QemuUUID;

#define UUID_LE(time_low, time_mid, time_hi_and_version,                    \
  clock_seq_hi_and_reserved, clock_seq_low, node0, node1, node2,            \
  node3, node4, node5)                                                      \
  { (time_low) & 0xff, ((time_low) >> 8) & 0xff, ((time_low) >> 16) & 0xff, \
    ((time_low) >> 24) & 0xff, (time_mid) & 0xff, ((time_mid) >> 8) & 0xff, \
    (time_hi_and_version) & 0xff, ((time_hi_and_version) >> 8) & 0xff,      \
    (clock_seq_hi_and_reserved), (clock_seq_low), (node0), (node1), (node2),\
    (node3), (node4), (node5) }

#define UUID(time_low, time_mid, time_hi_and_version,                    \
  clock_seq_hi_and_reserved, clock_seq_low, node0, node1, node2,         \
  node3, node4, node5)                                                   \
  { ((time_low) >> 24) & 0xff, ((time_low) >> 16) & 0xff,                \
    ((time_low) >> 8) & 0xff, (time_low) & 0xff,                         \
    ((time_mid) >> 8) & 0xff, (time_mid) & 0xff,                         \
    ((time_hi_and_version) >> 8) & 0xff, (time_hi_and_version) & 0xff,   \
    (clock_seq_hi_and_reserved), (clock_seq_low),                        \
    (node0), (node1), (node2), (node3), (node4), (node5)                 \
  }

#define UUID_FMT "%02hhx%02hhx%02hhx%02hhx-" \
                 "%02hhx%02hhx-%02hhx%02hhx-" \
                 "%02hhx%02hhx-" \
                 "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx"

#define UUID_NONE "00000000-0000-0000-0000-000000000000"
QEMU_BUILD_BUG_ON(sizeof(UUID_NONE) - 1 != 36);

#define UUID_STR_LEN sizeof(UUID_NONE)

void qemu_uuid_generate(QemuUUID *out);

int qemu_uuid_is_null(const QemuUUID *uu);

int qemu_uuid_is_equal(const QemuUUID *lhv, const QemuUUID *rhv);

void qemu_uuid_unparse(const QemuUUID *uuid, char *out);

char *qemu_uuid_unparse_strdup(const QemuUUID *uuid);

int qemu_uuid_parse(const char *str, QemuUUID *uuid);

QemuUUID qemu_uuid_bswap(QemuUUID uuid);

uint32_t qemu_uuid_hash(const void *uuid);

#endif
