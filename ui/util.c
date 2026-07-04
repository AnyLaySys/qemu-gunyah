
#include "qemu/osdep.h"

#include "hw/pci/pci_device.h"
#include "hw/pci/pci_bus.h"
#include "qapi/error.h"
#include "ui/console.h"

static bool append_pci_address(char *buf, size_t buf_size, const PCIDevice *pci)
{
    PCIBus *bus = pci_get_bus(pci);
    if (bus->parent_dev != NULL) {
        append_pci_address(buf, buf_size, bus->parent_dev);
    }

    size_t len = strlen(buf);
    ssize_t written = snprintf(buf + len, buf_size - len, "/%02x.%x",
        PCI_SLOT(pci->devfn), PCI_FUNC(pci->devfn));

    return written > 0 && written < buf_size - len;
}

bool qemu_console_fill_device_address(QemuConsole *con,
                                      char *device_address,
                                      size_t size,
                                      Error **errp)
{
    DeviceState *dev = DEVICE(object_property_get_link(OBJECT(con),
                                                       "device",
                                                       &error_abort));
    PCIDevice *pci = (PCIDevice *) object_dynamic_cast(OBJECT(dev),
                                                       TYPE_PCI_DEVICE);

    if (pci == NULL) {
        error_setg(errp, "Setting device address of a display device: "
                   "Not a PCI device.");
        return false;
    }

    strncpy(device_address, "pci/0000", size);
    if (!append_pci_address(device_address, size, pci)) {
        error_setg(errp, "Setting device address of a display device: "
                   "Too many PCI devices in the chain.");
        return false;
    }

    return true;
}
