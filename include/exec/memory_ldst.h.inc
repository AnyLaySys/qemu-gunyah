
#ifdef TARGET_ENDIANNESS
uint16_t glue(address_space_lduw, SUFFIX)(ARG1_DECL,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint32_t glue(address_space_ldl, SUFFIX)(ARG1_DECL,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint64_t glue(address_space_ldq, SUFFIX)(ARG1_DECL,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
void glue(address_space_stl_notdirty, SUFFIX)(ARG1_DECL,
    hwaddr addr, uint32_t val, MemTxAttrs attrs, MemTxResult *result);
void glue(address_space_stw, SUFFIX)(ARG1_DECL,
    hwaddr addr, uint16_t val, MemTxAttrs attrs, MemTxResult *result);
void glue(address_space_stl, SUFFIX)(ARG1_DECL,
    hwaddr addr, uint32_t val, MemTxAttrs attrs, MemTxResult *result);
void glue(address_space_stq, SUFFIX)(ARG1_DECL,
    hwaddr addr, uint64_t val, MemTxAttrs attrs, MemTxResult *result);
#else
uint8_t glue(address_space_ldub, SUFFIX)(ARG1_DECL,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint16_t glue(address_space_lduw_le, SUFFIX)(ARG1_DECL,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint16_t glue(address_space_lduw_be, SUFFIX)(ARG1_DECL,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint32_t glue(address_space_ldl_le, SUFFIX)(ARG1_DECL,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint32_t glue(address_space_ldl_be, SUFFIX)(ARG1_DECL,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint64_t glue(address_space_ldq_le, SUFFIX)(ARG1_DECL,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint64_t glue(address_space_ldq_be, SUFFIX)(ARG1_DECL,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
void glue(address_space_stb, SUFFIX)(ARG1_DECL,
    hwaddr addr, uint8_t val, MemTxAttrs attrs, MemTxResult *result);
void glue(address_space_stw_le, SUFFIX)(ARG1_DECL,
    hwaddr addr, uint16_t val, MemTxAttrs attrs, MemTxResult *result);
void glue(address_space_stw_be, SUFFIX)(ARG1_DECL,
    hwaddr addr, uint16_t val, MemTxAttrs attrs, MemTxResult *result);
void glue(address_space_stl_le, SUFFIX)(ARG1_DECL,
    hwaddr addr, uint32_t val, MemTxAttrs attrs, MemTxResult *result);
void glue(address_space_stl_be, SUFFIX)(ARG1_DECL,
    hwaddr addr, uint32_t val, MemTxAttrs attrs, MemTxResult *result);
void glue(address_space_stq_le, SUFFIX)(ARG1_DECL,
    hwaddr addr, uint64_t val, MemTxAttrs attrs, MemTxResult *result);
void glue(address_space_stq_be, SUFFIX)(ARG1_DECL,
    hwaddr addr, uint64_t val, MemTxAttrs attrs, MemTxResult *result);
#endif

#undef ARG1_DECL
#undef ARG1
#undef SUFFIX
#undef TARGET_ENDIANNESS
