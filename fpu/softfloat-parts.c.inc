
static void partsN(return_nan)(FloatPartsN *a, float_status *s)
{
    switch (a->cls) {
    case float_class_snan:
        float_raise(float_flag_invalid | float_flag_invalid_snan, s);
        if (s->default_nan_mode) {
            parts_default_nan(a, s);
        } else {
            parts_silence_nan(a, s);
        }
        break;
    case float_class_qnan:
        if (s->default_nan_mode) {
            parts_default_nan(a, s);
        }
        break;
    default:
        g_assert_not_reached();
    }
}

static FloatPartsN *partsN(pick_nan)(FloatPartsN *a, FloatPartsN *b,
                                     float_status *s)
{
    bool have_snan = false;
    FloatPartsN *ret;
    int cmp;

    if (is_snan(a->cls) || is_snan(b->cls)) {
        float_raise(float_flag_invalid | float_flag_invalid_snan, s);
        have_snan = true;
    }

    if (s->default_nan_mode) {
        parts_default_nan(a, s);
        return a;
    }

    switch (s->float_2nan_prop_rule) {
    case float_2nan_prop_s_ab:
        if (have_snan) {
            ret = is_snan(a->cls) ? a : b;
            break;
        }
    case float_2nan_prop_ab:
        ret = is_nan(a->cls) ? a : b;
        break;
    case float_2nan_prop_s_ba:
        if (have_snan) {
            ret = is_snan(b->cls) ? b : a;
            break;
        }
    case float_2nan_prop_ba:
        ret = is_nan(b->cls) ? b : a;
        break;
    case float_2nan_prop_x87:
        if (is_snan(a->cls)) {
            if (!is_snan(b->cls)) {
                ret = is_qnan(b->cls) ? b : a;
                break;
            }
        } else if (is_qnan(a->cls)) {
            if (is_snan(b->cls) || !is_qnan(b->cls)) {
                ret = a;
                break;
            }
        } else {
            ret = b;
            break;
        }
        cmp = frac_cmp(a, b);
        if (cmp == 0) {
            cmp = a->sign < b->sign;
        }
        ret = cmp > 0 ? a : b;
        break;
    default:
        g_assert_not_reached();
    }

    if (is_snan(ret->cls)) {
        parts_silence_nan(ret, s);
    }
    return ret;
}

static FloatPartsN *partsN(pick_nan_muladd)(FloatPartsN *a, FloatPartsN *b,
                                            FloatPartsN *c, float_status *s,
                                            int ab_mask, int abc_mask)
{
    bool infzero = (ab_mask == float_cmask_infzero);
    bool have_snan = (abc_mask & float_cmask_snan);
    FloatPartsN *ret;

    if (unlikely(have_snan)) {
        float_raise(float_flag_invalid | float_flag_invalid_snan, s);
    }

    if (infzero &&
        !(s->float_infzeronan_rule & float_infzeronan_suppress_invalid)) {
        float_raise(float_flag_invalid | float_flag_invalid_imz, s);
    }

    if (s->default_nan_mode) {
        goto default_nan;
    } else if (infzero) {
        switch (s->float_infzeronan_rule & ~float_infzeronan_suppress_invalid) {
        case float_infzeronan_dnan_never:
            break;
        case float_infzeronan_dnan_always:
            goto default_nan;
        case float_infzeronan_dnan_if_qnan:
            if (is_qnan(c->cls)) {
                goto default_nan;
            }
            break;
        default:
            g_assert_not_reached();
        }
        ret = c;
    } else {
        FloatPartsN *val[R_3NAN_1ST_MASK + 1] = { a, b, c };
        Float3NaNPropRule rule = s->float_3nan_prop_rule;

        assert(rule != float_3nan_prop_none);
        if (have_snan && (rule & R_3NAN_SNAN_MASK)) {
            do {
                ret = val[rule & R_3NAN_1ST_MASK];
                rule >>= R_3NAN_1ST_LENGTH;
            } while (!is_snan(ret->cls));
        } else {
            do {
                ret = val[rule & R_3NAN_1ST_MASK];
                rule >>= R_3NAN_1ST_LENGTH;
            } while (!is_nan(ret->cls));
        }
    }

    if (is_snan(ret->cls)) {
        parts_silence_nan(ret, s);
    }
    return ret;

 default_nan:
    parts_default_nan(a, s);
    return a;
}

static void partsN(canonicalize)(FloatPartsN *p, float_status *status,
                                 const FloatFmt *fmt)
{
    bool has_pseudo_denormals = fmt->has_explicit_bit &&
        (status->floatx80_behaviour & floatx80_pseudo_denormal_valid);

    if (unlikely(p->exp == 0)) {
        if (likely(frac_eqz(p))) {
            p->cls = float_class_zero;
        } else if (status->flush_inputs_to_zero) {
            float_raise(float_flag_input_denormal_flushed, status);
            p->cls = float_class_zero;
            frac_clear(p);
        } else {
            int shift = frac_normalize(p);
            p->cls = float_class_denormal;
            p->exp = fmt->frac_shift - fmt->exp_bias
                   - shift + !has_pseudo_denormals;
        }
    } else if (likely(p->exp < fmt->exp_max) || fmt->arm_althp) {
        p->cls = float_class_normal;
        p->exp -= fmt->exp_bias;
        frac_shl(p, fmt->frac_shift);
        p->frac_hi |= DECOMPOSED_IMPLICIT_BIT;
    } else if (likely(frac_eqz(p))) {
        p->cls = float_class_inf;
    } else {
        frac_shl(p, fmt->frac_shift);
        p->cls = (parts_is_snan_frac(p->frac_hi, status)
                  ? float_class_snan : float_class_qnan);
    }
}

static void partsN(uncanon_normal)(FloatPartsN *p, float_status *s,
                                   const FloatFmt *fmt)
{
    const int exp_max = fmt->exp_max;
    const int frac_shift = fmt->frac_shift;
    const uint64_t round_mask = fmt->round_mask;
    const uint64_t frac_lsb = round_mask + 1;
    const uint64_t frac_lsbm1 = round_mask ^ (round_mask >> 1);
    const uint64_t roundeven_mask = round_mask | frac_lsb;
    uint64_t inc;
    bool overflow_norm = false;
    int exp, flags = 0;

    switch (s->float_rounding_mode) {
    case float_round_nearest_even_max:
        overflow_norm = true;
    case float_round_nearest_even:
        if (N > 64 && frac_lsb == 0) {
            inc = ((p->frac_hi & 1) || (p->frac_lo & round_mask) != frac_lsbm1
                   ? frac_lsbm1 : 0);
        } else {
            inc = ((p->frac_lo & roundeven_mask) != frac_lsbm1
                   ? frac_lsbm1 : 0);
        }
        break;
    case float_round_ties_away:
        inc = frac_lsbm1;
        break;
    case float_round_to_zero:
        overflow_norm = true;
        inc = 0;
        break;
    case float_round_up:
        inc = p->sign ? 0 : round_mask;
        overflow_norm = p->sign;
        break;
    case float_round_down:
        inc = p->sign ? round_mask : 0;
        overflow_norm = !p->sign;
        break;
    case float_round_to_odd:
        overflow_norm = true;
    case float_round_to_odd_inf:
        if (N > 64 && frac_lsb == 0) {
            inc = p->frac_hi & 1 ? 0 : round_mask;
        } else {
            inc = p->frac_lo & frac_lsb ? 0 : round_mask;
        }
        break;
    default:
        g_assert_not_reached();
    }

    exp = p->exp + fmt->exp_bias;
    if (likely(exp > 0)) {
        if (p->frac_lo & round_mask) {
            flags |= float_flag_inexact;
            if (frac_addi(p, p, inc)) {
                frac_shr(p, 1);
                p->frac_hi |= DECOMPOSED_IMPLICIT_BIT;
                exp++;
            }
            p->frac_lo &= ~round_mask;
        }

        if (fmt->arm_althp) {
            if (unlikely(exp > exp_max)) {
                flags = float_flag_invalid;
                exp = exp_max;
                frac_allones(p);
                p->frac_lo &= ~round_mask;
            }
        } else if (unlikely(exp >= exp_max)) {
            flags |= float_flag_overflow;
            if (s->rebias_overflow) {
                exp -= fmt->exp_re_bias;
            } else if (overflow_norm) {
                flags |= float_flag_inexact;
                exp = exp_max - 1;
                frac_allones(p);
                p->frac_lo &= ~round_mask;
            } else {
                flags |= float_flag_inexact;
                p->cls = float_class_inf;
                exp = exp_max;
                frac_clear(p);
            }
        }
        frac_shr(p, frac_shift);
    } else if (unlikely(s->rebias_underflow)) {
        flags |= float_flag_underflow;
        exp += fmt->exp_re_bias;
        if (p->frac_lo & round_mask) {
            flags |= float_flag_inexact;
            if (frac_addi(p, p, inc)) {
                frac_shr(p, 1);
                p->frac_hi |= DECOMPOSED_IMPLICIT_BIT;
                exp++;
            }
            p->frac_lo &= ~round_mask;
        }
        frac_shr(p, frac_shift);
    } else if (s->flush_to_zero &&
               s->ftz_detection == float_ftz_before_rounding) {
        flags |= float_flag_output_denormal_flushed;
        p->cls = float_class_zero;
        exp = 0;
        frac_clear(p);
    } else {
        bool is_tiny = s->tininess_before_rounding || exp < 0;
        bool has_pseudo_denormals = fmt->has_explicit_bit &&
            (s->floatx80_behaviour & floatx80_pseudo_denormal_valid);

        if (!is_tiny) {
            FloatPartsN discard;
            is_tiny = !frac_addi(&discard, p, inc);
        }

        frac_shrjam(p, !has_pseudo_denormals - exp);

        if (p->frac_lo & round_mask) {
            switch (s->float_rounding_mode) {
            case float_round_nearest_even:
                if (N > 64 && frac_lsb == 0) {
                    inc = ((p->frac_hi & 1) ||
                           (p->frac_lo & round_mask) != frac_lsbm1
                           ? frac_lsbm1 : 0);
                } else {
                    inc = ((p->frac_lo & roundeven_mask) != frac_lsbm1
                           ? frac_lsbm1 : 0);
                }
                break;
            case float_round_to_odd:
            case float_round_to_odd_inf:
                if (N > 64 && frac_lsb == 0) {
                    inc = p->frac_hi & 1 ? 0 : round_mask;
                } else {
                    inc = p->frac_lo & frac_lsb ? 0 : round_mask;
                }
                break;
            default:
                break;
            }
            flags |= float_flag_inexact;
            frac_addi(p, p, inc);
            p->frac_lo &= ~round_mask;
        }

        exp = (p->frac_hi & DECOMPOSED_IMPLICIT_BIT) && !has_pseudo_denormals;
        frac_shr(p, frac_shift);

        if (is_tiny) {
            if (s->flush_to_zero) {
                assert(s->ftz_detection == float_ftz_after_rounding);
                flags |= float_flag_output_denormal_flushed;
                p->cls = float_class_zero;
                exp = 0;
                frac_clear(p);
            } else if (flags & float_flag_inexact) {
                flags |= float_flag_underflow;
            }
            if (exp == 0 && frac_eqz(p)) {
                p->cls = float_class_zero;
            }
        }
    }
    p->exp = exp;
    float_raise(flags, s);
}

static void partsN(uncanon)(FloatPartsN *p, float_status *s,
                            const FloatFmt *fmt)
{
    if (likely(is_anynorm(p->cls))) {
        parts_uncanon_normal(p, s, fmt);
    } else {
        switch (p->cls) {
        case float_class_zero:
            p->exp = 0;
            frac_clear(p);
            return;
        case float_class_inf:
            g_assert(!fmt->arm_althp);
            p->exp = fmt->exp_max;
            frac_clear(p);
            return;
        case float_class_qnan:
        case float_class_snan:
            g_assert(!fmt->arm_althp);
            p->exp = fmt->exp_max;
            frac_shr(p, fmt->frac_shift);
            return;
        default:
            break;
        }
        g_assert_not_reached();
    }
}

static FloatPartsN *partsN(addsub)(FloatPartsN *a, FloatPartsN *b,
                                   float_status *s, bool subtract)
{
    bool b_sign = b->sign ^ subtract;
    int ab_mask = float_cmask(a->cls) | float_cmask(b->cls);

    if ((ab_mask & (float_cmask_denormal | float_cmask_anynan)) ==
        float_cmask_denormal) {
        float_raise(float_flag_input_denormal_used, s);
    }

    if (a->sign != b_sign) {
        if (likely(cmask_is_only_normals(ab_mask))) {
            if (parts_sub_normal(a, b)) {
                return a;
            }
            ab_mask = float_cmask_zero;
        }

        if (ab_mask == float_cmask_zero) {
            a->sign = s->float_rounding_mode == float_round_down;
            return a;
        }

        if (unlikely(ab_mask & float_cmask_anynan)) {
            goto p_nan;
        }

        if (ab_mask & float_cmask_inf) {
            if (a->cls != float_class_inf) {
                goto return_b;
            }
            if (b->cls != float_class_inf) {
                return a;
            }
            float_raise(float_flag_invalid | float_flag_invalid_isi, s);
            parts_default_nan(a, s);
            return a;
        }
    } else {
        if (likely(cmask_is_only_normals(ab_mask))) {
            parts_add_normal(a, b);
            return a;
        }

        if (ab_mask == float_cmask_zero) {
            return a;
        }

        if (unlikely(ab_mask & float_cmask_anynan)) {
            goto p_nan;
        }

        if (ab_mask & float_cmask_inf) {
            a->cls = float_class_inf;
            return a;
        }
    }

    if (b->cls == float_class_zero) {
        g_assert(is_anynorm(a->cls));
        return a;
    }

    g_assert(a->cls == float_class_zero);
    g_assert(is_anynorm(b->cls));
 return_b:
    b->sign = b_sign;
    return b;

 p_nan:
    return parts_pick_nan(a, b, s);
}

static FloatPartsN *partsN(mul)(FloatPartsN *a, FloatPartsN *b,
                                float_status *s)
{
    int ab_mask = float_cmask(a->cls) | float_cmask(b->cls);
    bool sign = a->sign ^ b->sign;

    if (likely(cmask_is_only_normals(ab_mask))) {
        FloatPartsW tmp;

        if (ab_mask & float_cmask_denormal) {
            float_raise(float_flag_input_denormal_used, s);
        }

        frac_mulw(&tmp, a, b);
        frac_truncjam(a, &tmp);

        a->exp += b->exp + 1;
        if (!(a->frac_hi & DECOMPOSED_IMPLICIT_BIT)) {
            frac_add(a, a, a);
            a->exp -= 1;
        }

        a->sign = sign;
        return a;
    }

    if (unlikely(ab_mask == float_cmask_infzero)) {
        float_raise(float_flag_invalid | float_flag_invalid_imz, s);
        parts_default_nan(a, s);
        return a;
    }

    if (unlikely(ab_mask & float_cmask_anynan)) {
        return parts_pick_nan(a, b, s);
    }

    if (ab_mask & float_cmask_denormal) {
        float_raise(float_flag_input_denormal_used, s);
    }

    if (ab_mask & float_cmask_inf) {
        a->cls = float_class_inf;
        a->sign = sign;
        return a;
    }

    g_assert(ab_mask & float_cmask_zero);
    a->cls = float_class_zero;
    a->sign = sign;
    return a;
}

static FloatPartsN *partsN(muladd_scalbn)(FloatPartsN *a, FloatPartsN *b,
                                          FloatPartsN *c, int scale,
                                          int flags, float_status *s)
{
    int ab_mask, abc_mask;
    FloatPartsW p_widen, c_widen;

    ab_mask = float_cmask(a->cls) | float_cmask(b->cls);
    abc_mask = float_cmask(c->cls) | ab_mask;

    if (unlikely(abc_mask & float_cmask_anynan)) {
        return parts_pick_nan_muladd(a, b, c, s, ab_mask, abc_mask);
    }

    if (flags & float_muladd_negate_c) {
        c->sign ^= 1;
    }

    a->sign ^= b->sign;
    if (flags & float_muladd_negate_product) {
        a->sign ^= 1;
    }

    if (unlikely(!cmask_is_only_normals(ab_mask))) {
        if (unlikely(ab_mask == float_cmask_infzero)) {
            float_raise(float_flag_invalid | float_flag_invalid_imz, s);
            goto d_nan;
        }

        if (ab_mask & float_cmask_inf) {
            if (c->cls == float_class_inf && a->sign != c->sign) {
                float_raise(float_flag_invalid | float_flag_invalid_isi, s);
                goto d_nan;
            }
            goto return_inf;
        }

        g_assert(ab_mask & float_cmask_zero);
        if (is_anynorm(c->cls)) {
            *a = *c;
            goto return_normal;
        }
        if (c->cls == float_class_zero) {
            if (flags & float_muladd_suppress_add_product_zero) {
                a->sign = c->sign;
            } else if (a->sign != c->sign) {
                goto return_sub_zero;
            }
            goto return_zero;
        }
        g_assert(c->cls == float_class_inf);
    }

    if (unlikely(c->cls == float_class_inf)) {
        a->sign = c->sign;
        goto return_inf;
    }

    p_widen.sign = a->sign;
    p_widen.exp = a->exp + b->exp + 1;
    frac_mulw(&p_widen, a, b);
    if (!(p_widen.frac_hi & DECOMPOSED_IMPLICIT_BIT)) {
        frac_add(&p_widen, &p_widen, &p_widen);
        p_widen.exp -= 1;
    }

    if (c->cls != float_class_zero) {
        frac_widen(&c_widen, c);
        c_widen.exp = c->exp;

        if (a->sign == c->sign) {
            parts_add_normal(&p_widen, &c_widen);
        } else if (!parts_sub_normal(&p_widen, &c_widen)) {
            goto return_sub_zero;
        }
    }

    frac_truncjam(a, &p_widen);
    a->sign = p_widen.sign;
    a->exp = p_widen.exp;

 return_normal:
    a->exp += scale;
 finish_sign:
    if (flags & float_muladd_negate_result) {
        a->sign ^= 1;
    }

    if (abc_mask & float_cmask_denormal) {
        float_raise(float_flag_input_denormal_used, s);
    }
    return a;

 return_sub_zero:
    a->sign = s->float_rounding_mode == float_round_down;
 return_zero:
    a->cls = float_class_zero;
    goto finish_sign;

 return_inf:
    a->cls = float_class_inf;
    goto finish_sign;

 d_nan:
    parts_default_nan(a, s);
    return a;
}

static FloatPartsN *partsN(div)(FloatPartsN *a, FloatPartsN *b,
                                float_status *s)
{
    int ab_mask = float_cmask(a->cls) | float_cmask(b->cls);
    bool sign = a->sign ^ b->sign;

    if (likely(cmask_is_only_normals(ab_mask))) {
        if (ab_mask & float_cmask_denormal) {
            float_raise(float_flag_input_denormal_used, s);
        }
        a->sign = sign;
        a->exp -= b->exp + frac_div(a, b);
        return a;
    }

    if (unlikely(ab_mask == float_cmask_zero)) {
        float_raise(float_flag_invalid | float_flag_invalid_zdz, s);
        goto d_nan;
    }
    if (unlikely(ab_mask == float_cmask_inf)) {
        float_raise(float_flag_invalid | float_flag_invalid_idi, s);
        goto d_nan;
    }

    if (unlikely(ab_mask & float_cmask_anynan)) {
        return parts_pick_nan(a, b, s);
    }

    if ((ab_mask & float_cmask_denormal) && b->cls != float_class_zero) {
        float_raise(float_flag_input_denormal_used, s);
    }

    a->sign = sign;

    if (a->cls == float_class_inf) {
        return a;
    }

    if (a->cls == float_class_zero) {
        return a;
    }

    if (b->cls == float_class_inf) {
        a->cls = float_class_zero;
        return a;
    }

    g_assert(b->cls == float_class_zero);
    float_raise(float_flag_divbyzero, s);
    a->cls = float_class_inf;
    return a;

 d_nan:
    parts_default_nan(a, s);
    return a;
}

static FloatPartsN *partsN(modrem)(FloatPartsN *a, FloatPartsN *b,
                                   uint64_t *mod_quot, float_status *s)
{
    int ab_mask = float_cmask(a->cls) | float_cmask(b->cls);

    if (likely(cmask_is_only_normals(ab_mask))) {
        if (ab_mask & float_cmask_denormal) {
            float_raise(float_flag_input_denormal_used, s);
        }
        frac_modrem(a, b, mod_quot);
        return a;
    }

    if (mod_quot) {
        *mod_quot = 0;
    }

    if (unlikely(ab_mask & float_cmask_anynan)) {
        return parts_pick_nan(a, b, s);
    }

    if (a->cls == float_class_inf || b->cls == float_class_zero) {
        float_raise(float_flag_invalid, s);
        parts_default_nan(a, s);
        return a;
    }

    if (ab_mask & float_cmask_denormal) {
        float_raise(float_flag_input_denormal_used, s);
    }

    g_assert(b->cls == float_class_inf || a->cls == float_class_zero);
    return a;
}

static void partsN(sqrt)(FloatPartsN *a, float_status *status,
                         const FloatFmt *fmt)
{
    const uint32_t three32 = 3u << 30;
    const uint64_t three64 = 3ull << 62;
    uint32_t d32, m32, r32, s32, u32;            /* 32-bit computation */
    uint64_t d64, m64, r64, s64, u64;            /* 64-bit computation */
    uint64_t dh, dl, rh, rl, sh, sl, uh, ul;     /* 128-bit computation */
    uint64_t d0h, d0l, d1h, d1l, d2h, d2l;
    uint64_t discard;
    bool exp_odd;
    size_t index;

    if (unlikely(a->cls != float_class_normal)) {
        switch (a->cls) {
        case float_class_denormal:
            if (!a->sign) {
                float_raise(float_flag_input_denormal_used, status);
            }
            break;
        case float_class_snan:
        case float_class_qnan:
            parts_return_nan(a, status);
            return;
        case float_class_zero:
            return;
        case float_class_inf:
            if (unlikely(a->sign)) {
                goto d_nan;
            }
            return;
        default:
            g_assert_not_reached();
        }
    }

    if (unlikely(a->sign)) {
        goto d_nan;
    }

    exp_odd = a->exp & 1;
    index = extract64(a->frac_hi, 57, 6) | (!exp_odd << 6);
    if (!exp_odd) {
        frac_shr(a, 1);
    }

    m64 = a->frac_hi;
    m32 = m64 >> 32;

    r32 = rsqrt_tab[index] << 16;

    s32 = ((uint64_t)m32 * r32) >> 32;
    d32 = ((uint64_t)s32 * r32) >> 32;
    u32 = three32 - d32;

    if (N == 64) {

        r32 = ((uint64_t)r32 * u32) >> 31;

        s32 = ((uint64_t)m32 * r32) >> 32;
        d32 = ((uint64_t)s32 * r32) >> 32;
        u32 = three32 - d32;

        if (fmt->frac_size <= 23) {

            s32 = ((uint64_t)s32 * u32) >> 32;  /* 3.29 */
            s32 = (s32 - 1) >> 6;               /* 9.23 */

            uint32_t d0 = (m32 << 16) - s32 * s32;
            uint32_t d1 = s32 - d0;
            uint32_t d2 = d1 + s32 + 1;
            s32 += d1 >> 31;
            a->frac_hi = (uint64_t)s32 << (64 - 25);

            if (d2 != 0) {
                a->frac_hi += ((int32_t)(d1 ^ d2) < 0 ? -1 : 1);
            }
            goto done;
        }


        r64 = (uint64_t)r32 * u32 * 2;
        mul64To128(m64, r64, &s64, &discard);
        mul64To128(s64, r64, &d64, &discard);
        u64 = three64 - d64;

        mul64To128(s64, u64, &s64, &discard);  /* 3.61 */
        s64 = (s64 - 2) >> 9;                  /* 12.52 */

        uint64_t d0 = (m64 << 42) - s64 * s64;
        uint64_t d1 = s64 - d0;
        uint64_t d2 = d1 + s64 + 1;
        s64 += d1 >> 63;
        a->frac_hi = s64 << (64 - 54);

        if (d2 != 0) {
            a->frac_hi += ((int64_t)(d1 ^ d2) < 0 ? -1 : 1);
        }
        goto done;
    }

    r64 = (uint64_t)r32 * u32 * 2;

    mul64To128(m64, r64, &s64, &discard);
    mul64To128(s64, r64, &d64, &discard);
    u64 = three64 - d64;
    mul64To128(u64, r64, &r64, &discard);
    r64 <<= 1;

    mul64To128(m64, r64, &s64, &discard);
    mul64To128(s64, r64, &d64, &discard);
    u64 = three64 - d64;
    mul64To128(u64, r64, &rh, &rl);
    add128(rh, rl, rh, rl, &rh, &rl);

    mul128To256(a->frac_hi, a->frac_lo, rh, rl, &sh, &sl, &discard, &discard);
    mul128To256(sh, sl, rh, rl, &dh, &dl, &discard, &discard);
    sub128(three64, 0, dh, dl, &uh, &ul);
    mul128To256(uh, ul, sh, sl, &sh, &sl, &discard, &discard);  /* 3.125 */

    sub128(sh, sl, 0, 4, &sh, &sl);
    shift128Right(sh, sl, 13, &sh, &sl);  /* 16.112 */

    mul64To128(sl, sl, &d0h, &d0l);
    d0h += 2 * sh * sl;
    sub128(a->frac_lo << 34, 0, d0h, d0l, &d0h, &d0l);
    sub128(sh, sl, d0h, d0l, &d1h, &d1l);
    add128(sh, sl, 0, 1, &d2h, &d2l);
    add128(d2h, d2l, d1h, d1l, &d2h, &d2l);
    add128(sh, sl, 0, d1h >> 63, &sh, &sl);
    shift128Left(sh, sl, 128 - 114, &sh, &sl);

    if (d2h | d2l) {
        if ((int64_t)(d1h ^ d2h) < 0) {
            sub128(sh, sl, 0, 1, &sh, &sl);
        } else {
            add128(sh, sl, 0, 1, &sh, &sl);
        }
    }
    a->frac_lo = sl;
    a->frac_hi = sh;

 done:
    a->exp >>= 1;
    if (!(a->frac_hi & DECOMPOSED_IMPLICIT_BIT)) {
        frac_add(a, a, a);
    } else {
        a->exp += 1;
    }
    return;

 d_nan:
    float_raise(float_flag_invalid | float_flag_invalid_sqrt, status);
    parts_default_nan(a, status);
}

static bool partsN(round_to_int_normal)(FloatPartsN *a, FloatRoundMode rmode,
                                        int scale, int frac_size)
{
    uint64_t frac_lsb, frac_lsbm1, rnd_even_mask, rnd_mask, inc;
    int shift_adj;

    scale = MIN(MAX(scale, -0x10000), 0x10000);
    a->exp += scale;

    if (a->exp < 0) {
        bool one;

        switch (rmode) {
        case float_round_nearest_even:
            one = false;
            if (a->exp == -1) {
                FloatPartsN tmp;
                frac_add(&tmp, a, a);
                one = !frac_eqz(&tmp);
            }
            break;
        case float_round_ties_away:
            one = a->exp == -1;
            break;
        case float_round_to_zero:
            one = false;
            break;
        case float_round_up:
            one = !a->sign;
            break;
        case float_round_down:
            one = a->sign;
            break;
        case float_round_to_odd:
            one = true;
            break;
        default:
            g_assert_not_reached();
        }

        frac_clear(a);
        a->exp = 0;
        if (one) {
            a->frac_hi = DECOMPOSED_IMPLICIT_BIT;
        } else {
            a->cls = float_class_zero;
        }
        return true;
    }

    if (a->exp >= frac_size) {
        return false;
    }

    if (N > 64 && a->exp < N - 64) {
        shift_adj = (N - 1) - (a->exp + 2);
        frac_shrjam(a, shift_adj);
        frac_lsb = 1 << 2;
    } else {
        shift_adj = 0;
        frac_lsb = DECOMPOSED_IMPLICIT_BIT >> (a->exp & 63);
    }

    frac_lsbm1 = frac_lsb >> 1;
    rnd_mask = frac_lsb - 1;
    rnd_even_mask = rnd_mask | frac_lsb;

    if (!(a->frac_lo & rnd_mask)) {
        frac_shl(a, shift_adj);
        return false;
    }

    switch (rmode) {
    case float_round_nearest_even:
        inc = ((a->frac_lo & rnd_even_mask) != frac_lsbm1 ? frac_lsbm1 : 0);
        break;
    case float_round_ties_away:
        inc = frac_lsbm1;
        break;
    case float_round_to_zero:
        inc = 0;
        break;
    case float_round_up:
        inc = a->sign ? 0 : rnd_mask;
        break;
    case float_round_down:
        inc = a->sign ? rnd_mask : 0;
        break;
    case float_round_to_odd:
        inc = a->frac_lo & frac_lsb ? 0 : rnd_mask;
        break;
    default:
        g_assert_not_reached();
    }

    if (shift_adj == 0) {
        if (frac_addi(a, a, inc)) {
            frac_shr(a, 1);
            a->frac_hi |= DECOMPOSED_IMPLICIT_BIT;
            a->exp++;
        }
        a->frac_lo &= ~rnd_mask;
    } else {
        frac_addi(a, a, inc);
        a->frac_lo &= ~rnd_mask;
        frac_shl(a, shift_adj - 1);
        if (a->frac_hi & DECOMPOSED_IMPLICIT_BIT) {
            a->exp++;
        } else {
            frac_add(a, a, a);
        }
    }
    return true;
}

static void partsN(round_to_int)(FloatPartsN *a, FloatRoundMode rmode,
                                 int scale, float_status *s,
                                 const FloatFmt *fmt)
{
    switch (a->cls) {
    case float_class_qnan:
    case float_class_snan:
        parts_return_nan(a, s);
        break;
    case float_class_zero:
    case float_class_inf:
        break;
    case float_class_normal:
    case float_class_denormal:
        if (parts_round_to_int_normal(a, rmode, scale, fmt->frac_size)) {
            float_raise(float_flag_inexact, s);
        }
        break;
    default:
        g_assert_not_reached();
    }
}

static int64_t partsN(float_to_sint)(FloatPartsN *p, FloatRoundMode rmode,
                                     int scale, int64_t min, int64_t max,
                                     float_status *s)
{
    int flags = 0;
    uint64_t r;

    switch (p->cls) {
    case float_class_snan:
        flags |= float_flag_invalid_snan;
    case float_class_qnan:
        flags |= float_flag_invalid;
        r = max;
        break;

    case float_class_inf:
        flags = float_flag_invalid | float_flag_invalid_cvti;
        r = p->sign ? min : max;
        break;

    case float_class_zero:
        return 0;

    case float_class_normal:
    case float_class_denormal:
        if (parts_round_to_int_normal(p, rmode, scale, N - 2)) {
            flags = float_flag_inexact;
        }

        if (p->exp <= DECOMPOSED_BINARY_POINT) {
            r = p->frac_hi >> (DECOMPOSED_BINARY_POINT - p->exp);
        } else {
            r = UINT64_MAX;
        }
        if (p->sign) {
            if (r <= -(uint64_t)min) {
                r = -r;
            } else {
                flags = float_flag_invalid | float_flag_invalid_cvti;
                r = min;
            }
        } else if (r > max) {
            flags = float_flag_invalid | float_flag_invalid_cvti;
            r = max;
        }
        break;

    default:
        g_assert_not_reached();
    }

    float_raise(flags, s);
    return r;
}

static uint64_t partsN(float_to_uint)(FloatPartsN *p, FloatRoundMode rmode,
                                      int scale, uint64_t max, float_status *s)
{
    int flags = 0;
    uint64_t r;

    switch (p->cls) {
    case float_class_snan:
        flags |= float_flag_invalid_snan;
    case float_class_qnan:
        flags |= float_flag_invalid;
        r = max;
        break;

    case float_class_inf:
        flags = float_flag_invalid | float_flag_invalid_cvti;
        r = p->sign ? 0 : max;
        break;

    case float_class_zero:
        return 0;

    case float_class_normal:
    case float_class_denormal:
        if (parts_round_to_int_normal(p, rmode, scale, N - 2)) {
            flags = float_flag_inexact;
            if (p->cls == float_class_zero) {
                r = 0;
                break;
            }
        }

        if (p->sign) {
            flags = float_flag_invalid | float_flag_invalid_cvti;
            r = 0;
        } else if (p->exp > DECOMPOSED_BINARY_POINT) {
            flags = float_flag_invalid | float_flag_invalid_cvti;
            r = max;
        } else {
            r = p->frac_hi >> (DECOMPOSED_BINARY_POINT - p->exp);
            if (r > max) {
                flags = float_flag_invalid | float_flag_invalid_cvti;
                r = max;
            }
        }
        break;

    default:
        g_assert_not_reached();
    }

    float_raise(flags, s);
    return r;
}

static int64_t partsN(float_to_sint_modulo)(FloatPartsN *p,
                                            FloatRoundMode rmode,
                                            int bitsm1, float_status *s)
{
    int flags = 0;
    uint64_t r;
    bool overflow = false;

    switch (p->cls) {
    case float_class_snan:
        flags |= float_flag_invalid_snan;
    case float_class_qnan:
        flags |= float_flag_invalid;
        r = 0;
        break;

    case float_class_inf:
        overflow = true;
        r = 0;
        break;

    case float_class_zero:
        return 0;

    case float_class_normal:
    case float_class_denormal:
        if (parts_round_to_int_normal(p, rmode, 0, N - 2)) {
            flags = float_flag_inexact;
        }

        if (p->exp <= DECOMPOSED_BINARY_POINT) {
            r = p->frac_hi >> (DECOMPOSED_BINARY_POINT - p->exp);
            if (p->exp < bitsm1) {
            } else if (p->exp == bitsm1) {
                overflow = !p->sign || p->frac_hi != DECOMPOSED_IMPLICIT_BIT;
            } else {
                overflow = true;
            }
        } else {
            int shl = p->exp - DECOMPOSED_BINARY_POINT;
            if (shl < N) {
                frac_shl(p, shl);
                r = p->frac_hi;
            } else {
                r = 0;
            }
            overflow = true;
        }

        if (p->sign) {
            r = -r;
        }
        break;

    default:
        g_assert_not_reached();
    }

    if (overflow) {
        flags = float_flag_invalid | float_flag_invalid_cvti;
    }
    float_raise(flags, s);
    return r;
}

static void partsN(sint_to_float)(FloatPartsN *p, int64_t a,
                                  int scale, float_status *s)
{
    uint64_t f = a;
    int shift;

    memset(p, 0, sizeof(*p));

    if (a == 0) {
        p->cls = float_class_zero;
        return;
    }

    p->cls = float_class_normal;
    if (a < 0) {
        f = -f;
        p->sign = true;
    }
    shift = clz64(f);
    scale = MIN(MAX(scale, -0x10000), 0x10000);

    p->exp = DECOMPOSED_BINARY_POINT - shift + scale;
    p->frac_hi = f << shift;
}

static void partsN(uint_to_float)(FloatPartsN *p, uint64_t a,
                                  int scale, float_status *status)
{
    memset(p, 0, sizeof(*p));

    if (a == 0) {
        p->cls = float_class_zero;
    } else {
        int shift = clz64(a);
        scale = MIN(MAX(scale, -0x10000), 0x10000);
        p->cls = float_class_normal;
        p->exp = DECOMPOSED_BINARY_POINT - shift + scale;
        p->frac_hi = a << shift;
    }
}

static FloatPartsN *partsN(minmax)(FloatPartsN *a, FloatPartsN *b,
                                   float_status *s, int flags)
{
    int ab_mask = float_cmask(a->cls) | float_cmask(b->cls);
    int a_exp, b_exp, cmp;

    if (unlikely(ab_mask & float_cmask_anynan)) {
        if ((flags & (minmax_isnum | minmax_isnumber))
            && !(ab_mask & float_cmask_snan)
            && (ab_mask & ~float_cmask_qnan)) {
            if (ab_mask & float_cmask_denormal) {
                float_raise(float_flag_input_denormal_used, s);
            }
            return is_nan(a->cls) ? b : a;
        }

        if ((flags & minmax_isnumber)
            && (ab_mask & float_cmask_snan)
            && (ab_mask & ~float_cmask_anynan)) {
            float_raise(float_flag_invalid, s);
            return is_nan(a->cls) ? b : a;
        }

        return parts_pick_nan(a, b, s);
    }

    if (ab_mask & float_cmask_denormal) {
        float_raise(float_flag_input_denormal_used, s);
    }

    a_exp = a->exp;
    b_exp = b->exp;

    if (unlikely(!cmask_is_only_normals(ab_mask))) {
        switch (a->cls) {
        case float_class_normal:
        case float_class_denormal:
            break;
        case float_class_inf:
            a_exp = INT16_MAX;
            break;
        case float_class_zero:
            a_exp = INT16_MIN;
            break;
        default:
            g_assert_not_reached();
        }
        switch (b->cls) {
        case float_class_normal:
        case float_class_denormal:
            break;
        case float_class_inf:
            b_exp = INT16_MAX;
            break;
        case float_class_zero:
            b_exp = INT16_MIN;
            break;
        default:
            g_assert_not_reached();
        }
    }

    cmp = a_exp - b_exp;
    if (cmp == 0) {
        cmp = frac_cmp(a, b);
    }

    if (!(flags & minmax_ismag) || cmp == 0) {
        if (a->sign != b->sign) {
            cmp = a->sign ? -1 : 1;
        } else if (a->sign) {
            cmp = -cmp;
        }
    }

    if (flags & minmax_ismin) {
        cmp = -cmp;
    }
    return cmp < 0 ? b : a;
}

static FloatRelation partsN(compare)(FloatPartsN *a, FloatPartsN *b,
                                     float_status *s, bool is_quiet)
{
    int ab_mask = float_cmask(a->cls) | float_cmask(b->cls);

    if (likely(cmask_is_only_normals(ab_mask))) {
        FloatRelation cmp;

        if (ab_mask & float_cmask_denormal) {
            float_raise(float_flag_input_denormal_used, s);
        }

        if (a->sign != b->sign) {
            goto a_sign;
        }
        if (a->exp == b->exp) {
            cmp = frac_cmp(a, b);
        } else if (a->exp < b->exp) {
            cmp = float_relation_less;
        } else {
            cmp = float_relation_greater;
        }
        if (a->sign) {
            cmp = -cmp;
        }
        return cmp;
    }

    if (unlikely(ab_mask & float_cmask_anynan)) {
        if (ab_mask & float_cmask_snan) {
            float_raise(float_flag_invalid | float_flag_invalid_snan, s);
        } else if (!is_quiet) {
            float_raise(float_flag_invalid, s);
        }
        return float_relation_unordered;
    }

    if (ab_mask & float_cmask_denormal) {
        float_raise(float_flag_input_denormal_used, s);
    }

    if (ab_mask & float_cmask_zero) {
        if (ab_mask == float_cmask_zero) {
            return float_relation_equal;
        } else if (a->cls == float_class_zero) {
            goto b_sign;
        } else {
            goto a_sign;
        }
    }

    if (ab_mask == float_cmask_inf) {
        if (a->sign == b->sign) {
            return float_relation_equal;
        }
    } else if (b->cls == float_class_inf) {
        goto b_sign;
    } else {
        g_assert(a->cls == float_class_inf);
    }

 a_sign:
    return a->sign ? float_relation_less : float_relation_greater;
 b_sign:
    return b->sign ? float_relation_greater : float_relation_less;
}

static void partsN(scalbn)(FloatPartsN *a, int n, float_status *s)
{
    switch (a->cls) {
    case float_class_snan:
    case float_class_qnan:
        parts_return_nan(a, s);
        break;
    case float_class_zero:
    case float_class_inf:
        break;
    case float_class_denormal:
        float_raise(float_flag_input_denormal_used, s);
    case float_class_normal:
        a->exp += MIN(MAX(n, -0x10000), 0x10000);
        break;
    default:
        g_assert_not_reached();
    }
}

static void partsN(log2)(FloatPartsN *a, float_status *s, const FloatFmt *fmt)
{
    uint64_t a0, a1, r, t, ign;
    FloatPartsN f;
    int i, n, a_exp, f_exp;

    if (unlikely(a->cls != float_class_normal)) {
        switch (a->cls) {
        case float_class_denormal:
            if (!a->sign) {
                float_raise(float_flag_input_denormal_used, s);
            }
            break;
        case float_class_snan:
        case float_class_qnan:
            parts_return_nan(a, s);
            return;
        case float_class_zero:
            float_raise(float_flag_divbyzero, s);
            a->cls = float_class_inf;
            a->sign = 1;
            return;
        case float_class_inf:
            if (unlikely(a->sign)) {
                goto d_nan;
            }
            return;
        default:
            g_assert_not_reached();
        }
    }
    if (unlikely(a->sign)) {
        goto d_nan;
    }

    g_assert(N == 64);

    a_exp = a->exp;
    f_exp = -1;

    r = 0;
    t = DECOMPOSED_IMPLICIT_BIT;
    a0 = a->frac_hi;
    a1 = 0;

    n = fmt->frac_size + 2;
    if (unlikely(a_exp == -1)) {
        n = fmt->frac_size * 2 + 2;
        n = MIN(n, 62);
    }

    for (i = 0; i < n; i++) {
        if (a1) {
            mul128To256(a0, a1, a0, a1, &a0, &a1, &ign, &ign);
        } else if (a0 & 0xffffffffull) {
            mul64To128(a0, a0, &a0, &a1);
        } else if (a0 & ~DECOMPOSED_IMPLICIT_BIT) {
            a0 >>= 32;
            a0 *= a0;
        } else {
            goto exact;
        }

        if (a0 & DECOMPOSED_IMPLICIT_BIT) {
            if (unlikely(a_exp == 0 && r == 0)) {
                f_exp -= i;
                t = r = DECOMPOSED_IMPLICIT_BIT;
                i = 0;
            } else {
                r |= t;
            }
        } else {
            add128(a0, a1, a0, a1, &a0, &a1);
        }
        t >>= 1;
    }

    r |= (a1 || a0 & ~DECOMPOSED_IMPLICIT_BIT);

 exact:
    parts_sint_to_float(a, a_exp, 0, s);
    if (r == 0) {
        return;
    }

    memset(&f, 0, sizeof(f));
    f.cls = float_class_normal;
    f.frac_hi = r;
    f.exp = f_exp - frac_normalize(&f);

    if (a_exp < 0) {
        parts_sub_normal(a, &f);
    } else if (a_exp > 0) {
        parts_add_normal(a, &f);
    } else {
        *a = f;
    }
    return;

 d_nan:
    float_raise(float_flag_invalid, s);
    parts_default_nan(a, s);
}
