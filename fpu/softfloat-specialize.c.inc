



static inline bool no_signaling_nans(float_status *status)
{
    return status->no_signaling_nans;
}

static inline bool snan_bit_is_one(float_status *status)
{
    return status->snan_bit_is_one;
}


static bool parts_is_snan_frac(uint64_t frac, float_status *status)
{
    if (no_signaling_nans(status)) {
        return false;
    } else {
        bool msb = extract64(frac, DECOMPOSED_BINARY_POINT - 1, 1);
        return msb == snan_bit_is_one(status);
    }
}


static void parts64_default_nan(FloatParts64 *p, float_status *status)
{
    bool sign = 0;
    uint64_t frac;
    uint8_t dnan_pattern = status->default_nan_pattern;

    assert(dnan_pattern != 0);

    sign = dnan_pattern >> 7;
    frac = deposit64(0, DECOMPOSED_BINARY_POINT - 7, 7, dnan_pattern);
    frac = deposit64(frac, 0, DECOMPOSED_BINARY_POINT - 7, -(dnan_pattern & 1));

    *p = (FloatParts64) {
        .cls = float_class_qnan,
        .sign = sign,
        .exp = INT_MAX,
        .frac = frac
    };
}

static void parts128_default_nan(FloatParts128 *p, float_status *status)
{
    FloatParts64 p64;
    parts64_default_nan(&p64, status);

    *p = (FloatParts128) {
        .cls = float_class_qnan,
        .sign = p64.sign,
        .exp = INT_MAX,
        .frac_hi = p64.frac,
        .frac_lo = -(p64.frac & 1)
    };
}


static uint64_t parts_silence_nan_frac(uint64_t frac, float_status *status)
{
    g_assert(!no_signaling_nans(status));

    if (snan_bit_is_one(status)) {
        frac &= ~(1ULL << (DECOMPOSED_BINARY_POINT - 1));
        frac |= 1ULL << (DECOMPOSED_BINARY_POINT - 2);
    } else {
        frac |= 1ULL << (DECOMPOSED_BINARY_POINT - 1);
    }
    return frac;
}

static void parts64_silence_nan(FloatParts64 *p, float_status *status)
{
    p->frac = parts_silence_nan_frac(p->frac, status);
    p->cls = float_class_qnan;
}

static void parts128_silence_nan(FloatParts128 *p, float_status *status)
{
    p->frac_hi = parts_silence_nan_frac(p->frac_hi, status);
    p->cls = float_class_qnan;
}

floatx80 floatx80_default_nan(float_status *status)
{
    floatx80 r;
    FloatParts64 p64;
    parts64_default_nan(&p64, status);

    r.high = 0x7FFF | (p64.sign << 15);
    r.low = (1ULL << DECOMPOSED_BINARY_POINT) | p64.frac;
    return r;
}


floatx80 floatx80_default_inf(bool zSign, float_status *status)
{
    bool z = status->floatx80_behaviour & floatx80_default_inf_int_bit_is_zero;
    return packFloatx80(zSign, 0x7fff, z ? 0 : (1ULL << 63));
}


bool float16_is_quiet_nan(float16 a_, float_status *status)
{
    if (no_signaling_nans(status)) {
        return float16_is_any_nan(a_);
    } else {
        uint16_t a = float16_val(a_);
        if (snan_bit_is_one(status)) {
            return (((a >> 9) & 0x3F) == 0x3E) && (a & 0x1FF);
        } else {

            return ((a >> 9) & 0x3F) == 0x3F;
        }
    }
}


bool bfloat16_is_quiet_nan(bfloat16 a_, float_status *status)
{
    if (no_signaling_nans(status)) {
        return bfloat16_is_any_nan(a_);
    } else {
        uint16_t a = a_;
        if (snan_bit_is_one(status)) {
            return (((a >> 6) & 0x1FF) == 0x1FE) && (a & 0x3F);
        } else {
            return ((a >> 6) & 0x1FF) == 0x1FF;
        }
    }
}


bool float16_is_signaling_nan(float16 a_, float_status *status)
{
    if (no_signaling_nans(status)) {
        return 0;
    } else {
        uint16_t a = float16_val(a_);
        if (snan_bit_is_one(status)) {
            return ((a >> 9) & 0x3F) == 0x3F;
        } else {
            return (((a >> 9) & 0x3F) == 0x3E) && (a & 0x1FF);
        }
    }
}


bool bfloat16_is_signaling_nan(bfloat16 a_, float_status *status)
{
    if (no_signaling_nans(status)) {
        return 0;
    } else {
        uint16_t a = a_;
        if (snan_bit_is_one(status)) {
            return ((a >> 6) & 0x1FF) == 0x1FF;
        } else {
            return (((a >> 6) & 0x1FF) == 0x1FE) && (a & 0x3F);
        }
    }
}


bool float32_is_quiet_nan(float32 a_, float_status *status)
{
    if (no_signaling_nans(status)) {
        return float32_is_any_nan(a_);
    } else {
        uint32_t a = float32_val(a_);
        if (snan_bit_is_one(status)) {
            return (((a >> 22) & 0x1FF) == 0x1FE) && (a & 0x003FFFFF);
        } else {
            return ((uint32_t)(a << 1) >= 0xFF800000);
        }
    }
}


bool float32_is_signaling_nan(float32 a_, float_status *status)
{
    if (no_signaling_nans(status)) {
        return 0;
    } else {
        uint32_t a = float32_val(a_);
        if (snan_bit_is_one(status)) {
            return ((uint32_t)(a << 1) >= 0xFF800000);
        } else {
            return (((a >> 22) & 0x1FF) == 0x1FE) && (a & 0x003FFFFF);
        }
    }
}


bool float64_is_quiet_nan(float64 a_, float_status *status)
{
    if (no_signaling_nans(status)) {
        return float64_is_any_nan(a_);
    } else {
        uint64_t a = float64_val(a_);
        if (snan_bit_is_one(status)) {
            return (((a >> 51) & 0xFFF) == 0xFFE)
                && (a & 0x0007FFFFFFFFFFFFULL);
        } else {
            return ((a << 1) >= 0xFFF0000000000000ULL);
        }
    }
}


bool float64_is_signaling_nan(float64 a_, float_status *status)
{
    if (no_signaling_nans(status)) {
        return 0;
    } else {
        uint64_t a = float64_val(a_);
        if (snan_bit_is_one(status)) {
            return ((a << 1) >= 0xFFF0000000000000ULL);
        } else {
            return (((a >> 51) & 0xFFF) == 0xFFE)
                && (a & UINT64_C(0x0007FFFFFFFFFFFF));
        }
    }
}


int floatx80_is_quiet_nan(floatx80 a, float_status *status)
{
    if (no_signaling_nans(status)) {
        return floatx80_is_any_nan(a);
    } else {
        if (snan_bit_is_one(status)) {
            uint64_t aLow;

            aLow = a.low & ~0x4000000000000000ULL;
            return ((a.high & 0x7FFF) == 0x7FFF)
                && (aLow << 1)
                && (a.low == aLow);
        } else {
            return ((a.high & 0x7FFF) == 0x7FFF)
                && (UINT64_C(0x8000000000000000) <= ((uint64_t)(a.low << 1)));
        }
    }
}


int floatx80_is_signaling_nan(floatx80 a, float_status *status)
{
    if (no_signaling_nans(status)) {
        return 0;
    } else {
        if (snan_bit_is_one(status)) {
            return ((a.high & 0x7FFF) == 0x7FFF)
                && ((a.low << 1) >= 0x8000000000000000ULL);
        } else {
            uint64_t aLow;

            aLow = a.low & ~UINT64_C(0x4000000000000000);
            return ((a.high & 0x7FFF) == 0x7FFF)
                && (uint64_t)(aLow << 1)
                && (a.low == aLow);
        }
    }
}


floatx80 floatx80_silence_nan(floatx80 a, float_status *status)
{
    assert(!snan_bit_is_one(status));
    a.low |= UINT64_C(0xC000000000000000);
    return a;
}


bool float128_is_quiet_nan(float128 a, float_status *status)
{
    if (no_signaling_nans(status)) {
        return float128_is_any_nan(a);
    } else {
        if (snan_bit_is_one(status)) {
            return (((a.high >> 47) & 0xFFFF) == 0xFFFE)
                && (a.low || (a.high & 0x00007FFFFFFFFFFFULL));
        } else {
            return ((a.high << 1) >= 0xFFFF000000000000ULL)
                && (a.low || (a.high & 0x0000FFFFFFFFFFFFULL));
        }
    }
}


bool float128_is_signaling_nan(float128 a, float_status *status)
{
    if (no_signaling_nans(status)) {
        return 0;
    } else {
        if (snan_bit_is_one(status)) {
            return ((a.high << 1) >= 0xFFFF000000000000ULL)
                && (a.low || (a.high & 0x0000FFFFFFFFFFFFULL));
        } else {
            return (((a.high >> 47) & 0xFFFF) == 0xFFFE)
                && (a.low || (a.high & UINT64_C(0x00007FFFFFFFFFFF)));
        }
    }
}
