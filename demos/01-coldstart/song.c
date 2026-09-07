#include "song.h"
#include "../../engine/dm_synth.h"

#include <stdlib.h>

const int SONG_SECTION_BAR[SEC_COUNT + 1] = { 0, 4, 8, 16, 24, 28, 32, 48, 56, 60 };

song_section song_section_at(f64 t)
{
    f64 bar = t / SONG_BAR_SEC;
    for (int s = SEC_COUNT - 1; s >= 0; s--)
        if (bar >= SONG_SECTION_BAR[s]) return (song_section)s;
    return SEC_INTRO_PAD;
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

/* One chord per bar, repeating every four: Am - F - C - G. The i-VI-III-VII
 * loop is the spine of the entire genre; the voicings below are inverted so
 * the top voice barely moves, which keeps the pad from sounding like block
 * chords being stamped out. */
static const u8 CHORD[4][3] = {
    { 57, 60, 64 },   /* Am : A3 C4 E4 */
    { 53, 57, 60 },   /* F  : F3 A3 C4 */
    { 52, 55, 60 },   /* C  : E3 G3 C4 (first inversion)  */
    { 50, 55, 59 },   /* G  : D3 G3 B3 (second inversion) */
};

static const u8 ROOT[4] = { 45, 41, 48, 43 };   /* A2 F2 C3 G2 */

typedef struct { u16 step; u8 len; u8 note; f32 vel; } snote;

/* The theme. Eight bars, 128 sixteenths, stated over two turns of the chord
 * loop: a rising answer in the second half so it does not just repeat. */
static const snote THEME[] = {
    {   0, 6, 69, 1.00f }, {   6, 2, 72, 0.80f }, {   8, 8, 76, 0.95f },
    {  16, 6, 77, 1.00f }, {  22, 2, 76, 0.78f }, {  24, 8, 72, 0.90f },
    {  32, 4, 67, 0.85f }, {  36, 4, 72, 0.88f }, {  40, 4, 76, 0.92f }, { 44, 4, 79, 0.98f },
    {  48, 6, 74, 0.95f }, {  54, 2, 71, 0.75f }, {  56, 8, 67, 0.85f },
    {  64, 6, 69, 1.00f }, {  70, 2, 72, 0.80f }, {  72, 4, 76, 0.95f }, { 76, 4, 81, 1.00f },
    {  80, 8, 77, 0.95f }, {  88, 8, 76, 0.88f },
    {  96, 4, 79, 0.95f }, { 100, 4, 76, 0.85f }, { 104, 8, 72, 0.90f },
    { 112, 4, 74, 0.90f }, { 116, 4, 71, 0.82f }, { 120, 8, 69, 0.95f },
};

/* ---- patches ------------------------------------------------------------ */

static dm_patch patch_pad(void)
{
    dm_patch p = dm_patch_default();
    p.kind = DM_OSC_SAW;
    p.unison = 6; p.detune_cents = 26.0f; p.spread = 0.95f;
    p.amp_a = 1.10f; p.amp_d = 2.00f; p.amp_s = 0.80f; p.amp_r = 2.20f;
    p.flt_a = 1.60f; p.flt_d = 3.00f; p.flt_s = 0.55f; p.flt_r = 2.00f;
    p.cutoff = 260.0f; p.env_amount = 1300.0f; p.resonance = 0.18f;
    p.gain = 0.085f; p.send = 0.60f;
    return p;
}

static dm_patch patch_bass(void)
{
    dm_patch p = dm_patch_default();
    p.kind = DM_OSC_SAW;
    p.unison = 2; p.detune_cents = 7.0f; p.spread = 0.10f;
    p.sub_level = 0.55f;
    p.amp_a = 0.002f; p.amp_d = 0.13f; p.amp_s = 0.00f; p.amp_r = 0.05f;
    p.flt_a = 0.001f; p.flt_d = 0.075f; p.flt_s = 0.00f; p.flt_r = 0.05f;
    /* A short filter envelope on every sixteenth is what turns a held saw into
     * a plucked bass line. Almost all of the groove lives in flt_d. */
    p.cutoff = 150.0f; p.env_amount = 2400.0f; p.resonance = 0.42f;
    p.drive = 0.35f; p.gain = 0.34f; p.send = 0.05f;
    return p;
}

static dm_patch patch_arp(void)
{
    dm_patch p = dm_patch_default();
    p.kind = DM_OSC_SQUARE; p.pulse_width = 0.34f;
    p.unison = 2; p.detune_cents = 11.0f; p.spread = 0.55f;
    p.amp_a = 0.002f; p.amp_d = 0.10f; p.amp_s = 0.00f; p.amp_r = 0.09f;
    p.flt_a = 0.001f; p.flt_d = 0.065f; p.flt_s = 0.00f; p.flt_r = 0.08f;
    p.cutoff = 800.0f; p.env_amount = 4200.0f; p.resonance = 0.55f;
    p.gain = 0.13f; p.send = 0.40f;
    return p;
}

static dm_patch patch_lead(void)
{
    dm_patch p = dm_patch_default();
    p.kind = DM_OSC_SAW;
    p.unison = 7; p.detune_cents = 30.0f; p.spread = 0.85f;
    p.amp_a = 0.020f; p.amp_d = 0.60f; p.amp_s = 0.72f; p.amp_r = 0.35f;
    p.flt_a = 0.030f; p.flt_d = 0.70f; p.flt_s = 0.45f; p.flt_r = 0.35f;
    p.cutoff = 1300.0f; p.env_amount = 3400.0f; p.resonance = 0.22f;
    p.vib_rate = 5.2f; p.vib_depth = 0.11f;
    p.drive = 0.22f; p.gain = 0.155f; p.send = 0.45f;
    return p;
}

static dm_patch patch_bell(void)
{
    dm_patch p = dm_patch_default();
    p.kind = DM_OSC_TRI;
    p.unison = 3; p.detune_cents = 9.0f; p.spread = 0.7f;
    p.amp_a = 0.001f; p.amp_d = 0.55f; p.amp_s = 0.0f; p.amp_r = 0.5f;
    p.flt_a = 0.001f; p.flt_d = 0.40f; p.flt_s = 0.1f; p.flt_r = 0.4f;
    p.cutoff = 1200.0f; p.env_amount = 3800.0f; p.resonance = 0.30f;
    p.gain = 0.12f; p.send = 0.70f;
    return p;
}

/* ---- arrangement -------------------------------------------------------- */

dm_audio *song_render(void)
{
    int n = dm_sec_to_frames(SONG_LEN_SEC + 5.0);

    dm_audio *drums = dm_audio_new(n);
    dm_audio *music = dm_audio_new(n);
    dm_audio *send  = dm_audio_new(n);
    dm_audio *rev   = dm_audio_new(n);
    dm_audio *out   = dm_audio_new(n);
    if (!drums || !music || !send || !rev || !out) return NULL;

    dm_patch pad = patch_pad(), bass = patch_bass();
    dm_patch arp = patch_arp(), lead = patch_lead(), bell = patch_bell();

    f64 kicks[512];
    int nkick = 0;
    u32 seed = 1;

    for (int bar = 0; bar < SONG_BARS; bar++) {
        song_section sec = song_section_at(song_bar(bar));
        f64 t0  = song_bar(bar);
        int ch  = bar & 3;
        f64 stp = SONG_STEP_SEC;

        int drums_on   = (sec == SEC_INTRO_KICK || sec == SEC_GROOVE || sec == SEC_LEAD_A ||
                          sec == SEC_DROP || sec == SEC_CLIMAX);
        int bass_on    = (sec == SEC_GROOVE || sec == SEC_LEAD_A || sec == SEC_DROP || sec == SEC_CLIMAX);
        int arp_on     = (sec == SEC_LEAD_A || sec == SEC_DROP || sec == SEC_CLIMAX);
        int lead_on    = (sec == SEC_DROP || sec == SEC_CLIMAX);
        int pad_on     = (sec != SEC_BUILD);

        /* -- pad: one long chord per bar, always the harmonic floor -------- */
        if (pad_on) {
            f32 g = (sec == SEC_BREAK || sec == SEC_INTRO_PAD || sec == SEC_OUTRO) ? 1.0f : 0.65f;
            for (int v = 0; v < 3; v++) {
                dm_synth_note(music, send, &pad, t0, SONG_BAR_SEC * 0.98,
                              (f32)CHORD[ch][v], 0.85f * g, seed++);
                /* An octave doubling an octave up, quieter, for air. */
                dm_synth_note(music, send, &pad, t0, SONG_BAR_SEC * 0.98,
                              (f32)CHORD[ch][v] + 12.0f, 0.32f * g, seed++);
            }
        }

        /* -- drums --------------------------------------------------------- */
        if (drums_on) {
            for (int b = 0; b < 4; b++) {
                f64 t = t0 + b * song_beat(1.0);
                dm_drum_kick(drums, send, t, b == 0 ? 1.0f : 0.92f, 0.02f);
                if (nkick < 512) kicks[nkick++] = t;
            }
            /* Clap on the backbeat, snare doubling it once the drop lands. */
            dm_drum_clap(drums, send, t0 + song_beat(1.0), 0.95f, 0.30f);
            dm_drum_clap(drums, send, t0 + song_beat(3.0), 0.95f, 0.30f);
            if (sec == SEC_DROP || sec == SEC_CLIMAX) {
                dm_drum_snare(drums, send, t0 + song_beat(1.0), 0.55f, 0.22f);
                dm_drum_snare(drums, send, t0 + song_beat(3.0), 0.55f, 0.22f);
            }
            for (int s = 2; s < 16; s += 4)
                dm_drum_hat(drums, send, t0 + s * stp, 0.75f, s == 14 ? 0.55f : 0.0f, 0.10f);
            if (sec == SEC_CLIMAX)
                for (int s = 0; s < 16; s += 4)
                    dm_drum_hat(drums, send, t0 + s * stp, 0.35f, 0.0f, 0.06f);
        } else if (sec == SEC_BREAK) {
            /* Half time: only the downbeat and the "and" of three. */
            dm_drum_kick(drums, send, t0, 0.85f, 0.05f);
            if (nkick < 512) kicks[nkick++] = t0;
            dm_drum_clap(drums, send, t0 + song_beat(2.0), 0.6f, 0.55f);
        }

        /* -- build: accelerating snare roll into the drop ------------------ */
        if (sec == SEC_BUILD) {
            int bar_in = bar - SONG_SECTION_BAR[SEC_BUILD];   /* 0..3 */
            int div    = bar_in < 2 ? 4 : (bar_in < 3 ? 2 : 1);
            for (int s = 0; s < 16; s += div) {
                f32 v = 0.35f + 0.65f * ((f32)(bar_in * 16 + s) / 63.0f);
                dm_drum_snare(drums, send, t0 + s * stp, v * 0.8f, 0.25f);
            }
            dm_drum_kick(drums, send, t0, 0.9f, 0.02f);
            if (nkick < 512) kicks[nkick++] = t0;
        }

        /* -- bass: straight sixteenths, octave jumps for the lift ---------- */
        if (bass_on) {
            for (int s = 0; s < 16; s++) {
                f32 v = (s % 4 == 0) ? 1.00f : ((s % 2 == 0) ? 0.80f : 0.66f);
                int oct = ((bar & 1) && (s == 6 || s == 7 || s == 14 || s == 15)) ? 12 : 0;
                dm_synth_note(music, send, &bass, t0 + s * stp, stp * 0.92,
                              (f32)ROOT[ch] + oct, v, seed++);
            }
        }

        /* -- arp: sixteenths climbing the chord over two octaves ----------- */
        if (arp_on) {
            static const int SHAPE[16] = { 0,1,2,3, 4,3,2,1, 0,2,4,5, 4,2,1,0 };
            for (int s = 0; s < 16; s++) {
                int k = SHAPE[s];
                f32 note = (f32)CHORD[ch][k % 3] + 12.0f * (f32)(k / 3) + 12.0f;
                dm_synth_note(music, send, &arp, t0 + s * stp, stp * 0.85,
                              note, s % 4 == 0 ? 0.95f : 0.7f, seed++);
            }
        }

        /* -- lead: the theme, doubled an octave up in the climax ----------- */
        if (lead_on) {
            int phrase_bar = (bar - SONG_SECTION_BAR[SEC_DROP]) & 7;
            for (int i = 0; i < DM_COUNT(THEME); i++) {
                int nb = THEME[i].step / 16;
                if (nb != phrase_bar) continue;
                f64 t = t0 + (THEME[i].step % 16) * stp;
                dm_synth_note(music, send, &lead, t, THEME[i].len * stp * 0.95,
                              (f32)THEME[i].note, THEME[i].vel, seed++);
                if (sec == SEC_CLIMAX)
                    dm_synth_note(music, send, &bell, t, THEME[i].len * stp * 0.95,
                                  (f32)THEME[i].note + 12.0f, THEME[i].vel * 0.5f, seed++);
            }
        }

        /* -- breakdown: sparse bell statement of the theme ----------------- */
        if (sec == SEC_BREAK) {
            int phrase_bar = (bar - SONG_SECTION_BAR[SEC_BREAK]) & 7;
            for (int i = 0; i < DM_COUNT(THEME); i++) {
                int nb = THEME[i].step / 16;
                if (nb != phrase_bar) continue;
                if ((THEME[i].step % 16) % 4 != 0) continue;   /* thin it out */
                dm_synth_note(music, send, &bell, t0 + (THEME[i].step % 16) * stp,
                              THEME[i].len * stp, (f32)THEME[i].note, THEME[i].vel * 0.8f, seed++);
            }
        }

        /* -- outro: the theme once more, bell only, thinning out ----------- */
        if (sec == SEC_OUTRO) {
            int phrase_bar = (bar - SONG_SECTION_BAR[SEC_OUTRO]) & 7;
            for (int i = 0; i < DM_COUNT(THEME); i++) {
                if (THEME[i].step / 16 != phrase_bar) continue;
                dm_synth_note(music, send, &bell, t0 + (THEME[i].step % 16) * stp,
                              THEME[i].len * stp, (f32)THEME[i].note, THEME[i].vel * 0.6f, seed++);
            }
        }
    }

    /* -- transitions ------------------------------------------------------- */
    dm_fx_riser(drums, song_bar(SONG_SECTION_BAR[SEC_GROOVE]),   song_bar(2.0), 0.55f);
    dm_fx_riser(drums, song_bar(SONG_SECTION_BAR[SEC_LEAD_A]),   song_bar(2.0), 0.60f);
    dm_fx_riser(drums, song_bar(SONG_SECTION_BAR[SEC_DROP]),     song_bar(4.0), 0.95f);
    dm_fx_riser(drums, song_bar(SONG_SECTION_BAR[SEC_CLIMAX]),   song_bar(2.0), 0.70f);
    dm_fx_sweep(drums, song_bar(SONG_SECTION_BAR[SEC_BREAK]),    song_bar(1.0), 0.55f, 0);
    dm_fx_sweep(drums, song_bar(SONG_SECTION_BAR[SEC_OUTRO]),    song_bar(1.0), 0.45f, 0);

    /* -- mix --------------------------------------------------------------- */

    /* The pump. Ducking everything melodic against the kick is the single
     * gesture that makes this genre breathe, and it has to happen before the
     * effects so the reverb tail pumps with it. */
    dm_fx_sidechain(music, kicks, nkick, 0.52f, 0.006, 0.17);

    dm_audio_mix(rev, send, 1.0f);          /* copy the send bus */
    dm_fx_reverb(rev, 0.86f, 0.36f, 0.92f);
    dm_fx_highpass(rev, 180.0f);            /* keep the low end out of the tail */
    dm_fx_delay(send, 0.375, 0.500, 0.44f, 0.38f);   /* dotted eighth / quarter */
    dm_fx_sidechain(rev,  kicks, nkick, 0.35f, 0.006, 0.17);
    dm_fx_sidechain(send, kicks, nkick, 0.35f, 0.006, 0.17);

    /* Deliberately mixed about 3 dB below the ceiling. YouTube normalises to
     * roughly -14 LUFS anyway, so pushing the limiter harder would only trade
     * dynamics for loudness that the platform then takes straight back. */
    dm_audio_mix(out, drums, 0.72f);
    dm_audio_mix(out, music, 0.72f);
    dm_audio_mix(out, rev,   0.40f);
    dm_audio_mix(out, send,  0.22f);

    dm_fx_highpass(out, 26.0f);
    dm_fx_widen(out, 0.22f);
    dm_fx_saturate(out, 0.10f);
    dm_fx_limiter(out, -1.0f, 0.004, 0.13);

    dm_fx_fade(out, 0.0, 0.04, 0.0f, 1.0f);
    dm_fx_fade(out, SONG_LEN_SEC - 2.5, SONG_LEN_SEC + 1.5, 1.0f, 0.0f);

    dm_audio_free(drums);
    dm_audio_free(music);
    dm_audio_free(send);
    dm_audio_free(rev);
    return out;
}
