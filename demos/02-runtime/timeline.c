#include "demo.h"

#include <string.h>

int rt_aa = 1;

static const dm_audio *g_audio = NULL;

void rt_set_audio(const dm_audio *a) { g_audio = a; }
const dm_audio *rt_audio(void)       { return g_audio; }

/* ---- context ------------------------------------------------------------- */

rt_ctx rt_ctx_make(f64 t, int frame, int w, int h)
{
    rt_ctx c;
    c.t      = t;
    c.frame  = frame;
    c.w      = w;
    c.h      = h;
    c.aspect = (f32)w / (f32)h;

    c.bar  = t / SONG_BAR_SEC;
    c.beat = t / song_beat(1.0);

    f64 bf = c.beat - floor(c.beat);
    f64 rf = c.bar  - floor(c.bar);
    c.beat_hit = expf(-(f32)bf * 8.5f);
    c.bar_hit  = expf(-(f32)rf * 3.6f);

    c.sec       = song_section_at(t);
    c.sec_t     = t - song_section_start(c.sec);
    c.sec_phase = song_section_phase(t);
    return c;
}

/* ---- shading driver ------------------------------------------------------ */

typedef struct {
    dm_fb        *fb;
    const rt_ctx *c;
    rt_pixel_fn   fn;
    int           samples;
} shade_job;

static void shade_row(int y, int thread, void *ud)
{
    (void)thread;
    shade_job    *s = (shade_job *)ud;
    const rt_ctx *c = s->c;

    f32 inv  = 1.0f / (f32)s->samples;
    f32 invw = 2.0f / (f32)c->w;
    f32 invh = 2.0f / (f32)c->h;

    for (int x = 0; x < c->w; x++) {
        v3 acc = V3(0.0f, 0.0f, 0.0f);
        for (int k = 0; k < s->samples; k++) {
            v2 j = s->samples > 1 ? dm_r2((u32)k + (u32)c->frame * 7919u)
                                  : V2(0.5f, 0.5f);
            /* Vertical frame, so x is the short axis and gets scaled by the
             * aspect. That keeps the vertical field of view constant and lets
             * the same camera code serve a portrait and a landscape crop. */
            v2 uv = V2((((f32)x + j.x) * invw - 1.0f) * c->aspect,
                       1.0f - ((f32)y + j.y) * invh);
            acc = v3_add(acc, s->fn(uv, c, dm_hash3i(x, y, k + c->frame * 64, 0x9e37u)));
        }
        dm_fb_set(s->fb, x, y, v3_scl(acc, inv));
    }
}

void rt_shade(dm_fb *fb, const rt_ctx *c, rt_pixel_fn fn, int samples)
{
    shade_job s;
    s.fb      = fb;
    s.c       = c;
    s.fn      = fn;
    s.samples = samples < 1 ? 1 : samples;
    dm_job_for(fb->h, shade_row, &s);
}

/* ---- the reveal ---------------------------------------------------------- */

void rt_reveal(dm_fb *fb, f32 progress)
{
    if (progress >= 1.0f) return;

    f32 H = (f32)fb->h;

    /* Eight workers, so eight rows are in flight at once. Each row is given a
     * cost between half and one and a half of the average, which is what makes
     * the front ragged instead of a ruler-straight wipe. */
    const int   workers = 8;
    const f32   band    = (f32)workers;
    f32 front = progress * (H + band * 6.0f) - band * 3.0f;

    for (int y = 0; y < fb->h; y++) {
        /* A per-row offset in the same spirit as the varying cost of a row:
         * deterministic, small, and different for every row. */
        f32 jitter = (dm_hash1f((f32)y * 0.37f, 0x51ed27u) - 0.5f) * band * 2.2f;
        f32 d = front - ((f32)y + jitter);

        if (d >= band) continue;                    /* long since finished */

        if (d <= 0.0f) {
            /* Not computed yet. Not black -- black would read as a wipe over a
             * picture. This is memory nobody has written, so it gets the faint
             * blue-grey the rest of the piece uses for nothing at all. */
            for (int x = 0; x < fb->w; x++)
                dm_fb_set(fb, x, y, v3_scl(dm_rgb8(10, 14, 30), 0.020f));
            continue;
        }

        /* Inside the band: the row exists but is still warm. Fade it up and
         * add a hot edge at the very front, which is the only part of this
         * that is theatre rather than description. */
        f32 k    = d / band;
        f32 edge = expf(-(1.0f - k) * (1.0f - k) * 26.0f);
        v3  hot  = v3_scl(dm_rgb8(150, 205, 255), edge * 0.85f);

        for (int x = 0; x < fb->w; x++) {
            v3 col = v3_scl(dm_fb_get(fb, x, y), k);
            dm_fb_set(fb, x, y, v3_add(col, hot));
        }
    }
}

/* ---- captions ------------------------------------------------------------ */

void rt_caption(dm_fb *fb, const rt_ctx *c, const char *s, f64 t0, f64 t1)
{
    if (c->t < t0 || c->t > t1) return;

    /* 0.22 s in and out. Short enough to feel cut rather than dissolved, long
     * enough that the letters do not pop and alias on the way. */
    f32 a = dm_sat((f32)((c->t - t0) / 0.22)) *
            dm_sat((f32)((t1 - c->t) / 0.22));

    f32 W = (f32)fb->w, H = (f32)fb->h;

    dm_text st = dm_text_default();
    st.size     = W * 0.088f;
    st.tracking = 0.20f;
    st.weight   = st.size * 0.085f;

    /* Shrink to fit rather than run off the edge: the longest caption here is
     * fourteen characters and the shortest is six. */
    f32 want = W * 0.84f;
    f32 have = dm_text_width(s, &st);
    if (have > want) {
        st.size   *= want / have;
        st.weight  = st.size * 0.085f;
    }

    st.color     = v3_scl(dm_rgb8(255, 248, 236), 1.55f * a);
    st.glow      = st.size * 0.16f;
    st.glow_gain = 0.42f * a;

    f32 cy = H * 0.735f;

    /* Sink a band under the text. On a fractal this busy, additive type
     * disappears into the background it is sitting on. */
    int y0 = (int)(cy - st.size * 1.5f), y1 = (int)(cy + st.size * 1.5f);
    if (y0 < 0) y0 = 0;
    if (y1 > fb->h) y1 = fb->h;
    for (int y = y0; y < y1; y++) {
        f32 d = fabsf(((f32)y - cy) / (st.size * 1.5f));
        f32 k = 1.0f - 0.62f * a * (1.0f - dm_smoothstep(0.45f, 1.0f, d));
        for (int x = 0; x < fb->w; x++)
            dm_fb_set(fb, x, y, v3_scl(dm_fb_get(fb, x, y), k));
    }

    dm_text_draw_centered(fb, V2(W * 0.5f, cy), s, &st);
}

/* ---- dispatch ------------------------------------------------------------ */

void demo_frame(dm_fb *fb, const rt_ctx *c)
{
    switch (c->sec) {
    case SEC_ASSEMBLE: scene_assemble(fb, c); break;
    case SEC_LOCK:     scene_lock(fb, c);     break;
    case SEC_DRIVE:    scene_drive(fb, c);    break;
    case SEC_EXPOSE:   scene_expose(fb, c);   break;
    default:           scene_sign(fb, c);     break;
    }

    /* A one-frame flash on the two hard cuts. The assemble/lock boundary is
     * deliberately left alone: there the picture completing is the event, and
     * a flash would step on it. */
    if (c->sec == SEC_DRIVE || c->sec == SEC_EXPOSE) {
        f32 cut = expf(-(f32)c->sec_t * 30.0f);
        if (cut > 0.004f) {
            f32 g = cut * 2.2f;
            for (int y = 0; y < fb->h; y++)
                for (int x = 0; x < fb->w; x++)
                    dm_fb_add(fb, x, y, V3(g, g * 0.97f, g * 1.06f));
        }
    }

    /* Out to black on the last beat so the loop point is a true cut rather
     * than a jump. There is no fade in: a short has to be alive at frame one. */
    if (c->t > SONG_LEN_SEC - 1.1)
        dm_fb_scale(fb, 1.0f - dm_smoothstep((f32)SONG_LEN_SEC - 1.1f,
                                             (f32)SONG_LEN_SEC - 0.05f, (f32)c->t));
}

dm_post_params demo_post(const rt_ctx *c)
{
    dm_post_params p = dm_post_defaults();

    switch (c->sec) {
    case SEC_ASSEMBLE:
        /* While the picture is being computed the lens stays honest: almost no
         * bloom, so what is on screen is what was calculated. */
        p.bloom_threshold = 1.20f;
        p.bloom_intensity = 0.10f;
        p.chroma          = 0.0010f;
        p.vignette        = 0.42f;
        p.grain           = 0.014f;
        break;

    case SEC_LOCK:
    case SEC_DRIVE:
        p.bloom_threshold = 1.00f;
        p.bloom_intensity = 0.16f;
        p.chroma          = 0.0026f;
        p.barrel          = 0.015f;
        p.vignette        = 0.34f;
        p.grain           = 0.010f;
        break;

    case SEC_EXPOSE:
        p.bloom_threshold = 0.80f;
        p.bloom_intensity = 0.20f;
        p.chroma          = 0.0014f;
        p.vignette        = 0.50f;
        p.grain           = 0.012f;
        break;

    default:
        p.bloom_threshold = 0.90f;
        p.bloom_intensity = 0.18f;
        p.vignette        = 0.45f;
        p.grain           = 0.010f;
        break;
    }

    p.bloom_intensity *= 1.0f + 0.40f * c->beat_hit;
    p.barrel          += 0.005f * c->beat_hit;
    p.frame            = (u32)c->frame;
    return p;
}
