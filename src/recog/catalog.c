#include "catalog.h"

static const catalog_song songs[CATALOG_COUNT] = {
    {"Zelda's Lullaby",   {3,4,2,3,4,2},     6, 96},
    {"Sun's Song",        {2,1,4,2,1,4},     6, 99},
    {"Minuet of Forest",  {0,4,3,2,3,2},     6, 90},
    {"Requiem of Spirit", {0,1,0,2,1,0},     6, 93},
    {"Epona's Song",      {4,3,2,4,3,2},     6, 97},
    {"Song of Time",      {2,0,1,2,0,1},     6, 100},
    {"Bolero of Fire",    {1,0,1,0,2,1,2,1}, 8, 91},
    {"Nocturne of Shadow",{3,2,2,0,3,2,1},   7, 94},
    {"Saria's Song",      {1,2,3,1,2,3},     6, 98},
    {"Song of Storms",    {0,1,4,0,1,4},     6, 101},
    {"Serenade of Water", {0,1,2,2,3},       5, 92},
    {"Prelude of Light",  {4,2,4,2,3,4},     6, 95}
};
static const int note_midi[5] = {62, 65, 69, 71, 74};
static const unsigned note_button[5] = {BTN_L, BTN_R, BTN_Y, BTN_X, BTN_A};

const catalog_song *catalog_get(int id)
{
    return (id >= 0 && id < CATALOG_COUNT) ? &songs[id] : 0;
}

int catalog_cents(int id, int i)
{
    const catalog_song *s = catalog_get(id);
    if (!s || i < 0 || i >= s->length) return 0;
    return (note_midi[s->notes[i]] - note_midi[s->notes[0]]) * 100;
}

unsigned catalog_button(int id, int i)
{
    const catalog_song *s = catalog_get(id);
    if (!s || i < 0 || i >= s->length) return 0;
    return note_button[s->notes[i]];
}

int catalog_replay(int id, replay_step *out, int cap)
{
    const catalog_song *s = catalog_get(id);
    int i;
    if (!s || cap < s->length) return 0;
    for (i = 0; i < s->length; i++) {
        out[i].mask   = note_button[s->notes[i]];
        out[i].on_ms  = (unsigned)(i * CATALOG_REPLAY_SLOT_MS);
        out[i].off_ms = (unsigned)(i * CATALOG_REPLAY_SLOT_MS + CATALOG_REPLAY_HOLD_MS);
    }
    return s->length;
}

int catalog_replay_mask(int id, int elapsed_ms, unsigned *mask)
{
    const catalog_song *s = catalog_get(id);
    int idx;
    *mask = 0;
    if (!s) return 0;
    if (elapsed_ms < 0) return 1;
    if (elapsed_ms >= s->length * CATALOG_REPLAY_SLOT_MS) return 0;
    idx = elapsed_ms / CATALOG_REPLAY_SLOT_MS;
    if (elapsed_ms - idx * CATALOG_REPLAY_SLOT_MS < CATALOG_REPLAY_HOLD_MS) *mask = note_button[s->notes[idx]];
    return 1;
}

int catalog_dialog_text(int id, unsigned char *out, int cap)
{
    static const unsigned char tail[] = {0x7F,0x1C,0x7F,0x1C,0x7F,0x1A,0xFF,0xFF,0xFF,0xFF,
                                         0x7F,0x1D,'B','Y','e','s',0x7F,0x1C,'N','o',
                                         0x7F,0x1D,0x00,0x7F,0x00};
    const catalog_song *s = catalog_get(id);
    int n = 0, i;
    if (!s) return -1;
    for (i = 0; s->name[i]; i++) {
        unsigned char c = (unsigned char)s->name[i];
        if (c < 0x20 || c > 0x7E) continue;
        if (n >= cap) return -1;
        out[n++] = c;
    }
    if (n >= cap) return -1;
    out[n++] = '?';
    if (n + (int)sizeof tail > cap) return -1;
    for (i = 0; i < (int)sizeof tail; i++) out[n++] = tail[i];
    return n;
}
