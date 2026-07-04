
#ifndef I2C_DDC_H
#define I2C_DDC_H

#include "hw/display/edid.h"
#include "qom/object.h"

struct I2CDDCState {
    I2CSlave i2c;
    bool firstbyte;
    uint8_t reg;
    qemu_edid_info edid_info;
    uint8_t edid_blob[128];
};


#define TYPE_I2CDDC "i2c-ddc"
OBJECT_DECLARE_SIMPLE_TYPE(I2CDDCState, I2CDDC)

#endif /* I2C_DDC_H */
