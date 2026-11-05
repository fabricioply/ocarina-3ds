/* Probe10: pure memory helpers for the native Yes/No dialog (README "Native dialog" steps: save, text patch, open, restore), working on the message
 * context through a translate callback so they run on the console (identity + svcQueryMemory checks) and on the host
 * (fake byte arrays). Pure C99, no libc, no allocation. */
#ifndef DLGMEM_H
#define DLGMEM_H

/* msgCtx layout (found on the PC prototype, confirmed on the console) */
#define DM_OFF_PLAY     0x08u     /* play-state pointer */
#define DM_OFF_DRAW     0x0Du     /* draw flag byte */
#define DM_OFF_TABLE    0x1Cu     /* message table object: +4 file base, +8 records (0x60 bytes each, sorted by id) */
#define DM_OFF_STATE    0xF38u    /* text state byte */
#define DM_OFF_LANG     0xF3Cu
#define DM_OFF_ACTOR    0xFA0u
#define DM_OFF_NEXT     0xFA4u
#define DM_OFF_CHOICE   0xFC4u
#define DM_CTX_BYTES    0x1090u   /* the whole context span that must be mapped read/write */
#define DM_TEXT_ID      0x1Au     /* "Do you want to play again?" Yes/No record, text replaced during the dialog */

/* core ranges of the context saved before the dialog and put back after it: [0,0x20) [0x164,0x168) [0xF38,0xFD0) */
#define DM_CORE_A_LEN   0x20u
#define DM_CORE_B_OFF   0x164u
#define DM_CORE_B_LEN   4u
#define DM_CORE_C_OFF   0xF38u
#define DM_CORE_C_LEN   0x98u
#define DM_CORE_BYTES   (DM_CORE_A_LEN + DM_CORE_B_LEN + DM_CORE_C_LEN)

#define DM_TEXT_MAX     96        /* largest replacement text we save/restore */

/* Translate a game address to a host pointer for len bytes (write != 0: must be writable). NULL = not usable. */
typedef unsigned char *(*dm_xlat_fn)(void *user, unsigned addr, unsigned len, int write);

typedef struct {
    unsigned char *ctx;      /* msgCtx (already a valid pointer) */
    dm_xlat_fn     xlat;
    void          *user;
} dlg_mem;

/* text patch error codes (0 = ok) */
enum { DM_OK = 0, DM_E_LANG = 1, DM_E_TABLE = 2, DM_E_COUNT = 3, DM_E_NOREC = 4, DM_E_SIZE = 5, DM_E_SANITY = 6,
       DM_E_WRITE = 7, DM_E_TOOBIG = 8 };

typedef struct {
    int           active;
    unsigned char *entry;                   /* the (offset,size) pair of the language */
    unsigned char *text;
    unsigned      old_size;
    unsigned      new_size;
    unsigned      lang;
    unsigned char saved[DM_TEXT_MAX];       /* the bytes that were overwritten */
} dlg_textsave;

unsigned dm_rd32(const unsigned char *p);
void     dm_wr32(unsigned char *p, unsigned v);

void dm_core_save(const unsigned char *ctx, unsigned char *buf /* DM_CORE_BYTES */);
void dm_core_restore(unsigned char *ctx, const unsigned char *buf);

/* Step 3: replace the text of record DM_TEXT_ID for the current language by text[0..n). On DM_E_SIZE *old_size_out gets
 * the original size. */
int  dm_text_patch(const dlg_mem *m, const unsigned char *text, unsigned n, dlg_textsave *sv);
void dm_text_restore(dlg_textsave *sv);

/* Step 2: close a finished message like Message_CloseTextbox: next = 3, state = 0xE. */
void dm_close_prev(unsigned char *ctx);
/* Step 4 state writes after the loader returned non-zero: actor = 0, state = 1, next = 2, draw flag = 1. */
void dm_open_writes(unsigned char *ctx);
/* Force-close the dialog box (same writes as dm_close_prev). */
void dm_force_close(unsigned char *ctx);

#endif
