#ifndef QEMU_DATADIR_H
#define QEMU_DATADIR_H

#define QEMU_FILE_TYPE_BIOS   0
char *qemu_find_file(int type, const char *name);
void qemu_add_default_firmwarepath(void);
void qemu_add_data_dir(char *path);
void qemu_list_data_dirs(void);

#endif
