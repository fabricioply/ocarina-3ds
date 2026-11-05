/* Hook planning + report formatting. Pure C99, no libc, no allocation. Host-testable. */
#ifndef HOOK_H
#define HOOK_H

#define HOOK_W0_MASK   0xFFFFF000u   /* ldr r2,[pc,#imm12] */
#define HOOK_W0_VAL    0xE59F2000u
#define HOOK_W1_VAL    0xE92D4030u   /* push {r4,r5,lr} */
#define HOOK_LDR_PC_M4 0xE51FF004u   /* ldr pc,[pc,#-4] : jumps to the next word */

enum { HOOK_OK = 0, HOOK_ERR_PROLOGUE = 1, HOOK_ERR_RANGE = 2, HOOK_ERR_ARGS = 3 };

typedef struct {
    unsigned w0, w1;          /* original prologue words */
    unsigned literal_addr;    /* func + 8 + imm12 */
    unsigned literal;         /* value read from the pool at that address */
    unsigned resume;          /* func + 8 : where the trampoline continues */
    unsigned patch[2];        /* new first two words of the function */
} hook_plan;

unsigned hook_rd32(const unsigned char *p);                 /* little endian read */
void     hook_wr32(unsigned char *p, unsigned v);
int      hook_prologue_ok(unsigned w0, unsigned w1);
unsigned hook_literal_addr(unsigned func, unsigned w0);     /* func + 8 + imm12 */
void     hook_make_patch(unsigned tramp, unsigned patch[2]);

/* Validates + reads the literal from an image (img maps [base, base+size)). */
int hook_plan_make(const unsigned char *img, unsigned size, unsigned base,
                   unsigned func, unsigned tramp, hook_plan *out);
/* Same, then writes the 2-word patch into `copy` (a size-byte copy of img). */
int hook_apply_to_copy(const unsigned char *img, unsigned size, unsigned base,
                       unsigned func, unsigned tramp, unsigned char *copy, hook_plan *out);

/* ---- mid-function hook (probe9): two position-independent instructions at `site` are replaced by
 *   ldr pc,[pc,#-4] ; .word tramp
 * and the trampoline re-executes them and resumes at site+8. Used for the game's pad read (see sigs.h PadHold):
 *   site+0 : ldr r0,[r5]            (0xE5950000)  r0 = hold mask just written by the HID ring copy
 *   site+4 : bic r0,r0,#0x2000      (0xE3C00A02)
 * Both words are checked against the expected values before anything is planned. */
#define HOOK_PAD_W0 0xE5950000u
#define HOOK_PAD_W1 0xE3C00A02u

typedef struct {
    unsigned w0, w1;          /* original words at the site */
    unsigned resume;          /* site + 8 */
    unsigned patch[2];
} hook_mid_plan;

int hook_mid_plan_make(const unsigned char *img, unsigned size, unsigned base, unsigned site, unsigned tramp,
                       unsigned exp0, unsigned exp1, hook_mid_plan *out);   /* HOOK_OK / ERR_PROLOGUE (= words differ) / RANGE / ARGS */
/* Number of direct B/BL instructions (any condition except the BLX-immediate encoding) in the image whose target is
 * `target`. A mid-function hook is only safe when nothing branches to site+4 (the word holding the trampoline address). */
unsigned hook_branch_refs(const unsigned char *img, unsigned size, unsigned base, unsigned target);

/* ticks (1/268111856 s) -> microseconds, multiply+shift: ticks * 16019311 >> 32.
 * 16019311 = round(2^32 * 1e6 / 268111856). Exact for any 32-bit ticks within 1 us. */
unsigned hook_ticks_to_us(unsigned ticks);
/* 64-bit / 32-bit unsigned division by shift-subtract (no libgcc). n==0 returns 0. */
unsigned long long hook_div64(unsigned long long total, unsigned n);
/* "n:0E10 st:00C1 gs:0091 avg:12us max:340us io:0041E944"; returns length (< 90). */
int hook_format_report(char *out, unsigned n, unsigned st, unsigned gs,
                       unsigned avg_us, unsigned max_us, unsigned io);

#endif
