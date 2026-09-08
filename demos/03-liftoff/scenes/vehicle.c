/* vehicle.c -- the object itself.
 *
 * Sixty-two and a half metres tall on a body three point six six across, which
 * is a slenderness ratio of seventeen. That number is the reason a launch
 * vehicle is instantly recognisable from a silhouette and the reason it fits a
 * vertical frame without being cropped into one: nothing else built is that
 * thin and that tall and still standing up.
 *
 * All of it is signed distance functions -- cylinders, one truncated cone per
 * engine bell, and an ellipsoid for the fairing, which gives exactly the ogive
 * profile r = R*sqrt(1 - u^2) with no special case. The detail that makes it
 * read as hardware is not geometry at all; it is in the albedo. Weld rings
 * every three metres, a raceway down one side, a black interstage, and frost
 * where the oxygen tank is, because that is what is actually cold.
 */
#include "../demo.h"

/* ---- primitives ---------------------------------------------------------- */

static f32 sd_cyl(v3 p, f32 y0, f32 y1, f32 r)
{
    f32 dr = sqrtf(p.x * p.x + p.z * p.z) - r;
    f32 dy = fabsf(p.y - (y0 + y1) * 0.5f) - (y1 - y0) * 0.5f;
    f32 ox = DM_MAX(dr, 0.0f), oy = DM_MAX(dy, 0.0f);
    return DM_MIN(DM_MAX(dr, dy), 0.0f) + sqrtf(ox * ox + oy * oy);
}

/* Truncated cone along y: radius r0 at y0, r1 at y1. */
static f32 sd_cone(v3 p, f32 y0, f32 r0, f32 y1, f32 r1)
{
    f32 h  = (y1 - y0) * 0.5f;
    f32 cy = (y0 + y1) * 0.5f;
    v2  q  = V2(sqrtf(p.x * p.x + p.z * p.z), p.y - cy);

    v2 k1 = V2(r1, h);
    v2 k2 = V2(r1 - r0, 2.0f * h);
    v2 ca = V2(q.x - DM_MIN(q.x, q.y < 0.0f ? r0 : r1), fabsf(q.y) - h);
    v2 cb = v2_add(v2_sub(q, k1),
                   v2_scl(k2, dm_sat(v2_dot(v2_sub(k1, q), k2) / v2_dot(k2, k2))));
    f32 s = (cb.x < 0.0f && ca.y < 0.0f) ? -1.0f : 1.0f;
    return s * sqrtf(DM_MIN(v2_dot(ca, ca), v2_dot(cb, cb)));
}

/* An ellipsoid, in the cheap bounded form. Used once, for the fairing. */
static f32 sd_ellipsoid(v3 p, v3 r)
{
    f32 k0 = v3_len(V3(p.x / r.x, p.y / r.y, p.z / r.z));
    f32 k1 = v3_len(V3(p.x / (r.x * r.x), p.y / (r.y * r.y), p.z / (r.z * r.z)));
    return k1 > 1e-9f ? k0 * (k0 - 1.0f) / k1 : -DM_MIN(r.x, DM_MIN(r.y, r.z));
}

/* ---- the stack ----------------------------------------------------------- */

#define R        LO_RADIUS
#define Y_INTER  LO_H_STAGE1                       /* 42.6: tanks end        */
#define Y_S2    (LO_H_STAGE1 + LO_H_INTER)         /* 49.0: the split        */
#define Y_FAIR  (Y_S2 + 6.0f)                      /* 55.0: fairing begins   */
#define Y_TIP    LO_HEIGHT                         /* 62.5                   */

/* Everything below the split: nine engines, the thrust structure, the tanks. */
static f32 stage1(v3 p, int *part)
{
    f32 body = sd_cyl(p, 0.55f, Y_INTER, R);
    int   pt = LO_PART_TANK;

    /* The octaweb: a slightly wider, squarer base than the tank above it. */
    f32 base = sd_cyl(p, 0.35f, 2.30f, R * 1.02f);
    if (base < body) { body = base; pt = LO_PART_ENGINE; }

    /* Nine bells. Eight are one bell drawn through an eightfold fold of the
     * angle, which is how a ring of anything should be done: one distance
     * evaluation for the ring, not one per member. */
    if (p.y < 1.2f) {
        f32 rad = sqrtf(p.x * p.x + p.z * p.z);
        f32 ang = atan2f(p.z, p.x);
        const f32 sect = DM_TAU / 8.0f;
        ang = dm_mod(ang + sect * 0.5f, sect) - sect * 0.5f;

        v3 q = V3(rad * cosf(ang) - 1.16f, p.y, rad * sinf(ang));
        f32 bell = sd_cone(q, -1.55f, 0.62f, 0.45f, 0.30f);

        v3 qc = V3(p.x, p.y, p.z);
        f32 mid = sd_cone(qc, -1.70f, 0.66f, 0.45f, 0.32f);

        f32 eng = DM_MIN(bell, mid);
        if (eng < body) { body = eng; pt = LO_PART_ENGINE; }
    }

    /* The raceway: the conduit that runs the length of the stage. It is two
     * hundred millimetres of pipe and it is the single detail that stops the
     * silhouette being a plain cylinder. */
    v3 rw = V3(p.x, p.y, p.z - (R + 0.16f));
    f32 race = sd_cyl(rw, 2.4f, Y_INTER - 0.4f, 0.24f);
    if (race < body) { body = race; pt = LO_PART_TANK; }

    /* Four stowed legs, flat against the base. */
    if (p.y < 8.0f) {
        f32 rad = sqrtf(p.x * p.x + p.z * p.z);
        f32 ang = atan2f(p.z, p.x);
        const f32 sect = DM_TAU / 4.0f;
        ang = dm_mod(ang + sect * 0.5f, sect) - sect * 0.5f;
        v3 q = V3(rad * cosf(ang) - (R + 0.22f), p.y, rad * sinf(ang));
        f32 leg = sd_cone(q, 0.8f, 0.42f, 7.2f, 0.16f);
        if (leg < body) { body = leg; pt = LO_PART_ENGINE; }
    }

    if (part) *part = pt;
    return body;
}

/* Everything above the split: interstage, second stage, fairing. */
static f32 stage2(v3 p, int *part)
{
    f32 inter = sd_cyl(p, Y_INTER, Y_S2, R);
    f32 body  = sd_cyl(p, Y_S2, Y_FAIR, R);

    /* The fairing. An ellipsoid cut off at its equator is exactly the ogive
     * r = R*sqrt(1 - u^2), so the nose costs one primitive and no special
     * case at the tip. */
    v3  q    = V3(p.x, p.y - Y_FAIR, p.z);
    f32 nose = DM_MAX(sd_ellipsoid(q, V3(R, Y_TIP - Y_FAIR, R)), -q.y);

    f32 d  = inter;
    int pt = LO_PART_BLACK;
    if (body < d) { d = body; pt = LO_PART_TANK; }
    if (nose < d) { d = nose; pt = LO_PART_NOSE; }

    if (part) *part = pt;
    return d;
}

f32 lo_vehicle_sdf(v3 p, f32 sep, int *part)
{
    int p1 = LO_PART_TANK, p2 = LO_PART_TANK;

    /* The discarded stage falls behind and rolls. `sep` drives both, so one
     * number describes the whole event and nothing has to be keyframed. */
    v3 q1 = p;
    if (sep > 0.0f) {
        v3 c = V3(0.0f, Y_INTER * 0.5f, 0.0f);
        q1 = v3_add(lo_rot_z(v3_sub(v3_add(p, V3(0.0f, sep, 0.0f)), c),
                             sep * 0.022f), c);
    }
    f32 d1 = stage1(q1, &p1);
    f32 d2 = stage2(p, &p2);

    if (part) *part = d1 < d2 ? p1 : p2;
    return DM_MIN(d1, d2);
}

v3 lo_vehicle_normal(v3 p, f32 sep)
{
    const f32 e = 0.010f;
    f32 dx = lo_vehicle_sdf(V3(p.x + e, p.y, p.z), sep, 0)
           - lo_vehicle_sdf(V3(p.x - e, p.y, p.z), sep, 0);
    f32 dy = lo_vehicle_sdf(V3(p.x, p.y + e, p.z), sep, 0)
           - lo_vehicle_sdf(V3(p.x, p.y - e, p.z), sep, 0);
    f32 dz = lo_vehicle_sdf(V3(p.x, p.y, p.z + e), sep, 0)
           - lo_vehicle_sdf(V3(p.x, p.y, p.z - e), sep, 0);
    return v3_norm(V3(dx, dy, dz));
}

/* ---- the surface ----------------------------------------------------------
 *
 * Almost none of what makes this look like hardware is in the geometry above.
 * It is here: rings where the barrel sections are welded together, a stencil
 * band, black where the interstage is black, and frost on the part of the tank
 * that has liquid oxygen behind it -- which is the top two thirds of the first
 * stage, and is exactly where the frost is on the real thing.
 */
v3 lo_vehicle_albedo(v3 p, int part, u32 seed)
{
    (void)seed;

    if (part == LO_PART_ENGINE) {
        /* Soot and scorched metal, darker the closer to the throat. */
        f32 burn = dm_smoothstep(-1.6f, 1.2f, p.y);
        v3 col = v3_lerp(v3_scl(dm_rgb8(26, 22, 20), 0.9f),
                         v3_scl(dm_rgb8(96, 92, 88), 0.9f), burn);
        f32 n = dm_fbm3(v3_scl(p, 3.2f), 3, 2.0f, 0.5f, 0x9a1);
        return v3_scl(col, 0.75f + 0.45f * dm_sat(n + 0.5f));
    }

    if (part == LO_PART_BLACK) {
        f32 n = dm_fbm3(v3_scl(p, 6.0f), 3, 2.0f, 0.5f, 0x51b);
        return v3_scl(dm_rgb8(20, 20, 24), 0.55f + 0.30f * dm_sat(n + 0.5f));
    }

    v3 white = dm_rgb8(232, 234, 238);

    /* Weld rings every three metres, and a narrower one between them: the
     * barrel sections of a real tank are that long because that is what fits
     * on the machine that friction stir welds them. */
    f32 ring  = fabsf(dm_fract(p.y / 3.05f) - 0.5f) * 2.0f;
    f32 seam  = 1.0f - dm_smoothstep(0.90f, 1.00f, ring);
    v3  col   = v3_scl(white, 1.0f - 0.20f * seam);

    /* Frost. Above the interstage there is no oxygen tank, so there is no
     * frost -- which is why the fairing on a real vehicle stays white while
     * the stage below it goes grey and streaked. */
    if (part == LO_PART_TANK && p.y > 12.0f && p.y < Y_INTER) {
        f32 band = dm_smoothstep(12.0f, 22.0f, p.y) *
                   (1.0f - dm_smoothstep(Y_INTER - 3.0f, Y_INTER, p.y));
        f32 n = dm_fbm3(V3(p.x * 1.4f, p.y * 0.55f, p.z * 1.4f), 4, 2.2f, 0.55f, 0x3f7);
        f32 ice = dm_sat(n * 2.0f + 0.35f) * band;
        col = v3_lerp(col, v3_scl(dm_rgb8(206, 226, 244), 1.06f), ice * 0.75f);
    }

    return col;
}
