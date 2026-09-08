/* sc_structure.c -- the thing being computed.
 *
 * A Menger sponge, twisted between levels and repeated along the vertical axis
 * so it streams past the camera forever. A portrait frame wants a subject that
 * is taller than it is wide; a bounded object would leave the top and bottom of
 * a 1080x1920 frame empty, and cropping a landscape composition to fill them is
 * how a short ends up looking like an offcut.
 *
 * Two cameras share the whole file. The first watches the tower from a distance
 * as it rises; the second sits at its foot looking up. Cutting between two views
 * of one world reads as intent. Cutting between two worlds in thirty seconds
 * reads as a compilation.
 */
#include "../demo.h"

/* ---- the distance estimator ---------------------------------------------- */

static inline v3 rot_y(v3 p, f32 a)
{
    f32 c = cosf(a), s = sinf(a);
    return V3(c * p.x - s * p.z, p.y, s * p.x + c * p.z);
}

static inline v3 rot_x(v3 p, f32 a)
{
    f32 c = cosf(a), s = sinf(a);
    return V3(p.x, c * p.y - s * p.z, s * p.y + c * p.z);
}

static inline f32 sd_box(v3 p, f32 b)
{
    v3 q = v3_sub(v3_abs(p), V3(b, b, b));
    return v3_len(v3_max(q, V3(0, 0, 0))) + DM_MIN(v3_maxc(q), 0.0f);
}

#define MENGER_LEVELS 4

/* A Menger sponge with a twist applied between levels.
 *
 * The plain sponge is already architecture -- square openings, flat faces,
 * right angles all the way down -- but every level lines up with the one above
 * it, so it reads as one shape photographed at four zooms. Rotating the space
 * a little between levels breaks that alignment, and what falls out looks like
 * a building that could not be built: arches that pass through each other,
 * floors that do not agree on which way is down.
 *
 * Chosen over the sorted folds this file started with for a simple reason: the
 * sponge is crisp. Folded distance estimators drift toward the organic, and an
 * organic blob does not say "this was computed" -- it says the opposite.
 */
static f32 sponge(v3 p, f32 *trap)
{
    f32 d = sd_box(p, 1.0f);
    f32 s = 1.0f;
    f32 t = 1e9f;

    for (int i = 0; i < MENGER_LEVELS; i++) {
        p = rot_y(p, 0.085f);
        p = rot_x(p, 0.045f);

        v3 a = v3_sub(v3_mod(v3_scl(p, s), 2.0f), V3(1.0f, 1.0f, 1.0f));
        s *= 3.0f;

        /* Carve the cross out of the middle of every cell. The three pairwise
         * maxima are the three square shafts through the cube's faces. */
        v3  r  = v3_abs(v3_sub(V3(1.0f, 1.0f, 1.0f), v3_scl(v3_abs(a), 3.0f)));
        f32 da = DM_MAX(r.x, r.y);
        f32 db = DM_MAX(r.y, r.z);
        f32 dc = DM_MAX(r.z, r.x);
        f32 c  = (DM_MIN(da, DM_MIN(db, dc)) - 1.0f) / s;

        if (c > d) d = c;

        f32 rr = v3_dot(a, a);
        if (rr < t) t = rr;
    }

    *trap = t;
    return d;
}

/* The sponge spans [-1, 1], so repeating it every two units stacks the copies
 * face to face into a column with no gap and no overlap. The seam every two
 * units is not a defect: it reads as a storey, and it is the only thing in
 * frame that gives the ascent a sense of speed. */
#define REPEAT 2.0f

static f32 map(v3 p, f32 *trap)
{
    v3 q = p;
    q.y = dm_mod(q.y + REPEAT * 0.5f, REPEAT) - REPEAT * 0.5f;
    return sponge(q, trap);
}

static v3 normal_at(v3 p)
{
    const f32 e = 0.0016f;
    f32 t;
    f32 dx = map(V3(p.x + e, p.y, p.z), &t) - map(V3(p.x - e, p.y, p.z), &t);
    f32 dy = map(V3(p.x, p.y + e, p.z), &t) - map(V3(p.x, p.y - e, p.z), &t);
    f32 dz = map(V3(p.x, p.y, p.z + e), &t) - map(V3(p.x, p.y, p.z - e), &t);
    return v3_norm(V3(dx, dy, dz));
}

/* One shadow ray, kept soft by tracking the closest approach rather than only
 * whether something was hit. Without it the sponge is uniformly lit and reads
 * as a textured slab: it is the shadow of one storey falling across the next
 * that turns a repeating pattern into a building. */
static f32 soft_shadow(v3 ro, v3 rd)
{
    f32 res = 1.0f, t = 0.03f, tr;
    for (int i = 0; i < 32; i++) {
        f32 h = map(v3_add(ro, v3_scl(rd, t)), &tr);
        if (h < 0.0010f) return 0.0f;
        f32 s = 11.0f * h / t;
        if (s < res) res = s;
        t += dm_clamp(h, 0.012f, 0.34f);
        if (t > 7.0f) break;
    }
    return dm_sat(res);
}

/* Five taps along the normal, for the contact darkening a shadow ray is too
 * coarse to catch. */
static f32 ambient_occlusion(v3 p, v3 n)
{
    f32 occ = 0.0f, w = 1.0f, t;
    for (int i = 1; i <= 5; i++) {
        f32 h = 0.012f * (f32)i * (f32)i;
        f32 d = map(v3_add(p, v3_scl(n, h)), &t);
        occ += (h - d) * w;
        w   *= 0.62f;
    }
    return dm_sat(1.0f - 2.4f * occ);
}

/* ---- shading ------------------------------------------------------------- */

typedef struct { v3 ro, rd; } ray;

static v3 render(ray r, f32 glow_gain)
{
    f32 t = 0.02f, trap = 1e9f, best_trap = 1e9f;
    int hit = 0;
    f32 steps = 0.0f;

    for (int i = 0; i < 110; i++) {
        v3  p = v3_add(r.ro, v3_scl(r.rd, t));
        f32 d = map(p, &trap);
        if (trap < best_trap) best_trap = trap;
        steps += 1.0f;

        /* Epsilon grows with distance: a pixel covers more world further out,
         * so marching to a fixed epsilon there is wasted work that only buys
         * aliasing. */
        if (d < 0.0009f * t) { hit = 1; break; }
        t += d * 0.86f;
        if (t > 22.0f) break;
    }

    /* The glow is the march itself made visible: rays that spent many steps
     * near the surface without landing on it are the ones grazing the edges,
     * and lighting those is what gives the silhouette its halo. */
    f32 near_miss = steps / 110.0f;
    v3  glow = v3_scl(dm_pal_house(0.58f), powf(near_miss, 3.4f) * glow_gain);

    if (!hit) {
        v3 sky = v3_scl(dm_rgb8(11, 17, 38), 0.030f);
        return v3_add(sky, glow);
    }

    v3 p = v3_add(r.ro, v3_scl(r.rd, t));
    v3 n = normal_at(p);

    /* Orbit trap into the house palette. The trap is how far the folded point
     * ever got from the origin, so it varies with structure rather than with
     * position, and the colour follows the geometry instead of smearing across
     * it like a gradient would. */
    f32 tc  = dm_sat(powf(best_trap, 0.30f) * 1.45f);
    v3  alb = dm_pal_house(0.505f + tc * 0.115f);
    alb = v3_scl(alb, 0.30f + 0.70f * tc);

    v3  key_dir = v3_norm(V3(0.40f, 0.72f, -0.56f));
    f32 key = dm_sat(v3_dot(n, key_dir));
    if (key > 0.001f) key *= soft_shadow(v3_add(p, v3_scl(n, 0.004f)), key_dir);
    f32 fil = dm_sat(v3_dot(n, v3_norm(V3(-0.62f, 0.18f, 0.42f))));
    f32 rim = powf(1.0f - dm_sat(v3_dot(n, v3_neg(r.rd))), 3.0f);
    f32 ao  = ambient_occlusion(p, n);
    ao = powf(ao, 1.45f);

    v3 col = v3_mul(alb, v3_scl(dm_rgb8(255, 234, 205), powf(key, 1.25f) * 2.05f));
    col = v3_add(col, v3_mul(alb, v3_scl(dm_rgb8(80, 138, 250), fil * 0.40f)));
    col = v3_add(col, v3_scl(dm_rgb8(140, 200, 255), rim * 0.75f));
    col = v3_scl(col, ao);

    /* A cold haze with distance keeps the repeated copies from stacking into
     * visual noise: the far ones fall away instead of competing. */
    f32 fog = 1.0f - expf(-t * 0.135f);
    col = v3_lerp(col, v3_scl(dm_rgb8(14, 22, 48), 0.055f), dm_sat(fog));

    return v3_add(col, glow);
}

/* ---- camera A: outside, rising ------------------------------------------- */

/* Height is a pure function of time so the two cameras stay on the same shaft
 * and a cut between them reads as one continuous ascent. */
static f32 shaft_y(f64 t) { return (f32)t * 0.62f; }

static v3 outside_px(v2 uv, const rt_ctx *c, u32 seed)
{
    (void)seed;
    f32 y = shaft_y(c->t);

    v3 ro = V3(0.0f, y, -4.60f);
    ro = rot_y(ro, 0.62f + (f32)c->t * 0.085f);
    ro.y = y;

    v3 ta = V3(0.0f, y + 0.55f, 0.0f);
    v3 rr, uu, ff;
    rt_basis(ro, ta, &rr, &uu, &ff);

    v3 rd = rt_ray(uv, 1.25f);
    ray r;
    r.ro = ro;
    r.rd = v3_norm(v3_add(v3_add(v3_scl(rr, rd.x), v3_scl(uu, rd.y)),
                          v3_scl(ff, rd.z)));

    v3 col = render(r, 1.7f);
    return v3_scl(col, rt_vignette(uv, 0.30f));
}

/* ---- camera B: at the foot, looking up ----------------------------------- */

static v3 inside_px(v2 uv, const rt_ctx *c, u32 seed)
{
    (void)seed;
    f32 y = shaft_y(c->t);

    v3 ro = V3(1.85f, y, -2.05f);
    ro = rot_y(ro, 0.30f + (f32)c->t * 0.30f);
    ro.y = y;

    /* Looking almost straight up the shaft. The target leads the camera by a
     * long way in Y and barely at all in X and Z, which is what puts the
     * vanishing point at the top of a vertical frame. */
    v3 ta = V3(0.0f, y + 1.35f, 0.0f);
    v3 rr, uu, ff;
    rt_basis(ro, ta, &rr, &uu, &ff);

    v3 rd = rt_ray(uv, 1.05f);          /* wider lens, closer subject */
    ray r;
    r.ro = ro;
    r.rd = v3_norm(v3_add(v3_add(v3_scl(rr, rd.x), v3_scl(uu, rd.y)),
                          v3_scl(ff, rd.z)));

    v3 col = render(r, 1.30f);
    return v3_scl(col, rt_vignette(uv, 0.22f));
}

/* ---- scenes -------------------------------------------------------------- */

void scene_assemble(dm_fb *fb, const rt_ctx *c)
{
    rt_shade(fb, c, outside_px, rt_aa);

    /* Four bars to fill the frame once, then it holds for a beat before the
     * kick lands. Cubed so it starts fast and settles -- a linear wipe reads
     * as a transition effect, an easing one reads as work finishing. */
    f32 p = (f32)c->sec_phase / 0.92f;
    rt_reveal(fb, dm_ease_out_cubic(dm_sat(p)));

    rt_caption(fb, c, "COMPUTING", song_bar(0.35), song_bar(1.9));
}

void scene_lock(dm_fb *fb, const rt_ctx *c)
{
    rt_shade(fb, c, outside_px, rt_aa);
    rt_caption(fb, c, "NO GPU", song_bar(2.5), song_bar(5.6));
}

void scene_drive(dm_fb *fb, const rt_ctx *c)
{
    rt_shade(fb, c, inside_px, rt_aa);

    /* The second reveal is over in half a bar. Once the idea has landed, a
     * long repeat of it costs more attention than it returns. */
    f32 p = (f32)(c->sec_t / song_bar(0.5));
    rt_reveal(fb, dm_ease_out_cubic(dm_sat(p)));

    rt_caption(fb, c, "NO IMAGE FILES", song_bar(6.7), song_bar(10.6));
}
