/* sc_plasma.c -- PLASMA (bar 16).
 *
 * Two effects the scene has been doing since 1991, both taken seriously.
 *
 * The plasma behind is not the old sum-of-sines: it is a domain-warped noise
 * field, meaning noise is used to displace the lookup coordinates of more
 * noise. That single feedback step is the difference between the smooth
 * hypnotic wobble everyone remembers and something that actually churns.
 *
 * The metaballs in front are not the old 2D falloff-summing blobs either.
 * They are spheres unioned with a polynomial smooth-minimum, raymarched, and
 * lit as glass -- so they merge in three dimensions and reflect the plasma
 * that is generating them.
 */
#include "../demo.h"

#define NBALLS 7

/* The ball positions depend only on time, so they are resolved once per frame
 * and read from here by every ray. Recomputing them inside the distance
 * function costs four hashes and six transcendentals per ball per march step
 * -- with seven balls and a hundred steps that was, on measurement, about
 * nine tenths of the cost of this entire shot. */
static v3  g_ball[NBALLS];
static f32 g_rad[NBALLS];

static void ball_prepare(f32 t, f32 pulse)
{
    for (int i = 0; i < NBALLS; i++) {
        u32 h  = dm_hash_u32((u32)i * 2654435761u + 5u);
        f32 a  = dm_u32_to_f32(h) * DM_TAU;         h = dm_hash_u32(h);
        f32 b  = dm_u32_to_f32(h) * DM_TAU;         h = dm_hash_u32(h);
        f32 sp = 0.45f + dm_u32_to_f32(h) * 0.55f;  h = dm_hash_u32(h);
        f32 r  = 0.85f + dm_u32_to_f32(h) * 1.05f;

        g_ball[i] = V3(cosf(t * 0.52f * sp + a) * r,
                       sinf(t * 0.44f * sp + b) * r * 0.85f,
                       sinf(t * 0.37f * sp + a * 2.0f) * r);
        g_rad[i]  = 0.40f + 0.10f * sinf(t * 0.9f + (f32)i * 1.7f) + 0.06f * pulse;
    }
}

static f32 ball_map(v3 p)
{
    f32 d = 1e9f;
    for (int i = 0; i < NBALLS; i++) {
        f32 di = v3_len(v3_sub(p, g_ball[i])) - g_rad[i];
        /* Smooth minimum: the blend radius is what makes them merge like
         * mercury instead of intersecting like billiard balls. */
        d = dm_smin(d, di, 0.55f);
    }
    return d;
}

static v3 ball_normal(v3 p)
{
    const f32 e = 0.0035f;
    return v3_norm(V3(
        ball_map(V3(p.x + e, p.y, p.z)) - ball_map(V3(p.x - e, p.y, p.z)),
        ball_map(V3(p.x, p.y + e, p.z)) - ball_map(V3(p.x, p.y - e, p.z)),
        ball_map(V3(p.x, p.y, p.z + e)) - ball_map(V3(p.x, p.y, p.z - e))));
}

/* The plasma, evaluated in a direction rather than on the screen, so the
 * metaballs can reflect it as if it were an environment. */
static v3 plasma_env(v3 d, f32 t)
{
    v3  q = v3_scl(d, 2.2f);
    f32 n = dm_warp3(V3(q.x, q.y, q.z + t * 0.09f), 1.35f, 5, 77u);

    v3 col = dm_pal_house(0.52f + n * 0.42f);
    f32 m  = dm_sat(n * 0.5f + 0.5f);
    col = v3_scl(col, 0.06f + 0.55f * powf(m, 2.8f));

    /* Iso-contours: bright filaments where the field crosses fixed levels.
     * They give the plasma an edge, which is what keeps it from reading as
     * an out-of-focus smear once the bloom gets hold of it. Kept sparse --
     * at five bands per unit the contours stop being filaments and start
     * being a marble texture. */
    f32 band = fabsf(dm_fract(n * 2.6f) - 0.5f) * 2.0f;
    col = v3_add(col, v3_scl(dm_pal_house(0.14f), powf(1.0f - band, 30.0f) * 1.15f));
    return col;
}

static v3 plasma_px(v2 uv, const yk_ctx *c, u32 seed)
{
    (void)seed;
    f32 t     = (f32)c->t;
    f32 pulse = c->beat_hit;

    /* Slow orbit. The camera never stops moving, but never draws attention
     * to itself either. */
    f32 orb = t * 0.16f;
    v3  ro  = V3(sinf(orb) * 4.6f, sinf(t * 0.11f) * 1.1f, cosf(orb) * 4.6f);
    m3  cam = m3_look_at(ro, V3(0, 0, 0), sinf(t * 0.19f) * 0.16f);
    v3  rd  = m3_mul_v3(cam, v3_norm(V3(uv.x, uv.y, 1.7f)));

    /* The balls orbit within about 2.4 units of the origin; the smooth-minimum
     * blend can only pull the surface outward by the blend radius. Anything
     * outside that sphere is background and needs no marching at all. */
    f32 t_near, t_far;
    int inside = yk_sphere(ro, rd, 3.0f, &t_near, &t_far);

    f32 dist = inside ? DM_MAX(t_near, 0.0f) : 0.0f;
    f32 dmax = inside ? DM_MIN(t_far, 14.0f) : 0.0f;
    int hit  = 0;
    v3  p    = ro;

    for (int i = 0; inside && i < 80; i++) {
        p = v3_add(ro, v3_scl(rd, dist));
        f32 d = ball_map(p);
        if (d < 0.0018f) { hit = 1; break; }
        dist += d * 0.90f;
        if (dist > dmax) break;
    }

    v3 col;

    if (hit) {
        v3 n = ball_normal(p);
        v3 v = v3_neg(rd);

        /* Glass: what you see is mostly reflection at grazing angles and
         * mostly what is behind it head on. Schlick gives that for free. */
        f32 fres = 0.04f + 0.96f * powf(1.0f - dm_sat(v3_dot(n, v)), 5.0f);

        v3 refl = plasma_env(v3_reflect(rd, n), t);
        v3 refr = plasma_env(v3_refract(rd, n, 0.72f), t);

        col = v3_lerp(v3_scl(refr, 0.55f), v3_scl(refl, 1.25f), fres);

        /* Two hard lights so the silhouette still reads against the plasma. */
        v3 l1 = v3_norm(V3(0.6f, 0.8f, -0.3f));
        v3 l2 = v3_norm(V3(-0.7f, 0.2f, 0.6f));
        col = v3_add(col, v3_scl(dm_rgb8(255, 170, 235),
                                 powf(dm_sat(v3_dot(v3_reflect(rd, n), l1)), 48.0f) * 2.2f));
        col = v3_add(col, v3_scl(dm_rgb8(140, 230, 255),
                                 powf(dm_sat(v3_dot(v3_reflect(rd, n), l2)), 32.0f) * 1.4f));

        /* Rim: the tell that these are volumes, not discs. */
        col = v3_add(col, v3_scl(dm_pal_house(0.22f),
                                 powf(1.0f - dm_sat(v3_dot(n, v)), 3.0f) * (0.5f + pulse)));
    } else {
        col = plasma_env(rd, t);
        col = v3_scl(col, 0.85f + 0.5f * pulse);
    }

    return v3_scl(col, yk_vignette(uv, 0.45f));
}

void scene_plasma(dm_fb *fb, const yk_ctx *c)
{
    ball_prepare((f32)c->t, c->beat_hit);
    yk_shade(fb, c, plasma_px, yk_aa);
}
