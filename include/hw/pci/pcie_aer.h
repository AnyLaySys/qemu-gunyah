
#ifndef QEMU_PCIE_AER_H
#define QEMU_PCIE_AER_H

#include "hw/pci/pci_regs.h"


typedef struct PCIEAERErr {
    uint32_t status;    /* error status bits */
    uint16_t source_id; /* bdf */

#define PCIE_AER_ERR_IS_CORRECTABLE     0x1     /* correctable/uncorrectable */
#define PCIE_AER_ERR_MAYBE_ADVISORY     0x2     /* maybe advisory non-fatal */
#define PCIE_AER_ERR_HEADER_VALID       0x4     /* TLP header is logged */
#define PCIE_AER_ERR_TLP_PREFIX_PRESENT 0x8     /* TLP Prefix is logged */
    uint16_t flags;

    uint32_t header[4]; /* TLP header */
    uint32_t prefix[4]; /* TLP header prefix */
} PCIEAERErr;

typedef struct PCIEAERLog {

    uint16_t log_num;

#define PCIE_AER_LOG_MAX_DEFAULT        8
#define PCIE_AER_LOG_MAX_LIMIT          128
    uint16_t log_max;

    PCIEAERErr *log;
} PCIEAERLog;

typedef struct PCIEAERMsg {
    uint32_t severity;

    uint16_t source_id; /* bdf */
} PCIEAERMsg;

static inline bool
pcie_aer_msg_is_uncor(const PCIEAERMsg *msg)
{
    return msg->severity == PCI_ERR_ROOT_CMD_NONFATAL_EN ||
        msg->severity == PCI_ERR_ROOT_CMD_FATAL_EN;
}

extern const VMStateDescription vmstate_pcie_aer_log;

int pcie_aer_init(PCIDevice *dev, uint8_t cap_ver, uint16_t offset,
                  uint16_t size, Error **errp);
void pcie_aer_exit(PCIDevice *dev);
void pcie_aer_write_config(PCIDevice *dev,
                           uint32_t addr, uint32_t val, int len);

void pcie_aer_root_set_vector(PCIDevice *dev, unsigned int vector);
void pcie_aer_root_init(PCIDevice *dev);
void pcie_aer_root_reset(PCIDevice *dev);
void pcie_aer_root_write_config(PCIDevice *dev,
                                uint32_t addr, uint32_t val, int len,
                                uint32_t root_cmd_prev);

int pcie_aer_inject_error(PCIDevice *dev, const PCIEAERErr *err);
#endif /* QEMU_PCIE_AER_H */
