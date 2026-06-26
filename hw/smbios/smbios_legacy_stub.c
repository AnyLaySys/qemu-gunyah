
#include "qemu/osdep.h"
#include "hw/firmware/smbios.h"

void smbios_add_usr_blob_size(size_t size)
{
}

uint8_t *smbios_get_table_legacy(size_t *length, Error **errp)
{
    g_assert_not_reached();
}
