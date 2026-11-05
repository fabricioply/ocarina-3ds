/* RAM log buffer (from probe10): lines are queued here at events and written to the SD only when the game is idle (a single
 * append of everything pending). All-or-nothing append, never overruns. Pure C99, no libc. */
#ifndef LOGBUF_H
#define LOGBUF_H

#define LB_CAP 8192u

typedef struct {
    char     buf[LB_CAP];
    unsigned len;
    unsigned dropped;     /* lines that did not fit */
    unsigned flushes;
} logbuf_t;

void     lb_init(logbuf_t *b);
/* Queue len bytes; returns 1 if queued, 0 (and counts a drop) if it would not fit. */
int      lb_put(logbuf_t *b, const char *s, unsigned len);
unsigned lb_pending(const logbuf_t *b);
/* Marks the pending bytes as written (call after a successful SD append of buf[0..len)); a failed append keeps them but
 * the caller decides (probe10 drops them to avoid a retry storm). */
void     lb_clear(logbuf_t *b);

#endif
