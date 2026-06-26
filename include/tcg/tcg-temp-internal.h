
#ifndef TCG_TEMP_INTERNAL_H
#define TCG_TEMP_INTERNAL_H


void tcg_temp_free_internal(TCGTemp *);

void tcg_temp_free_i32(TCGv_i32 arg);
void tcg_temp_free_i64(TCGv_i64 arg);
void tcg_temp_free_i128(TCGv_i128 arg);
void tcg_temp_free_ptr(TCGv_ptr arg);
void tcg_temp_free_vec(TCGv_vec arg);

TCGv_i32 tcg_temp_ebb_new_i32(void);
TCGv_i64 tcg_temp_ebb_new_i64(void);
TCGv_ptr tcg_temp_ebb_new_ptr(void);
TCGv_i128 tcg_temp_ebb_new_i128(void);

static inline void tcg_temp_ebb_reset_freed(TCGContext *s)
{
    memset(s->free_temps, 0, sizeof(s->free_temps));
}

#endif /* TCG_TEMP_FREE_H */
