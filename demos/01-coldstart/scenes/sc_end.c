/* sc_end.c -- END (bar 56).
 *
 * The sign-off, in the form the scene has used since the disk-magazine era:
 * a logo, and a sine scroller carrying the greetings underneath it.
 *
 * The scroller is not decoration. It is the one place a demo speaks in its own
 * voice, and it is the reason the format survived -- everything else on screen
 * is showing off, but the text is talking to you.
 */
#include "../demo.h"

#include <string.h>

/* Length matters here. The scroller runs from the top of the climax to the
 * fade, and its speed is derived from the length so that the last character
 * leaves the frame exactly as the picture goes. Written any longer, the end
 * would be cut off; written any shorter, it would finish early and sit there. */
static const char *SCROLL_TEXT =
    "COLD START BY KORMOS   ...   "
    "EVERY PIXEL AND EVERY SAMPLE COMPUTED FROM NOTHING BUT C   ...   "
    "NO TEXTURES  NO SAMPLES  NO ENGINE  NO STOCK ANYTHING   ...   "
    "GREETINGS TO EVERYONE STILL WRITING SOFTWARE RENDERERS   ...   "
    "FROM KERNEL   ";

#define SCROLL_T0 96.0    /* top of the climax */
#define SCROLL_T1 119.2   /* just before the fade completes */

void yk_scroller(dm_fb *fb, const yk_ctx *c)
{
    if (c->t < SCROLL_T0 || c->t > SCROLL_T1) return;

    f32 W = (f32)c->w, H = (f32)c->h;

    dm_text sc = dm_text_default();
    sc.size      = H * 0.042f;
    sc.weight    = sc.size * 0.060f;
    sc.tracking  = 0.06f;
    sc.color     = v3_scl(dm_rgb8(255, 216, 150), 2.3f);
    sc.glow      = sc.size * 0.16f;
    sc.glow_gain = 0.36f;

    f32 adv = dm_text_advance(&sc);
    int len = (int)strlen(SCROLL_TEXT);

    /* Travel far enough for the last glyph to clear the left edge, spread over
     * exactly the time available. */
    f32 speed = (W + (f32)len * adv) / (f32)(SCROLL_T1 - SCROLL_T0);
    f32 x0    = W - (f32)(c->t - SCROLL_T0) * speed;

    f32 band_y = H * 0.845f;

    /* Over the lattice the background is bright enough to swallow additive
     * text, so darken a soft band under it first. */
    int y0 = (int)(band_y - H * 0.115f), y1 = (int)(band_y + H * 0.095f);
    if (y0 < 0) y0 = 0;
    if (y1 > fb->h) y1 = fb->h;
    for (int y = y0; y < y1; y++) {
        f32 d = fabsf(((f32)y - band_y) / (H * 0.105f));
        f32 k = 1.0f - 0.62f * (1.0f - dm_smoothstep(0.55f, 1.0f, d));
        for (int x = 0; x < fb->w; x++)
            dm_fb_set(fb, x, y, v3_scl(dm_fb_get(fb, x, y), k));
    }

    int first = (int)floorf((-x0 - adv) / adv);
    if (first < 0) first = 0;

    for (int i = first; i < len; i++) {
        f32 x = x0 + (f32)i * adv;
        if (x > W + adv) break;
        if (x < -adv) continue;

        f32 ph = x * 0.0042f + (f32)c->t * 0.9f;
        f32 y  = band_y + sinf(ph) * H * 0.042f;
        dm_char_draw(fb, (unsigned char)SCROLL_TEXT[i], V2(x, y), cosf(ph) * 0.20f, &sc);
    }
}

static v3 end_px(v2 uv, const yk_ctx *c, u32 seed)
{
    (void)seed;
    f32 t  = (f32)c->t;
    f32 r2 = v2_dot(uv, uv);

    /* Back to where the film started: the same nebula, the same darkness. */
    f32 n = dm_fbm3(V3(uv.x * 0.85f, uv.y * 0.85f, t * 0.04f), 5, 2.05f, 0.52f, 3u);
    f32 m = dm_sat(n * 0.5f + 0.5f);
    v3 col = v3_scl(dm_pal_house(0.52f + n * 0.13f), powf(m, 3.0f) * 0.10f);
    col = v3_add(col, v3_scl(dm_rgb8(16, 24, 54), 0.030f));

    /* And the spark, still burning. */
    col = v3_add(col, v3_scl(V3(0.75f, 0.86f, 1.0f), expf(-r2 * 40.0f) * 0.45f));
    col = v3_add(col, v3_scl(dm_rgb8(110, 170, 255), expf(-r2 * 3.5f) * 0.16f));

    return v3_scl(col, yk_vignette(uv, 0.52f));
}

static void end_stars(dm_fb *fb, const yk_ctx *c)
{
    const int N = 900;
    f32 t = (f32)c->t;

    for (int i = 0; i < N; i++) {
        u32 h  = dm_hash_u32((u32)i * 2654435761u + 91u);
        f32 ax = dm_u32_to_f32(h) * 2.0f - 1.0f;  h = dm_hash_u32(h);
        f32 ay = dm_u32_to_f32(h) * 2.0f - 1.0f;  h = dm_hash_u32(h);
        f32 az = dm_u32_to_f32(h);

        f32 z = 0.7f + 3.0f * dm_fract(az + t * 0.006f);
        v2 uv = V2(ax * 2.8f / z, ay * 2.0f / z);
        if (fabsf(uv.x) > c->aspect * 1.1f || fabsf(uv.y) > 1.1f) continue;

        f32 tw = 0.5f + 0.5f * sinf(t * 1.3f + az * 80.0f);
        yk_blob(fb, yk_to_screen(c, uv), 0.7f + 1.0f / z,
                v3_scl(dm_rgb8(170, 200, 255), tw * 0.22f / (z * z)));
    }
}

void scene_end(dm_fb *fb, const yk_ctx *c)
{
    yk_shade(fb, c, end_px, yk_aa);
    end_stars(fb, c);

    f32 W = (f32)c->w, H = (f32)c->h;
    f32 st = (f32)c->sec_t;

    /* --- logo ------------------------------------------------------------ */
    f32 a = dm_smootherstep(0.2f, 1.6f, st);
    dm_text lg = dm_text_default();
    lg.size      = H * 0.115f;
    lg.weight    = lg.size * 0.050f;
    lg.tracking  = 0.34f;
    lg.color     = v3_scl(dm_rgb8(235, 225, 255), a * (1.5f + 0.9f * c->bar_hit));
    lg.glow      = lg.size * 0.13f;
    lg.glow_gain = 0.38f * a;
    dm_text_draw_centered(fb, V2(W * 0.5f, H * 0.40f), "KORMOS", &lg);

    dm_text sub = dm_text_default();
    sub.size      = H * 0.030f;
    sub.weight    = sub.size * 0.075f;
    sub.tracking  = 0.75f;
    sub.color     = v3_scl(dm_rgb8(120, 170, 235), a * 1.2f);
    sub.glow      = sub.size * 0.22f;
    sub.glow_gain = 0.25f * a;
    dm_text_draw_centered(fb, V2(W * 0.5f, H * 0.495f), "COLD START", &sub);

    f32 b = dm_smootherstep(1.6f, 3.0f, st);
    dm_text cr = dm_text_default();
    cr.size      = H * 0.020f;
    cr.weight    = cr.size * 0.085f;
    cr.tracking  = 0.70f;
    cr.color     = v3_scl(dm_rgb8(95, 125, 180), b * 1.0f);
    dm_text_draw_centered(fb, V2(W * 0.5f, H * 0.565f), "CODE AND MUSIC BY KORMOS", &cr);
    dm_text_draw_centered(fb, V2(W * 0.5f, H * 0.610f), "FROM KERNEL", &cr);

    /* The scroller is not drawn here: it starts back at the climax and is laid
     * over whichever scene is running, by demo_frame. */
}
