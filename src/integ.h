/* ocarina.3gx integration state machine: mic samples -> pitch detector -> note segmenter -> song matcher -> (SONG) button
 * replay, (AMBIGUOUS) native Yes/No dialog one candidate at a time -> replay on Yes. Pure C99, no libc, no float, no
 * division by variables, no allocation: all state lives in ig_t (owned by the caller). Host-testable with fake samples
 * and a fake message context.
 *
 * Per game frame (InputOwner hook) the caller does:
 *     ig_push(...)  copy the new mic samples (cheap, always)
 *     ig_frame(...) everything else; returns the button mask for the pad hook and whether to write the log now.
 * Analysis (pitch_detect, ~2 ms) runs ONLY while the ocarina is open (msgCtx+0xF38 == 6), at most once per frame, one
 * hop of 512 new samples at a time (window 1024). It is muted while our dialog is open or requested, while a replay is
 * requested/running/settling, while the user holds an ocarina button (A X Y L R: playing by hand), and for
 * IG_MUTE_AFTER_MS after any of these; after a mute the segmenter and the matcher restart (noise floor kept). */
#ifndef INTEG_H
#define INTEG_H

#include "pitch.h"
#include "notes.h"
#include "matcher.h"
#include "catalog.h"
#include "inject.h"
#include "drv.h"
#include "logbuf.h"

#define IG_RING            4096u     /* analysis ring (samples, power of two) = 0.5 s */
#define IG_HOP             512u      /* 62.5 ms at 8182 Hz */
#define IG_MUTE_AFTER_MS   1000u
#define IG_NEXT_ASK_MS     500u      /* after a No: wait, then ask the next candidate */
#define IG_CLOSE_MS        300u      /* state != 6 this long (and no dialog of ours) = ocarina closed */
#define IG_MAX_LAG_HOPS    2u        /* more backlog than this: hops are skipped (previous result repeated) */
#define IG_MAX_PHRASE      24
#define IG_INJ_WATCHDOG_MS 15000u    /* a replay (incl. 1.5 s delay + settle) never takes this long */
#define IG_PANIC_ANOMALIES 3u
#define IG_PANIC_DLGFAILS  2u
#define IG_BUTTONS         0x00000F01u   /* A X Y L R (the ocarina notes) */
#define IG_FLUSH_IDLE_MIN  1u        /* bytes pending that make a closed-ocarina flush worth it */

/* anomaly kinds (log "anomaly kind=") */
enum { IG_AN_DLGFAIL = 1, IG_AN_STATE = 2, IG_AN_INJWD = 3, IG_AN_MIC = 4, IG_AN_SLOW = 5, IG_AN_OVERFLOW = 6 };

/* ig_frame output bits */
#define IG_OUT_FLUSH     0x01u   /* write the pending log now (ocarina closed, nothing of ours running) */
#define IG_OUT_ANALYSED  0x02u   /* a pitch analysis ran in this frame */
#define IG_OUT_PANIC     0x04u   /* features are off for the rest of the session */

typedef struct {
    unsigned frames, hops, ran, voiced, notes, phrases;
    unsigned songs, ambig, timeouts, asked, yes, no, plays, skipped, overflows, dropped_req;
    unsigned peak, ana_max_us, ana_sum_us, ana_n, idle_max_us;
    unsigned open_ms;
} ig_stats;

typedef struct {
    /* collaborators */
    unsigned char *ctx;          /* msgCtx (state byte at +0xF38, read every frame) */
    int            dialogs;      /* native dialog available */
    drv_t          drv;
    inj_t          inj;
    matcher_state  m;
    notes_state    ns;
    logbuf_t      *lb;
    /* audio */
    short          ring[IG_RING];
    unsigned       wpos, apos;   /* samples written so far / start of the next analysis window (free running) */
    short          work[PITCH_N];
    pitch_result   last_pr;
    unsigned       peak_now;
    /* gating */
    int            open;         /* ocarina session open (debounced) */
    int            notsix_set;
    unsigned       notsix_ms;
    unsigned       mute_until;
    int            restart;      /* segmenter/matcher restart before the next analysis */
    int            last_st;
    /* dialog queue */
    int            cand[MATCHER_MAX_CAND], ncand, icand;
    int            ask_pending;
    unsigned       ask_at;
    int            dlg_blocked;  /* a dialog failed: none until the ocarina closes */
    /* replay watchdog */
    int            inj_was_busy;
    unsigned       inj_busy_ms;
    int            play_song;
    /* phrase (debug log) */
    int            pn;
    int            pc[IG_MAX_PHRASE], pstart[IG_MAX_PHRASE], pdur[IG_MAX_PHRASE];
    /* safety */
    int            panic;
    unsigned       anomalies, dlgfails, panic_why;
    unsigned       sess;
    ig_stats       st;
    unsigned       flush_req;
    char           line[512];
} ig_t;

/* ctx: the message context (must stay valid); dlgmem/loader: the dialog plumbing (dialogs = 0 disables the dialog, mem
 * and loader are then unused). lb: RAM log buffer. */
void ig_init(ig_t *g, unsigned char *ctx, int dialogs, const dlg_mem *mem, unsigned ctx_addr, drv_loader_fn loader,
             void *user, logbuf_t *lb);
/* Copies n samples starting at ring[first] (wrapping at ring_len) into the analysis ring. Cheap; call every frame. */
void ig_push(ig_t *g, const short *ring, unsigned ring_len, unsigned first, unsigned n);
/* One game frame. now_ms monotonic; real_hold = the pad hold word as the game read it (before our OR); a_edges = running
 * count of real A presses. *mask = buttons to OR into the next pad read. Returns IG_OUT_*. */
unsigned ig_frame(ig_t *g, unsigned now_ms, unsigned real_hold, unsigned a_edges, unsigned *mask);
/* Cost bookkeeping of the previous frame (microseconds; analysed = it ran a pitch analysis). */
void ig_cost(ig_t *g, unsigned now_ms, unsigned us, int analysed);
#define IG_SLOW_US 40000u        /* a hook frame slower than this (without an SD write) is an anomaly */
/* Report something unexpected (IG_AN_*); enough of them switch everything off (panic). */
void ig_anomaly(ig_t *g, unsigned now_ms, unsigned kind, unsigned detail);

/* helpers shared with the glue's log lines */
char *ig_put_str(char *p, const char *s);
char *ig_put_dec(char *p, unsigned v);
char *ig_put_int(char *p, int v);
char *ig_put_hex(char *p, unsigned v, int digits);
int   ig_fin(char *out, char *p);
unsigned ig_udiv(unsigned n, unsigned d);

#endif
