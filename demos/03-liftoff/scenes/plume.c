/* plume.c -- the exhaust.
 *
 * Marched as a volume that emits and does not scatter, which for something
 * this much brighter than its surroundings is not an approximation worth
 * apologising for.
 *
 * The one thing worth getting right is that the shape is a function of the air
 * and not of the section it appears in. A nozzle is cut for one ambient
 * pressure; below it the jet is squeezed into a narrow column with standing
 * shock diamonds in it, and above it there is nothing left to squeeze and the
 * plume opens into a bell a hundred metres wide. So the same code draws a hard
 * white column at the pad and an enormous translucent flare at eighty
 * kilometres, and it is the density of the atmosphere in `flight.c` that moves
 * it between them.
 */
#include "../demo.h"

#define Y_S2  (LO_H_STAGE1 + LO_H_INTER)

/* How much fire there is, which is not the same as how much thrust there is.
 * The chamber lights in a fraction of a second and the exhaust is incandescent
 * from that instant; thrust takes another two and a half seconds to arrive as
 * the pumps spin up and the chamber pressure builds. On screen that makes the
 * ignition one event and the release a separate one three seconds later, which
 * is exactly what a launch looks like from the ground. */
static f32 flame_of(const lo_ctx *c) { return dm_sat(c->fl.throttle * 2.2f); }

/* Turbulence, and a specific kind: stretched along the axis, because that is
 * what a jet does to the eddies inside it. */
static f32 churn(v3 q, f32 t, u32 seed)
{
    v3 w = V3(q.x * 0.55f, q.y * 0.16f + t * 5.5f, q.z * 0.55f);
    return dm_fbm3(w, 2, 2.4f, 0.55f, seed);
}

v3 lo_plume(v3 ro, v3 rd, f32 t_max, const lo_ctx *c, u32 seed)
{
    f32 th = flame_of(c);
    if (th <= 0.001f) return V3(0, 0, 0);

    /* One over the sea level density: 1 on the pad, 0.03 at thirty kilometres,
     * nothing above sixty. */
    f32 amb = dm_sat(lo_air_density(c->fl.alt) / 1.225f);

    /* Where the gas leaves. The second stage burns from its own engine plane,
     * which is at the split. */
    f32 y0 = c->fl.stage == 2 ? (f32)Y_S2 - 0.4f : -1.55f;

    f32 flare = dm_lerp(0.155f, 0.050f, amb);     /* radius gained per metre  */
    f32 len   = dm_lerp(120.0f, 52.0f, amb) * (0.55f + 0.45f * th);
    f32 r0    = c->fl.stage == 2 ? 0.75f : 1.70f;
    /* Thin, out of the air. There is nothing for the exhaust to burn against
     * on the way out, so a vacuum plume is a wide faint glow rather than the
     * wall of fire it is at sea level -- and the shock diamonds go with the
     * air too, which the `dia` term below takes care of. */
    f32 gain  = dm_lerp(0.26f, 1.00f, amb) * th;

    /* Bound the march to the jet: a cylinder about the axis wide enough to
     * hold the cone, intersected with the slab of height it occupies. The slab
     * alone is not enough -- from a camera near the ground almost every ray in
     * the frame crosses it, and each one would then pay twenty-eight steps to
     * discover it was nowhere near the fire. */
    f32 t0 = 0.0f, t1 = t_max;
    {
        f32 Rmax = (r0 + len * flare) * 2.2f + 0.5f;
        f32 a  = rd.x * rd.x + rd.z * rd.z;
        f32 b  = ro.x * rd.x + ro.z * rd.z;
        f32 cc = ro.x * ro.x + ro.z * ro.z - Rmax * Rmax;
        if (a > 1e-8f) {
            f32 disc = b * b - a * cc;
            if (disc <= 0.0f) return V3(0, 0, 0);
            f32 s = sqrtf(disc);
            t0 = DM_MAX(t0, (-b - s) / a);
            t1 = DM_MIN(t1, (-b + s) / a);
        } else if (cc > 0.0f) {
            return V3(0, 0, 0);
        }

        f32 ylo = y0 - len, yhi = y0;
        if (fabsf(rd.y) > 1e-5f) {
            f32 ta = (ylo - ro.y) / rd.y, tb = (yhi - ro.y) / rd.y;
            if (ta > tb) { f32 s = ta; ta = tb; tb = s; }
            t0 = DM_MAX(t0, ta);
            t1 = DM_MIN(t1, tb);
        } else if (ro.y < ylo || ro.y > yhi) {
            return V3(0, 0, 0);
        }
        if (t1 <= t0) return V3(0, 0, 0);
    }

    const int N = 28;
    f32 step = (t1 - t0) / (f32)N;
    f32 jit  = dm_u32_to_f32(dm_hash_u32(seed)) * step;

    v3 acc = V3(0, 0, 0);

    /* The blackbody ramp, evaluated at three temperatures and interpolated
     * between them rather than called per step. `dm_kelvin` is two logarithms
     * and three powers, and unlike the fixed colours around it the temperature
     * genuinely varies down the jet, so nothing was going to hoist it. Over
     * the twelve hundred kelvin this jet spans the locus is close enough to
     * straight that three points hold it to within a per cent. */
    v3 bb_hot  = dm_kelvin(3200.0f);
    v3 bb_mid  = dm_kelvin(2600.0f);
    v3 bb_cool = dm_kelvin(2000.0f);
    v3 vacuum  = dm_rgb8(178, 206, 255);
    f32 mix    = amb * 0.9f + 0.1f;

    for (int i = 0; i < N; i++) {
        f32 t = t0 + jit + (f32)i * step;
        v3  q = v3_add(ro, v3_scl(rd, t));

        f32 s = y0 - q.y;                              /* distance down the jet */
        if (s < 0.0f || s > len) continue;

        f32 rr = sqrtf(q.x * q.x + q.z * q.z);
        f32 Rp = r0 + s * flare;
        f32 u  = rr / Rp;
        if (u > 2.2f) continue;

        f32 axial = powf(1.0f - s / len, 1.9f);
        f32 core  = expf(-u * u * 3.4f);

        /* Nothing here can survive the turbulence term, so do not pay for it.
         * Most of the samples in a cone this long are in the thin tail. */
        if (core * axial < 0.004f) continue;

        /* Shock diamonds. They exist because the jet leaves the nozzle at the
         * wrong pressure and keeps over- and under-correcting on the way out,
         * so they only appear where there is an atmosphere to be wrong about,
         * and they die out as the jet loses coherence. */
        f32 dia = 1.0f + 0.85f * amb * expf(-s * 0.055f)
                       * dm_sat(cosf(s * (DM_TAU / 5.4f)));

        f32 turb = 0.42f + 1.15f * dm_sat(churn(q, (f32)c->mt, seed) + 0.5f);

        /* The column near the throat is smooth; it only breaks up once it has
         * had time to. */
        turb = dm_lerp(1.0f, turb, dm_smoothstep(0.0f, 0.35f, s / len));

        f32 dens = core * axial * dia * turb;
        if (dens < 0.002f) continue;

        /* Temperature falls along the jet and away from the axis, and the
         * colour is a blackbody at that temperature -- so the flame is white
         * at the throat and orange at the tip because it is, not because a
         * gradient was chosen. */
        f32 k  = dm_sat(s / len * 1.25f + u * 0.35f);
        v3  em = k < 0.5f ? v3_lerp(bb_hot, bb_mid, k * 2.0f)
                          : v3_lerp(bb_mid, bb_cool, k * 2.0f - 1.0f);

        /* Out of the air the same gas glows blue-white: there is nothing left
         * to burn in and what is left is the shock structure. */
        em = v3_lerp(vacuum, em, mix);

        acc = v3_add(acc, v3_scl(em, dens * step * gain * 0.085f));
    }

    return acc;
}

/* The engines as a light source. Everything else in the pad shot is lit by
 * this: the vehicle, the tower, the ground, the smoke. It is one number and a
 * colour, and the inverse square is applied by whoever uses it. */
v3 lo_plume_light(const lo_ctx *c)
{
    f32 th = flame_of(c);
    if (th <= 0.001f) return V3(0, 0, 0);

    f32 amb  = dm_sat(lo_air_density(c->fl.alt) / 1.225f);
    v3  tint = v3_lerp(dm_rgb8(190, 214, 255), dm_rgb8(255, 178, 96), amb);
    return v3_scl(tint, th * (7.0e2f + 1.5e3f * amb));
}
