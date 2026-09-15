#include "demo.h"
#include "scenes/scenes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- the shot list ----------------------------------------------------------
 *
 * Cuts are placed in bars, so a cut and a change in the music are the same
 * event by construction rather than by alignment.
 */
/* Each shot's exposure is what a camera would need, not what an eye would
 * see, and its grade is the white balance the camera was set to: night cool,
 * the way film has always shot it; under water, part of the way back from
 * blue. */
const en_shot_def EN_SHOTS[] = {
    {  0.0, shot_winter_wide, "WINTER-WIDE", 15.8f, { 0.55f, 0.80f, 1.45f } },
    {  4.0, shot_winter_ship, "WINTER-SHIP", 15.8f, { 0.55f, 0.80f, 1.45f } },
    {  8.0, shot_pressure_night, "PRESS-NIGHT", 7.6f, { 0.80f, 0.92f, 1.20f } },
    { 11.0, shot_pressure_masts, "PRESS-MASTS", -2.6f, { 1.00f, 1.00f, 1.00f } },
    { 14.0, shot_sinking_far, "SINK-FAR",    -2.2f, { 1.00f, 0.98f, 0.96f } },
    /* Under water, balanced part of the way back from blue, the way a
     * camera would be. */
    { 17.0, shot_sinking_under, "SINK-UNDER", 2.5f, { 2.20f, 1.35f, 0.85f } },
    { 19.0, shot_deep_fall,   "DEEP-FALL",    2.5f, { 2.20f, 1.35f, 0.85f } },
    { 22.0, shot_deep_dark,   "DEEP-DARK",    0.0f, { 1.00f, 1.00f, 1.00f } },
    /* The vehicle's camera, white-balanced the way any underwater camera is:
     * red lifted back toward what a few metres of water took out of it,
     * though not all the way, because the water is part of the picture. */
    { 25.0, shot_found_wheel, "FOUND-WHEEL",  0.4f, { 11.0f, 1.55f, 1.00f } },
    { 27.5, shot_found_stern, "FOUND-STERN", -1.8f, { 11.0f, 1.55f, 1.00f } },
};
const int EN_SHOT_COUNT = DM_COUNT(EN_SHOTS);

f64 en_shot_start(int i) { return song_bar(EN_SHOTS[i].bar); }
f64 en_shot_end(int i)   { return i + 1 < EN_SHOT_COUNT ? song_bar(EN_SHOTS[i + 1].bar) : SONG_LEN_SEC; }

/* ---- context ------------------------------------------------------------- */

en_ctx en_ctx_make(f64 t, int frame, int w, int h, int spp)
{
    en_ctx c;
    c.t      = t;
    c.frame  = frame;
    c.w      = w;
    c.h      = h;
    c.aspect = (f32)w / (f32)h;
    c.spp    = spp < 1 ? 1 : (spp > EN_MAX_SLICES ? EN_MAX_SLICES : spp);

    c.bar  = t / SONG_BAR_SEC;
    c.beat = t / SONG_BEAT_SEC;

    c.sec       = song_section_at(t);
    c.sec_t     = t - song_section_start(c.sec);
    c.sec_phase = song_section_phase(t);

    c.shot = 0;
    for (int i = EN_SHOT_COUNT - 1; i >= 0; i--)
        if (t >= en_shot_start(i)) { c.shot = i; break; }
    f64 a = en_shot_start(c.shot), b = en_shot_end(c.shot);
    c.shot_t     = t - a;
    c.shot_phase = b > a ? dm_clamp((f32)((t - a) / (b - a)), 0.0f, 1.0f) : 0.0f;
    return c;
}

/* ---- captions ---------------------------------------------------------------- */

static f32 fade(const en_ctx *c, f64 t0, f64 t1, f64 len)
{
    if (c->t < t0 || c->t > t1) return 0.0f;
    f32 a = dm_sat((f32)((c->t - t0) / len)) * dm_sat((f32)((t1 - c->t) / len));
    return dm_smoothstep(0.0f, 1.0f, a);
}

/* Light type over snow does not read, so every caption sits on a scrim: the
 * picture darkened under it, by at most half, falling off to nothing well
 * outside the letters. */
static void scrim(dm_fb *fb, f32 x0, f32 y0, f32 x1, f32 y1, f32 pad, f32 a)
{
    int ix0 = DM_MAX(0, (int)(x0 - pad)), ix1 = DM_MIN(fb->w - 1, (int)(x1 + pad));
    int iy0 = DM_MAX(0, (int)(y0 - pad)), iy1 = DM_MIN(fb->h - 1, (int)(y1 + pad));
    for (int y = iy0; y <= iy1; y++) {
        f32 dy = DM_MAX(DM_MAX(y0 - (f32)y, (f32)y - y1), 0.0f) / pad;
        for (int x = ix0; x <= ix1; x++) {
            f32 dx = DM_MAX(DM_MAX(x0 - (f32)x, (f32)x - x1), 0.0f) / pad;
            f32 m = 1.0f - 0.55f * a * expf(-3.5f * (dx * dx + dy * dy));
            f32 *q = fb->px + ((size_t)y * fb->w + x) * 3;
            q[0] *= m; q[1] *= m; q[2] *= m;
        }
    }
}

void en_slate(dm_fb *fb, en_rect pic, const en_ctx *c, const char *top,
              const char *bottom, f64 t0, f64 t1)
{
    f32 a = fade(c, t0, t1, 0.8);
    if (a <= 0.001f) return;

    f32 W = pic.w, H = pic.h;
    dm_text st = dm_text_default();
    st.additive  = 1;

    st.size      = H * 0.030f;
    st.tracking  = 0.55f;
    st.weight    = st.size * 0.060f;
    st.color     = v3_scl(dm_rgb8(244, 238, 226), 0.95f * a);
    st.glow      = st.size * 0.30f;
    st.glow_gain = 0.10f * a;
    f32 x = pic.x + W * 0.065f, y = pic.y + H * 0.845f;
    {
        f32 w = dm_text_width(top, &st);
        f32 yb = y + (bottom && bottom[0] ? H * 0.048f : 0.0f);
        scrim(fb, x, y - st.size, x + w, yb, H * 0.12f, a);
    }
    dm_text_draw(fb, V2(x, y), top, &st);

    if (bottom && bottom[0]) {
        st.size   = H * 0.021f;
        st.weight = st.size * 0.070f;
        st.color  = v3_scl(dm_rgb8(214, 206, 192), 0.80f * a);
        dm_text_draw(fb, V2(x, y + H * 0.048f), bottom, &st);
    }
}

void en_line_text(dm_fb *fb, en_rect pic, const en_ctx *c, const char *s,
                  f32 y, f32 size, f64 t0, f64 t1)
{
    f32 a = fade(c, t0, t1, 0.9);
    if (a <= 0.001f) return;

    f32 W = pic.w, H = pic.h;
    dm_text st = dm_text_default();
    st.additive  = 1;
    st.size      = H * size;
    st.tracking  = 0.42f;
    st.weight    = st.size * 0.062f;

    f32 want = W * 0.80f, have = dm_text_width(s, &st);
    if (have > want) { st.size *= want / have; st.weight = st.size * 0.062f; }

    st.color     = v3_scl(dm_rgb8(244, 238, 226), 1.0f * a);
    st.glow      = st.size * 0.25f;
    st.glow_gain = 0.12f * a;
    {
        f32 w = dm_text_width(s, &st), cx = pic.x + W * 0.5f, cy = pic.y + H * y;
        scrim(fb, cx - w * 0.5f, cy - st.size * 0.6f, cx + w * 0.5f, cy + st.size * 0.6f, H * 0.12f, a);
    }
    dm_text_draw_centered(fb, V2(pic.x + W * 0.5f, pic.y + H * y), s, &st);
}

/* ---- the frame ------------------------------------------------------------- */

extern f32 g_en_ev_offset;

void demo_frame(dm_fb *fb, f32 *depth, const en_ctx *c)
{
    g_en_ev_offset = 0.0f;
    EN_SHOTS[c->shot].fn(fb, depth, c);
}

/* Digits in threes with a comma, the way the depth is written in English. */
static void group_int(char *dst, size_t cap, long v)
{
    char raw[24];
    int n = snprintf(raw, sizeof raw, "%ld", v < 0 ? -v : v);
    size_t o = 0;
    for (int i = 0; i < n && o + 1 < cap; i++) {
        if (i > 0 && (n - i) % 3 == 0 && o + 1 < cap) dst[o++] = ',';
        if (o + 1 < cap) dst[o++] = raw[i];
    }
    dst[o] = 0;
}

/* The depth gauge: small, in the lower right, for the whole of act four. */
static void depth_counter(dm_fb *fb, en_rect pic, const en_ctx *c)
{
    f64 a = song_section_start(SEC_DEEP), b = song_section_end(SEC_DEEP);
    if (c->t < a || c->t > b + 1.5) return;
    f32 alpha = dm_sat((f32)((c->t - a) / 0.8)) * dm_sat((f32)((b + 1.5 - c->t) / 0.8));
    char num[24], line[40];
    group_int(num, sizeof num, (long)(en_deep_depth(c->t) + 0.5f));
    snprintf(line, sizeof line, "%s M", num);

    dm_text st = dm_text_default();
    st.additive  = 1;
    st.size      = pic.h * 0.034f;
    st.tracking  = 0.30f;
    st.weight    = st.size * 0.065f;
    st.color     = v3_scl(dm_rgb8(226, 236, 244), 0.95f * alpha);
    st.glow      = st.size * 0.3f;
    st.glow_gain = 0.10f * alpha;
    f32 w = dm_text_width(line, &st);
    dm_text_draw(fb, V2(pic.x + pic.w * 0.935f - w, pic.y + pic.h * 0.90f), line, &st);
}

void demo_overlay(dm_fb *frame, en_rect pic, const en_ctx *c)
{
    en_slate(frame, pic, c, "THE WEDDELL SEA", "22 JUNE 1915", song_bar(0.6), song_bar(3.5));
    en_line_text(frame, pic, c, "HELD FAST IN THE ICE SINCE JANUARY", 0.88f, 0.030f,
                 song_bar(5.0), song_bar(7.5));
    en_slate(frame, pic, c, "27 OCTOBER 1915", "", song_bar(8.4), song_bar(10.4));
    en_line_text(frame, pic, c, "THE ICE HAD BEGUN TO CRUSH HER", 0.88f, 0.030f,
                 song_bar(11.3), song_bar(13.6));
    en_slate(frame, pic, c, "21 NOVEMBER 1915", "OCEAN CAMP", song_bar(14.15), song_bar(15.0));
    en_line_text(frame, pic, c, "\"SHE'S GOING, BOYS!\"", 0.86f, 0.036f,
                 song_bar(15.2), song_bar(16.6));
    en_line_text(frame, pic, c, "EVERY ONE OF HER 28 MEN SURVIVED", 0.50f, 0.032f,
                 song_bar(20.0), song_bar(21.9));
    en_line_text(frame, pic, c, "NO ONE SAW HER AGAIN FOR 106 YEARS", 0.50f, 0.032f,
                 song_bar(22.6), song_bar(24.6));
    depth_counter(frame, pic, c);
    en_slate(frame, pic, c, "5 MARCH 2022", "3,008 METRES", song_bar(25.5), song_bar(27.3));
    en_line_text(frame, pic, c, "SHE LIES THERE STILL, A PROTECTED HISTORIC SITE", 0.88f, 0.026f,
                 song_bar(28.35), song_bar(29.75));

    /* And out, to black. */
    f32 out = dm_smoothstep((f32)song_bar(29.55), (f32)SONG_LEN_SEC, (f32)c->t);
    if (out > 0.0f) {
        f32 k = 1.0f - out;
        for (int y = (int)pic.y; y < (int)(pic.y + pic.h); y++) {
            f32 *q = frame->px + (size_t)y * frame->w * 3;
            for (int x = 0; x < frame->w * 3; x++) q[x] *= k;
        }
    }
}

f32 demo_exposure(const en_ctx *c)
{
    f32 ev = EN_SHOTS[c->shot].ev + g_en_ev_offset;
    if (getenv("EV")) ev += (f32)atof(getenv("EV"));
    return ev;
}

v3 demo_grade(const en_ctx *c)
{
    return EN_SHOTS[c->shot].grade;
}

dm_post_params demo_post(const en_ctx *c)
{
    dm_post_params p = dm_post_defaults();
    p.bloom_threshold = 1.1f;
    p.bloom_knee      = 0.6f;
    p.bloom_intensity = 0.10f;
    p.bloom_radius    = 0.75f;
    p.barrel          = 0.010f;
    p.chroma          = 0.0010f;
    p.vignette        = 0.36f;
    p.grain           = 0.010f;
    p.frame           = (u32)c->frame;
    if (getenv("NOBLOOM")) p.bloom_intensity = 0.0f;
    return p;
}
