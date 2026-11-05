#include "ringcopy.h"

int rc_pos(unsigned off, unsigned ring_len, unsigned *pos)
{
    if (ring_len == 0 || off >= ring_len * 2u) return 0;
    *pos = off >> 1;
    return 1;
}

unsigned rc_avail(unsigned last, unsigned pos, unsigned ring_len)
{
    return pos >= last ? pos - last : pos + ring_len - last;
}

unsigned rc_adv(unsigned last, unsigned n, unsigned ring_len)
{
    last += n;
    if (last >= ring_len) last -= ring_len;
    return last;
}

unsigned rc_copy(const short *ring, unsigned ring_len, unsigned last, unsigned n, short *dst,
                 unsigned *peak, unsigned *clip)
{
    unsigned i, p = last, pk = *peak, cl = *clip;
    for (i = 0; i < n; i++) {
        int s = ring[p];
        unsigned a = s < 0 ? (unsigned)(-s) : (unsigned)s;
        if (a > pk) pk = a;
        if (a >= RC_CLIP_LEVEL) cl++;
        dst[i] = (short)s;
        if (++p == ring_len) p = 0;
    }
    *peak = pk; *clip = cl;
    return p;
}
