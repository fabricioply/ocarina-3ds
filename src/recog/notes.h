/* Note segmenter + noise-floor gate for the integer pitch detector. Pure C99, integer only, no libc,
 * no allocation, deterministic. Feed one pitch_result per hop; it emits note-on / note-off events.
 * Times are in HOPS (the caller knows the hop length: 1024 samples = 125 ms, 512 = 62.5 ms). */
#ifndef NOTES_H
#define NOTES_H

#include "pitch.h"

#ifndef NOTES_MIN_HOPS
#define NOTES_MIN_HOPS   3     /* consecutive stable voiced hops needed to start (or change) a note */
#endif
#ifndef NOTES_MAX_GAP
#define NOTES_MAX_GAP    1     /* unvoiced hops tolerated inside a note / candidate */
#endif
#define NOTES_HYST_CENTS 60    /* leave the current note only when the pitch is further than this from its centre */
#define NOTES_STABLE_CENTS 60  /* candidate hops must stay within this of each other */
#define NOTES_FLOOR_N    128   /* RMS history length for the noise floor */
#define NOTES_FLOOR_PCT  15    /* percentile of the history taken as the noise floor */
#define NOTES_GATE_NUM   5     /* gate = floor * NUM / DEN ... */
#define NOTES_GATE_DEN   2     /* ... = 2.5x the noise floor */
#define NOTES_GATE_MIN   80    /* absolute minimum gate (RMS) */

#define NOTE_ON  1
#define NOTE_OFF 2

typedef struct {
    int type;        /* NOTE_ON / NOTE_OFF */
    int midi;        /* MIDI note number (A4 = 69 = 440 Hz) */
    int cents;       /* pitch in cents above MIDI 0 (midi*100 = exact semitone), centre of the note */
    int hz;          /* ON: Hz of the first stable hop; OFF: 0 */
    int start_hop;   /* hop index of the first hop of the note */
    int end_hop;     /* OFF only: hop index after the last voiced hop */
} note_event;

typedef struct {
    int hop;                     /* hops seen so far */
    int rms[NOTES_FLOOR_N]; int nrms, rpos;
    int floor_rms;               /* current noise-floor estimate */
    int active, center, midi, start_hop, last_voiced;
    int miss;
    int cand_n, cand_c, cand_start, cand_hz;
} notes_state;

void notes_init(notes_state *s);
/* RMS below which pitch_detect need not run for the next hop. */
int  notes_gate(const notes_state *s);
/* hz > 0 -> cents above MIDI 0 (440 Hz = 6900); 0 for hz <= 0. */
int  notes_hz_to_cents(int hz);
/* Feeds one hop; writes up to 2 events (OFF then ON) to out[]; returns how many. */
int  notes_feed(notes_state *s, const pitch_result *r, note_event out[2]);
/* End of stream: closes an active note (0 or 1 events). */
int  notes_flush(notes_state *s, note_event out[1]);

#endif
