/* Native Yes/No dialog state machine for ocarina.3gx (pure C99, no libc, no allocation, state in dlg_t).
 * Same steps and timeouts as the console-proven native-dialog probe (docs/HISTORY.md), but REQUEST driven: nothing happens
 * until dlg_request() is called while the ocarina session is open (msgCtx state 6).
 *
 *   IDLE --(request, state 6)--> CLOSEPREV [act CLOSEPREV: save core, close the finished ocarina message: next=3
 *   state=14] --(state 0)--> OPENING [act OPEN: patch text, loader(ctx,0x1A), state writes; the caller answers with
 *   dlg_open_result()] --(state 7)--> CHOICE --(A edge >= 250 ms later, or state leaves 7)--> CLOSING [force close after
 *   400 ms if the game does not] --(state 0)--> FINISH [act FINISH: restore text + core] --> DONE --(next step)--> IDLE.
 * A watchdog of 20 s (from CLOSEPREV) finishes with WATCHDOG (the caller restores everything). A request made while
 * the state is not 6 is dropped (DLG_EV_DROPPED). */
#ifndef DLG_H
#define DLG_H

#define DLG_WATCHDOG_MS        20000u
#define DLG_CLOSEPREV_MAX_MS   2000u
#define DLG_VANISH_MS          2500u
#define DLG_NOCHOICE_MAX_MS    10000u
#define DLG_MIN_CHOICE_MS      250u
#define DLG_FORCE_CLOSE_MS     400u
#define DLG_CLOSING_MAX_MS     5000u
#define DLG_ST_OPEN            6
#define DLG_ST_CHOICE          7
#define DLG_ST_CLOSING         14

enum { DLG_IDLE = 0, DLG_CLOSEPREV = 2, DLG_OPENING = 3, DLG_CHOICE = 4, DLG_CLOSING = 5, DLG_DONE = 6 };
enum { DLG_ACT_NONE = 0, DLG_ACT_CLOSEPREV = 1, DLG_ACT_OPEN = 2, DLG_ACT_FORCE_CLOSE = 3, DLG_ACT_FINISH = 4 };
enum { DLG_RES_NONE = 0, DLG_RES_YES = 1, DLG_RES_NO = 2, DLG_RES_FAIL = 3, DLG_RES_WATCHDOG = 4 };
enum { DLG_F_NONE = 0, DLG_F_CLOSEPREV = 1, DLG_F_OPEN = 2, DLG_F_NOCHOICE = 3, DLG_F_VANISHED = 4 };

#define DLG_EV_DROPPED   0x01u   /* a request was dropped (state was not 6) */
#define DLG_EV_TRANS     0x04u   /* msgCtx state byte changed while the dialog is active */
#define DLG_EV_PHASE     0x08u   /* dialog phase changed */
#define DLG_EV_A_EDGE    0x10u   /* real A press seen while the choice is shown */
#define DLG_EV_CHOSEN    0x20u   /* option decided (d->chosen) */
#define DLG_EV_FINISH    0x40u   /* dialog over: d->result / d->fail valid */
#define DLG_EV_WATCHDOG  0x80u   /* the finish is the watchdog */

typedef struct {
    int      phase, act, result, fail;
    int      req;                /* a dialog was requested and not started yet */
    unsigned start_ms, phase_ms, choice_ms;
    int      last_st;
    unsigned edges_seen;
    unsigned last_choice;      /* last highlighted option sampled while the choice was shown (0xFF = none) */
    unsigned chosen;           /* decided option (0 = Yes, 1 = No, 0xFF = unknown) */
    int      forced;
} dlg_t;

void dlg_init(dlg_t *d);
/* Ask for a dialog; it starts at the next dlg_step if the state is 6 then (else it is dropped). */
void dlg_request(dlg_t *d);
/* One step per frame: now_ms monotonic; st = msgCtx+0xF38 (-1 unreadable); choice = u32 at msgCtx+0xFC4; a_edges =
 * running count of real A presses. Returns DLG_EV_*; d->act tells what the caller must do now (at most one). */
unsigned dlg_step(dlg_t *d, unsigned now_ms, int st, unsigned choice, unsigned a_edges);
/* Answer to DLG_ACT_OPEN: code 0 = opened (loader returned non-zero and state writes done), else a failure code (the
 * caller then runs its finish routine; d->result = FAIL, d->fail = DLG_F_OPEN). */
void dlg_open_result(dlg_t *d, unsigned now_ms, int code);
int  dlg_active(const dlg_t *d);   /* CLOSEPREV..CLOSING: the message context belongs to the dialog */
int  dlg_busy(const dlg_t *d);     /* active, or requested and not started yet */
const char *dlg_phase_name(int phase);

#endif
