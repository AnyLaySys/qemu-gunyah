
#include "qemu/osdep.h"
#include "qemu/log.h"
#include "trace.h"
#include "gicv3_internal.h"

static uint32_t mask_group(GICv3CPUState *cs, MemTxAttrs attrs)
{
    if (!attrs.secure && !(cs->gic->gicd_ctlr & GICD_CTLR_DS)) {
        return cs->gicr_igroupr0;
    }
    return 0xFFFFFFFFU;
}

static int gicr_ns_access(GICv3CPUState *cs, int irq)
{
    assert(irq < 16);
    return extract32(cs->gicr_nsacr, irq * 2, 2);
}

static void gicr_write_bitmap_reg(GICv3CPUState *cs, MemTxAttrs attrs,
                                  uint32_t *reg, uint32_t val)
{
    val &= mask_group(cs, attrs);
    *reg = val;
    gicv3_redist_update(cs);
}

static void gicr_write_set_bitmap_reg(GICv3CPUState *cs, MemTxAttrs attrs,
                                      uint32_t *reg, uint32_t val)
{
    val &= mask_group(cs, attrs);
    *reg |= val;
    gicv3_redist_update(cs);
}

static void gicr_write_clear_bitmap_reg(GICv3CPUState *cs, MemTxAttrs attrs,
                                        uint32_t *reg, uint32_t val)
{
    val &= mask_group(cs, attrs);
    *reg &= ~val;
    gicv3_redist_update(cs);
}

static uint32_t gicr_read_bitmap_reg(GICv3CPUState *cs, MemTxAttrs attrs,
                                     uint32_t reg)
{
    reg &= mask_group(cs, attrs);
    return reg;
}

static bool vcpu_resident(GICv3CPUState *cs, uint64_t vptaddr)
{
    if (!FIELD_EX64(cs->gicr_vpendbaser, GICR_VPENDBASER, VALID)) {
        return false;
    }
    return vptaddr == (cs->gicr_vpendbaser & R_GICR_VPENDBASER_PHYADDR_MASK);
}

static void update_for_one_lpi(GICv3CPUState *cs, int irq,
                               uint64_t ctbase, bool ds, PendingIrq *hpp)
{
    uint8_t lpite;
    uint8_t prio;

    address_space_read(&cs->gic->dma_as,
                       ctbase + ((irq - GICV3_LPI_INTID_START) * sizeof(lpite)),
                       MEMTXATTRS_UNSPECIFIED, &lpite, sizeof(lpite));

    if (!(lpite & LPI_CTE_ENABLED)) {
        return;
    }

    if (ds) {
        prio = lpite & LPI_PRIORITY_MASK;
    } else {
        prio = ((lpite & LPI_PRIORITY_MASK) >> 1) | 0x80;
    }

    if ((prio < hpp->prio) ||
        ((prio == hpp->prio) && (irq <= hpp->irq))) {
        hpp->irq = irq;
        hpp->prio = prio;
        hpp->nmi = false;
        hpp->grp = GICV3_G1NS;
    }
}

static void update_for_all_lpis(GICv3CPUState *cs, uint64_t ptbase,
                                uint64_t ctbase, unsigned ptsizebits,
                                bool ds, PendingIrq *hpp)
{
    AddressSpace *as = &cs->gic->dma_as;
    uint8_t pend;
    uint32_t pendt_size = (1ULL << (ptsizebits + 1));
    int i, bit;

    hpp->prio = 0xff;
    hpp->nmi = false;

    for (i = GICV3_LPI_INTID_START / 8; i < pendt_size / 8; i++) {
        address_space_read(as, ptbase + i, MEMTXATTRS_UNSPECIFIED, &pend, 1);
        while (pend) {
            bit = ctz32(pend);
            update_for_one_lpi(cs, i * 8 + bit, ctbase, ds, hpp);
            pend &= ~(1 << bit);
        }
    }
}

static bool set_pending_table_bit(GICv3CPUState *cs, uint64_t ptbase,
                                  int irq, bool level)
{
    AddressSpace *as = &cs->gic->dma_as;
    uint64_t addr = ptbase + irq / 8;
    uint8_t pend;

    address_space_read(as, addr, MEMTXATTRS_UNSPECIFIED, &pend, 1);
    if (extract32(pend, irq % 8, 1) == level) {
        return false;
    }
    pend = deposit32(pend, irq % 8, 1, level ? 1 : 0);
    address_space_write(as, addr, MEMTXATTRS_UNSPECIFIED, &pend, 1);
    return true;
}

static uint8_t gicr_read_ipriorityr(GICv3CPUState *cs, MemTxAttrs attrs,
                                    int irq)
{
    uint32_t prio;

    prio = cs->gicr_ipriorityr[irq];

    if (!attrs.secure && !(cs->gic->gicd_ctlr & GICD_CTLR_DS)) {
        if (!(cs->gicr_igroupr0 & (1U << irq))) {
            return 0;
        }
        prio = (prio << 1) & 0xff;
    }
    return prio;
}

static void gicr_write_ipriorityr(GICv3CPUState *cs, MemTxAttrs attrs, int irq,
                                  uint8_t value)
{
    if (!attrs.secure && !(cs->gic->gicd_ctlr & GICD_CTLR_DS)) {
        if (!(cs->gicr_igroupr0 & (1U << irq))) {
            return;
        }
        value = 0x80 | (value >> 1);
    }
    cs->gicr_ipriorityr[irq] = value;
}

static void gicv3_redist_update_vlpi_only(GICv3CPUState *cs)
{
    uint64_t ptbase, ctbase, idbits;

    if (!FIELD_EX64(cs->gicr_vpendbaser, GICR_VPENDBASER, VALID)) {
        cs->hppvlpi.prio = 0xff;
        cs->hppvlpi.nmi = false;
        return;
    }

    ptbase = cs->gicr_vpendbaser & R_GICR_VPENDBASER_PHYADDR_MASK;
    ctbase = cs->gicr_vpropbaser & R_GICR_VPROPBASER_PHYADDR_MASK;
    idbits = FIELD_EX64(cs->gicr_vpropbaser, GICR_VPROPBASER, IDBITS);

    update_for_all_lpis(cs, ptbase, ctbase, idbits, true, &cs->hppvlpi);
}

static void gicv3_redist_update_vlpi(GICv3CPUState *cs)
{
    gicv3_redist_update_vlpi_only(cs);
    gicv3_cpuif_virt_irq_fiq_update(cs);
}

static void gicr_write_vpendbaser(GICv3CPUState *cs, uint64_t newval)
{
    bool oldvalid = FIELD_EX64(cs->gicr_vpendbaser, GICR_VPENDBASER, VALID);
    bool newvalid = FIELD_EX64(newval, GICR_VPENDBASER, VALID);
    bool pendinglast;

    newval &= R_GICR_VPENDBASER_INNERCACHE_MASK |
        R_GICR_VPENDBASER_SHAREABILITY_MASK |
        R_GICR_VPENDBASER_PHYADDR_MASK |
        R_GICR_VPENDBASER_OUTERCACHE_MASK |
        R_GICR_VPENDBASER_PENDINGLAST_MASK |
        R_GICR_VPENDBASER_IDAI_MASK |
        R_GICR_VPENDBASER_VALID_MASK;

    if (oldvalid && newvalid) {
        if (cs->gicr_vpendbaser ^ newval) {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "%s: Changing GICR_VPENDBASER when VALID=1 "
                          "is UNPREDICTABLE\n", __func__);
        }
        return;
    }
    if (!oldvalid && !newvalid) {
        cs->gicr_vpendbaser = newval;
        return;
    }

    if (newvalid) {
        pendinglast = true;
    } else {
        pendinglast = cs->hppvlpi.prio != 0xff;
    }

    newval = FIELD_DP64(newval, GICR_VPENDBASER, PENDINGLAST, pendinglast);
    cs->gicr_vpendbaser = newval;
    gicv3_redist_update_vlpi(cs);
}

static MemTxResult gicr_readb(GICv3CPUState *cs, hwaddr offset,
                              uint64_t *data, MemTxAttrs attrs)
{
    switch (offset) {
    case GICR_IPRIORITYR ... GICR_IPRIORITYR + 0x1f:
        *data = gicr_read_ipriorityr(cs, attrs, offset - GICR_IPRIORITYR);
        return MEMTX_OK;
    default:
        return MEMTX_ERROR;
    }
}

static MemTxResult gicr_writeb(GICv3CPUState *cs, hwaddr offset,
                               uint64_t value, MemTxAttrs attrs)
{
    switch (offset) {
    case GICR_IPRIORITYR ... GICR_IPRIORITYR + 0x1f:
        gicr_write_ipriorityr(cs, attrs, offset - GICR_IPRIORITYR, value);
        gicv3_redist_update(cs);
        return MEMTX_OK;
    default:
        return MEMTX_ERROR;
    }
}

static MemTxResult gicr_readl(GICv3CPUState *cs, hwaddr offset,
                              uint64_t *data, MemTxAttrs attrs)
{
    switch (offset) {
    case GICR_CTLR:
        *data = cs->gicr_ctlr;
        return MEMTX_OK;
    case GICR_IIDR:
        *data = gicv3_iidr();
        return MEMTX_OK;
    case GICR_TYPER:
        *data = extract64(cs->gicr_typer, 0, 32);
        return MEMTX_OK;
    case GICR_TYPER + 4:
        *data = extract64(cs->gicr_typer, 32, 32);
        return MEMTX_OK;
    case GICR_STATUSR:
        *data = 0;
        return MEMTX_OK;
    case GICR_WAKER:
        *data = cs->gicr_waker;
        return MEMTX_OK;
    case GICR_PROPBASER:
        *data = extract64(cs->gicr_propbaser, 0, 32);
        return MEMTX_OK;
    case GICR_PROPBASER + 4:
        *data = extract64(cs->gicr_propbaser, 32, 32);
        return MEMTX_OK;
    case GICR_PENDBASER:
        *data = extract64(cs->gicr_pendbaser, 0, 32);
        return MEMTX_OK;
    case GICR_PENDBASER + 4:
        *data = extract64(cs->gicr_pendbaser, 32, 32);
        return MEMTX_OK;
    case GICR_IGROUPR0:
        if (!attrs.secure && !(cs->gic->gicd_ctlr & GICD_CTLR_DS)) {
            *data = 0;
            return MEMTX_OK;
        }
        *data = cs->gicr_igroupr0;
        return MEMTX_OK;
    case GICR_ISENABLER0:
    case GICR_ICENABLER0:
        *data = gicr_read_bitmap_reg(cs, attrs, cs->gicr_ienabler0);
        return MEMTX_OK;
    case GICR_ISPENDR0:
    case GICR_ICPENDR0:
    {
        uint32_t val = cs->gicr_ipendr0 | (~cs->edge_trigger & cs->level);
        *data = gicr_read_bitmap_reg(cs, attrs, val);
        return MEMTX_OK;
    }
    case GICR_ISACTIVER0:
    case GICR_ICACTIVER0:
        *data = gicr_read_bitmap_reg(cs, attrs, cs->gicr_iactiver0);
        return MEMTX_OK;
    case GICR_IPRIORITYR ... GICR_IPRIORITYR + 0x1f:
    {
        int i, irq = offset - GICR_IPRIORITYR;
        uint32_t value = 0;

        for (i = irq + 3; i >= irq; i--) {
            value <<= 8;
            value |= gicr_read_ipriorityr(cs, attrs, i);
        }
        *data = value;
        return MEMTX_OK;
    }
    case GICR_INMIR0:
        *data = cs->gic->nmi_support ?
                gicr_read_bitmap_reg(cs, attrs, cs->gicr_inmir0) : 0;
        return MEMTX_OK;
    case GICR_ICFGR0:
    case GICR_ICFGR1:
    {
        uint32_t value;

        value = cs->edge_trigger & mask_group(cs, attrs);
        value = extract32(value, (offset == GICR_ICFGR1) ? 16 : 0, 16);
        value = half_shuffle32(value) << 1;
        *data = value;
        return MEMTX_OK;
    }
    case GICR_IGRPMODR0:
        if ((cs->gic->gicd_ctlr & GICD_CTLR_DS) || !attrs.secure) {
            *data = 0;
            return MEMTX_OK;
        }
        *data = cs->gicr_igrpmodr0;
        return MEMTX_OK;
    case GICR_NSACR:
        if ((cs->gic->gicd_ctlr & GICD_CTLR_DS) || !attrs.secure) {
            *data = 0;
            return MEMTX_OK;
        }
        *data = cs->gicr_nsacr;
        return MEMTX_OK;
    case GICR_IDREGS ... GICR_IDREGS + 0x2f:
        *data = gicv3_idreg(cs->gic, offset - GICR_IDREGS, GICV3_PIDR0_REDIST);
        return MEMTX_OK;
    case GICR_VPROPBASER:
        *data = extract64(cs->gicr_vpropbaser, 0, 32);
        return MEMTX_OK;
    case GICR_VPROPBASER + 4:
        *data = extract64(cs->gicr_vpropbaser, 32, 32);
        return MEMTX_OK;
    case GICR_VPENDBASER:
        *data = extract64(cs->gicr_vpendbaser, 0, 32);
        return MEMTX_OK;
    case GICR_VPENDBASER + 4:
        *data = extract64(cs->gicr_vpendbaser, 32, 32);
        return MEMTX_OK;
    default:
        return MEMTX_ERROR;
    }
}

static MemTxResult gicr_writel(GICv3CPUState *cs, hwaddr offset,
                               uint64_t value, MemTxAttrs attrs)
{
    switch (offset) {
    case GICR_CTLR:
        if (cs->gicr_typer & GICR_TYPER_PLPIS) {
            if (value & GICR_CTLR_ENABLE_LPIS) {
                cs->gicr_ctlr |= GICR_CTLR_ENABLE_LPIS;
                gicv3_redist_update_lpi(cs);
            } else {
                cs->gicr_ctlr &= ~GICR_CTLR_ENABLE_LPIS;
                gicv3_redist_update(cs);
            }
        }
        return MEMTX_OK;
    case GICR_STATUSR:
        return MEMTX_OK;
    case GICR_WAKER:
        value &= GICR_WAKER_ProcessorSleep;
        if (value & GICR_WAKER_ProcessorSleep) {
            value |= GICR_WAKER_ChildrenAsleep;
        }
        cs->gicr_waker = value;
        return MEMTX_OK;
    case GICR_PROPBASER:
        cs->gicr_propbaser = deposit64(cs->gicr_propbaser, 0, 32, value);
        return MEMTX_OK;
    case GICR_PROPBASER + 4:
        cs->gicr_propbaser = deposit64(cs->gicr_propbaser, 32, 32, value);
        return MEMTX_OK;
    case GICR_PENDBASER:
        cs->gicr_pendbaser = deposit64(cs->gicr_pendbaser, 0, 32, value);
        return MEMTX_OK;
    case GICR_PENDBASER + 4:
        cs->gicr_pendbaser = deposit64(cs->gicr_pendbaser, 32, 32, value);
        return MEMTX_OK;
    case GICR_IGROUPR0:
        if (!attrs.secure && !(cs->gic->gicd_ctlr & GICD_CTLR_DS)) {
            return MEMTX_OK;
        }
        cs->gicr_igroupr0 = value;
        gicv3_redist_update(cs);
        return MEMTX_OK;
    case GICR_ISENABLER0:
        gicr_write_set_bitmap_reg(cs, attrs, &cs->gicr_ienabler0, value);
        return MEMTX_OK;
    case GICR_ICENABLER0:
        gicr_write_clear_bitmap_reg(cs, attrs, &cs->gicr_ienabler0, value);
        return MEMTX_OK;
    case GICR_ISPENDR0:
        gicr_write_set_bitmap_reg(cs, attrs, &cs->gicr_ipendr0, value);
        return MEMTX_OK;
    case GICR_ICPENDR0:
        gicr_write_clear_bitmap_reg(cs, attrs, &cs->gicr_ipendr0, value);
        return MEMTX_OK;
    case GICR_ISACTIVER0:
        gicr_write_set_bitmap_reg(cs, attrs, &cs->gicr_iactiver0, value);
        return MEMTX_OK;
    case GICR_ICACTIVER0:
        gicr_write_clear_bitmap_reg(cs, attrs, &cs->gicr_iactiver0, value);
        return MEMTX_OK;
    case GICR_IPRIORITYR ... GICR_IPRIORITYR + 0x1f:
    {
        int i, irq = offset - GICR_IPRIORITYR;

        for (i = irq; i < irq + 4; i++, value >>= 8) {
            gicr_write_ipriorityr(cs, attrs, i, value);
        }
        gicv3_redist_update(cs);
        return MEMTX_OK;
    }
    case GICR_INMIR0:
        if (cs->gic->nmi_support) {
            gicr_write_bitmap_reg(cs, attrs, &cs->gicr_inmir0, value);
        }
        return MEMTX_OK;

    case GICR_ICFGR0:
        return MEMTX_OK;
    case GICR_ICFGR1:
    {
        uint32_t mask;

        value = half_unshuffle32(value >> 1) << 16;
        mask = mask_group(cs, attrs) & 0xffff0000U;

        cs->edge_trigger &= ~mask;
        cs->edge_trigger |= (value & mask);

        gicv3_redist_update(cs);
        return MEMTX_OK;
    }
    case GICR_IGRPMODR0:
        if ((cs->gic->gicd_ctlr & GICD_CTLR_DS) || !attrs.secure) {
            return MEMTX_OK;
        }
        cs->gicr_igrpmodr0 = value;
        gicv3_redist_update(cs);
        return MEMTX_OK;
    case GICR_NSACR:
        if ((cs->gic->gicd_ctlr & GICD_CTLR_DS) || !attrs.secure) {
            return MEMTX_OK;
        }
        cs->gicr_nsacr = value;
        return MEMTX_OK;
    case GICR_IIDR:
    case GICR_TYPER:
    case GICR_IDREGS ... GICR_IDREGS + 0x2f:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: invalid guest write to RO register at offset "
                      HWADDR_FMT_plx "\n", __func__, offset);
        return MEMTX_OK;
    case GICR_VPROPBASER:
        cs->gicr_vpropbaser = deposit64(cs->gicr_vpropbaser, 0, 32, value);
        return MEMTX_OK;
    case GICR_VPROPBASER + 4:
        cs->gicr_vpropbaser = deposit64(cs->gicr_vpropbaser, 32, 32, value);
        return MEMTX_OK;
    case GICR_VPENDBASER:
        gicr_write_vpendbaser(cs, deposit64(cs->gicr_vpendbaser, 0, 32, value));
        return MEMTX_OK;
    case GICR_VPENDBASER + 4:
        gicr_write_vpendbaser(cs, deposit64(cs->gicr_vpendbaser, 32, 32, value));
        return MEMTX_OK;
    default:
        return MEMTX_ERROR;
    }
}

static MemTxResult gicr_readll(GICv3CPUState *cs, hwaddr offset,
                               uint64_t *data, MemTxAttrs attrs)
{
    switch (offset) {
    case GICR_TYPER:
        *data = cs->gicr_typer;
        return MEMTX_OK;
    case GICR_PROPBASER:
        *data = cs->gicr_propbaser;
        return MEMTX_OK;
    case GICR_PENDBASER:
        *data = cs->gicr_pendbaser;
        return MEMTX_OK;
    case GICR_VPROPBASER:
        *data = cs->gicr_vpropbaser;
        return MEMTX_OK;
    case GICR_VPENDBASER:
        *data = cs->gicr_vpendbaser;
        return MEMTX_OK;
    default:
        return MEMTX_ERROR;
    }
}

static MemTxResult gicr_writell(GICv3CPUState *cs, hwaddr offset,
                                uint64_t value, MemTxAttrs attrs)
{
    switch (offset) {
    case GICR_PROPBASER:
        cs->gicr_propbaser = value;
        return MEMTX_OK;
    case GICR_PENDBASER:
        cs->gicr_pendbaser = value;
        return MEMTX_OK;
    case GICR_TYPER:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: invalid guest write to RO register at offset "
                      HWADDR_FMT_plx "\n", __func__, offset);
        return MEMTX_OK;
    case GICR_VPROPBASER:
        cs->gicr_vpropbaser = value;
        return MEMTX_OK;
    case GICR_VPENDBASER:
        gicr_write_vpendbaser(cs, value);
        return MEMTX_OK;
    default:
        return MEMTX_ERROR;
    }
}

MemTxResult gicv3_redist_read(void *opaque, hwaddr offset, uint64_t *data,
                              unsigned size, MemTxAttrs attrs)
{
    GICv3RedistRegion *region = opaque;
    GICv3State *s = region->gic;
    GICv3CPUState *cs;
    MemTxResult r;
    int cpuidx;

    assert((offset & (size - 1)) == 0);

    cpuidx = region->cpuidx + offset / gicv3_redist_size(s);
    offset %= gicv3_redist_size(s);

    cs = &s->cpu[cpuidx];

    switch (size) {
    case 1:
        r = gicr_readb(cs, offset, data, attrs);
        break;
    case 4:
        r = gicr_readl(cs, offset, data, attrs);
        break;
    case 8:
        r = gicr_readll(cs, offset, data, attrs);
        break;
    default:
        r = MEMTX_ERROR;
        break;
    }

    if (r != MEMTX_OK) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: invalid guest read at offset " HWADDR_FMT_plx
                      " size %u\n", __func__, offset, size);
        trace_gicv3_redist_badread(gicv3_redist_affid(cs), offset,
                                   size, attrs.secure);
        r = MEMTX_OK;
        *data = 0;
    } else {
        trace_gicv3_redist_read(gicv3_redist_affid(cs), offset, *data,
                                size, attrs.secure);
    }
    return r;
}

MemTxResult gicv3_redist_write(void *opaque, hwaddr offset, uint64_t data,
                               unsigned size, MemTxAttrs attrs)
{
    GICv3RedistRegion *region = opaque;
    GICv3State *s = region->gic;
    GICv3CPUState *cs;
    MemTxResult r;
    int cpuidx;

    assert((offset & (size - 1)) == 0);

    cpuidx = region->cpuidx + offset / gicv3_redist_size(s);
    offset %= gicv3_redist_size(s);

    cs = &s->cpu[cpuidx];

    switch (size) {
    case 1:
        r = gicr_writeb(cs, offset, data, attrs);
        break;
    case 4:
        r = gicr_writel(cs, offset, data, attrs);
        break;
    case 8:
        r = gicr_writell(cs, offset, data, attrs);
        break;
    default:
        r = MEMTX_ERROR;
        break;
    }

    if (r != MEMTX_OK) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: invalid guest write at offset " HWADDR_FMT_plx
                      " size %u\n", __func__, offset, size);
        trace_gicv3_redist_badwrite(gicv3_redist_affid(cs), offset, data,
                                    size, attrs.secure);
        r = MEMTX_OK;
    } else {
        trace_gicv3_redist_write(gicv3_redist_affid(cs), offset, data,
                                 size, attrs.secure);
    }
    return r;
}

static void gicv3_redist_check_lpi_priority(GICv3CPUState *cs, int irq)
{
    uint64_t lpict_baddr = cs->gicr_propbaser & R_GICR_PROPBASER_PHYADDR_MASK;

    update_for_one_lpi(cs, irq, lpict_baddr,
                       cs->gic->gicd_ctlr & GICD_CTLR_DS,
                       &cs->hpplpi);
}

void gicv3_redist_update_lpi_only(GICv3CPUState *cs)
{
    uint64_t lpipt_baddr, lpict_baddr;
    uint64_t idbits;

    idbits = MIN(FIELD_EX64(cs->gicr_propbaser, GICR_PROPBASER, IDBITS),
                 GICD_TYPER_IDBITS);

    if (!(cs->gicr_ctlr & GICR_CTLR_ENABLE_LPIS)) {
        return;
    }

    lpipt_baddr = cs->gicr_pendbaser & R_GICR_PENDBASER_PHYADDR_MASK;
    lpict_baddr = cs->gicr_propbaser & R_GICR_PROPBASER_PHYADDR_MASK;

    update_for_all_lpis(cs, lpipt_baddr, lpict_baddr, idbits,
                        cs->gic->gicd_ctlr & GICD_CTLR_DS, &cs->hpplpi);
}

void gicv3_redist_update_lpi(GICv3CPUState *cs)
{
    gicv3_redist_update_lpi_only(cs);
    gicv3_redist_update(cs);
}

void gicv3_redist_lpi_pending(GICv3CPUState *cs, int irq, int level)
{
    uint64_t lpipt_baddr;

    lpipt_baddr = cs->gicr_pendbaser & R_GICR_PENDBASER_PHYADDR_MASK;
    if (!set_pending_table_bit(cs, lpipt_baddr, irq, level)) {
        return;
    }

    if (level) {
        gicv3_redist_check_lpi_priority(cs, irq);
        gicv3_redist_update(cs);
    } else {
        if (irq == cs->hpplpi.irq) {
            gicv3_redist_update_lpi(cs);
        }
    }
}

void gicv3_redist_process_lpi(GICv3CPUState *cs, int irq, int level)
{
    uint64_t idbits;

    idbits = MIN(FIELD_EX64(cs->gicr_propbaser, GICR_PROPBASER, IDBITS),
                 GICD_TYPER_IDBITS);

    if (!(cs->gicr_ctlr & GICR_CTLR_ENABLE_LPIS) ||
        (irq > (1ULL << (idbits + 1)) - 1) || irq < GICV3_LPI_INTID_START) {
        return;
    }

    gicv3_redist_lpi_pending(cs, irq, level);
}

void gicv3_redist_inv_lpi(GICv3CPUState *cs, int irq)
{
    gicv3_redist_update_lpi(cs);
}

void gicv3_redist_mov_lpi(GICv3CPUState *src, GICv3CPUState *dest, int irq)
{
    uint64_t idbits;
    uint32_t pendt_size;
    uint64_t src_baddr;

    if (!(src->gicr_ctlr & GICR_CTLR_ENABLE_LPIS) ||
        !(dest->gicr_ctlr & GICR_CTLR_ENABLE_LPIS)) {
        return;
    }

    idbits = MIN(FIELD_EX64(src->gicr_propbaser, GICR_PROPBASER, IDBITS),
                 GICD_TYPER_IDBITS);
    idbits = MIN(FIELD_EX64(dest->gicr_propbaser, GICR_PROPBASER, IDBITS),
                 idbits);

    pendt_size = 1ULL << (idbits + 1);
    if ((irq / 8) >= pendt_size) {
        return;
    }

    src_baddr = src->gicr_pendbaser & R_GICR_PENDBASER_PHYADDR_MASK;

    if (!set_pending_table_bit(src, src_baddr, irq, 0)) {
        return;
    }
    if (irq == src->hpplpi.irq) {
        gicv3_redist_update_lpi(src);
    }
    gicv3_redist_lpi_pending(dest, irq, 1);
}

void gicv3_redist_movall_lpis(GICv3CPUState *src, GICv3CPUState *dest)
{
    AddressSpace *as = &src->gic->dma_as;
    uint64_t idbits;
    uint32_t pendt_size;
    uint64_t src_baddr, dest_baddr;
    int i;

    if (!(src->gicr_ctlr & GICR_CTLR_ENABLE_LPIS) ||
        !(dest->gicr_ctlr & GICR_CTLR_ENABLE_LPIS)) {
        return;
    }

    idbits = MIN(FIELD_EX64(src->gicr_propbaser, GICR_PROPBASER, IDBITS),
                 GICD_TYPER_IDBITS);
    idbits = MIN(FIELD_EX64(dest->gicr_propbaser, GICR_PROPBASER, IDBITS),
                 idbits);

    pendt_size = 1ULL << (idbits + 1);
    src_baddr = src->gicr_pendbaser & R_GICR_PENDBASER_PHYADDR_MASK;
    dest_baddr = dest->gicr_pendbaser & R_GICR_PENDBASER_PHYADDR_MASK;

    for (i = GICV3_LPI_INTID_START / 8; i < pendt_size / 8; i++) {
        uint8_t src_pend, dest_pend;

        address_space_read(as, src_baddr + i, MEMTXATTRS_UNSPECIFIED,
                           &src_pend, sizeof(src_pend));
        if (!src_pend) {
            continue;
        }
        address_space_read(as, dest_baddr + i, MEMTXATTRS_UNSPECIFIED,
                           &dest_pend, sizeof(dest_pend));
        dest_pend |= src_pend;
        src_pend = 0;
        address_space_write(as, src_baddr + i, MEMTXATTRS_UNSPECIFIED,
                            &src_pend, sizeof(src_pend));
        address_space_write(as, dest_baddr + i, MEMTXATTRS_UNSPECIFIED,
                            &dest_pend, sizeof(dest_pend));
    }

    gicv3_redist_update_lpi(src);
    gicv3_redist_update_lpi(dest);
}

void gicv3_redist_vlpi_pending(GICv3CPUState *cs, int irq, int level)
{
    uint64_t vptbase, ctbase;

    vptbase = FIELD_EX64(cs->gicr_vpendbaser, GICR_VPENDBASER, PHYADDR) << 16;

    if (set_pending_table_bit(cs, vptbase, irq, level)) {
        if (level) {
            ctbase = cs->gicr_vpropbaser & R_GICR_VPROPBASER_PHYADDR_MASK;
            update_for_one_lpi(cs, irq, ctbase, true, &cs->hppvlpi);
            gicv3_cpuif_virt_irq_fiq_update(cs);
        } else {
            if (irq == cs->hppvlpi.irq) {
                gicv3_redist_update_vlpi(cs);
            }
        }
    }
}

void gicv3_redist_process_vlpi(GICv3CPUState *cs, int irq, uint64_t vptaddr,
                               int doorbell, int level)
{
    bool bit_changed;
    bool resident = vcpu_resident(cs, vptaddr);
    uint64_t ctbase;

    if (resident) {
        uint32_t idbits = FIELD_EX64(cs->gicr_vpropbaser, GICR_VPROPBASER, IDBITS);
        if (irq >= (1ULL << (idbits + 1))) {
            return;
        }
    }

    bit_changed = set_pending_table_bit(cs, vptaddr, irq, level);
    if (resident && bit_changed) {
        if (level) {
            ctbase = cs->gicr_vpropbaser & R_GICR_VPROPBASER_PHYADDR_MASK;
            update_for_one_lpi(cs, irq, ctbase, true, &cs->hppvlpi);
            gicv3_cpuif_virt_irq_fiq_update(cs);
        } else {
            if (irq == cs->hppvlpi.irq) {
                gicv3_redist_update_vlpi(cs);
            }
        }
    }

    if (!resident && level && doorbell != INTID_SPURIOUS &&
        (cs->gicr_ctlr & GICR_CTLR_ENABLE_LPIS)) {
        gicv3_redist_process_lpi(cs, doorbell, 1);
    }
}

void gicv3_redist_mov_vlpi(GICv3CPUState *src, uint64_t src_vptaddr,
                           GICv3CPUState *dest, uint64_t dest_vptaddr,
                           int irq, int doorbell)
{
    if (!set_pending_table_bit(src, src_vptaddr, irq, 0)) {
        return;
    }
    if (vcpu_resident(src, src_vptaddr) && irq == src->hppvlpi.irq) {
        gicv3_redist_update_vlpi(src);
    }
    gicv3_redist_process_vlpi(dest, irq, dest_vptaddr, doorbell, irq);
}

void gicv3_redist_vinvall(GICv3CPUState *cs, uint64_t vptaddr)
{
    if (!vcpu_resident(cs, vptaddr)) {
        return;
    }

    gicv3_redist_update_vlpi(cs);
}

void gicv3_redist_inv_vlpi(GICv3CPUState *cs, int irq, uint64_t vptaddr)
{
    gicv3_redist_vinvall(cs, vptaddr);
}

void gicv3_redist_set_irq(GICv3CPUState *cs, int irq, int level)
{
    if (level == extract32(cs->level, irq, 1)) {
        return;
    }

    trace_gicv3_redist_set_irq(gicv3_redist_affid(cs), irq, level);

    cs->level = deposit32(cs->level, irq, 1, level);

    if (level) {
        if (extract32(cs->edge_trigger, irq, 1)) {
            cs->gicr_ipendr0 = deposit32(cs->gicr_ipendr0, irq, 1, 1);
        }
    }

    gicv3_redist_update(cs);
}

void gicv3_redist_send_sgi(GICv3CPUState *cs, int grp, int irq, bool ns)
{
    int irqgrp = gicv3_irq_group(cs->gic, cs, irq);

    if (grp == GICV3_G1 && irqgrp == GICV3_G0) {
        grp = GICV3_G0;
    }

    if (grp != irqgrp) {
        return;
    }

    if (ns && !(cs->gic->gicd_ctlr & GICD_CTLR_DS)) {
        int nsaccess = gicr_ns_access(cs, irq);

        if ((irqgrp == GICV3_G0 && nsaccess < 1) ||
            (irqgrp == GICV3_G1 && nsaccess < 2)) {
            return;
        }
    }

    trace_gicv3_redist_send_sgi(gicv3_redist_affid(cs), irq);
    cs->gicr_ipendr0 = deposit32(cs->gicr_ipendr0, irq, 1, 1);
    gicv3_redist_update(cs);
}
