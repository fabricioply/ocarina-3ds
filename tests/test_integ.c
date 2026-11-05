/* Host tests for the ocarina.3gx integration (integ.c): fake mic samples synthesised from a song's notes (sine segments,
 * 8182 Hz, deterministic noise) and a fake message context / game, 30 fps frames. Covers: SONG -> replay of that song's
 * buttons; AMBIGUOUS -> native dialog -> Yes -> replay; No -> next candidate; all refused; TIMEOUT; ocarina-closed
 * gating; mute windows (dialog, replay + 1 s, user holding a button); close resets the matcher; dialog failure ->
 * blocked -> panic; log flushed only while closed; ring overflow; determinism. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "integ.h"
#include "catalog.h"

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

/* ---------------- fake game memory (same world as the native-dialog probe tests) ---------------- */
#define FBASE   0x10000u
#define FSIZE   0x8000u
#define FILE_OFF 0x3000u
#define REC_OFF  0x3100u
#define NREC 3
#define CTX_ADDR 0x005C1878u
static unsigned char fmem[FSIZE];
static unsigned char ctxmem[0x1100];
static const unsigned char kOld[] = "Do you want to play again?";
#define OLD_SIZE 60u

static unsigned char *fx(void *u, unsigned addr, unsigned len, int write)
{
    (void)u; (void)write;
    if (addr < FBASE || addr + len > FBASE + FSIZE || addr + len < addr) return 0;
    return fmem + (addr - FBASE);
}

static void build_world(void)
{
    unsigned i, lang = 1;
    memset(fmem, 0, sizeof fmem);
    memset(ctxmem, 0, sizeof ctxmem);
    dm_wr32(fmem + FILE_OFF + 8, NREC);
    for (i = 0; i < NREC; i++) {
        static const unsigned ids[NREC] = {0x10, 0x1A, 0x30};
        unsigned char *r = fmem + REC_OFF + i * 0x60;
        dm_wr32(r, ids[i]);
        dm_wr32(r + 0x10 + lang * 8, 0x800 + i * 0x100);
        dm_wr32(r + 0x14 + lang * 8, i == 1 ? OLD_SIZE : 20u);
    }
    memcpy(fmem + FILE_OFF + 0x900, kOld, sizeof kOld - 1);
    memset(fmem + FILE_OFF + 0x900 + sizeof kOld - 1, 'x', OLD_SIZE - (sizeof kOld - 1));
    dm_wr32(ctxmem + 0x20, FBASE + FILE_OFF);
    dm_wr32(ctxmem + 0x24, FBASE + REC_OFF);
    dm_wr32(ctxmem + DM_OFF_LANG, lang);
    dm_wr32(ctxmem + DM_OFF_PLAY, FBASE);
    ctxmem[0x00] = 0x11; ctxmem[0x164] = 0x22; ctxmem[0xF40] = 0x33; ctxmem[0xFC0] = 0x55;
}

static unsigned g_loader_calls, g_loader_ret;
static unsigned fake_loader(void *u, unsigned ctx, unsigned id)
{
    (void)u; (void)ctx; (void)id;
    g_loader_calls++;
    ctxmem[0xF40] = 0xEE;                          /* dirties the core: the restore must put it back */
    return g_loader_ret;
}

/* ---------------- fake audio ---------------- */
typedef struct { unsigned start_ms, dur_ms; double hz; } tone;
#define MAX_TONES 64
static tone g_tones[MAX_TONES];
static int g_ntones;
static unsigned g_seed;
static unsigned long long g_sample;               /* absolute sample index of the next sample */

static void tones_clear(void) { g_ntones = 0; }
static void add_tone(unsigned start_ms, unsigned dur_ms, double hz)
{
    if (g_ntones < MAX_TONES) { g_tones[g_ntones].start_ms = start_ms; g_tones[g_ntones].dur_ms = dur_ms; g_tones[g_ntones].hz = hz; g_ntones++; }
}
static double midi_hz(double midi) { return 440.0 * pow(2.0, (midi - 69.0) / 12.0); }

/* whistles `song` (catalog notes, +7 semitones: 440..880 Hz) from t0: notes of note_ms with gap_ms silence */
static unsigned add_song(int song, unsigned t0, unsigned note_ms, unsigned gap_ms)
{
    static const int midi[5] = {62, 65, 69, 71, 74};
    const catalog_song *c = catalog_get(song);
    unsigned t = t0;
    int i;
    for (i = 0; i < c->length; i++) { add_tone(t, note_ms, midi_hz(midi[c->notes[i]] + 7)); t += note_ms + gap_ms; }
    return t;
}
static unsigned add_cents(const int *cents, int n, unsigned t0, unsigned note_ms, unsigned gap_ms)
{
    unsigned t = t0;
    int i;
    for (i = 0; i < n; i++) { add_tone(t, note_ms, midi_hz(cents[i] / 100.0)); t += note_ms + gap_ms; }
    return t;
}

static short sample_at(unsigned long long i)
{
    double ms = (double)i * 1000.0 / 8182.0, v = 0;
    int k;
    g_seed = g_seed * 1103515245u + 12345u;
    v = (double)((int)((g_seed >> 16) & 0x7F) - 64);          /* +-64 background noise */
    for (k = 0; k < g_ntones; k++) {
        const tone *t = &g_tones[k];
        if (ms >= t->start_ms && ms < t->start_ms + t->dur_ms) {
            double e = ms - t->start_ms, f = t->start_ms + t->dur_ms - ms, a = 4000.0;
            if (e < 10) a *= e / 10; if (f < 10) a *= f / 10;
            v += a * sin(2.0 * 3.14159265358979 * t->hz * (double)i / 8182.0);
        }
    }
    if (v > 32767) v = 32767; if (v < -32768) v = -32768;
    return (short)v;
}

/* ---------------- fake game + frame loop ---------------- */
typedef struct {
    int open;                 /* the user holds the ocarina out */
    int in_dialog;            /* the game is running a box (between our close and our restore) */
    int answers[4], nans, k;  /* 0 = Yes, 1 = No for the k-th box */
    unsigned t7;
    int stuck;                /* the box never reaches the choice */
    unsigned hold_from, hold_to;   /* the user holds A (real press) */
    unsigned other_state;     /* state byte while not open (0 = free) */
} fgame;

static ig_t ig;
static logbuf_t lb;
static unsigned g_edges, g_flush_open, g_flushes, g_analysed, g_mask_bad, g_onsets[32], g_nonsets, g_prev_mask;
static unsigned g_now;
static char g_log[200000];
static unsigned g_loglen;

static void reset_all(int dialogs)
{
    dlg_mem m;
    build_world();
    m.ctx = ctxmem; m.xlat = fx; m.user = 0;
    lb_init(&lb);
    ig_init(&ig, ctxmem, dialogs, &m, CTX_ADDR, fake_loader, 0, &lb);
    g_loader_calls = 0; g_loader_ret = 1; g_edges = 0; g_flush_open = 0; g_flushes = 0; g_analysed = 0;
    g_mask_bad = 0; g_nonsets = 0; g_prev_mask = 0; g_now = 0; g_loglen = 0; g_log[0] = 0;
    g_sample = 0; g_seed = 12345u;
    tones_clear();
}

static void game_step(fgame *G, unsigned t)
{
    unsigned char *s = &ctxmem[DM_OFF_STATE];
    if (G->in_dialog) {
        if (*s == 6) { G->in_dialog = 0; return; }                 /* our restore put the ocarina back */
        if (*s == 14) { *s = 0; return; }
        if (G->stuck) return;
        if (*s >= 1 && *s < 4) { (*s)++; return; }
        if (*s == 4) { *s = 7; dm_wr32(ctxmem + DM_OFF_CHOICE, 0); G->t7 = t; return; }
        if (*s == 7 && t - G->t7 >= 300) {                          /* the user moves the cursor ... */
            int a = G->k < G->nans ? G->answers[G->k] : 1;
            dm_wr32(ctxmem + DM_OFF_CHOICE, (unsigned)a);
            if (t - G->t7 >= 600) { g_edges++; G->k++; *s = 14; }  /* ... then presses A */
        }
        return;
    }
    if (*s == 14) { G->in_dialog = 1; *s = 0; return; }           /* our close of the ocarina message */
    *s = (unsigned char)(G->open ? 6 : G->other_state);
}

static void frames(fgame *G, unsigned t1)
{
    static short buf[8192];
    for (; g_now < t1; g_now += 33) {
        unsigned long long want = (unsigned long long)g_now * 8182u / 1000u;
        unsigned n = 0, out, mask = 0, hold = 0, rising;
        game_step(G, g_now);
        while (g_sample < want && n < sizeof buf / sizeof buf[0]) buf[n++] = sample_at(g_sample++);
        if (n) ig_push(&ig, buf, n, 0, n);
        if (g_now >= G->hold_from && g_now < G->hold_to) hold = 0x1;
        out = ig_frame(&ig, g_now, hold, g_edges, &mask);
        if (out & IG_OUT_ANALYSED) g_analysed++;
        if (mask && ctxmem[DM_OFF_STATE] != 6) g_mask_bad++;
        if (mask & ~INJ_ALLOWED_BUTTONS) g_mask_bad++;
        rising = mask & ~g_prev_mask;
        if (rising && g_nonsets < 32) g_onsets[g_nonsets++] = rising;
        g_prev_mask = mask;
        if (out & IG_OUT_FLUSH) {
            if (ig.open) g_flush_open++;
            g_flushes++;
            if (g_loglen + lb.len < sizeof g_log) { memcpy(g_log + g_loglen, lb.buf, lb.len); g_loglen += lb.len; g_log[g_loglen] = 0; }
            lb_clear(&lb);
        }
    }
}

/* the whole log so far (flushed + pending) */
static const char *logtext(void)
{
    static char all[220000];
    memcpy(all, g_log, g_loglen); memcpy(all + g_loglen, lb.buf, lb.len); all[g_loglen + lb.len] = 0;
    return all;
}
static int count_str(const char *s, const char *w) { int n = 0; const char *p = s; while ((p = strstr(p, w)) != 0) { n++; p++; } return n; }
static void dump_on_fail(int before) { if (fails != before) printf("---- log ----\n%s-------------\n", logtext()); }

static int onsets_are(int song)
{
    const catalog_song *c = catalog_get(song);
    int i;
    if ((int)g_nonsets != c->length) return 0;
    for (i = 0; i < c->length; i++) if (g_onsets[i] != catalog_button(song, i)) return 0;
    return 1;
}

static void check_restored(void)
{
    CHECK(ctxmem[0x00] == 0x11 && ctxmem[0x164] == 0x22 && ctxmem[0xF40] == 0x33 && ctxmem[0xFC0] == 0x55);
    CHECK(memcmp(fmem + FILE_OFF + 0x900, kOld, 19) == 0);
    CHECK(dm_rd32(fmem + REC_OFF + 0x60 + 0x14 + 8) == OLD_SIZE);
}

/* ---------------- tests ---------------- */
static void test_song_path(void)
{
    fgame G; int f0 = fails;
    memset(&G, 0, sizeof G); G.hold_from = G.hold_to = 0;
    reset_all(1);
    G.open = 1;
    add_song(SONG_ZELDA, 2000, 400, 100);
    frames(&G, 12000);
    CHECK(strstr(logtext(), "-> SONG 0 'Zelda's Lullaby'") != 0);
    CHECK(strstr(logtext(), "play t=") && strstr(logtext(), "via=SONG"));
    CHECK(onsets_are(SONG_ZELDA));
    CHECK(g_loader_calls == 0);
    CHECK(ig.st.songs == 1 && ig.st.plays == 1 && ig.st.asked == 0);
    CHECK(strstr(logtext(), "replay start") && strstr(logtext(), "replay end t=") && strstr(logtext(), "abort=0"));
    CHECK(g_mask_bad == 0);
    CHECK(g_flush_open == 0 && g_flushes == 0);          /* nothing written while the ocarina is open */
    CHECK(g_analysed > 20);
    /* close: the session line + stats, then ONE flush */
    G.open = 0;
    frames(&G, 13000);
    CHECK(g_flushes == 1 && lb.len == 0);
    CHECK(strstr(g_log, "sess#1 open") && strstr(g_log, "sess#1 close") && strstr(g_log, "song=1") && strstr(g_log, "play=1"));
    CHECK(strstr(g_log, "c/ms@ms="));                     /* debug aid: the notes heard (cents/duration@start) */
    dump_on_fail(f0);
}

static void test_determinism(void)
{
    static char first[220000];
    fgame G;
    memset(&G, 0, sizeof G);
    reset_all(1); G.open = 1; add_song(SONG_EPONA, 1500, 350, 120); frames(&G, 9000); G.open = 0; frames(&G, 10000);
    strcpy(first, logtext());
    memset(&G, 0, sizeof G);
    reset_all(1); G.open = 1; add_song(SONG_EPONA, 1500, 350, 120); frames(&G, 9000); G.open = 0; frames(&G, 10000);
    CHECK(strcmp(first, logtext()) == 0);
    CHECK(strstr(first, "-> SONG 4 'Epona's Song'") != 0);
}

static void test_all_songs(void)
{
    int s, ok = 0;
    for (s = 0; s < CATALOG_COUNT; s++) {
        fgame G; int f0 = fails;
        memset(&G, 0, sizeof G);
        reset_all(1); G.open = 1;
        add_song(s, 1500, 400, 300);   /* repeated notes (Nocturne, Serenade) need a ~0.3 s pause to split */
        frames(&G, 14000);
        if (ig.st.songs == 1 && onsets_are(s)) ok++;
        else if (ig.st.ambig) printf("  song %d -> asked instead of played (%s)\n", s, catalog_get(s)->name);
        else printf("  song %d not recognised\n%s", s, logtext());
        CHECK(g_mask_bad == 0);
        (void)f0;
    }
    printf("whistled songs recognised end-to-end (synthetic): %d/12\n", ok);
    CHECK(ok == 12);
}

static const int kRise[6] = {7400, 7700, 8100, 7400, 7700, 8100};   /* Saria/Storms look-alike */

static void test_ambiguous_yes(void)
{
    fgame G; int f0 = fails;
    memset(&G, 0, sizeof G);
    reset_all(1); G.open = 1; G.answers[0] = 0; G.nans = 1;
    add_cents(kRise, 6, 1500, 350, 100);
    frames(&G, 14000);
    CHECK(strstr(logtext(), "-> AMBIG 8 'Saria's Song' 9 'Song of Storms'") != 0);
    CHECK(strstr(logtext(), "ask t=") && strstr(logtext(), "song=8 'Saria's Song' (1/2)"));
    CHECK(g_loader_calls == 1);
    CHECK(strstr(logtext(), "answer t=") && strstr(logtext(), "res=YES"));
    CHECK(strstr(logtext(), "via=YES"));
    CHECK(onsets_are(SONG_SARIA));
    CHECK(ig.st.asked == 1 && ig.st.yes == 1 && ig.st.plays == 1);
    CHECK(g_mask_bad == 0 && g_flush_open == 0);
    check_restored();
    dump_on_fail(f0);
}

static void test_ambiguous_no_then_yes(void)
{
    fgame G; int f0 = fails;
    memset(&G, 0, sizeof G);
    reset_all(1); G.open = 1; G.answers[0] = 1; G.answers[1] = 0; G.nans = 2;
    add_cents(kRise, 6, 1500, 350, 100);
    frames(&G, 16000);
    CHECK(g_loader_calls == 2);
    CHECK(strstr(logtext(), "(1/2)") && strstr(logtext(), "song=9 'Song of Storms' (2/2)"));
    CHECK(count_str(logtext(), "res=NO") >= 1 && strstr(logtext(), "res=YES"));
    CHECK(onsets_are(SONG_STORMS));
    CHECK(ig.st.no == 1 && ig.st.yes == 1);
    check_restored();
    dump_on_fail(f0);
}

static void test_all_refused(void)
{
    fgame G; int f0 = fails;
    memset(&G, 0, sizeof G);
    reset_all(1); G.open = 1; G.answers[0] = 1; G.answers[1] = 1; G.nans = 2;
    add_cents(kRise, 6, 1500, 350, 100);
    frames(&G, 16000);
    CHECK(g_loader_calls == 2);
    CHECK(strstr(logtext(), "all refused") != 0);
    CHECK(g_nonsets == 0 && ig.st.plays == 0);
    check_restored();
    /* listening resumes: a clear song afterwards is played */
    add_song(SONG_TIME, 17000, 400, 100);
    frames(&G, 26000);
    CHECK(onsets_are(SONG_TIME));
    dump_on_fail(f0);
}

static void test_timeout(void)
{
    static const int odd[4] = {7000, 7900, 7150, 8350};
    fgame G; int f0 = fails;
    memset(&G, 0, sizeof G);
    reset_all(1); G.open = 1;
    add_cents(odd, 4, 1500, 350, 100);
    frames(&G, 8000);
    CHECK(strstr(logtext(), "-> TIMEOUT") != 0);
    CHECK(ig.st.timeouts == 1 && g_nonsets == 0 && g_loader_calls == 0);
    dump_on_fail(f0);
}

static void test_closed_gating(void)
{
    static const unsigned others[5] = {0, 1, 7, 13, 14};
    int k;
    for (k = 0; k < 5; k++) {
        fgame G; int f0 = fails;
        memset(&G, 0, sizeof G);
        reset_all(1); G.open = 0; G.other_state = others[k];
        if (others[k] == 14) G.other_state = 13;              /* 14 is "our close" in the fake; 13 stands for it */
        add_song(SONG_ZELDA, 1500, 400, 100);
        add_cents(kRise, 6, 6000, 350, 100);
        frames(&G, 14000);
        CHECK(g_analysed == 0 && g_nonsets == 0 && g_loader_calls == 0 && ig.st.hops == 0);
        CHECK(strstr(logtext(), "phrase") == 0 && strstr(logtext(), "sess#") == 0);
        dump_on_fail(f0);
    }
}

static void test_mute_replay_echo(void)
{
    /* the game's own ocarina sound during the replay (an echo of the song in the mic) and up to 1 s after it is not
     * heard; a new whistle later is */
    fgame G; int f0 = fails;
    unsigned t;
    memset(&G, 0, sizeof G);
    reset_all(1); G.open = 1;
    t = add_song(SONG_ZELDA, 1500, 400, 100);              /* ends ~4500, SONG right at the last note */
    add_song(SONG_ZELDA, t + 1600, 180, 20);               /* the replay sound (6 x 200 ms, 1.5 s later) */
    frames(&G, 14000);
    CHECK(ig.st.songs == 1 && ig.st.plays == 1);
    add_song(SONG_ZELDA, 15000, 400, 100);
    frames(&G, 24000);
    CHECK(ig.st.songs == 2 && ig.st.plays == 2);
    dump_on_fail(f0);
}

static void test_mute_dialog_audio(void)
{
    /* whistling a clear song while our dialog is on screen does nothing; after Yes only the chosen song plays */
    fgame G; int f0 = fails;
    memset(&G, 0, sizeof G);
    reset_all(1); G.open = 1; G.answers[0] = 1; G.answers[1] = 1; G.nans = 2;
    add_cents(kRise, 6, 1500, 350, 100);                  /* -> ambiguous at ~4200 */
    add_song(SONG_TIME, 4800, 300, 60);                   /* during the dialogs */
    frames(&G, 15000);
    CHECK(ig.st.songs == 0 && g_nonsets == 0);
    CHECK(g_loader_calls == 2);
    dump_on_fail(f0);
}

static void test_mute_buttons(void)
{
    /* the user plays by hand (holds A) while a song is in the mic: nothing is detected, nothing injected */
    fgame G; int f0 = fails;
    memset(&G, 0, sizeof G);
    reset_all(1); G.open = 1; G.hold_from = 1400; G.hold_to = 4800;
    add_song(SONG_ZELDA, 1500, 400, 100);
    frames(&G, 9000);
    CHECK(ig.st.songs == 0 && g_nonsets == 0 && ig.st.notes == 0);
    dump_on_fail(f0);
}

static void test_close_resets(void)
{
    /* first half, close, reopen, second half: never a song */
    static const int z1[3] = {7800, 8100, 7600}, z2[3] = {7800, 8100, 7600};
    fgame G; int f0 = fails;
    memset(&G, 0, sizeof G);
    reset_all(1); G.open = 1;
    add_cents(z1, 3, 1500, 400, 100);
    frames(&G, 3000);
    G.open = 0; frames(&G, 3600);
    G.open = 1;
    add_cents(z2, 3, 3700, 400, 100);
    frames(&G, 9000);
    CHECK(ig.st.songs == 0 && g_nonsets == 0);
    CHECK(strstr(logtext(), "-> CLOSED") != 0);
    CHECK(ig.sess == 2);
    dump_on_fail(f0);
}

static void test_dialog_fail_panic(void)
{
    fgame G; int f0 = fails;
    memset(&G, 0, sizeof G);
    reset_all(1); G.open = 1; g_loader_ret = 0;           /* the loader refuses: dialog FAIL */
    add_cents(kRise, 6, 1500, 350, 100);
    frames(&G, 9000);
    CHECK(g_loader_calls == 1 && ig.dlgfails == 1 && !ig.panic);
    check_restored();
    /* blocked until close: another ambiguous phrase is not asked */
    add_cents(kRise, 6, 9500, 350, 100);
    frames(&G, 15000);
    CHECK(g_loader_calls == 1 && strstr(logtext(), "ask skipped"));
    /* reopen: asked again, fails again -> panic, then nothing at all */
    G.open = 0; frames(&G, 16000);
    G.open = 1;
    add_cents(kRise, 6, 16500, 350, 100);
    frames(&G, 22000);
    CHECK(g_loader_calls == 2 && ig.panic);
    CHECK(strstr(logtext(), "PANIC") != 0);
    add_song(SONG_ZELDA, 23000, 400, 100);
    frames(&G, 32000);
    CHECK(g_nonsets == 0 && ig.st.songs == 0);
    G.open = 0; frames(&G, 33000);
    CHECK(g_flush_open == 0 && g_flushes >= 1);
    dump_on_fail(f0);
}

static void test_dialog_stuck(void)
{
    /* the box never reaches the choice: FAIL after 10 s (no choice), core restored, blocked */
    fgame G; int f0 = fails;
    memset(&G, 0, sizeof G);
    reset_all(1); G.open = 1; G.stuck = 1;
    add_cents(kRise, 6, 1500, 350, 100);
    frames(&G, 20000);
    CHECK(g_loader_calls == 1 && ig.dlgfails == 1 && ig.dlg_blocked);
    CHECK(ctxmem[DM_OFF_STATE] == 6);
    check_restored();
    dump_on_fail(f0);
}

static void test_no_dialog_build(void)
{
    /* dialogs unavailable (loader not found): ambiguous phrases are logged and skipped, songs still play */
    fgame G; int f0 = fails;
    memset(&G, 0, sizeof G);
    reset_all(0); G.open = 1;
    add_cents(kRise, 6, 1500, 350, 100);
    add_song(SONG_SUN, 7000, 400, 100);
    frames(&G, 16000);
    CHECK(strstr(logtext(), "why=no_dialog") && g_loader_calls == 0);
    CHECK(onsets_are(SONG_SUN));
    dump_on_fail(f0);
}

static void test_overflow_and_or(void)
{
    static short big[6000];
    unsigned mask, i;
    fgame G; int f0 = fails;
    memset(&G, 0, sizeof G);
    reset_all(1); G.open = 1;
    frames(&G, 2000);
    for (i = 0; i < 6000; i++) big[i] = (short)(i * 7);
    ig_push(&ig, big, 6000, 0, 6000);                     /* a 0.7 s burst at once (game stalled) */
    ig_frame(&ig, 2000, 0, g_edges, &mask);
    CHECK(ig.st.overflows == 1 && strstr(logtext(), "overflow"));
    /* OR-only merge: a real press is never removed, only A X Y L R are added */
    CHECK(inj_merge(0x1u, 0x400u, 100u, 200u) == 0x401u);
    CHECK(inj_merge(0x4u, 0xFFFFFFFFu, 100u, 200u) == (0x4u | INJ_ALLOWED_BUTTONS));
    CHECK(inj_merge(0x8u, 0, 100u, 200u) == 0x8u);
    dump_on_fail(f0);
}

static void test_slow_anomalies_panic(void)
{
    fgame G; int f0 = fails;
    memset(&G, 0, sizeof G);
    reset_all(1); G.open = 1;
    frames(&G, 1000);
    ig_cost(&ig, 1000, 50000, 0); ig_cost(&ig, 1100, 2000, 1); ig_cost(&ig, 1200, 45000, 0);
    CHECK(!ig.panic && ig.anomalies == 2);
    ig_anomaly(&ig, 1300, IG_AN_MIC, 7);
    frames(&G, 1500);
    CHECK(ig.panic);
    g_analysed = 0;
    add_song(SONG_ZELDA, 2000, 400, 100);
    frames(&G, 9000);
    CHECK(g_nonsets == 0 && g_analysed == 0);
    dump_on_fail(f0);
}

int main(void)
{
    test_song_path();
    test_determinism();
    test_all_songs();
    test_ambiguous_yes();
    test_ambiguous_no_then_yes();
    test_all_refused();
    test_timeout();
    test_closed_gating();
    test_mute_replay_echo();
    test_mute_dialog_audio();
    test_mute_buttons();
    test_close_resets();
    test_dialog_fail_panic();
    test_dialog_stuck();
    test_no_dialog_build();
    test_overflow_and_or();
    test_slow_anomalies_panic();
    printf("integ: %d checks, %d failures; sizeof(ig_t)=%u\n", checks, fails, (unsigned)sizeof(ig_t));
    return fails ? 1 : 0;
}
