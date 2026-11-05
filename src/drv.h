/* Native dialog driver for ocarina.3gx: glues dlg.c (when) + dlgmem.c (what to write) + logbuf/dlglog (what to say) +
 * the game loader call (a callback). Same flow as the console-proven native-dialog probe driver (docs/HISTORY.md) except that:
 *  - a dialog starts only on drv_request(song) (the song asked is a parameter, not fixed);
 *  - the result is left in v->d.result for the caller (no injection coupling here);
 *  - no SD flush before the loader (the log is only written while the ocarina is closed).
 * Pure C99, no libc; every side effect goes through dlg_mem / the callback, so the whole flow runs on the host against a
 * fake message context. One drv_tick per game frame. */
#ifndef DRV_H
#define DRV_H

#include "dlg.h"
#include "dlgmem.h"
#include "logbuf.h"

/* Calls the game's message loader: loader(msgCtx, textId); returns its r0 (non-zero = success). */
typedef unsigned (*drv_loader_fn)(void *user, unsigned ctx_addr, unsigned text_id);

/* drv_tick return bits */
#define DRV_FINISHED   0x01u    /* the dialog just finished: v->d.result / v->d.fail / v->open_code are valid */
#define DRV_DROPPED    0x02u    /* the request was dropped (the ocarina was not open any more) */

#define DRV_MAX_LINES  16u      /* log lines per dialog */

/* dialog disabled reasons (log "dlg=OFF why=") */
#define DRV_WHY_CTX      1u     /* msgCtx not mapped read/write */
#define DRV_WHY_LOADER   2u     /* message loader signature not found */

typedef struct {
    dlg_t          d;
    dlg_mem        mem;
    unsigned       ctx_addr;
    drv_loader_fn  loader;
    void          *user;
    logbuf_t      *lb;
    int            song;
    int            core_saved;
    unsigned char  core[DM_CORE_BYTES];
    dlg_textsave   ts;
    unsigned       textlen;
    unsigned       open_code;      /* last open failure code (100+patch error, 200.., 0) */
    unsigned       lines;          /* dialog log lines queued in the current dialog */
    unsigned       loader_calls;
    char           line[200];      /* scratch for one log line */
} drv_t;

void     drv_init(drv_t *v, const dlg_mem *mem, unsigned ctx_addr, drv_loader_fn loader, void *user, logbuf_t *lb);
/* Ask "<song>?" (starts at the next drv_tick if the state is still 6). */
void     drv_request(drv_t *v, int song);
unsigned drv_tick(drv_t *v, unsigned now_ms, unsigned a_edges);

#endif
