#include "song.h"
#include "../../engine/dm_synth.h"

#include <stdlib.h>

const int SONG_SECTION_BAR[SEC_COUNT + 1] = { 0, 2, 5, 9, 12, 14 };

song_section song_section_at(f64 t)
{
    f64 bar = t / SONG_BAR_SEC;
    for (int s = SEC_COUNT - 1; s >= 0; s--)
        if (bar >= SONG_SECTION_BAR[s]) return (song_section)s;
    return SEC_HOLD;
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

/* E minor, one chord per bar over a four-bar loop, offset so that the bar the
 * vehicle is released on is the tonic. The two bars before it are therefore G
 * and D -- the relative major and the dominant, which is the oldest way there
 * is of saying "something is about to happen" without saying anything.
 *
 * Voicings keep the top voice on B or C throughout. The chord underneath moves
 * and the colour on top does not, so the pad reads as one held note being
 * re-lit rather than as four chords being announced.
 */
static const u8 CHORD[4][3] = {
    { 52, 55, 59 },   /* Em : E3 G3 B3    */
    { 52, 55, 60 },   /* C  : E3 G3 C4    */
    { 50, 55, 59 },   /* G  : D3 G3 B3    */
    { 50, 54, 57 },   /* D  : D3 F#3 A3   */
};

static const u8 ROOT[4] = { 40, 36, 43, 38 };   /* E2 C2 G2 D2 */

static int chord_of_bar(int bar) { return (bar + 2) & 3; }

/* The arp of the climb. Sixteenths, up and back, with the octave jumps put
 * where the picture is moving fastest. */
static const u8 ARP[16]     = { 0, 1, 2, 1, 2, 1, 0, 2, 0, 1, 2, 1, 2, 0, 1, 2 };
static const u8 ARP_OCT[16] = { 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 1, 0, 1, 0, 1 };

/* ---- patches ------------------------------------------------------------ */

/* The hold. One note, two bars, no movement in it at all: a launch vehicle on
 * the pad is a very large object doing nothing, and the sound has to be that
 * before it can be anything else. */
static dm_patch patch_drone(void)
{
    dm_patch p = dm_patch_default();
    p.kind = DM_OSC_SAW;
    p.unison = 4; p.detune_cents = 8.0f; p.spread = 0.35f;
    p.sub_level = 0.85f;
    p.amp_a = 0.90f; p.amp_d = 2.00f; p.amp_s = 0.80f; p.amp_r = 2.20f;
    p.flt_a = 1.40f; p.flt_d = 2.40f; p.flt_s = 0.55f; p.flt_r = 2.00f;
    p.cutoff = 90.0f; p.env_amount = 460.0f; p.resonance = 0.16f;
    p.drive = 0.22f; p.gain = 0.105f; p.send = 0.30f;
    return p;
}

/* The count. A range clock beeps once a second and this one beeps once a beat,
 * which is the same thing done in the tempo of the piece: short, hard, and the
 * only thing with any transient in it until the release. */
static dm_patch patch_beep(void)
{
    dm_patch p = dm_patch_default();
    p.kind = DM_OSC_SINE;
    p.amp_a = 0.001f; p.amp_d = 0.075f; p.amp_s = 0.0f; p.amp_r = 0.05f;
    p.flt_a = 0.001f; p.flt_d = 0.060f; p.flt_s = 0.0f; p.flt_r = 0.05f;
    p.cutoff = 2400.0f; p.env_amount = 900.0f; p.resonance = 0.05f;
    p.gain = 0.070f; p.send = 0.22f;
    return p;
}

static dm_patch patch_pad(void)
{
    dm_patch p = dm_patch_default();
    p.kind = DM_OSC_SAW;
    p.unison = 6; p.detune_cents = 20.0f; p.spread = 0.95f;
    p.amp_a = 0.30f; p.amp_d = 1.30f; p.amp_s = 0.78f; p.amp_r = 1.80f;
    p.flt_a = 0.55f; p.flt_d = 2.40f; p.flt_s = 0.52f; p.flt_r = 1.60f;
    p.cutoff = 230.0f; p.env_amount = 1650.0f; p.resonance = 0.20f;
    p.gain = 0.076f; p.send = 0.66f;
    return p;
}

static dm_patch patch_bass(void)
{
    dm_patch p = dm_patch_default();
    p.kind = DM_OSC_SAW;
    p.unison = 2; p.detune_cents = 5.0f; p.spread = 0.06f;
    p.sub_level = 0.72f;
    p.amp_a = 0.002f; p.amp_d = 0.26f; p.amp_s = 0.66f; p.amp_r = 0.12f;
    p.flt_a = 0.002f; p.flt_d = 0.18f; p.flt_s = 0.32f; p.flt_r = 0.12f;
    p.cutoff = 95.0f; p.env_amount = 820.0f; p.key_track = 0.32f;
    p.resonance = 0.28f; p.drive = 0.50f;
    p.gain = 0.122f; p.send = 0.05f;
    return p;
}

static dm_patch patch_arp(void)
{
    dm_patch p = dm_patch_default();
    p.kind = DM_OSC_SAW;
    p.unison = 3; p.detune_cents = 13.0f; p.spread = 0.80f;
    p.amp_a = 0.002f; p.amp_d = 0.13f; p.amp_s = 0.08f; p.amp_r = 0.18f;
    p.flt_a = 0.002f; p.flt_d = 0.16f; p.flt_s = 0.16f; p.flt_r = 0.18f;
    p.cutoff = 620.0f; p.env_amount = 3600.0f; p.key_track = 0.45f;
    p.resonance = 0.44f; p.drive = 0.18f;
    p.gain = 0.052f; p.send = 0.72f;
    return p;
}

/* Vacuum. Nothing to hide behind and nothing to be loud over, so it is a plain
 * tone with a very long tail and almost all of it going to the reverb. */
static dm_patch patch_bell(void)
{
    dm_patch p = dm_patch_default();
    p.kind = DM_OSC_SINE;
    p.amp_a = 0.006f; p.amp_d = 1.10f; p.amp_s = 0.0f; p.amp_r = 1.60f;
    p.flt_a = 0.006f; p.flt_d = 0.90f; p.flt_s = 0.0f; p.flt_r = 1.30f;
    p.cutoff = 1500.0f; p.env_amount = 2400.0f; p.resonance = 0.08f;
    p.gain = 0.090f; p.send = 0.88f;
    return p;
}

/* ---- the track ----------------------------------------------------------- */

dm_audio *song_render(void)
{
    /* A second and a half of tail past the end so the last reverb is not
     * chopped; the video stops at SONG_LEN_SEC and ffmpeg trims the rest. */
    int frames = dm_sec_to_frames(SONG_LEN_SEC + 1.5);

    dm_audio *out   = dm_audio_new(frames);
    dm_audio *drums = dm_audio_new(frames);
    dm_audio *music = dm_audio_new(frames);
    dm_audio *send  = dm_audio_new(frames);
    dm_audio *rev   = dm_audio_new(frames);
    if (!out || !drums || !music || !send || !rev) return NULL;

    dm_patch drone = patch_drone(), beep = patch_beep(), pad = patch_pad();
    dm_patch bass  = patch_bass(),  arp  = patch_arp(), bell = patch_bell();

    const f64 stp = SONG_STEP_SEC;
    u32 seed = 7u;

    f64 kicks[SONG_BARS * 4];
    int nkick = 0;

    for (int bar = 0; bar < SONG_BARS; bar++) {
        f64 t0 = song_bar(bar);
        int ch = chord_of_bar(bar);
        song_section sec = song_section_at(t0 + 0.001);

        /* -- the hold: one low note and a clock ----------------------------- */
        if (sec == SEC_HOLD) {
            if (bar == 0)
                dm_synth_note(music, send, &drone, 0.0, SONG_BAR_SEC * 2.10,
                              28.0f, 1.00f, seed++);   /* E1 */

            /* Eight beeps, one per beat, counting the section out. The last
             * three are the seconds the engines are already running for. */
            for (int b = 0; b < 4; b++) {
                int n = bar * 4 + b;
                f32 vel = n >= 5 ? 0.95f : 0.62f;
                dm_synth_note(music, send, &beep, t0 + song_beat(b), 0.10,
                              n >= 5 ? 83.0f : 78.0f, vel, seed++);
            }

            /* The pad slides in under the last bar of the count. */
            if (bar == 1)
                for (int v = 0; v < 3; v++)
                    dm_synth_note(music, send, &pad, t0, SONG_BAR_SEC * 1.4,
                                  (f32)CHORD[ch][v], 0.55f, seed++);
        }

        /* -- pad: from the release onward it is the thread through the piece - */
        if (sec == SEC_LIFT || sec == SEC_CLIMB || sec == SEC_STAGE) {
            /* The vacuum section is quieter, not absent. Losing the drums and
             * the arp is already a seventeen decibel drop; letting the pad go
             * with them reads on a phone as the audio having failed rather
             * than as the engines having stopped. */
            f32 g = sec == SEC_STAGE ? 0.95f : 1.0f;
            f64 len = sec == SEC_STAGE ? SONG_BAR_SEC * 1.9 : SONG_BAR_SEC * 1.05;
            for (int v = 0; v < 3; v++)
                dm_synth_note(music, send, &pad, t0, len,
                              (f32)CHORD[ch][v], 0.88f * g, seed++);
        }

        /* -- drums: they exist only while there is air --------------------- */
        if (sec == SEC_LIFT || sec == SEC_CLIMB) {
            for (int b = 0; b < 4; b++) {
                f64 tk = t0 + song_beat(b);
                dm_drum_kick(drums, send, tk, b == 0 ? 1.0f : 0.88f, 0.10f);
                if (nkick < DM_COUNT(kicks)) kicks[nkick++] = tk;
            }
            dm_drum_clap(drums, send, t0 + song_beat(1.0), 0.80f, 0.34f);
            dm_drum_clap(drums, send, t0 + song_beat(3.0), 0.80f, 0.34f);

            int dense = sec == SEC_CLIMB;
            for (int s = 2; s < 16; s += dense ? 2 : 4) {
                int open = (s == 14);
                dm_drum_hat(drums, send, t0 + s * stp,
                            open ? 0.60f : (dense ? 0.38f : 0.44f),
                            open ? 0.52f : 0.10f, 0.16f);
            }
        }

        /* The release itself gets two toms under the first kick. A kick alone
         * is a transient; a launch vehicle leaving the ground is a mass. */
        if (bar == SONG_SECTION_BAR[SEC_LIFT]) {
            dm_drum_tom(drums, send, t0,              1.00f, 62.0f, 0.30f);
            dm_drum_tom(drums, send, t0 + 3.0 * stp,  0.62f, 48.0f, 0.35f);
        }

        /* -- bass ----------------------------------------------------------- */
        if (sec == SEC_LIFT || sec == SEC_CLIMB) {
            dm_synth_note(music, send, &bass, t0,            stp * 6.0, (f32)ROOT[ch],      1.00f, seed++);
            dm_synth_note(music, send, &bass, t0 + 6  * stp, stp * 2.0, (f32)ROOT[ch],      0.70f, seed++);
            dm_synth_note(music, send, &bass, t0 + 10 * stp, stp * 3.0, (f32)ROOT[ch] + 12, 0.78f, seed++);
            dm_synth_note(music, send, &bass, t0 + 14 * stp, stp * 2.0, (f32)ROOT[ch],      0.66f, seed++);
        }

        /* -- the arp is what makes the climb a climb ------------------------ */
        if (sec == SEC_CLIMB) {
            for (int s = 0; s < 16; s++) {
                f32 note = (f32)CHORD[ch][ARP[s]] + 12.0f * (f32)ARP_OCT[s];
                f32 vel  = (s & 3) == 0 ? 0.95f : 0.68f;
                dm_synth_note(music, send, &arp, t0 + s * stp, stp * 0.9, note, vel, seed++);
            }
        }

        /* -- cut-off: there is no air out there, and the mix says so -------- */
        if (sec == SEC_STAGE) {
            int k = bar - SONG_SECTION_BAR[SEC_STAGE];

            /* One deep hit on the cut-off itself, and nothing after it. The
             * silence is the point: the engines stop and the sound stops with
             * them, because from here on there is nothing to carry it. */
            if (k == 0) {
                dm_drum_kick(drums, send, t0, 0.85f, 0.55f);
                if (nkick < DM_COUNT(kicks)) kicks[nkick++] = t0;
            }
            /* The second stage lights on the downbeat of the middle bar. One
             * tone, an octave above the top of the chord, so it reads as
             * something starting rather than as the piece resuming. */
            if (k == 1)
                dm_synth_note(music, send, &bell, t0, SONG_BAR_SEC * 1.8,
                              (f32)CHORD[ch][2] + 12.0f, 1.00f, seed++);
        }

        /* -- orbit: one chord, held, decaying into the loop point ----------- */
        if (sec == SEC_ORBIT && bar == SONG_SECTION_BAR[SEC_ORBIT]) {
            dm_drum_kick(drums, send, t0, 0.95f, 0.40f);
            if (nkick < DM_COUNT(kicks)) kicks[nkick++] = t0;

            /* E minor with the ninth on top -- open, unresolved, and the right
             * shape for a thing that has arrived somewhere but not stopped. */
            static const f32 ORBIT[4] = { 40.0f, 52.0f, 59.0f, 66.0f };
            for (int v = 0; v < 4; v++)
                dm_synth_note(music, send, &pad, t0, SONG_BAR_SEC * 2.6,
                              ORBIT[v], 0.80f, seed++);
            for (int v = 1; v < 4; v++)
                dm_synth_note(music, send, &bell, t0, SONG_BAR_SEC * 2.4,
                              ORBIT[v] + 12.0f, 0.55f, seed++);
        }
    }

    /* -- transitions ------------------------------------------------------- */

    /* The riser under the count lands exactly on the release. */
    dm_fx_riser(drums, song_bar(SONG_SECTION_BAR[SEC_LIFT]),  song_bar(2.0), 0.85f);
    dm_fx_riser(drums, song_bar(SONG_SECTION_BAR[SEC_CLIMB]), song_bar(1.0), 0.55f);
    /* Falling, not rising: the cut to vacuum is a loss, not an arrival. */
    dm_fx_sweep(drums, song_bar(SONG_SECTION_BAR[SEC_STAGE]), song_bar(1.5), 0.60f, 0);
    dm_fx_riser(drums, song_bar(SONG_SECTION_BAR[SEC_ORBIT]), song_bar(1.0), 0.60f);

    /* -- mix --------------------------------------------------------------- */
    dm_fx_sidechain(music, kicks, nkick, 0.52f, 0.005, 0.16);

    dm_audio_mix(rev, send, 1.0f);
    dm_fx_reverb(rev, 0.88f, 0.30f, 0.95f);
    dm_fx_highpass(rev, 190.0f);
    /* Dotted eighth at 112 BPM is 0.4018 s; the quarter on the right puts the
     * repeats apart without smearing the arp. */
    dm_fx_delay(send, 0.4018, 0.5357, 0.40f, 0.36f);
    dm_fx_sidechain(rev,  kicks, nkick, 0.32f, 0.005, 0.16);
    dm_fx_sidechain(send, kicks, nkick, 0.32f, 0.005, 0.16);

    dm_audio_mix(out, drums, 0.78f);
    dm_audio_mix(out, music, 0.74f);
    dm_audio_mix(out, rev,   0.40f);
    dm_audio_mix(out, send,  0.20f);

    dm_fx_highpass(out, 26.0f);
    dm_fx_widen(out, 0.22f);
    dm_fx_saturate(out, 0.12f);
    dm_fx_limiter(out, -1.0f, 0.004, 0.12);

    /* Phones start muted and the viewer unmutes a second in, so the opening
     * cannot afford a slow fade -- 20 ms is enough to kill the click. */
    dm_fx_fade(out, 0.0, 0.02, 0.0f, 1.0f);
    dm_fx_fade(out, SONG_LEN_SEC - 1.0, SONG_LEN_SEC, 1.0f, 0.0f);

    dm_audio_free(drums);
    dm_audio_free(music);
    dm_audio_free(send);
    dm_audio_free(rev);
    return out;
}
