
#include "qemu/osdep.h"
#include "system/block-backend.h"
#include "qapi/qapi-types-block.h"
#include "qemu/bswap.h"
#include "hw/block/block.h"
#include "trace.h"

struct partition {
        uint8_t boot_ind;           /* 0x80 - active */
        uint8_t head;               /* starting head */
        uint8_t sector;             /* starting sector */
        uint8_t cyl;                /* starting cylinder */
        uint8_t sys_ind;            /* What partition type */
        uint8_t end_head;           /* end head */
        uint8_t end_sector;         /* end sector */
        uint8_t end_cyl;            /* end cylinder */
        uint32_t start_sect;        /* starting sector counting from 0 */
        uint32_t nr_sects;          /* nr of sectors in partition */
} QEMU_PACKED;

static int guess_disk_lchs(BlockBackend *blk,
                           int *pcylinders, int *pheads, int *psectors)
{
    uint8_t buf[BDRV_SECTOR_SIZE];
    int i, heads, sectors, cylinders;
    struct partition *p;
    uint32_t nr_sects;
    uint64_t nb_sectors;

    blk_get_geometry(blk, &nb_sectors);

    if (blk_pread(blk, 0, BDRV_SECTOR_SIZE, buf, 0) < 0) {
        return -1;
    }
    if (buf[510] != 0x55 || buf[511] != 0xaa) {
        return -1;
    }
    for (i = 0; i < 4; i++) {
        p = ((struct partition *)(buf + 0x1be)) + i;
        nr_sects = le32_to_cpu(p->nr_sects);
        if (nr_sects && p->end_head) {
            heads = p->end_head + 1;
            sectors = p->end_sector & 63;
            if (sectors == 0) {
                continue;
            }
            cylinders = nb_sectors / (heads * sectors);
            if (cylinders < 1 || cylinders > 16383) {
                continue;
            }
            *pheads = heads;
            *psectors = sectors;
            *pcylinders = cylinders;
            trace_hd_geometry_lchs_guess(blk, cylinders, heads, sectors);
            return 0;
        }
    }
    return -1;
}

static void guess_chs_for_size(BlockBackend *blk,
                uint32_t *pcyls, uint32_t *pheads, uint32_t *psecs)
{
    uint64_t nb_sectors;
    int cylinders;

    blk_get_geometry(blk, &nb_sectors);

    cylinders = nb_sectors / (16 * 63);
    if (cylinders > 16383) {
        cylinders = 16383;
    } else if (cylinders < 2) {
        cylinders = 2;
    }
    *pcyls = cylinders;
    *pheads = 16;
    *psecs = 63;
}

void hd_geometry_guess(BlockBackend *blk,
                       uint32_t *pcyls, uint32_t *pheads, uint32_t *psecs,
                       int *ptrans)
{
    int cylinders, heads, secs, translation;
    HDGeometry geo;

    if (blk_probe_geometry(blk, &geo) == 0) {
        *pcyls = geo.cylinders;
        *psecs = geo.sectors;
        *pheads = geo.heads;
        translation = BIOS_ATA_TRANSLATION_NONE;
    } else if (guess_disk_lchs(blk, &cylinders, &heads, &secs) < 0) {
        guess_chs_for_size(blk, pcyls, pheads, psecs);
        translation = hd_bios_chs_auto_trans(*pcyls, *pheads, *psecs);
    } else if (heads > 16) {
        guess_chs_for_size(blk, pcyls, pheads, psecs);
        translation = *pcyls * *pheads <= 131072
            ? BIOS_ATA_TRANSLATION_LARGE
            : BIOS_ATA_TRANSLATION_LBA;
    } else {
        *pcyls = cylinders;
        *pheads = heads;
        *psecs = secs;
        translation = BIOS_ATA_TRANSLATION_NONE;
    }
    if (ptrans) {
        if (*ptrans == BIOS_ATA_TRANSLATION_AUTO) {
            *ptrans = translation;
        } else {
            translation = *ptrans;
        }
    }
    trace_hd_geometry_guess(blk, *pcyls, *pheads, *psecs, translation);
}

int hd_bios_chs_auto_trans(uint32_t cyls, uint32_t heads, uint32_t secs)
{
    return cyls <= 1024 && heads <= 16 && secs <= 63
        ? BIOS_ATA_TRANSLATION_NONE
        : BIOS_ATA_TRANSLATION_LBA;
}
