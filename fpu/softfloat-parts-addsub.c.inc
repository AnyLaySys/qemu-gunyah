
static void partsN(add_normal)(FloatPartsN *a, FloatPartsN *b)
{
    int exp_diff = a->exp - b->exp;

    if (exp_diff > 0) {
        frac_shrjam(b, exp_diff);
    } else if (exp_diff < 0) {
        frac_shrjam(a, -exp_diff);
        a->exp = b->exp;
    }

    if (frac_add(a, a, b)) {
        frac_shrjam(a, 1);
        a->frac_hi |= DECOMPOSED_IMPLICIT_BIT;
        a->exp += 1;
    }
}

static bool partsN(sub_normal)(FloatPartsN *a, FloatPartsN *b)
{
    int exp_diff = a->exp - b->exp;
    int shift;

    if (exp_diff > 0) {
        frac_shrjam(b, exp_diff);
        frac_sub(a, a, b);
    } else if (exp_diff < 0) {
        a->exp = b->exp;
        a->sign ^= 1;
        frac_shrjam(a, -exp_diff);
        frac_sub(a, b, a);
    } else if (frac_sub(a, a, b)) {
        frac_neg(a);
        a->sign ^= 1;
    }

    shift = frac_normalize(a);
    if (likely(shift < N)) {
        a->exp -= shift;
	return true;
    }
    a->cls = float_class_zero;
    return false;
}
