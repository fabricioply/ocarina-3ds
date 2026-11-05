/* Probe9 injection logic: trigger state machine (ocarina open -> wait -> replay a fixed song -> settle -> repeat)
 * and the OR-merge of the replay mask into the game's pad "hold" word. Pure C99, no libc, no allocation, no mutable
 * globals (all state lives in inj_t, owned by the caller). Host-testable. */
#ifndef INJECT_H
#define INJECT_H

#include "catalog.h"

#define INJ_OSTATE_OPEN       6        /* msgCtx+0xF38 value while the ocarina session is open */
#define INJ_TRIGGER_DELAY_MS  3000     /* ocarina seen open -> injection start (the "3 s" of the brief) */
#define INJ_PERIOD_MS         20000    /* injection start -> next injection start while it stays open */
#define INJ_SETTLE_MS         1500     /* after the last note: observe, then write the end record */
#define INJ_SAMPLE_MS         3000     /* second state sample: 3 s after the injection start */
#define INJ_MAX_AGE_TICKS     40000000u /* a mask older than ~149 ms (268111856 ticks/s) is ignored by the pad hook */
#define INJ_ALLOWED_BUTTONS   0x00000F01u /* A X Y L R: the only bits ever injected (same rule as game_input.cpp) */
#define INJ_MAX_TRANS         8
#define INJ_NONE              0xFFFFFFFFu

enum { INJ_IDLE = 0, INJ_WAIT = 1, INJ_REPLAY = 2, INJ_SETTLE = 3 };

/* events returned by inj_step (bit flags; the caller does at most one SD append per call) */
#define INJ_EV_OPENED       0x01u   /* ocarina state became 6 */
#define INJ_EV_START        0x02u   /* injection starts (delay timer begins) */
#define INJ_EV_NOTES_BEGIN  0x04u   /* first note is now being held */
#define INJ_EV_NOTES_END    0x08u   /* last note released */
#define INJ_EV_SETTLED      0x10u   /* run record final (settle time elapsed, or aborted / ocarina closed) */
#define INJ_EV_CLOSED       0x20u   /* ocarina left state 6 */
#define INJ_EV_ABORT        0x40u   /* closed before the notes finished */

typedef struct {
    int      phase;
    int      song;
    int      manual;                /* 1: never starts by itself, only after inj_go() (probe10); no periodic repeat */
    int      go;                    /* start requested (manual mode) */
    unsigned runs;                  /* injections started so far */
    unsigned open_ms, trig_ms, start_ms, end_ms;
    int      last_state;            /* last ocarina state sample (-1 none) */
    int      st_start, st_note1, st_3s, st_end, st_settle;   /* state byte at the sample points (-1 = not sampled) */
    unsigned aux_start, aux_note1, aux_end, aux_settle;      /* caller-defined extra sample (INJ_NONE = none) */
    unsigned aux_change_ms;         /* ms after START when aux first differed from aux_start (INJ_NONE if never) */
    int      got_note1, got_3s;
    int      ntrans;
    unsigned trans_ms[INJ_MAX_TRANS];   /* ms after START */
    unsigned char trans_from[INJ_MAX_TRANS], trans_to[INJ_MAX_TRANS];
} inj_t;

void inj_init(inj_t *s, int song);
/* Manual mode: the replay starts only once inj_go() was called while the ocarina is open (immediately, then the usual
 * catalog delay), runs once and returns to idle. go is dropped if the ocarina is seen closed while waiting. */
void inj_set_manual(inj_t *s, int manual);
void inj_go(inj_t *s);
/* Manual mode: play `song` (sets the song, then like inj_go). */
void inj_play(inj_t *s, int song);
/* 1 while a replay is requested, running or settling. */
int  inj_busy(const inj_t *s);

/* One step per game frame. now_ms: monotonic ms; ostate: msgCtx+0xF38 (-1 if unreadable); aux: extra sample or
 * INJ_NONE. Writes the button mask to hold for the next pad read into *mask (0 outside a note) and returns INJ_EV_*. */
unsigned inj_step(inj_t *s, unsigned now_ms, int ostate, unsigned aux, unsigned *mask);

/* OR-merge for the pad hook: returns hold | (mask & INJ_ALLOWED_BUTTONS) if the mask is fresh
 * (now_ticks - stamp_ticks <= INJ_MAX_AGE_TICKS, unsigned wrap-safe), else hold unchanged. Never clears a real press. */
unsigned inj_merge(unsigned hold, unsigned mask, unsigned stamp_ticks, unsigned now_ticks);

#endif
