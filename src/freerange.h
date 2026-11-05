/* Free address-range finder over an abstract memory query (svcQueryMemory on the console, a fake on the host).
 * Pure C99, no libc. */
#ifndef FREERANGE_H
#define FREERANGE_H
#define FR_STATE_FREE 0u   /* MEMSTATE_FREE */

/* Describes the region containing addr: base, size and state. Returns < 0 on error. */
typedef int (*fr_query_fn)(void *ctx, unsigned addr, unsigned *base, unsigned *size, unsigned *state);

/* Walks [lo, hi) region by region. Finds a FREE page-aligned range of at least `need` bytes inside [lo,hi).
 * want_last=0: start of the first such range.  want_last=1: the highest address (end of the LAST such range
 * minus need). Returns 1 and *out on success, 0 if none, -1 if the very first query failed. */
int fr_find(fr_query_fn q, void *ctx, unsigned lo, unsigned hi, unsigned need, int want_last, unsigned *out);
#endif
