#include "matcher.h"

#define TOL_ASSISTED 150   /* cents */
#define TOL_PRECISE   35
#define MIN_NOTE_MS   60
#define MAX_GAP_MS  1250
#define WINDOW_MS  20000
#define SCORE_SONG   800
#define SCORE_LEAD   100

static int iabs(int v) { return v < 0 ? -v : v; }
static int imin(int a, int b) { return a < b ? a : b; }
static int imax(int a, int b) { return a > b ? a : b; }

static int isqrt(int v)
{
    int r = 0, b = 1 << 28;
    if (v <= 0) return 0;
    while (b > v) b >>= 2;
    while (b) {
        if (v >= r + b) { v -= r + b; r = (r >> 1) + b; } else r >>= 1;
        b >>= 2;
    }
    return r;
}

/* ---- ornament / connection logic (PC: Connected, Ornament) ---- */
static int connected(const matcher_note_ev *n, int from, int to)
{
    int i;
    for (i = from; i < to; i++) if (n[i + 1].start_ms - n[i].end_ms > 30) return 0;
    return 1;
}

static int ornament(const matcher_note_ev *n, int index, int before, int after)
{
    int p = n[index].cents;
    int low = imin(n[before].cents, n[after].cents), high = imax(n[before].cents, n[after].cents);
    int distance = imin(iabs(p - n[before].cents), iabs(p - n[after].cents));
    int a;
    if (n[index].end_ms - n[index].start_ms <= 180 && (distance <= 150 || (p >= low - 50 && p <= high + 50))) return 1;
    for (a = 0; a < 2; a++) {
        int anchor = a ? after : before, first, last, i, adjacent = 1;
        if (iabs(p - n[anchor].cents) > 35) continue;
        first = imin(index, anchor); last = imax(index, anchor);
        if (!connected(n, first, last)) continue;
        for (i = first + 1; i < last; i++)
            if (n[i].end_ms - n[i].start_ms > 180 || iabs(n[i].cents - p) > 150) adjacent = 0;
        if (adjacent) return 1;
    }
    return 0;
}

/* ---- alignment ---- */
typedef struct {
    const matcher_note_ev *notes;
    int song, begin, span, length;
    int assisted, signature;
    int sel[CATALOG_MAX_NOTES];
    int nsel;
    int best;
} align_ctx;

static int distinctive_rise(const align_ctx *c)
{
    int motif;
    if (c->song != SONG_SARIA && c->song != SONG_STORMS) return 1;
    for (motif = 0; motif + 2 < c->nsel; motif += 3) {
        int first = c->notes[c->begin + c->sel[motif + 1]].cents - c->notes[c->begin + c->sel[motif]].cents;
        int fin   = c->notes[c->begin + c->sel[motif + 2]].cents - c->notes[c->begin + c->sel[motif + 1]].cents;
        if (c->song == SONG_SARIA && !(first >= 250 && fin >= 50 && fin <= 350 && first - fin >= 50)) return 0;
        if (c->song == SONG_STORMS && !(first >= 100 && fin >= 600 && fin - first >= 250)) return 0;
    }
    return 1;
}

static int alignment_score(const align_ctx *c)
{
    int len = c->nsel, i, k, off[CATALOG_MAX_NOTES], offset, tol, skipped;
    long err = 0;
    int rms10;
    if (c->signature && !distinctive_rise(c)) return 0;
    for (i = 0; i < len; i++) {
        int v = c->notes[c->begin + c->sel[i]].cents - catalog_cents(c->song, i);
        for (k = i; k > 0 && off[k - 1] > v; k--) off[k] = off[k - 1];
        off[k] = v;
    }
    offset = off[len / 2];
    if (len % 2 == 0) offset = (offset + off[len / 2 - 1]) >> 1;
    tol = c->assisted ? TOL_ASSISTED : TOL_PRECISE;
    for (i = 0; i < len; i++) {
        int r = c->notes[c->begin + c->sel[i]].cents - catalog_cents(c->song, i) - offset;
        if (iabs(r) > tol) return 0;
        err += (long)r * r;
    }
    for (i = 0; i + 1 < len; i++)
        for (skipped = c->sel[i] + 1; skipped < c->sel[i + 1]; skipped++)
            if (!ornament(c->notes, c->begin + skipped, c->begin + c->sel[i], c->begin + c->sel[i + 1])) return 0;
    rms10 = isqrt((int)(err * 100 / len));          /* rms in 0.1 cent */
    /* PC: 1 - (assisted ? 0.2 : 0.3) * rms_semitones - 0.035 * (span - length) */
    return 1000 - (c->assisted ? rms10 / 5 : rms10 * 3 / 10) - 35 * (c->span - c->length);
}

static void visit(align_ctx *c, int next)
{
    if (c->nsel == c->length - 1) {
        int s;
        if (next > c->span - 1) return;
        c->sel[c->nsel++] = c->span - 1;
        s = alignment_score(c);
        if (s > c->best) c->best = s;
        c->nsel--;
        return;
    }
    {
        int remaining = c->length - c->nsel - 1, i;
        for (i = next; i + remaining < c->span; i++) {
            c->sel[c->nsel++] = i;
            visit(c, i + 1);
            c->nsel--;
        }
    }
}

static int best_alignment(const matcher_note_ev *notes, int total, int song, int assisted, int signature)
{
    align_ctx c;
    int length = catalog_get(song)->length, span, maxspan;
    if (length < 3 || length > total || length > CATALOG_MAX_NOTES) return 0;
    c.notes = notes; c.song = song; c.length = length; c.assisted = assisted; c.signature = signature; c.best = 0;
    maxspan = imin(total, length + (assisted ? 2 : 0));
    for (span = length; span <= maxspan; span++) {
        c.span = span; c.begin = total - span;
        c.sel[0] = 0; c.nsel = 1;
        visit(&c, 1);
    }
    return c.best;
}

/* Assisted fallback: same-direction intervals, each within 500 cents of the song's (PC: LooseScore). */
static int loose_score(const matcher_note_ev *notes, int total, int song)
{
    int length = catalog_get(song)->length, begin, i;
    long sum = 0;
    if (length < 5 || length > total) return 0;
    begin = total - length;
    for (i = 0; i + 1 < length; i++) {
        int expected = catalog_cents(song, i + 1) - catalog_cents(song, i);
        int observed = notes[begin + i + 1].cents - notes[begin + i].cents;
        int diff;
        if (iabs(expected) < 50) { if (iabs(observed) > 160) return 0; }
        else if ((long)observed * expected <= 0 || iabs(observed) < 100) return 0;
        diff = iabs(observed - expected);
        if (diff > 500) return 0;
        sum += diff;
    }
    return 1000 - (int)(sum * 5 / (3 * (length - 1)));     /* 1 - avg/600 */
}

static int rising_motifs(const matcher_note_ev *n, int total)
{
    int begin, motif, i;
    if (total < 6) return 0;
    begin = total - 6;
    for (motif = 0; motif < 6; motif += 3) {
        int a = n[begin + motif].cents, b = n[begin + motif + 1].cents, c = n[begin + motif + 2].cents;
        if (b - a < 200 || b - a > 650 || c - b < 50 || c - b > 1200 || c - a < 400 || c - a > 1600) return 0;
    }
    for (i = 0; i < 3; i++) if (iabs(n[begin + i].cents - n[begin + i + 3].cents) > 200) return 0;
    return 1;
}

/* ---- public ---- */
static void clear_result(matcher_result *r)
{
    int i;
    r->state = MATCH_NONE; r->song = -1; r->count = 0; r->score = 0;
    for (i = 0; i < MATCHER_MAX_CAND; i++) r->candidates[i] = -1;
}

void matcher_reset(matcher_state *m)
{
    m->n = 0; m->latched = 0; m->has_pending = 0; m->finish_due = 0; m->last_end_ms = 0;
    clear_result(&m->pending);
}

void matcher_init(matcher_state *m, int assisted)
{
    m->assisted = assisted ? 1 : 0;
    m->enabled = (1u << CATALOG_COUNT) - 1u;
    matcher_reset(m);
}

void matcher_set_enabled(matcher_state *m, unsigned mask) { m->enabled = mask & ((1u << CATALOG_COUNT) - 1u); }

static void pending_pair(matcher_state *m, int score)
{
    matcher_result *r = &m->pending;
    clear_result(r);
    if (m->enabled & (1u << SONG_SARIA)) r->candidates[r->count++] = SONG_SARIA;
    if (m->enabled & (1u << SONG_STORMS)) r->candidates[r->count++] = SONG_STORMS;
    if (!r->count) return;
    r->state = MATCH_AMBIGUOUS; r->song = r->candidates[0]; r->score = score;
    m->has_pending = 1;
}

void matcher_note(matcher_state *m, const matcher_note_ev *ev)
{
    int song, best = 0, second = 0, best_id = -1, uncertain = 0, rising, clear_pair, i;
    if (m->latched) return;
    if (ev->end_ms < ev->start_ms) { matcher_reset(m); return; }
    if (ev->end_ms - ev->start_ms < MIN_NOTE_MS) return;
    if (m->n > 0 && (ev->start_ms < m->notes[m->n - 1].end_ms || ev->start_ms - m->notes[m->n - 1].end_ms > MAX_GAP_MS)) m->n = 0;
    m->notes[m->n++] = *ev;
    while (m->n > MATCHER_MAX_NOTES || (m->n > 0 && ev->end_ms - m->notes[0].start_ms > WINDOW_MS)) {
        for (i = 1; i < m->n; i++) m->notes[i - 1] = m->notes[i];
        m->n--;
    }
    m->finish_due = 1; m->last_end_ms = ev->end_ms;
    m->has_pending = 0; clear_result(&m->pending);

    for (song = 0; song < CATALOG_COUNT; song++) {
        int score;
        if (!(m->enabled & (1u << song))) continue;
        score = best_alignment(m->notes, m->n, song, m->assisted, 1);
        if (m->assisted && (song == SONG_SARIA || song == SONG_STORMS) && score < SCORE_SONG) {
            int w = best_alignment(m->notes, m->n, song, m->assisted, 0);
            if (w >= SCORE_SONG) uncertain = imax(uncertain, w);
        }
        if (score > best) { second = best; best = score; best_id = song; } else second = imax(second, score);
    }
    rising = m->assisted && rising_motifs(m->notes, m->n);
    clear_pair = (best_id == SONG_SARIA || best_id == SONG_STORMS) && best >= 900 && best - second >= SCORE_LEAD;
    if (rising && !clear_pair) {
        pending_pair(m, imax(best, uncertain));
        m->n = 0; return;
    }
    if (best >= SCORE_SONG && best - second >= SCORE_LEAD) {
        matcher_result *r = &m->pending;
        m->latched = 1;
        clear_result(r);
        r->state = MATCH_SONG; r->song = best_id; r->count = 1; r->candidates[0] = best_id; r->score = best;
        m->has_pending = 1;
        return;
    }
    if (uncertain >= SCORE_SONG) {
        pending_pair(m, uncertain);
        m->n = 0;      /* a retry starts a new phrase */
    }
}

/* Called after a silent pause: offer loosely matching songs (PC: SongMatcher::Finish). */
static void finish(matcher_state *m)
{
    int ids[CATALOG_COUNT], sc[CATALOG_COUNT], cnt = 0, song, i, k;
    if (m->assisted && !m->latched && m->n >= 5) {
        for (song = 0; song < CATALOG_COUNT; song++) {
            int s;
            if (!(m->enabled & (1u << song))) continue;
            if (m->n > catalog_get(song)->length + 1) continue;
            s = loose_score(m->notes, m->n, song);
            if (s >= 450) {
                for (k = cnt; k > 0 && sc[k - 1] < s; k--) { sc[k] = sc[k - 1]; ids[k] = ids[k - 1]; }   /* stable: ties keep catalog order */
                sc[k] = s; ids[k] = song; cnt++;
            }
        }
    }
    clear_result(&m->pending);
    if (cnt) {
        matcher_result *r = &m->pending;
        r->state = MATCH_AMBIGUOUS;
        for (i = 0; i < cnt && i < MATCHER_MAX_CAND; i++) r->candidates[r->count++] = ids[i];
        r->song = ids[0]; r->score = sc[0];
        m->has_pending = 1;
    } else if (m->n > 0 && !m->latched) {
        m->pending.state = MATCH_TIMEOUT;
        m->has_pending = 1;
    }
    m->n = 0;
}

int matcher_poll(matcher_state *m, int now_ms, int sounding, matcher_result *out)
{
    if (!m->has_pending && m->finish_due && !sounding && now_ms - m->last_end_ms >= MATCHER_FINISH_MS) {
        m->finish_due = 0;
        if (!m->latched) finish(m); else m->n = 0;
    }
    if (m->has_pending) {
        *out = m->pending;
        m->has_pending = 0;
        clear_result(&m->pending);
        return 1;
    }
    clear_result(out);
    return 0;
}
