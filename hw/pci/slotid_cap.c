#include "qemu/osdep.h"
#include "hw/pci/slotid_cap.h"
#include "hw/pci/pci_device.h"
#include "qemu/error-report.h"
#include "qapi/error.h"

#define SLOTID_CAP_LENGTH 4
#define SLOTID_NSLOTS_SHIFT ctz32(PCI_SID_ESR_NSLOTS)

int slotid_cap_init(PCIDevice *d, int nslots,
                    uint8_t chassis,
                    unsigned offset,
                    Error **errp)
{
    int cap;

    if (!chassis) {
        error_setg(errp, "Bridge chassis not specified. Each bridge is required"
                   " to be assigned a unique chassis id > 0.");
        return -EINVAL;
    }
    if (nslots < 0 || nslots > (PCI_SID_ESR_NSLOTS >> SLOTID_NSLOTS_SHIFT)) {
        return -EINVAL;
    }

    cap = pci_add_capability(d, PCI_CAP_ID_SLOTID, offset,
                             SLOTID_CAP_LENGTH, errp);
    if (cap < 0) {
        return cap;
    }
    d->config[cap + PCI_SID_ESR] = PCI_SID_ESR_FIC |
        (nslots << SLOTID_NSLOTS_SHIFT);
    d->cmask[cap + PCI_SID_ESR] = 0xff;
    d->config[cap + PCI_SID_CHASSIS_NR] = chassis;
    d->wmask[cap + PCI_SID_CHASSIS_NR] = 0xff;

    d->cap_present |= QEMU_PCI_CAP_SLOTID;
    return 0;
}

void slotid_cap_cleanup(PCIDevice *d)
{
    d->cap_present &= ~QEMU_PCI_CAP_SLOTID;
}
