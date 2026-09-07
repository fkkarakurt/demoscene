/* sc_fractal.c -- FRACTAL (bar 32).
 *
 * The centrepiece: thirty-two seconds inside a Mandelbox.
 *
 * The object is defined by four lines of arithmetic repeated twelve times --
 * fold the space into a box, fold it through a sphere, scale, add the original
 * point. Everything the camera flies past, every ledge and shaft and corner,
 * is a consequence of those four lines. There is no model here, and there
 * could not be one: the surface has detail at every scale you care to look at.
 *
 * The distance estimator is what makes it renderable at all. Tracking how much
 * the folds stretch space (`dr`) gives a conservative bound on how far a ray
 * can safely jump, which turns an infinitely detailed object into something a
 * CPU can march through in a hundred steps.
 */
#include "../demo.h"

/* Ten folds rather than twelve. Each one roughly doubles the detail, so the
 * last two only describe structure finer than a pixel at the distances this
 * camera ever gets to -- they were paying for aliasing, not for detail. */
#define MB_ITERS 10

static f32 mbox_de(v3 p, f32 scale, v3 *trap)
{
    const f32 MIN_R2 = 0.25f, FIX_R2 = 1.0f;

    v3  z  = p;
    f32 dr = 1.0f;
    v3  tr = V3(1e9f, 1e9f, 1e9f);

    for (int i = 0; i < MB_ITERS; i++) {
        /* Box fold: reflect anything outside the unit cube back inside. */
        z = v3_sub(v3_scl(v3_clamp(z, -1.0f, 1.0f), 2.0f), z);

        /* Sphere fold: invert the region between two radii inside out. */
        f32 r2 = v3_dot(z, z);
        if (r2 < MIN_R2) {
            f32 m = FIX_R2 / MIN_R2;
            z = v3_scl(z, m);
            dr *= m;
        } else if (r2 < FIX_R2) {
            f32 m = FIX_R2 / r2;
            z = v3_scl(z, m);
            dr *= m;
        }

        z  = v3_add(v3_scl(z, scale), p);
        dr = dr * fabsf(scale) + 1.0f;

        /* Orbit trap: how close the iterate came to each axis. This is the
         * only colour information the fractal has, and it is what makes the
         * surface look painted rather than plastic. */
        tr = v3_min(tr, v3_abs(z));
    }

    if (trap) *trap = tr;
    return v3_len(z) / fabsf(dr);
}

/* Tetrahedron normal: four samples at the corners of a tetrahedron give the
 * same gradient as six axis-aligned ones, weighted by the corner directions.
 * A third off the cost of every shaded pixel, for identical output. */
static v3 mbox_normal(v3 p, f32 scale)
{
    const f32 e = 0.0012f;
    static const f32 K[4][3] = {
        {  1.0f, -1.0f, -1.0f },
        { -1.0f, -1.0f,  1.0f },
        { -1.0f,  1.0f, -1.0f },
        {  1.0f,  1.0f,  1.0f },
    };
    v3 n = V3(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 4; i++) {
        v3 k = V3(K[i][0], K[i][1], K[i][2]);
        n = v3_add(n, v3_scl(k, mbox_de(v3_add(p, v3_scl(k, e)), scale, NULL)));
    }
    return v3_norm(n);
}

/* Ambient occlusion by walking a short way along the normal and asking how
 * much closer the surface stayed than it should have. */
static f32 mbox_ao(v3 p, v3 n, f32 scale)
{
    f32 occ = 0.0f, w = 1.0f;
    for (int i = 1; i <= 5; i++) {
        f32 h = 0.02f * (f32)i * (f32)i;
        f32 d = mbox_de(v3_add(p, v3_scl(n, h)), scale, NULL);
        occ += (h - d) * w;
        w   *= 0.62f;
    }
    return dm_sat(1.0f - occ * 2.6f);
}

/* Soft shadow: the penumbra falls out of how close the ray passed to the
 * surface, without ever tracing a second full path. */
static f32 mbox_shadow(v3 ro, v3 rd, f32 scale)
{
    f32 res = 1.0f, t = 0.02f;
    for (int i = 0; i < 24; i++) {
        f32 h = mbox_de(v3_add(ro, v3_scl(rd, t)), scale, NULL);
        if (h < 0.0008f) return 0.0f;
        res = DM_MIN(res, 12.0f * h / t);
        t  += dm_clamp(h, 0.012f, 0.40f);
        if (t > 7.0f) break;
    }
    return dm_sat(res);
}

static v3 fractal_px(v2 uv, const yk_ctx *c, u32 seed)
{
    (void)seed;
    f32 t = (f32)c->t;
    f32 p01 = (f32)c->sec_phase;

    /* The fold scale breathes with the bars. Changing it does not move the
     * object, it rebuilds it -- ledges grow and close over as the number
     * drifts, which is a kind of motion nothing else can do. */
    f32 scale = 2.42f + 0.16f * sinf(t * 0.10f) + 0.045f * c->bar_hit;

    /* A single long move: start outside looking at the whole object, spiral
     * in until the frame is filled by a few of its corridors, ease back out
     * before the section ends. */
    f32 in   = dm_ease_in_out_cubic(dm_sat(p01 * 1.25f));
    f32 out  = dm_smootherstep(0.86f, 1.0f, p01);
    f32 dist = dm_lerp(dm_lerp(7.4f, 3.05f, in), 5.2f, out);

    f32 yaw   = t * 0.135f;
    f32 pitch = 0.32f + 0.30f * sinf(t * 0.083f);

    v3 ro = V3(sinf(yaw) * cosf(pitch) * dist,
               sinf(pitch) * dist,
               cosf(yaw) * cosf(pitch) * dist);
    m3 cam = m3_look_at(ro, V3(0.0f, 0.0f, 0.0f), sinf(t * 0.06f) * 0.22f);
    v3 rd  = m3_mul_v3(cam, v3_norm(V3(uv.x, uv.y, 1.55f)));

    /* The Mandelbox at this scale is contained in a sphere of radius ~5.5.
     * Clipping the march to that sphere is what makes the wide shots at the
     * start of the section affordable: with the object small in frame most
     * rays miss it, and each of those was previously walking a hundred and
     * fifty twelve-iteration steps through nothing. */
    f32 t_near, t_far;
    if (!yk_sphere(ro, rd, 5.5f, &t_near, &t_far)) {
        f32 r2 = v2_dot(uv, uv);
        v3  bg = v3_scl(dm_pal_house(0.60f), 0.045f + 0.10f * expf(-r2 * 1.2f));
        bg = v3_add(bg, v3_scl(dm_pal_house(0.20f),
                               expf(-r2 * 5.0f) * 0.20f * (0.5f + c->bar_hit)));
        return v3_scl(bg, yk_vignette(uv, 0.42f));
    }

    f32 d    = DM_MAX(t_near, 0.0f) + 0.001f;
    f32 dmax = DM_MIN(t_far, 22.0f);
    int hit  = 0;
    v3  p    = ro, trap = V3(0, 0, 0);

    for (int i = 0; i < 130; i++) {
        p = v3_add(ro, v3_scl(rd, d));
        f32 h = mbox_de(p, scale, &trap);
        /* Epsilon grows with distance: past a certain range the extra detail
         * is finer than a pixel and marching for it only buys aliasing. */
        if (h < 0.00035f * (1.0f + d * 2.2f)) { hit = 1; break; }
        d += h * 0.92f;
        if (d > dmax) break;
    }

    v3 col;

    if (hit) {
        v3  n  = mbox_normal(p, scale);
        f32 ao = mbox_ao(p, n, scale);

        /* Colour from the orbit trap. The span has to be wide -- a narrow one
         * collapses the whole object into a single hue, and thirty-two seconds
         * of one colour is a long time to look at. The section phase drags the
         * whole range around on top of that, so the fractal is a different
         * colour when the camera pulls out than when it went in. */
        f32 tv = dm_sat(v3_minc(trap) * 1.9f);
        f32 tw = dm_sat(v3_len(trap) * 0.42f);
        v3  base = dm_pal_house(0.08f + tv * 0.44f + tw * 0.20f + p01 * 0.32f);
        base = v3_scl(base, 0.46f);

        v3  key  = v3_norm(V3(0.55f, 0.72f, -0.42f));
        f32 sh   = mbox_shadow(v3_add(p, v3_scl(n, 0.004f)), key, scale);
        f32 diff = dm_sat(v3_dot(n, key));

        col = v3_scl(base, diff * sh * 2.2f);

        /* Cold fill from above so the shadowed side is readable. With shadows
         * this hard the fill is doing most of the work of keeping the deep
         * shafts from going to solid black. */
        col = v3_add(col, v3_scl(v3_mul(base, dm_rgb8(110, 160, 255)),
                                 (0.55f + 0.5f * dm_sat(n.y)) * ao * 2.4f));

        /* Warm bounce from below, the colour the folds trap in their creases. */
        col = v3_add(col, v3_scl(v3_mul(base, dm_rgb8(255, 130, 165)),
                                 dm_sat(-n.y) * ao * 0.85f));

        f32 spec = powf(dm_sat(v3_dot(v3_reflect(rd, n), key)), 40.0f);
        col = v3_add(col, v3_scl(V3(1.0f, 0.95f, 0.88f), spec * sh * 1.5f));

        f32 rim = powf(1.0f - dm_sat(v3_dot(n, v3_neg(rd))), 3.5f);
        col = v3_add(col, v3_scl(dm_pal_house(0.16f), rim * (0.55f + 0.9f * c->beat_hit)));

        col = v3_scl(col, 0.25f + 0.75f * ao);
        col = yk_fog(col, v3_scl(dm_rgb8(24, 30, 74), 0.14f), d, 0.055f);
    } else {
        /* Background: the same palette, so the silhouette does not cut out
         * against an unrelated colour. */
        f32 r2 = v2_dot(uv, uv);
        col = v3_scl(dm_pal_house(0.60f), 0.045f + 0.10f * expf(-r2 * 1.2f));
        col = v3_add(col, v3_scl(dm_pal_house(0.20f),
                                 expf(-r2 * 5.0f) * 0.20f * (0.5f + c->bar_hit)));
    }

    return v3_scl(col, yk_vignette(uv, 0.42f));
}

void scene_fractal(dm_fb *fb, const yk_ctx *c)
{
    yk_shade(fb, c, fractal_px, yk_aa);
}
