
#include "qemu/osdep.h"
#include "util.h"

int net_parse_macaddr(uint8_t *macaddr, const char *p)
{
    int i;
    char *last_char;
    long int offset;

    errno = 0;
    offset = strtol(p, &last_char, 0);
    if (errno == 0 && *last_char == '\0' &&
        offset >= 0 && offset <= 0xFFFFFF) {
        macaddr[3] = (offset & 0xFF0000) >> 16;
        macaddr[4] = (offset & 0xFF00) >> 8;
        macaddr[5] = offset & 0xFF;
        return 0;
    }

    for (i = 0; i < 6; i++) {
        macaddr[i] = strtol(p, (char **)&p, 16);
        if (i == 5) {
            if (*p != '\0') {
                return -1;
            }
        } else {
            if (*p != ':' && *p != '-') {
                return -1;
            }
            p++;
        }
    }

    return 0;
}
