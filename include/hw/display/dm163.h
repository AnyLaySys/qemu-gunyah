
#ifndef HW_DISPLAY_DM163_H
#define HW_DISPLAY_DM163_H

#include "qom/object.h"
#include "hw/qdev-core.h"

#define TYPE_DM163 "dm163"
OBJECT_DECLARE_SIMPLE_TYPE(DM163State, DM163);

#define RGB_MATRIX_NUM_ROWS 8
#define RGB_MATRIX_NUM_COLS 8
#define DM163_NUM_LEDS (RGB_MATRIX_NUM_COLS * 3)
#define COLOR_BUFFER_SIZE (RGB_MATRIX_NUM_ROWS + 1)

typedef struct DM163State {
    DeviceState parent_obj;

    uint64_t bank0_shift_register[3];
    uint64_t bank1_shift_register[3];
    uint16_t latched_outputs[DM163_NUM_LEDS];
    uint16_t outputs[DM163_NUM_LEDS];
    qemu_irq sout;

    uint8_t sin;
    uint8_t dck;
    uint8_t rst_b;
    uint8_t lat_b;
    uint8_t selbk;
    uint8_t en_b;

    uint8_t activated_rows;

    QemuConsole *console;
    uint8_t redraw;
    uint32_t buffer[COLOR_BUFFER_SIZE][RGB_MATRIX_NUM_COLS];
    uint8_t last_buffer_idx;
    uint8_t buffer_idx_of_row[RGB_MATRIX_NUM_ROWS];
    uint8_t row_persistence_delay[RGB_MATRIX_NUM_ROWS];
} DM163State;

#endif /* HW_DISPLAY_DM163_H */
