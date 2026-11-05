#include "scan.h"

int scan_find(const unsigned char* image, unsigned size, unsigned base,
              const unsigned char* bytes, const unsigned char* mask, unsigned len,
              unsigned* found_addr)
{
    unsigned i, k, last, count = 0, first = 0;
    if (len == 0 || size < len) return 0;
    last = size - len;
    for (i = 0; i <= last; i++) {
        const unsigned char* p = image + i;
        for (k = 0; k < len; k++) {
            if ((unsigned char)(p[k] & mask[k]) != (unsigned char)(bytes[k] & mask[k])) break;
        }
        if (k == len) {
            if (count == 0) first = i;
            if (++count >= 2) return 2;
        }
    }
    if (count == 1) { *found_addr = base + first; return 1; }
    return 0;
}
