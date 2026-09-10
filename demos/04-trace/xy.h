/* xy.h -- instruments that draw.
 *
 * Everything here writes a stereo signal whose left channel is a horizontal
 * position and whose right channel is a vertical one: the two inputs of an
 * oscilloscope in XY mode. So every instrument is also a shape, and the two
 * are not related by a mapping. They are the same numbers.
 *
 * The consequences run through the whole piece:
 *
 *   - A pitch is how many times a second a shape is traced. A pure tone in
 *     both channels, a quarter cycle apart, is a circle, and it is the same
 *     circle at any pitch -- only its size says anything.
 *   - A timbre is the shape itself. A closed path is a sum of rotating
 *     circles (its Fourier series), and each of those is one harmonic. A
 *     figure with n-fold symmetry keeps only the harmonics that are one more
 *     than a multiple of n, so a square has the odd ones, a triangle has no
 *     multiples of three, and a circle has only the fundamental.
 *   - Two voices in a small whole-number ratio trace a figure that closes and
 *     stands still. Two that are not trace one that never quite closes and
 *     turns instead, at the rate the interval beats. The tuning is visible,
 *     which is why this piece is in just intonation.
 *   - A low-pass filter rounds a shape's corners off, and resonance puts a
 *     small loop at each one.
 *
 * All of it renders offline into dm_audio buffers at DM_SR.
 */
#ifndef TRACE_XY_H
#define TRACE_XY_H

#include "../../engine/dm_audio.h"
#include "../../engine/dm_vec.h"

/* ---- paths ----------------------------------------------------------------
 *
 * A path is a closed curve with its parameter running 0..1 once around it,
 * uniform in time rather than in length: where the beam should linger, the
 * parameter crawls. `t` is seconds since the note began, for shapes that move.
 */
typedef v2 (*xy_path_fn)(f64 phase, f64 t, const void *ud);

v2 xy_path_circle(f64 phase, f64 t, const void *ud);

/* A path sampled once into a table and looked up with linear interpolation. */
typedef struct {
    int n;
    v2 *p;
} xy_table;

void      xy_table_free(xy_table *tb);
v2        xy_path_table(f64 phase, f64 t, const void *ud);   /* ud = xy_table */

/* A regular polygon, or a star if `step` > 1: {n/step} in Schlafli notation.
 * Traced edge to edge at constant speed. */
xy_table *xy_table_polygon(int n, int step, int samples);

/* A line of text in the engine's stroke face. Strokes are drawn at one speed
 * and the pen-up moves between them at `jump` times that, so a jump is a
 * faint line rather than no line: there is no way to lift the beam of a scope
 * in XY mode, and this does not pretend otherwise. Scaled to span
 * [-half_width, half_width] and centred on its own average position, which is
 * where a high-pass filter would put it anyway. */
xy_table *xy_table_text(const char *s, f32 half_width, f32 jump, int samples);

/* A (p, q) torus knot, turning in three dimensions and seen straight on. Its
 * projection is three circles -- harmonics p, p + q and p - q -- so the knot
 * is a chord, and turning it moves energy between them. */
typedef struct {
    int p, q;
    f32 R, r;          /* tube radii, R + r = 1 fills the unit circle        */
    f32 tilt;          /* radians about the x axis at t = 0                  */
    f32 tilt_rate;     /* radians per second                                 */
} xy_knot;

v2 xy_path_knot(f64 phase, f64 t, const void *ud);

/* ---- a voice --------------------------------------------------------------- */

typedef struct {
    xy_path_fn  path;
    const void *ud;

    f32 dir;                         /* +1 anticlockwise, -1 clockwise        */
    f32 size;                        /* radius of the figure at full level    */
    f32 amp_a, amp_d, amp_s, amp_r;  /* seconds, sustain is a level           */

    /* A low-pass on both axes alike. Zero cutoff means no filter at all. */
    f32 cutoff, env_amount, resonance;
    f32 flt_a, flt_d, flt_s, flt_r;

    f32 spin;                        /* whole-figure turns per second         */
    f32 angle;                       /* starting rotation, in turns           */
    f32 phase;                       /* starting point on the path, 0..1      */

    /* Enter `detune` semitones flat and slide into tune over `glide`
     * seconds. The slide ends exactly at zero, so a figure that locks, locks
     * completely rather than settling toward it forever. */
    f32 detune, glide;

    /* And the opposite at the other end: over the last `bend_len` seconds
     * of the note, bend `bend` semitones away. Voices bent by different
     * amounts leave just intonation, and the figure they make starts to
     * turn, faster the further they go. */
    f32 bend, bend_len;

    f32 drive;                       /* per-axis saturation: squares a curve  */
} xy_voice;

xy_voice xy_voice_default(void);

void xy_note(dm_audio *bus, const xy_voice *v, f64 t0, f64 len, f32 hz, f32 vel);

/* ---- percussion ------------------------------------------------------------ */

/* A kick drum is a sine whose pitch falls from about 190 Hz to 45 Hz in thirty
 * milliseconds. Put that sine in both channels a quarter cycle apart and it is
 * a circle whose radius is the envelope: a spiral, collapsing. `decay` is the
 * envelope's time constant in seconds. */
void xy_kick(dm_audio *bus, f64 t, f32 vel, f32 size, f32 decay);

/* A ring whose radius is noise. A snare is a drum tone and a burst of noise
 * at once; here the noise rides on the tone instead of beside it, so what is
 * heard is a snare and what is seen is a circle that fizzes. Small and high,
 * the same thing is a hat.
 *
 * There is no plain noise instrument, deliberately. Noise in both channels
 * draws a scribble -- a new random point every sample, joined up -- and one
 * noise riser under a clean figure is enough to fray every edge of it. */
void xy_spark(dm_audio *bus, f64 t, f32 vel, f32 radius, f32 hz, f32 fizz,
              f32 decay, u32 seed);

/* ---- chaos ----------------------------------------------------------------- */

/* The Lorenz system integrated at audio rate, x to the left channel and z to
 * the right: the butterfly, drawn by the sound it makes. Its time scale is set
 * so the orbit around each wing turns at the pitch asked for, so it can be
 * played like any other bass. */
typedef struct {
    const f32 *hz;          /* pitch, one entry every `step` seconds          */
    int        nsteps;
    f64        step;
    f32        size;
    f32        vel0, vel1;  /* level at the start and at the end              */
    f32        pump;        /* how far the level falls between strikes        */
    f64        strike;      /* seconds between them                           */
} xy_lorenz_part;

void xy_lorenz(dm_audio *bus, f64 t0, f64 len, const xy_lorenz_part *p);

#endif /* TRACE_XY_H */
