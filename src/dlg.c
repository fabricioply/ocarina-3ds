#include "dlg.h"

void dlg_init(dlg_t *d)
{
    d->phase = DLG_IDLE; d->act = DLG_ACT_NONE; d->result = DLG_RES_NONE; d->fail = DLG_F_NONE; d->req = 0;
    d->start_ms = d->phase_ms = d->choice_ms = 0;
    d->last_st = -1; d->edges_seen = 0; d->last_choice = 0xFFu; d->chosen = 0xFFu; d->forced = 0;
}

void dlg_request(dlg_t *d) { if (d->phase == DLG_IDLE || d->phase == DLG_DONE) d->req = 1; }

int dlg_active(const dlg_t *d) { return d->phase >= DLG_CLOSEPREV && d->phase <= DLG_CLOSING; }
int dlg_busy(const dlg_t *d) { return d->req || dlg_active(d); }

const char *dlg_phase_name(int p)
{
    switch (p) {
    case DLG_IDLE: return "IDLE";
    case DLG_CLOSEPREV: return "CLOSEPREV";
    case DLG_OPENING: return "OPENING";
    case DLG_CHOICE: return "CHOICE";
    case DLG_CLOSING: return "CLOSING";
    case DLG_DONE: return "DONE";
    }
    return "?";
}

static unsigned finish(dlg_t *d, int result, int fail)
{
    d->result = result; d->fail = fail; d->act = DLG_ACT_FINISH;
    d->phase = DLG_DONE;
    return DLG_EV_FINISH | DLG_EV_PHASE | (result == DLG_RES_WATCHDOG ? DLG_EV_WATCHDOG : 0u);
}

static void to_phase(dlg_t *d, int phase, unsigned now) { d->phase = phase; d->phase_ms = now; }

void dlg_open_result(dlg_t *d, unsigned now_ms, int code)
{
    (void)now_ms;
    if (code == 0) return;                      /* stays in OPENING, the game drives states 1..7 */
    d->result = DLG_RES_FAIL; d->fail = DLG_F_OPEN;
    d->phase = DLG_DONE;
}

unsigned dlg_step(dlg_t *d, unsigned now_ms, int st, unsigned choice, unsigned a_edges)
{
    unsigned ev = 0;

    d->act = DLG_ACT_NONE;
    if (dlg_active(d)) {
        if (st != d->last_st) ev |= DLG_EV_TRANS;
        if (now_ms - d->start_ms >= DLG_WATCHDOG_MS) {
            d->last_st = st;
            return ev | finish(d, DLG_RES_WATCHDOG, DLG_F_NONE);
        }
    }
    d->last_st = st;

    switch (d->phase) {
    case DLG_DONE:
        d->phase = DLG_IDLE;
        /* fall through: a request made right after a finish can start now */
    case DLG_IDLE:
        if (d->req) {
            d->req = 0;
            if (st != DLG_ST_OPEN) { ev |= DLG_EV_DROPPED; break; }
            d->act = DLG_ACT_CLOSEPREV; d->start_ms = now_ms; to_phase(d, DLG_CLOSEPREV, now_ms);
            d->result = DLG_RES_NONE; d->fail = DLG_F_NONE; d->chosen = 0xFFu; d->last_choice = 0xFFu;
            d->forced = 0; ev |= DLG_EV_PHASE;
        }
        break;
    case DLG_CLOSEPREV:
        if (st == 0) { d->act = DLG_ACT_OPEN; to_phase(d, DLG_OPENING, now_ms); ev |= DLG_EV_PHASE; }
        else if (now_ms - d->phase_ms >= DLG_CLOSEPREV_MAX_MS) ev |= finish(d, DLG_RES_FAIL, DLG_F_CLOSEPREV);
        break;
    case DLG_OPENING:
        if (st == DLG_ST_CHOICE) {
            to_phase(d, DLG_CHOICE, now_ms); d->choice_ms = now_ms; d->edges_seen = a_edges;
            d->last_choice = choice <= 1u ? choice : 0xFFu; ev |= DLG_EV_PHASE;
        } else if (st == 0 && now_ms - d->phase_ms >= DLG_VANISH_MS) ev |= finish(d, DLG_RES_FAIL, DLG_F_VANISHED);
        else if (now_ms - d->phase_ms >= DLG_NOCHOICE_MAX_MS) ev |= finish(d, DLG_RES_FAIL, DLG_F_NOCHOICE);
        break;
    case DLG_CHOICE:
        if (st == DLG_ST_CHOICE) {
            if (choice <= 1u) d->last_choice = choice;
            if (a_edges != d->edges_seen) {
                d->edges_seen = a_edges; ev |= DLG_EV_A_EDGE;
                if (now_ms - d->choice_ms >= DLG_MIN_CHOICE_MS && choice <= 1u) {
                    d->chosen = choice; to_phase(d, DLG_CLOSING, now_ms); d->forced = 0;
                    ev |= DLG_EV_CHOSEN | DLG_EV_PHASE;
                }
            }
        } else {                                 /* the game left the choice state by itself */
            d->chosen = d->last_choice; to_phase(d, DLG_CLOSING, now_ms); d->forced = 1;
            ev |= DLG_EV_CHOSEN | DLG_EV_PHASE;
            if (st == 0) ev |= finish(d, d->chosen == 0u ? DLG_RES_YES : DLG_RES_NO, DLG_F_NONE);
        }
        break;
    case DLG_CLOSING:
        if (st == 0) ev |= finish(d, d->chosen == 0u ? DLG_RES_YES : DLG_RES_NO, DLG_F_NONE);
        else if (!d->forced && st != DLG_ST_CLOSING && now_ms - d->phase_ms >= DLG_FORCE_CLOSE_MS) {
            d->act = DLG_ACT_FORCE_CLOSE; d->forced = 1;
        } else if (now_ms - d->phase_ms >= DLG_CLOSING_MAX_MS)
            ev |= finish(d, d->chosen == 0u ? DLG_RES_YES : DLG_RES_NO, DLG_F_NONE);
        break;
    }
    return ev;
}
