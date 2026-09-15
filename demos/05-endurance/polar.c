/* polar.c -- the pack, the ship, and the light on them.
 *
 * Three kinds of light, and each is what it physically was. The sky: the sun
 * and moon scattered through the air, integrated over the hemisphere once a
 * frame and read back as an ambient term. The sun or the moon directly, with
 * the ship's shadow and the ridges' shadows in it. And the lamps Hurley rigged
 * on poles over the side, which fall off with the square of the distance, so
 * the pools of light on the ice have edges nobody drew.
 */
#include "polar.h"

#include <stdlib.h>
#include <string.h>

/* ---- setup ------------------------------------------------------------------ */

void polar_view_light(polar_view *v)
{
    const en_sky *s = v->sky;
    v->sun_irr  = v3_scl(en_sun_transmittance(s), s->sun_power);
    v->moon_irr = en_moonlight(s);
}

void polar_view_ship_lamps(polar_view *v)
{
    en_ship_lamp sl[POLAR_MAX_LAMPS];
    int n = en_ship_lamps(&v->st, sl, POLAR_MAX_LAMPS - v->nlamps);
    for (int i = 0; i < n && v->nlamps < POLAR_MAX_LAMPS; i++) {
        en_lamp *L = &v->lamps[v->nlamps++];
        L->pos = en_ship_to_world(&v->pose, sl[i].pos);
        L->dir = en_ship_dir_to_world(&v->pose, sl[i].dir);
        L->cos_outer = sl[i].cos_outer;
        L->cos_inner = sl[i].cos_inner;
        L->power = sl[i].power;
    }
}

f32 polar_hull_fn(f32 x, f32 z, void *ud)
{
    const polar_view *v = (const polar_view *)ud;
    v3 l = en_ship_to_local(&v->pose, V3(x, 0.3f, z));
    /* Far from her, a box round the hull is distance enough, and the lines
     * are not worth lofting for every step of every ray across the pack. */
    f32 box = DM_MAX(fabsf(l.x) - 23.0f, fabsf(l.z) - 4.0f);
    if (box > 8.0f) return box;
    return en_hull_waterline_distance(l.x, l.z, l.y);
}

/* ---- the ship --------------------------------------------------------------- */

static int trace_ship(const polar_view *v, v3 ro, v3 rd, f32 t_max, f32 cone,
                      f32 *t_out, int *part_out)
{
    return en_ship_march(&v->st, en_ship_to_local(&v->pose, ro), en_ship_dir_to_local(&v->pose, rd),
                         t_max, cone, t_out, part_out);
}

static f32 ship_shadow(const polar_view *v, v3 p, v3 l)
{
    /* The sun is half a degree wide and the edge of a shadow is as soft as
     * that makes it: sharp under a rope, soft at the foot of a mast. */
    return en_ship_shadow(&v->st, en_ship_to_local(&v->pose, p), en_ship_dir_to_local(&v->pose, l), 0.0046f);
}

static f32 ship_ao(const polar_view *v, v3 lp, v3 ln)
{
    f32 occ = 0.0f, w = 1.0f;
    for (int i = 1; i <= 4; i++) {
        f32 h = 0.08f * (f32)(i * i);
        f32 d = en_ship_sdf(v3_add(lp, v3_scl(ln, h)), &v->st, 0);
        occ += (h - d) * w;
        w *= 0.6f;
    }
    return dm_sat(1.0f - 0.9f * occ);
}

/* ---- the ice ---------------------------------------------------------------- */

/* The ridges' own shadows, for a low light: walk toward it and see whether the
 * pack rises above the ray within a few tens of metres. */
static f32 ice_shadow(const polar_view *v, v3 p, v3 l)
{
    if (l.y <= 0.0f) return 0.0f;
    if (l.y > 0.6f) return 1.0f;
    f32 t = 0.08f, res = 1.0f;
    for (int i = 0; i < 24 && t < 40.0f; i++) {
        v3 q = v3_add(p, v3_scl(l, t));
        if (q.y > 4.0f) break;
        f32 h = en_ice_height(&v->ice, q.x, q.z, DM_MAX(0.05f, t * 0.02f), NULL);
        f32 gap = q.y - h;
        if (gap < 0.0f) return 0.0f;
        res = DM_MIN(res, gap / (t * 0.02f + 0.01f));
        t += DM_MAX(gap * 0.8f, 0.10f + t * 0.08f);
    }
    return dm_sat(res);
}

/* Occlusion from the shape of the pack: a hollow sees less sky than a crest. */
static f32 ice_ao(const polar_view *v, v3 p)
{
    const f32 r = 0.6f;
    f32 h0 = p.y;
    f32 s = 0.0f;
    static const v2 D[4] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    for (int i = 0; i < 4; i++)
        s += DM_MAX(en_ice_height(&v->ice, p.x + D[i].x * r, p.z + D[i].y * r, 0.1f, NULL) - h0, 0.0f);
    return dm_sat(1.0f - 0.35f * s);
}

/* ---- light ------------------------------------------------------------------ */

static v3 lamps_at(const polar_view *v, v3 p, v3 n, f32 wrap)
{
    v3 acc = V3(0, 0, 0);
    for (int i = 0; i < v->nlamps; i++) {
        const en_lamp *L = &v->lamps[i];
        v3  dv = v3_sub(L->pos, p);
        f32 r2 = v3_dot(dv, dv) + 0.05f;
        f32 r  = sqrtf(r2);
        v3  ld = v3_scl(dv, 1.0f / r);
        f32 lam = dm_sat((v3_dot(n, ld) + wrap) / (1.0f + wrap));
        f32 cone = dm_smoothstep(L->cos_outer, L->cos_inner, v3_dot(v3_neg(ld), L->dir));
        acc = v3_add(acc, v3_scl(L->power, lam * cone / r2));
    }
    return acc;
}

/* Light the air round a lamp scatters toward the eye: a point source in a thin
 * uniform haze, whose single-scattering integral along a ray has a closed form
 * -- the angle the ray subtends at the lamp, over its closest distance. */
static v3 lamp_glow(const polar_view *v, v3 ro, v3 rd, f32 t_end)
{
    if (v->air_glow <= 0.0f) return V3(0, 0, 0);
    v3 acc = V3(0, 0, 0);
    for (int i = 0; i < v->nlamps; i++) {
        const en_lamp *L = &v->lamps[i];
        v3  oc = v3_sub(L->pos, ro);
        f32 tc = v3_dot(oc, rd);
        f32 D  = DM_MAX(v3_len(v3_sub(oc, v3_scl(rd, tc))), 0.05f);
        f32 ang = atanf((DM_MIN(t_end, 1e5f) - tc) / D) - atanf((0.0f - tc) / D);
        acc = v3_add(acc, v3_scl(L->power, v->air_glow * ang / D * (0.25f * DM_INVPI)));
    }
    return acc;
}

typedef struct {
    v3 l;       /* direction toward the light */
    v3 irr;     /* irradiance it delivers     */
} dir_light;

/* Snow sends most of the light it gets back up again. Anything that faces
 * down or sideways -- a black hull, the underside of a yard -- is lit by the
 * pack beneath it as well as by the sky, and at night on the ice that second
 * light is most of what shows the hull as a shape at all. A lambertian plane of
 * albedo 0.85 under the whole scene, lit by what lights the pack. */
static v3 snow_bounce(const dir_light *dl, int ndl, v3 sky_up, v3 n)
{
    v3 e = sky_up;
    for (int i = 0; i < ndl; i++) e = v3_add(e, v3_scl(dl[i].irr, DM_MAX(dl[i].l.y, 0.0f)));
    return v3_scl(e, 0.85f * (1.0f - n.y) * 0.5f);
}

static int dir_lights(const polar_view *v, dir_light *out)
{
    int n = 0;
    if (v->sky->sun.y > -0.02f && v3_maxc(v->sun_irr) > 0.0f) { out[n].l = v->sky->sun; out[n].irr = v->sun_irr; n++; }
    if (v->sky->moon.y > -0.02f && v3_maxc(v->moon_irr) > 0.0f) { out[n].l = v->sky->moon; out[n].irr = v->moon_irr; n++; }
    return n;
}

/* ---- the pixel ----------------------------------------------------------------- */

v3 polar_px(const en_ray *r, const void *vp, f32 *t_hit)
{
    const polar_view *v = (const polar_view *)vp;
    const en_sky *sky = v->sky;
    v3 ro = r->ro, rd = r->rd;

    f32 t_best = 1e9f;
    int what = 0;               /* 0 sky, 1 ice, 2 ship */
    int part = 0, mat = 0;
    v3  n = V3(0, 1, 0);

    if (v->has_ship) {
        f32 ts;
        if (trace_ship(v, ro, rd, t_best, r->cone, &ts, &part)) { t_best = ts; what = 2; }
    }
    {
        f32 ti; v3 ni; int mi;
        if (en_ice_trace(&v->ice, ro, rd, t_best, r->cone, &ti, &ni, &mi)) {
            t_best = ti; what = 1; n = ni; mat = mi;
        }
    }

    v3 col;
    if (what == 0) {
        col = en_sky_radiance(sky, rd, r->seed);
    } else {
        v3 p = v3_add(ro, v3_scl(rd, t_best));
        v3 view = v3_neg(rd);
        dir_light dl[2];
        int ndl = dir_lights(v, dl);

        if (what == 1) {
            f32 tr;
            v3 alb = en_ice_albedo(&v->ice, p, n, mat, &tr);
            f32 ao = ice_ao(v, p);
            if (v->has_ship) {
                /* The hull shades the ice right against it from the sky. */
                v3 lp = en_ship_to_local(&v->pose, p);
                f32 d = en_ship_sdf(lp, &v->st, 0);
                ao *= dm_lerp(0.45f, 1.0f, dm_smoothstep(0.0f, 3.5f, d));
            }
            v3 irr = v3_scl(v3_add(en_sky_ambient(sky, n), snow_bounce(dl, ndl, en_sky_ambient(sky, V3(0, 1, 0)), n)), ao);
            for (int i = 0; i < ndl; i++) {
                f32 lam = dm_sat(v3_dot(n, dl[i].l));
                if (lam <= 0.0f) continue;
                f32 sh = ice_shadow(v, v3_add(p, V3(0, 0.02f, 0)), dl[i].l);
                if (v->has_ship && sh > 0.0f) sh *= ship_shadow(v, p, dl[i].l);
                irr = v3_add(irr, v3_scl(dl[i].irr, lam * sh));
            }
            irr = v3_add(irr, lamps_at(v, p, n, 0.0f));
            col = v3_scl(v3_mul(alb, irr), DM_INVPI);

            /* Light carried under the surface of the ice and out again: blue,
             * and mostly on the broken faces of the blocks. */
            if (tr > 0.0f) {
                v3 blue = V3(0.10f, 0.45f, 0.85f);
                v3 sub = v3_scl(en_sky_ambient(sky, V3(0, 1, 0)), 0.5f);
                for (int i = 0; i < ndl; i++) sub = v3_add(sub, v3_scl(dl[i].irr, dm_sat(dl[i].l.y) * 0.5f));
                sub = v3_add(sub, v3_scl(lamps_at(v, p, V3(0, 1, 0), 1.0f), 0.5f));
                col = v3_add(col, v3_scl(v3_mul(blue, sub), tr * 0.18f * DM_INVPI));
            }

            if (mat == ICE_SNOW || mat == ICE_BLOCK) {
                for (int i = 0; i < ndl; i++)
                    col = v3_add(col, v3_scl(dl[i].irr, en_snow_glint(p, n, dl[i].l, view, r->cone * t_best) * 0.004f));
                for (int i = 0; i < v->nlamps; i++) {
                    v3 lv = v3_sub(v->lamps[i].pos, p);
                    f32 r2 = v3_dot(lv, lv) + 0.05f;
                    col = v3_add(col, v3_scl(v->lamps[i].power,
                                             en_snow_glint(p, n, v3_scl(lv, 1.0f / sqrtf(r2)), view, r->cone * t_best) * 0.02f / r2));
                }
            }
            if (mat == ICE_WATER) {
                /* Open water at the freezing point: nearly black, a mirror at
                 * a grazing angle, and wherever something has just gone down
                 * through it, churned white. */
                f32 tt = (f32)v->v.t;
                v3 wn = v3_norm(V3(0.06f * dm_pnoise3(V3(p.x * 0.7f, tt * 0.6f, p.z * 0.7f), 0x3a1u), 1.0f,
                                   0.06f * dm_pnoise3(V3(p.x * 0.7f + 9.0f, tt * 0.6f, p.z * 0.7f), 0x3a2u)));
                v3 rr = v3_reflect(rd, wn);
                if (rr.y < 0.0f) rr.y = -rr.y;
                f32 fres = 0.02f + 0.98f * powf(1.0f - dm_sat(v3_dot(wn, view)), 5.0f);
                v3 deep = v3_scl(v3_mul(V3(0.01f, 0.03f, 0.04f), en_sky_ambient(sky, V3(0, 1, 0))), 1.0f);
                col = v3_add(deep, v3_scl(en_sky_radiance(sky, rr, r->seed), fres));
                for (int i = 0; i < ndl; i++) {
                    f32 g = powf(dm_sat(v3_dot(rr, dl[i].l)), 800.0f) * 200.0f;
                    col = v3_add(col, v3_scl(dl[i].irr, g * fres));
                }
                if (v->foam > 0.0f) {
                    f32 dd = v2_len(V2(p.x - v->foam_at.x, p.z - v->foam_at.y)) / DM_MAX(v->foam_radius, 0.1f);
                    f32 streak = dm_sat(dm_fbm3(V3(p.x * 0.35f, tt * 0.5f, p.z * 0.35f), 3, 2.0f, 0.5f, 0xf0a1u) * 1.8f + 0.4f);
                    f32 k = v->foam * dm_sat(1.0f - dd) * streak;
                    v3 white = v3_scl(V3(0.85f, 0.88f, 0.90f), DM_INVPI);
                    v3 lit = en_sky_ambient(sky, V3(0, 1, 0));
                    for (int i = 0; i < ndl; i++) lit = v3_add(lit, v3_scl(dl[i].irr, dm_sat(dl[i].l.y)));
                    col = v3_lerp(col, v3_mul(white, lit), dm_sat(k));
                }
            }
        } else {
            v3 lp = en_ship_to_local(&v->pose, p);
            v3 ln = en_ship_normal(lp, &v->st);
            n = en_ship_dir_to_world(&v->pose, ln);
            f32 rough;
            v3 alb = en_ship_albedo(lp, ln, part, &v->st, &rough);
            f32 ao = ship_ao(v, lp, ln);

            v3 irr = v3_scl(v3_add(en_sky_ambient(sky, n), snow_bounce(dl, ndl, en_sky_ambient(sky, V3(0, 1, 0)), n)), ao);
            v3 spec = V3(0, 0, 0);
            f32 shin = part == SHIP_GILT ? 90.0f : 12.0f;
            f32 ks   = part == SHIP_GILT ? 0.9f : 0.04f;
            v3 sc    = part == SHIP_GILT ? alb : V3s(1.0f);
            for (int i = 0; i < ndl; i++) {
                f32 lam = dm_sat(v3_dot(n, dl[i].l));
                if (lam <= 0.0f) continue;
                f32 sh = ship_shadow(v, v3_add(p, v3_scl(n, 0.01f)), dl[i].l);
                irr = v3_add(irr, v3_scl(dl[i].irr, lam * sh));
                v3 h = v3_norm(v3_add(dl[i].l, view));
                spec = v3_add(spec, v3_scl(dl[i].irr, ks * powf(dm_sat(v3_dot(n, h)), shin) * (shin + 2.0f) / 8.0f * lam * sh));
            }
            irr = v3_add(irr, lamps_at(v, p, n, 0.15f));
            col = v3_add(v3_scl(v3_mul(alb, irr), DM_INVPI * (1.0f - ks * 0.5f)), v3_scl(v3_mul(sc, spec), DM_INVPI));
            col = v3_add(col, en_ship_emission(lp, ln, part, &v->st));
        }

        col = en_air(sky, rd, t_best, col);
        *t_hit = t_best;
    }

    col = v3_add(col, lamp_glow(v, ro, rd, t_best));
    return col;
}

/* ---- rigging ------------------------------------------------------------------ */

int polar_rigging(const polar_view *v, en_line *out, int cap)
{
    if (!v->has_ship) return 0;
    int n = en_ship_rigging(&v->st, out, cap);
    dir_light dl[2];
    int ndl = dir_lights(v, dl);

    /* Hemp is dark; hemp with frost on it is nearly white. */
    v3 alb = v3_lerp(V3(0.07f, 0.06f, 0.05f), V3(0.80f, 0.84f, 0.88f), dm_sat(v->st.rime));

    for (int i = 0; i < n; i++) {
        out[i].a = en_ship_to_world(&v->pose, out[i].a);
        out[i].b = en_ship_to_world(&v->pose, out[i].b);
        for (int e = 0; e < 2; e++) {
            v3 p = e ? out[i].b : out[i].a;
            /* A rope is lit from all round: half the sky, half of each light. */
            v3 irr = v3_scl(v3_add(en_sky_ambient(v->sky, V3(0, 1, 0)), en_sky_ambient(v->sky, V3(0, -1, 0))), 0.5f);
            for (int k = 0; k < ndl; k++) irr = v3_add(irr, v3_scl(dl[k].irr, 0.5f));
            irr = v3_add(irr, snow_bounce(dl, ndl, en_sky_ambient(v->sky, V3(0, 1, 0)), V3(0, 0, 0)));
            for (int k = 0; k < v->nlamps; k++) {
                const en_lamp *L = &v->lamps[k];
                v3  dv = v3_sub(p, L->pos);
                f32 r2 = v3_dot(dv, dv) + 1.0f;
                f32 cone = dm_smoothstep(L->cos_outer, L->cos_inner, v3_dot(v3_scl(dv, 1.0f / sqrtf(r2)), L->dir));
                irr = v3_add(irr, v3_scl(L->power, 0.5f * cone / r2));
            }
            v3 c = v3_scl(v3_mul(alb, irr), DM_INVPI);
            f32 dist = v3_len(v3_sub(p, v->v.cam.pos));
            c = en_air(v->sky, v3_norm(v3_sub(p, v->v.cam.pos)), dist, c);
            if (e) out[i].cb = c; else out[i].ca = c;
        }
    }
    return n;
}

/* ---- a whole shot ---------------------------------------------------------------- */

void polar_render(dm_fb *fb, f32 *depth, const en_ctx *c, const polar_shot *shot)
{
    static polar_view views[EN_MAX_SLICES];
    static en_ice_bake *bake = NULL, *bake2 = NULL;
    static en_line lines[1024];
    if (!bake) bake = en_ice_bake_new();
    if (shot->two_bakes && !bake2) bake2 = en_ice_bake_new();

    int n = c->spp;
    for (int k = 0; k < n; k++) {
        f64 t = en_slice_time(c, k, n);
        polar_view *v = &views[k];
        memset(v, 0, sizeof *v);
        v->v.c = c;
        v->v.t = t;
        v->sky = shot->sky;
        v->ice.t = t;
        shot->setup(v, c, t, shot->ud);
        /* The ice's callbacks look back at the view they belong to. */
        if (v->ice.hull_fn && !v->ice.hull_ud) v->ice.hull_ud = v;
        if (v->ice.extra_fn && !v->ice.extra_ud) v->ice.extra_ud = v;
        if (v->has_ship) en_ship_state_prepare(&v->st);
        polar_view_light(v);
        polar_view_ship_lamps(v);
    }

    /* One bake serves every slice of a frame and, while the key holds, every
     * frame of a shot. */
    en_ice_bake_build(bake, &views[0].ice, shot->bake_centre.x, shot->bake_centre.y, shot->bake_key);
    if (shot->two_bakes)
        en_ice_bake_build(bake2, &views[0].ice, shot->bake2_centre.x, shot->bake2_centre.y,
                          shot->bake2_key ? shot->bake2_key : shot->bake_key + 0x9e37u);
    for (int k = 0; k < n; k++) { views[k].ice.bake = bake; views[k].ice.bake2 = shot->two_bakes ? bake2 : NULL; }

    en_shade(fb, depth, c, polar_px, views, sizeof views[0], n);

    for (int k = 0; k < n; k++) {
        int nl = polar_rigging(&views[k], lines, 1024);
        en_lines_draw(fb, depth, &views[k].v.cam, lines, nl, 1.0f / (f32)n);

        /* The lamps themselves: a filament a few centimetres across, far
         * brighter than anything it lights -- behind a frosted glass, which
         * spreads it over the whole globe and keeps the lens from flaring the
         * picture white. */
        for (int i = 0; i < views[k].nlamps; i++) {
            const en_lamp *L = &views[k].lamps[i];
            f32 area = DM_PI * 0.12f * 0.12f;
            en_splat(fb, depth, &views[k].v.cam, L->pos, 0.12f, v3_scl(L->power, 0.08f / area), 1.0f / (f32)n);
        }
    }
}

f32 polar_fall_angle(f32 t, f32 length, f32 lean0, f32 stop)
{
    if (t <= 0.0f) return lean0;
    /* A uniform rod about one end: I = mL^2/3, torque = mgL/2 sin(theta),
     * so theta'' = (3g / 2L) sin(theta). Integrated in millisecond steps. */
    const f32 g = 9.81f, dt = 0.001f;
    f32 k = 1.5f * g / length;
    f32 th = lean0, w = 0.0f;
    int steps = (int)(t / dt);
    for (int i = 0; i < steps; i++) {
        w += k * sinf(th) * dt;
        th += w * dt;
        if (th >= stop) {
            /* It lands, and gives back a little. */
            f32 after = t - (f32)i * dt;
            return stop - 0.03f * expf(-after * 6.0f) * fabsf(sinf(after * 18.0f));
        }
    }
    return th;
}

f32 polar_shot_u(const en_ctx *c, f64 t)
{
    f64 a = en_shot_start(c->shot), b = en_shot_end(c->shot);
    return dm_clamp((f32)((t - a) / (b - a)), 0.0f, 1.0f);
}