#include "demo.h"

int yk_aa = 1;

/* ---- context ------------------------------------------------------------ */

yk_ctx yk_ctx_make(f64 t, int frame, int w, int h)
{
    yk_ctx c;
    c.t      = t;
    c.frame  = frame;
    c.w      = w;
    c.h      = h;
    c.aspect = (f32)w / (f32)h;

    c.bar  = t / SONG_BAR_SEC;
    c.beat = t / song_beat(1.0);

    /* A percussive envelope on every beat and every bar. Scenes multiply
     * brightness, scale or camera shake by these to lock the picture to the
     * track without any of them having to know what the track is doing. */
    f64 bf = c.beat - floor(c.beat);
    f64 rf = c.bar  - floor(c.bar);
    c.beat_hit = expf(-(f32)bf * 7.0f);
    c.bar_hit  = expf(-(f32)rf * 3.2f);
    c.phrase   = (f32)dm_fract((f32)c.bar * 0.25f);

    c.sec       = song_section_at(t);
    c.sec_t     = t - song_section_start(c.sec);
    c.sec_phase = song_section_phase(t);
    return c;
}

/* ---- shading driver ------------------------------------------------------ */

typedef struct {
    dm_fb        *fb;
    const yk_ctx *c;
    yk_pixel_fn   fn;
    int           samples;
} shade_job;

static void shade_row(int y, int thread, void *ud)
{
    (void)thread;
    shade_job    *s = (shade_job *)ud;
    const yk_ctx *c = s->c;

    f32 inv  = 1.0f / (f32)s->samples;
    f32 invw = 2.0f / (f32)c->w;
    f32 invh = 2.0f / (f32)c->h;

    for (int x = 0; x < c->w; x++) {
        v3 acc = V3(0.0f, 0.0f, 0.0f);
        for (int k = 0; k < s->samples; k++) {
            /* R2 low-discrepancy jitter, offset per frame so the sample grid
             * does not stay locked in place and turn into fixed pattern noise
             * once the frames are played in sequence. */
            v2 j = s->samples > 1 ? dm_r2((u32)k + (u32)c->frame * 7919u)
                                  : V2(0.5f, 0.5f);
            v2 uv = V2((((f32)x + j.x) * invw - 1.0f) * c->aspect,
                       1.0f - ((f32)y + j.y) * invh);
            acc = v3_add(acc, s->fn(uv, c, dm_hash3i(x, y, k + c->frame * 64, 0x51ed27u)));
        }
        dm_fb_set(s->fb, x, y, v3_scl(acc, inv));
    }
}

void yk_shade(dm_fb *fb, const yk_ctx *c, yk_pixel_fn fn, int samples)
{
    shade_job s;
    s.fb      = fb;
    s.c       = c;
    s.fn      = fn;
    s.samples = samples < 1 ? 1 : samples;
    dm_job_for(fb->h, shade_row, &s);
}

/* ---- drawing helpers ----------------------------------------------------- */

v2 yk_to_screen(const yk_ctx *c, v2 uv)
{
    return V2((uv.x / c->aspect * 0.5f + 0.5f) * (f32)c->w,
              (0.5f - uv.y * 0.5f) * (f32)c->h);
}

void yk_blob(dm_fb *fb, v2 p, f32 radius, v3 col)
{
    if (radius < 0.35f) radius = 0.35f;
    f32 reach = radius * 3.0f;
    int x0 = (int)floorf(p.x - reach), x1 = (int)ceilf(p.x + reach);
    int y0 = (int)floorf(p.y - reach), y1 = (int)ceilf(p.y + reach);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > fb->w - 1) x1 = fb->w - 1;
    if (y1 > fb->h - 1) y1 = fb->h - 1;

    f32 inv2r2 = 1.0f / (2.0f * radius * radius);
    for (int y = y0; y <= y1; y++) {
        f32 dy = (f32)y + 0.5f - p.y;
        for (int x = x0; x <= x1; x++) {
            f32 dx = (f32)x + 0.5f - p.x;
            f32 w  = expf(-(dx * dx + dy * dy) * inv2r2);
            if (w > 0.002f) dm_fb_add(fb, x, y, v3_scl(col, w));
        }
    }
}

void yk_streak(dm_fb *fb, v2 a, v2 b, f32 width, v3 col)
{
    if (width < 0.35f) width = 0.35f;
    f32 reach = width * 3.0f;
    int x0 = (int)floorf(DM_MIN(a.x, b.x) - reach), x1 = (int)ceilf(DM_MAX(a.x, b.x) + reach);
    int y0 = (int)floorf(DM_MIN(a.y, b.y) - reach), y1 = (int)ceilf(DM_MAX(a.y, b.y) + reach);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > fb->w - 1) x1 = fb->w - 1;
    if (y1 > fb->h - 1) y1 = fb->h - 1;
    if (x1 < x0 || y1 < y0) return;

    v2  ba = v2_sub(b, a);
    f32 bb = v2_dot(ba, ba);
    f32 inv2w2 = 1.0f / (2.0f * width * width);

    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            v2  p  = V2((f32)x + 0.5f, (f32)y + 0.5f);
            v2  pa = v2_sub(p, a);
            f32 h  = bb > 1e-9f ? dm_sat(v2_dot(pa, ba) / bb) : 0.0f;
            f32 d2 = v2_len2(v2_sub(pa, v2_scl(ba, h)));
            f32 w  = expf(-d2 * inv2w2);
            if (w > 0.002f) dm_fb_add(fb, x, y, v3_scl(col, w));
        }
    }
}

/* ---- dispatch ------------------------------------------------------------ */

void demo_frame(dm_fb *fb, const yk_ctx *c)
{
    switch (c->sec) {
    case SEC_INTRO_PAD:  scene_void(fb, c);     break;
    case SEC_INTRO_KICK: scene_ignite(fb, c);   break;
    case SEC_GROOVE:     scene_tunnel(fb, c);   break;
    case SEC_LEAD_A:     scene_plasma(fb, c);   break;
    case SEC_BREAK:      scene_mirror(fb, c);   break;
    case SEC_BUILD:      scene_fracture(fb, c); break;
    case SEC_DROP:       scene_fractal(fb, c);  break;
    case SEC_CLIMAX:     scene_lattice(fb, c);  break;
    default:             scene_end(fb, c);      break;
    }

    yk_scroller(fb, c);

    /* A one-frame white flash on each section boundary. Cheap, and it hides
     * the seam between two completely unrelated pictures far better than any
     * cross-fade would. */
    f32 cut = expf(-(f32)c->sec_t * 26.0f);
    if (cut > 0.004f) {
        f32 g = cut * 2.6f;
        for (int y = 0; y < fb->h; y++)
            for (int x = 0; x < fb->w; x++)
                dm_fb_add(fb, x, y, V3(g, g * 0.96f, g * 1.05f));
    }

    /* Fade up from black at the very start and out to black at the end. */
    if (c->t < 1.2) dm_fb_scale(fb, dm_smoothstep(0.0f, 1.2f, (f32)c->t));
    if (c->t > SONG_LEN_SEC - 2.5)
        dm_fb_scale(fb, 1.0f - dm_smoothstep((f32)SONG_LEN_SEC - 2.5f,
                                             (f32)SONG_LEN_SEC - 0.2f, (f32)c->t));
}

dm_post_params demo_post(const yk_ctx *c)
{
    dm_post_params p = dm_post_defaults();

    switch (c->sec) {
    case SEC_INTRO_PAD:
    case SEC_BREAK:
        /* Quiet passages: wide soft bloom, heavy vignette, almost no chroma. */
        p.bloom_threshold = 0.65f;
        p.bloom_intensity = 0.13f;
        p.chroma          = 0.0012f;
        p.vignette        = 0.55f;
        p.grain           = 0.016f;
        break;

    case SEC_BUILD:
        /* The build wants the lens to feel like it is about to break. */
        p.bloom_threshold = 0.8f;
        p.bloom_intensity = 0.16f;
        p.chroma          = 0.0060f + 0.010f * (f32)c->sec_phase;
        p.barrel          = 0.02f + 0.09f * (f32)c->sec_phase;
        p.vignette        = 0.30f;
        p.grain           = 0.030f;
        break;

    case SEC_DROP:
    case SEC_CLIMAX:
        p.bloom_threshold = 1.05f;
        p.bloom_intensity = 0.10f;
        p.chroma          = 0.0028f;
        p.vignette        = 0.34f;
        p.grain           = 0.010f;
        break;

    default:
        break;
    }

    /* Every kick pushes the lens a little: bloom swells and the barrel
     * distortion breathes. Subtle per frame, unmistakable over two minutes. */
    p.bloom_intensity *= 1.0f + 0.35f * c->beat_hit;
    p.barrel          += 0.006f * c->beat_hit;
    p.frame            = (u32)c->frame;
    return p;
}
