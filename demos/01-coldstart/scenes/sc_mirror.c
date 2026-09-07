/* sc_mirror.c -- MIRROR (bar 24).
 *
 * The breakdown. Every demo needs one shot where nothing is thrown at the
 * viewer, or the loud parts stop registering as loud.
 *
 * An analytic plane at y=0 with a noise-perturbed normal, one Fresnel term,
 * and a procedural sky reflected in it. No geometry, no raymarching: a ray
 * either hits the floor (one division) or it does not.
 */
#include "../demo.h"

/* Procedural star field in spherical coordinates. Sampling a 3x3 cell
 * neighbourhood rather than one cell means stars near a cell boundary are not
 * clipped in half as the camera turns. */
static f32 sky_stars(v3 d)
{
    f32 az = atan2f(d.z, d.x);
    f32 el = asinf(dm_clamp(d.y, -1.0f, 1.0f));
    v2  s  = V2(az * 46.0f, el * 46.0f);
    v2  ip = v2_floor(s);
    v2  fp = v2_sub(s, ip);

    f32 acc = 0.0f;
    for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
            u32 h = dm_hash2i((i32)ip.x + i, (i32)ip.y + j, 0x51a3u);
            if (dm_u32_to_f32(h) > 0.16f) continue;      /* most cells are empty */
            h = dm_hash_u32(h);
            f32 cx = dm_u32_to_f32(h);
            h = dm_hash_u32(h);
            f32 cy = dm_u32_to_f32(h);
            h = dm_hash_u32(h);
            f32 mag = dm_u32_to_f32(h);

            v2  o  = V2((f32)i + cx - fp.x, (f32)j + cy - fp.y);
            f32 d2 = v2_dot(o, o);
            acc += expf(-d2 * 170.0f) * (0.15f + mag * mag * 2.4f);
        }
    }
    return acc;
}

static v3 mirror_sky(v3 d, f32 t)
{
    f32 up = dm_sat(d.y * 0.5f + 0.5f);
    v3 col = v3_lerp(v3_scl(dm_rgb8(10, 14, 34), 0.55f),
                     v3_scl(dm_rgb8(16, 22, 62), 0.75f), up);

    /* Nebula. Ridged rather than plain fBm so it has filaments instead of
     * looking like fog. */
    f32 n = dm_ridge3(v3_add(v3_scl(d, 2.1f), V3(0.0f, 0.0f, t * 0.012f)),
                      5, 2.05f, 0.55f, 61u);
    col = v3_add(col, v3_scl(dm_pal_house(0.58f + n * 0.16f),
                             powf(n, 3.0f) * 0.85f * dm_sat(d.y * 1.6f + 0.35f)));

    col = v3_add(col, v3_scl(V3(0.85f, 0.92f, 1.0f), sky_stars(d) * 0.55f));

    /* One low, cold sun sitting just above the horizon -- the anchor the eye
     * returns to, and the thing the floor is really reflecting. */
    v3  sun = v3_norm(V3(0.28f, 0.075f, 1.0f));
    f32 sd  = dm_sat(v3_dot(d, sun));
    col = v3_add(col, v3_scl(dm_rgb8(150, 200, 255), powf(sd, 2200.0f) * 22.0f));
    col = v3_add(col, v3_scl(dm_rgb8(90,  150, 255), powf(sd, 60.0f) * 0.55f));
    col = v3_add(col, v3_scl(dm_rgb8(60,  100, 220), powf(sd, 8.0f) * 0.12f));
    return col;
}

/* Height of the floor ripples. Kept tiny -- this is a polished surface with a
 * breath of movement on it, not water. */
static f32 mirror_ripple(f32 x, f32 z, f32 t)
{
    return dm_fbm3(V3(x * 0.26f, z * 0.26f, t * 0.10f), 4, 2.1f, 0.5f, 91u) * 0.055f
         + sinf(z * 0.9f - t * 0.6f) * 0.008f;
}

static v3 mirror_px(v2 uv, const yk_ctx *c, u32 seed)
{
    (void)seed;
    f32 t = (f32)c->t;

    v3 ro = V3(sinf(t * 0.06f) * 0.8f, 0.92f, t * 0.62f);
    v3 at = V3(sinf(t * 0.045f) * 1.6f, 0.80f, ro.z + 7.0f);
    m3 cam = m3_look_at(ro, at, sinf(t * 0.037f) * 0.045f);
    v3 rd  = m3_mul_v3(cam, v3_norm(V3(uv.x, uv.y, 1.65f)));

    v3 col;

    if (rd.y < -0.0015f) {
        f32 dist = -ro.y / rd.y;
        v3  p    = v3_add(ro, v3_scl(rd, dist));

        /* Finite-difference the ripple field for the normal. The step scales
         * with distance so the ripples band-limit themselves toward the
         * horizon instead of turning into aliasing. */
        f32 e  = 0.06f + dist * 0.010f;
        f32 hx = mirror_ripple(p.x + e, p.z, t) - mirror_ripple(p.x - e, p.z, t);
        f32 hz = mirror_ripple(p.x, p.z + e, t) - mirror_ripple(p.x, p.z - e, t);
        v3  n  = v3_norm(V3(-hx, 2.0f * e, -hz));

        v3  refl = mirror_sky(v3_reflect(rd, n), t);
        f32 fres = 0.035f + 0.965f * powf(1.0f - dm_sat(v3_dot(n, v3_neg(rd))), 5.0f);

        col = v3_scl(refl, fres);
        col = v3_add(col, v3_scl(dm_rgb8(12, 18, 46), 0.05f));

        /* A faint grid, because this is still a demo and the floor should
         * admit it is made of coordinates. */
        f32 gx = fabsf(dm_fract(p.x * 0.5f) - 0.5f);
        f32 gz = fabsf(dm_fract(p.z * 0.5f) - 0.5f);
        f32 grid = powf(1.0f - DM_MIN(gx, gz) * 2.0f, 60.0f);
        col = v3_add(col, v3_scl(dm_rgb8(70, 130, 220),
                                 grid * 0.45f * expf(-dist * 0.045f)));

        col = yk_fog(col, v3_scl(dm_rgb8(14, 20, 52), 0.55f), dist, 0.017f);
    } else {
        col = mirror_sky(rd, t);
    }

    return v3_scl(col, yk_vignette(uv, 0.50f));
}

void scene_mirror(dm_fb *fb, const yk_ctx *c)
{
    yk_shade(fb, c, mirror_px, yk_aa);

    /* The quietest moment in the film is the right place to state what it is.
     * Two lines, low contrast, gone before they outstay their welcome. */
    f32 a = dm_smootherstep(1.0f, 2.6f, (f32)c->sec_t)
          * (1.0f - dm_smootherstep(6.4f, 7.8f, (f32)c->sec_t));
    if (a > 0.002f) {
        dm_text st = dm_text_default();
        st.size      = (f32)c->h * 0.026f;
        st.weight    = st.size * 0.075f;
        st.tracking  = 0.62f;
        st.color     = v3_scl(dm_rgb8(150, 190, 245), 1.1f * a);
        st.glow      = st.size * 0.22f;
        st.glow_gain = 0.22f * a;
        dm_text_draw_centered(fb, V2((f32)c->w * 0.5f, (f32)c->h * 0.80f),
                              "NO TEXTURES  NO SAMPLES  NO ENGINE", &st);

        st.size      = (f32)c->h * 0.020f;
        st.weight    = st.size * 0.08f;
        st.color     = v3_scl(dm_rgb8(110, 145, 200), 0.9f * a);
        dm_text_draw_centered(fb, V2((f32)c->w * 0.5f, (f32)c->h * 0.855f),
                              "EVERY PIXEL AND EVERY SAMPLE IS C CODE", &st);
    }
}
