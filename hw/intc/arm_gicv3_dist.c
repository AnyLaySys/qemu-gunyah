
#include "qemu/osdep.h"
#include "qemu/log.h"
#include "trace.h"
#include "gicv3_internal.h"


typedef uint32_t maskfn(GICv3State *s, int irq);

static uint32_t mask_nsacr_ge1(GICv3State *s, int irq)
{
    uint64_t raw_nsacr = s->gicd_nsacr[irq / 16 + 1];

    raw_nsacr = raw_nsacr << 32 | s->gicd_nsacr[irq / 16];
    raw_nsacr = (raw_nsacr >> 1) | raw_nsacr;
    return half_unshuffle64(raw_nsacr);
}

static uint32_t mask_nsacr_ge2(GICv3State *s, int irq)
{
    uint64_t raw_nsacr = s->gicd_nsacr[irq / 16 + 1];

    raw_nsacr = raw_nsacr << 32 | s->gicd_nsacr[irq / 16];
    raw_nsacr = raw_nsacr >> 1;
    return half_unshuffle64(raw_nsacr);
}


static uint32_t mask_group_and_nsacr(GICv3State *s, MemTxAttrs attrs,
                                     maskfn *maskfn, int irq)
{
    uint32_t mask;

    if (!attrs.secure && !(s->gicd_ctlr & GICD_CTLR_DS)) {
        mask = *gic_bmp_ptr32(s->group, irq);
        if (maskfn) {
            mask |= maskfn(s, irq);
        }
        return mask;
    }
    return 0xFFFFFFFFU;
}

static int gicd_ns_access(GICv3State *s, int irq)
{
    if (irq < GIC_INTERNAL || irq >= s->num_irq) {
        return 0;
    }
    return extract32(s->gicd_nsacr[irq / 16], (irq % 16) * 2, 2);
}

static void gicd_write_bitmap_reg(GICv3State *s, MemTxAttrs attrs,
                                  uint32_t *bmp, maskfn *maskfn,
                                  int offset, uint32_t val)
{
    int irq = offset * 8;

    if (irq < GIC_INTERNAL || irq >= s->num_irq) {
        return;
    }
    val &= mask_group_and_nsacr(s, attrs, maskfn, irq);
    *gic_bmp_ptr32(bmp, irq) = val;
    gicv3_update(s, irq, 32);
}

static void gicd_write_set_bitmap_reg(GICv3State *s, MemTxAttrs attrs,
                                      uint32_t *bmp,
                                      maskfn *maskfn,
                                      int offset, uint32_t val)
{
    int irq = offset * 8;

    if (irq < GIC_INTERNAL || irq >= s->num_irq) {
        return;
    }
    val &= mask_group_and_nsacr(s, attrs, maskfn, irq);
    *gic_bmp_ptr32(bmp, irq) |= val;
    gicv3_update(s, irq, 32);
}

static void gicd_write_clear_bitmap_reg(GICv3State *s, MemTxAttrs attrs,
                                        uint32_t *bmp,
                                        maskfn *maskfn,
                                        int offset, uint32_t val)
{
    int irq = offset * 8;

    if (irq < GIC_INTERNAL || irq >= s->num_irq) {
        return;
    }
    val &= mask_group_and_nsacr(s, attrs, maskfn, irq);
    *gic_bmp_ptr32(bmp, irq) &= ~val;
    gicv3_update(s, irq, 32);
}

static uint32_t gicd_read_bitmap_reg(GICv3State *s, MemTxAttrs attrs,
                                     uint32_t *bmp,
                                     maskfn *maskfn,
                                     int offset)
{
    int irq = offset * 8;
    uint32_t val;

    if (irq < GIC_INTERNAL || irq >= s->num_irq) {
        return 0;
    }
    val = *gic_bmp_ptr32(bmp, irq);
    if (bmp == s->pending) {
        uint32_t edge = *gic_bmp_ptr32(s->edge_trigger, irq);
        uint32_t level = *gic_bmp_ptr32(s->level, irq);
        val |= (~edge & level);
    }
    val &= mask_group_and_nsacr(s, attrs, maskfn, irq);
    return val;
}

static uint8_t gicd_read_ipriorityr(GICv3State *s, MemTxAttrs attrs, int irq)
{
    uint32_t prio;

    if (irq < GIC_INTERNAL || irq >= s->num_irq) {
        return 0;
    }

    prio = s->gicd_ipriority[irq];

    if (!attrs.secure && !(s->gicd_ctlr & GICD_CTLR_DS)) {
        if (!gicv3_gicd_group_test(s, irq)) {
            return 0;
        }
        prio = (prio << 1) & 0xff;
    }
    return prio;
}

static void gicd_write_ipriorityr(GICv3State *s, MemTxAttrs attrs, int irq,
                                  uint8_t value)
{
    if (irq < GIC_INTERNAL || irq >= s->num_irq) {
        return;
    }

    if (!attrs.secure && !(s->gicd_ctlr & GICD_CTLR_DS)) {
        if (!gicv3_gicd_group_test(s, irq)) {
            return;
        }
        value = 0x80 | (value >> 1);
    }
    s->gicd_ipriority[irq] = value;
}

static uint64_t gicd_read_irouter(GICv3State *s, MemTxAttrs attrs, int irq)
{
    if (irq < GIC_INTERNAL || irq >= s->num_irq) {
        return 0;
    }

    if (!attrs.secure && !(s->gicd_ctlr & GICD_CTLR_DS)) {
        if (!gicv3_gicd_group_test(s, irq)) {
            if (gicd_ns_access(s, irq) != 3) {
                return 0;
            }
        }
    }

    return s->gicd_irouter[irq];
}

static void gicd_write_irouter(GICv3State *s, MemTxAttrs attrs, int irq,
                               uint64_t val)
{
    if (irq < GIC_INTERNAL || irq >= s->num_irq) {
        return;
    }

    if (!attrs.secure && !(s->gicd_ctlr & GICD_CTLR_DS)) {
        if (!gicv3_gicd_group_test(s, irq)) {
            if (gicd_ns_access(s, irq) != 3) {
                return;
            }
        }
    }

    s->gicd_irouter[irq] = val;
    gicv3_cache_target_cpustate(s, irq);
    gicv3_update(s, irq, 1);
}


static bool gicd_readb(GICv3State *s, hwaddr offset,
                       uint64_t *data, MemTxAttrs attrs)
{
    switch (offset) {
    case GICD_CPENDSGIR ... GICD_CPENDSGIR + 0xf:
    case GICD_SPENDSGIR ... GICD_SPENDSGIR + 0xf:
    case GICD_ITARGETSR ... GICD_ITARGETSR + 0x3ff:
        return true;
    case GICD_IPRIORITYR ... GICD_IPRIORITYR + 0x3ff:
        *data = gicd_read_ipriorityr(s, attrs, offset - GICD_IPRIORITYR);
        return true;
    default:
        return false;
    }
}

static bool gicd_writeb(GICv3State *s, hwaddr offset,
                        uint64_t value, MemTxAttrs attrs)
{
    switch (offset) {
    case GICD_CPENDSGIR ... GICD_CPENDSGIR + 0xf:
    case GICD_SPENDSGIR ... GICD_SPENDSGIR + 0xf:
    case GICD_ITARGETSR ... GICD_ITARGETSR + 0x3ff:
        return true;
    case GICD_IPRIORITYR ... GICD_IPRIORITYR + 0x3ff:
    {
        int irq = offset - GICD_IPRIORITYR;

        if (irq < GIC_INTERNAL || irq >= s->num_irq) {
            return true;
        }
        gicd_write_ipriorityr(s, attrs, irq, value);
        gicv3_update(s, irq, 1);
        return true;
    }
    default:
        return false;
    }
}

static bool gicd_readw(GICv3State *s, hwaddr offset,
                       uint64_t *data, MemTxAttrs attrs)
{
    return false;
}

static bool gicd_writew(GICv3State *s, hwaddr offset,
                        uint64_t value, MemTxAttrs attrs)
{
    return false;
}

static bool gicd_readl(GICv3State *s, hwaddr offset,
                       uint64_t *data, MemTxAttrs attrs)
{

    switch (offset) {
    case GICD_CTLR:
        if (!attrs.secure && !(s->gicd_ctlr & GICD_CTLR_DS)) {
            *data = s->gicd_ctlr & (GICD_CTLR_ARE_S |
                                    GICD_CTLR_EN_GRP1NS |
                                    GICD_CTLR_RWP);
        } else {
            *data = s->gicd_ctlr;
        }
        return true;
    case GICD_TYPER:
    {
        int itlinesnumber = (s->num_irq / 32) - 1;
        bool sec_extn = !(s->gicd_ctlr & GICD_CTLR_DS);
        bool dvis = s->revision >= 4;

        *data = (1 << 25) | (1 << 24) | (dvis << 18) | (sec_extn << 10) |
            (s->nmi_support << GICD_TYPER_NMI_SHIFT) |
            (s->lpi_enable << GICD_TYPER_LPIS_SHIFT) |
            (0xf << 19) | itlinesnumber;
        return true;
    }
    case GICD_IIDR:
        *data = gicv3_iidr();
        return true;
    case GICD_STATUSR:
        *data = 0;
        return true;
    case GICD_IGROUPR ... GICD_IGROUPR + 0x7f:
    {
        int irq;

        if (!attrs.secure && !(s->gicd_ctlr & GICD_CTLR_DS)) {
            *data = 0;
            return true;
        }
        irq = (offset - GICD_IGROUPR) * 8;
        if (irq < GIC_INTERNAL || irq >= s->num_irq) {
            *data = 0;
            return true;
        }
        *data = *gic_bmp_ptr32(s->group, irq);
        return true;
    }
    case GICD_ISENABLER ... GICD_ISENABLER + 0x7f:
        *data = gicd_read_bitmap_reg(s, attrs, s->enabled, NULL,
                                     offset - GICD_ISENABLER);
        return true;
    case GICD_ICENABLER ... GICD_ICENABLER + 0x7f:
        *data = gicd_read_bitmap_reg(s, attrs, s->enabled, NULL,
                                     offset - GICD_ICENABLER);
        return true;
    case GICD_ISPENDR ... GICD_ISPENDR + 0x7f:
        *data = gicd_read_bitmap_reg(s, attrs, s->pending, mask_nsacr_ge1,
                                     offset - GICD_ISPENDR);
        return true;
    case GICD_ICPENDR ... GICD_ICPENDR + 0x7f:
        *data = gicd_read_bitmap_reg(s, attrs, s->pending, mask_nsacr_ge2,
                                     offset - GICD_ICPENDR);
        return true;
    case GICD_ISACTIVER ... GICD_ISACTIVER + 0x7f:
        *data = gicd_read_bitmap_reg(s, attrs, s->active, mask_nsacr_ge2,
                                     offset - GICD_ISACTIVER);
        return true;
    case GICD_ICACTIVER ... GICD_ICACTIVER + 0x7f:
        *data = gicd_read_bitmap_reg(s, attrs, s->active, mask_nsacr_ge2,
                                     offset - GICD_ICACTIVER);
        return true;
    case GICD_IPRIORITYR ... GICD_IPRIORITYR + 0x3ff:
    {
        int i, irq = offset - GICD_IPRIORITYR;
        uint32_t value = 0;

        for (i = irq + 3; i >= irq; i--) {
            value <<= 8;
            value |= gicd_read_ipriorityr(s, attrs, i);
        }
        *data = value;
        return true;
    }
    case GICD_ITARGETSR ... GICD_ITARGETSR + 0x3ff:
        *data = 0;
        return true;
    case GICD_ICFGR ... GICD_ICFGR + 0xff:
    {
        int irq = (offset - GICD_ICFGR) * 4;
        uint32_t value = 0;

        if (irq < GIC_INTERNAL || irq >= s->num_irq) {
            *data = 0;
            return true;
        }

        value = *gic_bmp_ptr32(s->edge_trigger, irq & ~0x1f);
        value &= mask_group_and_nsacr(s, attrs, NULL, irq & ~0x1f);
        value = extract32(value, (irq & 0x1f) ? 16 : 0, 16);
        value = half_shuffle32(value) << 1;
        *data = value;
        return true;
    }
    case GICD_IGRPMODR ... GICD_IGRPMODR + 0xff:
    {
        int irq;

        if ((s->gicd_ctlr & GICD_CTLR_DS) || !attrs.secure) {
            *data = 0;
            return true;
        }
        irq = (offset - GICD_IGRPMODR) * 8;
        if (irq < GIC_INTERNAL || irq >= s->num_irq) {
            *data = 0;
            return true;
        }
        *data = *gic_bmp_ptr32(s->grpmod, irq);
        return true;
    }
    case GICD_NSACR ... GICD_NSACR + 0xff:
    {
        int irq = (offset - GICD_NSACR) * 4;

        if (irq < GIC_INTERNAL || irq >= s->num_irq) {
            *data = 0;
            return true;
        }

        if ((s->gicd_ctlr & GICD_CTLR_DS) || !attrs.secure) {
            *data = 0;
            return true;
        }

        *data = s->gicd_nsacr[irq / 16];
        return true;
    }
    case GICD_CPENDSGIR ... GICD_CPENDSGIR + 0xf:
    case GICD_SPENDSGIR ... GICD_SPENDSGIR + 0xf:
        *data = 0;
        return true;
    case GICD_INMIR ... GICD_INMIR + 0x7f:
        *data = (!s->nmi_support) ? 0 :
                gicd_read_bitmap_reg(s, attrs, s->nmi, NULL,
                                     offset - GICD_INMIR);
        return true;
    case GICD_IROUTER ... GICD_IROUTER + 0x1fdf:
    {
        uint64_t r;
        int irq = (offset - GICD_IROUTER) / 8;

        r = gicd_read_irouter(s, attrs, irq);
        if (offset & 7) {
            *data = r >> 32;
        } else {
            *data = (uint32_t)r;
        }
        return true;
    }
    case GICD_IDREGS ... GICD_IDREGS + 0x2f:
        *data = gicv3_idreg(s, offset - GICD_IDREGS, GICV3_PIDR0_DIST);
        return true;
    case GICD_SGIR:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: invalid guest read from WO register at offset "
                      HWADDR_FMT_plx "\n", __func__, offset);
        *data = 0;
        return true;
    default:
        return false;
    }
}

static bool gicd_writel(GICv3State *s, hwaddr offset,
                        uint64_t value, MemTxAttrs attrs)
{

    switch (offset) {
    case GICD_CTLR:
    {
        uint32_t mask;
        if (s->gicd_ctlr & GICD_CTLR_DS) {
            mask = GICD_CTLR_EN_GRP0 | GICD_CTLR_EN_GRP1NS;
        } else {
            if (attrs.secure) {
                mask = GICD_CTLR_DS | GICD_CTLR_EN_GRP0 | GICD_CTLR_EN_GRP1_ALL;
            } else {
                mask = GICD_CTLR_EN_GRP1NS;
            }
        }
        s->gicd_ctlr = (s->gicd_ctlr & ~mask) | (value & mask);
        if (value & mask & GICD_CTLR_DS) {
            s->gicd_ctlr &= ~(GICD_CTLR_EN_GRP1S | GICD_CTLR_ARE_NS);
        }
        gicv3_full_update(s);
        return true;
    }
    case GICD_STATUSR:
        return true;
    case GICD_IGROUPR ... GICD_IGROUPR + 0x7f:
    {
        int irq;

        if (!attrs.secure && !(s->gicd_ctlr & GICD_CTLR_DS)) {
            return true;
        }
        irq = (offset - GICD_IGROUPR) * 8;
        if (irq < GIC_INTERNAL || irq >= s->num_irq) {
            return true;
        }
        *gic_bmp_ptr32(s->group, irq) = value;
        gicv3_update(s, irq, 32);
        return true;
    }
    case GICD_ISENABLER ... GICD_ISENABLER + 0x7f:
        gicd_write_set_bitmap_reg(s, attrs, s->enabled, NULL,
                                  offset - GICD_ISENABLER, value);
        return true;
    case GICD_ICENABLER ... GICD_ICENABLER + 0x7f:
        gicd_write_clear_bitmap_reg(s, attrs, s->enabled, NULL,
                                    offset - GICD_ICENABLER, value);
        return true;
    case GICD_ISPENDR ... GICD_ISPENDR + 0x7f:
        gicd_write_set_bitmap_reg(s, attrs, s->pending, mask_nsacr_ge1,
                                  offset - GICD_ISPENDR, value);
        return true;
    case GICD_ICPENDR ... GICD_ICPENDR + 0x7f:
        gicd_write_clear_bitmap_reg(s, attrs, s->pending, mask_nsacr_ge2,
                                    offset - GICD_ICPENDR, value);
        return true;
    case GICD_ISACTIVER ... GICD_ISACTIVER + 0x7f:
        gicd_write_set_bitmap_reg(s, attrs, s->active, NULL,
                                  offset - GICD_ISACTIVER, value);
        return true;
    case GICD_ICACTIVER ... GICD_ICACTIVER + 0x7f:
        gicd_write_clear_bitmap_reg(s, attrs, s->active, NULL,
                                    offset - GICD_ICACTIVER, value);
        return true;
    case GICD_IPRIORITYR ... GICD_IPRIORITYR + 0x3ff:
    {
        int i, irq = offset - GICD_IPRIORITYR;

        if (irq < GIC_INTERNAL || irq + 3 >= s->num_irq) {
            return true;
        }

        for (i = irq; i < irq + 4; i++, value >>= 8) {
            gicd_write_ipriorityr(s, attrs, i, value);
        }
        gicv3_update(s, irq, 4);
        return true;
    }
    case GICD_ITARGETSR ... GICD_ITARGETSR + 0x3ff:
        return true;
    case GICD_ICFGR ... GICD_ICFGR + 0xff:
    {
        int irq = (offset - GICD_ICFGR) * 4;
        uint32_t mask, oldval;

        if (irq < GIC_INTERNAL || irq >= s->num_irq) {
            return true;
        }

        value = half_unshuffle32(value >> 1);
        mask = mask_group_and_nsacr(s, attrs, NULL, irq & ~0x1f);
        if (irq & 0x1f) {
            value <<= 16;
            mask &= 0xffff0000U;
        } else {
            mask &= 0xffff;
        }
        oldval = *gic_bmp_ptr32(s->edge_trigger, (irq & ~0x1f));
        value = (oldval & ~mask) | (value & mask);
        *gic_bmp_ptr32(s->edge_trigger, irq & ~0x1f) = value;
        return true;
    }
    case GICD_IGRPMODR ... GICD_IGRPMODR + 0xff:
    {
        int irq;

        if ((s->gicd_ctlr & GICD_CTLR_DS) || !attrs.secure) {
            return true;
        }
        irq = (offset - GICD_IGRPMODR) * 8;
        if (irq < GIC_INTERNAL || irq >= s->num_irq) {
            return true;
        }
        *gic_bmp_ptr32(s->grpmod, irq) = value;
        gicv3_update(s, irq, 32);
        return true;
    }
    case GICD_NSACR ... GICD_NSACR + 0xff:
    {
        int irq = (offset - GICD_NSACR) * 4;

        if (irq < GIC_INTERNAL || irq >= s->num_irq) {
            return true;
        }

        if ((s->gicd_ctlr & GICD_CTLR_DS) || !attrs.secure) {
            return true;
        }

        s->gicd_nsacr[irq / 16] = value;
        return true;
    }
    case GICD_SGIR:
        return true;
    case GICD_CPENDSGIR ... GICD_CPENDSGIR + 0xf:
    case GICD_SPENDSGIR ... GICD_SPENDSGIR + 0xf:
        return true;
    case GICD_INMIR ... GICD_INMIR + 0x7f:
        if (s->nmi_support) {
            gicd_write_bitmap_reg(s, attrs, s->nmi, NULL,
                                  offset - GICD_INMIR, value);
        }
        return true;
    case GICD_IROUTER ... GICD_IROUTER + 0x1fdf:
    {
        uint64_t r;
        int irq = (offset - GICD_IROUTER) / 8;

        if (irq < GIC_INTERNAL || irq >= s->num_irq) {
            return true;
        }

        r = gicd_read_irouter(s, attrs, irq);
        r = deposit64(r, (offset & 7) ? 32 : 0, 32, value);
        gicd_write_irouter(s, attrs, irq, r);
        return true;
    }
    case GICD_IDREGS ... GICD_IDREGS + 0x2f:
    case GICD_TYPER:
    case GICD_IIDR:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: invalid guest write to RO register at offset "
                      HWADDR_FMT_plx "\n", __func__, offset);
        return true;
    default:
        return false;
    }
}

static bool gicd_writeq(GICv3State *s, hwaddr offset,
                        uint64_t value, MemTxAttrs attrs)
{
    int irq;

    switch (offset) {
    case GICD_IROUTER ... GICD_IROUTER + 0x1fdf:
        irq = (offset - GICD_IROUTER) / 8;
        gicd_write_irouter(s, attrs, irq, value);
        return true;
    default:
        return false;
    }
}

static bool gicd_readq(GICv3State *s, hwaddr offset,
                       uint64_t *data, MemTxAttrs attrs)
{
    int irq;

    switch (offset) {
    case GICD_IROUTER ... GICD_IROUTER + 0x1fdf:
        irq = (offset - GICD_IROUTER) / 8;
        *data = gicd_read_irouter(s, attrs, irq);
        return true;
    default:
        return false;
    }
}

MemTxResult gicv3_dist_read(void *opaque, hwaddr offset, uint64_t *data,
                            unsigned size, MemTxAttrs attrs)
{
    GICv3State *s = (GICv3State *)opaque;
    bool r;

    switch (size) {
    case 1:
        r = gicd_readb(s, offset, data, attrs);
        break;
    case 2:
        r = gicd_readw(s, offset, data, attrs);
        break;
    case 4:
        r = gicd_readl(s, offset, data, attrs);
        break;
    case 8:
        r = gicd_readq(s, offset, data, attrs);
        break;
    default:
        r = false;
        break;
    }

    if (!r) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: invalid guest read at offset " HWADDR_FMT_plx
                      " size %u\n", __func__, offset, size);
        trace_gicv3_dist_badread(offset, size, attrs.secure);
        *data = 0;
    } else {
        trace_gicv3_dist_read(offset, *data, size, attrs.secure);
    }
    return MEMTX_OK;
}

MemTxResult gicv3_dist_write(void *opaque, hwaddr offset, uint64_t data,
                             unsigned size, MemTxAttrs attrs)
{
    GICv3State *s = (GICv3State *)opaque;
    bool r;

    switch (size) {
    case 1:
        r = gicd_writeb(s, offset, data, attrs);
        break;
    case 2:
        r = gicd_writew(s, offset, data, attrs);
        break;
    case 4:
        r = gicd_writel(s, offset, data, attrs);
        break;
    case 8:
        r = gicd_writeq(s, offset, data, attrs);
        break;
    default:
        r = false;
        break;
    }

    if (!r) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: invalid guest write at offset " HWADDR_FMT_plx
                      " size %u\n", __func__, offset, size);
        trace_gicv3_dist_badwrite(offset, data, size, attrs.secure);
    } else {
        trace_gicv3_dist_write(offset, data, size, attrs.secure);
    }
    return MEMTX_OK;
}

void gicv3_dist_set_irq(GICv3State *s, int irq, int level)
{
    if (level == gicv3_gicd_level_test(s, irq)) {
        return;
    }

    trace_gicv3_dist_set_irq(irq, level);

    gicv3_gicd_level_replace(s, irq, level);

    if (level) {
        if (gicv3_gicd_edge_trigger_test(s, irq)) {
            gicv3_gicd_pending_set(s, irq);
        }
    }

    gicv3_update(s, irq, 1);
}
