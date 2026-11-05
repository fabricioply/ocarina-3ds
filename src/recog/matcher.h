/* Song matcher: integer-only port of the OcarinaMod PC prototype's song_matcher.cpp (assisted mode by default).
 * Pure C99, no float, no libc, no allocation. Pitches are CENTS (MIDI*100, e.g. note_event.cents), times are ms.
 * Matching is by intervals (transposition invariant); tolerances are in cents (assisted: 150, precise: 35).
 * Scores are permille (0..1000); thresholds: song >= 800 with a lead of >= 100 over the runner-up.
 *
 * Flow: feed every COMPLETED note (the segmenter's NOTE_OFF: centre cents, start/end) with matcher_note(), and call
 * matcher_poll() every frame. A SONG is reported as soon as the last note arrives. The saria/storms look-alike
 * (rising triplets) is reported as AMBIGUOUS at once. Any other loose "looks like" phrase is offered (AMBIGUOUS)
 * after MATCHER_FINISH_MS of silence (PC: 0.9 s). The caller asks the candidates ONE at a time ("<name>?"), then
 * calls matcher_reset() when the attempt is over (accepted, refused all, or retry). */
#ifndef MATCHER_H
#define MATCHER_H

#include "catalog.h"

#define MATCHER_MAX_NOTES   32
#define MATCHER_FINISH_MS   900     /* silence after the last note that triggers the loose "offer" */
#define MATCHER_MAX_CAND    3

enum { MATCH_NONE = 0, MATCH_SONG = 1, MATCH_AMBIGUOUS = 2, MATCH_TIMEOUT = 3 };

typedef struct {
    int cents;      /* pitch centre of the note, cents above MIDI 0 */
    int start_ms;
    int end_ms;
} matcher_note_ev;

typedef struct {
    int state;                        /* MATCH_* */
    int song;                         /* SONG: the id; AMBIGUOUS: first candidate; else -1 */
    int count;                        /* number of valid candidates[] (SONG: 1) */
    int candidates[MATCHER_MAX_CAND]; /* song ids, best first */
    int score;                        /* permille */
} matcher_result;

typedef struct {
    matcher_note_ev notes[MATCHER_MAX_NOTES];
    int n;
    int assisted;
    int latched;                    /* a SONG was reported; ignore notes until matcher_reset */
    unsigned enabled;               /* bit per song id; default all (use to restrict to learned songs) */
    int has_pending;
    matcher_result pending;
    int finish_due;                 /* notes arrived since the last finish */
    int last_end_ms;
} matcher_state;

/* assisted != 0: loose tolerance + loose offers (PC default). 0: precise (35 cents, no offers). */
void matcher_init(matcher_state *m, int assisted);
/* Forget notes / latch / pending result (keeps assisted and enabled). */
void matcher_reset(matcher_state *m);
/* Bit per song id (1u << id) of the songs that may be reported. */
void matcher_set_enabled(matcher_state *m, unsigned mask);
/* Feed one completed note. Notes shorter than 60 ms are ignored; a gap > 1250 ms or an overlap starts a new phrase. */
void matcher_note(matcher_state *m, const matcher_note_ev *ev);
/* Returns 1 and fills *out when something is to be reported (MATCH_SONG / MATCH_AMBIGUOUS / MATCH_TIMEOUT), else 0
 * with out->state = MATCH_NONE. `sounding` != 0 while a note is currently active (blocks the silence timer).
 * MATCH_TIMEOUT: the phrase went quiet for MATCHER_FINISH_MS with notes that resemble nothing; notes are dropped. */
int  matcher_poll(matcher_state *m, int now_ms, int sounding, matcher_result *out);

#endif
