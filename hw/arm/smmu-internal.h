#ifndef HW_ARM_SMMU_INTERNAL_H
#define HW_ARM_SMMU_INTERNAL_H

#define TBI0(tbi) ((tbi) & 0x1)
#define TBI1(tbi) ((tbi) & 0x2 >> 1)

#define ARM_LPAE_PTE_TYPE_SHIFT         0
#define ARM_LPAE_PTE_TYPE_MASK          0x3

#define ARM_LPAE_PTE_TYPE_BLOCK         1
#define ARM_LPAE_PTE_TYPE_TABLE         3

#define ARM_LPAE_L3_PTE_TYPE_RESERVED   1
#define ARM_LPAE_L3_PTE_TYPE_PAGE       3

#define ARM_LPAE_PTE_VALID              (1 << 0)

#define PTE_ADDRESS(pte, shift) \
    (extract64(pte, shift, 47 - shift + 1) << shift)

#define is_invalid_pte(pte) (!(pte & ARM_LPAE_PTE_VALID))

#define is_reserved_pte(pte, level)                                      \
    ((level == 3) &&                                                     \
     ((pte & ARM_LPAE_PTE_TYPE_MASK) == ARM_LPAE_L3_PTE_TYPE_RESERVED))

#define is_block_pte(pte, level)                                         \
    ((level < 3) &&                                                      \
     ((pte & ARM_LPAE_PTE_TYPE_MASK) == ARM_LPAE_PTE_TYPE_BLOCK))

#define is_table_pte(pte, level)                                        \
    ((level < 3) &&                                                     \
     ((pte & ARM_LPAE_PTE_TYPE_MASK) == ARM_LPAE_PTE_TYPE_TABLE))

#define is_page_pte(pte, level)                                         \
    ((level == 3) &&                                                    \
     ((pte & ARM_LPAE_PTE_TYPE_MASK) == ARM_LPAE_L3_PTE_TYPE_PAGE))

#define PTE_AP(pte) \
    (extract64(pte, 6, 2))

#define PTE_APTABLE(pte) \
    (extract64(pte, 61, 2))

#define PTE_AF(pte) \
    (extract64(pte, 10, 1))

#define is_permission_fault(ap, perm) \
    (((perm) & IOMMU_WO) && ((ap) & 0x2))

#define is_permission_fault_s2(s2ap, perm) \
    (!(((s2ap) & (perm)) == (perm)))

#define PTE_AP_TO_PERM(ap) \
    (IOMMU_ACCESS_FLAG(true, !((ap) & 0x2)))

static inline int level_shift(int level, int granule_sz)
{
    return granule_sz + (3 - level) * (granule_sz - 3);
}

static inline uint64_t level_page_mask(int level, int granule_sz)
{
    return ~(MAKE_64BIT_MASK(0, level_shift(level, granule_sz)));
}

static inline
uint64_t iova_level_offset(uint64_t iova, int inputsize,
                           int level, int gsz)
{
    return ((iova & MAKE_64BIT_MASK(0, inputsize)) >> level_shift(level, gsz)) &
            MAKE_64BIT_MASK(0, gsz - 3);
}

static inline int get_start_level(int sl0 , int granule_sz)
{
    if (granule_sz == 12) {
        return 2 - sl0;
    }
    return 3 - sl0;
}

static inline int pgd_concat_idx(int start_level, int granule_sz,
                                 dma_addr_t ipa)
{
    uint64_t ret;
    int shift =  level_shift(start_level - 1, granule_sz);

    ret = ipa >> shift;
    return ret;
}

#define SMMU_IOTLB_ASID(key) ((key).asid)
#define SMMU_IOTLB_VMID(key) ((key).vmid)

typedef struct SMMUIOTLBPageInvInfo {
    int asid;
    int vmid;
    uint64_t iova;
    uint64_t mask;
} SMMUIOTLBPageInvInfo;

#endif
