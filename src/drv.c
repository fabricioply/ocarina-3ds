#include "drv.h"
#include "dlglog.h"
#include "catalog.h"

void drv_init(drv_t *v, const dlg_mem *mem, unsigned ctx_addr, drv_loader_fn loader, void *user, logbuf_t *lb)
{
    dlg_init(&v->d);
    v->mem = *mem; v->ctx_addr = ctx_addr; v->loader = loader; v->user = user;
    v->lb = lb; v->song = 0; v->core_saved = 0;
    v->ts.active = 0; v->textlen = 0; v->open_code = 0; v->lines = 0; v->loader_calls = 0;
}

void drv_request(drv_t *v, int song)
{
    v->song = song;
    v->lines = 0;
    v->open_code = 0;
    dlg_request(&v->d);
}

/* queue the line built in v->line */
static void line(drv_t *v, int len)
{
    if (len <= 0 || v->lines >= DRV_MAX_LINES || !v->lb) return;
    v->lines++;
    lb_put(v->lb, v->line, (unsigned)len);
}

static unsigned opt_of(const drv_t *v) { return dm_rd32(v->mem.ctx + DM_OFF_CHOICE); }
static int st_of(const drv_t *v) { return v->mem.ctx[DM_OFF_STATE]; }

/* StartTextbox's 0x343F0C(play, play+0x224C) part: clear two flags unless both gates are zero (guarded, skipped if unsure) */
static void play_gates(drv_t *v)
{
    unsigned play = dm_rd32(v->mem.ctx + DM_OFF_PLAY);
    unsigned char *g;
    if (!play) return;
    g = v->mem.xlat(v->mem.user, play + 0x225Cu, 0x6u, 0);          /* covers +0x225C,+0x225D and +0x2260,+0x2261 */
    if (!g) return;
    if (!(g[1] == 0u && g[4] == 0u && g[5] == 0u)) {
        g = v->mem.xlat(v->mem.user, play + 0x225Cu, 2u, 1);
        if (!g) return;
        g[0] = 0u; g[1] = 0u;
    }
}

static void do_finish(drv_t *v, unsigned now)
{
    int n;
    dm_text_restore(&v->ts);
    if (v->core_saved) { dm_core_restore(v->mem.ctx, v->core); v->core_saved = 0; }
    n = dlglog_line_result(v->line, now, v->d.result, v->d.fail, v->d.chosen, st_of(v));
    line(v, n);
}

static void do_open(drv_t *v, unsigned now)
{
    unsigned char text[DM_TEXT_MAX];
    int tl, n;
    unsigned code = 0, ret = 0, patch = 0, oldsize = 0, lang = 0;

    if (!dm_rd32(v->mem.ctx + DM_OFF_PLAY)) code = 200u;
    else {
        tl = catalog_dialog_text(v->song, text, (int)sizeof text);
        if (tl <= 0) code = 202u;
        else {
            v->textlen = (unsigned)tl;
            patch = (unsigned)dm_text_patch(&v->mem, text, (unsigned)tl, &v->ts);
            oldsize = v->ts.old_size; lang = dm_rd32(v->mem.ctx + DM_OFF_LANG);
            if (patch) code = 100u + patch;
        }
    }
    if (code) {
        n = dlglog_line_open(v->line, now, patch, lang, v->textlen, oldsize, 0u, st_of(v));
        line(v, n);
        v->open_code = code;
        dlg_open_result(&v->d, now, (int)code);
        do_finish(v, now);
        return;
    }
    play_gates(v);
    v->loader_calls++;
    ret = v->loader(v->user, v->ctx_addr, DM_TEXT_ID);
    if (ret == 0u) {
        n = dlglog_line_open(v->line, now, 0u, lang, v->textlen, oldsize, ret, st_of(v));
        line(v, n);
        v->open_code = 201u;
        dlg_open_result(&v->d, now, 201);
        do_finish(v, now);
        return;
    }
    dm_open_writes(v->mem.ctx);
    n = dlglog_line_open(v->line, now, 0u, lang, v->textlen, oldsize, ret, st_of(v));
    line(v, n);
    v->open_code = 0;
    dlg_open_result(&v->d, now, 0);
}

unsigned drv_tick(drv_t *v, unsigned now, unsigned a_edges)
{
    unsigned ev, out = 0;
    int n, st = st_of(v);
    unsigned opt = opt_of(v);

    ev = dlg_step(&v->d, now, st, opt, a_edges);
    if (ev & DLG_EV_DROPPED) out |= DRV_DROPPED;
    if (ev & DLG_EV_A_EDGE) { n = dlglog_line_aedge(v->line, now, st, opt); line(v, n); }
    if ((ev & (DLG_EV_TRANS | DLG_EV_PHASE)) && !(ev & DLG_EV_FINISH)) {
        n = dlglog_line_trans(v->line, now, dlg_phase_name(v->d.phase), st, opt); line(v, n);
    }
    if (ev & DLG_EV_CHOSEN) { n = dlglog_line_chosen(v->line, now, v->d.chosen); line(v, n); }

    switch (v->d.act) {
    case DLG_ACT_CLOSEPREV:
        dm_core_save(v->mem.ctx, v->core);
        v->core_saved = 1;
        dm_close_prev(v->mem.ctx);
        n = dlglog_line_closeprev(v->line, now, st); line(v, n);
        break;
    case DLG_ACT_OPEN:
        do_open(v, now);
        break;
    case DLG_ACT_FORCE_CLOSE:
        n = dlglog_line_force(v->line, now, st); line(v, n);
        dm_force_close(v->mem.ctx);
        break;
    case DLG_ACT_FINISH:
        do_finish(v, now);
        break;
    default:
        break;
    }
    if (v->d.act == DLG_ACT_FINISH || (v->d.act == DLG_ACT_OPEN && v->d.phase == DLG_DONE)) out |= DRV_FINISHED;
    return out;
}
