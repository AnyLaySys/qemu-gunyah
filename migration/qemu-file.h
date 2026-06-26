
#ifndef MIGRATION_QEMU_FILE_H
#define MIGRATION_QEMU_FILE_H

#include <zlib.h>
#include "exec/cpu-common.h"
#include "io/channel.h"

QEMUFile *qemu_file_new_input(QIOChannel *ioc);
QEMUFile *qemu_file_new_output(QIOChannel *ioc);
int qemu_fclose(QEMUFile *f);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(QEMUFile, qemu_fclose)

uint64_t qemu_file_transferred(QEMUFile *f);

void qemu_put_buffer_async(QEMUFile *f, const uint8_t *buf, size_t size,
                           bool may_free);

#include "migration/qemu-file-types.h"

size_t coroutine_mixed_fn qemu_peek_buffer(QEMUFile *f, uint8_t **buf, size_t size, size_t offset);
size_t coroutine_mixed_fn qemu_get_buffer_in_place(QEMUFile *f, uint8_t **buf, size_t size);

int coroutine_mixed_fn qemu_peek_byte(QEMUFile *f, int offset);
void qemu_file_skip(QEMUFile *f, int size);
int qemu_file_get_error_obj_any(QEMUFile *f1, QEMUFile *f2, Error **errp);
void qemu_file_set_error_obj(QEMUFile *f, int ret, Error *err);
int qemu_file_get_error_obj(QEMUFile *f, Error **errp);
void qemu_file_set_error(QEMUFile *f, int ret);
int qemu_file_shutdown(QEMUFile *f);
QEMUFile *qemu_file_get_return_path(QEMUFile *f);
int qemu_fflush(QEMUFile *f);
void qemu_file_set_blocking(QEMUFile *f, bool block);
int qemu_file_get_to_fd(QEMUFile *f, int fd, size_t size);
void qemu_set_offset(QEMUFile *f, off_t off, int whence);
off_t qemu_get_offset(QEMUFile *f);
void qemu_put_buffer_at(QEMUFile *f, const uint8_t *buf, size_t buflen,
                        off_t pos);
size_t qemu_get_buffer_at(QEMUFile *f, const uint8_t *buf, size_t buflen,
                          off_t pos);

QIOChannel *qemu_file_get_ioc(QEMUFile *file);
int qemu_file_put_fd(QEMUFile *f, int fd);
int qemu_file_get_fd(QEMUFile *f);

#endif
