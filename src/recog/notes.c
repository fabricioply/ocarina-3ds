#include "notes.h"

static const unsigned short log2tab[32] = {
    0, 2909, 5732, 8473, 11136, 13727, 16248, 18704, 21098, 23433, 25711, 27936, 30109, 32234, 34312,
    36346, 38336, 40286, 42196, 44068, 45904, 47705, 49472, 51207, 52911, 54584, 56229, 57845, 59434,
    60997, 62534, 64047
};

/* log2(v) in Q16, v >= 1, error < 0.25 cent (32-segment linear interpolation of log2(1+x)) */
static int log2_q16(unsigned v)
{
    int e = 31, idx, rem, lo, hi;
    unsigned x;
    while (e > 0 && !((v >> e) & 1u)) e--;
    x = (v << (31 - e)) << 1;              /* fraction of the mantissa, Q32 */
    idx = (int)(x >> 27);
    rem = (int)((x >> 11) & 0xFFFFu);
    lo = log2tab[idx];
    hi = idx == 31 ? 65536 : log2tab[idx + 1];
    return e * 65536 + lo + (((hi - lo) * rem) >> 16);
}

int notes_hz_to_cents(int hz)
{
    int l;
    if (hz <= 0) return 0;
    l = log2_q16((unsigned)hz) - 575495;                 /* relative to 440 Hz */
    return 6900 + (l * 75 + (l < 0 ? -2048 : 2048)) / 4096;   /* l*1200/65536 = l*75/4096; |l| < 2^20 */
}

static int iabs(int v) { return v < 0 ? -v : v; }

void notes_init(notes_state *s)
{
    unsigned char *p = (unsigned char *)s;
    unsigned i;
    for (i = 0; i < sizeof *s; i++) p[i] = 0;
}

static int floor_estimate(const notes_state *s)
{
    int tmp[NOTES_FLOOR_N], i, k, n = s->nrms, target;
    if (n < 8) return 0;
    for (i = 0; i < n; i++) tmp[i] = s->rms[i];
    target = n * NOTES_FLOOR_PCT / 100;
    for (k = 0; k <= target; k++) {            /* selection of the target-th smallest */
        int m = k;
        for (i = k + 1; i < n; i++) if (tmp[i] < tmp[m]) m = i;
        i = tmp[k]; tmp[k] = tmp[m]; tmp[m] = i;
    }
    return tmp[target];
}

int notes_gate(const notes_state *s)
{
    int g = s->floor_rms * NOTES_GATE_NUM / NOTES_GATE_DEN;
    return g < NOTES_GATE_MIN ? NOTES_GATE_MIN : g;
}

static void ev_on(note_event *e, notes_state *s, int center, int hz, int start)
{
    s->active = 1; s->center = center; s->start_hop = start;
    s->midi = (center + 50) / 100;
    e->type = NOTE_ON; e->midi = s->midi; e->cents = center; e->hz = hz; e->start_hop = start; e->end_hop = 0;
}

static void ev_off(note_event *e, notes_state *s, int end)
{
    e->type = NOTE_OFF; e->midi = s->midi; e->cents = s->center; e->hz = 0;
    e->start_hop = s->start_hop; e->end_hop = end;
    s->active = 0;
}

int notes_feed(notes_state *s, const pitch_result *r, note_event out[2])
{
    int n = 0;
    s->hop++;
    /* noise-floor history (always fed, whatever the signal) */
    s->rms[s->rpos] = r->rms;
    s->rpos = (s->rpos + 1) % NOTES_FLOOR_N;
    if (s->nrms < NOTES_FLOOR_N) s->nrms++;
    s->floor_rms = floor_estimate(s);

    if (r->hz > 0) {
        int c = notes_hz_to_cents(r->hz), hop = s->hop - 1;
        s->miss = 0;
        s->last_voiced = hop;
        if (s->active && iabs(c - s->center) <= NOTES_HYST_CENTS) {
            s->center += (c - s->center) / 4;
            s->cand_n = 0;
        } else {
            if (s->cand_n && iabs(c - s->cand_c) <= NOTES_STABLE_CENTS) {
                s->cand_n++;
                s->cand_c += (c - s->cand_c) / 2;
            } else {
                s->cand_n = 1; s->cand_c = c; s->cand_start = hop; s->cand_hz = r->hz;
            }
            if (s->cand_n >= NOTES_MIN_HOPS) {
                if (s->active) ev_off(&out[n++], s, s->cand_start);
                ev_on(&out[n++], s, s->cand_c, s->cand_hz, s->cand_start);
                s->cand_n = 0;
            }
        }
    } else {
        s->miss++;
        if (s->miss > NOTES_MAX_GAP) {
            s->cand_n = 0;
            if (s->active) ev_off(&out[n++], s, s->last_voiced + 1);
        }
    }
    return n;
}

int notes_flush(notes_state *s, note_event out[1])
{
    if (!s->active) return 0;
    ev_off(&out[0], s, s->last_voiced + 1);
    return 1;
}
