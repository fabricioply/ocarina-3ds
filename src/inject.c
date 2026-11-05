#include "inject.h"

void inj_init(inj_t *s, int song)
{
    unsigned i;
    s->phase = INJ_IDLE; s->song = song; s->runs = 0; s->manual = 0; s->go = 0;
    s->open_ms = s->trig_ms = s->start_ms = s->end_ms = 0;
    s->last_state = -1;
    s->st_start = s->st_note1 = s->st_3s = s->st_end = s->st_settle = -1;
    s->aux_start = s->aux_note1 = s->aux_end = s->aux_settle = INJ_NONE;
    s->aux_change_ms = INJ_NONE;
    s->got_note1 = s->got_3s = 0;
    s->ntrans = 0;
    for (i = 0; i < INJ_MAX_TRANS; i++) { s->trans_ms[i] = 0; s->trans_from[i] = 0; s->trans_to[i] = 0; }
}

void inj_set_manual(inj_t *s, int manual) { s->manual = manual ? 1 : 0; }
void inj_go(inj_t *s) { s->go = 1; }
void inj_play(inj_t *s, int song) { s->song = song; s->go = 1; }
int  inj_busy(const inj_t *s) { return s->go || s->phase == INJ_REPLAY || s->phase == INJ_SETTLE; }

static void begin_run(inj_t *s, unsigned now_ms, int ostate, unsigned aux)
{
    unsigned i;
    s->phase = INJ_REPLAY;
    s->go = 0;
    s->start_ms = now_ms;
    s->runs++;
    s->st_start = ostate; s->st_note1 = s->st_3s = s->st_end = s->st_settle = -1;
    s->aux_start = aux; s->aux_note1 = s->aux_end = s->aux_settle = INJ_NONE;
    s->aux_change_ms = INJ_NONE;
    s->got_note1 = s->got_3s = 0;
    s->ntrans = 0;
    for (i = 0; i < INJ_MAX_TRANS; i++) { s->trans_ms[i] = 0; s->trans_from[i] = 0; s->trans_to[i] = 0; }
    s->last_state = ostate;
}

unsigned inj_step(inj_t *s, unsigned now_ms, int ostate, unsigned aux, unsigned *mask)
{
    unsigned ev = 0, rel;
    int open = (ostate == INJ_OSTATE_OPEN);
    *mask = 0;

    if (s->phase == INJ_IDLE) {
        if (open) {
            s->phase = INJ_WAIT;
            s->open_ms = now_ms;
            s->trig_ms = now_ms + INJ_TRIGGER_DELAY_MS;
            ev |= INJ_EV_OPENED;
            if (s->manual && s->go) { begin_run(s, now_ms, ostate, aux); ev |= INJ_EV_START; }
        } else s->go = 0;                          /* never start later in another state */
        s->last_state = ostate;
        return ev;
    }

    if (s->phase == INJ_WAIT) {
        if (!open) { s->phase = INJ_IDLE; s->go = 0; ev |= INJ_EV_CLOSED; }
        else if (s->manual) { if (s->go) { begin_run(s, now_ms, ostate, aux); ev |= INJ_EV_START; } }
        else if ((int)(now_ms - s->trig_ms) >= 0) { begin_run(s, now_ms, ostate, aux); ev |= INJ_EV_START; }
        s->last_state = ostate;
        return ev;
    }

    /* REPLAY or SETTLE: record state transitions and the aux sample, relative to START */
    rel = now_ms - s->start_ms;
    if (ostate != s->last_state && s->ntrans < INJ_MAX_TRANS) {
        s->trans_ms[s->ntrans] = rel;
        s->trans_from[s->ntrans] = (unsigned char)(s->last_state < 0 ? 0xFF : s->last_state);
        s->trans_to[s->ntrans] = (unsigned char)(ostate < 0 ? 0xFF : ostate);
        s->ntrans++;
    }
    s->last_state = ostate;
    if (aux != INJ_NONE && s->aux_start != INJ_NONE && aux != s->aux_start && s->aux_change_ms == INJ_NONE)
        s->aux_change_ms = rel;
    if (!s->got_3s && rel >= INJ_SAMPLE_MS) { s->got_3s = 1; s->st_3s = ostate; }

    if (!open) {                                   /* the session left state 6: stop at once, no stuck button */
        if (s->phase == INJ_REPLAY) { s->st_end = ostate; s->aux_end = aux; ev |= INJ_EV_ABORT; }
        s->st_settle = ostate; s->aux_settle = aux;
        if (!s->got_3s) { s->got_3s = 1; s->st_3s = ostate; }
        s->phase = INJ_IDLE;
        ev |= INJ_EV_CLOSED | INJ_EV_SETTLED;
        return ev;
    }

    if (s->phase == INJ_REPLAY) {
        int el = (int)rel - CATALOG_REPLAY_DELAY_MS;
        unsigned m = 0;
        int running = catalog_replay_mask(s->song, el, &m);
        if (running && el >= 0 && !s->got_note1) { s->got_note1 = 1; s->st_note1 = ostate; s->aux_note1 = aux; ev |= INJ_EV_NOTES_BEGIN; }
        if (running) *mask = m;
        else {
            s->phase = INJ_SETTLE; s->end_ms = now_ms; s->st_end = ostate; s->aux_end = aux; ev |= INJ_EV_NOTES_END;
            if (!s->got_note1) { s->got_note1 = 1; s->st_note1 = ostate; s->aux_note1 = aux; }   /* only if the very first step already finished */
        }
        return ev;
    }

    /* SETTLE */
    if (now_ms - s->end_ms >= INJ_SETTLE_MS) {
        s->st_settle = ostate; s->aux_settle = aux;
        if (!s->got_3s) { s->got_3s = 1; s->st_3s = ostate; }
        s->phase = s->manual ? INJ_IDLE : INJ_WAIT;
        s->trig_ms += INJ_PERIOD_MS;          /* nominal schedule: no drift from frame quantisation */
        ev |= INJ_EV_SETTLED;
    }
    return ev;
}

unsigned inj_merge(unsigned hold, unsigned mask, unsigned stamp_ticks, unsigned now_ticks)
{
    if (!mask) return hold;
    if ((unsigned)(now_ticks - stamp_ticks) > INJ_MAX_AGE_TICKS) return hold;
    return hold | (mask & INJ_ALLOWED_BUTTONS);
}
