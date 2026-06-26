
#ifdef TARGET_ENDIANNESS
static inline uint16_t glue(lduw_phys, SUFFIX)(ARG1_DECL, hwaddr addr)
{
    return glue(address_space_lduw, SUFFIX)(ARG1, addr,
                                            MEMTXATTRS_UNSPECIFIED, NULL);
}

static inline uint32_t glue(ldl_phys, SUFFIX)(ARG1_DECL, hwaddr addr)
{
    return glue(address_space_ldl, SUFFIX)(ARG1, addr,
                                           MEMTXATTRS_UNSPECIFIED, NULL);
}

static inline uint64_t glue(ldq_phys, SUFFIX)(ARG1_DECL, hwaddr addr)
{
    return glue(address_space_ldq, SUFFIX)(ARG1, addr,
                                           MEMTXATTRS_UNSPECIFIED, NULL);
}

static inline void glue(stw_phys, SUFFIX)(ARG1_DECL, hwaddr addr, uint16_t val)
{
    glue(address_space_stw, SUFFIX)(ARG1, addr, val,
                                    MEMTXATTRS_UNSPECIFIED, NULL);
}

static inline void glue(stl_phys, SUFFIX)(ARG1_DECL, hwaddr addr, uint32_t val)
{
    glue(address_space_stl, SUFFIX)(ARG1, addr, val,
                                    MEMTXATTRS_UNSPECIFIED, NULL);
}

static inline void glue(stq_phys, SUFFIX)(ARG1_DECL, hwaddr addr, uint64_t val)
{
    glue(address_space_stq, SUFFIX)(ARG1, addr, val,
                                    MEMTXATTRS_UNSPECIFIED, NULL);
}
#else
static inline uint8_t glue(ldub_phys, SUFFIX)(ARG1_DECL, hwaddr addr)
{
    return glue(address_space_ldub, SUFFIX)(ARG1, addr,
                                            MEMTXATTRS_UNSPECIFIED, NULL);
}

static inline uint16_t glue(lduw_le_phys, SUFFIX)(ARG1_DECL, hwaddr addr)
{
    return glue(address_space_lduw_le, SUFFIX)(ARG1, addr,
                                               MEMTXATTRS_UNSPECIFIED, NULL);
}

static inline uint16_t glue(lduw_be_phys, SUFFIX)(ARG1_DECL, hwaddr addr)
{
    return glue(address_space_lduw_be, SUFFIX)(ARG1, addr,
                                               MEMTXATTRS_UNSPECIFIED, NULL);
}

static inline uint32_t glue(ldl_le_phys, SUFFIX)(ARG1_DECL, hwaddr addr)
{
    return glue(address_space_ldl_le, SUFFIX)(ARG1, addr,
                                              MEMTXATTRS_UNSPECIFIED, NULL);
}

static inline uint32_t glue(ldl_be_phys, SUFFIX)(ARG1_DECL, hwaddr addr)
{
    return glue(address_space_ldl_be, SUFFIX)(ARG1, addr,
                                              MEMTXATTRS_UNSPECIFIED, NULL);
}

static inline uint64_t glue(ldq_le_phys, SUFFIX)(ARG1_DECL, hwaddr addr)
{
    return glue(address_space_ldq_le, SUFFIX)(ARG1, addr,
                                              MEMTXATTRS_UNSPECIFIED, NULL);
}

static inline uint64_t glue(ldq_be_phys, SUFFIX)(ARG1_DECL, hwaddr addr)
{
    return glue(address_space_ldq_be, SUFFIX)(ARG1, addr,
                                              MEMTXATTRS_UNSPECIFIED, NULL);
}

static inline void glue(stb_phys, SUFFIX)(ARG1_DECL, hwaddr addr, uint8_t val)
{
    glue(address_space_stb, SUFFIX)(ARG1, addr, val,
                                    MEMTXATTRS_UNSPECIFIED, NULL);
}

static inline void glue(stw_le_phys, SUFFIX)(ARG1_DECL, hwaddr addr, uint16_t val)
{
    glue(address_space_stw_le, SUFFIX)(ARG1, addr, val,
                                       MEMTXATTRS_UNSPECIFIED, NULL);
}

static inline void glue(stw_be_phys, SUFFIX)(ARG1_DECL, hwaddr addr, uint16_t val)
{
    glue(address_space_stw_be, SUFFIX)(ARG1, addr, val,
                                       MEMTXATTRS_UNSPECIFIED, NULL);
}

static inline void glue(stl_le_phys, SUFFIX)(ARG1_DECL, hwaddr addr, uint32_t val)
{
    glue(address_space_stl_le, SUFFIX)(ARG1, addr, val,
                                       MEMTXATTRS_UNSPECIFIED, NULL);
}

static inline void glue(stl_be_phys, SUFFIX)(ARG1_DECL, hwaddr addr, uint32_t val)
{
    glue(address_space_stl_be, SUFFIX)(ARG1, addr, val,
                                       MEMTXATTRS_UNSPECIFIED, NULL);
}

static inline void glue(stq_le_phys, SUFFIX)(ARG1_DECL, hwaddr addr, uint64_t val)
{
    glue(address_space_stq_le, SUFFIX)(ARG1, addr, val,
                                       MEMTXATTRS_UNSPECIFIED, NULL);
}

static inline void glue(stq_be_phys, SUFFIX)(ARG1_DECL, hwaddr addr, uint64_t val)
{
    glue(address_space_stq_be, SUFFIX)(ARG1, addr, val,
                                       MEMTXATTRS_UNSPECIFIED, NULL);
}
#endif

#undef ARG1_DECL
#undef ARG1
#undef SUFFIX
#undef TARGET_ENDIANNESS
