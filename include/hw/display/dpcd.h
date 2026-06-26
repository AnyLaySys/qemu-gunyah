
#ifndef DPCD_H
#define DPCD_H
#include "qom/object.h"


#define TYPE_DPCD "dpcd"
OBJECT_DECLARE_SIMPLE_TYPE(DPCDState, DPCD)

#define DPCD_REVISION                           0x00
#define DPCD_REV_1_0                            0x10
#define DPCD_REV_1_1                            0x11

#define DPCD_MAX_LINK_RATE                      0x01
#define DPCD_1_62GBPS                           0x06
#define DPCD_2_7GBPS                            0x0A
#define DPCD_5_4GBPS                            0x14

#define DPCD_MAX_LANE_COUNT                     0x02
#define DPCD_ONE_LANE                           0x01
#define DPCD_TWO_LANES                          0x02
#define DPCD_FOUR_LANES                         0x04

#define DPCD_UP_TO_0_5                          0x01
#define DPCD_NO_AUX_HANDSHAKE_LINK_TRAINING     0x40

#define DPCD_DISPLAY_PORT                       0x00
#define DPCD_ANALOG                             0x02
#define DPCD_DVI_HDMI                           0x04
#define DPCD_OTHER                              0x06

#define DPCD_FORMAT_CONVERSION                  0x08

#define DPCD_ANSI_8B_10B                        0x01

#define DPCD_OUI_SUPPORTED                      0x80

#define DPCD_RECEIVE_PORT0_CAP_0                0x08
#define DPCD_RECEIVE_PORT0_CAP_1                0x09
#define DPCD_EDID_PRESENT                       0x02
#define DPCD_ASSOCIATED_TO_PRECEDING_PORT       0x04

#define DPCD_CAP_DISPLAY_PORT                   0x000
#define DPCD_CAP_ANALOG_VGA                     0x001
#define DPCD_CAP_DVI                            0x002
#define DPCD_CAP_HDMI                           0x003
#define DPCD_CAP_OTHER                          0x100

#define DPCD_LANE0_1_STATUS                     0x202
#define DPCD_LANE0_CR_DONE                      (1 << 0)
#define DPCD_LANE0_CHANNEL_EQ_DONE              (1 << 1)
#define DPCD_LANE0_SYMBOL_LOCKED                (1 << 2)
#define DPCD_LANE1_CR_DONE                      (1 << 4)
#define DPCD_LANE1_CHANNEL_EQ_DONE              (1 << 5)
#define DPCD_LANE1_SYMBOL_LOCKED                (1 << 6)

#define DPCD_LANE2_3_STATUS                     0x203
#define DPCD_LANE2_CR_DONE                      (1 << 0)
#define DPCD_LANE2_CHANNEL_EQ_DONE              (1 << 1)
#define DPCD_LANE2_SYMBOL_LOCKED                (1 << 2)
#define DPCD_LANE3_CR_DONE                      (1 << 4)
#define DPCD_LANE3_CHANNEL_EQ_DONE              (1 << 5)
#define DPCD_LANE3_SYMBOL_LOCKED                (1 << 6)

#define DPCD_LANE_ALIGN_STATUS_UPDATED          0x204
#define DPCD_INTERLANE_ALIGN_DONE               0x01
#define DPCD_DOWNSTREAM_PORT_STATUS_CHANGED     0x40
#define DPCD_LINK_STATUS_UPDATED                0x80

#define DPCD_SINK_STATUS                        0x205
#define DPCD_RECEIVE_PORT_0_STATUS              0x01

#endif /* DPCD_H */
