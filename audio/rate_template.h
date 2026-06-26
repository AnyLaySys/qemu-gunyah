
void NAME (void *opaque, struct st_sample *ibuf, struct st_sample *obuf,
           size_t *isamp, size_t *osamp)
{
    struct rate *rate = opaque;
    struct st_sample *istart, *iend;
    struct st_sample *ostart, *oend;
    struct st_sample ilast, icur, out;
#ifdef FLOAT_MIXENG
    mixeng_real t;
#else
    int64_t t;
#endif

    istart = ibuf;
    iend = ibuf + *isamp;

    ostart = obuf;
    oend = obuf + *osamp;

    if (rate->opos_inc == (1ULL + UINT_MAX)) {
        int i, n = *isamp > *osamp ? *osamp : *isamp;
        for (i = 0; i < n; i++) {
            OP (obuf[i].l, ibuf[i].l);
            OP (obuf[i].r, ibuf[i].r);
        }
        *isamp = n;
        *osamp = n;
        return;
    }

    if (ibuf >= iend) {
        *osamp = 0;
        return;
    }

    ilast = rate->ilast;

    while (true) {

        while (rate->ipos <= (rate->opos >> 32)) {
            ilast = *ibuf++;
            rate->ipos++;

            if (ibuf >= iend) {
                goto the_end;
            }
        }

        if (obuf >= oend) {
            break;
        }

        icur = *ibuf;

        if (rate->ipos >= 0x10001) {
            rate->ipos = 1;
            rate->opos &= 0xffffffff;
        }

#ifdef FLOAT_MIXENG
#ifdef RECIPROCAL
        t = (rate->opos & UINT_MAX) * (1.f / UINT_MAX);
#else
        t = (rate->opos & UINT_MAX) / (mixeng_real) UINT_MAX;
#endif
        out.l = (ilast.l * (1.0 - t)) + icur.l * t;
        out.r = (ilast.r * (1.0 - t)) + icur.r * t;
#else
        t = rate->opos & 0xffffffff;
        out.l = (ilast.l * ((int64_t) UINT_MAX - t) + icur.l * t) >> 32;
        out.r = (ilast.r * ((int64_t) UINT_MAX - t) + icur.r * t) >> 32;
#endif

        OP (obuf->l, out.l);
        OP (obuf->r, out.r);
        obuf += 1;
        rate->opos += rate->opos_inc;
    }

the_end:
    *isamp = ibuf - istart;
    *osamp = obuf - ostart;
    rate->ilast = ilast;
}

#undef NAME
#undef OP
