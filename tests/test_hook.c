/* Host test for hook.c and the signatures in sigs.h.
 *
 * The synthetic checks always run. The checks against the game's code need a code image of Ocarina of Time 3D that you
 * extracted yourself from your own copy of the game (code loaded at base 0x00100000; it is NOT part of this repository).
 * Point the environment variable OOT3D_CANONICAL_CODE at that file to run them; without it they are skipped and the
 * test still exits 0. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scan.h"
#include "sigs.h"
#include "hook.h"

#define BASE 0x00100000u
#define TRAMP 0x07000234u

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

/* checks that need no game data */
static void synthetic_checks(void)
{
    char buf[128];
    int len;
    /* the branch counter on a synthetic image: B +0 at 0x1000 targets 0x1008; BLX imm (cond F) is ignored */
    { unsigned char t[16]; memset(t, 0, sizeof t);
      hook_wr32(t, 0xEA000000u); hook_wr32(t + 4, 0xFA000000u); hook_wr32(t + 8, 0x1AFFFFFEu); /* bne . */
      CHECK(hook_branch_refs(t, 16, 0x1000u, 0x1008u) == 2);   /* B +0 at 0x1000 and "bne ." at 0x1008 */
      CHECK(hook_branch_refs(t, 16, 0x1000u, 0x100Cu) == 0);   /* the BLX-immediate word is not counted */
      CHECK(hook_branch_refs(t, 16, 0x1000u, 0x1010u) == 0); }
    /* prologue validation on literal words */
    CHECK(hook_prologue_ok(0xE59F22ACu, 0xE92D4030u));
    CHECK(!hook_prologue_ok(0xE59F32ACu, 0xE92D4030u));
    CHECK(!hook_prologue_ok(0xE59F22ACu, 0xE92D4010u));
    /* ticks -> us */
    CHECK(hook_ticks_to_us(0) == 0);
    CHECK(hook_ticks_to_us(268u) == 0);
    CHECK(hook_ticks_to_us(269u) == 1);
    CHECK(hook_ticks_to_us(268112u) == 1000);
    CHECK(hook_ticks_to_us(268111856u) >= 999999u && hook_ticks_to_us(268111856u) <= 1000000u);
    /* div64 */
    CHECK(hook_div64(0, 3600) == 0);
    CHECK(hook_div64(3600ull * 12345ull + 77ull, 3600) == 12345ull);
    CHECK(hook_div64(0xFFFFFFFFFFull, 7) == 0xFFFFFFFFFFull / 7);
    CHECK(hook_div64(10, 0) == 0);
    /* report text */
    len = hook_format_report(buf, 0x0E10, 0x00C1, 0x0091, 12, 340, 0x0041E944);
    printf("report: '%s' (%d chars)\n", buf, len);
    CHECK(strcmp(buf, "n:0E10 st:00C1 gs:0091 avg:12us max:340us io:0041E944") == 0);
    len = hook_format_report(buf, 0xFFFF, 0xFFFF, 0xFFFF, 999999999u, 999999999u, 0xFFFFFFFFu);
    CHECK(len < 90);
    len = hook_format_report(buf, 0, 0, 0, 0, 0, 0);
    CHECK(strcmp(buf, "n:0000 st:0000 gs:0000 avg:0us max:0us io:00000000") == 0);
}

int main(void)
{
    const char *path = getenv("OOT3D_CANONICAL_CODE");
    FILE *f;
    unsigned char *img, *cp;
    long sz;
    unsigned io = 0, gs = 0, i, diff, size, ctx;
    hook_plan pl, pl2;
    int r;

    synthetic_checks();
    f = (path && *path) ? fopen(path, "rb") : 0;
    if (!f) {
        printf("SKIP: game-code checks skipped (set OOT3D_CANONICAL_CODE to your own extracted code image to run them)\n");
        printf("%d checks, %d failures\n", checks, fails);
        return fails ? 1 : 0;
    }
    fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
    img = (unsigned char *)malloc((size_t)sz);
    cp = (unsigned char *)malloc((size_t)sz);
    if (fread(img, 1, (size_t)sz, f) != (size_t)sz) { printf("read error\n"); return 1; }
    fclose(f);
    size = (unsigned)sz;

    /* locate through the real signature scanner, like the plugin does */
    CHECK(scan_find(img, size, BASE, kSigInputOwner_bytes, kSigInputOwner_mask, kSigInputOwner_len, &io) == 1);
    CHECK(scan_find(img, size, BASE, kSigMessageGetState_bytes, kSigMessageGetState_mask, kSigMessageGetState_len, &gs) == 1);
    CHECK(io == 0x0041E968u);
    CHECK(gs == 0x003769D8u);
    ctx = hook_rd32(img + (gs - BASE) + kMsgCtxLiteralOffset);
    printf("InputOwner=%08X MessageGetState=%08X msgCtx=%08X\n", io, gs, ctx);
    CHECK(ctx == 0x005C1878u);

    /* prologue words */
    CHECK(hook_prologue_ok(hook_rd32(img + (io - BASE)), hook_rd32(img + (io - BASE) + 4)));
    CHECK(hook_rd32(img + (io - BASE) + 4) == 0xE92D4030u);
    CHECK((hook_rd32(img + (io - BASE)) & 0xFFFFF000u) == 0xE59F2000u);

    /* plan */
    memset(&pl, 0, sizeof pl);
    r = hook_plan_make(img, size, BASE, io, TRAMP, &pl);
    CHECK(r == HOOK_OK);
    printf("w0=%08X w1=%08X lit_addr=%08X literal=%08X resume=%08X patch=%08X %08X\n",
           pl.w0, pl.w1, pl.literal_addr, pl.literal, pl.resume, pl.patch[0], pl.patch[1]);
    CHECK(pl.w0 == 0xE59F22ACu);
    CHECK(pl.literal_addr == io + 8u + 0x2ACu);
    CHECK(pl.literal == 0x005043D4u);
    CHECK(pl.resume == 0x0041E970u);
    CHECK(pl.patch[0] == 0xE51FF004u && pl.patch[1] == TRAMP);

    /* apply to a copy: only 8 bytes change, rest identical */
    memcpy(cp, img, size);
    r = hook_apply_to_copy(img, size, BASE, io, TRAMP, cp, &pl2);
    CHECK(r == HOOK_OK);
    CHECK(pl2.resume == 0x0041E970u);
    diff = 0;
    for (i = 0; i < size; i++) if (cp[i] != img[i]) { diff++; CHECK(i >= io - BASE && i < io - BASE + 8); }
    CHECK(diff <= 8 && diff >= 5);
    CHECK(hook_rd32(cp + (io - BASE)) == 0xE51FF004u);
    CHECK(hook_rd32(cp + (io - BASE) + 4) == TRAMP);
    CHECK(hook_rd32(img + (io - BASE)) == 0xE59F22ACu);   /* original untouched */
    /* executing the patched prologue: ldr pc,[pc,#-4] at io -> pc=io+8, reads io+4 */
    CHECK(((io + 8u) - 4u) == io + 4u);
    /* the pool literal itself is unchanged in the copy */
    CHECK(hook_rd32(cp + (pl.literal_addr - BASE)) == 0x005043D4u);

    /* refusals: changed prologue must not modify the copy */
    memcpy(cp, img, size);
    hook_wr32(cp + (io - BASE) + 4, 0xE92D4010u);               /* push {r4,lr} */
    { unsigned char *c2 = (unsigned char *)malloc(size); memcpy(c2, cp, size);
      r = hook_apply_to_copy(cp, size, BASE, io, TRAMP, c2, &pl2);
      CHECK(r == HOOK_ERR_PROLOGUE);
      CHECK(memcmp(c2, cp, size) == 0);
      CHECK(pl2.w1 == 0xE92D4010u);
      free(c2); }
    memcpy(cp, img, size);
    hook_wr32(cp + (io - BASE), 0xE59F3000u | 0x2ACu);          /* ldr r3 (wrong reg) */
    CHECK(hook_plan_make(cp, size, BASE, io, TRAMP, &pl2) == HOOK_ERR_PROLOGUE);
    hook_wr32(cp + (io - BASE), 0xE1A00000u);                   /* nop */
    CHECK(hook_plan_make(cp, size, BASE, io, TRAMP, &pl2) == HOOK_ERR_PROLOGUE);
    /* a different imm12 is still accepted (mask) */
    memcpy(cp, img, size);
    hook_wr32(cp + (io - BASE), 0xE59F2000u | 0x10u);
    r = hook_plan_make(cp, size, BASE, io, TRAMP, &pl2);
    CHECK(r == HOOK_OK && pl2.literal_addr == io + 8u + 0x10u);
    /* range / arg errors */
    CHECK(hook_plan_make(img, size, BASE, BASE + size - 4u, TRAMP, &pl2) == HOOK_ERR_RANGE);
    CHECK(hook_plan_make(img, size, BASE, BASE - 4u, TRAMP, &pl2) == HOOK_ERR_RANGE);
    CHECK(hook_plan_make(img, size, BASE, io + 1u, TRAMP, &pl2) == HOOK_ERR_ARGS);
    CHECK(hook_plan_make(img, size, BASE, io, TRAMP + 2u, &pl2) == HOOK_ERR_ARGS);
    /* literal outside the image */
    memcpy(cp, img, size);
    CHECK(hook_plan_make(cp, io - BASE + 0x100u, BASE, io, TRAMP, &pl2) == HOOK_ERR_RANGE);


    /* ---- mid-function pad hook (PadHold) ---- */
    {
        unsigned pd = 0, site, k;
        hook_mid_plan mp, mp2;
        CHECK(scan_find(img, size, BASE, kSigPadHold_bytes, kSigPadHold_mask, kSigPadHold_len, &pd) == 1);
        CHECK(pd == 0x004233C0u);
        site = pd + kSigPadHold_site;
        CHECK(site == 0x004233D8u);
        CHECK(hook_rd32(img + (site - BASE)) == HOOK_PAD_W0);          /* ldr r0,[r5] */
        CHECK(hook_rd32(img + (site - BASE) + 4) == HOOK_PAD_W1);      /* bic r0,r0,#0x2000 */
        CHECK(hook_rd32(img + (site - BASE) + 8) == 0xE5850000u);      /* str r0,[r5]  : stores the hold word */
        CHECK(hook_rd32(img + (site - BASE) + 12) == 0xE5941008u);     /* ldr r1,[r4,#8]: r1 is written before being read */
        CHECK(HOOK_PAD_W0 == 0xE5950000u && HOOK_PAD_W1 == 0xE3C00A02u);
        r = hook_mid_plan_make(img, size, BASE, site, TRAMP, HOOK_PAD_W0, HOOK_PAD_W1, &mp);
        CHECK(r == HOOK_OK);
        CHECK(mp.resume == site + 8u && mp.patch[0] == 0xE51FF004u && mp.patch[1] == TRAMP);
        /* nothing branches into the middle of the patched pair (site+4 holds the trampoline address) */
        CHECK(hook_branch_refs(img, size, BASE, site + 4u) == 0);
        printf("PadHold site=%08X branch refs to site=%u site+4=%u site+8=%u\n", site,
               hook_branch_refs(img, size, BASE, site), hook_branch_refs(img, size, BASE, site + 4u),
               hook_branch_refs(img, size, BASE, site + 8u));
        /* the branch counter finds the one known caller of InputOwner (bl at 0x4191E4) */
        CHECK(hook_branch_refs(img, size, BASE, io) == 1);
        /* applying to a copy changes only the 8 bytes of the pair */
        memcpy(cp, img, size);
        hook_wr32(cp + (site - BASE), mp.patch[0]); hook_wr32(cp + (site - BASE) + 4, mp.patch[1]);
        diff = 0;
        for (i = 0; i < size; i++) if (cp[i] != img[i]) { diff++; CHECK(i >= site - BASE && i < site - BASE + 8); }
        CHECK(diff >= 5 && diff <= 8);
        /* refusals */
        CHECK(hook_mid_plan_make(img, size, BASE, site + 4u, TRAMP, HOOK_PAD_W0, HOOK_PAD_W1, &mp2) == HOOK_ERR_PROLOGUE);
        CHECK(hook_mid_plan_make(img, size, BASE, site, TRAMP, HOOK_PAD_W0, 0xE3C00A03u, &mp2) == HOOK_ERR_PROLOGUE);
        CHECK(hook_mid_plan_make(img, size, BASE, site + 1u, TRAMP, HOOK_PAD_W0, HOOK_PAD_W1, &mp2) == HOOK_ERR_ARGS);
        CHECK(hook_mid_plan_make(img, size, BASE, site, TRAMP + 2u, HOOK_PAD_W0, HOOK_PAD_W1, &mp2) == HOOK_ERR_ARGS);
        CHECK(hook_mid_plan_make(img, size, BASE, BASE + size - 4u, TRAMP, HOOK_PAD_W0, HOOK_PAD_W1, &mp2) == HOOK_ERR_RANGE);
        CHECK(hook_mid_plan_make(img, size, BASE, BASE - 4u, TRAMP, HOOK_PAD_W0, HOOK_PAD_W1, &mp2) == HOOK_ERR_RANGE);
        /* signature robustness: a different bl offset still matches (masked), a changed opcode does not */
        memcpy(cp, img, size);
        hook_wr32(cp + (pd - BASE) + 16 * 4, 0xEB000123u);
        CHECK(scan_find(cp, size, BASE, kSigPadHold_bytes, kSigPadHold_mask, kSigPadHold_len, &k) == 1 && k == pd);
        hook_wr32(cp + (pd - BASE) + 5 * 4, 0xE1A00000u);
        CHECK(scan_find(cp, size, BASE, kSigPadHold_bytes, kSigPadHold_mask, kSigPadHold_len, &k) == 0);
    }

    printf("%d checks, %d failures\n", checks, fails);
    free(img); free(cp);
    return fails ? 1 : 0;
}
