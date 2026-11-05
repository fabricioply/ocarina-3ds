#include "hook.h"

unsigned hook_rd32(const unsigned char *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

void hook_wr32(unsigned char *p, unsigned v)
{
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
}

int hook_prologue_ok(unsigned w0, unsigned w1)
{
    return (w0 & HOOK_W0_MASK) == HOOK_W0_VAL && w1 == HOOK_W1_VAL;
}

unsigned hook_literal_addr(unsigned func, unsigned w0)
{
    return func + 8u + (w0 & 0xFFFu);
}

void hook_make_patch(unsigned tramp, unsigned patch[2])
{
    patch[0] = HOOK_LDR_PC_M4;
    patch[1] = tramp;
}

int hook_plan_make(const unsigned char *img, unsigned size, unsigned base,
                   unsigned func, unsigned tramp, hook_plan *out)
{
    unsigned off, loff;
    if (!img || !out || (func & 3u) || (tramp & 3u)) return HOOK_ERR_ARGS;
    if (func < base || func - base > size || size - (func - base) < 8u) return HOOK_ERR_RANGE;
    off = func - base;
    out->w0 = hook_rd32(img + off);
    out->w1 = hook_rd32(img + off + 4);
    if (!hook_prologue_ok(out->w0, out->w1)) return HOOK_ERR_PROLOGUE;
    out->literal_addr = hook_literal_addr(func, out->w0);
    if (out->literal_addr < base || out->literal_addr - base > size ||
        size - (out->literal_addr - base) < 4u) return HOOK_ERR_RANGE;
    loff = out->literal_addr - base;
    out->literal = hook_rd32(img + loff);
    out->resume = func + 8u;
    hook_make_patch(tramp, out->patch);
    return HOOK_OK;
}

int hook_apply_to_copy(const unsigned char *img, unsigned size, unsigned base,
                       unsigned func, unsigned tramp, unsigned char *copy, hook_plan *out)
{
    int r = hook_plan_make(img, size, base, func, tramp, out);
    if (r != HOOK_OK) return r;
    hook_wr32(copy + (func - base), out->patch[0]);
    hook_wr32(copy + (func - base) + 4, out->patch[1]);
    return HOOK_OK;
}

int hook_mid_plan_make(const unsigned char *img, unsigned size, unsigned base, unsigned site, unsigned tramp,
                       unsigned exp0, unsigned exp1, hook_mid_plan *out)
{
    unsigned off;
    if (!img || !out || (site & 3u) || (tramp & 3u)) return HOOK_ERR_ARGS;
    if (site < base || site - base > size || size - (site - base) < 8u) return HOOK_ERR_RANGE;
    off = site - base;
    out->w0 = hook_rd32(img + off);
    out->w1 = hook_rd32(img + off + 4);
    if (out->w0 != exp0 || out->w1 != exp1) return HOOK_ERR_PROLOGUE;
    out->resume = site + 8u;
    hook_make_patch(tramp, out->patch);
    return HOOK_OK;
}

unsigned hook_branch_refs(const unsigned char *img, unsigned size, unsigned base, unsigned target)
{
    unsigned off, n = 0;
    for (off = 0; off + 4u <= size; off += 4u) {
        unsigned w = hook_rd32(img + off);
        unsigned cond = w >> 28;
        int imm;
        if ((w & 0x0E000000u) != 0x0A000000u || cond == 0xFu) continue;     /* B / BL, not BLX imm */
        imm = (int)((w & 0x00FFFFFFu) << 8) >> 6;                           /* sign-extended imm24 * 4 */
        if (base + off + 8u + (unsigned)imm == target) n++;
    }
    return n;
}

unsigned hook_ticks_to_us(unsigned ticks)
{
    return (unsigned)(((unsigned long long)ticks * 16019311ull) >> 32);
}

unsigned long long hook_div64(unsigned long long total, unsigned n)
{
    unsigned long long q = 0, r = 0;
    int i;
    if (n == 0) return 0;
    for (i = 63; i >= 0; i--) {
        r = (r << 1) | ((total >> i) & 1u);
        if (r >= n) { r -= n; q |= 1ull << i; }
    }
    return q;
}

static char *put_str(char *p, const char *s) { while (*s) *p++ = *s++; return p; }

static char *put_hexw(char *p, unsigned v, int digits)
{
    int i;
    for (i = digits - 1; i >= 0; i--) {
        unsigned d = (v >> (i * 4)) & 0xFu;
        *p++ = (char)(d < 10 ? '0' + d : 'A' + d - 10);
    }
    return p;
}

static char *put_dec(char *p, unsigned v)   /* no division */
{
    static const unsigned pw[9] = {100000000u, 10000000u, 1000000u, 100000u, 10000u, 1000u, 100u, 10u, 1u};
    int i, started = 0;
    for (i = 0; i < 9; i++) {
        unsigned d = 0;
        while (v >= pw[i]) { v -= pw[i]; d++; }
        if (d || started || i == 8) { started = 1; *p++ = (char)('0' + d); }
    }
    return p;
}

int hook_format_report(char *out, unsigned n, unsigned st, unsigned gs,
                       unsigned avg_us, unsigned max_us, unsigned io)
{
    char *p = out;
    p = put_str(p, "n:");   p = put_hexw(p, n, 4);
    p = put_str(p, " st:"); p = put_hexw(p, st, 4);
    p = put_str(p, " gs:"); p = put_hexw(p, gs, 4);
    p = put_str(p, " avg:"); p = put_dec(p, avg_us); p = put_str(p, "us");
    p = put_str(p, " max:"); p = put_dec(p, max_us); p = put_str(p, "us");
    p = put_str(p, " io:"); p = put_hexw(p, io, 8);
    *p = 0;
    return (int)(p - out);
}
