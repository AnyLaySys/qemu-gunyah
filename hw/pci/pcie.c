
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/pci/pcie.h"
#include "hw/pci/pci_device.h"
#include "hw/pci/pci_bus.h"
#include "hw/pci/pcie_regs.h"

static void
pcie_cap_v1_fill(PCIDevice *dev, uint8_t port, uint8_t type, uint8_t version)
{
    uint8_t *exp_cap = dev->config + dev->exp.exp_cap;
    uint8_t *cmask = dev->cmask + dev->exp.exp_cap;

    pci_set_word(exp_cap + PCI_EXP_FLAGS,
                 ((type << PCI_EXP_FLAGS_TYPE_SHIFT) & PCI_EXP_FLAGS_TYPE) |
                 version);

    uint32_t devcap = PCI_EXP_DEVCAP_RBER;

    if (dev->cap_present & QEMU_PCIE_EXT_TAG) {
        devcap = PCI_EXP_DEVCAP_RBER | PCI_EXP_DEVCAP_EXT_TAG;
    }

    pci_set_long(exp_cap + PCI_EXP_DEVCAP, devcap);

    pci_set_long(exp_cap + PCI_EXP_LNKCAP,
                 (port << PCI_EXP_LNKCAP_PN_SHIFT) |
                 PCI_EXP_LNKCAP_ASPMS_0S |
                 QEMU_PCI_EXP_LNKCAP_MLW(QEMU_PCI_EXP_LNK_X1) |
                 QEMU_PCI_EXP_LNKCAP_MLS(QEMU_PCI_EXP_LNK_2_5GT));

    pci_set_word(exp_cap + PCI_EXP_LNKSTA,
                 QEMU_PCI_EXP_LNKSTA_NLW(QEMU_PCI_EXP_LNK_X1) |
                 QEMU_PCI_EXP_LNKSTA_CLS(QEMU_PCI_EXP_LNK_2_5GT));

    pci_set_word(cmask + PCI_EXP_LNKSTA, 0);
}

static void pcie_cap_fill_lnk(uint8_t *exp_cap, PCIExpLinkWidth width,
                              PCIExpLinkSpeed speed)
{
    pci_long_test_and_clear_mask(exp_cap + PCI_EXP_LNKCAP,
                                 PCI_EXP_LNKCAP_MLW | PCI_EXP_LNKCAP_SLS);
    pci_long_test_and_set_mask(exp_cap + PCI_EXP_LNKCAP,
                               QEMU_PCI_EXP_LNKCAP_MLW(width) |
                               QEMU_PCI_EXP_LNKCAP_MLS(speed));

    if (speed > QEMU_PCI_EXP_LNK_2_5GT) {
        pci_word_test_and_clear_mask(exp_cap + PCI_EXP_LNKCTL2,
                                     PCI_EXP_LNKCTL2_TLS);
        pci_word_test_and_set_mask(exp_cap + PCI_EXP_LNKCTL2,
                                   QEMU_PCI_EXP_LNKCAP_MLS(speed) &
                                   PCI_EXP_LNKCTL2_TLS);
    }

    if (speed > QEMU_PCI_EXP_LNK_5GT) {
        pci_long_test_and_clear_mask(exp_cap + PCI_EXP_LNKCAP2, ~0U);
        pci_long_test_and_set_mask(exp_cap + PCI_EXP_LNKCAP2,
                                   PCI_EXP_LNKCAP2_SLS_2_5GB |
                                   PCI_EXP_LNKCAP2_SLS_5_0GB |
                                   PCI_EXP_LNKCAP2_SLS_8_0GB);
        if (speed > QEMU_PCI_EXP_LNK_8GT) {
            pci_long_test_and_set_mask(exp_cap + PCI_EXP_LNKCAP2,
                                       PCI_EXP_LNKCAP2_SLS_16_0GB);
        }
        if (speed > QEMU_PCI_EXP_LNK_16GT) {
            pci_long_test_and_set_mask(exp_cap + PCI_EXP_LNKCAP2,
                                       PCI_EXP_LNKCAP2_SLS_32_0GB);
        }
        if (speed > QEMU_PCI_EXP_LNK_32GT) {
            pci_long_test_and_set_mask(exp_cap + PCI_EXP_LNKCAP2,
                                       PCI_EXP_LNKCAP2_SLS_64_0GB);
        }
    }
}

void pcie_cap_fill_link_ep_usp(PCIDevice *dev, PCIExpLinkWidth width,
                               PCIExpLinkSpeed speed)
{
    uint8_t *exp_cap = dev->config + dev->exp.exp_cap;

    pci_long_test_and_clear_mask(exp_cap + PCI_EXP_LNKSTA,
                                 PCI_EXP_LNKSTA_CLS | PCI_EXP_LNKSTA_NLW);
    pci_long_test_and_set_mask(exp_cap + PCI_EXP_LNKSTA,
                               QEMU_PCI_EXP_LNKSTA_NLW(width) |
                               QEMU_PCI_EXP_LNKSTA_CLS(speed));

    pcie_cap_fill_lnk(exp_cap, width, speed);
}

int pcie_cap_init(PCIDevice *dev, uint8_t offset,
                  uint8_t type, uint8_t port,
                  Error **errp)
{
    int pos;
    uint8_t *exp_cap;

    assert(pci_is_express(dev));

    pos = pci_add_capability(dev, PCI_CAP_ID_EXP, offset,
                             PCI_EXP_VER2_SIZEOF, errp);
    if (pos < 0) {
        return pos;
    }
    dev->exp.exp_cap = pos;
    exp_cap = dev->config + pos;

    pcie_cap_v1_fill(dev, port, type, PCI_EXP_FLAGS_VER2);

    pci_set_long(exp_cap + PCI_EXP_DEVCAP2,
                 PCI_EXP_DEVCAP2_EFF | PCI_EXP_DEVCAP2_EETLPP);

    pci_set_word(dev->wmask + pos + PCI_EXP_DEVCTL2, PCI_EXP_DEVCTL2_EETLPPB);

    if (dev->cap_present & QEMU_PCIE_EXTCAP_INIT) {
        pci_set_long(dev->wmask + PCI_CONFIG_SPACE_SIZE, 0);
    }

    return pos;
}

int pcie_cap_v1_init(PCIDevice *dev, uint8_t offset, uint8_t type,
                     uint8_t port)
{
    int pos;
    Error *local_err = NULL;

    assert(pci_is_express(dev));

    pos = pci_add_capability(dev, PCI_CAP_ID_EXP, offset,
                             PCI_EXP_VER1_SIZEOF, &local_err);
    if (pos < 0) {
        error_report_err(local_err);
        return pos;
    }
    dev->exp.exp_cap = pos;

    pcie_cap_v1_fill(dev, port, type, PCI_EXP_FLAGS_VER1);

    return pos;
}

static int
pcie_endpoint_cap_common_init(PCIDevice *dev, uint8_t offset, uint8_t cap_size)
{
    uint8_t type = PCI_EXP_TYPE_ENDPOINT;
    Error *local_err = NULL;
    int ret;

    if (pci_bus_is_express(pci_get_bus(dev))
        && pci_bus_is_root(pci_get_bus(dev))) {
        type = PCI_EXP_TYPE_RC_END;
    }

    if (cap_size == PCI_EXP_VER1_SIZEOF) {
        return pcie_cap_v1_init(dev, offset, type, 0);
    } else {
        ret = pcie_cap_init(dev, offset, type, 0, &local_err);

        if (ret < 0) {
            error_report_err(local_err);
        }

        return ret;
    }
}

int pcie_endpoint_cap_init(PCIDevice *dev, uint8_t offset)
{
    return pcie_endpoint_cap_common_init(dev, offset, PCI_EXP_VER2_SIZEOF);
}

int pcie_endpoint_cap_v1_init(PCIDevice *dev, uint8_t offset)
{
    return pcie_endpoint_cap_common_init(dev, offset, PCI_EXP_VER1_SIZEOF);
}

void pcie_cap_exit(PCIDevice *dev)
{
    pci_del_capability(dev, PCI_CAP_ID_EXP, PCI_EXP_VER2_SIZEOF);
}

void pcie_cap_v1_exit(PCIDevice *dev)
{
    pci_del_capability(dev, PCI_CAP_ID_EXP, PCI_EXP_VER1_SIZEOF);
}

uint8_t pcie_cap_get_type(const PCIDevice *dev)
{
    uint32_t pos = dev->exp.exp_cap;
    assert(pos > 0);
    return (pci_get_word(dev->config + pos + PCI_EXP_FLAGS) &
            PCI_EXP_FLAGS_TYPE) >> PCI_EXP_FLAGS_TYPE_SHIFT;
}

uint8_t pcie_cap_get_version(const PCIDevice *dev)
{
    uint32_t pos = dev->exp.exp_cap;
    assert(pos > 0);
    return pci_get_word(dev->config + pos + PCI_EXP_FLAGS) & PCI_EXP_FLAGS_VERS;
}

void pcie_cap_flags_set_vector(PCIDevice *dev, uint8_t vector)
{
    uint8_t *exp_cap = dev->config + dev->exp.exp_cap;
    assert(vector < 32);
    pci_word_test_and_clear_mask(exp_cap + PCI_EXP_FLAGS, PCI_EXP_FLAGS_IRQ);
    pci_word_test_and_set_mask(exp_cap + PCI_EXP_FLAGS,
                               vector << PCI_EXP_FLAGS_IRQ_SHIFT);
}

uint8_t pcie_cap_flags_get_vector(PCIDevice *dev)
{
    return (pci_get_word(dev->config + dev->exp.exp_cap + PCI_EXP_FLAGS) &
            PCI_EXP_FLAGS_IRQ) >> PCI_EXP_FLAGS_IRQ_SHIFT;
}

void pcie_cap_deverr_init(PCIDevice *dev)
{
    uint32_t pos = dev->exp.exp_cap;
    pci_long_test_and_set_mask(dev->config + pos + PCI_EXP_DEVCAP,
                               PCI_EXP_DEVCAP_RBER);
    pci_long_test_and_set_mask(dev->wmask + pos + PCI_EXP_DEVCTL,
                               PCI_EXP_DEVCTL_CERE | PCI_EXP_DEVCTL_NFERE |
                               PCI_EXP_DEVCTL_FERE | PCI_EXP_DEVCTL_URRE);
    pci_long_test_and_set_mask(dev->w1cmask + pos + PCI_EXP_DEVSTA,
                               PCI_EXP_DEVSTA_CED | PCI_EXP_DEVSTA_NFED |
                               PCI_EXP_DEVSTA_FED | PCI_EXP_DEVSTA_URD);
}

void pcie_cap_deverr_reset(PCIDevice *dev)
{
    uint8_t *devctl = dev->config + dev->exp.exp_cap + PCI_EXP_DEVCTL;
    pci_long_test_and_clear_mask(devctl,
                                 PCI_EXP_DEVCTL_CERE | PCI_EXP_DEVCTL_NFERE |
                                 PCI_EXP_DEVCTL_FERE | PCI_EXP_DEVCTL_URRE);
}

void pcie_cap_lnkctl_init(PCIDevice *dev)
{
    uint32_t pos = dev->exp.exp_cap;
    pci_long_test_and_set_mask(dev->wmask + pos + PCI_EXP_LNKCTL,
                               PCI_EXP_LNKCTL_CCC | PCI_EXP_LNKCTL_ES);
}

void pcie_cap_lnkctl_reset(PCIDevice *dev)
{
    uint8_t *lnkctl = dev->config + dev->exp.exp_cap + PCI_EXP_LNKCTL;
    pci_long_test_and_clear_mask(lnkctl,
                                 PCI_EXP_LNKCTL_CCC | PCI_EXP_LNKCTL_ES);
}

void pcie_cap_root_init(PCIDevice *dev)
{
    pci_set_word(dev->wmask + dev->exp.exp_cap + PCI_EXP_RTCTL,
                 PCI_EXP_RTCTL_SECEE | PCI_EXP_RTCTL_SENFEE |
                 PCI_EXP_RTCTL_SEFEE);
}

void pcie_cap_root_reset(PCIDevice *dev)
{
    pci_set_word(dev->config + dev->exp.exp_cap + PCI_EXP_RTCTL, 0);
}

void pcie_cap_flr_init(PCIDevice *dev)
{
    pci_long_test_and_set_mask(dev->config + dev->exp.exp_cap + PCI_EXP_DEVCAP,
                               PCI_EXP_DEVCAP_FLR);

    pci_word_test_and_set_mask(dev->wmask + dev->exp.exp_cap + PCI_EXP_DEVCTL,
                               PCI_EXP_DEVCTL_BCR_FLR);
}

void pcie_cap_flr_write_config(PCIDevice *dev,
                               uint32_t addr, uint32_t val, int len)
{
    uint8_t *devctl = dev->config + dev->exp.exp_cap + PCI_EXP_DEVCTL;
    if (pci_get_word(devctl) & PCI_EXP_DEVCTL_BCR_FLR) {
        pci_device_reset(dev);
        pci_word_test_and_clear_mask(devctl, PCI_EXP_DEVCTL_BCR_FLR);
    }
}

void pcie_cap_arifwd_init(PCIDevice *dev)
{
    uint32_t pos = dev->exp.exp_cap;
    pci_long_test_and_set_mask(dev->config + pos + PCI_EXP_DEVCAP2,
                               PCI_EXP_DEVCAP2_ARI);
    pci_long_test_and_set_mask(dev->wmask + pos + PCI_EXP_DEVCTL2,
                               PCI_EXP_DEVCTL2_ARI);
}

void pcie_cap_arifwd_reset(PCIDevice *dev)
{
    uint8_t *devctl2 = dev->config + dev->exp.exp_cap + PCI_EXP_DEVCTL2;
    pci_long_test_and_clear_mask(devctl2, PCI_EXP_DEVCTL2_ARI);
}

bool pcie_cap_is_arifwd_enabled(const PCIDevice *dev)
{
    if (!pci_is_express(dev)) {
        return false;
    }
    if (!dev->exp.exp_cap) {
        return false;
    }

    return pci_get_long(dev->config + dev->exp.exp_cap + PCI_EXP_DEVCTL2) &
        PCI_EXP_DEVCTL2_ARI;
}


static uint16_t pcie_find_capability_list(PCIDevice *dev, uint32_t cap_id,
                                          uint16_t *prev_p)
{
    uint16_t prev = 0;
    uint16_t next;
    uint32_t header = pci_get_long(dev->config + PCI_CONFIG_SPACE_SIZE);

    if (!header) {
        next = 0;
        goto out;
    }
    for (next = PCI_CONFIG_SPACE_SIZE; next;
         prev = next, next = PCI_EXT_CAP_NEXT(header)) {

        assert(next >= PCI_CONFIG_SPACE_SIZE);
        assert(next <= PCIE_CONFIG_SPACE_SIZE - 8);

        header = pci_get_long(dev->config + next);
        if (PCI_EXT_CAP_ID(header) == cap_id) {
            break;
        }
    }

out:
    if (prev_p) {
        *prev_p = prev;
    }
    return next;
}

uint16_t pcie_find_capability(PCIDevice *dev, uint16_t cap_id)
{
    return pcie_find_capability_list(dev, cap_id, NULL);
}

static void pcie_ext_cap_set_next(PCIDevice *dev, uint16_t pos, uint16_t next)
{
    uint32_t header = pci_get_long(dev->config + pos);
    assert(!(next & (PCI_EXT_CAP_ALIGN - 1)));
    header = (header & ~PCI_EXT_CAP_NEXT_MASK) |
        ((next << PCI_EXT_CAP_NEXT_SHIFT) & PCI_EXT_CAP_NEXT_MASK);
    pci_set_long(dev->config + pos, header);
}

void pcie_add_capability(PCIDevice *dev,
                         uint16_t cap_id, uint8_t cap_ver,
                         uint16_t offset, uint16_t size)
{
    assert(offset >= PCI_CONFIG_SPACE_SIZE);
    assert(offset < (uint16_t)(offset + size));
    assert((uint16_t)(offset + size) <= PCIE_CONFIG_SPACE_SIZE);
    assert(size >= 8);
    assert(pci_is_express(dev));

    if (offset != PCI_CONFIG_SPACE_SIZE) {
        uint16_t prev;

        pcie_find_capability_list(dev, 0xffffffff, &prev);
        assert(prev >= PCI_CONFIG_SPACE_SIZE);
        pcie_ext_cap_set_next(dev, prev, offset);
    }
    pci_set_long(dev->config + offset, PCI_EXT_CAP(cap_id, cap_ver, 0));

    memset(dev->wmask + offset, 0, size);
    memset(dev->w1cmask + offset, 0, size);
    memset(dev->cmask + offset, 0xFF, size);
}

void pcie_ari_init(PCIDevice *dev, uint16_t offset)
{
    uint16_t nextfn = dev->cap_present & QEMU_PCIE_ARI_NEXTFN_1 ? 1 : 0;

    pcie_add_capability(dev, PCI_EXT_CAP_ID_ARI, PCI_ARI_VER,
                        offset, PCI_ARI_SIZEOF);
    pci_set_long(dev->config + offset + PCI_ARI_CAP, (nextfn & 0xff) << 8);
}

void pcie_dev_ser_num_init(PCIDevice *dev, uint16_t offset, uint64_t ser_num)
{
    static const int pci_dsn_ver = 1;
    static const int pci_dsn_cap = 4;

    pcie_add_capability(dev, PCI_EXT_CAP_ID_DSN, pci_dsn_ver, offset,
                        PCI_EXT_CAP_DSN_SIZEOF);
    pci_set_quad(dev->config + offset + pci_dsn_cap, ser_num);
}

void pcie_ats_init(PCIDevice *dev, uint16_t offset, bool aligned)
{
    pcie_add_capability(dev, PCI_EXT_CAP_ID_ATS, 0x1,
                        offset, PCI_EXT_CAP_ATS_SIZEOF);

    dev->exp.ats_cap = offset;

    if (aligned) {
        pci_set_word(dev->config + offset + PCI_ATS_CAP,
                     PCI_ATS_CAP_PAGE_ALIGNED);
    }
    pci_set_word(dev->config + offset + PCI_ATS_CTRL, 0);

    pci_set_word(dev->wmask + dev->exp.ats_cap + PCI_ATS_CTRL, 0x800f);
}

void pcie_acs_init(PCIDevice *dev, uint16_t offset)
{
    bool is_downstream = pci_is_express_downstream_port(dev);
    uint16_t cap_bits = 0;

    assert(is_downstream ||
           (dev->cap_present & QEMU_PCI_CAP_MULTIFUNCTION) ||
           PCI_FUNC(dev->devfn));

    pcie_add_capability(dev, PCI_EXT_CAP_ID_ACS, PCI_ACS_VER, offset,
                        PCI_ACS_SIZEOF);
    dev->exp.acs_cap = offset;

    if (is_downstream) {
        cap_bits = PCI_ACS_SV | PCI_ACS_TB | PCI_ACS_RR |
            PCI_ACS_CR | PCI_ACS_UF | PCI_ACS_DT;
    }

    pci_set_word(dev->config + offset + PCI_ACS_CAP, cap_bits);
    pci_set_word(dev->wmask + offset + PCI_ACS_CTRL, cap_bits);
}

void pcie_acs_reset(PCIDevice *dev)
{
    if (dev->exp.acs_cap) {
        pci_set_word(dev->config + dev->exp.acs_cap + PCI_ACS_CTRL, 0);
    }
}
