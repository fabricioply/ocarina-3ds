/* Host tests for catalog.c + matcher.c. Deterministic (fixed LCG). Reproduces the PC tests/test_matcher.cpp cases
 * (pitches in semitones*100) and adds: all 12 songs transposed with +-20 cent jitter, ornament/missing note,
 * loose offers, ambiguity, random rejection, replay + dialog text. */
#include <stdio.h>
#include <string.h>
#include "matcher.h"

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static unsigned lcg = 987654321u;
static int rnd(int n) { lcg = lcg * 1664525u + 1013904223u; return (int)((lcg >> 8) % (unsigned)n); }

static int g_end;   /* end time of the last note played */

/* Plays notes (cents) with `dur` ms each and `gap` ms between; polls after each note like the plugin would.
 * Returns the first reported result (state MATCH_NONE if none). */
static matcher_result play_t(matcher_state *m, const int *p, int n, int dur, int gap, int start)
{
    matcher_result first, r;
    int i, t = start;
    first.state = MATCH_NONE; first.song = -1; first.count = 0; first.score = 0;
    for (i = 0; i < n; i++) {
        matcher_note_ev ev; ev.cents = p[i]; ev.start_ms = t; ev.end_ms = t + dur;
        matcher_note(m, &ev);
        if (matcher_poll(m, t + dur, 1, &r) && first.state == MATCH_NONE) first = r;
        g_end = t + dur;
        t += dur + gap;
    }
    return first;
}
static matcher_result play(matcher_state *m, const int *p, int n) { return play_t(m, p, n, 300, 80, 0); }
/* silence after the phrase */
static matcher_result silence(matcher_state *m)
{
    matcher_result r;
    matcher_poll(m, g_end + MATCHER_FINISH_MS - 1, 0, &r);
    CHECK(r.state == MATCH_NONE);                    /* not yet */
    matcher_poll(m, g_end + MATCHER_FINISH_MS, 0, &r);
    return r;
}
#define PLAY(m, ...) (play((m), (const int[]){__VA_ARGS__}, (int)(sizeof((const int[]){__VA_ARGS__}) / sizeof(int))))

static void fresh(matcher_state *m, int assisted) { matcher_init(m, assisted); }
static void only(matcher_state *m, int a, int b) { matcher_set_enabled(m, (1u << a) | (1u << b)); }

/* ---------------- catalog ---------------- */
static void test_catalog(void)
{
    static const char *names[12] = {"Zelda's Lullaby", "Sun's Song", "Minuet of Forest", "Requiem of Spirit", "Epona's Song",
        "Song of Time", "Bolero of Fire", "Nocturne of Shadow", "Saria's Song", "Song of Storms", "Serenade of Water", "Prelude of Light"};
    static const int lens[12] = {6, 6, 6, 6, 6, 6, 8, 7, 6, 6, 5, 6};
    static const int flags[12] = {96, 99, 90, 93, 97, 100, 91, 94, 98, 101, 92, 95};
    int i, j;
    CHECK(CATALOG_COUNT == 12 && catalog_get(12) == 0 && catalog_get(-1) == 0);
    for (i = 0; i < 12; i++) {
        const catalog_song *s = catalog_get(i);
        replay_step st[8];
        CHECK(strcmp(s->name, names[i]) == 0);
        CHECK(s->length == lens[i] && s->flag_item == flags[i]);
        CHECK(catalog_cents(i, 0) == 0);
        CHECK(catalog_replay(i, st, 8) == lens[i]);
        for (j = 0; j < s->length; j++) {
            static const unsigned btn[5] = {0x200, 0x100, 0x800, 0x400, 0x1};
            CHECK(st[j].mask == btn[s->notes[j]] && catalog_button(i, j) == st[j].mask);
            CHECK(st[j].on_ms == (unsigned)(200 * j) && st[j].off_ms == (unsigned)(200 * j + 120));
        }
        CHECK(catalog_replay(i, st, s->length - 1) == 0);
    }
    /* semitones from the PC: Saria 65,69,71 over 65 = 0,4,6; Storms 62,65,74 = 0,3,12; Time 69,62,65 = 0,-7,-4 */
    CHECK(catalog_cents(SONG_SARIA, 1) == 400 && catalog_cents(SONG_SARIA, 2) == 600);
    CHECK(catalog_cents(SONG_STORMS, 1) == 300 && catalog_cents(SONG_STORMS, 2) == 1200);
    CHECK(catalog_cents(SONG_TIME, 1) == -700 && catalog_cents(SONG_TIME, 2) == -400);
    CHECK(catalog_cents(SONG_ZELDA, 1) == 300 && catalog_cents(SONG_ZELDA, 2) == -200);
    /* replay timing: Time = 2,0,1,2,0,1 -> Y,L,R,Y,L,R, 6 slots of 200 ms */
    {
        unsigned mk;
        CHECK(CATALOG_REPLAY_DELAY_MS == 1500);
        CHECK(catalog_replay_mask(SONG_TIME, -1, &mk) == 1 && mk == 0);
        CHECK(catalog_replay_mask(SONG_TIME, 0, &mk) == 1 && mk == BTN_Y);
        CHECK(catalog_replay_mask(SONG_TIME, 119, &mk) == 1 && mk == BTN_Y);
        CHECK(catalog_replay_mask(SONG_TIME, 120, &mk) == 1 && mk == 0);
        CHECK(catalog_replay_mask(SONG_TIME, 200, &mk) == 1 && mk == BTN_L);
        CHECK(catalog_replay_mask(SONG_TIME, 1119, &mk) == 1 && mk == BTN_R);
        CHECK(catalog_replay_mask(SONG_TIME, 1199, &mk) == 1 && mk == 0);
        CHECK(catalog_replay_mask(SONG_TIME, 1200, &mk) == 0 && mk == 0);
    }
    /* dialog text */
    {
        unsigned char b[64];
        static const unsigned char tail[] = {0x7F,0x1C,0x7F,0x1C,0x7F,0x1A,0xFF,0xFF,0xFF,0xFF,0x7F,0x1D,'B','Y','e','s',
                                             0x7F,0x1C,'N','o',0x7F,0x1D,0x00,0x7F,0x00};
        int n = catalog_dialog_text(SONG_SARIA, b, 64);
        CHECK(n == (int)strlen("Saria's Song?") + (int)sizeof tail);
        CHECK(memcmp(b, "Saria's Song?", 13) == 0 && memcmp(b + 13, tail, sizeof tail) == 0);
        CHECK(catalog_dialog_text(SONG_SARIA, b, 20) == -1 && catalog_dialog_text(99, b, 64) == -1);
        for (i = 0; i < 12; i++) CHECK(catalog_dialog_text(i, b, 64) > 0);   /* the longest name fits 64 bytes */
    }
}

/* ---------------- PC cases ---------------- */
static void test_pc_cases(void)
{
    matcher_state m, ms;
    matcher_result r;
    int d, k;
    /* the ten melodies of the PC test, plain */
    static const struct { int id; int p[8]; int n; } mel[] = {
        {SONG_SARIA,   {6500,6900,7100,6500,6900,7100}, 6}, {SONG_EPONA,  {7400,7100,6900,7400,7100,6900}, 6},
        {SONG_SUN,     {6900,6500,7400,6900,6500,7400}, 6}, {SONG_STORMS, {6200,6500,7400,6200,6500,7400}, 6},
        {SONG_MINUET,  {6200,7400,7100,6900,7100,6900}, 6}, {SONG_BOLERO, {6500,6200,6500,6200,6900,6500,6900,6500}, 8},
        {SONG_SERENADE,{6200,6500,6900,6900,7100}, 5},      {SONG_REQUIEM,{6200,6500,6200,6900,6500,6200}, 6},
        {SONG_NOCTURNE,{7100,6900,6900,6200,7100,6900,6500}, 7}, {SONG_PRELUDE,{7400,6900,7400,6900,7100,7400}, 6}};
    for (k = 0; k < 10; k++) {
        fresh(&m, 1);
        r = play(&m, mel[k].p, mel[k].n);
        CHECK(r.state == MATCH_SONG && r.song == mel[k].id && r.count == 1 && r.candidates[0] == mel[k].id);
    }
    /* Song of Time, different transpositions and note lengths */
    {
        static const int starts[3] = {4800, 6000, 7200}, durs[2] = {150, 600};
        int a, b;
        for (a = 0; a < 3; a++) for (b = 0; b < 2; b++) {
            int s = starts[a], p[6];
            p[0] = s; p[1] = s - 700; p[2] = s - 400; p[3] = s; p[4] = s - 700; p[5] = s - 400;
            fresh(&m, 1);
            r = play_t(&m, p, 6, durs[b], 80, 0);
            CHECK(r.state == MATCH_SONG && r.song == SONG_TIME && r.score >= 800);
        }
    }
    /* original two-song catalog: Time + Lullaby */
    fresh(&m, 1); only(&m, SONG_TIME, SONG_ZELDA);
    r = PLAY(&m, 6040, 5260, 5640, 5960, 5340, 5560); CHECK(r.state == MATCH_SONG && r.song == SONG_TIME);
    matcher_reset(&m); r = PLAY(&m, 6000, 5300, 5600, 6000, 5300); CHECK(r.state == MATCH_NONE);
    matcher_reset(&m); r = PLAY(&m, 6000, 5300, 5900, 6000, 5300, 5600); CHECK(r.state == MATCH_NONE);
    matcher_reset(&m); r = PLAY(&m, 5600, 5300, 6000, 5600, 5300, 6000); CHECK(r.state == MATCH_NONE);
    matcher_reset(&m); r = PLAY(&m, 6000, 5400, 5800, 6300, 5700, 6100); CHECK(r.state == MATCH_NONE);
    /* precise mode rejects the sloppy bends; assisted accepts them */
    fresh(&ms, 0);
    r = PLAY(&ms, 6040, 5260, 5640, 5960, 5340, 5560); CHECK(r.state == MATCH_NONE);
    fresh(&m, 1);
    r = PLAY(&m, 6080, 5220, 5680, 5920, 5380, 5520); CHECK(r.state == MATCH_SONG && r.song == SONG_TIME);
    /* a long phrase (5 s notes) falls out of the 20 s window */
    fresh(&m, 1); only(&m, SONG_TIME, SONG_ZELDA);
    { int p[6] = {6000, 5300, 5600, 6000, 5300, 5600}; r = play_t(&m, p, 6, 5000, 80, 0); CHECK(r.state == MATCH_NONE); }
    /* latch: second identical phrase is ignored until reset */
    fresh(&m, 1); only(&m, SONG_TIME, SONG_ZELDA);
    r = PLAY(&m, 6000, 5300, 5600, 6000, 5300, 5600); CHECK(r.state == MATCH_SONG);
    { int p[6] = {6000, 5300, 5600, 6000, 5300, 5600}; r = play_t(&m, p, 6, 300, 80, 5000); CHECK(r.state == MATCH_NONE); }
    matcher_reset(&m); r = PLAY(&m, 6000, 5300, 5600, 6000, 5300, 5600); CHECK(r.state == MATCH_SONG);
    /* Lullaby, two transpositions, and a wrong last note */
    fresh(&m, 1); only(&m, SONG_TIME, SONG_ZELDA);
    r = PLAY(&m, 7100, 7400, 6900, 7100, 7400, 6900); CHECK(r.state == MATCH_SONG && r.song == SONG_ZELDA);
    matcher_reset(&m); r = PLAY(&m, 5900, 6200, 5700, 5900, 6200, 5700); CHECK(r.state == MATCH_SONG && r.song == SONG_ZELDA);
    matcher_reset(&m); r = PLAY(&m, 7100, 7400, 6900, 7100, 7400, 7300); CHECK(r.state == MATCH_NONE);
    /* ornament (slide note of 100 ms) between stable notes: still Song of Time */
    fresh(&m, 1);
    {
        static const int p[8] = {6000, 5300, 5400, 5300, 5600, 6000, 5300, 5600};
        static const int len[8] = {400, 300, 100, 300, 400, 400, 300, 400};
        int i, t = 0; matcher_result q;
        r.state = MATCH_NONE;
        for (i = 0; i < 8; i++) {
            matcher_note_ev ev; ev.cents = p[i]; ev.start_ms = t; ev.end_ms = t + len[i]; t += len[i];
            matcher_note(&m, &ev);
            if (matcher_poll(&m, t, 1, &q) && r.state == MATCH_NONE) r = q;
        }
        CHECK(r.state == MATCH_SONG && r.song == SONG_TIME);
        fresh(&ms, 0); t = 0; r.state = MATCH_NONE;                                /* precise: no ornaments */
        for (i = 0; i < 8; i++) {
            matcher_note_ev ev; ev.cents = p[i]; ev.start_ms = t; ev.end_ms = t + len[i]; t += len[i];
            matcher_note(&ms, &ev);
            if (matcher_poll(&ms, t, 1, &q) && r.state == MATCH_NONE) r = q;
        }
        CHECK(r.state == MATCH_NONE);
    }
    /* stable wrong note cannot be deleted to fabricate a song */
    fresh(&m, 1); r = PLAY(&m, 6000, 5300, 5900, 5600, 6000, 5300, 5600); CHECK(r.state != MATCH_SONG);
    /* tiny fragment (40 ms) is ignored, not wiping the phrase */
    fresh(&m, 1);
    {
        static const int p[7] = {6500, 6900, 7000, 7100, 6500, 6900, 7100};
        static const int s[7] = {0, 300, 600, 640, 940, 1240, 1540}, e[7] = {300, 600, 640, 940, 1240, 1540, 1840};
        int i; matcher_result q; r.state = MATCH_NONE;
        for (i = 0; i < 7; i++) {
            matcher_note_ev ev; ev.cents = p[i]; ev.start_ms = s[i]; ev.end_ms = e[i];
            matcher_note(&m, &ev);
            if (matcher_poll(&m, e[i], 1, &q) && r.state == MATCH_NONE) r = q;
        }
        CHECK(r.state == MATCH_SONG && r.song == SONG_SARIA);
    }
    /* attempts separated by a long pause do not combine */
    fresh(&m, 1);
    r = PLAY(&m, 7100, 7400, 6900); CHECK(r.state == MATCH_NONE);
    { int p[3] = {7100, 7400, 6900}; r = play_t(&m, p, 3, 300, 80, 3000); CHECK(r.state == MATCH_NONE); }
    /* durations: a 50 ms blip is ignored; overlap starts a new phrase */
    fresh(&m, 1);
    { matcher_note_ev ev = {6000, 0, 50}; matcher_note(&m, &ev); CHECK(m.n == 0); }
    { matcher_note_ev a = {6000, 0, 300}, b = {6200, 200, 500}; matcher_note(&m, &a); matcher_note(&m, &b); CHECK(m.n == 1); }
    (void)d;
}

/* ---------------- Saria vs Storms ---------------- */
static void test_saria_storms(void)
{
    matcher_state m;
    matcher_result r;
    static const int bases[3] = {4800, 6200, 7400};
    int b;
    /* compressed rise: ambiguous, ONE decision for the caller (Saria first, Storms second), no song */
    fresh(&m, 1);
    r = PLAY(&m, 6000, 6300, 6700, 6000, 6300, 6700);
    CHECK(r.state == MATCH_AMBIGUOUS && r.count == 2 && r.candidates[0] == SONG_SARIA && r.candidates[1] == SONG_STORMS && r.song == SONG_SARIA);
    CHECK(r.score >= 800);
    CHECK(m.n == 0);                                           /* phrase dropped so the retry starts clean */
    /* clean retry resolves without reset */
    { int p[6] = {6000, 6300, 7200, 6000, 6300, 7200}; r = play_t(&m, p, 6, 300, 80, 3000); CHECK(r.state == MATCH_SONG && r.song == SONG_STORMS); }
    for (b = 0; b < 3; b++) {
        int base = bases[b];
        matcher_state s1, s2;
        int p1[6], p2[6];
        p1[0] = base + 20; p1[1] = base + 380; p1[2] = base + 610; p1[3] = base - 20; p1[4] = base + 420; p1[5] = base + 590;
        p2[0] = base + 20; p2[1] = base + 280; p2[2] = base + 1210; p2[3] = base - 20; p2[4] = base + 320; p2[5] = base + 1190;
        fresh(&s1, 1); r = play(&s1, p1, 6); CHECK(r.state == MATCH_SONG && r.song == SONG_SARIA);
        fresh(&s2, 1); r = play(&s2, p2, 6); CHECK(r.state == MATCH_SONG && r.song == SONG_STORMS);
    }
    /* mixed evidence between halves: never a song */
    fresh(&m, 1); r = PLAY(&m, 6000, 6300, 6700, 6000, 6400, 6600); CHECK(r.state != MATCH_SONG);
    /* the real log of the PC: ascending motifs -> pair offered */
    fresh(&m, 1); r = PLAY(&m, 5502, 5929, 6442, 5375, 5978, 6466);
    CHECK(r.state == MATCH_AMBIGUOUS && r.candidates[0] == SONG_SARIA && r.candidates[1] == SONG_STORMS);
    /* only Saria learned: a single candidate; none learned: nothing */
    fresh(&m, 1); matcher_set_enabled(&m, 1u << SONG_SARIA);
    r = PLAY(&m, 6000, 6300, 6700, 6000, 6300, 6700); CHECK(r.state == MATCH_AMBIGUOUS && r.count == 1 && r.candidates[0] == SONG_SARIA);
    fresh(&m, 1); matcher_set_enabled(&m, 0);
    r = PLAY(&m, 6000, 6300, 6700, 6000, 6300, 6700); CHECK(r.state == MATCH_NONE);
    /* precise mode: no pair offers */
    fresh(&m, 0); r = PLAY(&m, 6000, 6300, 6700, 6000, 6300, 6700); CHECK(r.state == MATCH_NONE);
}

/* ---------------- loose offers after silence ---------------- */
static void test_loose(void)
{
    matcher_state m;
    matcher_result r;
    /* sloppy Lullaby (+3,+5,-9,+3,+5 ideal) played with off intervals: proposed after the pause, never lost */
    fresh(&m, 1);
    r = PLAY(&m, 7100, 7600, 6800, 7300, 7700, 6900); CHECK(r.state == MATCH_NONE);
    r = silence(&m);
    CHECK(r.state == MATCH_AMBIGUOUS && r.count >= 1 && r.count <= 3 && r.candidates[0] == SONG_ZELDA);
    CHECK(matcher_poll(&m, g_end + 5000, 0, &r) == 0 && r.state == MATCH_NONE);       /* reported once */
    /* a sounding note blocks the silence timer */
    fresh(&m, 1);
    r = PLAY(&m, 7100, 7600, 6800, 7300, 7700, 6900);
    CHECK(matcher_poll(&m, g_end + 3000, 1, &r) == 0);
    CHECK(matcher_poll(&m, g_end + 3000, 0, &r) == 1 && r.state == MATCH_AMBIGUOUS);
    /* wrong direction (rising scale) is not similar: TIMEOUT, not an offer */
    fresh(&m, 1);
    r = PLAY(&m, 6000, 6200, 6400, 6600, 6800, 7000); CHECK(r.state == MATCH_NONE);
    r = silence(&m); CHECK(r.state == MATCH_TIMEOUT && r.count == 0);
    /* a short phrase (3 notes) also times out; afterwards the matcher is empty */
    fresh(&m, 1);
    r = PLAY(&m, 7100, 7400, 6900); r = silence(&m); CHECK(r.state == MATCH_TIMEOUT && m.n == 0);
    /* precise mode never proposes */
    fresh(&m, 0);
    r = PLAY(&m, 7100, 7600, 6800, 7300, 7700, 6900); r = silence(&m); CHECK(r.state == MATCH_TIMEOUT);
    /* after a recognised song nothing else is reported */
    fresh(&m, 1);
    r = PLAY(&m, 6000, 5300, 5600, 6000, 5300, 5600); CHECK(r.state == MATCH_SONG);
    r = silence(&m); CHECK(r.state == MATCH_NONE);
    /* several candidates are ordered best first and capped at 3 */
    fresh(&m, 1);
    r = PLAY(&m, 6000, 6300, 6500, 6200, 6400, 6300);   /* arbitrary shape: only check ordering invariants */
    r = silence(&m);
    if (r.state == MATCH_AMBIGUOUS) { int i; CHECK(r.count <= 3); for (i = 0; i < r.count; i++) CHECK(r.candidates[i] >= 0 && r.candidates[i] < 12); }
}

/* ---------------- every song: transposition, jitter, extra / missing note ---------------- */
static int build(int id, int base, int jit, int *p)
{
    const catalog_song *s = catalog_get(id);
    int i;
    for (i = 0; i < s->length; i++) p[i] = base + catalog_cents(id, i) + (jit ? rnd(2 * jit + 1) - jit : 0);
    return s->length;
}

static void test_all_songs(void)
{
    static const int bases[] = {5030, 5500, 6000, 6640, 7000, 7450};   /* up and down; some near an E5/F5-like boundary */
    int id, b, jit_trials, known_ext = 0, amb_ext = 0, bad_ext = 0, ext_n = 0, miss_wrong = 0, miss_n = 0, miss_offer = 0;
    for (id = 0; id < CATALOG_COUNT; id++) {
        for (b = 0; b < (int)(sizeof bases / sizeof bases[0]); b++) {
            matcher_state m;
            matcher_result r;
            int p[8], n, q[10], k, pos;
            fresh(&m, 1);
            n = build(id, bases[b], 0, p);
            r = play(&m, p, n); CHECK(r.state == MATCH_SONG && r.song == id);
            for (jit_trials = 0; jit_trials < 8; jit_trials++) {       /* +-20 cents jitter */
                fresh(&m, 1);
                n = build(id, bases[b], 20, p);
                r = play(&m, p, n);
                CHECK(r.state == MATCH_SONG && r.song == id);
                if (!(r.state == MATCH_SONG && r.song == id)) printf("  jitter miss: song %d base %d state %d song %d\n", id, bases[b], r.state, r.song);
            }
            /* tolerance edge: a note 120 cents off (still inside the 150 cent tolerance) */
            fresh(&m, 1);
            n = build(id, bases[b], 0, p); p[n / 2] += 120;
            r = play(&m, p, n);
            if (r.state == MATCH_SONG) CHECK(r.song == id);
            /* extra note: a short (100 ms) grace note between two notes, near the pitch of one of them */
            n = build(id, bases[b], 0, p);
            for (pos = 1; pos < n; pos++) {
                matcher_note_ev ev; matcher_result q2;
                int t = 0, i;
                fresh(&m, 1);
                r.state = MATCH_NONE;
                for (i = 0; i < n; i++) {
                    if (i == pos) {
                        ev.cents = p[i - 1] + (p[i] > p[i - 1] ? 60 : -60); ev.start_ms = t; ev.end_ms = t + 100; t += 100;
                        matcher_note(&m, &ev);
                        if (matcher_poll(&m, t, 1, &q2) && r.state == MATCH_NONE) r = q2;
                    }
                    ev.cents = p[i]; ev.start_ms = t; ev.end_ms = t + 300; t += 300;
                    matcher_note(&m, &ev);
                    if (matcher_poll(&m, t, 1, &q2) && r.state == MATCH_NONE) r = q2;
                }
                ext_n++;
                if (r.state == MATCH_AMBIGUOUS && r.candidates[0] == id) amb_ext++;   /* Saria/Storms: asked, Saria first - acceptable */
                else if (!(r.state == MATCH_SONG && r.song == id)) {
                    /* Known, same as on the PC rules: a 100 ms grace note after Prelude's 2nd note makes the window
                     * 0,-500,-440,0,-500,-200 which fits Song of Time inside the 150 cent assisted tolerance. */
                    if (id == SONG_PRELUDE && pos == 2 && r.state == MATCH_SONG && r.song == SONG_TIME) known_ext++;
                    else { bad_ext++; printf("  extra-note miss: song %d base %d pos %d -> state %d song %d\n", id, bases[b], pos, r.state, r.song); }
                }
            }
            /* missing note: never a wrong SONG; (PC behaviour: usually nothing) */
            n = build(id, bases[b], 0, p);
            for (pos = 1; pos < n - 1; pos++) {
                int i, qn = 0;
                for (i = 0; i < n; i++) if (i != pos) q[qn++] = p[i];
                fresh(&m, 1);
                r = play(&m, q, qn);
                miss_n++;
                if (r.state == MATCH_SONG && r.song != id) miss_wrong++;
                if (r.state != MATCH_SONG) { r = silence(&m); if (r.state == MATCH_AMBIGUOUS) miss_offer++; }
            }
            (void)k;
        }
    }
    printf("  extra-note: %d/%d exact SONG, %d confirmation asked (right song first), %d Prelude/Time look-alikes\n",
           ext_n - bad_ext - amb_ext - known_ext, ext_n, amb_ext, known_ext);
    printf("  missing-note: %d cases, wrong SONG %d, loose offers after pause %d\n", miss_n, miss_wrong, miss_offer);
    CHECK(bad_ext == 0);
    CHECK(miss_wrong == 0);
}

/* ---------------- rejection ---------------- */
static void test_random(void)
{
    int trial, songs = 0, offers = 0, timeouts = 0, none = 0;
    for (trial = 0; trial < 1000; trial++) {
        matcher_state m;
        matcher_result r;
        int p[12], n = 4 + rnd(9), i, dur = 100 + rnd(400), gap = 40 + rnd(300), t = 0;
        for (i = 0; i < n; i++) p[i] = 5500 + rnd(2800);
        fresh(&m, 1);
        r = play_t(&m, p, n, dur, gap, t);
        if (r.state == MATCH_SONG) { songs++; printf("  random SONG: trial %d song %d\n", trial, r.song); continue; }
        if (r.state == MATCH_AMBIGUOUS) { offers++; continue; }
        r = silence(&m);
        if (r.state == MATCH_AMBIGUOUS) offers++; else if (r.state == MATCH_TIMEOUT) timeouts++; else none++;
    }
    printf("  random: 1000 phrases -> SONG %d, offered-for-confirmation %d, timeout %d, none %d\n", songs, offers, timeouts, none);
    CHECK(songs == 0);
    CHECK(offers < 50);
    /* wide random leaps (a mess, not a melody) are rejected outright */
    {
        matcher_state m; matcher_result r; int p[7] = {5600, 7900, 5700, 8000, 5500, 7800, 5600};
        fresh(&m, 1); r = play(&m, p, 7); CHECK(r.state == MATCH_NONE); r = silence(&m); CHECK(r.state == MATCH_TIMEOUT);
    }
    /* repeated single note / silence */
    {
        matcher_state m; matcher_result r; int p[10] = {6900, 6900, 6900, 6900, 6900, 6900, 6900, 6900, 6900, 6900};
        fresh(&m, 1); r = play(&m, p, 10); CHECK(r.state == MATCH_NONE);
        r = silence(&m); CHECK(r.state == MATCH_TIMEOUT);
        CHECK(matcher_poll(&m, 100000, 0, &r) == 0);
    }
}

/* ---------------- phrase in the middle of noise, determinism ---------------- */
static void test_misc(void)
{
    matcher_state a, b;
    matcher_result ra, rb;
    int p[6] = {6000, 5300, 5600, 6000, 5300, 5600}, junk[3] = {5700, 7500, 6100};
    /* junk notes before the phrase (within the gap): tail alignment still finds it */
    fresh(&a, 1);
    play_t(&a, junk, 3, 200, 100, 0);
    ra = play_t(&a, p, 6, 300, 80, g_end + 100);
    CHECK(ra.state == MATCH_SONG && ra.song == SONG_TIME);
    /* determinism */
    fresh(&a, 1); fresh(&b, 1);
    ra = play(&a, p, 6); rb = play(&b, p, 6);
    CHECK(memcmp(&ra, &rb, sizeof ra) == 0 && a.n == b.n && memcmp(a.notes, b.notes, sizeof(matcher_note_ev) * (size_t)a.n) == 0);
    /* reset keeps configuration */
    matcher_set_enabled(&a, 1u << SONG_TIME); matcher_reset(&a); CHECK(a.enabled == (1u << SONG_TIME) && a.assisted == 1);
    /* a note that ends before it starts resets */
    { matcher_note_ev ev = {6000, 500, 400}; matcher_note(&a, &ev); CHECK(a.n == 0 && !a.latched); }
}

int main(void)
{
    test_catalog();
    test_pc_cases();
    test_saria_storms();
    test_loose();
    test_all_songs();
    test_random();
    test_misc();
    printf("%d checks, %d failures\n", checks, fails);
    return fails ? 1 : 0;
}
