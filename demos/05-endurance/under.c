/* under.c -- light under water.
 *
 * Everything below the surface is lit by two things at most, and in the deep
 * by only one. Daylight comes down through the surface, bent steeper by
 * refraction and losing its red in the first ten metres, its green in the
 * first hundred and its blue by a few hundred; it arrives as a direct beam and
 * as a glow from above, and both are attenuated by exactly the depth the point
 * is at. Below that there is only what was brought down: the lamps of the
 * vehicle that found her.
 *
 * The water between the camera and what it sees is the other half of every
 * picture here. It takes light away with distance and puts back the light it
 * scatters -- daylight near the surface, shafts of it where the ice lets it
 * through, and in the deep the cone of each lamp, which is why a lamp in water
 * shows its own beam.
 */
#include "under.h"

#include <stdlib.h>
#include <string.h>

/* ---- daylight ---------------------------------------------------------------- */

void under_view_daylight(under_view *v, v3 sun, f32 sun_power)
{
    if (sun.y <= 0.0f) {
        v->sun_irr = v->sky_irr = V3(0, 0, 0);
        v->sun_up = V3(0, 1, 0);
        return;
    }
    const f32 N_WATER = 1.333f;
    f32 cos_a = sun.y, sin_a = sqrtf(DM_MAX(0.0f, 1.0f - cos_a * cos_a));
    f32 sin_w = sin_a / N_WATER, cos_w = sqrtf(DM_MAX(0.0f, 1.0f - sin_w * sin_w));
    v3  hor = sin_a > 1e-5f ? v3_scl(V3(sun.x, 0.0f, sun.z), 1.0f / sin_a) : V3(1, 0, 0);
    v->sun_up = v3_norm(v3_add(v3_scl(hor, sin_w), V3(0.0f, cos_w, 0.0f)));

    /* Fresnel transmission into water, Schlick's form, at the sun's angle. */
    f32 R0 = 0.02f;
    f32 T  = 1.0f - (R0 + (1.0f - R0) * powf(1.0f - cos_a, 5.0f));
    /* A low sun has been reddened by the air; at twenty degrees, a little. */
    v3 air = V3(0.90f, 0.82f, 0.68f);
    v->sun_irr = v3_scl(air, sun_power * T);
    v->sky_irr = v3_scl(V3(0.55f, 0.72f, 1.0f), sun_power * 0.12f);
}

/* Is the surface above a point open water? 1 through a lead, a few per cent
 * through ice and snow a metre or two thick. */
static f32 surface_open(const under_view *v, v3 x)
{
    if (!v->has_ice) return 1.0f;
    int mat;
    f32 h = en_ice_height(&v->ice, x.x, x.z, 0.1f, &mat);
    if (mat == ICE_WATER || h < 0.02f) return 1.0f;
    return 0.015f;
}

/* Direct sunlight arriving at a point: along the refracted beam, through the
 * depth of water it crosses, and through the lead or the ice above. */
static v3 sun_at(const under_view *v, v3 p)
{
    if (v3_maxc(v->sun_irr) <= 0.0f) return V3(0, 0, 0);
    f32 d = DM_MAX(v->depth_bias - p.y, 0.0f);
    f32 path = d / DM_MAX(v->sun_up.y, 0.05f);
    v3 top = v3_add(p, v3_scl(v->sun_up, path));
    return v3_scl(v3_mul(v->sun_irr, en_water_trans(path)), surface_open(v, top));
}

static v3 ambient_at(const under_view *v, v3 n, f32 depth)
{
    v3 e = v3_mul(v->sky_irr, en_daylight_at(depth));
    return v3_scl(e, 0.55f + 0.45f * n.y);
}

/* ---- the ice from below -------------------------------------------------------
 *
 * Sea ice floats with about five-sixths of itself under water, and a ridge's
 * keel goes down three or four times as far as its sail goes up. From
 * underneath, then, the pack is a ceiling a metre or two down, with keels
 * hanging from it, and the leads are slots of light.
 */
static f32 ice_bottom(const under_view *v, f32 x, f32 z, int *open)
{
    int mat;
    f32 h = en_ice_height(&v->ice, x, z, 0.08f, &mat);
    if (mat == ICE_WATER || h < 0.02f) { *open = 1; return 0.0f; }
    *open = 0;
    /* Level ice floats with its freeboard a tenth of its thickness and the
     * snow on it; a ridge's keel hangs three or four times its sail. */
    return -(0.35f + 1.8f * DM_MAX(h - 0.05f, 0.0f) + 2.2f * DM_MAX(h - 0.8f, 0.0f));
}

static int trace_ice_bottom(const under_view *v, v3 ro, v3 dir, f32 t_max, f32 *t_out, int *open_out)
{
    if (dir.y <= 1e-4f) return 0;
    /* Nothing hangs lower than the deepest keel. */
    f32 t = ro.y < -14.0f ? (-14.0f - ro.y) / dir.y : 0.0f;
    f32 t_end = DM_MIN(t_max, (0.0f - ro.y) / dir.y + 0.01f);
    f32 prev = t;
    for (int i = 0; i < 90 && t < t_end; i++) {
        v3  p = v3_add(ro, v3_scl(dir, t));
        int open;
        f32 b = ice_bottom(v, p.x, p.z, &open);
        f32 gap = b - p.y;
        if (gap <= 0.0f) {
            f32 a = prev, c = t;
            for (int k = 0; k < 5; k++) {
                f32 m = 0.5f * (a + c);
                v3  q = v3_add(ro, v3_scl(dir, m));
                if (ice_bottom(v, q.x, q.z, &open) - q.y <= 0.0f) c = m; else a = m;
            }
            t = 0.5f * (a + c);
            p = v3_add(ro, v3_scl(dir, t));
            ice_bottom(v, p.x, p.z, &open);
            *t_out = t;
            *open_out = open;
            return 1;
        }
        prev = t;
        t += DM_MAX(gap / (dir.y + 0.6f), 0.05f);
    }
    /* A ray can step past the surface itself between two samples; if it got
     * that far without meeting ice, it meets the water's surface. */
    f32 ts = -ro.y / dir.y;
    if (t >= t_end && ts <= t_max) {
        v3 p = v3_add(ro, v3_scl(dir, ts));
        int open;
        ice_bottom(v, p.x, p.z, &open);
        *t_out = ts;
        *open_out = open;
        return 1;
    }
    return 0;
}

/* ---- the seabed ---------------------------------------------------------------
 *
 * Abyssal mud: fine, pale, flat to the eye, pocked by what lives in it and
 * scattered with stones the ice once carried out from Antarctica and dropped.
 */
static f32 seabed_height(const under_view *v, f32 x, f32 z)
{
    f32 h = v->seabed_y;
    h += 0.25f * dm_fbm3(V3(x * 0.08f, 0.0f, z * 0.08f), 3, 2.0f, 0.5f, 0x5eadu);
    h += 0.03f * dm_fbm3(V3(x * 1.2f, 0.0f, z * 1.2f), 2, 2.0f, 0.5f, 0x5eaeu);
    /* Burrows and mounds: the traces of animals that live in the sediment. */
    f32 w = dm_worley3(V3(x * 0.9f, 0.3f, z * 0.9f), 0xb0bu);
    h -= 0.04f * dm_smoothstep(0.25f, 0.0f, w);
    return h;
}

static int trace_seabed(const under_view *v, v3 ro, v3 dir, f32 t_max, f32 *t_out)
{
    if (dir.y >= -1e-4f && ro.y > v->seabed_y + 0.4f) return 0;
    f32 top = v->seabed_y + 0.35f;
    f32 t = ro.y > top ? (ro.y - top) / -dir.y : 0.0f;
    f32 prev = t;
    for (int i = 0; i < 80 && t < t_max; i++) {
        v3  p = v3_add(ro, v3_scl(dir, t));
        f32 gap = p.y - seabed_height(v, p.x, p.z);
        if (gap < 0.0f) {
            f32 a = prev, c = t;
            for (int k = 0; k < 5; k++) {
                f32 m = 0.5f * (a + c);
                v3  q = v3_add(ro, v3_scl(dir, m));
                if (q.y - seabed_height(v, q.x, q.z) < 0.0f) c = m; else a = m;
            }
            *t_out = 0.5f * (a + c);
            return 1;
        }
        prev = t;
        t += DM_MAX(gap / (fabsf(dir.y) + 0.5f), 0.01f);
    }
    return 0;
}

/* ---- the pixel ----------------------------------------------------------------- */

static v3 lamps_on(const under_view *v, v3 p, v3 n)
{
    v3 acc = V3(0, 0, 0);
    for (int i = 0; i < v->nlamps; i++) acc = v3_add(acc, en_lamp_irradiance(&v->lamps[i], p, n));
    return acc;
}

v3 under_px(const en_ray *r, const void *vp, f32 *t_hit)
{
    const under_view *v = (const under_view *)vp;
    v3 ro = r->ro, dir = r->rd;

    f32 t_best = 400.0f;
    int what = 0;            /* 0 water, 1 ship, 2 ice, 3 surface, 4 seabed, 5 extra */
    int part = 0;

    if (v->dark) {
        /* A depth to a few pixels' accuracy is all that is wanted. */
        f32 ts;
        if (v->has_ship && en_ship_march(&v->st, en_ship_to_local(&v->pose, ro), en_ship_dir_to_local(&v->pose, dir),
                                         t_best, r->cone * 8.0f, &ts, &part))
            *t_hit = ts;
        return V3(0, 0, 0);
    }

    if (v->has_ship) {
        f32 ts;
        if (en_ship_march(&v->st, en_ship_to_local(&v->pose, ro), en_ship_dir_to_local(&v->pose, dir),
                          t_best, r->cone, &ts, &part)) { t_best = ts; what = 1; }
    }
    if (v->extra_sdf) {
        f32 a, b;
        if (en_ray_box(ro, dir, v->extra_lo, v->extra_hi, &a, &b)) {
            f32 t = DM_MAX(a, 0.0f);
            for (int i = 0; i < 120 && t < DM_MIN(b, t_best); i++) {
                int m;
                f32 d = v->extra_sdf(v3_add(ro, v3_scl(dir, t)), v->extra_ud, &m);
                if (d < t * r->cone * 0.6f + 0.0003f) { t_best = t; what = 5; part = m; break; }
                t += d * 0.9f;
            }
        }
    }
    if (v->has_seabed) {
        f32 ts;
        if (trace_seabed(v, ro, dir, t_best, &ts)) { t_best = ts; what = 4; }
    }
    int open = 0;
    if (v->has_ice) {
        f32 ti;
        if (trace_ice_bottom(v, ro, dir, t_best, &ti, &open)) { t_best = ti; what = open ? 3 : 2; }
    } else if (v->depth_bias <= 0.0f && dir.y > 1e-4f && ro.y < 0.0f) {
        f32 ts = -ro.y / dir.y;
        if (ts < t_best) { t_best = ts; what = 3; }
    }

    v3 col = V3(0, 0, 0);
    if (what) {
        v3 p = v3_add(ro, v3_scl(dir, t_best));
        f32 depth = DM_MAX(v->depth_bias - p.y, 0.0f);

        if (what == 3) {
            /* The underside of open water: inside Snell's window, the sky;
             * outside it, the water below reflected back down. */
            f32 cosw = dir.y;
            if (cosw > 0.66f) {
                v3 sky = v3_scl(v->sky_irr, DM_INVPI * 1.2f);
                f32 glare = powf(dm_sat(v3_dot(dir, v->sun_up)), 400.0f) * 60.0f;
                col = v3_add(sky, v3_scl(v->sun_irr, glare));
            } else {
                col = v3_scl(v3_mul(v->sky_irr, V3(0.02f, 0.06f, 0.08f)), DM_INVPI);
            }
        } else {
            v3 n, alb;
            f32 spec_k = 0.0f;
            if (what == 1) {
                v3 lp = en_ship_to_local(&v->pose, p);
                v3 ln = en_ship_normal(lp, &v->st);
                n = en_ship_dir_to_world(&v->pose, ln);
                f32 rough;
                alb = en_ship_albedo(lp, ln, part, &v->st, &rough);
                if (part == SHIP_GILT) spec_k = 1.0f;
            } else if (what == 5) {
                const f32 e = 0.002f;
                int m;
                f32 dx = v->extra_sdf(V3(p.x + e, p.y, p.z), v->extra_ud, &m) - v->extra_sdf(V3(p.x - e, p.y, p.z), v->extra_ud, &m);
                f32 dy = v->extra_sdf(V3(p.x, p.y + e, p.z), v->extra_ud, &m) - v->extra_sdf(V3(p.x, p.y - e, p.z), v->extra_ud, &m);
                f32 dz = v->extra_sdf(V3(p.x, p.y, p.z + e), v->extra_ud, &m) - v->extra_sdf(V3(p.x, p.y, p.z - e), v->extra_ud, &m);
                n = v3_norm(V3(dx, dy, dz));
                alb = v->extra_albedo ? v->extra_albedo(p, n, part, v->extra_ud) : V3s(0.5f);
            } else if (what == 4) {
                const f32 e = 0.01f;
                f32 hx = seabed_height(v, p.x + e, p.z) - seabed_height(v, p.x - e, p.z);
                f32 hz = seabed_height(v, p.x, p.z + e) - seabed_height(v, p.x, p.z - e);
                n = v3_norm(V3(-hx, 2.0f * e, -hz));
                f32 k = dm_fbm3(V3(p.x * 0.6f, 0.0f, p.z * 0.6f), 3, 2.0f, 0.5f, 0x5ea5u);
                alb = v3_scl(V3(0.52f, 0.48f, 0.40f), 0.85f + 0.25f * k);
                /* Dropstones: dark pebbles and cobbles on the mud. */
                f32 w = dm_worley3(V3(p.x * 0.35f, 0.7f, p.z * 0.35f), 0xd50u);
                alb = v3_lerp(alb, V3(0.14f, 0.13f, 0.12f), dm_smoothstep(0.12f, 0.08f, w));
            } else {
                /* The ice from below: blue-green, stained brown by the algae
                 * that live in its bottom few centimetres. */
                n = V3(0, -1, 0);
                f32 k = dm_fbm3(V3(p.x * 0.3f, 0.0f, p.z * 0.3f), 3, 2.0f, 0.5f, 0x1ceu);
                alb = v3_lerp(V3(0.30f, 0.45f, 0.40f), V3(0.35f, 0.30f, 0.18f), dm_sat(k + 0.5f));
            }

            v3 irr = ambient_at(v, n, depth);
            v3 sun = sun_at(v, p);
            if (v3_maxc(sun) > 0.0f) {
                f32 lam = dm_sat(v3_dot(n, v->sun_up));
                if (lam > 0.0f && v->has_ship && what != 1)
                    lam *= en_ship_shadow(&v->st, en_ship_to_local(&v->pose, p),
                                          en_ship_dir_to_local(&v->pose, v->sun_up), 0.02f);
                irr = v3_add(irr, v3_scl(sun, lam));
            }
            irr = v3_add(irr, lamps_on(v, p, n));
            col = v3_scl(v3_mul(alb, irr), DM_INVPI);
            if (what == 2) {
                /* Light through the ice itself, diffused by it into a glow
                 * over its whole underside: a few per cent through a metre of
                 * snow-covered floe, less through a keel, and blue-green,
                 * because the ice has had its red on the way. */
                f32 thick = -p.y * 1.15f;
                v3 through = v3_scl(v3_mul(v3_add(v->sun_irr, v->sky_irr), V3(0.30f, 0.80f, 0.85f)),
                                    0.06f * expf(-1.4f * thick) * DM_INVPI);
                col = v3_add(col, through);
            }

            if (spec_k > 0.0f) {
                v3 view = v3_neg(dir);
                for (int i = 0; i < v->nlamps; i++) {
                    v3 l = v3_norm(v3_sub(v->lamps[i].pos, p));
                    v3 h = v3_norm(v3_add(l, view));
                    v3 e = en_lamp_irradiance(&v->lamps[i], p, n);
                    col = v3_add(col, v3_scl(v3_mul(e, alb), powf(dm_sat(v3_dot(n, h)), 60.0f) * 8.0f * DM_INVPI));
                }
            }
        }
        col = v3_mul(col, en_water_trans(t_best));
        *t_hit = t_best;
    }

    /* The water in front of it. */
    v3 amb = v3_scl(v->sky_irr, 1.0f);
    col = v3_add(col, en_water_inscatter(ro, dir, what ? t_best : 400.0f, v->depth_bias,
                                         v->lamps, v->nlamps, amb, r->seed));

    /* Shafts of sunlight through the leads, marched: at each sample, whether
     * the sun's refracted beam reaches it through open water. */
    if (v->god_rays > 0.0f && v3_maxc(v->sun_irr) > 0.0f) {
        const int N = 14;
        f32 T = DM_MIN(what ? t_best : 120.0f, 120.0f);
        f32 step = T / (f32)N;
        f32 jit = dm_u32_to_f32(dm_hash_u32(r->seed ^ 0x90d5u));
        f32 cosang = v3_dot(dir, v->sun_up);
        const f32 g = 0.85f;
        f32 den = 1.0f + g * g - 2.0f * g * cosang;
        f32 ph = (1.0f - g * g) / (4.0f * DM_PI * den * sqrtf(DM_MAX(den, 1e-6f)));
        v3 acc = V3(0, 0, 0);
        for (int i = 0; i < N; i++) {
            f32 t = ((f32)i + jit) * step;
            v3  x = v3_add(ro, v3_scl(dir, t));
            if (x.y > 0.0f) break;
            v3 s = sun_at(v, x);
            acc = v3_add(acc, v3_mul(s, en_water_trans(t)));
        }
        col = v3_add(col, v3_scl(v3_mul(acc, EN_WATER_SCAT), ph * step * v->god_rays));
    }
    return col;
}

/* ---- a whole shot ---------------------------------------------------------------- */

void under_render(dm_fb *fb, f32 *depth, const en_ctx *c, const under_shot *shot)
{
    static under_view views[EN_MAX_SLICES];
    static en_ice_bake *bake = NULL;
    int n = c->spp;
    for (int k = 0; k < n; k++) {
        under_view *v = &views[k];
        memset(v, 0, sizeof *v);
        v->v.c = c;
        v->v.t = en_slice_time(c, k, n);
        v->ice.t = v->v.t;
        shot->setup(v, c, v->v.t, shot->ud);
        if (v->has_ship) en_ship_state_prepare(&v->st);
    }
    if (shot->bake_ice) {
        if (!bake) bake = en_ice_bake_new();
        en_ice_bake_build(bake, &views[0].ice, shot->bake_centre.x, shot->bake_centre.y, shot->bake_key);
        for (int k = 0; k < n; k++) views[k].ice.bake = bake;
    }

    en_shade(fb, depth, c, under_px, views, sizeof views[0], n);

    /* Marine snow: the slow fall of everything that dies in the water above,
     * in flakes a millimetre or two across. It hangs in a lattice of cells
     * round the camera, one flake to a cell by chance, each drifting upward
     * past the lens as the camera sinks through it -- and each lit only by
     * what light there is where it is. */
    if (shot->snow > 0.0f) {
        for (int k = 0; k < n; k++) {
            const under_view *v = &views[k];
            const en_cam *cam = &v->v.cam;
            const f32 CELL = 0.6f;
            f32 keep = shot->snow * CELL * CELL * CELL;
            f32 off  = (f32)(shot->snow_drift * v->v.t);
            v3  cp = cam->pos;
            const int R = 14;
            i32 cx = (i32)floorf(cp.x / CELL), cy = (i32)floorf((cp.y - off) / CELL), cz = (i32)floorf(cp.z / CELL);
            for (int j = -R; j <= R; j++)
            for (int i = -R; i <= R; i++)
            for (int l = -R; l <= R; l++) {
                u32 h = dm_hash3i(cx + i, cy + j, cz + l, 0x5a0fu);
                if (dm_u32_to_f32(h) > keep) continue;
                v3 q = V3(((f32)(cx + i) + dm_u32_to_f32(dm_hash_u32(h ^ 1u))) * CELL,
                          ((f32)(cy + j) + dm_u32_to_f32(dm_hash_u32(h ^ 2u))) * CELL + off,
                          ((f32)(cz + l) + dm_u32_to_f32(dm_hash_u32(h ^ 3u))) * CELL);
                v3 rel = v3_sub(q, cp);
                if (v3_dot(rel, cam->fw) < 0.05f) continue;
                f32 dist = v3_len(rel);
                f32 size = 0.0006f + 0.0016f * dm_u32_to_f32(dm_hash_u32(h ^ 4u));
                v3 irr = v3_add(ambient_at(v, V3(0, 1, 0), DM_MAX(v->depth_bias - q.y, 0.0f)), lamps_on(v, q, v3_norm(v3_neg(rel))));
                irr = v3_add(irr, v3_scl(sun_at(v, q), 0.5f));
                v3 rad = v3_mul(v3_scl(irr, 0.8f * DM_INVPI), en_water_trans(dist));
                en_splat(fb, depth, cam, q, size, rad, 1.0f / (f32)n);
            }
        }
    }
}
