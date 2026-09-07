/* dm_synth.h -- subtractive synthesiser, drum machine and mix effects.
 *
 * Enough of a studio to build a darksynth track out of nothing: band-limited
 * oscillators, a stable state-variable filter, envelopes, a drum kit made of
 * shaped sine and noise, and the three effects the genre actually lives on --
 * a plate-ish reverb, a ping-pong delay, and sidechain ducking off the kick.
 *
 * Everything renders offline into dm_audio buffers, so nothing here has to be
 * realtime and the code can be written for clarity instead of for a deadline.
 */
#ifndef DM_SYNTH_H
#define DM_SYNTH_H

#include "dm_audio.h"

typedef enum { DM_OSC_SAW, DM_OSC_SQUARE, DM_OSC_TRI, DM_OSC_SINE, DM_OSC_NOISE } dm_osc_kind;
typedef enum { DM_LP, DM_BP, DM_HP } dm_filter_mode;

typedef struct {
    dm_osc_kind kind;
    int  unison;         /* 1..8 detuned copies, the width of a supersaw */
    f32  detune_cents;
    f32  pulse_width;    /* square only, 0.5 = symmetric */
    f32  sub_level;      /* sine one octave below, for weight */

    f32  amp_a, amp_d, amp_s, amp_r;   /* seconds, sustain is a level */
    f32  flt_a, flt_d, flt_s, flt_r;

    dm_filter_mode mode;
    f32  cutoff;         /* Hz at zero filter envelope */
    f32  env_amount;     /* Hz added at full filter envelope */
    f32  key_track;      /* 0..1, how much cutoff follows the note */
    f32  resonance;      /* 0..0.98 */

    f32  drive;          /* pre-filter saturation */
    f32  gain;
    f32  pan;            /* -1 left, +1 right */
    f32  spread;         /* unison stereo spread, 0..1 */
    f32  vib_rate, vib_depth;   /* Hz, semitones */
    f32  send;           /* share routed to the effects bus */
} dm_patch;

dm_patch dm_patch_default(void);

static inline f32 dm_midi_hz(f32 m) { return 440.0f * powf(2.0f, (m - 69.0f) * (1.0f / 12.0f)); }

/* ---- voices ------------------------------------------------------------ */

void dm_synth_note(dm_audio *bus, dm_audio *send, const dm_patch *p,
                   f64 t_start, f64 t_len, f32 midi, f32 velocity, u32 seed);

/* ---- drums -------------------------------------------------------------- */

void dm_drum_kick (dm_audio *bus, dm_audio *send, f64 t, f32 vel, f32 send_amt);
void dm_drum_snare(dm_audio *bus, dm_audio *send, f64 t, f32 vel, f32 send_amt);
void dm_drum_clap (dm_audio *bus, dm_audio *send, f64 t, f32 vel, f32 send_amt);
void dm_drum_hat  (dm_audio *bus, dm_audio *send, f64 t, f32 vel, f32 open, f32 send_amt);
void dm_drum_tom  (dm_audio *bus, dm_audio *send, f64 t, f32 vel, f32 hz, f32 send_amt);

/* Rising or falling filtered-noise sweep -- the transition glue of the genre. */
void dm_fx_sweep(dm_audio *bus, f64 t, f64 len, f32 vel, int rising);
/* Reverse-cymbal style swell that lands exactly on `t_land`. */
void dm_fx_riser(dm_audio *bus, f64 t_land, f64 len, f32 vel);

/* ---- mix effects, applied in place -------------------------------------- */

void dm_fx_reverb(dm_audio *a, f32 room, f32 damp, f32 width);
void dm_fx_delay(dm_audio *a, f64 time_l, f64 time_r, f32 feedback, f32 damp);
void dm_fx_sidechain(dm_audio *a, const f64 *triggers, int count,
                     f32 depth, f64 attack, f64 release);
void dm_fx_saturate(dm_audio *a, f32 drive);
void dm_fx_highpass(dm_audio *a, f32 hz);
void dm_fx_lowpass(dm_audio *a, f32 hz);
void dm_fx_widen(dm_audio *a, f32 amount);
void dm_fx_limiter(dm_audio *a, f32 ceiling_db, f64 attack, f64 release);
void dm_fx_fade(dm_audio *a, f64 t0, f64 t1, f32 g0, f32 g1);

#endif /* DM_SYNTH_H */
