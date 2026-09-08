#include "demo.h"

#include <stdio.h>
#include <string.h>

int lo_aa = 1;

/* Five and a half degrees below the horizon: late nautical twilight. See
 * demo.h for why that particular angle and not another. */
const v3 LO_SUN_DIR = { -0.6166f, -0.0958f, 0.7816f };

/* ---- the clock ------------------------------------------------------------
 *
 * Mission time advances by a fixed number of seconds per beat, and the rate
 * changes only where the picture cuts. That is the whole trick that lets a
 * nine minute flight fit in thirty seconds without any of it being faked: the
 * count runs at one second per beat so the clock ticks with the music, the
 * ascent at two, the climb out of the atmosphere at eight, and the coast to
 * orbit at forty-five. Every event in the flight lands on a beat because it
 * was placed in beats, not because it was nudged there afterwards.
 */
static const f64 CLOCK_RATE[SEC_COUNT]  = {  1.0, 2.0,  8.0,   2.0,  45.0 };
static const f64 CLOCK_START[SEC_COUNT] = { -8.0, 0.0, 24.0, 152.0, 176.0 };

f64 lo_mission_time(f64 t)
{
    song_section s = song_section_at(t);
    f64 a = song_section_start(s);
    return CLOCK_START[s] + (t - a) / SONG_BEAT_SEC * CLOCK_RATE[s];
}

f64 lo_video_time(f64 mission_t)
{
    for (int s = SEC_COUNT - 1; s >= 0; s--) {
        if (mission_t < CLOCK_START[s] && s > 0) continue;
        f64 a = song_section_start((song_section)s);
        return a + (mission_t - CLOCK_START[s]) / CLOCK_RATE[s] * SONG_BEAT_SEC;
    }
    return 0.0;
}

/* ---- context ------------------------------------------------------------- */

lo_ctx lo_ctx_make(f64 t, int frame, int w, int h)
{
    lo_ctx c;
    c.t      = t;
    c.frame  = frame;
    c.w      = w;
    c.h      = h;
    c.aspect = (f32)w / (f32)h;

    c.bar  = t / SONG_BAR_SEC;
    c.beat = t / SONG_BEAT_SEC;

    f64 bf = c.beat - floor(c.beat);
    f64 rf = c.bar  - floor(c.bar);
    c.beat_hit = expf(-(f32)bf * 8.0f);
    c.bar_hit  = expf(-(f32)rf * 3.2f);

    c.sec       = song_section_at(t);
    c.sec_t     = t - song_section_start(c.sec);
    c.sec_phase = song_section_phase(t);

    c.mt = lo_mission_time(t);
    c.fl = lo_flight_at(c.mt);
    return c;
}

/* ---- shading driver ------------------------------------------------------ */

typedef struct {
    dm_fb        *fb;
    const lo_ctx *c;
    lo_pixel_fn   fn;
    int           samples;
} shade_job;

static void shade_row(int y, int thread, void *ud)
{
    (void)thread;
    shade_job    *s = (shade_job *)ud;
    const lo_ctx *c = s->c;

    f32 inv  = 1.0f / (f32)s->samples;
    f32 invw = 2.0f / (f32)c->w;
    f32 invh = 2.0f / (f32)c->h;

    for (int x = 0; x < c->w; x++) {
        v3 acc = V3(0.0f, 0.0f, 0.0f);
        for (int k = 0; k < s->samples; k++) {
            v2 j = s->samples > 1 ? dm_r2((u32)k + (u32)c->frame * 7919u)
                                  : V2(0.5f, 0.5f);
            /* Vertical frame, so x is the short axis and carries the aspect.
             * That keeps the vertical field of view fixed and lets the same
             * camera code serve a portrait and a landscape crop. */
            v2 uv = V2((((f32)x + j.x) * invw - 1.0f) * c->aspect,
                       1.0f - ((f32)y + j.y) * invh);
            acc = v3_add(acc, s->fn(uv, c, dm_hash3i(x, y, k + c->frame * 64, 0x51a7u)));
        }
        dm_fb_set(s->fb, x, y, v3_scl(acc, inv));
    }
}

void lo_shade(dm_fb *fb, const lo_ctx *c, lo_pixel_fn fn, int samples)
{
    shade_job s;
    s.fb      = fb;
    s.c       = c;
    s.fn      = fn;
    s.samples = samples < 1 ? 1 : samples;
    dm_job_for(fb->h, shade_row, &s);
}

/* ---- captions ------------------------------------------------------------ */

void lo_caption(dm_fb *fb, const lo_ctx *c, const char *s, f64 t0, f64 t1)
{
    if (c->t < t0 || c->t > t1) return;

    f32 a = dm_sat((f32)((c->t - t0) / 0.20)) *
            dm_sat((f32)((t1 - c->t) / 0.20));

    f32 W = (f32)fb->w, H = (f32)fb->h;

    dm_text st = dm_text_default();
    st.size     = W * 0.084f;
    st.tracking = 0.22f;
    st.weight   = st.size * 0.085f;

    f32 want = W * 0.84f;
    f32 have = dm_text_width(s, &st);
    if (have > want) {
        st.size   *= want / have;
        st.weight  = st.size * 0.085f;
    }

    st.color     = v3_scl(dm_rgb8(255, 250, 240), 1.55f * a);
    st.glow      = st.size * 0.16f;
    st.glow_gain = 0.42f * a;

    f32 cy = H * 0.735f;

    /* Sink a band under the text. Over a plume this bright, additive type
     * disappears into what it is sitting on. */
    int y0 = (int)(cy - st.size * 1.5f), y1 = (int)(cy + st.size * 1.5f);
    if (y0 < 0) y0 = 0;
    if (y1 > fb->h) y1 = fb->h;
    for (int y = y0; y < y1; y++) {
        f32 d = fabsf(((f32)y - cy) / (st.size * 1.5f));
        f32 k = 1.0f - 0.64f * a * (1.0f - dm_smoothstep(0.45f, 1.0f, d));
        for (int x = 0; x < fb->w; x++)
            dm_fb_set(fb, x, y, v3_scl(dm_fb_get(fb, x, y), k));
    }

    dm_text_draw_centered(fb, V2(W * 0.5f, cy), s, &st);
}

/* ---- the readout ---------------------------------------------------------
 *
 * Three lines in the top left, which is the one part of a phone screen that a
 * short does not cover with something of its own. Every figure is read out of
 * the integration; none of them is typed in. The clock is the only one with
 * any presentation applied to it, and only to put a colon in.
 */

/* Digits in threes, separated by a space. A six figure altitude is unreadable
 * without it and this font has a space. */
static void group_int(char *dst, size_t cap, long v)
{
    char raw[24];
    int n = snprintf(raw, sizeof raw, "%ld", v < 0 ? -v : v);
    size_t o = 0;
    if (v < 0 && o + 1 < cap) dst[o++] = '-';
    for (int i = 0; i < n; i++) {
        if (i > 0 && (n - i) % 3 == 0 && o + 1 < cap) dst[o++] = ' ';
        if (o + 1 < cap) dst[o++] = raw[i];
    }
    dst[o < cap ? o : cap - 1] = 0;
}

void lo_hud(dm_fb *fb, const lo_ctx *c)
{
    /* Out on the closing card: by then the readout has said everything it can
     * and the frame belongs to the text. */
    f32 a = 1.0f;
    if (c->sec == SEC_ORBIT)
        a = 1.0f - dm_smoothstep((f32)song_bar(SONG_SECTION_BAR[SEC_ORBIT]),
                                 (f32)song_bar(SONG_SECTION_BAR[SEC_ORBIT] + 0.5),
                                 (f32)c->t);
    /* And in on the first beat, so frame one is the vehicle, not a caption. */
    a *= dm_sat((f32)(c->t / 0.35));
    if (a <= 0.002f) return;

    f32 W = (f32)fb->w, H = (f32)fb->h;
    const lo_state *f = &c->fl;

    long secs = (long)floor(fabs(c->mt));
    char clock[32], line[3][40], num[24];
    snprintf(clock, sizeof clock, "T%c%02ld:%02ld",
             c->mt < 0.0 ? '-' : '+', secs / 60, secs % 60);

    group_int(num, sizeof num, (long)(f->alt + 0.5f));
    snprintf(line[0], sizeof line[0], "ALT %s M", num);
    group_int(num, sizeof num, (long)(f->vel + 0.5f));
    snprintf(line[1], sizeof line[1], "VEL %s M/S", num);
    snprintf(line[2], sizeof line[2], "MACH %.1f", (double)f->mach);

    /* Above about eighty kilometres there is no Mach number worth printing --
     * there is not enough air to have a speed of sound in. The line becomes
     * the acceleration instead, which is the number that still means
     * something up there. */
    if (f->alt > 80000.0f) {
        f32 gs = f->accel / 9.80665f;
        if (gs > -0.05f && gs < 0.05f) gs = 0.0f;      /* no signed zero */
        snprintf(line[2], sizeof line[2], "ACC %.1f G", (double)gs);
    }

    dm_text st = dm_text_default();
    st.tracking  = 0.16f;
    st.additive  = 1;

    f32 x = W * 0.075f;
    f32 y = H * 0.115f;

    /* The clock is twice the size of the rest of it. It is the only figure a
     * viewer reads without being asked to. */
    st.size      = W * 0.062f;
    st.weight    = st.size * 0.095f;
    st.color     = v3_scl(dm_rgb8(190, 226, 255), 1.30f * a);
    st.glow      = st.size * 0.18f;
    st.glow_gain = 0.32f * a;
    dm_text_draw(fb, V2(x, y), clock, &st);

    st.size      = W * 0.030f;
    st.weight    = st.size * 0.115f;
    st.color     = v3_scl(dm_rgb8(140, 190, 235), 0.95f * a);
    st.glow      = st.size * 0.22f;
    st.glow_gain = 0.22f * a;
    for (int i = 0; i < 3; i++)
        dm_text_draw(fb, V2(x, y + W * (0.050f + 0.043f * (f32)i)), line[i], &st);
}

/* ---- dispatch ------------------------------------------------------------ */

void demo_frame(dm_fb *fb, const lo_ctx *c)
{
    switch (c->sec) {
    case SEC_HOLD:  scene_hold(fb, c);  break;
    case SEC_LIFT:  scene_lift(fb, c);  break;
    case SEC_CLIMB: scene_climb(fb, c); break;
    case SEC_STAGE: scene_stage(fb, c); break;
    default:        scene_orbit(fb, c); break;
    }

    lo_hud(fb, c);

    /* There is deliberately no flash on the cuts, which the two pieces before
     * this one both had. It does not survive a black sky: an eighth of a unit
     * of light added to every pixel is invisible over a fractal and is a grey
     * card over three quarters of this frame, and it takes four frames to
     * decay out of a night shot instead of one. The cuts here do not need the
     * help either -- the camera, the framing and the arrangement all change on
     * the same downbeat, which is what a cut is. */

    /* Out to black on the last beat so the loop point is a cut and not a jump.
     * There is no fade in: a short has to be alive at frame one. */
    if (c->t > SONG_LEN_SEC - 1.2)
        dm_fb_scale(fb, 1.0f - dm_smoothstep((f32)SONG_LEN_SEC - 1.2f,
                                             (f32)SONG_LEN_SEC - 0.05f, (f32)c->t));
}

dm_post_params demo_post(const lo_ctx *c)
{
    dm_post_params p = dm_post_defaults();

    switch (c->sec) {
    case SEC_HOLD:
        /* Night, floodlights, and nothing bright in frame but the vehicle. A
         * low threshold here would bloom the whole white body into mush. */
        p.bloom_threshold = 1.05f;
        p.bloom_intensity = 0.16f;
        p.chroma          = 0.0014f;
        p.vignette        = 0.48f;
        p.grain           = 0.016f;
        break;

    case SEC_LIFT:
        /* The exhaust is four orders of magnitude brighter than anything else
         * that has been on screen, and a lens does something with that. */
        p.bloom_threshold = 1.05f;
        p.bloom_intensity = 0.24f;
        p.bloom_radius    = 0.75f;
        p.chroma          = 0.0032f;
        p.barrel          = 0.018f;
        p.vignette        = 0.36f;
        p.grain           = 0.013f;
        break;

    case SEC_CLIMB:
        p.bloom_threshold = 0.95f;
        p.bloom_intensity = 0.22f;
        p.chroma          = 0.0026f;
        p.barrel          = 0.014f;
        p.vignette        = 0.34f;
        p.grain           = 0.011f;
        break;

    case SEC_STAGE:
        /* Vacuum: no haze, no scatter, hard edges. The lens gets out of the
         * way because up there nothing is between the camera and the subject. */
        p.bloom_threshold = 1.15f;
        p.bloom_intensity = 0.14f;
        p.chroma          = 0.0010f;
        p.vignette        = 0.40f;
        p.grain           = 0.009f;
        break;

    default:
        /* The lit half of a planet fills the bottom of this shot, and a low
         * threshold would bloom it up over the type. */
        p.bloom_threshold = 1.35f;
        p.bloom_intensity = 0.18f;
        p.chroma          = 0.0012f;
        p.vignette        = 0.44f;
        p.grain           = 0.010f;
        break;
    }

    p.bloom_intensity *= 1.0f + 0.35f * c->beat_hit;
    p.frame            = (u32)c->frame;
    return p;
}
