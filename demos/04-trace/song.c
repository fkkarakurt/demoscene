#include "song.h"
#include "xy.h"
#include "../../engine/dm_synth.h"

const int SONG_SECTION_BAR[SEC_COUNT + 1] = { 0, 2, 5, 8, 13, 15 };

song_section song_section_at(f64 t)
{
    f64 bar = t / SONG_BAR_SEC;
    for (int s = SEC_COUNT - 1; s >= 0; s--)
        if (bar >= SONG_SECTION_BAR[s]) return (song_section)s;
    return SEC_TUNE;
}

f64 song_section_start(song_section s) { return SONG_SECTION_BAR[s] * SONG_BAR_SEC; }
f64 song_section_end  (song_section s) { return SONG_SECTION_BAR[s + 1] * SONG_BAR_SEC; }

f64 song_section_phase(f64 t)
{
    song_section s = song_section_at(t);
    f64 a = song_section_start(s), b = song_section_end(s);
    return b > a ? dm_clamp((f32)((t - a) / (b - a)), 0.0f, 1.0f) : 0.0f;
}

/* ---- tuning ----------------------------------------------------------------
 *
 * Just intonation over A = 55 Hz. Every pitch in the piece is 55 Hz times a
 * ratio of small whole numbers, so any two voices sounding at once are in a
 * ratio of small whole numbers too, and the figure they trace closes on
 * itself and stands still. In equal temperament nothing but the octave does:
 * a tempered major third beats against a pure one four times a second at this
 * pitch, and the figure would turn four times a second with it.
 */
#define A1 55.0f

static inline f32 ji(int num, int den) { return A1 * (f32)num / (f32)den; }

/* Root, fifth, and the third an octave up -- a spread voicing, because three
 * circles close together in size trace a blob and three far apart trace a
 * figure. The major chords are 2:3:5 and close dozens of times a second; the
 * minor one is 10:15:24 and takes four to six times as long, so it is visibly
 * the more intricate shape. */
typedef struct { f32 hz[3]; } chord;

static const chord AM = { { 110.0f, 165.0f,  264.0f  } };   /* A2 E3 C4  */
static const chord FM = { {  88.0f, 132.0f,  220.0f  } };   /* F2 C3 A3  */
static const chord CM = { { 132.0f, 198.0f,  330.0f  } };   /* C3 G3 E4  */
static const chord EM = { {  82.5f, 123.75f, 206.25f } };   /* E2 B2 G#3 */

static const chord *PULSE_CHORDS[3] = { &AM, &FM, &CM };
static const chord *DROP_CHORDS[5]  = { &AM, &FM, &CM, &EM, &AM };

/* ---- the one rule ------------------------------------------------------------
 *
 * Everything that sounds at once is drawn at once, on top of itself, and the
 * persistence stacks the last second of it behind. So the arrangement is
 * written to take turns: the kick has the downbeat to itself, the chord the
 * off-beat, and noise -- which draws nothing but scribble -- is kept out of
 * the picture except where it is wrapped around a tone.
 */

static xy_voice voice_partial(f32 size, f32 dir)
{
    xy_voice v = xy_voice_default();
    v.size  = size;
    v.dir   = dir;
    /* The release is short enough to be gone before the kick has opened: the
     * downbeat is the cut, in the picture as much as in the sound. */
    v.amp_a = 0.30f; v.amp_d = 1.0f; v.amp_s = 1.0f; v.amp_r = 0.012f;
    /* Enter a little flat and slide into tune: the figure turns while the
     * interval beats and stops dead when it is pure. */
    v.detune = 0.55f;
    v.glide  = 0.45f;
    return v;
}

/* A house stab: short, on the off-beat, out of the kick's way. */
static xy_voice voice_stab(f32 size, f32 dir)
{
    xy_voice v = xy_voice_default();
    v.size  = size;
    v.dir   = dir;
    v.amp_a = 0.003f; v.amp_d = 0.10f; v.amp_s = 0.72f; v.amp_r = 0.035f;
    return v;
}

/* ---- the track ------------------------------------------------------------- */

dm_audio *song_render(void)
{
    int frames = dm_sec_to_frames(SONG_LEN_SEC + 0.5);

    dm_audio *out   = dm_audio_new(frames);
    dm_audio *drums = dm_audio_new(frames);
    dm_audio *music = dm_audio_new(frames);
    dm_audio *lead  = dm_audio_new(frames);
    if (!out || !drums || !music || !lead) return NULL;

    xy_table *square = xy_table_polygon(4, 1, 4096);
    xy_table *word   = xy_table_text("KORMOS", 0.86f, 6.0f, 8192);
    if (!square || !word) return NULL;

    f64 kicks[SONG_BARS * 4];
    int nkick = 0;
    u32 seed = 11u;

    /* -- TUNE: the harmonic series of 55 Hz, a partial at a time ------------- */
    {
        /* Partials 2, 3, 5 and 7: A, E, C sharp, and the seventh harmonic,
         * which no piano has -- it lies a third of a semitone below the
         * tempered G and only just intonation can reach it. Alternate
         * partials turn the other way, which makes the sum a rosette rather
         * than a loop inside a loop.
         *
         * Over the last beat every partial but the root bends sharp, each by
         * more than the one below it. Nothing is in tune with anything any
         * more, so the figure spins, faster the further they go -- a riser
         * made of beating -- and the downbeat cuts it off. */
        /* An octave above the series of 55 Hz rather than on it. The ratios
         * are the ratios, so the figure is the same; but a phone speaker
         * gives back almost nothing below two hundred hertz, and this is the
         * part of the piece that has nothing else to be heard by. */
        static const int PART[4] = { 4, 6, 10, 14 };
        static const f32 SIZE[4] = { 0.23f, 0.12f, 0.072f, 0.047f };
        static const f32 BEAT[4] = { 0.0f, 2.0f, 4.0f, 5.0f };
        static const f32 BEND[4] = { 0.0f, 0.35f, 0.70f, 1.05f };
        f64 end = song_bar(2.0);
        for (int k = 0; k < 4; k++) {
            xy_voice v = voice_partial(SIZE[k], (k & 1) ? -1.0f : 1.0f);
            if (k == 0) { v.detune = 0.0f; v.glide = 0.0f; v.amp_a = 0.40f; }
            v.bend = BEND[k];
            v.bend_len = (f32)song_beat(1.0);
            f64 on = song_beat(BEAT[k]);
            xy_note(music, &v, on, end - on, ji(PART[k], 1), 1.0f);
        }
    }

    for (int bar = 0; bar < SONG_BARS; bar++) {
        f64 t0 = song_bar(bar);
        song_section sec = song_section_at(t0 + 1e-3);

        /* -- the kick: the downbeat of every beat, alone on it -------------- */
        if (sec == SEC_PULSE || sec == SEC_DROP) {
            for (int b = 0; b < 4; b++) {
                f64 tk = t0 + song_beat(b);
                xy_kick(drums, tk, b == 0 ? 1.0f : 0.92f, sec == SEC_DROP ? 0.80f : 0.64f, 0.12f);
                if (nkick < DM_COUNT(kicks)) kicks[nkick++] = tk;
            }
        }

        /* -- the snare, a ring that fizzes, on two and four ----------------- */
        if (sec == SEC_DROP || (sec == SEC_PULSE && bar > SONG_SECTION_BAR[SEC_PULSE])) {
            for (int b = 1; b < 4; b += 2)
                xy_spark(drums, t0 + song_beat(b), 0.9f, 0.20f, 185.0f, 0.35f, 0.075f, seed++);
        }

        /* -- and a roll into the drop: eighths, sixteenths, thirty-seconds,
         * each ring bigger than the last. Around the butterfly they pile up
         * into a halo that the downbeat cuts off. */
        if (sec == SEC_CHAOS && bar == SONG_SECTION_BAR[SEC_DROP] - 1) {
            int n = 0;
            for (f64 b = 0.0; b < 4.0 - 1e-9; n++) {
                f32 u = (f32)(b / 4.0);
                xy_spark(drums, t0 + song_beat(b), dm_lerp(0.45f, 1.0f, u),
                         dm_lerp(0.10f, 0.26f, u * u), 185.0f, 0.35f, 0.06f, seed++);
                b += b < 2.0 ? 0.5 : (b < 3.0 ? 0.25 : 0.125);
            }
        }

        /* -- hats: the same ring, tiny and high, riding whatever is there --
         * On the off-beats only. Sixteenths of it turn every line in the frame
         * to fur. */
        if (sec == SEC_DROP) {
            for (int s = 2; s < 16; s += 4)
                xy_spark(drums, t0 + s * SONG_STEP_SEC, 0.9f, 0.016f, 2400.0f,
                         1.0f, 0.030f, seed++);
        }

        /* -- PULSE: the chord on the off-beat, the kick on the beat --------- */
        /* An octave up, where house chords live, and pushed a little on each
         * axis so the stab has some edge: the star it draws goes slightly
         * square at the tips, which is the same edge seen. */
        if (sec == SEC_PULSE) {
            const chord *ch = PULSE_CHORDS[bar - SONG_SECTION_BAR[SEC_PULSE]];
            static const f32 SZ[3]  = { 0.27f, 0.15f, 0.09f };
            static const f32 DIR[3] = { 1.0f, -1.0f, 1.0f };
            for (int b = 0; b < 4; b++)
                for (int v = 0; v < 3; v++) {
                    xy_voice sv = voice_stab(SZ[v], DIR[v]);
                    sv.drive = 0.15f;
                    xy_note(music, &sv, t0 + song_beat(b + 0.5), song_beat(0.38),
                            ch->hz[v] * 2.0f, 1.0f);
                }
        }

        /* -- DROP: a square bass between the kicks, a knot on top ---------- */
        if (sec == SEC_DROP) {
            int k = bar - SONG_SECTION_BAR[SEC_DROP];
            const chord *ch = DROP_CHORDS[k];

            /* The bass is a square, traced: odd harmonics only, like the
             * square wave it sounds like, and a filter envelope that opens it
             * from a circle into corners and closes it again every note. */
            xy_voice bass = xy_voice_default();
            bass.path = xy_path_table; bass.ud = square;
            bass.size = 0.52f;
            bass.amp_a = 0.003f; bass.amp_d = 0.10f; bass.amp_s = 0.70f; bass.amp_r = 0.035f;
            bass.cutoff = 240.0f; bass.env_amount = 4200.0f; bass.resonance = 0.45f;
            bass.flt_a = 0.002f; bass.flt_d = 0.09f; bass.flt_s = 0.12f; bass.flt_r = 0.05f;
            static const int STEPS[8] = { 2, 3, 6, 7, 10, 11, 14, 15 };
            for (int s = 0; s < 8; s++)
                xy_note(music, &bass, t0 + STEPS[s] * SONG_STEP_SEC, SONG_STEP_SEC * 0.88,
                        ch->hz[0], s & 1 ? 0.85f : 1.0f);

            /* The lead is a trefoil knot turning in space. Seen flat it is
             * three circles, at the first, second and fifth harmonic, so every
             * note is a small chord and the turning moves weight between
             * them. Chord tones only, so it shares the bass's period and the
             * whole figure still closes. */
            static xy_knot knot = { 2, 3, 0.62f, 0.38f, 0.3f, 0.9f };
            xy_voice ld = xy_voice_default();
            ld.path = xy_path_knot; ld.ud = &knot;
            ld.size = 0.34f;
            ld.amp_a = 0.004f; ld.amp_d = 0.16f; ld.amp_s = 0.62f; ld.amp_r = 0.07f;
            ld.cutoff = dm_lerp(600.0f, 3200.0f, (f32)k / 4.0f);
            ld.env_amount = 2200.0f; ld.resonance = 0.30f;
            ld.flt_a = 0.002f; ld.flt_d = 0.12f; ld.flt_s = 0.3f; ld.flt_r = 0.08f;

            /* The last bar goes up an octave and does not come back down:
             * the word takes over from its top note. */
            static const f32 MEL[5][8] = {
                { 660.0f, 0, 528.0f, 440.0f, 0, 528.0f, 0, 660.0f },       /* Am */
                { 704.0f, 0, 528.0f, 440.0f, 0, 528.0f, 0, 440.0f },       /* F  */
                { 792.0f, 0, 660.0f, 528.0f, 0, 660.0f, 0, 792.0f },       /* C  */
                { 660.0f, 0, 495.0f, 412.5f, 0, 495.0f, 660.0f, 825.0f },  /* E  */
                { 880.0f, 0, 660.0f, 528.0f, 0, 660.0f, 880.0f, 1056.0f }, /* Am */
            };
            for (int s = 0; s < 8; s++) {
                f32 hz = MEL[k][s];
                if (hz <= 0.0f) continue;
                xy_note(lead, &ld, t0 + s * 2 * SONG_STEP_SEC, SONG_STEP_SEC * 1.7, hz, 1.0f);
            }
        }

        /* -- CHAOS: the Lorenz bass ----------------------------------------
         * Two bars of a riff, struck on the beat, and then a bar that climbs
         * the scale of E in sixteenths: the build, played by the one
         * instrument in the piece whose figure never closes. */
        if (sec == SEC_CHAOS && bar == SONG_SECTION_BAR[SEC_CHAOS]) {
            static const f32 RIFF[8] = { 220.0f, 220.0f, 264.0f, 220.0f,
                                         176.0f, 176.0f, 198.0f, 176.0f };
            static const f32 CLIMB[16] = {
                165.0f, 165.0f, 165.0f, 165.0f, 198.0f, 198.0f, 220.0f, 220.0f,
                247.5f, 247.5f, 264.0f, 297.0f, 330.0f, 396.0f, 440.0f, 495.0f,
            };
            f32 lz[48];
            for (int s = 0; s < 32; s++) lz[s] = RIFF[s / 4];
            for (int s = 0; s < 16; s++) lz[32 + s] = CLIMB[s];

            /* It gets louder as it goes, and the level falls further between
             * strikes: a build is a crescendo before it is anything else. */
            xy_lorenz_part lp = { lz, 48, SONG_STEP_SEC, 0.86f, 0.85f, 1.15f, 0.45f, SONG_BEAT_SEC };
            xy_lorenz(music, t0, song_bar(3.0) - 0.01, &lp);
        }

        /* -- NAME: the word, at A1 ----------------------------------------- */
        if (sec == SEC_NAME && bar == SONG_SECTION_BAR[SEC_NAME]) {
            xy_voice w = xy_voice_default();
            w.path = xy_path_table; w.ud = word;
            w.size = 1.0f;
            w.amp_a = 0.18f; w.amp_d = 1.0f; w.amp_s = 1.0f; w.amp_r = 0.10f;
            xy_note(music, &w, t0, SONG_LEN_SEC - 1.0 - t0, A1, 0.70f);
        }
    }

    /* -- mix ------------------------------------------------------------------ */

    /* Only the lead is ducked. Everything else already stays out of the
     * kick's way, and ducking it too would only make it shrink for nothing. */
    dm_fx_sidechain(lead, kicks, nkick, 0.55f, 0.004, 0.12);

    dm_audio_mix(out, drums, 0.95f);
    dm_audio_mix(out, music, 0.95f);
    dm_audio_mix(out, lead,  0.95f);

    /* No high-pass on the master, which every other piece has. A filter is a
     * phase shift, and a phase shift on a picture is a warp: a 12 Hz high-pass
     * turns the 55 Hz fundamental of the word by twenty-five degrees against
     * its harmonics and squeezes the end of the word back into its beginning.
     * Every instrument here is built with no average offset instead, so there
     * is nothing to take out.
     *
     * Linked limiting scales the whole figure at once, so it shrinks on a
     * peak instead of being clipped out of shape. */
    dm_fx_limiter(out, -1.0f, 0.004, 0.12);
    dm_fx_fade(out, SONG_LEN_SEC - 0.02, SONG_LEN_SEC, 1.0f, 0.0f);

    xy_table_free(square);
    xy_table_free(word);
    dm_audio_free(drums);
    dm_audio_free(music);
    dm_audio_free(lead);
    return out;
}
