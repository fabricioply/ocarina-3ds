/* ocarina.3gx - the integrated plugin (marco 1): microphone -> songs in Ocarina of Time 3D. Minimal: no libc, no libctru
 * init, only svc stubs. Console glue only; every decision is in host-tested pure code (integ.c + drv/dlg/dlgmem +
 * inject + ../ocarina pitch/notes/matcher/catalog).
 *
 * Start-up (plugin entry, before the game reaches the title screen):
 *   1. find InputOwner / MessageGetState (-> msgCtx) / MessageLoader / StartTextbox / PadHold by signature, validate
 *      both hook sites (prologue words, no branch into the mid-function site), RWX. Any failure: log + do nothing.
 *   2. mic:u: an 8 KB shared block reserved at the END of the last free heap range (probe6 method), PCM16 8182 Hz loop.
 *   3. SD log /ocarina/log.txt (header lines), then the two hooks are written (second word first).
 * Per frame (InputOwner hook): copy the new mic samples (cheap), ig_frame() (analysis only with the ocarina open),
 * publish the OR mask for the pad hook, write the RAM log to the SD only when ig_frame says so (ocarina closed).
 * Pad hook (inside the game's pad reader): OR the mask into the hold word (never removes a real press), count real A
 * presses, remember the real ocarina buttons held (the user playing by hand mutes the detector). */
#include <3ds/types.h>
#include <3ds/svc.h>
#include "scan.h"
#include "sigs.h"
#include "hook.h"
#include "fslog.h"
#include "freerange.h"
#include "ringcopy.h"
#include "logbuf.h"
#include "integ.h"

#define CODE_BASE     0x00100000u
#define MAX_REGION    0x600000u
#define STATE_OFF     0xF38u
#define TICKS_PER_MS  268112u
#define MAX_APPENDS   3000u
#define MAX_LOG_BYTES 0x00200000u      /* 2 MiB: then the log stops growing (the plugin keeps working) */
#define MIC_STALL_MS  2000u

/* mic shared block: 0x2000 bytes; sample ring = first 0x1FFC bytes, last 4 bytes = byte offset counter */
#define MIC_BLOCK     0x2000u
#define MIC_DATA_SIZE (MIC_BLOCK - 4u)
#define MIC_RING_LEN  (MIC_DATA_SIZE / 2u)
#define MIC_ENC_PCM16_SIGNED 3u
#define MIC_RATE_8180        3u     /* 8182.1245 Hz */

extern void hook_trampoline(void);
extern void pad_trampoline(void);
extern void hook_cache_sync(void);
extern int  hook_ctrl_process(unsigned h, unsigned op, unsigned a2, unsigned a3);
extern unsigned hook_tramp_lit, hook_tramp_resume, pad_tramp_resume;

#define DATA __attribute__((section(".data")))   /* never rely on .bss (bss size is 0 in the 3gx) */

static DATA Handle g_fs = 0;
static DATA unsigned g_len = 0;
static DATA unsigned g_logok = 0;
static DATA unsigned g_appends = 0;
static DATA u8  g_zero = 0;
static DATA char g_buf[1024];
static DATA char g_dir[] = "/ocarina";
static DATA char g_path[] = "/ocarina/log.txt";
static DATA logbuf_t g_lb;
static DATA ig_t g_ig;
static DATA unsigned g_ld = 0;
static DATA unsigned g_msgctx = 0;
static DATA u64 g_t0 = 0;
static DATA unsigned g_t0_set = 0;
/* mic */
static DATA volatile unsigned g_mic = 0;
static DATA unsigned g_mic_last = 0, g_mic_have = 0, g_mic_move_ms = 0, g_mic_stall = 0, g_mic_bad = 0;
/* pad hook <-> frame hook */
static DATA volatile unsigned g_mask = 0;
static DATA volatile unsigned g_stamp = 0;
static DATA volatile unsigned g_a_edges = 0;
static DATA volatile unsigned g_prev_a = 0;
static DATA volatile unsigned g_real_hold = 0;
static DATA volatile unsigned g_pad_calls = 0;
static DATA volatile unsigned g_pad_or = 0;

/* memcpy/memset for any struct copy the compiler emits (freestanding: nothing else provides them) */
void *memcpy(void *d, const void *s, unsigned n)
{
    unsigned char *a = (unsigned char *)d; const unsigned char *b = (const unsigned char *)s;
    while (n--) *a++ = *b++;
    return d;
}
void *memset(void *d, int c, unsigned n)
{
    unsigned char *a = (unsigned char *)d;
    while (n--) *a++ = (unsigned char)c;
    return d;
}

/* Signed 32-bit division helper the compiler calls for `a / b` with a variable b (matcher.c: rms and loose scores; the
 * ARM11 has no divide instruction and libgcc is not linked). C semantics: truncation toward zero; b == 0 returns 0. */
int __aeabi_idiv(int a, int b)
{
    unsigned n = a < 0 ? 0u - (unsigned)a : (unsigned)a, d = b < 0 ? 0u - (unsigned)b : (unsigned)b;
    unsigned qt = ig_udiv(n, d);
    return ((a < 0) != (b < 0)) ? -(int)qt : (int)qt;
}

static inline unsigned *cmdbuf(void)
{
    unsigned *p;
    __asm__ volatile("mrc p15, 0, %0, c13, c0, 3" : "=r"(p));
    return (unsigned *)((u8 *)p + 0x80);
}

static unsigned ipc(Handle h)
{
    Result r = svcSendSyncRequest(h);
    if (r < 0) return (unsigned)r;
    return cmdbuf()[1];
}

/* ---- log.txt ---- */
static unsigned file_open(const char *path, unsigned plen, unsigned flags, Handle *file)
{
    unsigned r;
    fsl_build_open_file(cmdbuf(), FSL_ARCHIVE_SDMC, (unsigned)&g_zero, (unsigned)path, plen, flags);
    r = ipc(g_fs);
    if ((int)r >= 0) *file = (Handle)cmdbuf()[3];
    return r;
}

static unsigned file_close(Handle file)
{
    unsigned r;
    fsl_build_close_file(cmdbuf());
    r = ipc(file);
    svcCloseHandle(file);
    return r;
}

static unsigned log_write(Handle file, const char *src, unsigned len)
{
    unsigned r;
    fsl_build_write(cmdbuf(), g_len, 0, (unsigned)src, len, FSL_WRITE_FLUSH);
    r = ipc(file);
    if ((int)r < 0) return r;
    if (cmdbuf()[2] != len) return 0xE7FFFFFFu;
    g_len += len;
    return 0;
}

static unsigned log_append(const char *src, unsigned len)
{
    Handle f = 0;
    unsigned r = file_open(g_path, sizeof(g_path), FSL_OPEN_WRITE, &f);
    if ((int)r < 0) return r;
    r = log_write(f, src, len);
    if ((int)r < 0) { file_close(f); return r; }
    return file_close(f);
}

/* One SD append of everything pending (best effort; on failure the lines are dropped, no retry storm). */
static void flush_log(void)
{
    unsigned n = lb_pending(&g_lb);
    if (n && g_logok && g_appends < MAX_APPENDS && g_len + n < MAX_LOG_BYTES) {
        g_appends++;
        if ((int)log_append(g_lb.buf, n) < 0) g_logok = 0;
    }
    lb_clear(&g_lb);
}

/* identity translation for dlgmem, only for ranges the kernel reports mapped (and writable when asked) in ONE region */
static unsigned char *con_xlat(void *user, unsigned addr, unsigned len, int write)
{
    MemInfo mi;
    PageInfo pi;
    (void)user;
    if (len == 0) len = 1;
    if (addr < CODE_BASE || addr + len < addr) return 0;
    if (svcQueryMemory(&mi, &pi, addr) < 0 || mi.state == 0) return 0;
    if (!(mi.perm & 1u) || (write && !(mi.perm & 2u))) return 0;
    if (addr + len > mi.base_addr + mi.size) return 0;
    return (unsigned char *)addr;
}

/* the game's message loader (ARM, AAPCS: r0 = msgCtx, r1 = text id, non-zero = success), on the game thread */
static __attribute__((noinline)) unsigned call_loader(void *user, unsigned ctx, unsigned id)
{
    unsigned (*fn)(unsigned, unsigned) = (unsigned (*)(unsigned, unsigned))g_ld;
    (void)user;
    return fn(ctx, id);
}

/* new mic samples -> analysis ring (and stall/counter bookkeeping) */
static void mic_frame(unsigned now_ms)
{
    unsigned off, pos, n;
    if (!g_mic) return;
    svcInvalidateProcessDataCache(0xFFFF8001u, g_mic, MIC_BLOCK);
    off = *(volatile unsigned *)(g_mic + MIC_DATA_SIZE);
    if (!rc_pos(off, MIC_RING_LEN, &pos)) {
        if (++g_mic_bad == 30u) ig_anomaly(&g_ig, now_ms, IG_AN_MIC, off);
        return;
    }
    if (!g_mic_have) { g_mic_last = pos; g_mic_have = 1; g_mic_move_ms = now_ms; return; }
    n = rc_avail(g_mic_last, pos, MIC_RING_LEN);
    if (n) {
        ig_push(&g_ig, (const short *)g_mic, MIC_RING_LEN, g_mic_last, n);
        g_mic_last = pos;
        g_mic_move_ms = now_ms;
        g_mic_stall = 0;
    } else if (!g_mic_stall && g_ig.open && now_ms - g_mic_move_ms >= MIC_STALL_MS) {
        g_mic_stall = 1;
        ig_anomaly(&g_ig, now_ms, IG_AN_MIC, 0);
    }
}

/* Called by hook_trampoline once per game frame (InputOwner entry). No VFP, small stack. */
void hook_tick(unsigned game_state)
{
    u64 tick = (u64)svcGetSystemTick(), t1;
    unsigned now_ms, mask = 0, out;
    (void)game_state;

    if (!g_t0_set) { g_t0 = tick; g_t0_set = 1; }
    now_ms = (unsigned)hook_div64(tick - g_t0, TICKS_PER_MS);

    mic_frame(now_ms);
    out = ig_frame(&g_ig, now_ms, g_real_hold, g_a_edges, &mask);
    g_stamp = (unsigned)(u64)svcGetSystemTick();
    g_mask = mask;

    t1 = (u64)svcGetSystemTick();
    ig_cost(&g_ig, now_ms, hook_ticks_to_us((unsigned)(t1 - tick)), (out & IG_OUT_ANALYSED) ? 1 : 0);
    if (out & IG_OUT_FLUSH) flush_log();       /* only while the ocarina is closed (~25 ms) */
}

/* Called by pad_trampoline once per game frame, inside the game's pad reader (pad[0] = hold word from the HID ring). */
void pad_tick(unsigned *pad)
{
    unsigned h = pad[0], n = inj_merge(h, g_mask, g_stamp, (unsigned)svcGetSystemTick());
    if ((h & 1u) && !g_prev_a) g_a_edges++;
    g_prev_a = h & 1u;
    g_real_hold = h & IG_BUTTONS;              /* before our OR: the user's own presses */
    g_pad_calls++;
    if (n != h) { pad[0] = n; g_pad_or++; }
}

/* ---- setup ---- */
static int srv_get(const char *nm, unsigned nlen, Handle *out, unsigned *res)
{
    Handle srv = 0;
    unsigned *c, r, i;
    r = (unsigned)svcConnectToPort(&srv, "srv:");
    if ((int)r < 0) { *res = r; return 1; }
    c = cmdbuf();
    c[0] = 0x00010002; c[1] = 0x20; c[2] = 0;
    r = ipc(srv);
    if ((int)r < 0) { *res = r; svcCloseHandle(srv); return 2; }
    c = cmdbuf();
    c[0] = 0x00050100; c[1] = 0; c[2] = 0;
    for (i = 0; i < nlen; i++) ((char *)&c[1])[i] = nm[i];
    c[3] = nlen; c[4] = 0;
    r = ipc(srv);
    if ((int)r < 0) { *res = r; svcCloseHandle(srv); return 3; }
    *out = (Handle)cmdbuf()[3];
    svcCloseHandle(srv);
    return 0;
}

static int fs_setup(unsigned *res)
{
    unsigned r;
    int s = srv_get("fs:USER", 7, &g_fs, res);
    if (s) return s;
    fsl_build_initialize(cmdbuf());
    r = ipc(g_fs);
    if ((int)r < 0) { *res = r; return 4; }
    return 0;
}

static int make_dir(unsigned *dir_res, unsigned *res)
{
    unsigned r, lo, hi;
    fsl_build_open_archive(cmdbuf(), FSL_ARCHIVE_SDMC, (unsigned)&g_zero);
    r = ipc(g_fs);
    if ((int)r < 0) { *res = r; return 5; }
    lo = cmdbuf()[2]; hi = cmdbuf()[3];
    fsl_build_create_dir(cmdbuf(), lo, hi, (unsigned)g_dir, sizeof(g_dir));
    *dir_res = ipc(g_fs);
    fsl_build_close_archive(cmdbuf(), lo, hi);
    ipc(g_fs);
    return 0;
}

static int query_cb(void *ctx, unsigned addr, unsigned *base, unsigned *size, unsigned *state)
{
    MemInfo mi;
    PageInfo pi;
    (void)ctx;
    if (svcQueryMemory(&mi, &pi, addr) < 0) return -1;
    *base = mi.base_addr; *size = mi.size; *state = mi.state;
    return 0;
}

/* raw mic:u (probe6/probe8 protocol). 0 or the failing stage with *res; *strat = buffer strategy; g_mic set. */
static int mic_setup(unsigned *res, unsigned *strat)
{
    Handle mic = 0, blk = 0;
    unsigned *c, base = 0, r = 0;
    int k, s;
    static const struct { unsigned lo, hi, op; u8 last; } plan[5] = {
        {0x08000000u, 0x0E000000u, 3u,       1},
        {0x08000000u, 0x0E000000u, 3u,       0},
        {0x14000000u, 0x1C000000u, 0x10003u, 1},
        {0x14000000u, 0x1C000000u, 0x10103u, 1},
        {0x14000000u, 0x1C000000u, 0x10003u, 0},
    };
    s = srv_get("mic:u", 5, &mic, res);
    if (s) return s;
    *strat = 0;
    for (k = 0; k < 5 && !*strat; k++) {
        unsigned addr = 0; u32 got = 0;
        int f = fr_find(query_cb, 0, plan[k].lo, plan[k].hi, MIC_BLOCK, plan[k].last, &addr);
        if (f <= 0) continue;
        r = (unsigned)svcControlMemory(&got, addr, 0, MIC_BLOCK, (MemOp)plan[k].op, MEMPERM_READWRITE);
        if ((int)r < 0) continue;
        r = (unsigned)svcCreateMemoryBlock(&blk, addr, MIC_BLOCK, MEMPERM_READ, MEMPERM_READWRITE);
        if ((int)r >= 0) { *strat = (unsigned)k + 1u; base = addr; }
    }
    if (!*strat) { *res = r; return 4; }
    c = cmdbuf();
    c[0] = 0x00010042; c[1] = MIC_BLOCK; c[2] = 0; c[3] = (unsigned)blk;
    r = ipc(mic);
    if ((int)r < 0) { *res = r; return 5; }
    c = cmdbuf();
    c[0] = 0x000A0040; c[1] = 1;                       /* SetPower(true) */
    r = ipc(mic);
    if ((int)r < 0) { *res = r; return 6; }
    c = cmdbuf();
    c[0] = 0x00030140;                                 /* StartSampling(enc, rate, offset, size, loop) */
    c[1] = MIC_ENC_PCM16_SIGNED; c[2] = MIC_RATE_8180; c[3] = 0; c[4] = MIC_DATA_SIZE; c[5] = 1;
    r = ipc(mic);
    if ((int)r < 0) { *res = r; return 7; }
    c = cmdbuf();
    c[0] = 0x00060000;                                 /* IsSampling */
    r = ipc(mic);
    if ((int)r < 0) { *res = r; return 8; }
    if ((cmdbuf()[2] & 0xFFu) != 1u) { *res = 0; return 8; }
    g_mic = base;                                      /* the mic:u handle stays open on purpose */
    return 0;
}

static char *kv_hex(char *p, const char *k, unsigned v) { p = ig_put_str(p, k); return ig_put_hex(p, v, 8); }
static char *kv_dec(char *p, const char *k, unsigned v) { p = ig_put_str(p, k); return ig_put_dec(p, v); }

void ocarina_main(void)
{
    unsigned size = 0, res = 0, dir_res = 0xFFFFFFFFu;
    unsigned io = 0, gs = 0, ld = 0, tb = 0, pd = 0, site = 0;
    unsigned s0, s1, s2, s3, s4, hookst = 0, padst = 0, brefs = 0, ctx = 0, why = 0;
    unsigned mstage = 99, mres = 0, mstrat = 0;
    u64 tick;
    int i, r, dialogs = 0;
    unsigned len;
    char *p;
    const u8 *img = (const u8 *)CODE_BASE;
    MemInfo mi;
    PageInfo pi;
    hook_plan pl;
    hook_mid_plan mp;
    dlg_mem dm;
    Handle f = 0;

    pl.literal = 0; pl.resume = 0; pl.patch[0] = pl.patch[1] = 0;
    mp.resume = 0; mp.patch[0] = mp.patch[1] = 0;

    for (i = 0; i < 8 && size < MAX_REGION; i++) {
        if (svcQueryMemory(&mi, &pi, CODE_BASE + size) < 0) break;
        if (mi.base_addr != CODE_BASE + size && i > 0) break;
        if (!(mi.perm & 1) || mi.state == 0) break;
        size += mi.size - (CODE_BASE + size - mi.base_addr);
    }
    if (size > MAX_REGION) size = MAX_REGION;

    s0 = (unsigned)scan_find(img, size, CODE_BASE, kSigInputOwner_bytes, kSigInputOwner_mask, kSigInputOwner_len, &io);
    s1 = (unsigned)scan_find(img, size, CODE_BASE, kSigMessageGetState_bytes, kSigMessageGetState_mask, kSigMessageGetState_len, &gs);
    s2 = (unsigned)scan_find(img, size, CODE_BASE, kSigMessageLoader_bytes, kSigMessageLoader_mask, kSigMessageLoader_len, &ld);
    s3 = (unsigned)scan_find(img, size, CODE_BASE, kSigStartTextbox_bytes, kSigStartTextbox_mask, kSigStartTextbox_len, &tb);
    s4 = (unsigned)scan_find(img, size, CODE_BASE, kSigPadHold_bytes, kSigPadHold_mask, kSigPadHold_len, &pd);
    if (s4 == 1) site = pd + kSigPadHold_site;

    /* InputOwner hook: 0 ready, 1 not found, 2 prologue mismatch, 3 plan failed, 4 rwx refused, 5 msgCtx unusable */
    if (s0 != 1) hookst = 1;
    else if (s1 != 1) hookst = 5;
    else {
        r = hook_plan_make(img, size, CODE_BASE, io, (unsigned)(void *)hook_trampoline, &pl);
        if (r == HOOK_ERR_PROLOGUE) hookst = 2;
        else if (r != HOOK_OK) hookst = 3;
        else if (gs - CODE_BASE + kMsgCtxLiteralOffset + 4u > size) hookst = 5;
        else {
            ctx = hook_rd32(img + (gs - CODE_BASE) + kMsgCtxLiteralOffset);
            if (svcQueryMemory(&mi, &pi, ctx + STATE_OFF) < 0 || !(mi.perm & 1) || mi.state == 0) hookst = 5;
            else if (!con_xlat(0, ctx, DM_CTX_BYTES, 0)) hookst = 5;
        }
    }
    /* pad hook: 0 ready, 1 not found/ambiguous, 2 words differ, 3 plan failed, 4 branch into the site, 5 no InputOwner hook.
     * Without it nothing can be played (and no A presses are seen), so it is required too. */
    if (s4 != 1) padst = 1;
    else {
        r = hook_mid_plan_make(img, size, CODE_BASE, site, (unsigned)(void *)pad_trampoline, HOOK_PAD_W0, HOOK_PAD_W1, &mp);
        if (r == HOOK_ERR_PROLOGUE) padst = 2;
        else if (r != HOOK_OK) padst = 3;
        else {
            brefs = hook_branch_refs(img, size, CODE_BASE, site + 4u);
            if (brefs) padst = 4;
        }
    }
    if (hookst && !padst) padst = 5;
    if (!hookst && !padst) {
        r = hook_ctrl_process(0xFFFF8001u, 1u /* PROCESSOP_SET_MMU_TO_RWX */, 0, 0);
        if (r < 0) { hookst = 4; padst = 5; }
    }

    /* the mic only if we will hook (probe6 order): the block is reserved now, at game start */
    if (!hookst && !padst) mstage = (unsigned)mic_setup(&mres, &mstrat);

    lb_init(&g_lb);
    why = 0;
    if (s2 != 1) why = DRV_WHY_LOADER;
    else if (!ctx || !con_xlat(0, ctx, DM_CTX_BYTES, 1)) why = DRV_WHY_CTX;
    dialogs = !why;
    g_ld = ld;
    dm.ctx = (unsigned char *)ctx; dm.xlat = con_xlat; dm.user = 0;
    if (!hookst) ig_init(&g_ig, (unsigned char *)ctx, dialogs, &dm, ctx, call_loader, 0, &g_lb);

    /* ---- SD log header (best effort) ---- */
    r = fs_setup(&res);
    if (!r) r = make_dir(&dir_res, &res);
    if (!r) {
        res = file_open(g_path, sizeof(g_path), FSL_OPEN_WRITE | FSL_OPEN_CREATE, &f);
        if ((int)res >= 0) {
            fsl_build_set_size(cmdbuf(), 0, 0);
            res = ipc(f);
            if ((int)res < 0) file_close(f);
            else {
                tick = (u64)svcGetSystemTick();
                p = ig_put_str(g_buf, "OCARINA ocarina.3gx 1.0 (mic -> songs, native Yes/No dialog)");
                len = (unsigned)ig_fin(g_buf, p);
                len += (unsigned)fsl_line_tick(g_buf + len, (unsigned)tick, (unsigned)(tick >> 32), dir_res);
                len += (unsigned)fsl_line_addrs(g_buf + len, io, gs, ld, tb);
                len += (unsigned)fsl_line_status(g_buf + len, s0, s1, s2, s3, hookst);
                p = kv_hex(g_buf + len, "pad=", site); p = kv_dec(p, " padscan=", s4); p = kv_dec(p, " padhook=", padst);
                p = kv_dec(p, " brefs=", brefs); p = kv_hex(p, " ctx=", ctx);
                len += (unsigned)ig_fin(g_buf + len, p);
                p = kv_hex(g_buf + len, "mic=", g_mic); p = kv_dec(p, " stage=", mstage); p = kv_dec(p, " strat=", mstrat);
                p = kv_hex(p, " res=", mres);
                len += (unsigned)ig_fin(g_buf + len, p);
                p = ig_put_str(g_buf + len, "plan: listen while F38==6, hop 512/1024 @8182Hz; SONG -> replay (+1500ms); "
                                            "AMBIG -> dialog '<song>?' up to 3; mute dialog/replay/buttons +1000ms; dlg=");
                p = ig_put_str(p, dialogs ? "ON" : "OFF why=");
                if (!dialogs) p = ig_put_dec(p, why);
                p = ig_put_str(p, (!hookst && !padst && !mstage) ? " -> ACTIVE" : " -> REFUSED (nothing patched)");
                len += (unsigned)ig_fin(g_buf + len, p);
                res = log_write(f, g_buf, len);
                file_close(f);
                if ((int)res >= 0) g_logok = 1;
            }
        }
    }

    if (hookst || padst || mstage) return;             /* the log says why; the game just continues untouched */

    g_msgctx = ctx;
    hook_tramp_lit = pl.literal;
    hook_tramp_resume = pl.resume;
    pad_tramp_resume = mp.resume;

    /* second word first: a function is never half-redirected. Pad hook first, then InputOwner. */
    *(volatile unsigned *)(site + 4) = mp.patch[1];
    *(volatile unsigned *)(site)     = mp.patch[0];
    *(volatile unsigned *)(io + 4) = pl.patch[1];
    *(volatile unsigned *)(io)     = pl.patch[0];
    hook_cache_sync();
}
