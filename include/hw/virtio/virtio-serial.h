
#ifndef QEMU_VIRTIO_SERIAL_H
#define QEMU_VIRTIO_SERIAL_H

#include "standard-headers/linux/virtio_console.h"
#include "hw/virtio/virtio.h"
#include "qom/object.h"

struct virtio_serial_conf {
    uint32_t max_virtserial_ports;
};

#define TYPE_VIRTIO_SERIAL_PORT "virtio-serial-port"
OBJECT_DECLARE_TYPE(VirtIOSerialPort, VirtIOSerialPortClass,
                    VIRTIO_SERIAL_PORT)

typedef struct VirtIOSerial VirtIOSerial;

#define TYPE_VIRTIO_SERIAL_BUS "virtio-serial-bus"
OBJECT_DECLARE_SIMPLE_TYPE(VirtIOSerialBus, VIRTIO_SERIAL_BUS)


struct VirtIOSerialPortClass {
    DeviceClass parent_class;

    bool is_console;

    DeviceRealize realize;
    DeviceUnrealize unrealize;

    void (*set_guest_connected)(VirtIOSerialPort *port, int guest_connected);

    void (*enable_backend)(VirtIOSerialPort *port, bool enable);

    void (*guest_ready)(VirtIOSerialPort *port);

    void (*guest_writable)(VirtIOSerialPort *port);

    ssize_t (*have_data)(VirtIOSerialPort *port, const uint8_t *buf,
                         ssize_t len);
};

struct VirtIOSerialPort {
    DeviceState dev;

    QTAILQ_ENTRY(VirtIOSerialPort) next;

    VirtIOSerial *vser;

    VirtQueue *ivq, *ovq;

    char *name;

    uint32_t id;

    VirtQueueElement *elem;

    uint32_t iov_idx;
    uint64_t iov_offset;

    QEMUBH *bh;

    bool guest_connected;
    bool host_connected;
    bool throttled;
};

struct VirtIOSerialBus {
    BusState qbus;

    VirtIOSerial *vser;

    uint32_t max_nr_ports;
};

typedef struct VirtIOSerialPostLoad {
    QEMUTimer *timer;
    uint32_t nr_active_ports;
    struct {
        VirtIOSerialPort *port;
        uint8_t host_connected;
    } *connected;
} VirtIOSerialPostLoad;

struct VirtIOSerial {
    VirtIODevice parent_obj;

    VirtQueue *c_ivq, *c_ovq;
    VirtQueue **ivqs, **ovqs;

    VirtIOSerialBus bus;

    QTAILQ_HEAD(, VirtIOSerialPort) ports;

    QLIST_ENTRY(VirtIOSerial) next;

    uint32_t *ports_map;

    struct VirtIOSerialPostLoad *post_load;

    virtio_serial_conf serial;

    uint64_t host_features;
};


int virtio_serial_open(VirtIOSerialPort *port);

int virtio_serial_close(VirtIOSerialPort *port);

ssize_t virtio_serial_write(VirtIOSerialPort *port, const uint8_t *buf,
                            size_t size);

size_t virtio_serial_guest_ready(VirtIOSerialPort *port);

void virtio_serial_throttle_port(VirtIOSerialPort *port, bool throttle);

#define TYPE_VIRTIO_SERIAL "virtio-serial-device"
OBJECT_DECLARE_SIMPLE_TYPE(VirtIOSerial, VIRTIO_SERIAL)

#endif
