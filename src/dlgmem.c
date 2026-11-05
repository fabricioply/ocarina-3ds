#include "dlgmem.h"

unsigned dm_rd32(const unsigned char *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

void dm_wr32(unsigned char *p, unsigned v)
{
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
}

void dm_core_save(const unsigned char *ctx, unsigned char *buf)
{
    unsigned i, n = 0;
    for (i = 0; i < DM_CORE_A_LEN; i++) buf[n++] = ctx[i];
    for (i = 0; i < DM_CORE_B_LEN; i++) buf[n++] = ctx[DM_CORE_B_OFF + i];
    for (i = 0; i < DM_CORE_C_LEN; i++) buf[n++] = ctx[DM_CORE_C_OFF + i];
}

void dm_core_restore(unsigned char *ctx, const unsigned char *buf)
{
    unsigned i, n = 0;
    for (i = 0; i < DM_CORE_A_LEN; i++) ctx[i] = buf[n++];
    for (i = 0; i < DM_CORE_B_LEN; i++) ctx[DM_CORE_B_OFF + i] = buf[n++];
    for (i = 0; i < DM_CORE_C_LEN; i++) ctx[DM_CORE_C_OFF + i] = buf[n++];
}

int dm_text_patch(const dlg_mem *m, const unsigned char *text, unsigned n, dlg_textsave *sv)
{
    static const char expect[] = "Do you want to play";
    unsigned lang, base, recs, count, lo, hi, i, recaddr = 0;
    unsigned char *rec = 0, *p, *entry, *tx;
    unsigned off, size;

    sv->active = 0;
    if (n == 0 || n > DM_TEXT_MAX) return DM_E_TOOBIG;
    lang = dm_rd32(m->ctx + DM_OFF_LANG);
    if (lang >= 10u) return DM_E_LANG;
    base = dm_rd32(m->ctx + DM_OFF_TABLE + 4u);
    recs = dm_rd32(m->ctx + DM_OFF_TABLE + 8u);
    if (!base || base == 0xFFFFFFFFu || !recs || recs == 0xFFFFFFFFu) return DM_E_TABLE;
    p = m->xlat(m->user, base, 12u, 0);
    if (!p) return DM_E_TABLE;
    count = dm_rd32(p + 8u);
    if (count == 0u || count > 0x10000u) return DM_E_COUNT;
    lo = 0; hi = count;
    while (lo < hi) {
        unsigned mid = (lo + hi) / 2u, id;
        unsigned char *r = m->xlat(m->user, recs + mid * 0x60u, 0x60u, 0);
        if (!r) return DM_E_TABLE;
        id = dm_rd32(r);
        if (id == DM_TEXT_ID) { rec = r; recaddr = recs + mid * 0x60u; break; }
        if (id < DM_TEXT_ID) lo = mid + 1u; else hi = mid;
    }
    if (!rec) return DM_E_NOREC;
    (void)rec;
    entry = m->xlat(m->user, recaddr + 0x10u + lang * 8u, 8u, 1);   /* (offset,size) pair of the language, writable */
    if (!entry) return DM_E_WRITE;
    off = dm_rd32(entry);
    size = dm_rd32(entry + 4u);
    if (size == 0u || size > 0x1000u) return DM_E_SIZE;
    if (n > size) { sv->old_size = size; sv->new_size = n; return DM_E_SIZE; }
    tx = m->xlat(m->user, base + off, size, 1);
    if (!tx) return DM_E_WRITE;
    if (lang == 1u) {
        for (i = 0; i < 19u; i++) if (tx[i] != (unsigned char)expect[i]) return DM_E_SANITY;
    } else {
        int found = 0;
        for (i = 0; i + 1u < size; i++) if (tx[i] == 0x7F && tx[i + 1u] == 0x1A) { found = 1; break; }
        if (!found) return DM_E_SANITY;
    }
    sv->entry = entry; sv->text = tx; sv->old_size = size; sv->new_size = n; sv->lang = lang;
    for (i = 0; i < n; i++) sv->saved[i] = tx[i];
    for (i = 0; i < n; i++) tx[i] = text[i];
    dm_wr32(entry + 4u, n);
    sv->active = 1;
    return DM_OK;
}

void dm_text_restore(dlg_textsave *sv)
{
    unsigned i;
    if (!sv->active) return;
    for (i = 0; i < sv->new_size; i++) sv->text[i] = sv->saved[i];
    dm_wr32(sv->entry + 4u, sv->old_size);
    sv->active = 0;
}

void dm_close_prev(unsigned char *ctx)
{
    dm_wr32(ctx + DM_OFF_NEXT, 3u);
    ctx[DM_OFF_STATE] = 0xEu;
}

void dm_force_close(unsigned char *ctx) { dm_close_prev(ctx); }

void dm_open_writes(unsigned char *ctx)
{
    dm_wr32(ctx + DM_OFF_ACTOR, 0u);
    ctx[DM_OFF_STATE] = 1u;
    dm_wr32(ctx + DM_OFF_NEXT, 2u);
    ctx[DM_OFF_DRAW] = 1u;
}
