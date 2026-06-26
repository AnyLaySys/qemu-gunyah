

#ifndef SOFTFLOAT_HELPERS_H
#define SOFTFLOAT_HELPERS_H

#include "fpu/softfloat-types.h"

static inline void set_float_detect_tininess(bool val, float_status *status)
{
    status->tininess_before_rounding = val;
}

static inline void set_float_rounding_mode(FloatRoundMode val,
                                           float_status *status)
{
    status->float_rounding_mode = val;
}

static inline void set_float_exception_flags(int val, float_status *status)
{
    status->float_exception_flags = val;
}

static inline void set_floatx80_rounding_precision(FloatX80RoundPrec val,
                                                   float_status *status)
{
    status->floatx80_rounding_precision = val;
}

static inline void set_floatx80_behaviour(FloatX80Behaviour b,
                                          float_status *status)
{
    status->floatx80_behaviour = b;
}

static inline void set_float_2nan_prop_rule(Float2NaNPropRule rule,
                                            float_status *status)
{
    status->float_2nan_prop_rule = rule;
}

static inline void set_float_3nan_prop_rule(Float3NaNPropRule rule,
                                            float_status *status)
{
    status->float_3nan_prop_rule = rule;
}

static inline void set_float_infzeronan_rule(FloatInfZeroNaNRule rule,
                                             float_status *status)
{
    status->float_infzeronan_rule = rule;
}

static inline void set_float_default_nan_pattern(uint8_t dnan_pattern,
                                                 float_status *status)
{
    status->default_nan_pattern = dnan_pattern;
}

static inline void set_flush_to_zero(bool val, float_status *status)
{
    status->flush_to_zero = val;
}

static inline void set_flush_inputs_to_zero(bool val, float_status *status)
{
    status->flush_inputs_to_zero = val;
}

static inline void set_float_ftz_detection(FloatFTZDetection d,
                                           float_status *status)
{
    status->ftz_detection = d;
}

static inline void set_default_nan_mode(bool val, float_status *status)
{
    status->default_nan_mode = val;
}

static inline void set_snan_bit_is_one(bool val, float_status *status)
{
    status->snan_bit_is_one = val;
}

static inline void set_no_signaling_nans(bool val, float_status *status)
{
    status->no_signaling_nans = val;
}

static inline bool get_float_detect_tininess(const float_status *status)
{
    return status->tininess_before_rounding;
}

static inline FloatRoundMode get_float_rounding_mode(const float_status *status)
{
    return status->float_rounding_mode;
}

static inline int get_float_exception_flags(const float_status *status)
{
    return status->float_exception_flags;
}

static inline FloatX80RoundPrec
get_floatx80_rounding_precision(const float_status *status)
{
    return status->floatx80_rounding_precision;
}

static inline FloatX80Behaviour
get_floatx80_behaviour(const float_status *status)
{
    return status->floatx80_behaviour;
}

static inline Float2NaNPropRule
get_float_2nan_prop_rule(const float_status *status)
{
    return status->float_2nan_prop_rule;
}

static inline Float3NaNPropRule
get_float_3nan_prop_rule(const float_status *status)
{
    return status->float_3nan_prop_rule;
}

static inline FloatInfZeroNaNRule
get_float_infzeronan_rule(const float_status *status)
{
    return status->float_infzeronan_rule;
}

static inline uint8_t get_float_default_nan_pattern(const float_status *status)
{
    return status->default_nan_pattern;
}

static inline bool get_flush_to_zero(const float_status *status)
{
    return status->flush_to_zero;
}

static inline bool get_flush_inputs_to_zero(const float_status *status)
{
    return status->flush_inputs_to_zero;
}

static inline bool get_default_nan_mode(const float_status *status)
{
    return status->default_nan_mode;
}

static inline FloatFTZDetection get_float_ftz_detection(const float_status *status)
{
    return status->ftz_detection;
}

#endif /* SOFTFLOAT_HELPERS_H */
