/* Mic ring-buffer copy logic. Pure C99, no libc, no division. Host-testable.
 * The mic shared block holds a ring of ring_len signed 16-bit samples; the last 4 bytes of the block hold a
 * byte offset counter. SEMANTICS USED (conservative, correct for both "offset of newest sample" and
 * "offset of next sample to write"): the sample at index P = off>>1 may still be in flight, so only the
 * samples [last, P) are considered complete. After a copy last = P. */
#ifndef RINGCOPY_H
#define RINGCOPY_H
#define RC_CLIP_LEVEL 32000u

/* off: counter in bytes. Returns 1 and *pos = off>>1 if off < 2*ring_len, else 0 (invalid, ignore the counter). */
int rc_pos(unsigned off, unsigned ring_len, unsigned *pos);
/* Number of complete samples between last and pos going forward: (pos - last) mod ring_len. In [0, ring_len). */
unsigned rc_avail(unsigned last, unsigned pos, unsigned ring_len);
/* (last + n) mod ring_len, n < ring_len. */
unsigned rc_adv(unsigned last, unsigned n, unsigned ring_len);
/* Copies n samples starting at ring[last] (wrapping) to dst; updates *peak (max |s|) and *clip (|s| >= RC_CLIP_LEVEL).
 * Returns the new last index. */
unsigned rc_copy(const short *ring, unsigned ring_len, unsigned last, unsigned n, short *dst,
                 unsigned *peak, unsigned *clip);
#endif
