/* sc_intro.c -- the first sixteen seconds.
 *
 * VOID   (bar 0)  darkness, dust, and one point of light deciding to exist.
 * IGNITE (bar 4)  the pulse arrives and the light turns into velocity.
 */
#include "../demo.h"

/* ---- VOID ---------------------------------------------------------------- */

static v3 void_px(v2 uv, const yk_ctx *c, u32 seed)
{
    (void)seed;
    f32 t  = (f32)c->t;
    f32 st = (f32)c->sec_t;
    f32 r2 = v2_dot(uv, uv);

    /* The room is never fully black: a very slow nebula keeps the darkness
     * alive, so the eye has something to hold before anything happens. */
    f32 n = dm_fbm3(V3(uv.x * 0.9f, uv.y * 0.9f, t * 0.045f), 5, 2.05f, 0.52f, 3u);
    f32 m = dm_sat(n * 0.5f + 0.5f);
    v3 col = v3_scl(dm_pal_house(0.50f + n * 0.13f), powf(m, 3.2f) * 0.075f);
    col = v3_add(col, v3_scl(dm_rgb8(18, 30, 62), 0.020f));

    /* The spark. It arrives at 1.5 s, and for the next few seconds it is the
     * only thing in the frame that is brighter than its own background. */
    f32 born = dm_smootherstep(1.4f, 4.2f, st);
    f32 puls = 1.0f + 0.18f * sinf(t * 2.1f);
    col = v3_add(col, v3_scl(V3(0.80f, 0.90f, 1.00f), born * puls * expf(-r2 * 260.0f) * 30.0f));
    col = v3_add(col, v3_scl(dm_rgb8(110, 170, 255), born * puls * expf(-r2 * 14.0f) * 0.55f));

    /* Faint rays leaking out of it, angular noise so they are not a starburst. */
    f32 ang  = atan2f(uv.y, uv.x);
    f32 rays = dm_fbm3(V3(cosf(ang) * 2.4f, sinf(ang) * 2.4f, t * 0.20f), 3, 2.0f, 0.5f, 9u);
    col = v3_add(col, v3_scl(dm_rgb8(90, 150, 255),
                             born * dm_sat(rays) * expf(-r2 * 2.4f) * 0.22f));

    return v3_scl(col, yk_vignette(uv, 0.55f));
}

static void void_dust(dm_fb *fb, const yk_ctx *c)
{
    const int N = 2200;
    f32 t = (f32)c->t;
    f32 reveal = dm_smoothstep(0.3f, 3.0f, (f32)c->sec_t);

    for (int i = 0; i < N; i++) {
        u32 h  = dm_hash_u32((u32)i * 2654435761u + 11u);
        f32 ax = dm_u32_to_f32(h) * 2.0f - 1.0f;
        h = dm_hash_u32(h);
        f32 ay = dm_u32_to_f32(h) * 2.0f - 1.0f;
        h = dm_hash_u32(h);
        f32 az = dm_u32_to_f32(h);
        h = dm_hash_u32(h);
        f32 rate = 0.4f + 1.6f * dm_u32_to_f32(h);

        /* Motes rise and wrap. The depth drift is slower than the vertical
         * drift, which reads as a volume rather than a flat sheet. */
        f32 z = 0.55f + 3.4f * dm_fract(az + t * 0.011f * rate);
        f32 y = ay * 1.9f + dm_fract(t * 0.012f * rate) * 0.4f;
        y = dm_mod(y + 2.2f, 4.4f) - 2.2f;
        f32 x = ax * 2.6f + sinf(t * 0.18f * rate + az * 40.0f) * 0.06f;

        v2 uv = V2(x / z, y / z);
        if (fabsf(uv.x) > c->aspect * 1.15f || fabsf(uv.y) > 1.15f) continue;

        f32 tw   = 0.55f + 0.45f * sinf(t * (1.1f + rate) + az * 90.0f);
        f32 gain = reveal * tw / (z * z) * 0.30f;

        yk_blob(fb, yk_to_screen(c, uv), 0.8f + 1.4f / z,
                v3_scl(dm_rgb8(150, 190, 255), gain));
    }
}

void scene_void(dm_fb *fb, const yk_ctx *c)
{
    yk_shade(fb, c, void_px, yk_aa);
    void_dust(fb, c);

    /* The group signature, drifting up out of the dark. */
    f32 a = dm_smootherstep(3.6f, 6.0f, (f32)c->sec_t);
    if (a > 0.001f) {
        dm_text st = dm_text_default();
        st.size      = (f32)c->h * 0.072f;
        st.weight    = st.size * 0.045f;
        st.tracking  = 0.55f - 0.18f * a;      /* letters settle inwards */
        st.color     = v3_scl(dm_rgb8(190, 215, 255), 1.5f * a);
        st.glow      = st.size * 0.14f;
        st.glow_gain = 0.30f * a;
        dm_text_draw_centered(fb, V2((f32)c->w * 0.5f,
                                     (f32)c->h * (0.50f - 0.010f * (1.0f - a))),
                              "KORMOS", &st);

        dm_text sub = dm_text_default();
        sub.size      = (f32)c->h * 0.024f;
        sub.weight    = sub.size * 0.075f;
        sub.tracking  = 0.85f;
        sub.color     = v3_scl(dm_rgb8(110, 150, 210), 0.9f * a);
        sub.glow      = sub.size * 0.2f;
        sub.glow_gain = 0.2f * a;
        dm_text_draw_centered(fb, V2((f32)c->w * 0.5f, (f32)c->h * 0.575f),
                              "PRESENTS", &sub);
    }
}

/* ---- IGNITE -------------------------------------------------------------- */

static v3 ignite_px(v2 uv, const yk_ctx *c, u32 seed)
{
    (void)seed;
    f32 r2 = v2_dot(uv, uv);
    f32 p  = (f32)c->sec_phase;

    v3 col = v3_scl(dm_rgb8(14, 22, 46), 0.030f);

    /* The vanishing point glows harder as the field accelerates, and pumps
     * once per kick. */
    f32 core = expf(-r2 * (28.0f - 18.0f * p));
    col = v3_add(col, v3_scl(dm_rgb8(120, 180, 255),
                             core * (0.35f + 1.6f * p) * (1.0f + 1.2f * c->beat_hit)));

    /* Radial streaking haze, stretched along the direction of travel. */
    f32 ang = atan2f(uv.y, uv.x);
    f32 n   = dm_fbm3(V3(cosf(ang) * 3.0f, sinf(ang) * 3.0f, (f32)c->t * 1.3f), 3, 2.0f, 0.55f, 21u);
    col = v3_add(col, v3_scl(dm_rgb8(80, 130, 220),
                             dm_sat(n) * expf(-r2 * 1.6f) * 0.14f * (0.3f + p)));

    return v3_scl(col, yk_vignette(uv, 0.42f));
}

#define IGNITE_STARS 1500
#define IGNITE_RANGE 26.0f

static void ignite_field(dm_fb *fb, const yk_ctx *c)
{
    f32 t = (f32)c->t;
    f32 p = (f32)c->sec_phase;

    /* Acceleration is what sells this shot: the field starts as a drift and
     * ends as a jump to lightspeed, on the same curve the music builds on. */
    f32 speed = 3.0f + 46.0f * dm_ease_in_quint(p);
    f32 trail = (0.9f + 5.5f * p) / 60.0f;   /* seconds of motion to smear */

    for (int i = 0; i < IGNITE_STARS; i++) {
        u32 h   = dm_hash_u32((u32)i * 747796405u + 2891336453u);
        f32 ang = dm_u32_to_f32(h) * DM_TAU;
        h = dm_hash_u32(h);
        f32 rad = sqrtf(dm_u32_to_f32(h)) * 3.1f;   /* sqrt keeps it uniform in area */
        h = dm_hash_u32(h);
        f32 z0  = dm_u32_to_f32(h) * IGNITE_RANGE;
        h = dm_hash_u32(h);
        f32 hue = dm_u32_to_f32(h);

        f32 x = cosf(ang) * rad, y = sinf(ang) * rad;

        f32 z  = dm_mod(z0 - t * speed, IGNITE_RANGE) + 0.30f;
        f32 zp = z + speed * trail;

        v2 a = V2(x / zp, y / zp);
        v2 b = V2(x / z,  y / z);
        if (fabsf(b.x) > c->aspect * 1.4f || fabsf(b.y) > 1.4f) continue;

        /* Fade in as they emerge from the far plane so nothing pops. */
        f32 fade = dm_smoothstep(IGNITE_RANGE, IGNITE_RANGE * 0.72f, z);
        f32 gain = fade * 0.55f / (z * z) * (0.6f + 0.8f * c->beat_hit * 0.5f);
        if (gain < 0.0015f) continue;

        v3 col = v3_lerp(dm_rgb8(180, 205, 255), dm_rgb8(255, 190, 140), hue * hue);
        yk_streak(fb, yk_to_screen(c, a), yk_to_screen(c, b),
                  0.55f + 1.1f / z, v3_scl(col, gain));
    }
}

void scene_ignite(dm_fb *fb, const yk_ctx *c)
{
    yk_shade(fb, c, ignite_px, yk_aa);
    ignite_field(fb, c);

    /* The title lands on bar 6 -- two bars after the kick, on a downbeat. */
    f64 land = song_bar(6.0);
    if (c->t >= land) {
        f32 e = dm_ease_out_back(dm_sat((f32)((c->t - land) / 0.55)));
        f32 a = dm_sat((f32)((c->t - land) / 0.28));
        /* Flash white on impact, then settle. Without the decay the title just
         * stays clipped and the letterforms disappear into the bloom. */
        f32 flash = expf(-(f32)(c->t - land) * 5.5f);

        dm_text st = dm_text_default();
        /* Sized for a ten-character title: at the old setting COLD START was
         * 2020 px wide on a 1920 px frame and lost a letter off each end. */
        st.size      = (f32)c->h * (0.038f + 0.072f * e);
        st.weight    = st.size * 0.058f;
        st.tracking  = 0.22f;
        st.color     = v3_scl(dm_rgb8(255, 245, 225),
                              a * (1.15f + 4.5f * flash) * (0.9f + 0.35f * c->bar_hit));
        st.glow      = st.size * 0.13f;
        st.glow_gain = 0.42f * a;
        dm_text_draw_centered(fb, V2((f32)c->w * 0.5f, (f32)c->h * 0.5f), "COLD START", &st);
    }
}
