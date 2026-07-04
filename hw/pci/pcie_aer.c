
#include "qemu/osdep.h"
#include "migration/vmstate.h"
#include "hw/pci/pci_bridge.h"
#include "hw/pci/pcie.h"
#include "hw/pci/msix.h"
#include "hw/pci/msi.h"
#include "hw/pci/pci_bus.h"
#include "hw/pci/pcie_regs.h"
#include "pci-internal.h"

#ifdef DEBUG_PCIE
# define PCIE_DPRINTF(fmt, ...)                                         \
    fprintf(stderr, "%s:%d " fmt, __func__, __LINE__, ## __VA_ARGS__)
#else
# define PCIE_DPRINTF(fmt, ...) do {} while (0)
#endif
#define PCIE_DEV_PRINTF(dev, fmt, ...)                                  \
    PCIE_DPRINTF("%s:%x "fmt, (dev)->name, (dev)->devfn, ## __VA_ARGS__)

#define PCI_ERR_SRC_COR_OFFS    0
#define PCI_ERR_SRC_UNCOR_OFFS  2

static uint32_t pcie_aer_uncor_default_severity(uint32_t status)
{
    switch (status) {
    case PCI_ERR_UNC_INTN:
    case PCI_ERR_UNC_DLP:
    case PCI_ERR_UNC_SDN:
    case PCI_ERR_UNC_RX_OVER:
    case PCI_ERR_UNC_FCP:
    case PCI_ERR_UNC_MALF_TLP:
        return PCI_ERR_ROOT_CMD_FATAL_EN;
    case PCI_ERR_UNC_POISON_TLP:
    case PCI_ERR_UNC_ECRC:
    case PCI_ERR_UNC_UNSUP:
    case PCI_ERR_UNC_COMP_TIME:
    case PCI_ERR_UNC_COMP_ABORT:
    case PCI_ERR_UNC_UNX_COMP:
    case PCI_ERR_UNC_ACSV:
    case PCI_ERR_UNC_MCBTLP:
    case PCI_ERR_UNC_ATOP_EBLOCKED:
    case PCI_ERR_UNC_TLP_PRF_BLOCKED:
        return PCI_ERR_ROOT_CMD_NONFATAL_EN;
    default:
        abort();
        break;
    }
    return PCI_ERR_ROOT_CMD_FATAL_EN;
}

static int aer_log_add_err(PCIEAERLog *aer_log, const PCIEAERErr *err)
{
    if (aer_log->log_num == aer_log->log_max) {
        return -1;
    }
    memcpy(&aer_log->log[aer_log->log_num], err, sizeof *err);
    aer_log->log_num++;
    return 0;
}

static void aer_log_del_err(PCIEAERLog *aer_log, PCIEAERErr *err)
{
    assert(aer_log->log_num);
    *err = aer_log->log[0];
    aer_log->log_num--;
    memmove(&aer_log->log[0], &aer_log->log[1],
            aer_log->log_num * sizeof *err);
}

static void aer_log_clear_all_err(PCIEAERLog *aer_log)
{
    aer_log->log_num = 0;
}

int pcie_aer_init(PCIDevice *dev, uint8_t cap_ver, uint16_t offset,
                  uint16_t size, Error **errp)
{
    pcie_add_capability(dev, PCI_EXT_CAP_ID_ERR, cap_ver,
                        offset, size);
    dev->exp.aer_cap = offset;

    if (dev->exp.aer_log.log_max > PCIE_AER_LOG_MAX_LIMIT) {
        error_setg(errp, "Invalid aer_log_max %d. The max number of aer log "
                "is %d", dev->exp.aer_log.log_max, PCIE_AER_LOG_MAX_LIMIT);
        return -EINVAL;
    }
    dev->exp.aer_log.log = g_malloc0(sizeof dev->exp.aer_log.log[0] *
                                        dev->exp.aer_log.log_max);

    pci_set_long(dev->w1cmask + offset + PCI_ERR_UNCOR_STATUS,
                 PCI_ERR_UNC_SUPPORTED);

    if (dev->cap_present & QEMU_PCIE_ERR_UNC_MASK) {
        pci_set_long(dev->config + offset + PCI_ERR_UNCOR_MASK,
                     PCI_ERR_UNC_MASK_DEFAULT);
        pci_set_long(dev->wmask + offset + PCI_ERR_UNCOR_MASK,
                     PCI_ERR_UNC_SUPPORTED);
    }

    pci_set_long(dev->config + offset + PCI_ERR_UNCOR_SEVER,
                 PCI_ERR_UNC_SEVERITY_DEFAULT);
    pci_set_long(dev->wmask + offset + PCI_ERR_UNCOR_SEVER,
                 PCI_ERR_UNC_SUPPORTED);

    pci_long_test_and_set_mask(dev->w1cmask + offset + PCI_ERR_COR_STATUS,
                               PCI_ERR_COR_SUPPORTED);

    pci_set_long(dev->config + offset + PCI_ERR_COR_MASK,
                 PCI_ERR_COR_MASK_DEFAULT);
    pci_set_long(dev->wmask + offset + PCI_ERR_COR_MASK,
                 PCI_ERR_COR_SUPPORTED);

    if (dev->exp.aer_log.log_max > 0) {
        pci_set_long(dev->config + offset + PCI_ERR_CAP,
                     PCI_ERR_CAP_ECRC_GENC | PCI_ERR_CAP_ECRC_CHKC |
                     PCI_ERR_CAP_MHRC);
        pci_set_long(dev->wmask + offset + PCI_ERR_CAP,
                     PCI_ERR_CAP_ECRC_GENE | PCI_ERR_CAP_ECRC_CHKE |
                     PCI_ERR_CAP_MHRE);
    } else {
        pci_set_long(dev->config + offset + PCI_ERR_CAP,
                     PCI_ERR_CAP_ECRC_GENC | PCI_ERR_CAP_ECRC_CHKC);
        pci_set_long(dev->wmask + offset + PCI_ERR_CAP,
                     PCI_ERR_CAP_ECRC_GENE | PCI_ERR_CAP_ECRC_CHKE);
    }

    switch (pcie_cap_get_type(dev)) {
    case PCI_EXP_TYPE_ROOT_PORT:
    case PCI_EXP_TYPE_DOWNSTREAM:
    case PCI_EXP_TYPE_UPSTREAM:
        pci_word_test_and_set_mask(dev->wmask + PCI_BRIDGE_CONTROL,
                                   PCI_BRIDGE_CTL_SERR);
        pci_long_test_and_set_mask(dev->w1cmask + PCI_STATUS,
                                   PCI_SEC_STATUS_RCV_SYSTEM_ERROR);
        break;
    default:
        break;
    }
    return 0;
}

void pcie_aer_exit(PCIDevice *dev)
{
    g_free(dev->exp.aer_log.log);
}

static void pcie_aer_update_uncor_status(PCIDevice *dev)
{
    uint8_t *aer_cap = dev->config + dev->exp.aer_cap;
    PCIEAERLog *aer_log = &dev->exp.aer_log;

    uint16_t i;
    for (i = 0; i < aer_log->log_num; i++) {
        pci_long_test_and_set_mask(aer_cap + PCI_ERR_UNCOR_STATUS,
                                   dev->exp.aer_log.log[i].status);
    }
}

static bool
pcie_aer_msg_alldev(PCIDevice *dev, const PCIEAERMsg *msg)
{
    uint16_t devctl = pci_get_word(dev->config + dev->exp.exp_cap +
                                   PCI_EXP_DEVCTL);
    if (!(pcie_aer_msg_is_uncor(msg) &&
          (pci_get_word(dev->config + PCI_COMMAND) & PCI_COMMAND_SERR)) &&
        !((msg->severity == PCI_ERR_ROOT_CMD_NONFATAL_EN) &&
          (devctl & PCI_EXP_DEVCTL_NFERE)) &&
        !((msg->severity == PCI_ERR_ROOT_CMD_COR_EN) &&
          (devctl & PCI_EXP_DEVCTL_CERE)) &&
        !((msg->severity == PCI_ERR_ROOT_CMD_FATAL_EN) &&
          (devctl & PCI_EXP_DEVCTL_FERE))) {
        return false;
    }

    pci_word_test_and_set_mask(dev->config + PCI_STATUS,
                               PCI_STATUS_SIG_SYSTEM_ERROR);

    if (!(msg->severity &
          pci_get_word(dev->config + dev->exp.exp_cap + PCI_EXP_DEVCTL))) {
        return false;
    }

    return true;
}

static bool pcie_aer_msg_vbridge(PCIDevice *dev, const PCIEAERMsg *msg)
{
    uint16_t bridge_control = pci_get_word(dev->config + PCI_BRIDGE_CONTROL);

    if (pcie_aer_msg_is_uncor(msg)) {
        pci_word_test_and_set_mask(dev->config + PCI_SEC_STATUS,
                                   PCI_SEC_STATUS_RCV_SYSTEM_ERROR);
    }

    if (!(bridge_control & PCI_BRIDGE_CTL_SERR)) {
        return false;
    }
    return true;
}

void pcie_aer_root_set_vector(PCIDevice *dev, unsigned int vector)
{
    uint8_t *aer_cap = dev->config + dev->exp.aer_cap;
    assert(vector < PCI_ERR_ROOT_IRQ_MAX);
    pci_long_test_and_clear_mask(aer_cap + PCI_ERR_ROOT_STATUS,
                                 PCI_ERR_ROOT_IRQ);
    pci_long_test_and_set_mask(aer_cap + PCI_ERR_ROOT_STATUS,
                               vector << PCI_ERR_ROOT_IRQ_SHIFT);
}

static unsigned int pcie_aer_root_get_vector(PCIDevice *dev)
{
    uint8_t *aer_cap = dev->config + dev->exp.aer_cap;
    uint32_t root_status = pci_get_long(aer_cap + PCI_ERR_ROOT_STATUS);
    return (root_status & PCI_ERR_ROOT_IRQ) >> PCI_ERR_ROOT_IRQ_SHIFT;
}

static uint32_t pcie_aer_status_to_cmd(uint32_t status)
{
    uint32_t cmd = 0;
    if (status & PCI_ERR_ROOT_COR_RCV) {
        cmd |= PCI_ERR_ROOT_CMD_COR_EN;
    }
    if (status & PCI_ERR_ROOT_NONFATAL_RCV) {
        cmd |= PCI_ERR_ROOT_CMD_NONFATAL_EN;
    }
    if (status & PCI_ERR_ROOT_FATAL_RCV) {
        cmd |= PCI_ERR_ROOT_CMD_FATAL_EN;
    }
    return cmd;
}

static void pcie_aer_root_notify(PCIDevice *dev)
{
    if (msix_enabled(dev)) {
        msix_notify(dev, pcie_aer_root_get_vector(dev));
    } else if (msi_enabled(dev)) {
        msi_notify(dev, pcie_aer_root_get_vector(dev));
    } else if (pci_intx(dev) != -1) {
        pci_irq_assert(dev);
    }
}

static void pcie_aer_msg_root_port(PCIDevice *dev, const PCIEAERMsg *msg)
{
    uint16_t cmd;
    uint8_t *aer_cap;
    uint32_t root_cmd;
    uint32_t root_status, prev_status;

    cmd = pci_get_word(dev->config + PCI_COMMAND);
    aer_cap = dev->config + dev->exp.aer_cap;
    root_cmd = pci_get_long(aer_cap + PCI_ERR_ROOT_COMMAND);
    prev_status = root_status = pci_get_long(aer_cap + PCI_ERR_ROOT_STATUS);

    if (cmd & PCI_COMMAND_SERR) {
    }

    switch (msg->severity) {
    case PCI_ERR_ROOT_CMD_COR_EN:
        if (root_status & PCI_ERR_ROOT_COR_RCV) {
            root_status |= PCI_ERR_ROOT_MULTI_COR_RCV;
        } else {
            pci_set_word(aer_cap + PCI_ERR_ROOT_ERR_SRC + PCI_ERR_SRC_COR_OFFS,
                         msg->source_id);
        }
        root_status |= PCI_ERR_ROOT_COR_RCV;
        break;
    case PCI_ERR_ROOT_CMD_NONFATAL_EN:
        root_status |= PCI_ERR_ROOT_NONFATAL_RCV;
        break;
    case PCI_ERR_ROOT_CMD_FATAL_EN:
        if (!(root_status & PCI_ERR_ROOT_UNCOR_RCV)) {
            root_status |= PCI_ERR_ROOT_FIRST_FATAL;
        }
        root_status |= PCI_ERR_ROOT_FATAL_RCV;
        break;
    default:
        abort();
        break;
    }
    if (pcie_aer_msg_is_uncor(msg)) {
        if (root_status & PCI_ERR_ROOT_UNCOR_RCV) {
            root_status |= PCI_ERR_ROOT_MULTI_UNCOR_RCV;
        } else {
            pci_set_word(aer_cap + PCI_ERR_ROOT_ERR_SRC +
                         PCI_ERR_SRC_UNCOR_OFFS, msg->source_id);
        }
        root_status |= PCI_ERR_ROOT_UNCOR_RCV;
    }
    pci_set_long(aer_cap + PCI_ERR_ROOT_STATUS, root_status);

    if (!(root_cmd & msg->severity) ||
        (pcie_aer_status_to_cmd(prev_status) & root_cmd)) {
        return;
    }

    pcie_aer_root_notify(dev);
}

static void pcie_aer_msg(PCIDevice *dev, const PCIEAERMsg *msg)
{
    uint8_t type;

    while (dev) {
        if (!pci_is_express(dev)) {
            return;
        }

        type = pcie_cap_get_type(dev);
        if ((type == PCI_EXP_TYPE_ROOT_PORT ||
            type == PCI_EXP_TYPE_UPSTREAM ||
            type == PCI_EXP_TYPE_DOWNSTREAM) &&
            !pcie_aer_msg_vbridge(dev, msg)) {
                return;
        }
        if (!pcie_aer_msg_alldev(dev, msg)) {
            return;
        }
        if (type == PCI_EXP_TYPE_ROOT_PORT) {
            pcie_aer_msg_root_port(dev, msg);
            return;
        }
        dev = pci_bridge_get_device(pci_get_bus(dev));
    }
}

static void pcie_aer_update_log(PCIDevice *dev, const PCIEAERErr *err)
{
    uint8_t *aer_cap = dev->config + dev->exp.aer_cap;
    uint8_t first_bit = ctz32(err->status);
    uint32_t errcap = pci_get_long(aer_cap + PCI_ERR_CAP);
    int i;

    assert(err->status);
    assert(!(err->status & (err->status - 1)));

    errcap &= ~(PCI_ERR_CAP_FEP_MASK | PCI_ERR_CAP_TLP);
    errcap |= PCI_ERR_CAP_FEP(first_bit);

    if (err->flags & PCIE_AER_ERR_HEADER_VALID) {
        for (i = 0; i < ARRAY_SIZE(err->header); ++i) {
            uint8_t *header_log =
                aer_cap + PCI_ERR_HEADER_LOG + i * sizeof err->header[0];
            stl_be_p(header_log, err->header[i]);
        }
    } else {
        assert(!(err->flags & PCIE_AER_ERR_TLP_PREFIX_PRESENT));
        memset(aer_cap + PCI_ERR_HEADER_LOG, 0, PCI_ERR_HEADER_LOG_SIZE);
    }

    if ((err->flags & PCIE_AER_ERR_TLP_PREFIX_PRESENT) &&
        (pci_get_long(dev->config + dev->exp.exp_cap + PCI_EXP_DEVCAP2) &
         PCI_EXP_DEVCAP2_EETLPP)) {
        for (i = 0; i < ARRAY_SIZE(err->prefix); ++i) {
            uint8_t *prefix_log =
                aer_cap + PCI_ERR_TLP_PREFIX_LOG + i * sizeof err->prefix[0];
            stl_be_p(prefix_log, err->prefix[i]);
        }
        errcap |= PCI_ERR_CAP_TLP;
    } else {
        memset(aer_cap + PCI_ERR_TLP_PREFIX_LOG, 0,
               PCI_ERR_TLP_PREFIX_LOG_SIZE);
    }
    pci_set_long(aer_cap + PCI_ERR_CAP, errcap);
}

static void pcie_aer_clear_log(PCIDevice *dev)
{
    uint8_t *aer_cap = dev->config + dev->exp.aer_cap;

    pci_long_test_and_clear_mask(aer_cap + PCI_ERR_CAP,
                                 PCI_ERR_CAP_FEP_MASK | PCI_ERR_CAP_TLP);

    memset(aer_cap + PCI_ERR_HEADER_LOG, 0, PCI_ERR_HEADER_LOG_SIZE);
    memset(aer_cap + PCI_ERR_TLP_PREFIX_LOG, 0, PCI_ERR_TLP_PREFIX_LOG_SIZE);
}

static void pcie_aer_clear_error(PCIDevice *dev)
{
    uint8_t *aer_cap = dev->config + dev->exp.aer_cap;
    uint32_t errcap = pci_get_long(aer_cap + PCI_ERR_CAP);
    PCIEAERLog *aer_log = &dev->exp.aer_log;
    PCIEAERErr err;

    if (!(errcap & PCI_ERR_CAP_MHRE) || !aer_log->log_num) {
        pcie_aer_clear_log(dev);
        return;
    }

    pcie_aer_update_uncor_status(dev);

    aer_log_del_err(aer_log, &err);
    pcie_aer_update_log(dev, &err);
}

static int pcie_aer_record_error(PCIDevice *dev,
                                 const PCIEAERErr *err)
{
    uint8_t *aer_cap = dev->config + dev->exp.aer_cap;
    uint32_t errcap = pci_get_long(aer_cap + PCI_ERR_CAP);
    int fep = PCI_ERR_CAP_FEP(errcap);

    assert(err->status);
    assert(!(err->status & (err->status - 1)));

    if (errcap & PCI_ERR_CAP_MHRE &&
        (pci_get_long(aer_cap + PCI_ERR_UNCOR_STATUS) & (1U << fep))) {
        if (aer_log_add_err(&dev->exp.aer_log, err) < 0) {
            return -1;
        }
        return 0;
    }

    pcie_aer_update_log(dev, err);
    return 0;
}

typedef struct PCIEAERInject {
    PCIDevice *dev;
    uint8_t *aer_cap;
    const PCIEAERErr *err;
    uint16_t devctl;
    uint16_t devsta;
    uint32_t error_status;
    bool unsupported_request;
    bool log_overflow;
    PCIEAERMsg msg;
} PCIEAERInject;

static bool pcie_aer_inject_cor_error(PCIEAERInject *inj,
                                      uint32_t uncor_status,
                                      bool is_advisory_nonfatal)
{
    PCIDevice *dev = inj->dev;

    inj->devsta |= PCI_EXP_DEVSTA_CED;
    if (inj->unsupported_request) {
        inj->devsta |= PCI_EXP_DEVSTA_URD;
    }
    pci_set_word(dev->config + dev->exp.exp_cap + PCI_EXP_DEVSTA, inj->devsta);

    if (inj->aer_cap) {
        uint32_t mask;
        pci_long_test_and_set_mask(inj->aer_cap + PCI_ERR_COR_STATUS,
                                   inj->error_status);
        mask = pci_get_long(inj->aer_cap + PCI_ERR_COR_MASK);
        if (mask & inj->error_status) {
            return false;
        }
        if (is_advisory_nonfatal) {
            uint32_t uncor_mask =
                pci_get_long(inj->aer_cap + PCI_ERR_UNCOR_MASK);
            if (!(uncor_mask & uncor_status)) {
                inj->log_overflow = !!pcie_aer_record_error(dev, inj->err);
            }
            pci_long_test_and_set_mask(inj->aer_cap + PCI_ERR_UNCOR_STATUS,
                                       uncor_status);
        }
    }

    if (inj->unsupported_request && !(inj->devctl & PCI_EXP_DEVCTL_URRE)) {
        return false;
    }
    if (!(inj->devctl & PCI_EXP_DEVCTL_CERE)) {
        return false;
    }

    inj->msg.severity = PCI_ERR_ROOT_CMD_COR_EN;
    return true;
}

static bool pcie_aer_inject_uncor_error(PCIEAERInject *inj, bool is_fatal)
{
    PCIDevice *dev = inj->dev;
    uint16_t cmd;

    if (is_fatal) {
        inj->devsta |= PCI_EXP_DEVSTA_FED;
    } else {
        inj->devsta |= PCI_EXP_DEVSTA_NFED;
    }
    if (inj->unsupported_request) {
        inj->devsta |= PCI_EXP_DEVSTA_URD;
    }
    pci_set_long(dev->config + dev->exp.exp_cap + PCI_EXP_DEVSTA, inj->devsta);

    if (inj->aer_cap) {
        uint32_t mask = pci_get_long(inj->aer_cap + PCI_ERR_UNCOR_MASK);
        if (mask & inj->error_status) {
            pci_long_test_and_set_mask(inj->aer_cap + PCI_ERR_UNCOR_STATUS,
                                       inj->error_status);
            return false;
        }

        inj->log_overflow = !!pcie_aer_record_error(dev, inj->err);
        pci_long_test_and_set_mask(inj->aer_cap + PCI_ERR_UNCOR_STATUS,
                                   inj->error_status);
    }

    cmd = pci_get_word(dev->config + PCI_COMMAND);
    if (inj->unsupported_request &&
        !(inj->devctl & PCI_EXP_DEVCTL_URRE) && !(cmd & PCI_COMMAND_SERR)) {
        return false;
    }
    if (is_fatal) {
        if (!((cmd & PCI_COMMAND_SERR) ||
              (inj->devctl & PCI_EXP_DEVCTL_FERE))) {
            return false;
        }
        inj->msg.severity = PCI_ERR_ROOT_CMD_FATAL_EN;
    } else {
        if (!((cmd & PCI_COMMAND_SERR) ||
              (inj->devctl & PCI_EXP_DEVCTL_NFERE))) {
            return false;
        }
        inj->msg.severity = PCI_ERR_ROOT_CMD_NONFATAL_EN;
    }
    return true;
}

int pcie_aer_inject_error(PCIDevice *dev, const PCIEAERErr *err)
{
    uint8_t *aer_cap = NULL;
    uint16_t devctl = 0;
    uint16_t devsta = 0;
    uint32_t error_status = err->status;
    PCIEAERInject inj;

    if (!pci_is_express(dev)) {
        return -ENOSYS;
    }

    if (err->flags & PCIE_AER_ERR_IS_CORRECTABLE) {
        error_status &= PCI_ERR_COR_SUPPORTED;
    } else {
        error_status &= PCI_ERR_UNC_SUPPORTED;
    }

    if (!error_status || (error_status & (error_status - 1))) {
        return -EINVAL;
    }

    if (dev->exp.aer_cap) {
        uint8_t *exp_cap = dev->config + dev->exp.exp_cap;
        aer_cap = dev->config + dev->exp.aer_cap;
        devctl = pci_get_long(exp_cap + PCI_EXP_DEVCTL);
        devsta = pci_get_long(exp_cap + PCI_EXP_DEVSTA);
    }

    inj.dev = dev;
    inj.aer_cap = aer_cap;
    inj.err = err;
    inj.devctl = devctl;
    inj.devsta = devsta;
    inj.error_status = error_status;
    inj.unsupported_request = !(err->flags & PCIE_AER_ERR_IS_CORRECTABLE) &&
        err->status == PCI_ERR_UNC_UNSUP;
    inj.log_overflow = false;

    if (err->flags & PCIE_AER_ERR_IS_CORRECTABLE) {
        if (!pcie_aer_inject_cor_error(&inj, 0, false)) {
            return 0;
        }
    } else {
        bool is_fatal =
            pcie_aer_uncor_default_severity(error_status) ==
            PCI_ERR_ROOT_CMD_FATAL_EN;
        if (aer_cap) {
            is_fatal =
                error_status & pci_get_long(aer_cap + PCI_ERR_UNCOR_SEVER);
        }
        if (!is_fatal && (err->flags & PCIE_AER_ERR_MAYBE_ADVISORY)) {
            inj.error_status = PCI_ERR_COR_ADV_NONFATAL;
            if (!pcie_aer_inject_cor_error(&inj, error_status, true)) {
                return 0;
            }
        } else {
            if (!pcie_aer_inject_uncor_error(&inj, is_fatal)) {
                return 0;
            }
        }
    }

    inj.msg.source_id = err->source_id;
    pcie_aer_msg(dev, &inj.msg);

    if (inj.log_overflow) {
        PCIEAERErr header_log_overflow = {
            .status = PCI_ERR_COR_HL_OVERFLOW,
            .flags = PCIE_AER_ERR_IS_CORRECTABLE,
        };
        int ret = pcie_aer_inject_error(dev, &header_log_overflow);
        assert(!ret);
    }
    return 0;
}

void pcie_aer_write_config(PCIDevice *dev,
                           uint32_t addr, uint32_t val, int len)
{
    uint8_t *aer_cap = dev->config + dev->exp.aer_cap;
    uint32_t errcap = pci_get_long(aer_cap + PCI_ERR_CAP);
    uint32_t first_error = 1U << PCI_ERR_CAP_FEP(errcap);
    uint32_t uncorsta = pci_get_long(aer_cap + PCI_ERR_UNCOR_STATUS);

    if (!(uncorsta & first_error)) {
        pcie_aer_clear_error(dev);
    } else if (errcap & PCI_ERR_CAP_MHRE) {
        pcie_aer_update_uncor_status(dev);
    } else {
        aer_log_clear_all_err(&dev->exp.aer_log);
    }
}

void pcie_aer_root_init(PCIDevice *dev)
{
    uint16_t pos = dev->exp.aer_cap;

    pci_set_long(dev->wmask + pos + PCI_ERR_ROOT_COMMAND,
                 PCI_ERR_ROOT_CMD_EN_MASK);
    pci_set_long(dev->w1cmask + pos + PCI_ERR_ROOT_STATUS,
                 PCI_ERR_ROOT_STATUS_REPORT_MASK);
    pci_set_long(dev->cmask + pos + PCI_ERR_ROOT_STATUS,
                 ~PCI_ERR_ROOT_IRQ);
}

void pcie_aer_root_reset(PCIDevice *dev)
{
    uint8_t* aer_cap = dev->config + dev->exp.aer_cap;

    pci_set_long(aer_cap + PCI_ERR_ROOT_COMMAND, 0);

}

void pcie_aer_root_write_config(PCIDevice *dev,
                                uint32_t addr, uint32_t val, int len,
                                uint32_t root_cmd_prev)
{
    uint8_t *aer_cap = dev->config + dev->exp.aer_cap;
    uint32_t root_status = pci_get_long(aer_cap + PCI_ERR_ROOT_STATUS);
    uint32_t enabled_cmd = pcie_aer_status_to_cmd(root_status);
    uint32_t root_cmd = pci_get_long(aer_cap + PCI_ERR_ROOT_COMMAND);
    if (!msix_enabled(dev) && !msi_enabled(dev)) {
        if (pci_intx(dev) != -1) {
            pci_set_irq(dev, !!(root_cmd & enabled_cmd));
        }
        return;
    }

    if ((root_cmd_prev & enabled_cmd) || !(root_cmd & enabled_cmd)) {
        return;
    }

    pcie_aer_root_notify(dev);
}

static const VMStateDescription vmstate_pcie_aer_err = {
    .name = "PCIE_AER_ERROR",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(status, PCIEAERErr),
        VMSTATE_UINT16(source_id, PCIEAERErr),
        VMSTATE_UINT16(flags, PCIEAERErr),
        VMSTATE_UINT32_ARRAY(header, PCIEAERErr, 4),
        VMSTATE_UINT32_ARRAY(prefix, PCIEAERErr, 4),
        VMSTATE_END_OF_LIST()
    }
};

static bool pcie_aer_state_log_num_valid(void *opaque, int version_id)
{
    PCIEAERLog *s = opaque;

    return s->log_num <= s->log_max;
}

const VMStateDescription vmstate_pcie_aer_log = {
    .name = "PCIE_AER_ERROR_LOG",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT16(log_num, PCIEAERLog),
        VMSTATE_UINT16_EQUAL(log_max, PCIEAERLog, NULL),
        VMSTATE_VALIDATE("log_num <= log_max", pcie_aer_state_log_num_valid),
        VMSTATE_STRUCT_VARRAY_POINTER_UINT16(log, PCIEAERLog, log_num,
                              vmstate_pcie_aer_err, PCIEAERErr),
        VMSTATE_END_OF_LIST()
    }
};

typedef struct PCIEAERErrorName {
    const char *name;
    uint32_t val;
    bool correctable;
} PCIEAERErrorName;

static const struct PCIEAERErrorName pcie_aer_error_list[] = {
    {
        .name = "DLP",
        .val = PCI_ERR_UNC_DLP,
        .correctable = false,
    }, {
        .name = "SDN",
        .val = PCI_ERR_UNC_SDN,
        .correctable = false,
    }, {
        .name = "POISON_TLP",
        .val = PCI_ERR_UNC_POISON_TLP,
        .correctable = false,
    }, {
        .name = "FCP",
        .val = PCI_ERR_UNC_FCP,
        .correctable = false,
    }, {
        .name = "COMP_TIME",
        .val = PCI_ERR_UNC_COMP_TIME,
        .correctable = false,
    }, {
        .name = "COMP_ABORT",
        .val = PCI_ERR_UNC_COMP_ABORT,
        .correctable = false,
    }, {
        .name = "UNX_COMP",
        .val = PCI_ERR_UNC_UNX_COMP,
        .correctable = false,
    }, {
        .name = "RX_OVER",
        .val = PCI_ERR_UNC_RX_OVER,
        .correctable = false,
    }, {
        .name = "MALF_TLP",
        .val = PCI_ERR_UNC_MALF_TLP,
        .correctable = false,
    }, {
        .name = "ECRC",
        .val = PCI_ERR_UNC_ECRC,
        .correctable = false,
    }, {
        .name = "UNSUP",
        .val = PCI_ERR_UNC_UNSUP,
        .correctable = false,
    }, {
        .name = "ACSV",
        .val = PCI_ERR_UNC_ACSV,
        .correctable = false,
    }, {
        .name = "INTN",
        .val = PCI_ERR_UNC_INTN,
        .correctable = false,
    }, {
        .name = "MCBTLP",
        .val = PCI_ERR_UNC_MCBTLP,
        .correctable = false,
    }, {
        .name = "ATOP_EBLOCKED",
        .val = PCI_ERR_UNC_ATOP_EBLOCKED,
        .correctable = false,
    }, {
        .name = "TLP_PRF_BLOCKED",
        .val = PCI_ERR_UNC_TLP_PRF_BLOCKED,
        .correctable = false,
    }, {
        .name = "RCVR",
        .val = PCI_ERR_COR_RCVR,
        .correctable = true,
    }, {
        .name = "BAD_TLP",
        .val = PCI_ERR_COR_BAD_TLP,
        .correctable = true,
    }, {
        .name = "BAD_DLLP",
        .val = PCI_ERR_COR_BAD_DLLP,
        .correctable = true,
    }, {
        .name = "REP_ROLL",
        .val = PCI_ERR_COR_REP_ROLL,
        .correctable = true,
    }, {
        .name = "REP_TIMER",
        .val = PCI_ERR_COR_REP_TIMER,
        .correctable = true,
    }, {
        .name = "ADV_NONFATAL",
        .val = PCI_ERR_COR_ADV_NONFATAL,
        .correctable = true,
    }, {
        .name = "INTERNAL",
        .val = PCI_ERR_COR_INTERNAL,
        .correctable = true,
    }, {
        .name = "HL_OVERFLOW",
        .val = PCI_ERR_COR_HL_OVERFLOW,
        .correctable = true,
    },
};

int pcie_aer_parse_error_string(const char *error_name,
                                uint32_t *status, bool *correctable)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(pcie_aer_error_list); i++) {
        const  PCIEAERErrorName *e = &pcie_aer_error_list[i];
        if (strcmp(error_name, e->name)) {
            continue;
        }

        *status = e->val;
        *correctable = e->correctable;
        return 0;
    }
    return -EINVAL;
}
