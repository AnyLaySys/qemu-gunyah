
#ifndef XLNX_DP_H
#define XLNX_DP_H

#include "hw/sysbus.h"
#include "ui/console.h"
#include "hw/misc/auxbus.h"
#include "hw/display/dpcd.h"
#include "hw/display/i2c-ddc.h"
#include "qemu/fifo8.h"
#include "qemu/units.h"
#include "audio/audio.h"
#include "qom/object.h"
#include "hw/ptimer.h"

#define AUD_CHBUF_MAX_DEPTH                 (32 * KiB)
#define MAX_QEMU_BUFFER_SIZE                (4 * KiB)

#define DP_CORE_REG_OFFSET                  (0x0000)
#define DP_CORE_REG_ARRAY_SIZE              (0x3B0 >> 2)
#define DP_AVBUF_REG_OFFSET                 (0xB000)
#define DP_AVBUF_REG_ARRAY_SIZE             (0x238 >> 2)
#define DP_VBLEND_REG_OFFSET                (0xA000)
#define DP_VBLEND_REG_ARRAY_SIZE            (0x1E0 >> 2)
#define DP_AUDIO_REG_OFFSET                 (0xC000)
#define DP_AUDIO_REG_ARRAY_SIZE             (0x50 >> 2)
#define DP_CONTAINER_SIZE                   (0xC050)

struct PixmanPlane {
    pixman_format_code_t format;
    DisplaySurface *surface;
};

struct XlnxDPState {
    SysBusDevice parent_obj;

    MemoryRegion container;

    uint32_t core_registers[DP_CORE_REG_ARRAY_SIZE];
    MemoryRegion core_iomem;

    uint32_t avbufm_registers[DP_AVBUF_REG_ARRAY_SIZE];
    MemoryRegion avbufm_iomem;

    uint32_t vblend_registers[DP_VBLEND_REG_ARRAY_SIZE];
    MemoryRegion vblend_iomem;

    uint32_t audio_registers[DP_AUDIO_REG_ARRAY_SIZE];
    MemoryRegion audio_iomem;

    QemuConsole *console;

    struct PixmanPlane g_plane;
    struct PixmanPlane v_plane;
    struct PixmanPlane bout_plane;

    QEMUSoundCard aud_card;
    SWVoiceOut *amixer_output_stream;
    int16_t audio_buffer_0[AUD_CHBUF_MAX_DEPTH];
    int16_t audio_buffer_1[AUD_CHBUF_MAX_DEPTH];
    size_t audio_data_available[2];
    int64_t temp_buffer[AUD_CHBUF_MAX_DEPTH];
    int16_t out_buffer[AUD_CHBUF_MAX_DEPTH];
    size_t byte_left; /* byte available in out_buffer. */
    size_t data_ptr;  /* next byte to be sent to QEMU. */

    XlnxDPDMAState *dpdma;

    qemu_irq irq;

    AUXBus *aux_bus;
    Fifo8 rx_fifo;
    Fifo8 tx_fifo;

    DPCDState *dpcd;
    I2CDDCState *edid;

    ptimer_state *vblank;
};

#define TYPE_XLNX_DP "xlnx.v-dp"
OBJECT_DECLARE_SIMPLE_TYPE(XlnxDPState, XLNX_DP)

#endif
