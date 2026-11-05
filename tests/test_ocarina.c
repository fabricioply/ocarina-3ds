/* Host tests for pitch.c + notes.c: synthetic signals, plus optional checks on two private microphone recordings.
 * Deterministic (fixed LCG noise). The detector itself is integer only; <math.h> is used here only to make signals.
 *
 * The recording checks were tuned on two 20 s takes captured on a console (rec1.raw: background noise then six
 * whistled notes; rec2.raw: noise and a click burst; raw PCM16 little-endian mono at 8182 Hz). Those recordings are a
 * person's voice/whistle and are NOT distributed. Set OCARINA_RECORDINGS_DIR to a folder that holds them to run the
 * checks; otherwise they are skipped and the synthetic tests still decide the result. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pitch.h"
#include "notes.h"

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static unsigned lcg = 12345u;
static int noise(int amp)
{
    lcg = lcg * 1664525u + 1013904223u;
    return (int)(((lcg >> 8) & 0xFFFFu) * (2 * amp + 1) / 65536u) - amp;
}

/* sine with optional harmonics (h2, h3 as percent of the fundamental amplitude) */
static void synth(short *x, int n, double f0, double phase, int amp, int h2, int h3, int namp, int dc)
{
    int i;
    for (i = 0; i < n; i++) {
        double t = ((double)i + phase) / PITCH_RATE, w = 2.0 * 3.14159265358979323846 * f0 * t;
        double v = amp * (sin(w) + h2 / 100.0 * sin(2 * w + 0.7) + h3 / 100.0 * sin(3 * w + 1.9));
        int iv = (int)floor(v + 0.5) + dc;
        if (namp) iv += noise(namp);
        x[i] = (short)iv;
    }
}

static int within(int hz, double f, double pct) { return fabs(hz - f) <= pct / 100.0 * f; }

/* ---------- synthetic: pitch ---------- */
static void test_synth_pitch(void)
{
    static const double fs[] = {120, 200, 330, 440, 587, 700, 880, 987, 1175, 1318, 1500, 1600};
    unsigned k;
    short x[PITCH_N];
    pitch_result pr;
    for (k = 0; k < sizeof fs / sizeof fs[0]; k++) {
        double f = fs[k];
        int hz, ph;
        for (ph = 0; ph < 3; ph++) {                       /* pure tone, noise, dc, three phases */
            synth(x, PITCH_N, f, ph * 3.3, 3000, 0, 0, 150, 200);
            hz = pitch_detect(x, &pr, 0);
            CHECK(within(hz, f, 2.0));
            CHECK(pr.clarity > PITCH_VOICED_Q15);
        }
        if (f <= 1320) {   /* above ~1.3 kHz the period is < 6.3 samples: harmonics alias and octave-down errors appear */
            synth(x, PITCH_N, f, 1.0, 3000, 60, 30, 100, 0);   /* strong harmonics: must stay on the fundamental */
            hz = pitch_detect(x, &pr, 0);
            CHECK(within(hz, f, 1.5));
        }
        if (f <= 700) {
            synth(x, PITCH_N, f, 1.0, 800, 120, 0, 60, 0);     /* 2nd harmonic stronger than the fundamental */
            hz = pitch_detect(x, &pr, 0);
            CHECK(within(hz, f, 1.5) || within(hz, 2 * f, 1.5));   /* ambiguous by nature, but never 1/2 or 1/3 */
        }
    }
    /* loud clipping-level tone is scaled internally, still right */
    synth(x, PITCH_N, 700, 0, 30000, 0, 0, 0, 0);
    CHECK(within(pitch_detect(x, &pr, 0), 700, 1.0));
    /* silence / tiny noise: not analysed */
    memset(x, 0, sizeof x);
    CHECK(pitch_detect(x, &pr, 0) == 0 && pr.ran == 0);
    synth(x, PITCH_N, 700, 0, 20, 0, 0, 5, 0);
    CHECK(pitch_detect(x, &pr, 0) == 0 && pr.ran == 0);
    /* gate argument is honoured */
    synth(x, PITCH_N, 700, 0, 500, 0, 0, 0, 0);
    CHECK(pitch_detect(x, &pr, 400) == 0 && pr.ran == 0 && pr.rms > 300 && pr.rms < 400);
    /* white noise is not voiced */
    {
        int i, voiced = 0, rep;
        for (rep = 0; rep < 40; rep++) {
            for (i = 0; i < PITCH_N; i++) x[i] = (short)noise(900);
            voiced += pitch_detect(x, &pr, 0) != 0;
        }
        CHECK(voiced == 0);
    }
}

/* ---------- cents ---------- */
static void test_cents(void)
{
    int hz;
    CHECK(notes_hz_to_cents(440) == 6900);
    CHECK(notes_hz_to_cents(880) == 8100);
    CHECK(notes_hz_to_cents(220) == 5700);
    CHECK(notes_hz_to_cents(0) == 0);
    for (hz = 80; hz <= 2000; hz++) {
        double ex = 6900 + 1200 * log((double)hz / 440.0) / log(2.0);
        int c = notes_hz_to_cents(hz);
        if (fabs(c - ex) > 1.0) { CHECK(0); break; }
    }
}

/* ---------- synthetic: segmenter ---------- */
typedef struct { note_event ev[64]; int n; } evlist;

static void feed_hz(notes_state *s, evlist *L, int hz)
{
    pitch_result r; note_event e[2]; int k, ne;
    memset(&r, 0, sizeof r);
    r.hz = hz; r.rms = hz ? 400 : 60; r.ran = hz != 0; r.clarity = hz ? 30000 : 0;
    ne = notes_feed(s, &r, e);
    for (k = 0; k < ne; k++) L->ev[L->n++] = e[k];
}

static void test_segmenter(void)
{
    notes_state s; evlist L; int i;
    notes_init(&s); L.n = 0;
    for (i = 0; i < 5; i++) feed_hz(&s, &L, 700 + (i & 1) * 6);      /* steady 700 */
    for (i = 0; i < 3; i++) feed_hz(&s, &L, 0);
    CHECK(L.n == 2 && L.ev[0].type == NOTE_ON && L.ev[1].type == NOTE_OFF);
    CHECK(L.ev[0].midi == 77 && L.ev[0].start_hop == 0 && L.ev[1].end_hop == 5);

    notes_init(&s); L.n = 0;                                         /* 2 hops only: no note */
    for (i = 0; i < 2; i++) feed_hz(&s, &L, 700);
    for (i = 0; i < 4; i++) feed_hz(&s, &L, 0);
    CHECK(L.n == 0);

    notes_init(&s); L.n = 0;                                         /* a single dropped hop does not split a note */
    for (i = 0; i < 4; i++) feed_hz(&s, &L, 880);
    feed_hz(&s, &L, 0);
    for (i = 0; i < 4; i++) feed_hz(&s, &L, 880);
    for (i = 0; i < 3; i++) feed_hz(&s, &L, 0);
    CHECK(L.n == 2 && L.ev[0].midi == 81);

    notes_init(&s); L.n = 0;                                         /* octave glitch inside a note is ignored */
    for (i = 0; i < 4; i++) feed_hz(&s, &L, 700);
    feed_hz(&s, &L, 175);
    for (i = 0; i < 4; i++) feed_hz(&s, &L, 702);
    for (i = 0; i < 3; i++) feed_hz(&s, &L, 0);
    CHECK(L.n == 2 && L.ev[0].midi == 77 && L.ev[1].end_hop == 9);

    notes_init(&s); L.n = 0;                                         /* legato change 700 -> 880: off+on, hysteresis */
    for (i = 0; i < 4; i++) feed_hz(&s, &L, 700);
    for (i = 0; i < 4; i++) feed_hz(&s, &L, 880);
    for (i = 0; i < 3; i++) feed_hz(&s, &L, 0);
    CHECK(L.n == 4);
    CHECK(L.ev[0].type == NOTE_ON && L.ev[1].type == NOTE_OFF && L.ev[2].type == NOTE_ON && L.ev[3].type == NOTE_OFF);
    CHECK(L.ev[1].end_hop == 4 && L.ev[2].start_hop == 4 && L.ev[2].midi == 81);

    notes_init(&s); L.n = 0;                                         /* slow drift of 50 cents stays one note */
    {
        static const int d[] = {700, 703, 706, 709, 712, 715, 718, 721, 724, 727, 730};
        for (i = 0; i < 11; i++) feed_hz(&s, &L, d[i]);
        for (i = 0; i < 3; i++) feed_hz(&s, &L, 0);
        CHECK(L.n == 2);
    }

    /* noise floor tracking: floor follows the quiet level, gate = 2.5x, never below the minimum */
    notes_init(&s);
    for (i = 0; i < 40; i++) { pitch_result r; note_event e[2]; memset(&r, 0, sizeof r); r.rms = 100; notes_feed(&s, &r, e); }
    CHECK(notes_gate(&s) == 250);
    for (i = 0; i < 40; i++) { pitch_result r; note_event e[2]; memset(&r, 0, sizeof r); r.rms = 20; notes_feed(&s, &r, e); }
    CHECK(notes_gate(&s) >= NOTES_GATE_MIN);
}

/* ---------- real recordings (optional, see the top of the file) ---------- */
static short *load_raw(const char *dir, const char *name, size_t *n)
{
    char path[512]; FILE *f; short *b; size_t got;
    if (strlen(dir) + strlen(name) + 2 > sizeof path) return 0;
    sprintf(path, "%s/%s", dir, name);
    f = fopen(path, "rb");
    if (!f) { printf("cannot open %s\n", path); return 0; }
    b = (short *)malloc(8182 * 40 * 2);
    got = fread(b, 2, 8182 * 40, f);
    fclose(f);
    *n = got;
    return b;
}

typedef struct {
    evlist L;
    int hz[2048], voiced[2048], nh;     /* per-hop detections */
    unsigned long long checksum;
} run_t;

static void run_file(const short *buf, size_t n, int hop, run_t *R)
{
    notes_state s; size_t p; short w[PITCH_N]; pitch_result r; note_event e[2]; int k, ne;
    notes_init(&s); R->L.n = 0; R->nh = 0; R->checksum = 1469598103934665603ull;
    for (p = 0; p + PITCH_N <= n; p += (size_t)hop) {
        memcpy(w, buf + p, sizeof w);
        pitch_detect(w, &r, notes_gate(&s));
        R->hz[R->nh] = r.hz; R->nh++;
        R->checksum = (R->checksum ^ (unsigned)(r.hz * 65536 + r.clarity)) * 1099511628211ull;
        ne = notes_feed(&s, &r, e);
        for (k = 0; k < ne; k++) R->L.ev[R->L.n++] = e[k];
    }
    if (notes_flush(&s, e)) R->L.ev[R->L.n++] = e[0];
}

static double hop_t(int hop_idx, int hop) { return (double)hop_idx * hop / PITCH_RATE; }

static int count_on(const run_t *R, int hop, double t0, double t1)
{
    int i, c = 0;
    for (i = 0; i < R->L.n; i++)
        if (R->L.ev[i].type == NOTE_ON) {
            double t = hop_t(R->L.ev[i].start_hop, hop);
            if (t >= t0 && t < t1) c++;
        }
    return c;
}

/* expected whistled notes found by looking at the per-hop tables (see the report) */
static const struct { double t0, t1, f0, f1; } whistle[] = {
    {14.4, 15.4, 695, 725},     /* ~708 Hz */
    {15.4, 16.0, 950, 995},     /* ~965 Hz */
    {16.0, 17.4, 665, 700},     /* ~680 Hz */
    {17.4, 18.4, 745, 800},     /* ~770 Hz */
    {18.4, 19.0, 925, 975},     /* ~945 Hz */
    {19.2, 19.9, 685, 720},     /* ~705 Hz */
};

static void test_recordings(void)
{
    size_t n1 = 0, n2 = 0;
    const char *dir = getenv("OCARINA_RECORDINGS_DIR");
    short *rec1, *rec2;
    static run_t A, B, C;
    int hops[2] = {512, 1024}, hi, i, j;

    if (!dir || !*dir) {
        printf("SKIP: recording checks skipped (OCARINA_RECORDINGS_DIR not set; the recordings are not distributed)\n");
        return;
    }
    rec1 = load_raw(dir, "rec1.raw", &n1);
    rec2 = load_raw(dir, "rec2.raw", &n2);
    if (!rec1 || !rec2) {
        printf("SKIP: recording checks skipped (rec1.raw/rec2.raw not found in OCARINA_RECORDINGS_DIR)\n");
        free(rec1); free(rec2);
        return;
    }
    CHECK(n1 == 163840u && n2 == 163840u);

    for (hi = 0; hi < 2; hi++) {
        int hop = hops[hi];
        /* rec2 is background noise + a click burst: no notes at all */
        run_file(rec2, n2, hop, &B);
        CHECK(B.L.n == 0);
        /* rec1: no notes in the noise-only stretches (0-5.5 s and 6.4-12.4 s) */
        run_file(rec1, n1, hop, &A);
        CHECK(count_on(&A, hop, 0.0, 5.5) == 0);
        CHECK(count_on(&A, hop, 6.4, 12.4) == 0);
        /* events are well formed: alternating ON/OFF, durations positive */
        for (i = 0; i < A.L.n; i++) {
            CHECK(A.L.ev[i].type == ((i & 1) ? NOTE_OFF : NOTE_ON));
            if (i & 1) CHECK(A.L.ev[i].end_hop > A.L.ev[i].start_hop);
        }
        /* determinism: second run identical */
        run_file(rec1, n1, hop, &C);
        CHECK(C.L.n == A.L.n && C.checksum == A.checksum && memcmp(C.L.ev, A.L.ev, sizeof(note_event) * (size_t)A.L.n) == 0);
        /* no octave jumps between consecutive voiced hops of the whistled region */
        for (i = 1; i < A.nh; i++) {
            double t = hop_t(i, hop);
            if (t >= 14.4 && t < 19.9 && A.hz[i] && A.hz[i - 1]) {
                double ratio = (double)A.hz[i] / A.hz[i - 1];
                CHECK(!(ratio > 1.8 && ratio < 2.2) && !(ratio > 0.45 && ratio < 0.55));
                CHECK(ratio > 0.7 && ratio < 1.45);        /* nothing wilder than a 6-semitone step per hop here */
            }
        }
        /* whistled notes (hop 512 resolves all six, hop 1024 the four longest) */
        {
            int found = 0;
            for (j = 0; j < (int)(sizeof whistle / sizeof whistle[0]); j++)
                for (i = 0; i < A.L.n; i++)
                    if (A.L.ev[i].type == NOTE_ON) {
                        double t = hop_t(A.L.ev[i].start_hop, hop);
                        if (t >= whistle[j].t0 && t < whistle[j].t1 &&
                            A.L.ev[i].hz >= whistle[j].f0 && A.L.ev[i].hz <= whistle[j].f1) { found++; break; }
                    }
            CHECK(hop == 512 ? found == 6 : found >= 4);
            CHECK(count_on(&A, hop, 14.4, 19.9) == (hop == 512 ? 6 : 4));
            printf("  hop %4d: rec1 notes in whistle region found %d, total ON events in rec1 %d, rec2 %d\n",
                   hop, found, count_on(&A, hop, 0, 21), count_on(&B, hop, 0, 21));
        }
    }
    free(rec1); free(rec2);
}

int main(void)
{
    test_synth_pitch();
    test_cents();
    test_segmenter();
    test_recordings();
    printf("%d checks, %d failures\n", checks, fails);
    return fails ? 1 : 0;
}
