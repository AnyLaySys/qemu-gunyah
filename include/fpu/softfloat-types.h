


#ifndef SOFTFLOAT_TYPES_H
#define SOFTFLOAT_TYPES_H

#include "hw/registerfields.h"


typedef uint16_t float16;
typedef uint32_t float32;
typedef uint64_t float64;
#define float16_val(x) (x)
#define float32_val(x) (x)
#define float64_val(x) (x)
#define make_float16(x) (x)
#define make_float32(x) (x)
#define make_float64(x) (x)
#define const_float16(x) (x)
#define const_float32(x) (x)
#define const_float64(x) (x)
typedef struct {
    uint64_t low;
    uint16_t high;
} floatx80;
#define make_floatx80(exp, mant) ((floatx80) { mant, exp })
#define make_floatx80_init(exp, mant) { .low = mant, .high = exp }
typedef struct {
#if HOST_BIG_ENDIAN
    uint64_t high, low;
#else
    uint64_t low, high;
#endif
} float128;
#define make_float128(high_, low_) ((float128) { .high = high_, .low = low_ })
#define make_float128_init(high_, low_) { .high = high_, .low = low_ }

typedef uint16_t bfloat16;


#define float_tininess_after_rounding  false
#define float_tininess_before_rounding true


typedef enum __attribute__((__packed__)) {
    float_round_nearest_even = 0,
    float_round_down         = 1,
    float_round_up           = 2,
    float_round_to_zero      = 3,
    float_round_ties_away    = 4,
    float_round_to_odd       = 5,
    float_round_to_odd_inf   = 6,
    float_round_nearest_even_max = 7,
} FloatRoundMode;


enum {
    float_flag_invalid         = 0x0001,
    float_flag_divbyzero       = 0x0002,
    float_flag_overflow        = 0x0004,
    float_flag_underflow       = 0x0008,
    float_flag_inexact         = 0x0010,
    float_flag_input_denormal_flushed = 0x0020,
    float_flag_output_denormal_flushed = 0x0040,
    float_flag_invalid_isi     = 0x0080,  /* inf - inf */
    float_flag_invalid_imz     = 0x0100,  /* inf * 0 */
    float_flag_invalid_idi     = 0x0200,  /* inf / inf */
    float_flag_invalid_zdz     = 0x0400,  /* 0 / 0 */
    float_flag_invalid_sqrt    = 0x0800,  /* sqrt(-x) */
    float_flag_invalid_cvti    = 0x1000,  /* non-nan to integer */
    float_flag_invalid_snan    = 0x2000,  /* any operand was snan */
    float_flag_input_denormal_used = 0x4000,
};

typedef enum __attribute__((__packed__)) {
    floatx80_precision_x,
    floatx80_precision_d,
    floatx80_precision_s,
} FloatX80RoundPrec;

typedef enum __attribute__((__packed__)) {
    float_2nan_prop_none = 0,
    float_2nan_prop_s_ab,
    float_2nan_prop_s_ba,
    float_2nan_prop_ab,
    float_2nan_prop_ba,
    float_2nan_prop_x87,
} Float2NaNPropRule;


FIELD(3NAN, 1ST, 0, 2)   /* which operand is most preferred ? */
FIELD(3NAN, 2ND, 2, 2)   /* which operand is next most preferred ? */
FIELD(3NAN, 3RD, 4, 2)   /* which operand is least preferred ? */
FIELD(3NAN, SNAN, 6, 1)  /* do we prefer SNaN over QNaN ? */

#define PROPRULE(X, Y, Z) \
    ((X << R_3NAN_1ST_SHIFT) | (Y << R_3NAN_2ND_SHIFT) | (Z << R_3NAN_3RD_SHIFT))

typedef enum __attribute__((__packed__)) {
    float_3nan_prop_none = 0,     /* No propagation rule specified */
    float_3nan_prop_abc = PROPRULE(0, 1, 2),
    float_3nan_prop_acb = PROPRULE(0, 2, 1),
    float_3nan_prop_bac = PROPRULE(1, 0, 2),
    float_3nan_prop_bca = PROPRULE(1, 2, 0),
    float_3nan_prop_cab = PROPRULE(2, 0, 1),
    float_3nan_prop_cba = PROPRULE(2, 1, 0),
    float_3nan_prop_s_abc = float_3nan_prop_abc | R_3NAN_SNAN_MASK,
    float_3nan_prop_s_acb = float_3nan_prop_acb | R_3NAN_SNAN_MASK,
    float_3nan_prop_s_bac = float_3nan_prop_bac | R_3NAN_SNAN_MASK,
    float_3nan_prop_s_bca = float_3nan_prop_bca | R_3NAN_SNAN_MASK,
    float_3nan_prop_s_cab = float_3nan_prop_cab | R_3NAN_SNAN_MASK,
    float_3nan_prop_s_cba = float_3nan_prop_cba | R_3NAN_SNAN_MASK,
} Float3NaNPropRule;

#undef PROPRULE

typedef enum __attribute__((__packed__)) {
    float_infzeronan_none = 0,
    float_infzeronan_dnan_never = 1,
    float_infzeronan_dnan_always = 2,
    float_infzeronan_dnan_if_qnan = 3,
    float_infzeronan_suppress_invalid = (1 << 7),
} FloatInfZeroNaNRule;

typedef enum __attribute__((__packed__)) {
    float_ftz_after_rounding = 0,
    float_ftz_before_rounding = 1,
} FloatFTZDetection;

typedef enum __attribute__((__packed__)) {
    floatx80_default_inf_int_bit_is_zero = 1,
    floatx80_pseudo_inf_valid = 2,
    floatx80_pseudo_nan_valid = 4,
    floatx80_unnormal_valid = 8,

    floatx80_pseudo_denormal_valid = 16,
} FloatX80Behaviour;


typedef struct float_status {
    uint16_t float_exception_flags;
    FloatRoundMode float_rounding_mode;
    FloatX80RoundPrec floatx80_rounding_precision;
    FloatX80Behaviour floatx80_behaviour;
    Float2NaNPropRule float_2nan_prop_rule;
    Float3NaNPropRule float_3nan_prop_rule;
    FloatInfZeroNaNRule float_infzeronan_rule;
    bool tininess_before_rounding;
    bool flush_to_zero;
    FloatFTZDetection ftz_detection;
    bool flush_inputs_to_zero;
    bool default_nan_mode;
    uint8_t default_nan_pattern;
    bool snan_bit_is_one;
    bool no_signaling_nans;
    bool rebias_overflow;
    bool rebias_underflow;
} float_status;

#endif /* SOFTFLOAT_TYPES_H */
