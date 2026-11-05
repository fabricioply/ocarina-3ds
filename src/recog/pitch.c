#include "pitch.h"

/* 32-bit / 32-bit shift-subtract division (no libgcc). d must be non-zero. */
static unsigned udiv32(unsigned n, unsigned d)
{
    unsigned q = 0, r = 0;
    int i;
    for (i = 31; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1u);
        if (r >= d) { r -= d; q |= 1u << i; }
    }
    return q;
}

static unsigned isqrt32(unsigned v)
{
    unsigned r = 0, b = 1u << 30;
    while (b > v) b >>= 2;
    while (b) {
        if (v >= r + b) { v -= r + b; r = (r >> 1) + b; } else r >>= 1;
        b >>= 2;
    }
    return r;
}

/* 2r/m in Q15, clamped to [0, 32768]; r <= 0 -> 0 */
static int nsdf_q15(int r, unsigned m)
{
    unsigned rr, mm;
    int s = 0, n;
    if (r <= 0 || m == 0) return 0;
    while ((m >> s) >= 65536u) s++;
    rr = (unsigned)r >> s; mm = m >> s;
    if (mm == 0) return 0;
    n = (int)udiv32((2u * rr) << 15, mm);   /* 2*rr < 2^17, <<15 < 2^32 */
    return n > 32768 ? 32768 : n;
}

#define NLAGS (PITCH_TAU_MAX + 1 - (PITCH_TAU_MIN - 1) + 1)   /* lags TAU_MIN-1 .. TAU_MAX+1 */

int pitch_detect(short *x, pitch_result *res, int min_rms)
{
    int nsdf[NLAGS];
    unsigned etab[NLAGS];
    int i, t, sum = 0, mean, peak = 0, best = -1, nmax = 0, count = NLAGS, sh = 0;
    long long sq = 0;
    unsigned e0 = 0, et = 0;

    res->peak = 0; res->rms = 0; res->ran = 0; res->clarity = 0; res->hz = 0;

    for (i = 0; i < PITCH_N; i++) sum += x[i];
    mean = sum >> PITCH_LOG2N;
    for (i = 0; i < PITCH_N; i++) {
        int v = x[i] - mean, a;
        if (v > 32767) v = 32767;
        if (v < -32767) v = -32767;
        x[i] = (short)v;
        a = v < 0 ? -v : v;
        if (a > peak) peak = a;
        sq += (long long)v * v;
    }
    res->peak = peak;
    res->rms = (int)isqrt32((unsigned)(sq >> PITCH_LOG2N));
    if (min_rms < PITCH_ABS_MIN_RMS) min_rms = PITCH_ABS_MIN_RMS;
    if (res->rms < min_rms) return 0;
    res->ran = 1;

    /* scale so that |sample| < 1024: all products < 2^20, sums over <= 1024 terms < 2^30 -> 32-bit MACs */
    while ((peak >> sh) >= 1024) sh++;
    if (sh) for (i = 0; i < PITCH_N; i++) x[i] = (short)(x[i] >> sh);

    for (i = 0; i < PITCH_W; i++) {
        int a = x[i], b = x[i + PITCH_TAU_MIN - 1];
        e0 += (unsigned)(a * a);
        et += (unsigned)(b * b);
    }
    if (e0 == 0) return 0;

    /* energy of the shifted window for every lag (slides by one sample per lag) */
    for (t = PITCH_TAU_MIN - 1; t <= PITCH_TAU_MAX + 1; t++) {
        etab[t - (PITCH_TAU_MIN - 1)] = et;
        if (t < PITCH_TAU_MAX + 1) {
            int a = x[t], b = x[t + PITCH_W];
            et += (unsigned)(b * b) - (unsigned)(a * a);
        }
    }

    /* cross-correlations, two adjacent lags per pass (one load of the shifted signal per MAC) */
    for (t = PITCH_TAU_MIN - 1; t <= PITCH_TAU_MAX + 1; t += 2) {
        const short *p = x, *q = x + t;
        int r0 = 0, r1 = 0, two = (t + 1 <= PITCH_TAU_MAX + 1);
        int cur = q[0];
        if (two) {
            for (i = 0; i < PITCH_W; i++) {
                int pi = p[i], nxt = q[i + 1];
                r0 += pi * cur;
                r1 += pi * nxt;
                cur = nxt;
            }
        } else {
            for (i = 0; i < PITCH_W; i++) r0 += (int)p[i] * q[i];
        }
        nsdf[t - (PITCH_TAU_MIN - 1)] = nsdf_q15(r0, e0 + etab[t - (PITCH_TAU_MIN - 1)]);
        if (two) nsdf[t + 1 - (PITCH_TAU_MIN - 1)] = nsdf_q15(r1, e0 + etab[t + 1 - (PITCH_TAU_MIN - 1)]);
    }

    for (i = 1; i < count - 1; i++) {
        int v = nsdf[i];
        if (v > nsdf[i - 1] && v >= nsdf[i + 1] && v > nmax) nmax = v;
    }
    if (nmax < PITCH_VOICED_Q15) { res->clarity = nmax; return 0; }
    for (i = 1; i < count - 1; i++) {
        int v = nsdf[i];
        if (v > nsdf[i - 1] && v >= nsdf[i + 1] && v * 32 >= nmax * PITCH_PICK_NUM) { best = i; break; }
    }
    if (best < 0) { res->clarity = nmax; return 0; }
    res->clarity = nsdf[best];

    {
        int a = nsdf[best - 1], b = nsdf[best], c = nsdf[best + 1];
        int den = 2 * b - a - c;
        int num = (a - c) * 128, d = 0;
        unsigned tau_q8, f;
        if (den > 0) {
            int neg = num < 0;
            unsigned mag = udiv32((unsigned)(neg ? -num : num), (unsigned)den);
            d = neg ? (int)mag : -(int)mag;
            if (d > 128) d = 128;
            if (d < -128) d = -128;
        }
        tau_q8 = (unsigned)((best + PITCH_TAU_MIN - 1) * 256 + d);
        if (tau_q8 == 0) return 0;
        f = udiv32((unsigned)PITCH_RATE * 256u + (tau_q8 >> 1), tau_q8);
        res->hz = (int)f;
    }
    return res->hz;
}
