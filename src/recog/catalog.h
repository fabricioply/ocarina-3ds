/* Song catalog for the 3DS plugin: port of the OcarinaMod PC prototype's song_catalog.cpp (same 12 songs, same order,
 * same note indices and flag items), plus the button replay and the native-dialog text.
 * Pure C99, no libc, no allocation. Song id = index in the PC NativeSongCatalog() (0..11). */
#ifndef CATALOG_H
#define CATALOG_H

#define CATALOG_COUNT      12
#define CATALOG_MAX_NOTES  8

/* Song ids (PC catalog order). */
#define SONG_ZELDA     0
#define SONG_SUN       1
#define SONG_MINUET    2
#define SONG_REQUIEM   3
#define SONG_EPONA     4
#define SONG_TIME      5
#define SONG_BOLERO    6
#define SONG_NOCTURNE  7
#define SONG_SARIA     8
#define SONG_STORMS    9
#define SONG_SERENADE  10
#define SONG_PRELUDE   11

/* Replay timing (game_input.cpp): 1.5 s wait, then one 200 ms slot per note, button held the first 120 ms. */
#define CATALOG_REPLAY_DELAY_MS 1500
#define CATALOG_REPLAY_SLOT_MS  200
#define CATALOG_REPLAY_HOLD_MS  120

/* 3DS button masks for ocarina note types 0..4 (D4,F4,A4,B4,D5): L,R,Y,X,A. */
#define BTN_L 0x200u
#define BTN_R 0x100u
#define BTN_Y 0x800u
#define BTN_X 0x400u
#define BTN_A 0x001u

typedef struct {
    const char *name;                    /* UTF-8 (plain ASCII for all 12 songs) */
    unsigned char notes[CATALOG_MAX_NOTES]; /* note types 0..4 */
    unsigned char length;                /* 5..8 */
    unsigned char flag_item;             /* learned-song item index (PC "flagItem") */
} catalog_song;

typedef struct {
    unsigned mask;      /* button mask to hold */
    unsigned on_ms;     /* press time relative to replay start (after the delay) */
    unsigned off_ms;    /* release time */
} replay_step;

const catalog_song *catalog_get(int id);               /* NULL if id out of range */
/* Interval-friendly pitch of note i of the song, in cents relative to its first note (PC: pitches 62,65,69,71,74). */
int  catalog_cents(int id, int i);
unsigned catalog_button(int id, int i);                /* button mask of note i (0 if out of range) */
/* Fills out[] (capacity cap) with one step per note; returns the number of steps (0 on bad id / small cap). */
int  catalog_replay(int id, replay_step *out, int cap);
/* Which button mask is held at `elapsed_ms` after the replay delay elapsed (elapsed < 0 = still waiting).
 * Returns 1 while the replay is running (mask may be 0 between presses), 0 once finished. */
int  catalog_replay_mask(int id, int elapsed_ms, unsigned *mask);
/* Native-dialog message text (text patch step): "<name>?" 7F1C 7F1C 7F1A FFFFFFFF 7F1D 'B' "Yes" 7F1C "No" 7F1D 00 7F 00.
 * Name characters outside 0x20..0x7E are dropped (as on the PC). Returns the byte count, or -1 if cap is too small / bad id. */
int  catalog_dialog_text(int id, unsigned char *out, int cap);

#endif
