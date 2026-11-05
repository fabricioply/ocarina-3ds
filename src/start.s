.arm
.section .crt0,"ax"
.global _start
_start:
    b ocarina_main

.section .text.tramp,"ax"
.arm

/* ---- svc wrappers (Luma extensions) ---- */
.global hook_cache_sync
.type hook_cache_sync,%function
hook_cache_sync:                 /* same pair Luma's pluginLoader.s runs after patching game code */
    svc 0x92                     /* flushEntireDataCache */
    svc 0x94                     /* invalidateEntireInstructionCache */
    bx lr

.global hook_ctrl_process
.type hook_ctrl_process,%function
hook_ctrl_process:               /* Result svcControlProcess(Handle h, u32 op, u32 a2, u32 a3) */
    svc 0xB3
    bx lr

/* ---- hook 1: per-frame InputOwner trampoline ----
 * The game's InputOwner prologue is overwritten by:  ldr pc,[pc,#-4] ; .word hook_trampoline
 * so we arrive here with r0-r3, r12, lr, sp and flags exactly as at function entry.
 * Everything is saved, hook_tick(r0 = the function's first argument, the game/play state pointer) runs (C, AAPCS),
 * everything is restored, then the two displaced instructions are replayed:
 *     ldr  r2, =<literal read from the pool at runtime>
 *     push {r4,r5,lr}
 * and we jump to InputOwner+8.
 */
.global hook_trampoline
.type hook_trampoline,%function
hook_trampoline:
    push {r0-r4, r12, lr}
    mrs  r1, cpsr
    push {r1}                    /* save flags (r0 stays the original argument) */
    mov  r4, sp                  /* r4 is callee-saved; keep old sp here */
    bic  sp, sp, #7              /* AAPCS: 8-byte aligned stack for the C call */
    bl   hook_tick
    mov  sp, r4
    pop  {r1}
    msr  cpsr_f, r1              /* restore flags */
    pop  {r0-r4, r12, lr}        /* r0-r4, r12, lr, sp = exactly as at entry */
    push {r4, r5, lr}            /* displaced instruction 2 */
    sub  sp, sp, #4              /* scratch slot that "ldr pc,[sp],#4" will pop */
    ldr  r2, =hook_tramp_resume
    ldr  r2, [r2]
    str  r2, [sp]                /* slot = InputOwner+8 */
    ldr  r2, =hook_tramp_lit
    ldr  r2, [r2]                /* displaced instruction 1: r2 = pool literal */
    ldr  pc, [sp], #4            /* jump to InputOwner+8, sp back to [r4,r5,lr] */
.ltorg

/* ---- hook 2: pad-read trampoline (mid-function, inside the game's pad reader) ----
 * Site (canonical 0x004233D8): the two instructions
 *     ldr  r0, [r5]            @ r5 = pad status {hold, trigger, release, cpx, cpy}; hold was just copied from the HID ring
 *     bic  r0, r0, #0x2000
 * are replaced by  ldr pc,[pc,#-4] ; .word pad_trampoline.  The code after the site stores r0 as the hold word and
 * derives trigger/release from it, so ORing our mask into [r5] here makes the game see the injected buttons as
 * ordinary presses, with consistent edges, in the same frame (before the game's logic update).
 * Registers: r0 is dead at the site (overwritten by the first displaced instruction), r1 is dead at the resume point
 * (the next instructions write r1 before reading it). Everything else is saved and restored.
 */
.global pad_trampoline
.type pad_trampoline,%function
pad_trampoline:
    push {r0-r4, r12, lr}
    mrs  r1, cpsr
    push {r1}
    mov  r4, sp
    bic  sp, sp, #7
    mov  r0, r5                  /* pad_tick(pad status pointer) */
    bl   pad_tick
    mov  sp, r4
    pop  {r1}
    msr  cpsr_f, r1
    pop  {r0-r4, r12, lr}
    ldr  r0, [r5]                /* displaced instruction 1 (now includes the injected bits) */
    bic  r0, r0, #0x2000         /* displaced instruction 2 */
    ldr  r1, =pad_tramp_resume
    ldr  pc, [r1]                /* resume at site+8 ("str r0,[r5]") */
.ltorg

.section .data
.balign 4
.global hook_tramp_lit
.global hook_tramp_resume
.global pad_tramp_resume
hook_tramp_lit:    .word 0
hook_tramp_resume: .word 0
pad_tramp_resume:  .word 0
