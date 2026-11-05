#ifndef SCAN_H
#define SCAN_H
/* Freestanding masked signature scanner (C99, no libc, no allocation).
 * A signature matches at i if (image[i+k] & mask[k]) == (bytes[k] & mask[k]) for all k.
 * Returns 0 = missing, 1 = found (*found_addr = base + offset), 2 = ambiguous.
 * *found_addr is written only on 1. */
int scan_find(const unsigned char* image, unsigned size, unsigned base,
              const unsigned char* bytes, const unsigned char* mask, unsigned len,
              unsigned* found_addr);
#endif
