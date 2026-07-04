
#ifndef TCG_MO_H
#define TCG_MO_H

typedef enum {
    TCG_MO_LD_LD  = 0x01,
    TCG_MO_ST_LD  = 0x02,
    TCG_MO_LD_ST  = 0x04,
    TCG_MO_ST_ST  = 0x08,
    TCG_MO_ALL    = 0x0F,  /* OR of the above */

    TCG_BAR_LDAQ  = 0x10,  /* Following ops will not come forward */
    TCG_BAR_STRL  = 0x20,  /* Previous ops will not be delayed */
    TCG_BAR_SC    = 0x30,  /* No ops cross barrier; OR of the above */
} TCGBar;

#endif /* TCG_MO_H */
