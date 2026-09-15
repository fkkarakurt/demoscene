/* sea.c -- light in water.
 *
 * Water is blue for the same reason ice is: it absorbs red. Not much per
 * metre, but a lot per hundred -- and the sea is thousands of metres deep, so
 * the colour of a depth is a matter of arithmetic. Half the red light that
 * gets into this sea is gone in the first two metres; half the green in the
 * first eleven; the blue goes on for twenty. Below about two hundred metres
 * none of it is left worth measuring, and below a thousand there is nothing
 * at all but what the animals make themselves.
 *
 * Every shot under the ice and on the way down is lit by that, and by nothing
 * else until 2022, when the only light at three thousand metres is the light
 * that came down to look for her.
 */
#include "sea.h"

/* The Weddell Sea under the pack, before the spring bloom has properly begun:
 * as clear as sea water gets. Pure-water absorption, a trace of what lives in
 * it taking a little more of the blue, and particles enough to scatter a
 * visible beam but not to hide a ship fifty metres away. */
const v3 EN_WATER_ABS  = { 0.2800f, 0.0580f, 0.0300f };
const v3 EN_WATER_SCAT = { 0.0120f, 0.0140f, 0.0160f };
const v3 EN_WATER_EXT  = { 0.2920f, 0.0720f, 0.0460f };
const v3 EN_WATER_KD   = { 0.3000f, 0.0650f, 0.0400f };

v3 en_daylight_at(f32 depth_m)
{
    f32 d = DM_MAX(depth_m, 0.0f);
    return V3(expf(-EN_WATER_KD.x * d), expf(-EN_WATER_KD.y * d), expf(-EN_WATER_KD.z * d));
}

static f32 cone_of(const en_lamp *L, v3 from_lamp_dir)
{
    f32 c = v3_dot(from_lamp_dir, L->dir);
    return dm_smoothstep(L->cos_outer, L->cos_inner, c);
}

v3 en_lamp_irradiance(const en_lamp *L, v3 p, v3 n)
{
    v3  l  = v3_sub(L->pos, p);
    f32 r2 = v3_dot(l, l) + 1e-4f;
    f32 r  = sqrtf(r2);
    v3  ld = v3_scl(l, 1.0f / r);
    f32 lam = dm_sat(v3_dot(n, ld));
    if (lam <= 0.0f) return V3(0, 0, 0);
    f32 cone = cone_of(L, v3_neg(ld));
    if (cone <= 0.0f) return V3(0, 0, 0);
    v3 tr = en_water_trans(r);
    return v3_scl(v3_mul(L->power, tr), lam * cone / r2);
}

/* Henyey-Greenstein. Particles in sea water throw nearly all the light they
 * scatter forward, which is why a lamp pointed away from the camera still
 * shows a beam and one pointed at it shows a blaze. */
static f32 phase_hg(f32 cos_t, f32 g)
{
    f32 d = 1.0f + g * g - 2.0f * g * cos_t;
    return (1.0f - g * g) / (4.0f * DM_PI * d * sqrtf(DM_MAX(d, 1e-6f)));
}

v3 en_water_inscatter(v3 ro, v3 rd, f32 t_end, f32 depth_bias, const en_lamp *lamps, int nlamps,
                      v3 ambient, u32 seed)
{
    v3 acc = V3(0, 0, 0);

    /* Daylight, scattered in along the whole ray. The light at each point is
     * the surface light attenuated to that point's depth, so a ray looking
     * down picks up less and less of it as it goes; that integral has a
     * closed form, and it is used. */
    if (v3_maxc(ambient) > 0.0f) {
        f32 depth0 = DM_MAX(depth_bias - ro.y, 0.0f);
        f32 down   = -rd.y;
        f32 T = DM_MIN(t_end, 4000.0f);
        f32 ch[3];
        const f32 kd[3] = { EN_WATER_KD.x, EN_WATER_KD.y, EN_WATER_KD.z };
        const f32 c [3] = { EN_WATER_EXT.x, EN_WATER_EXT.y, EN_WATER_EXT.z };
        const f32 b [3] = { EN_WATER_SCAT.x, EN_WATER_SCAT.y, EN_WATER_SCAT.z };
        const f32 am[3] = { ambient.x, ambient.y, ambient.z };
        for (int i = 0; i < 3; i++) {
            f32 k = c[i] + kd[i] * down;
            f32 integ = fabsf(k) > 1e-5f ? (1.0f - expf(-k * T)) / k : T;
            ch[i] = am[i] * expf(-kd[i] * depth0) * b[i] * integ * (0.25f * DM_INVPI) * 2.0f;
        }
        acc = V3(ch[0], ch[1], ch[2]);
    }

    for (int li = 0; li < nlamps; li++) {
        const en_lamp *L = &lamps[li];
        /* Beyond this the beam has too little left to see. */
        f32 reach = 70.0f;
        f32 a = 0.0f, bnd = DM_MIN(t_end, 200.0f);

        v3  oc = v3_sub(L->pos, ro);
        f32 delta = v3_dot(oc, rd);
        f32 D = v3_len(v3_sub(oc, v3_scl(rd, delta)));
        if (D > reach) continue;
        a   = DM_MAX(a, delta - reach);
        bnd = DM_MIN(bnd, delta + reach);
        if (bnd <= a) continue;
        D = DM_MAX(D, 0.02f);

        /* Equiangular sampling: spread the samples evenly in the angle the
         * ray subtends at the lamp, which puts them where the light is. */
        f32 th_a = atanf((a - delta) / D);
        f32 th_b = atanf((bnd - delta) / D);
        const int N = 8;
        f32 jit = dm_u32_to_f32(dm_hash_u32(seed ^ (0x51u + (u32)li)));
        v3 sum = V3(0, 0, 0);
        for (int k = 0; k < N; k++) {
            f32 u  = ((f32)k + jit) / (f32)N;
            f32 th = dm_lerp(th_a, th_b, u);
            f32 t  = delta + D * tanf(th);
            f32 pdf_inv = (th_b - th_a) * (D * D + dm_sq(t - delta)) / D;

            v3  x  = v3_add(ro, v3_scl(rd, t));
            v3  l  = v3_sub(x, L->pos);
            f32 r2 = v3_dot(l, l) + 1e-4f;
            f32 r  = sqrtf(r2);
            v3  wi = v3_scl(l, 1.0f / r);
            f32 cone = cone_of(L, wi);
            if (cone <= 0.0f) continue;

            f32 ph = phase_hg(v3_dot(wi, v3_neg(rd)), 0.82f);
            v3  tr = en_water_trans(r + t);
            v3  e  = v3_mul(v3_mul(L->power, tr), EN_WATER_SCAT);
            sum = v3_add(sum, v3_scl(e, ph * cone / r2 * pdf_inv));
        }
        acc = v3_add(acc, v3_scl(sum, 1.0f / (f32)N));
    }
    return acc;
}
