
#ifndef TCG_TCG_OP_GVEC_COMMON_H
#define TCG_TCG_OP_GVEC_COMMON_H


typedef void gen_helper_gvec_2(TCGv_ptr, TCGv_ptr, TCGv_i32);
void tcg_gen_gvec_2_ool(uint32_t dofs, uint32_t aofs,
                        uint32_t oprsz, uint32_t maxsz, int32_t data,
                        gen_helper_gvec_2 *fn);

typedef void gen_helper_gvec_2i(TCGv_ptr, TCGv_ptr, TCGv_i64, TCGv_i32);
void tcg_gen_gvec_2i_ool(uint32_t dofs, uint32_t aofs, TCGv_i64 c,
                         uint32_t oprsz, uint32_t maxsz, int32_t data,
                         gen_helper_gvec_2i *fn);

typedef void gen_helper_gvec_2_ptr(TCGv_ptr, TCGv_ptr, TCGv_ptr, TCGv_i32);
void tcg_gen_gvec_2_ptr(uint32_t dofs, uint32_t aofs,
                        TCGv_ptr ptr, uint32_t oprsz, uint32_t maxsz,
                        int32_t data, gen_helper_gvec_2_ptr *fn);

typedef void gen_helper_gvec_3(TCGv_ptr, TCGv_ptr, TCGv_ptr, TCGv_i32);
void tcg_gen_gvec_3_ool(uint32_t dofs, uint32_t aofs, uint32_t bofs,
                        uint32_t oprsz, uint32_t maxsz, int32_t data,
                        gen_helper_gvec_3 *fn);

typedef void gen_helper_gvec_4(TCGv_ptr, TCGv_ptr, TCGv_ptr,
                               TCGv_ptr, TCGv_i32);
void tcg_gen_gvec_4_ool(uint32_t dofs, uint32_t aofs, uint32_t bofs,
                        uint32_t cofs, uint32_t oprsz, uint32_t maxsz,
                        int32_t data, gen_helper_gvec_4 *fn);

typedef void gen_helper_gvec_5(TCGv_ptr, TCGv_ptr, TCGv_ptr, TCGv_ptr,
                               TCGv_ptr, TCGv_i32);
void tcg_gen_gvec_5_ool(uint32_t dofs, uint32_t aofs, uint32_t bofs,
                        uint32_t cofs, uint32_t xofs, uint32_t oprsz,
                        uint32_t maxsz, int32_t data, gen_helper_gvec_5 *fn);

typedef void gen_helper_gvec_3_ptr(TCGv_ptr, TCGv_ptr, TCGv_ptr,
                                   TCGv_ptr, TCGv_i32);
void tcg_gen_gvec_3_ptr(uint32_t dofs, uint32_t aofs, uint32_t bofs,
                        TCGv_ptr ptr, uint32_t oprsz, uint32_t maxsz,
                        int32_t data, gen_helper_gvec_3_ptr *fn);

typedef void gen_helper_gvec_4_ptr(TCGv_ptr, TCGv_ptr, TCGv_ptr,
                                   TCGv_ptr, TCGv_ptr, TCGv_i32);
void tcg_gen_gvec_4_ptr(uint32_t dofs, uint32_t aofs, uint32_t bofs,
                        uint32_t cofs, TCGv_ptr ptr, uint32_t oprsz,
                        uint32_t maxsz, int32_t data,
                        gen_helper_gvec_4_ptr *fn);

typedef void gen_helper_gvec_5_ptr(TCGv_ptr, TCGv_ptr, TCGv_ptr, TCGv_ptr,
                                   TCGv_ptr, TCGv_ptr, TCGv_i32);
void tcg_gen_gvec_5_ptr(uint32_t dofs, uint32_t aofs, uint32_t bofs,
                        uint32_t cofs, uint32_t eofs, TCGv_ptr ptr,
                        uint32_t oprsz, uint32_t maxsz, int32_t data,
                        gen_helper_gvec_5_ptr *fn);

typedef struct {
    void (*fni8)(TCGv_i64, TCGv_i64);
    void (*fni4)(TCGv_i32, TCGv_i32);
    void (*fniv)(unsigned, TCGv_vec, TCGv_vec);
    gen_helper_gvec_2 *fno;
    const TCGOpcode *opt_opc;
    int32_t data;
    uint8_t vece;
    bool prefer_i64;
    bool load_dest;
} GVecGen2;

typedef struct {
    void (*fni8)(TCGv_i64, TCGv_i64, int64_t);
    void (*fni4)(TCGv_i32, TCGv_i32, int32_t);
    void (*fniv)(unsigned, TCGv_vec, TCGv_vec, int64_t);
    gen_helper_gvec_2 *fno;
    gen_helper_gvec_2i *fnoi;
    const TCGOpcode *opt_opc;
    uint8_t vece;
    bool prefer_i64;
    bool load_dest;
} GVecGen2i;

typedef struct {
    void (*fni8)(TCGv_i64, TCGv_i64, TCGv_i64);
    void (*fni4)(TCGv_i32, TCGv_i32, TCGv_i32);
    void (*fniv)(unsigned, TCGv_vec, TCGv_vec, TCGv_vec);
    gen_helper_gvec_2i *fno;
    const TCGOpcode *opt_opc;
    uint32_t data;
    uint8_t vece;
    bool prefer_i64;
    bool scalar_first;
} GVecGen2s;

typedef struct {
    void (*fni8)(TCGv_i64, TCGv_i64, TCGv_i64);
    void (*fni4)(TCGv_i32, TCGv_i32, TCGv_i32);
    void (*fniv)(unsigned, TCGv_vec, TCGv_vec, TCGv_vec);
    gen_helper_gvec_3 *fno;
    const TCGOpcode *opt_opc;
    int32_t data;
    uint8_t vece;
    bool prefer_i64;
    bool load_dest;
} GVecGen3;

typedef struct {
    void (*fni8)(TCGv_i64, TCGv_i64, TCGv_i64, int64_t);
    void (*fni4)(TCGv_i32, TCGv_i32, TCGv_i32, int32_t);
    void (*fniv)(unsigned, TCGv_vec, TCGv_vec, TCGv_vec, int64_t);
    gen_helper_gvec_3 *fno;
    const TCGOpcode *opt_opc;
    uint8_t vece;
    bool prefer_i64;
    bool load_dest;
    bool write_aofs;
} GVecGen3i;

typedef struct {
    void (*fni8)(TCGv_i64, TCGv_i64, TCGv_i64, TCGv_i64);
    void (*fni4)(TCGv_i32, TCGv_i32, TCGv_i32, TCGv_i32);
    void (*fniv)(unsigned, TCGv_vec, TCGv_vec, TCGv_vec, TCGv_vec);
    gen_helper_gvec_4 *fno;
    const TCGOpcode *opt_opc;
    int32_t data;
    uint8_t vece;
    bool prefer_i64;
    bool write_aofs;
} GVecGen4;

typedef struct {
    void (*fni8)(TCGv_i64, TCGv_i64, TCGv_i64, TCGv_i64, int64_t);
    void (*fni4)(TCGv_i32, TCGv_i32, TCGv_i32, TCGv_i32, int32_t);
    void (*fniv)(unsigned, TCGv_vec, TCGv_vec, TCGv_vec, TCGv_vec, int64_t);
    gen_helper_gvec_4 *fno;
    const TCGOpcode *opt_opc;
    uint8_t vece;
    bool prefer_i64;
} GVecGen4i;

void tcg_gen_gvec_2(uint32_t dofs, uint32_t aofs,
                    uint32_t oprsz, uint32_t maxsz, const GVecGen2 *);
void tcg_gen_gvec_2i(uint32_t dofs, uint32_t aofs, uint32_t oprsz,
                     uint32_t maxsz, int64_t c, const GVecGen2i *);
void tcg_gen_gvec_2s(uint32_t dofs, uint32_t aofs, uint32_t oprsz,
                     uint32_t maxsz, TCGv_i64 c, const GVecGen2s *);
void tcg_gen_gvec_3(uint32_t dofs, uint32_t aofs, uint32_t bofs,
                    uint32_t oprsz, uint32_t maxsz, const GVecGen3 *);
void tcg_gen_gvec_3i(uint32_t dofs, uint32_t aofs, uint32_t bofs,
                     uint32_t oprsz, uint32_t maxsz, int64_t c,
                     const GVecGen3i *);
void tcg_gen_gvec_4(uint32_t dofs, uint32_t aofs, uint32_t bofs, uint32_t cofs,
                    uint32_t oprsz, uint32_t maxsz, const GVecGen4 *);
void tcg_gen_gvec_4i(uint32_t dofs, uint32_t aofs, uint32_t bofs, uint32_t cofs,
                     uint32_t oprsz, uint32_t maxsz, int64_t c,
                     const GVecGen4i *);


void tcg_gen_gvec_mov(unsigned vece, uint32_t dofs, uint32_t aofs,
                      uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_not(unsigned vece, uint32_t dofs, uint32_t aofs,
                      uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_neg(unsigned vece, uint32_t dofs, uint32_t aofs,
                      uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_abs(unsigned vece, uint32_t dofs, uint32_t aofs,
                      uint32_t oprsz, uint32_t maxsz);

void tcg_gen_gvec_add(unsigned vece, uint32_t dofs, uint32_t aofs,
                      uint32_t bofs, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_sub(unsigned vece, uint32_t dofs, uint32_t aofs,
                      uint32_t bofs, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_mul(unsigned vece, uint32_t dofs, uint32_t aofs,
                      uint32_t bofs, uint32_t oprsz, uint32_t maxsz);

void tcg_gen_gvec_addi(unsigned vece, uint32_t dofs, uint32_t aofs,
                       int64_t c, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_muli(unsigned vece, uint32_t dofs, uint32_t aofs,
                       int64_t c, uint32_t oprsz, uint32_t maxsz);

void tcg_gen_gvec_adds(unsigned vece, uint32_t dofs, uint32_t aofs,
                       TCGv_i64 c, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_subs(unsigned vece, uint32_t dofs, uint32_t aofs,
                       TCGv_i64 c, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_muls(unsigned vece, uint32_t dofs, uint32_t aofs,
                       TCGv_i64 c, uint32_t oprsz, uint32_t maxsz);

void tcg_gen_gvec_ssadd(unsigned vece, uint32_t dofs, uint32_t aofs,
                        uint32_t bofs, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_sssub(unsigned vece, uint32_t dofs, uint32_t aofs,
                        uint32_t bofs, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_usadd(unsigned vece, uint32_t dofs, uint32_t aofs,
                        uint32_t bofs, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_ussub(unsigned vece, uint32_t dofs, uint32_t aofs,
                        uint32_t bofs, uint32_t oprsz, uint32_t maxsz);

void tcg_gen_gvec_smin(unsigned vece, uint32_t dofs, uint32_t aofs,
                       uint32_t bofs, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_umin(unsigned vece, uint32_t dofs, uint32_t aofs,
                       uint32_t bofs, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_smax(unsigned vece, uint32_t dofs, uint32_t aofs,
                       uint32_t bofs, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_umax(unsigned vece, uint32_t dofs, uint32_t aofs,
                       uint32_t bofs, uint32_t oprsz, uint32_t maxsz);

void tcg_gen_gvec_and(unsigned vece, uint32_t dofs, uint32_t aofs,
                      uint32_t bofs, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_or(unsigned vece, uint32_t dofs, uint32_t aofs,
                     uint32_t bofs, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_xor(unsigned vece, uint32_t dofs, uint32_t aofs,
                      uint32_t bofs, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_andc(unsigned vece, uint32_t dofs, uint32_t aofs,
                       uint32_t bofs, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_orc(unsigned vece, uint32_t dofs, uint32_t aofs,
                      uint32_t bofs, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_nand(unsigned vece, uint32_t dofs, uint32_t aofs,
                       uint32_t bofs, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_nor(unsigned vece, uint32_t dofs, uint32_t aofs,
                      uint32_t bofs, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_eqv(unsigned vece, uint32_t dofs, uint32_t aofs,
                      uint32_t bofs, uint32_t oprsz, uint32_t maxsz);

void tcg_gen_gvec_andi(unsigned vece, uint32_t dofs, uint32_t aofs,
                       int64_t c, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_xori(unsigned vece, uint32_t dofs, uint32_t aofs,
                       int64_t c, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_ori(unsigned vece, uint32_t dofs, uint32_t aofs,
                      int64_t c, uint32_t oprsz, uint32_t maxsz);

void tcg_gen_gvec_ands(unsigned vece, uint32_t dofs, uint32_t aofs,
                       TCGv_i64 c, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_andcs(unsigned vece, uint32_t dofs, uint32_t aofs,
                        TCGv_i64 c, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_xors(unsigned vece, uint32_t dofs, uint32_t aofs,
                       TCGv_i64 c, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_ors(unsigned vece, uint32_t dofs, uint32_t aofs,
                      TCGv_i64 c, uint32_t oprsz, uint32_t maxsz);

void tcg_gen_gvec_dup_mem(unsigned vece, uint32_t dofs, uint32_t aofs,
                          uint32_t s, uint32_t m);
void tcg_gen_gvec_dup_imm(unsigned vece, uint32_t dofs, uint32_t s,
                          uint32_t m, uint64_t imm);
void tcg_gen_gvec_dup_i32(unsigned vece, uint32_t dofs, uint32_t s,
                          uint32_t m, TCGv_i32);
void tcg_gen_gvec_dup_i64(unsigned vece, uint32_t dofs, uint32_t s,
                          uint32_t m, TCGv_i64);

void tcg_gen_gvec_shli(unsigned vece, uint32_t dofs, uint32_t aofs,
                       int64_t shift, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_shri(unsigned vece, uint32_t dofs, uint32_t aofs,
                       int64_t shift, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_sari(unsigned vece, uint32_t dofs, uint32_t aofs,
                       int64_t shift, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_rotli(unsigned vece, uint32_t dofs, uint32_t aofs,
                        int64_t shift, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_rotri(unsigned vece, uint32_t dofs, uint32_t aofs,
                        int64_t shift, uint32_t oprsz, uint32_t maxsz);

void tcg_gen_gvec_shls(unsigned vece, uint32_t dofs, uint32_t aofs,
                       TCGv_i32 shift, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_shrs(unsigned vece, uint32_t dofs, uint32_t aofs,
                       TCGv_i32 shift, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_sars(unsigned vece, uint32_t dofs, uint32_t aofs,
                       TCGv_i32 shift, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_rotls(unsigned vece, uint32_t dofs, uint32_t aofs,
                        TCGv_i32 shift, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_rotrs(unsigned vece, uint32_t dofs, uint32_t aofs,
                        TCGv_i32 shift, uint32_t oprsz, uint32_t maxsz);

void tcg_gen_gvec_shlv(unsigned vece, uint32_t dofs, uint32_t aofs,
                       uint32_t bofs, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_shrv(unsigned vece, uint32_t dofs, uint32_t aofs,
                       uint32_t bofs, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_sarv(unsigned vece, uint32_t dofs, uint32_t aofs,
                       uint32_t bofs, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_rotlv(unsigned vece, uint32_t dofs, uint32_t aofs,
                        uint32_t bofs, uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_rotrv(unsigned vece, uint32_t dofs, uint32_t aofs,
                        uint32_t bofs, uint32_t oprsz, uint32_t maxsz);

void tcg_gen_gvec_cmp(TCGCond cond, unsigned vece, uint32_t dofs,
                      uint32_t aofs, uint32_t bofs,
                      uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_cmpi(TCGCond cond, unsigned vece, uint32_t dofs,
                       uint32_t aofs, int64_t c,
                       uint32_t oprsz, uint32_t maxsz);
void tcg_gen_gvec_cmps(TCGCond cond, unsigned vece, uint32_t dofs,
                       uint32_t aofs, TCGv_i64 c,
                       uint32_t oprsz, uint32_t maxsz);

void tcg_gen_gvec_bitsel(unsigned vece, uint32_t dofs, uint32_t aofs,
                         uint32_t bofs, uint32_t cofs,
                         uint32_t oprsz, uint32_t maxsz);


void tcg_gen_vec_neg8_i64(TCGv_i64 d, TCGv_i64 a);
void tcg_gen_vec_neg16_i64(TCGv_i64 d, TCGv_i64 a);
void tcg_gen_vec_neg32_i64(TCGv_i64 d, TCGv_i64 a);

void tcg_gen_vec_add8_i64(TCGv_i64 d, TCGv_i64 a, TCGv_i64 b);
void tcg_gen_vec_add16_i64(TCGv_i64 d, TCGv_i64 a, TCGv_i64 b);
void tcg_gen_vec_add32_i64(TCGv_i64 d, TCGv_i64 a, TCGv_i64 b);

void tcg_gen_vec_sub8_i64(TCGv_i64 d, TCGv_i64 a, TCGv_i64 b);
void tcg_gen_vec_sub16_i64(TCGv_i64 d, TCGv_i64 a, TCGv_i64 b);
void tcg_gen_vec_sub32_i64(TCGv_i64 d, TCGv_i64 a, TCGv_i64 b);

void tcg_gen_vec_shl8i_i64(TCGv_i64 d, TCGv_i64 a, int64_t);
void tcg_gen_vec_shl16i_i64(TCGv_i64 d, TCGv_i64 a, int64_t);
void tcg_gen_vec_shr8i_i64(TCGv_i64 d, TCGv_i64 a, int64_t);
void tcg_gen_vec_shr16i_i64(TCGv_i64 d, TCGv_i64 a, int64_t);
void tcg_gen_vec_sar8i_i64(TCGv_i64 d, TCGv_i64 a, int64_t);
void tcg_gen_vec_sar16i_i64(TCGv_i64 d, TCGv_i64 a, int64_t);
void tcg_gen_vec_rotl8i_i64(TCGv_i64 d, TCGv_i64 a, int64_t c);
void tcg_gen_vec_rotl16i_i64(TCGv_i64 d, TCGv_i64 a, int64_t c);

void tcg_gen_vec_add8_i32(TCGv_i32 d, TCGv_i32 a, TCGv_i32 b);
void tcg_gen_vec_add16_i32(TCGv_i32 d, TCGv_i32 a, TCGv_i32 b);

void tcg_gen_vec_sub8_i32(TCGv_i32 d, TCGv_i32 a, TCGv_i32 b);
void tcg_gen_vec_sub16_i32(TCGv_i32 d, TCGv_i32 a, TCGv_i32 b);

void tcg_gen_vec_shl8i_i32(TCGv_i32 d, TCGv_i32 a, int32_t);
void tcg_gen_vec_shl16i_i32(TCGv_i32 d, TCGv_i32 a, int32_t);
void tcg_gen_vec_shr8i_i32(TCGv_i32 d, TCGv_i32 a, int32_t);
void tcg_gen_vec_shr16i_i32(TCGv_i32 d, TCGv_i32 a, int32_t);
void tcg_gen_vec_sar8i_i32(TCGv_i32 d, TCGv_i32 a, int32_t);
void tcg_gen_vec_sar16i_i32(TCGv_i32 d, TCGv_i32 a, int32_t);

#endif
