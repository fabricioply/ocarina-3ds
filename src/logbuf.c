#include "logbuf.h"

void lb_init(logbuf_t *b) { b->len = 0; b->dropped = 0; b->flushes = 0; b->buf[0] = 0; }

int lb_put(logbuf_t *b, const char *s, unsigned len)
{
    unsigned i;
    if (len == 0) return 1;
    if (len > LB_CAP - b->len - 1u) { b->dropped++; return 0; }
    for (i = 0; i < len; i++) b->buf[b->len + i] = s[i];
    b->len += len;
    b->buf[b->len] = 0;
    return 1;
}

unsigned lb_pending(const logbuf_t *b) { return b->len; }

void lb_clear(logbuf_t *b) { b->len = 0; b->buf[0] = 0; b->flushes++; }
