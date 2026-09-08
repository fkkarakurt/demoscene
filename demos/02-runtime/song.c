#include "song.h"
#include "../../engine/dm_synth.h"

#include <stdlib.h>

const int SONG_SECTION_BAR[SEC_COUNT + 1] = { 0, 2, 6, 11, 14, 16 };

song_section song_section_at(f64 t)
{
    f64 bar = t / SONG_BAR_SEC;
    for (int s = SEC_COUNT - 1; s >= 0; s--)
        if (bar >= SONG_SECTION_BAR[s]) return (song_section)s;
    return SEC_ASSEMBLE;
}

f64 song_section_start(song_section s) { return SONG_SECTION_BAR[s] * SONG_BAR_SEC; }
f64 song_section_end  (song_section s) { return SONG_SECTION_BAR[s + 1] * SONG_BAR_SEC; }

f64 song_section_phase(f64 t)
{
    song_section s = song_section_at(t);
    f64 a = song_section_start(s), b = song_section_end(s);
    return b > a ? dm_clamp((f32)((t - a) / (b - a)), 0.0f, 1.0f) : 0.0f;
}

/* ---- harmony ------------------------------------------------------------ */

/* D minor, one chord per bar over a four-bar loop: Dm - Bb - F - C. A
 * different key and a different loop from the film on purpose -- a short that
 * sounds like an offcut of the long video is worth less than one that sounds
 * like its own thing.
 *
 * The voicings keep the top voice on F or A throughout, so the pad reads as a
 * single sustained colour changing underneath rather than four separate chords
 * being announced. */
static const u8 CHORD[4][3] = {
    { 62, 65, 69 },   /* Dm : D4 F4 A4 */
    { 58, 62, 65 },   /* Bb : Bb3 D4 F4 */
    { 57, 60, 65 },   /* F  : A3 C4 F4  (first inversion)  */
    { 55, 60, 64 },   /* C  : G3 C4 E4  (second inversion) */
};

static const u8 ROOT[4] = { 38, 34, 41, 36 };   /* D2 Bb1 F2 C2 */

/* The arp, one bar of sixteenths, transposed onto each chord. It climbs, drops
 * a fourth, and climbs again -- the shape survives all four chords without
 * needing a second pattern. */
static const u8 ARP[16] = { 0, 1, 2, 1, 2, 1, 0, 2, 0, 1, 2, 1, 2, 0, 1, 2 };
static const u8 ARP_OCT[16] = { 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0 };

/* ---- patches ------------------------------------------------------------ */

static dm_patch patch_pad(void)
{
    dm_patch p = dm_patch_default();
    p.kind = DM_OSC_SAW;
    p.unison = 6; p.detune_cents = 22.0f; p.spread = 0.95f;
    p.amp_a = 0.35f; p.amp_d = 1.20f; p.amp_s = 0.75f; p.amp_r = 1.40f;
    p.flt_a = 0.60f; p.flt_d = 2.20f; p.flt_s = 0.50f; p.flt_r = 1.40f;
    p.cutoff = 240.0f; p.env_amount = 1500.0f; p.resonance = 0.20f;
    p.gain = 0.080f; p.send = 0.62f;
    return p;
}

/* The pulse that carries the opening. A short, hard, resonant blip on every
 * eighth: with no kick underneath it, the tempo has to come from somewhere,
 * and a filter that snaps shut reads as rhythm even at low volume. */
static dm_patch patch_pulse(void)
{
    dm_patch p = dm_patch_default();
    p.kind = DM_OSC_SQUARE; p.pulse_width = 0.32f;
    p.unison = 2; p.detune_cents = 9.0f; p.spread = 0.55f;
    p.amp_a = 0.001f; p.amp_d = 0.085f; p.amp_s = 0.0f;  p.amp_r = 0.09f;
    p.flt_a = 0.001f; p.flt_d = 0.070f; p.flt_s = 0.0f;  p.flt_r = 0.08f;
    p.cutoff = 320.0f; p.env_amount = 4200.0f; p.resonance = 0.72f;
    p.drive = 0.30f; p.gain = 0.098f; p.send = 0.45f;
    return p;
}

static dm_patch patch_bass(void)
{
    dm_patch p = dm_patch_default();
    p.kind = DM_OSC_SAW;
    p.unison = 2; p.detune_cents = 6.0f; p.spread = 0.08f;
    p.sub_level = 0.60f;
    p.amp_a = 0.002f; p.amp_d = 0.22f; p.amp_s = 0.62f; p.amp_r = 0.10f;
    p.flt_a = 0.002f; p.flt_d = 0.16f; p.flt_s = 0.30f; p.flt_r = 0.10f;
    p.cutoff = 110.0f; p.env_amount = 900.0f; p.key_track = 0.35f;
    p.resonance = 0.30f; p.drive = 0.45f;
    p.gain = 0.115f; p.send = 0.05f;
    return p;
}

static dm_patch patch_arp(void)
{
    dm_patch p = dm_patch_default();
    p.kind = DM_OSC_SAW;
    p.unison = 3; p.detune_cents = 14.0f; p.spread = 0.75f;
    p.amp_a = 0.002f; p.amp_d = 0.11f; p.amp_s = 0.10f; p.amp_r = 0.16f;
    p.flt_a = 0.002f; p.flt_d = 0.14f; p.flt_s = 0.18f; p.flt_r = 0.16f;
    p.cutoff = 700.0f; p.env_amount = 3400.0f; p.key_track = 0.45f;
    p.resonance = 0.42f; p.drive = 0.20f;
    p.gain = 0.058f; p.send = 0.68f;
    return p;
}

/* The sine that carries the exposed section. Nothing to hide behind, so it is
 * a plain tone with a long tail and a lot of send. */
static dm_patch patch_bell(void)
{
    dm_patch p = dm_patch_default();
    p.kind = DM_OSC_SINE;
    p.amp_a = 0.004f; p.amp_d = 0.90f; p.amp_s = 0.0f; p.amp_r = 1.20f;
    p.flt_a = 0.004f; p.flt_d = 0.80f; p.flt_s = 0.0f; p.flt_r = 1.00f;
    p.cutoff = 1800.0f; p.env_amount = 2600.0f; p.resonance = 0.10f;
    p.gain = 0.095f; p.send = 0.85f;
    return p;
}

/* ---- the track ----------------------------------------------------------- */

dm_audio *song_render(void)
{
    /* A second of tail past the end so the final reverb is not chopped; the
     * video stops at SONG_LEN_SEC and ffmpeg trims the rest. */
    int frames = dm_sec_to_frames(SONG_LEN_SEC + 1.0);

    dm_audio *out   = dm_audio_new(frames);
    dm_audio *drums = dm_audio_new(frames);
    dm_audio *music = dm_audio_new(frames);
    dm_audio *send  = dm_audio_new(frames);
    dm_audio *rev   = dm_audio_new(frames);
    if (!out || !drums || !music || !send || !rev) return NULL;

    dm_patch pad = patch_pad(), pulse = patch_pulse();
    dm_patch bass = patch_bass(), arp = patch_arp(), bell = patch_bell();

    const f64 stp = SONG_STEP_SEC;
    u32 seed = 1u;

    f64 kicks[SONG_BARS * 4];
    int nkick = 0;

    for (int bar = 0; bar < SONG_BARS; bar++) {
        f64 t0 = song_bar(bar);
        int ch = bar & 3;
        song_section sec = song_section_at(t0 + 0.001);

        /* -- pad: present in every section, it is the thread through the piece */
        {
            f32 g = 1.0f;
            if (sec == SEC_ASSEMBLE) g = 0.78f + 0.14f * (f32)bar;
            if (sec == SEC_EXPOSE)   g = 0.80f;
            if (sec == SEC_SIGN)     g = bar == SONG_SECTION_BAR[SEC_SIGN] ? 1.0f : 0.0f;
            if (g > 0.0f) {
                f64 len = sec == SEC_SIGN ? SONG_BAR_SEC * 2.2 : SONG_BAR_SEC * 1.05;
                for (int v = 0; v < 3; v++)
                    dm_synth_note(music, send, &pad, t0, len, (f32)CHORD[ch][v], 0.85f * g, seed++);
            }
        }

        /* -- the opening pulse: eighths, thinning out as the kick approaches */
        if (sec == SEC_ASSEMBLE) {
            for (int s = 0; s < 16; s += 2) {
                f32 vel = (s % 8 == 0) ? 1.0f : 0.62f;
                dm_synth_note(music, send, &pulse, t0 + s * stp, stp * 1.5,
                              (f32)CHORD[ch][s % 3], vel, seed++);
            }
        }

        /* -- drums ---------------------------------------------------------- */
        if (sec == SEC_LOCK || sec == SEC_DRIVE) {
            for (int b = 0; b < 4; b++) {
                f64 tk = t0 + b * song_beat(1.0);
                dm_drum_kick(drums, send, tk, 1.0f, 0.10f);
                if (nkick < DM_COUNT(kicks)) kicks[nkick++] = tk;
            }
            dm_drum_clap(drums, send, t0 + song_beat(1.0), 0.85f, 0.35f);
            dm_drum_clap(drums, send, t0 + song_beat(3.0), 0.85f, 0.35f);

            for (int s = 2; s < 16; s += 4) {
                int open = (s == 14);
                dm_drum_hat(drums, send, t0 + s * stp, open ? 0.62f : 0.44f,
                            open ? 0.55f : 0.10f, 0.16f);
            }
        }

        /* -- bass ----------------------------------------------------------- */
        if (sec == SEC_LOCK || sec == SEC_DRIVE) {
            /* Root on the downbeat, octave stab on the and-of-three. Two notes
             * is enough when the kick is doing the timekeeping. */
            dm_synth_note(music, send, &bass, t0,               stp * 6.0, (f32)ROOT[ch],      1.00f, seed++);
            dm_synth_note(music, send, &bass, t0 + 6  * stp,    stp * 2.0, (f32)ROOT[ch],      0.72f, seed++);
            dm_synth_note(music, send, &bass, t0 + 10 * stp,    stp * 3.0, (f32)ROOT[ch] + 12, 0.80f, seed++);
            dm_synth_note(music, send, &bass, t0 + 14 * stp,    stp * 2.0, (f32)ROOT[ch],      0.68f, seed++);
        }

        /* -- arp: only in the drive, which is what makes the drive a drive --- */
        if (sec == SEC_DRIVE) {
            for (int s = 0; s < 16; s++) {
                f32 note = (f32)CHORD[ch][ARP[s]] + 12.0f * (f32)ARP_OCT[s];
                f32 vel  = (s & 3) == 0 ? 0.95f : 0.70f;
                dm_synth_note(music, send, &arp, t0 + s * stp, stp * 0.9, note, vel, seed++);
            }
        }

        /* -- the exposed section: one bell tone per bar, and nothing else ---- */
        if (sec == SEC_EXPOSE) {
            int k = bar - SONG_SECTION_BAR[SEC_EXPOSE];
            static const f32 EXPOSE_NOTE[3] = { 77.0f, 74.0f, 72.0f };  /* F5 D5 C5 */
            dm_synth_note(music, send, &bell, t0, SONG_BAR_SEC * 1.6,
                          EXPOSE_NOTE[k % 3], 0.90f, seed++);
            /* A single soft kick per bar keeps a pulse under the silence
             * without putting the groove back. */
            f64 tk = t0;
            dm_drum_kick(drums, send, tk, 0.55f, 0.20f);
            if (nkick < DM_COUNT(kicks)) kicks[nkick++] = tk;
        }

        /* -- the sign-off: one hit, then decay ------------------------------- */
        if (sec == SEC_SIGN && bar == SONG_SECTION_BAR[SEC_SIGN]) {
            dm_drum_kick(drums, send, t0, 1.0f, 0.30f);
            if (nkick < DM_COUNT(kicks)) kicks[nkick++] = t0;
            dm_drum_clap(drums, send, t0, 0.70f, 0.60f);
            for (int v = 0; v < 3; v++)
                dm_synth_note(music, send, &bell, t0, SONG_BAR_SEC * 2.4,
                              (f32)CHORD[0][v] + 12.0f, 0.75f, seed++);
        }
    }

    /* -- transitions ------------------------------------------------------- */
    dm_fx_riser(drums, song_bar(SONG_SECTION_BAR[SEC_LOCK]),   song_bar(2.0), 0.75f);
    dm_fx_riser(drums, song_bar(SONG_SECTION_BAR[SEC_DRIVE]),  song_bar(1.0), 0.55f);
    dm_fx_sweep(drums, song_bar(SONG_SECTION_BAR[SEC_EXPOSE]), song_bar(1.0), 0.60f, 0);
    dm_fx_riser(drums, song_bar(SONG_SECTION_BAR[SEC_SIGN]),   song_bar(1.0), 0.65f);

    /* -- mix --------------------------------------------------------------- */
    dm_fx_sidechain(music, kicks, nkick, 0.50f, 0.005, 0.15);

    dm_audio_mix(rev, send, 1.0f);
    dm_fx_reverb(rev, 0.84f, 0.34f, 0.94f);
    dm_fx_highpass(rev, 200.0f);
    /* Dotted eighth at 128 BPM is 0.3516 s; the quarter on the right channel
     * puts the repeats slightly apart without smearing the arp. */
    dm_fx_delay(send, 0.3516, 0.4688, 0.40f, 0.36f);
    dm_fx_sidechain(rev,  kicks, nkick, 0.34f, 0.005, 0.15);
    dm_fx_sidechain(send, kicks, nkick, 0.34f, 0.005, 0.15);

    dm_audio_mix(out, drums, 0.76f);
    dm_audio_mix(out, music, 0.74f);
    dm_audio_mix(out, rev,   0.38f);
    dm_audio_mix(out, send,  0.20f);

    dm_fx_highpass(out, 28.0f);
    dm_fx_widen(out, 0.20f);
    dm_fx_saturate(out, 0.12f);
    dm_fx_limiter(out, -1.0f, 0.004, 0.12);

    /* Phones start muted and the viewer unmutes a second in, so the opening
     * cannot afford a slow fade -- 20 ms is enough to kill the click. */
    dm_fx_fade(out, 0.0, 0.02, 0.0f, 1.0f);
    dm_fx_fade(out, SONG_LEN_SEC - 0.9, SONG_LEN_SEC, 1.0f, 0.0f);

    dm_audio_free(drums);
    dm_audio_free(music);
    dm_audio_free(send);
    dm_audio_free(rev);
    return out;
}
