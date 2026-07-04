
#ifndef QEMU_ARM_GICV3_INTERNAL_H
#define QEMU_ARM_GICV3_INTERNAL_H

#include "hw/registerfields.h"
#include "hw/intc/arm_gicv3_common.h"

#define GICD_CTLR            0x0000
#define GICD_TYPER           0x0004
#define GICD_IIDR            0x0008
#define GICD_STATUSR         0x0010
#define GICD_SETSPI_NSR      0x0040
#define GICD_CLRSPI_NSR      0x0048
#define GICD_SETSPI_SR       0x0050
#define GICD_CLRSPI_SR       0x0058
#define GICD_SEIR            0x0068
#define GICD_IGROUPR         0x0080
#define GICD_ISENABLER       0x0100
#define GICD_ICENABLER       0x0180
#define GICD_ISPENDR         0x0200
#define GICD_ICPENDR         0x0280
#define GICD_ISACTIVER       0x0300
#define GICD_ICACTIVER       0x0380
#define GICD_IPRIORITYR      0x0400
#define GICD_ITARGETSR       0x0800
#define GICD_ICFGR           0x0C00
#define GICD_IGRPMODR        0x0D00
#define GICD_NSACR           0x0E00
#define GICD_SGIR            0x0F00
#define GICD_CPENDSGIR       0x0F10
#define GICD_SPENDSGIR       0x0F20
#define GICD_INMIR           0x0F80
#define GICD_INMIRnE         0x3B00
#define GICD_IROUTER         0x6000
#define GICD_IDREGS          0xFFD0

#define GICD_CTLR_EN_GRP0           (1U << 0)
#define GICD_CTLR_EN_GRP1NS         (1U << 1) /* GICv3 5.3.20 */
#define GICD_CTLR_EN_GRP1S          (1U << 2)
#define GICD_CTLR_EN_GRP1_ALL       (GICD_CTLR_EN_GRP1NS | GICD_CTLR_EN_GRP1S)
#define GICD_CTLR_ARE               (1U << 4)
#define GICD_CTLR_ARE_S             (1U << 4)
#define GICD_CTLR_ARE_NS            (1U << 5)
#define GICD_CTLR_DS                (1U << 6)
#define GICD_CTLR_E1NWF             (1U << 7)
#define GICD_CTLR_RWP               (1U << 31)

#define GICD_TYPER_NMI_SHIFT           9
#define GICD_TYPER_LPIS_SHIFT          17

#define GICD_TYPER_IDBITS            0xf

#define GICR_SGI_OFFSET 0x10000
#define GICR_VLPI_OFFSET 0x20000

#define GICR_CTLR             0x0000
#define GICR_IIDR             0x0004
#define GICR_TYPER            0x0008
#define GICR_STATUSR          0x0010
#define GICR_WAKER            0x0014
#define GICR_SETLPIR          0x0040
#define GICR_CLRLPIR          0x0048
#define GICR_PROPBASER        0x0070
#define GICR_PENDBASER        0x0078
#define GICR_INVLPIR          0x00A0
#define GICR_INVALLR          0x00B0
#define GICR_SYNCR            0x00C0
#define GICR_IDREGS           0xFFD0

#define GICR_IGROUPR0         (GICR_SGI_OFFSET + 0x0080)
#define GICR_ISENABLER0       (GICR_SGI_OFFSET + 0x0100)
#define GICR_ICENABLER0       (GICR_SGI_OFFSET + 0x0180)
#define GICR_ISPENDR0         (GICR_SGI_OFFSET + 0x0200)
#define GICR_ICPENDR0         (GICR_SGI_OFFSET + 0x0280)
#define GICR_ISACTIVER0       (GICR_SGI_OFFSET + 0x0300)
#define GICR_ICACTIVER0       (GICR_SGI_OFFSET + 0x0380)
#define GICR_IPRIORITYR       (GICR_SGI_OFFSET + 0x0400)
#define GICR_ICFGR0           (GICR_SGI_OFFSET + 0x0C00)
#define GICR_ICFGR1           (GICR_SGI_OFFSET + 0x0C04)
#define GICR_IGRPMODR0        (GICR_SGI_OFFSET + 0x0D00)
#define GICR_NSACR            (GICR_SGI_OFFSET + 0x0E00)
#define GICR_INMIR0           (GICR_SGI_OFFSET + 0x0F80)

#define GICR_VPROPBASER       (GICR_VLPI_OFFSET + 0x70)
#define GICR_VPENDBASER       (GICR_VLPI_OFFSET + 0x78)

#define GICR_CTLR_ENABLE_LPIS        (1U << 0)
#define GICR_CTLR_CES                (1U << 1)
#define GICR_CTLR_RWP                (1U << 3)
#define GICR_CTLR_DPG0               (1U << 24)
#define GICR_CTLR_DPG1NS             (1U << 25)
#define GICR_CTLR_DPG1S              (1U << 26)
#define GICR_CTLR_UWP                (1U << 31)

#define GICR_TYPER_PLPIS             (1U << 0)
#define GICR_TYPER_VLPIS             (1U << 1)
#define GICR_TYPER_DIRECTLPI         (1U << 3)
#define GICR_TYPER_LAST              (1U << 4)
#define GICR_TYPER_DPGS              (1U << 5)
#define GICR_TYPER_PROCNUM           (0xFFFFU << 8)
#define GICR_TYPER_COMMONLPIAFF      (0x3 << 24)
#define GICR_TYPER_AFFINITYVALUE     (0xFFFFFFFFULL << 32)

#define GICR_WAKER_ProcessorSleep    (1U << 1)
#define GICR_WAKER_ChildrenAsleep    (1U << 2)

FIELD(GICR_PROPBASER, IDBITS, 0, 5)
FIELD(GICR_PROPBASER, INNERCACHE, 7, 3)
FIELD(GICR_PROPBASER, SHAREABILITY, 10, 2)
FIELD(GICR_PROPBASER, PHYADDR, 12, 40)
FIELD(GICR_PROPBASER, OUTERCACHE, 56, 3)

FIELD(GICR_PENDBASER, INNERCACHE, 7, 3)
FIELD(GICR_PENDBASER, SHAREABILITY, 10, 2)
FIELD(GICR_PENDBASER, PHYADDR, 16, 36)
FIELD(GICR_PENDBASER, OUTERCACHE, 56, 3)
FIELD(GICR_PENDBASER, PTZ, 62, 1)

#define GICR_PROPBASER_IDBITS_THRESHOLD          0xd

FIELD(GICR_VPROPBASER, IDBITS, 0, 5)
FIELD(GICR_VPROPBASER, INNERCACHE, 7, 3)
FIELD(GICR_VPROPBASER, SHAREABILITY, 10, 2)
FIELD(GICR_VPROPBASER, PHYADDR, 12, 40)
FIELD(GICR_VPROPBASER, OUTERCACHE, 56, 3)

FIELD(GICR_VPENDBASER, INNERCACHE, 7, 3)
FIELD(GICR_VPENDBASER, SHAREABILITY, 10, 2)
FIELD(GICR_VPENDBASER, PHYADDR, 16, 36)
FIELD(GICR_VPENDBASER, OUTERCACHE, 56, 3)
FIELD(GICR_VPENDBASER, DIRTY, 60, 1)
FIELD(GICR_VPENDBASER, PENDINGLAST, 61, 1)
FIELD(GICR_VPENDBASER, IDAI, 62, 1)
FIELD(GICR_VPENDBASER, VALID, 63, 1)

#define ICC_CTLR_EL1_CBPR           (1U << 0)
#define ICC_CTLR_EL1_EOIMODE        (1U << 1)
#define ICC_CTLR_EL1_PMHE           (1U << 6)
#define ICC_CTLR_EL1_PRIBITS_SHIFT 8
#define ICC_CTLR_EL1_PRIBITS_MASK   (7U << ICC_CTLR_EL1_PRIBITS_SHIFT)
#define ICC_CTLR_EL1_IDBITS_SHIFT 11
#define ICC_CTLR_EL1_SEIS           (1U << 14)
#define ICC_CTLR_EL1_A3V            (1U << 15)

#define ICC_PMR_PRIORITY_MASK    0xff
#define ICC_BPR_BINARYPOINT_MASK 0x07
#define ICC_IGRPEN_ENABLE        0x01

#define ICC_CTLR_EL3_CBPR_EL1S (1U << 0)
#define ICC_CTLR_EL3_CBPR_EL1NS (1U << 1)
#define ICC_CTLR_EL3_EOIMODE_EL3 (1U << 2)
#define ICC_CTLR_EL3_EOIMODE_EL1S (1U << 3)
#define ICC_CTLR_EL3_EOIMODE_EL1NS (1U << 4)
#define ICC_CTLR_EL3_RM (1U << 5)
#define ICC_CTLR_EL3_PMHE (1U << 6)
#define ICC_CTLR_EL3_PRIBITS_SHIFT 8
#define ICC_CTLR_EL3_IDBITS_SHIFT 11
#define ICC_CTLR_EL3_SEIS (1U << 14)
#define ICC_CTLR_EL3_A3V (1U << 15)
#define ICC_CTLR_EL3_NDS (1U << 17)

#define ICC_AP1R_EL1_NMI (1ULL << 63)
#define ICC_RPR_EL1_NSNMI (1ULL << 62)
#define ICC_RPR_EL1_NMI (1ULL << 63)

#define ICH_VMCR_EL2_VENG0_SHIFT 0
#define ICH_VMCR_EL2_VENG0 (1U << ICH_VMCR_EL2_VENG0_SHIFT)
#define ICH_VMCR_EL2_VENG1_SHIFT 1
#define ICH_VMCR_EL2_VENG1 (1U << ICH_VMCR_EL2_VENG1_SHIFT)
#define ICH_VMCR_EL2_VACKCTL (1U << 2)
#define ICH_VMCR_EL2_VFIQEN (1U << 3)
#define ICH_VMCR_EL2_VCBPR_SHIFT 4
#define ICH_VMCR_EL2_VCBPR (1U << ICH_VMCR_EL2_VCBPR_SHIFT)
#define ICH_VMCR_EL2_VEOIM_SHIFT 9
#define ICH_VMCR_EL2_VEOIM (1U << ICH_VMCR_EL2_VEOIM_SHIFT)
#define ICH_VMCR_EL2_VBPR1_SHIFT 18
#define ICH_VMCR_EL2_VBPR1_LENGTH 3
#define ICH_VMCR_EL2_VBPR1_MASK (0x7U << ICH_VMCR_EL2_VBPR1_SHIFT)
#define ICH_VMCR_EL2_VBPR0_SHIFT 21
#define ICH_VMCR_EL2_VBPR0_LENGTH 3
#define ICH_VMCR_EL2_VBPR0_MASK (0x7U << ICH_VMCR_EL2_VBPR0_SHIFT)
#define ICH_VMCR_EL2_VPMR_SHIFT 24
#define ICH_VMCR_EL2_VPMR_LENGTH 8
#define ICH_VMCR_EL2_VPMR_MASK (0xffU << ICH_VMCR_EL2_VPMR_SHIFT)

#define ICH_HCR_EL2_EN (1U << 0)
#define ICH_HCR_EL2_UIE (1U << 1)
#define ICH_HCR_EL2_LRENPIE (1U << 2)
#define ICH_HCR_EL2_NPIE (1U << 3)
#define ICH_HCR_EL2_VGRP0EIE (1U << 4)
#define ICH_HCR_EL2_VGRP0DIE (1U << 5)
#define ICH_HCR_EL2_VGRP1EIE (1U << 6)
#define ICH_HCR_EL2_VGRP1DIE (1U << 7)
#define ICH_HCR_EL2_TC (1U << 10)
#define ICH_HCR_EL2_TALL0 (1U << 11)
#define ICH_HCR_EL2_TALL1 (1U << 12)
#define ICH_HCR_EL2_TSEI (1U << 13)
#define ICH_HCR_EL2_TDIR (1U << 14)
#define ICH_HCR_EL2_EOICOUNT_SHIFT 27
#define ICH_HCR_EL2_EOICOUNT_LENGTH 5
#define ICH_HCR_EL2_EOICOUNT_MASK (0x1fU << ICH_HCR_EL2_EOICOUNT_SHIFT)

#define ICH_LR_EL2_VINTID_SHIFT 0
#define ICH_LR_EL2_VINTID_LENGTH 32
#define ICH_LR_EL2_VINTID_MASK (0xffffffffULL << ICH_LR_EL2_VINTID_SHIFT)
#define ICH_LR_EL2_PINTID_SHIFT 32
#define ICH_LR_EL2_PINTID_LENGTH 10
#define ICH_LR_EL2_PINTID_MASK (0x3ffULL << ICH_LR_EL2_PINTID_SHIFT)
#define ICH_LR_EL2_EOI (1ULL << 41)
#define ICH_LR_EL2_PRIORITY_SHIFT 48
#define ICH_LR_EL2_PRIORITY_LENGTH 8
#define ICH_LR_EL2_PRIORITY_MASK (0xffULL << ICH_LR_EL2_PRIORITY_SHIFT)
#define ICH_LR_EL2_NMI (1ULL << 59)
#define ICH_LR_EL2_GROUP (1ULL << 60)
#define ICH_LR_EL2_HW (1ULL << 61)
#define ICH_LR_EL2_STATE_SHIFT 62
#define ICH_LR_EL2_STATE_LENGTH 2
#define ICH_LR_EL2_STATE_MASK (3ULL << ICH_LR_EL2_STATE_SHIFT)
#define ICH_LR_EL2_STATE_INVALID 0
#define ICH_LR_EL2_STATE_PENDING 1
#define ICH_LR_EL2_STATE_ACTIVE 2
#define ICH_LR_EL2_STATE_ACTIVE_PENDING 3
#define ICH_LR_EL2_STATE_PENDING_BIT (1ULL << ICH_LR_EL2_STATE_SHIFT)
#define ICH_LR_EL2_STATE_ACTIVE_BIT (2ULL << ICH_LR_EL2_STATE_SHIFT)

#define ICH_MISR_EL2_EOI (1U << 0)
#define ICH_MISR_EL2_U (1U << 1)
#define ICH_MISR_EL2_LRENP (1U << 2)
#define ICH_MISR_EL2_NP (1U << 3)
#define ICH_MISR_EL2_VGRP0E (1U << 4)
#define ICH_MISR_EL2_VGRP0D (1U << 5)
#define ICH_MISR_EL2_VGRP1E (1U << 6)
#define ICH_MISR_EL2_VGRP1D (1U << 7)

#define ICH_VTR_EL2_LISTREGS_SHIFT 0
#define ICH_VTR_EL2_TDS (1U << 19)
#define ICH_VTR_EL2_NV4 (1U << 20)
#define ICH_VTR_EL2_A3V (1U << 21)
#define ICH_VTR_EL2_SEIS (1U << 22)
#define ICH_VTR_EL2_IDBITS_SHIFT 23
#define ICH_VTR_EL2_PREBITS_SHIFT 26
#define ICH_VTR_EL2_PRIBITS_SHIFT 29

#define ICV_AP1R_EL1_NMI (1ULL << 63)
#define ICV_RPR_EL1_NMI (1ULL << 63)


FIELD(GITS_BASER, SIZE, 0, 8)
FIELD(GITS_BASER, PAGESIZE, 8, 2)
FIELD(GITS_BASER, SHAREABILITY, 10, 2)
FIELD(GITS_BASER, PHYADDR, 12, 36)
FIELD(GITS_BASER, PHYADDRL_64K, 16, 32)
FIELD(GITS_BASER, PHYADDRH_64K, 12, 4)
FIELD(GITS_BASER, ENTRYSIZE, 48, 5)
FIELD(GITS_BASER, OUTERCACHE, 53, 3)
FIELD(GITS_BASER, TYPE, 56, 3)
FIELD(GITS_BASER, INNERCACHE, 59, 3)
FIELD(GITS_BASER, INDIRECT, 62, 1)
FIELD(GITS_BASER, VALID, 63, 1)

FIELD(GITS_CBASER, SIZE, 0, 8)
FIELD(GITS_CBASER, SHAREABILITY, 10, 2)
FIELD(GITS_CBASER, PHYADDR, 12, 40)
FIELD(GITS_CBASER, OUTERCACHE, 53, 3)
FIELD(GITS_CBASER, INNERCACHE, 59, 3)
FIELD(GITS_CBASER, VALID, 63, 1)

FIELD(GITS_CREADR, STALLED, 0, 1)
FIELD(GITS_CREADR, OFFSET, 5, 15)

FIELD(GITS_CWRITER, RETRY, 0, 1)
FIELD(GITS_CWRITER, OFFSET, 5, 15)

FIELD(GITS_CTLR, ENABLED, 0, 1)
FIELD(GITS_CTLR, QUIESCENT, 31, 1)

FIELD(GITS_TYPER, PHYSICAL, 0, 1)
FIELD(GITS_TYPER, VIRTUAL, 1, 1)
FIELD(GITS_TYPER, ITT_ENTRY_SIZE, 4, 4)
FIELD(GITS_TYPER, IDBITS, 8, 5)
FIELD(GITS_TYPER, DEVBITS, 13, 5)
FIELD(GITS_TYPER, SEIS, 18, 1)
FIELD(GITS_TYPER, PTA, 19, 1)
FIELD(GITS_TYPER, CIDBITS, 32, 4)
FIELD(GITS_TYPER, CIL, 36, 1)
FIELD(GITS_TYPER, VMOVP, 37, 1)

#define GITS_IDREGS           0xFFD0

#define GITS_BASER_RO_MASK                  (R_GITS_BASER_ENTRYSIZE_MASK | \
                                              R_GITS_BASER_TYPE_MASK)

#define GITS_BASER_PAGESIZE_4K                0
#define GITS_BASER_PAGESIZE_16K               1
#define GITS_BASER_PAGESIZE_64K               2

#define GITS_BASER_TYPE_DEVICE               1ULL
#define GITS_BASER_TYPE_VPE                  2ULL
#define GITS_BASER_TYPE_COLLECTION           4ULL

#define GITS_PAGE_SIZE_4K       0x1000
#define GITS_PAGE_SIZE_16K      0x4000
#define GITS_PAGE_SIZE_64K      0x10000

#define L1TABLE_ENTRY_SIZE         8

#define LPI_CTE_ENABLED          TABLE_ENTRY_VALID_MASK
#define LPI_PRIORITY_MASK         0xfc

#define GITS_CMDQ_ENTRY_WORDS 4
#define GITS_CMDQ_ENTRY_SIZE  (GITS_CMDQ_ENTRY_WORDS * sizeof(uint64_t))

#define CMD_MASK                  0xff

#define GITS_CMD_MOVI             0x01
#define GITS_CMD_INT              0x03
#define GITS_CMD_CLEAR            0x04
#define GITS_CMD_SYNC             0x05
#define GITS_CMD_MAPD             0x08
#define GITS_CMD_MAPC             0x09
#define GITS_CMD_MAPTI            0x0A
#define GITS_CMD_MAPI             0x0B
#define GITS_CMD_INV              0x0C
#define GITS_CMD_INVALL           0x0D
#define GITS_CMD_MOVALL           0x0E
#define GITS_CMD_DISCARD          0x0F
#define GITS_CMD_VMOVI            0x21
#define GITS_CMD_VMOVP            0x22
#define GITS_CMD_VSYNC            0x25
#define GITS_CMD_VMAPP            0x29
#define GITS_CMD_VMAPTI           0x2A
#define GITS_CMD_VMAPI            0x2B
#define GITS_CMD_VINVALL          0x2D

#define ICID_LENGTH                  16
#define ICID_MASK                 ((1U << ICID_LENGTH) - 1)
FIELD(MAPC, RDBASE, 16, 32)

#define RDBASE_PROCNUM_LENGTH        16
#define RDBASE_PROCNUM_MASK       ((1ULL << RDBASE_PROCNUM_LENGTH) - 1)

#define ITTADDR_LENGTH               44
#define ITTADDR_SHIFT                 8
#define ITTADDR_MASK             MAKE_64BIT_MASK(ITTADDR_SHIFT, ITTADDR_LENGTH)
#define SIZE_MASK                 0x1f

#define EVENTID_MASK              ((1ULL << 32) - 1)

#define pINTID_SHIFT                 32
#define pINTID_MASK               MAKE_64BIT_MASK(32, 32)

#define DEVID_SHIFT                  32
#define DEVID_MASK                MAKE_64BIT_MASK(32, 32)

#define VALID_SHIFT               63
#define CMD_FIELD_VALID_MASK      (1ULL << VALID_SHIFT)
#define L2_TABLE_VALID_MASK       CMD_FIELD_VALID_MASK
#define TABLE_ENTRY_VALID_MASK    (1ULL << 0)

FIELD(MOVALL_2, RDBASE1, 16, 36)
FIELD(MOVALL_3, RDBASE2, 16, 36)

FIELD(MOVI_0, DEVICEID, 32, 32)
FIELD(MOVI_1, EVENTID, 0, 32)
FIELD(MOVI_2, ICID, 0, 16)

FIELD(INV_0, DEVICEID, 32, 32)
FIELD(INV_1, EVENTID, 0, 32)

FIELD(VMAPTI_0, DEVICEID, 32, 32)
FIELD(VMAPTI_1, EVENTID, 0, 32)
FIELD(VMAPTI_1, VPEID, 32, 16)
FIELD(VMAPTI_2, VINTID, 0, 32) /* VMAPTI only */
FIELD(VMAPTI_2, DOORBELL, 32, 32)

FIELD(VMAPP_0, ALLOC, 8, 1) /* GICv4.1 only */
FIELD(VMAPP_0, PTZ, 9, 1) /* GICv4.1 only */
FIELD(VMAPP_0, VCONFADDR, 16, 36) /* GICv4.1 only */
FIELD(VMAPP_1, DEFAULT_DOORBELL, 0, 32) /* GICv4.1 only */
FIELD(VMAPP_1, VPEID, 32, 16)
FIELD(VMAPP_2, RDBASE, 16, 36)
FIELD(VMAPP_2, V, 63, 1)
FIELD(VMAPP_3, VPTSIZE, 0, 8) /* For GICv4.0, bits [7:6] are RES0 */
FIELD(VMAPP_3, VPTADDR, 16, 36)

FIELD(VMOVP_0, SEQNUM, 32, 16) /* not used for GITS_TYPER.VMOVP == 1 */
FIELD(VMOVP_1, ITSLIST, 0, 16) /* not used for GITS_TYPER.VMOVP == 1 */
FIELD(VMOVP_1, VPEID, 32, 16)
FIELD(VMOVP_2, RDBASE, 16, 36)
FIELD(VMOVP_2, DB, 63, 1) /* GICv4.1 only */
FIELD(VMOVP_3, DEFAULT_DOORBELL, 0, 32) /* GICv4.1 only */

FIELD(VMOVI_0, DEVICEID, 32, 32)
FIELD(VMOVI_1, EVENTID, 0, 32)
FIELD(VMOVI_1, VPEID, 32, 16)
FIELD(VMOVI_2, D, 0, 1)
FIELD(VMOVI_2, DOORBELL, 32, 32)

FIELD(VINVALL_1, VPEID, 32, 16)

#define ITS_ITT_ENTRY_SIZE            0xC

FIELD(ITE_L, VALID, 0, 1)
FIELD(ITE_L, INTTYPE, 1, 1)
FIELD(ITE_L, INTID, 2, 24)
FIELD(ITE_L, ICID, 32, 16)
FIELD(ITE_L, VPEID, 48, 16)
FIELD(ITE_H, DOORBELL, 0, 24)

#define ITE_INTTYPE_VIRTUAL 0
#define ITE_INTTYPE_PHYSICAL 1

#define ITS_IDBITS                   GICD_TYPER_IDBITS

#define ITS_DEVBITS                   0xF

#define ITS_CIDBITS                  0xF

#define GITS_DTE_SIZE                 (0x8ULL)

FIELD(DTE, VALID, 0, 1)
FIELD(DTE, SIZE, 1, 5)
FIELD(DTE, ITTADDR, 6, 44)

#define GITS_CTE_SIZE                 (0x8ULL)
FIELD(CTE, VALID, 0, 1)
FIELD(CTE, RDBASE, 1, RDBASE_PROCNUM_LENGTH)

#define GITS_VPE_SIZE 0x8ULL

FIELD(VTE, VALID, 0, 1)
FIELD(VTE, VPTSIZE, 1, 5)
FIELD(VTE, VPTADDR, 6, 36)
FIELD(VTE, RDBASE, 42, RDBASE_PROCNUM_LENGTH)

#define INTID_SECURE 1020
#define INTID_NONSECURE 1021
#define INTID_NMI 1022
#define INTID_SPURIOUS 1023


static inline int gicv3_redist_size(GICv3State *s)
{
    if (s->cpu[0].gicr_typer & GICR_TYPER_VLPIS) {
        return GICV4_REDIST_SIZE;
    } else {
        return GICV3_REDIST_SIZE;
    }
}

static inline bool gicv3_intid_is_special(int intid)
{
    return intid >= INTID_SECURE && intid <= INTID_SPURIOUS;
}

void gicv3_redist_update(GICv3CPUState *cs);

void gicv3_update(GICv3State *s, int start, int len);

void gicv3_full_update_noirqset(GICv3State *s);

void gicv3_full_update(GICv3State *s);
MemTxResult gicv3_dist_read(void *opaque, hwaddr offset, uint64_t *data,
                            unsigned size, MemTxAttrs attrs);
MemTxResult gicv3_dist_write(void *opaque, hwaddr addr, uint64_t data,
                             unsigned size, MemTxAttrs attrs);
MemTxResult gicv3_redist_read(void *opaque, hwaddr offset, uint64_t *data,
                              unsigned size, MemTxAttrs attrs);
MemTxResult gicv3_redist_write(void *opaque, hwaddr offset, uint64_t data,
                               unsigned size, MemTxAttrs attrs);
void gicv3_dist_set_irq(GICv3State *s, int irq, int level);
void gicv3_redist_set_irq(GICv3CPUState *cs, int irq, int level);
void gicv3_redist_process_lpi(GICv3CPUState *cs, int irq, int level);
void gicv3_redist_process_vlpi(GICv3CPUState *cs, int irq, uint64_t vptaddr,
                               int doorbell, int level);
void gicv3_redist_vlpi_pending(GICv3CPUState *cs, int irq, int level);

void gicv3_redist_lpi_pending(GICv3CPUState *cs, int irq, int level);
void gicv3_redist_update_lpi(GICv3CPUState *cs);
void gicv3_redist_update_lpi_only(GICv3CPUState *cs);
void gicv3_redist_inv_lpi(GICv3CPUState *cs, int irq);
void gicv3_redist_inv_vlpi(GICv3CPUState *cs, int irq, uint64_t vptaddr);
void gicv3_redist_mov_lpi(GICv3CPUState *src, GICv3CPUState *dest, int irq);
void gicv3_redist_movall_lpis(GICv3CPUState *src, GICv3CPUState *dest);
void gicv3_redist_mov_vlpi(GICv3CPUState *src, uint64_t src_vptaddr,
                           GICv3CPUState *dest, uint64_t dest_vptaddr,
                           int irq, int doorbell);
void gicv3_redist_vinvall(GICv3CPUState *cs, uint64_t vptaddr);

void gicv3_redist_send_sgi(GICv3CPUState *cs, int grp, int irq, bool ns);
void gicv3_init_cpuif(GICv3State *s);

void gicv3_cpuif_update(GICv3CPUState *cs);

void gicv3_cpuif_virt_irq_fiq_update(GICv3CPUState *cs);

static inline uint32_t gicv3_iidr(void)
{
    return 0x43b;
}

#define GICV3_PIDR0_DIST 0x92
#define GICV3_PIDR0_REDIST 0x93
#define GICV3_PIDR0_ITS 0x94

static inline uint32_t gicv3_idreg(GICv3State *s, int regoffset, uint8_t pidr0)
{
    static const uint8_t gicd_ids[] = {
        0x44, 0x00, 0x00, 0x00, 0x92, 0xB4, 0x0B, 0x00, 0x0D, 0xF0, 0x05, 0xB1
    };
    uint32_t id;

    regoffset /= 4;

    if (regoffset == 4) {
        return pidr0;
    }
    id = gicd_ids[regoffset];
    if (regoffset == 6) {
        id |= s->revision << 4;
    }
    return id;
}

static inline int gicv3_irq_group(GICv3State *s, GICv3CPUState *cs, int irq)
{
    bool grpbit, grpmodbit;

    if (irq < GIC_INTERNAL) {
        grpbit = extract32(cs->gicr_igroupr0, irq, 1);
        grpmodbit = extract32(cs->gicr_igrpmodr0, irq, 1);
    } else {
        grpbit = gicv3_gicd_group_test(s, irq);
        grpmodbit = gicv3_gicd_grpmod_test(s, irq);
    }
    if (grpbit) {
        return GICV3_G1NS;
    }
    if (s->gicd_ctlr & GICD_CTLR_DS) {
        return GICV3_G0;
    }
    return grpmodbit ? GICV3_G1 : GICV3_G0;
}

static inline uint32_t gicv3_redist_affid(GICv3CPUState *cs)
{
    return cs->gicr_typer >> 32;
}

static inline void gicv3_cache_target_cpustate(GICv3State *s, int irq)
{
    GICv3CPUState *cs = NULL;
    int i;
    uint32_t tgtaff = extract64(s->gicd_irouter[irq], 0, 24) |
        extract64(s->gicd_irouter[irq], 32, 8) << 24;

    for (i = 0; i < s->num_cpu; i++) {
        if (s->cpu[i].gicr_typer >> 32 == tgtaff) {
            cs = &s->cpu[i];
            break;
        }
    }

    s->gicd_irouter_target[irq] = cs;
}

static inline void gicv3_cache_all_target_cpustates(GICv3State *s)
{
    int irq;

    for (irq = GIC_INTERNAL; irq < GICV3_MAXIRQ; irq++) {
        gicv3_cache_target_cpustate(s, irq);
    }
}

void gicv3_set_gicv3state(CPUState *cpu, GICv3CPUState *s);

#endif /* QEMU_ARM_GICV3_INTERNAL_H */
