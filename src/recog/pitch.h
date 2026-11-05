/* Integer-only pitch detector (normalised square difference function + parabolic interpolation).
 * Pure C99, no libc, no float, no 64-bit division, no allocation. Deterministic. Host-testable.
 * Safe for a hook that does not save VFP registers.
 * Tuned on real console recordings (private recordings, see docs/HISTORY.md). */
#ifndef PITCH_H
#define PITCH_H

#define PITCH_RATE      8182     /* Hz: MICU_SAMPLE_RATE_8180 is really 8182.1245 Hz */
#define PITCH_N         1024     /* samples analysed per call (125 ms at 8182 Hz) */
#define PITCH_LOG2N     10
#ifndef PITCH_TAU_MIN
#define PITCH_TAU_MIN   5        /* shortest period in samples -> max ~1636 Hz */
#endif
#ifndef PITCH_TAU_MAX
#define PITCH_TAU_MAX   82       /* longest period in samples  -> min ~100 Hz */
#endif
#define PITCH_W         (PITCH_N - PITCH_TAU_MAX - 1)   /* correlation window */
#ifndef PITCH_VOICED_Q15
#define PITCH_VOICED_Q15 26214   /* clarity needed to call it voiced: 0.80 */
#endif
#ifndef PITCH_PICK_NUM
#define PITCH_PICK_NUM  26       /* smallest-lag peak with nsdf >= PICK_NUM/32 of the best (octave-down guard) */
#endif
#define PITCH_ABS_MIN_RMS 60     /* never analyse below this RMS (after DC removal), whatever the floor says */

typedef struct {
    int peak;       /* max |sample| after DC removal */
    int rms;        /* RMS after DC removal (always computed) */
    int ran;        /* 1 if the correlation analysis ran (signal loud enough) */
    int clarity;    /* NSDF value at the chosen peak, Q15 (32768 = perfectly periodic); 0 if not analysed */
    int hz;         /* detected pitch in Hz, 0 = unvoiced */
} pitch_result;

/* x: PITCH_N signed 16-bit samples; MODIFIED IN PLACE (DC removed, scaled down if loud).
 * min_rms: analysis only runs when rms >= max(min_rms, PITCH_ABS_MIN_RMS) (caller: noise-floor gate).
 * Returns hz (0 = unvoiced / too quiet). */
int pitch_detect(short *x, pitch_result *res, int min_rms);

#endif
