#include "freerange.h"

int fr_find(fr_query_fn q, void *ctx, unsigned lo, unsigned hi, unsigned need, int want_last, unsigned *out)
{
    unsigned addr = lo;
    int found = 0, iter;
    for (iter = 0; iter < 256 && addr < hi; iter++) {
        unsigned base = 0, size = 0, state = 0, end, s, e;
        if (q(ctx, addr, &base, &size, &state) < 0) return found ? 1 : (iter == 0 ? -1 : 0);
        if (size == 0) break;
        end = base + size;
        if (end < base) end = 0xFFFFF000u;
        if (state == FR_STATE_FREE) {
            s = base < lo ? lo : base;
            e = end > hi ? hi : end;
            s = (s + 0xFFFu) & ~0xFFFu;
            e &= ~0xFFFu;
            if (e > s && e - s >= need) {
                *out = want_last ? e - need : s;
                found = 1;
                if (!want_last) return 1;
            }
        }
        if (end <= addr) break;
        addr = end;
    }
    return found;
}
