#include "integ.h"

#define ST_OFF   0xF38u
#define ST_OPEN  6
#define RMASK    (IG_RING - 1u)

/* ---------------- small formatting helpers (no libc, no division by variables) ---------------- */
char *ig_put_str(char *p, const char *s) { while (*s) *p++ = *s++; return p; }

char *ig_put_dec(char *p, unsigned v)
{
    static const unsigned pw[10] = {1000000000u, 100000000u, 10000000u, 1000000u, 100000u, 10000u, 1000u, 100u, 10u, 1u};
    int i, started = 0;
    for (i = 0; i < 10; i++) {
        unsigned d = 0;
        while (v >= pw[i]) { v -= pw[i]; d++; }
        if (d || started || i == 9) { started = 1; *p++ = (char)('0' + d); }
    }
    return p;
}

char *ig_put_int(char *p, int v)
{
    if (v < 0) { *p++ = '-'; return ig_put_dec(p, (unsigned)(-(long)v)); }
    return ig_put_dec(p, (unsigned)v);
}

char *ig_put_hex(char *p, unsigned v, int digits)
{
    int i;
    for (i = digits - 1; i >= 0; i--) {
        unsigned d = (v >> (i * 4)) & 0xFu;
        *p++ = (char)(d < 10 ? '0' + d : 'A' + d - 10);
    }
    return p;
}

int ig_fin(char *out, char *p)
{
    *p++ = '\r'; *p++ = '\n'; *p = 0;
    return (int)(p - out);
}

unsigned ig_udiv(unsigned n, unsigned d)
{
    unsigned q = 0, r = 0;
    int i;
    if (!d) return 0;
    for (i = 31; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1u);
        if (r >= d) { r -= d; q |= 1u << i; }
    }
    return q;
}

static void q(ig_t *g, char *end) { if (g->lb) lb_put(g->lb, g->line, (unsigned)ig_fin(g->line, end)); }

static char *put_t(ig_t *g, const char *what, unsigned now)
{
    char *p = ig_put_str(g->line, what);
    p = ig_put_str(p, " t=");
    return ig_put_dec(p, now);
}

static char *put_song(char *p, int song)
{
    const catalog_song *c = catalog_get(song);
    p = ig_put_dec(p, (unsigned)(song < 0 ? 99 : song));
    p = ig_put_str(p, " '");
    p = ig_put_str(p, c ? c->name : "?");
    return ig_put_str(p, "'");
}

static int state_of(const ig_t *g) { return g->ctx[ST_OFF]; }
static unsigned hop_ms(int hop) { return ((unsigned)hop * 125u) >> 1; }   /* 512 samples = 62.5 ms */

/* ---------------- init / samples ---------------- */
void ig_init(ig_t *g, unsigned char *ctx, int dialogs, const dlg_mem *mem, unsigned ctx_addr, drv_loader_fn loader,
             void *user, logbuf_t *lb)
{
    unsigned char *p = (unsigned char *)g;
    unsigned i;
    for (i = 0; i < sizeof *g; i++) p[i] = 0;
    g->ctx = ctx;
    g->lb = lb;
    g->dialogs = dialogs ? 1 : 0;
    if (g->dialogs) drv_init(&g->drv, mem, ctx_addr, loader, user, lb);
    else dlg_init(&g->drv.d);
    inj_init(&g->inj, SONG_ZELDA);
    inj_set_manual(&g->inj, 1);
    matcher_init(&g->m, 1);
    notes_init(&g->ns);
    g->last_st = -1;
    g->restart = 1;
    g->play_song = -1;
}

void ig_push(ig_t *g, const short *ring, unsigned ring_len, unsigned first, unsigned n)
{
    unsigned i, p = first, w = g->wpos, pk = g->peak_now;
    if (g->panic || !ring_len) return;
    if (n > ring_len) n = ring_len;
    for (i = 0; i < n; i++) {
        int s = ring[p];
        unsigned a = s < 0 ? (unsigned)(-s) : (unsigned)s;
        if (a > pk) pk = a;
        g->ring[w & RMASK] = (short)s;
        w++;
        if (++p >= ring_len) p = 0;
    }
    g->wpos = w;
    g->peak_now = pk;
}

/* ---------------- segmenter / phrase ---------------- */
static void seg_reset(notes_state *s)      /* restart the segmenter but keep the noise-floor history */
{
    s->hop = 0; s->active = 0; s->center = 0; s->midi = 0; s->start_hop = 0; s->last_voiced = 0; s->miss = 0;
    s->cand_n = 0; s->cand_c = 0; s->cand_start = 0; s->cand_hz = 0;
}

static void do_restart(ig_t *g)
{
    seg_reset(&g->ns);
    matcher_reset(&g->m);
    g->pn = 0;
    g->apos = g->wpos >= PITCH_N ? g->wpos - PITCH_N : 0u;
    g->last_pr.peak = g->last_pr.rms = g->last_pr.ran = g->last_pr.clarity = g->last_pr.hz = 0;
    g->restart = 0;
}

static void clear_queue(ig_t *g) { g->ncand = 0; g->icand = 0; g->ask_pending = 0; }

/* "phrase t=.. n=6 floor=60 c=7726/500@0 8274/312@562 ... -> SONG 0 'Zelda's Lullaby' sc=912" */
static void log_phrase(ig_t *g, unsigned now, const matcher_result *r, const char *closed)
{
    char *p = put_t(g, "phrase", now);
    int i;
    p = ig_put_str(p, " n="); p = ig_put_dec(p, (unsigned)g->pn);
    p = ig_put_str(p, " floor="); p = ig_put_dec(p, (unsigned)g->ns.floor_rms);
    p = ig_put_str(p, " c/ms@ms=");
    for (i = 0; i < g->pn; i++) {
        *p++ = ' ';
        p = ig_put_int(p, g->pc[i]); *p++ = '/';
        p = ig_put_dec(p, (unsigned)g->pdur[i]); *p++ = '@';
        p = ig_put_dec(p, (unsigned)(g->pstart[i] - g->pstart[0]));
    }
    p = ig_put_str(p, " -> ");
    if (closed) p = ig_put_str(p, closed);
    else if (r->state == MATCH_SONG) { p = ig_put_str(p, "SONG "); p = put_song(p, r->song); }
    else if (r->state == MATCH_AMBIGUOUS) {
        p = ig_put_str(p, "AMBIG");
        for (i = 0; i < r->count && i < MATCHER_MAX_CAND; i++) { *p++ = ' '; p = put_song(p, r->candidates[i]); }
    } else p = ig_put_str(p, "TIMEOUT");
    if (!closed && r->state != MATCH_TIMEOUT) { p = ig_put_str(p, " sc="); p = ig_put_dec(p, (unsigned)r->score); }
    q(g, p);
}

static void start_play(ig_t *g, int song, unsigned now, const char *via)
{
    char *p;
    inj_play(&g->inj, song);
    g->play_song = song;
    g->st.plays++;
    p = put_t(g, "play", now);
    p = ig_put_str(p, " song="); p = put_song(p, song);
    p = ig_put_str(p, " via="); p = ig_put_str(p, via);
    q(g, p);
}

static void ask(ig_t *g, unsigned now)
{
    char *p;
    drv_request(&g->drv, g->cand[g->icand]);
    g->st.asked++;
    p = put_t(g, "ask", now);
    p = ig_put_str(p, " song="); p = put_song(p, g->cand[g->icand]);
    p = ig_put_str(p, " ("); p = ig_put_dec(p, (unsigned)g->icand + 1u); *p++ = '/';
    p = ig_put_dec(p, (unsigned)g->ncand); *p++ = ')';
    q(g, p);
}

static void handle_result(ig_t *g, const matcher_result *r, unsigned now)
{
    int i;
    log_phrase(g, now, r, 0);
    g->st.phrases++;
    if (r->state == MATCH_SONG && catalog_get(r->song)) {
        g->st.songs++;
        start_play(g, r->song, now, "SONG");
    } else if (r->state == MATCH_AMBIGUOUS && r->count > 0) {
        g->st.ambig++;
        if (g->dialogs && !g->dlg_blocked) {
            g->ncand = 0;
            for (i = 0; i < r->count && i < MATCHER_MAX_CAND; i++)
                if (catalog_get(r->candidates[i])) g->cand[g->ncand++] = r->candidates[i];
            g->icand = 0;
            if (g->ncand) ask(g, now);
        } else {
            char *p = put_t(g, "ask skipped", now);
            p = ig_put_str(p, g->dialogs ? " why=dialog_failed_this_session" : " why=no_dialog");
            q(g, p);
        }
    } else g->st.timeouts++;
    matcher_reset(&g->m);
    g->pn = 0;
    if (r->state != MATCH_TIMEOUT) { g->restart = 1; g->mute_until = now + IG_MUTE_AFTER_MS; }
}

/* one hop through segmenter + matcher; returns 1 if a verdict asked for a restart */
static int feed(ig_t *g, const pitch_result *pr, unsigned now)
{
    note_event ev[2];
    matcher_result res;
    int n, k;
    n = notes_feed(&g->ns, pr, ev);
    g->st.hops++;
    if (pr->ran) g->st.ran++;
    if (pr->hz > 0) g->st.voiced++;
    for (k = 0; k < n; k++) {
        if (ev[k].type == NOTE_OFF) {
            matcher_note_ev me;
            me.cents = ev[k].cents;
            me.start_ms = (int)hop_ms(ev[k].start_hop);
            me.end_ms = (int)hop_ms(ev[k].end_hop);
            g->st.notes++;
            if (g->pn < IG_MAX_PHRASE) {
                g->pc[g->pn] = me.cents; g->pstart[g->pn] = me.start_ms; g->pdur[g->pn] = me.end_ms - me.start_ms;
                g->pn++;
            }
            matcher_note(&g->m, &me);
        }
    }
    if (matcher_poll(&g->m, (int)hop_ms(g->ns.hop), g->ns.active, &res)) {
        handle_result(g, &res, now);
        return g->restart;
    }
    return 0;
}

/* at most one pitch analysis; returns 1 if pitch_detect ran */
static int analyse(ig_t *g, unsigned now)
{
    unsigned avail = g->wpos - g->apos, lag, i;
    pitch_result pr;
    if (avail > IG_RING) {                       /* the window was overwritten (long stall): restart */
        char *p;
        g->st.overflows++;
        p = put_t(g, "overflow", now);
        p = ig_put_str(p, " behind="); p = ig_put_dec(p, avail);
        q(g, p);
        do_restart(g);
        if (g->st.overflows == 5u) ig_anomaly(g, now, IG_AN_OVERFLOW, g->st.overflows);
        return 0;
    }
    if (avail < PITCH_N) return 0;
    lag = (avail - PITCH_N) >> 9;
    while (lag > IG_MAX_LAG_HOPS) {             /* behind: repeat the previous hop's result (cheap) */
        g->apos += IG_HOP; lag--; g->st.skipped++;
        if (feed(g, &g->last_pr, now)) return 0;
    }
    for (i = 0; i < PITCH_N; i++) g->work[i] = g->ring[(g->apos + i) & RMASK];
    g->apos += IG_HOP;
    pitch_detect(g->work, &pr, notes_gate(&g->ns));
    g->last_pr = pr;
    feed(g, &pr, now);
    return 1;
}

/* ---------------- sessions ---------------- */
static void session_open(ig_t *g, unsigned now)
{
    unsigned char *p = (unsigned char *)&g->st;
    unsigned i;
    char *l;
    for (i = 0; i < sizeof g->st; i++) p[i] = 0;
    g->st.open_ms = now;
    g->sess++;
    g->open = 1;
    g->restart = 1;
    g->dlg_blocked = 0;
    g->peak_now = 0;
    clear_queue(g);
    l = ig_put_str(g->line, "sess#"); l = ig_put_dec(l, g->sess);
    l = ig_put_str(l, " open t="); l = ig_put_dec(l, now);
    l = ig_put_str(l, " floor="); l = ig_put_dec(l, (unsigned)g->ns.floor_rms);
    q(g, l);
}

static void session_close(ig_t *g, unsigned now)
{
    char *p;
    ig_stats *s = &g->st;
    if (g->pn > 0) log_phrase(g, now, 0, "CLOSED");
    p = ig_put_str(g->line, "sess#"); p = ig_put_dec(p, g->sess);
    p = ig_put_str(p, " close t="); p = ig_put_dec(p, now);
    p = ig_put_str(p, " dur="); p = ig_put_dec(p, now - s->open_ms);
    p = ig_put_str(p, " frames="); p = ig_put_dec(p, s->frames);
    p = ig_put_str(p, " hops="); p = ig_put_dec(p, s->hops);
    p = ig_put_str(p, " ran="); p = ig_put_dec(p, s->ran);
    p = ig_put_str(p, " voiced="); p = ig_put_dec(p, s->voiced);
    p = ig_put_str(p, " notes="); p = ig_put_dec(p, s->notes);
    p = ig_put_str(p, " phr="); p = ig_put_dec(p, s->phrases);
    p = ig_put_str(p, " song="); p = ig_put_dec(p, s->songs);
    p = ig_put_str(p, " amb="); p = ig_put_dec(p, s->ambig);
    p = ig_put_str(p, " to="); p = ig_put_dec(p, s->timeouts);
    p = ig_put_str(p, " asked="); p = ig_put_dec(p, s->asked);
    p = ig_put_str(p, " yes="); p = ig_put_dec(p, s->yes);
    p = ig_put_str(p, " no="); p = ig_put_dec(p, s->no);
    p = ig_put_str(p, " play="); p = ig_put_dec(p, s->plays);
    p = ig_put_str(p, " skip="); p = ig_put_dec(p, s->skipped);
    p = ig_put_str(p, " ovf="); p = ig_put_dec(p, s->overflows);
    p = ig_put_str(p, " floor="); p = ig_put_dec(p, (unsigned)g->ns.floor_rms);
    p = ig_put_str(p, " peak="); p = ig_put_dec(p, g->peak_now);
    p = ig_put_str(p, " ana_max="); p = ig_put_dec(p, s->ana_max_us);
    p = ig_put_str(p, "us ana_avg="); p = ig_put_dec(p, ig_udiv(s->ana_sum_us, s->ana_n));
    p = ig_put_str(p, "us idle_max="); p = ig_put_dec(p, s->idle_max_us);
    p = ig_put_str(p, "us logdrop="); p = ig_put_dec(p, g->lb ? g->lb->dropped : 0u);
    q(g, p);
    g->open = 0;
    g->notsix_set = 0;
    matcher_reset(&g->m);
    seg_reset(&g->ns);
    g->pn = 0;
    clear_queue(g);
    g->dlg_blocked = 0;
}

/* ---------------- safety ---------------- */
void ig_anomaly(ig_t *g, unsigned now_ms, unsigned kind, unsigned detail)
{
    char *p;
    if (g->panic) return;
    g->anomalies++;
    if (kind == IG_AN_DLGFAIL) g->dlgfails++;
    p = put_t(g, "anomaly", now_ms);
    p = ig_put_str(p, " kind="); p = ig_put_dec(p, kind);
    p = ig_put_str(p, " detail="); p = ig_put_dec(p, detail);
    p = ig_put_str(p, " n="); p = ig_put_dec(p, g->anomalies);
    q(g, p);
    if (!g->panic_why) g->panic_why = kind;
}

void ig_cost(ig_t *g, unsigned now_ms, unsigned us, int analysed)
{
    if (analysed) {
        if (us > g->st.ana_max_us) g->st.ana_max_us = us;
        if (g->st.ana_sum_us < 0x7FFFFFFFu - us) { g->st.ana_sum_us += us; g->st.ana_n++; }
    } else if (us > g->st.idle_max_us) g->st.idle_max_us = us;
    if (us >= IG_SLOW_US) ig_anomaly(g, now_ms, IG_AN_SLOW, us);
}

static void enter_panic(ig_t *g, unsigned now)
{
    char *p;
    g->panic = 1;
    inj_init(&g->inj, SONG_ZELDA);
    inj_set_manual(&g->inj, 1);
    clear_queue(g);
    p = put_t(g, "PANIC", now);
    p = ig_put_str(p, " anomalies="); p = ig_put_dec(p, g->anomalies);
    p = ig_put_str(p, " dlgfails="); p = ig_put_dec(p, g->dlgfails);
    p = ig_put_str(p, " first_kind="); p = ig_put_dec(p, g->panic_why);
    p = ig_put_str(p, " -> all features off until the game is restarted");
    q(g, p);
}

static void on_dialog_done(ig_t *g, unsigned now)
{
    int res = g->drv.d.result, six = state_of(g) == ST_OPEN;
    int song = g->ncand ? g->cand[g->icand] : -1;
    char *p = put_t(g, "answer", now);
    p = ig_put_str(p, " song="); p = put_song(p, song);
    p = ig_put_str(p, res == DLG_RES_YES ? " res=YES" : res == DLG_RES_NO ? " res=NO" :
                      res == DLG_RES_WATCHDOG ? " res=WATCHDOG" : " res=FAIL");
    p = ig_put_str(p, " code="); p = ig_put_dec(p, g->drv.open_code);
    p = ig_put_str(p, " fail="); p = ig_put_dec(p, (unsigned)g->drv.d.fail);
    p = ig_put_str(p, " st="); p = ig_put_hex(p, (unsigned)state_of(g), 2);
    q(g, p);
    g->mute_until = now + IG_MUTE_AFTER_MS;
    g->restart = 1;
    if (res == DLG_RES_YES || res == DLG_RES_NO) {
        if (!six) { ig_anomaly(g, now, IG_AN_STATE, (unsigned)state_of(g)); clear_queue(g); return; }
        if (res == DLG_RES_YES) {
            g->st.yes++;
            if (song >= 0) start_play(g, song, now, "YES");
            clear_queue(g);
        } else {
            g->st.no++;
            g->icand++;
            if (g->icand < g->ncand) { g->ask_pending = 1; g->ask_at = now + IG_NEXT_ASK_MS; }
            else { p = put_t(g, "all refused", now); q(g, p); clear_queue(g); }
        }
        return;
    }
    g->dlg_blocked = 1;
    clear_queue(g);
    ig_anomaly(g, now, IG_AN_DLGFAIL, res == DLG_RES_WATCHDOG ? 999u : g->drv.open_code);
}

/* ---------------- the frame ---------------- */
unsigned ig_frame(ig_t *g, unsigned now, unsigned real_hold, unsigned a_edges, unsigned *mask)
{
    unsigned out = 0, ev, dv;
    int st, ours, busy, muted;

    *mask = 0;
    st = state_of(g);
    g->st.frames++;

    /* 1. our dialog (also in panic while it is still on screen, so it restores the game's state) */
    if (g->dialogs && (!g->panic || dlg_active(&g->drv.d))) {
        dv = drv_tick(&g->drv, now, a_edges);
        if (dv & DRV_DROPPED) { char *p = put_t(g, "ask dropped", now); q(g, p); clear_queue(g); g->st.dropped_req++; }
        if (dv & DRV_FINISHED) on_dialog_done(g, now);
        st = state_of(g);
    }

    /* 2. next candidate after a No */
    if (g->ask_pending && (int)(now - g->ask_at) >= 0) {
        g->ask_pending = 0;
        if (!g->panic && st == ST_OPEN && !g->dlg_blocked && g->icand < g->ncand) ask(g, now);
        else { char *p = put_t(g, "ask dropped", now); q(g, p); clear_queue(g); g->st.dropped_req++; }
    }

    /* 3. replay (OR mask for the pad hook) + its watchdog */
    if (!g->panic) {
        ev = inj_step(&g->inj, now, st, INJ_NONE, mask);
        if (ev & INJ_EV_START) {
            char *p = put_t(g, "replay start", now);
            p = ig_put_str(p, " song="); p = put_song(p, g->inj.song);
            q(g, p);
        }
        if (ev & INJ_EV_SETTLED) {
            char *p = put_t(g, "replay end", now);
            p = ig_put_str(p, " abort="); p = ig_put_dec(p, (ev & INJ_EV_ABORT) ? 1u : 0u);
            p = ig_put_str(p, " st_end="); p = ig_put_hex(p, (unsigned)(g->inj.st_end < 0 ? 0xFF : g->inj.st_end), 2);
            p = ig_put_str(p, " st_settle="); p = ig_put_hex(p, (unsigned)(g->inj.st_settle < 0 ? 0xFF : g->inj.st_settle), 2);
            q(g, p);
        }
        busy = inj_busy(&g->inj);
        if (busy && !g->inj_was_busy) g->inj_busy_ms = now;
        if (busy && now - g->inj_busy_ms >= IG_INJ_WATCHDOG_MS) {
            inj_init(&g->inj, SONG_ZELDA); inj_set_manual(&g->inj, 1); *mask = 0; busy = 0;
            ig_anomaly(g, now, IG_AN_INJWD, 0);
        }
        g->inj_was_busy = busy;
    }

    /* 4. mute: our dialog / a replay / the user playing by hand -> nothing heard now counts, and 1 s after */
    ours = (g->dialogs && dlg_busy(&g->drv.d)) || g->ask_pending;
    busy = ours || inj_busy(&g->inj) || (real_hold & IG_BUTTONS);
    if (busy) { g->mute_until = now + IG_MUTE_AFTER_MS; g->restart = 1; }
    muted = (int)(now - g->mute_until) < 0;

    /* 5. ocarina session open/close (our own dialog moves the state byte: ignored while it runs) */
    if (!ours) {
        if (st == ST_OPEN) { g->notsix_set = 0; if (!g->open && !g->panic) session_open(g, now); }
        else if (g->open) {
            if (!g->notsix_set) { g->notsix_set = 1; g->notsix_ms = now; }
            else if (now - g->notsix_ms >= IG_CLOSE_MS) session_close(g, now);
        }
    }

    /* 6. listen: only with the ocarina open and nothing of ours going on */
    if (!g->panic && g->open && st == ST_OPEN && !muted && !ours && !g->ncand) {
        if (g->restart) do_restart(g);
        if (analyse(g, now)) out |= IG_OUT_ANALYSED;
    }

    /* 7. panic (never in the middle of our dialog: it must finish and restore first) */
    if (!g->panic && (g->anomalies >= IG_PANIC_ANOMALIES || g->dlgfails >= IG_PANIC_DLGFAILS) &&
        !(g->dialogs && dlg_active(&g->drv.d))) {
        enter_panic(g, now);
        *mask = 0;
        if (g->open) session_close(g, now);
    }
    if (g->panic) { out |= IG_OUT_PANIC; *mask = 0; }

    /* 8. SD log only while the ocarina is closed and nothing of ours runs */
    if (!g->open && g->lb && lb_pending(g->lb) >= IG_FLUSH_IDLE_MIN && !(g->dialogs && dlg_busy(&g->drv.d)) &&
        !inj_busy(&g->inj))
        out |= IG_OUT_FLUSH;

    g->last_st = st;
    return out;
}
