/* sc_expose.c -- the soundtrack, drawn from the samples it is playing.
 *
 * The claim the whole channel rests on is that the music is computed too, and
 * that half of it is invisible in a video. So for three bars the arrangement
 * drops to a single tone and the screen shows the actual contents of the audio
 * buffer: one column of pixels per 1.7 ms, spanning exactly one bar, with the
 * playhead where the sound you are hearing is. Nothing is stylised. What is on
 * screen is what is in the WAV.
 */
#include "../demo.h"

#include <string.h>

/* ---- background ---------------------------------------------------------- */

/* Cheap on purpose. The scope is the subject; a raymarched backdrop would
 * cost three bars of render time to be looked straight past. */
static v3 quiet_px(v2 uv, const rt_ctx *c, u32 seed)
{
    (void)seed;
    v3 col = v3_scl(dm_rgb8(10, 15, 34), 0.030f);

    f32 n = dm_fbm3(V3(uv.x * 1.1f, uv.y * 0.55f, (f32)c->t * 0.05f), 4, 2.0f, 0.5f, 41u);
    f32 m = dm_sat(n * 0.5f + 0.5f);
    col = v3_add(col, v3_scl(dm_pal_house(0.57f), powf(m, 4.0f) * 0.10f));

    /* A soft horizon behind the trace so the waveform has something to sit on
     * rather than floating in a black rectangle. */
    col = v3_add(col, v3_scl(dm_rgb8(70, 120, 210),
                             0.055f * expf(-dm_sq(uv.y * 3.2f))));

    return v3_scl(col, rt_vignette(uv, 0.40f));
}

/* ---- the scope ----------------------------------------------------------- */

static void draw_scope(dm_fb *fb, const rt_ctx *c)
{
    const dm_audio *a = rt_audio();
    if (!a) return;

    f32 W = (f32)fb->w, H = (f32)fb->h;
    f32 cy = H * 0.44f;
    f32 amp = H * 0.115f;

    /* One bar across the frame, so the playhead crosses the screen once per
     * bar and lands back at the left edge on the downbeat. */
    f64 bar_i  = floor(c->bar);
    f64 t_left = bar_i * SONG_BAR_SEC;
    f64 phase  = c->bar - bar_i;

    f32 head_x = (f32)phase * W;

    for (int x = 0; x < fb->w; x++) {
        f32 fx = (f32)x;
        if (fx > head_x) break;                     /* not played yet */

        f64 t0 = t_left + (f64)x       / (f64)fb->w * SONG_BAR_SEC;
        f64 t1 = t_left + (f64)(x + 1) / (f64)fb->w * SONG_BAR_SEC;

        int i0 = (int)(t0 * DM_SR), i1 = (int)(t1 * DM_SR);
        if (i0 < 0) i0 = 0;
        if (i1 > a->n) i1 = a->n;
        if (i1 <= i0) continue;

        /* Minimum and maximum over the samples this column covers -- the same
         * thing every audio editor draws, and the only honest way to show
         * eighty samples in one pixel of width. */
        f32 lo = 1e9f, hi = -1e9f;
        for (int i = i0; i < i1; i++) {
            f32 s = 0.5f * (a->l[i] + a->r[i]);
            if (s < lo) lo = s;
            if (s > hi) hi = s;
        }
        if (lo > hi) continue;

        /* Fade with distance behind the head: the trace is a memory of what
         * has been heard, and a uniform bar of it reads as a static graphic. */
        f32 age = (head_x - fx) / W;
        f32 k   = 0.30f + 0.70f * expf(-age * 2.6f);

        int y0 = (int)(cy - hi * amp), y1 = (int)(cy - lo * amp);
        if (y0 > y1) { int s = y0; y0 = y1; y1 = s; }
        if (y1 - y0 < 1) y1 = y0 + 1;
        if (y0 < 0) y0 = 0;
        if (y1 > fb->h - 1) y1 = fb->h - 1;

        v3 col = v3_scl(dm_rgb8(150, 210, 255), 0.42f * k);
        for (int y = y0; y <= y1; y++) dm_fb_add(fb, x, y, col);

        /* The extremes get a brighter cap, which is what makes a dense trace
         * read as a shape instead of a solid block. */
        v3 cap = v3_scl(dm_rgb8(220, 240, 255), 0.55f * k);
        dm_fb_add(fb, x, y0, cap);
        dm_fb_add(fb, x, y1, cap);
    }

    /* The playhead. */
    int hx = (int)head_x;
    for (int dx = -2; dx <= 2; dx++) {
        int x = hx + dx;
        if (x < 0 || x >= fb->w) continue;
        f32 g = expf(-(f32)(dx * dx) * 0.55f);
        for (int y = (int)(cy - amp * 1.5f); y <= (int)(cy + amp * 1.5f); y++) {
            if (y < 0 || y >= fb->h) continue;
            f32 d = fabsf(((f32)y - cy) / (amp * 1.5f));
            dm_fb_add(fb, x, y, v3_scl(dm_rgb8(255, 226, 170),
                                       g * 0.85f * (1.0f - d * d)));
        }
    }

    /* The zero line, faint, so the amplitude has a reference. */
    for (int x = 0; x < fb->w; x++)
        dm_fb_add(fb, x, (int)cy, v3_scl(dm_rgb8(70, 110, 180), 0.10f));
}

/* ---- scenes -------------------------------------------------------------- */

void scene_expose(dm_fb *fb, const rt_ctx *c)
{
    rt_shade(fb, c, quiet_px, 1);
    draw_scope(fb, c);

    rt_caption(fb, c, "THE MUSIC TOO", song_bar(11.3), song_bar(13.9));

    /* The count is the payload of this section: a number nobody can fake and
     * everybody can divide. Thirty seconds at 48 kHz. */
    if (c->t > song_bar(12.2) && c->t < song_bar(13.9)) {
        f32 a = dm_sat((f32)((c->t - song_bar(12.2)) / 0.25)) *
                dm_sat((f32)((song_bar(13.9) - c->t) / 0.25));

        dm_text st = dm_text_default();
        st.size      = (f32)fb->w * 0.042f;
        st.tracking  = 0.34f;
        st.weight    = st.size * 0.10f;
        st.color     = v3_scl(dm_rgb8(160, 205, 255), 1.15f * a);
        st.glow      = st.size * 0.20f;
        st.glow_gain = 0.30f * a;
        dm_text_draw_centered(fb, V2((f32)fb->w * 0.5f, (f32)fb->h * 0.60f),
                              "1 440 000 SAMPLES", &st);
    }
}

void scene_sign(dm_fb *fb, const rt_ctx *c)
{
    rt_shade(fb, c, quiet_px, 1);

    f32 W = (f32)fb->w, H = (f32)fb->h;
    f64 t0 = song_bar(SONG_SECTION_BAR[SEC_SIGN]);

    /* Three lines, landing one beat apart. Read in sequence they are the whole
     * argument of the channel, and they fit on a phone at arm's length. */
    static const char *LINE[3] = { "EVERY PIXEL", "EVERY SAMPLE", "COMPUTED" };

    for (int i = 0; i < 3; i++) {
        f64 on = t0 + song_beat(0.5 + 0.5 * i);
        if (c->t < on) continue;
        f32 a = dm_sat((f32)((c->t - on) / 0.13));
        f32 e = dm_ease_out_back(dm_sat((f32)((c->t - on) / 0.32)));

        dm_text st = dm_text_default();
        st.size      = W * (0.070f + 0.028f * e);
        st.tracking  = 0.16f;
        st.weight    = st.size * 0.082f;
        st.color     = v3_scl(dm_rgb8(255, 248, 238), 1.45f * a);
        st.glow      = st.size * 0.14f;
        st.glow_gain = 0.38f * a;

        f32 want = W * 0.86f, have = dm_text_width(LINE[i], &st);
        if (have > want) { st.size *= want / have; st.weight = st.size * 0.082f; }

        dm_text_draw_centered(fb, V2(W * 0.5f, H * (0.40f + 0.085f * (f32)i)),
                              LINE[i], &st);
    }

    /* The wordmark last, and small. The lines above are the argument; this is
     * only the signature on it. */
    f64 on = t0 + song_beat(2.6);
    if (c->t >= on) {
        f32 a = dm_sat((f32)((c->t - on) / 0.25));
        dm_text st = dm_text_default();
        st.size      = W * 0.048f;
        st.tracking  = 0.62f;
        st.weight    = st.size * 0.095f;
        st.color     = v3_scl(dm_rgb8(150, 195, 250), 1.10f * a);
        st.glow      = st.size * 0.22f;
        st.glow_gain = 0.28f * a;
        dm_text_draw_centered(fb, V2(W * 0.5f, H * 0.635f), "KORMOS", &st);
    }
}
