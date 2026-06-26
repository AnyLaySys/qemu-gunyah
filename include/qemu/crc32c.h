
#ifndef QEMU_CRC32C_H
#define QEMU_CRC32C_H


uint32_t crc32c(uint32_t crc, const uint8_t *data, unsigned int length);
uint32_t iov_crc32c(uint32_t crc, const struct iovec *iov, size_t iov_cnt);

#endif
