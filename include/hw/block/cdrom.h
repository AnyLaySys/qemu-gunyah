#ifndef HW_BLOCK_CDROM_H
#define HW_BLOCK_CDROM_H

int cdrom_read_toc(int nb_sectors, uint8_t *buf, int msf, int start_track);
int cdrom_read_toc_raw(int nb_sectors, uint8_t *buf, int msf, int session_num);

#endif
