
#ifndef QEMU_BASE64_H
#define QEMU_BASE64_H



uint8_t *qbase64_decode(const char *input,
                        size_t in_len,
                        size_t *out_len,
                        Error **errp);


#endif /* QEMU_BASE64_H */
