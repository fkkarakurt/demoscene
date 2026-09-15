/* sky.c -- the sun, the moon, the air, the stars and the aurora.
 *
 * Nothing in the sky is placed. The sun and the moon are computed from a date,
 * a time and a position, and those are the ship's. In the Weddell Sea in the
 * winter of 1915 that means a sun that set on the first of May and did not
 * come back until the twenty-sixth of July, and a polar night lit by whatever
 * the moon was doing -- which on any given night of that winter this file can
 * say, to a fraction of a degree. The aurora is here too, but faint, because
 * it was: the expedition's own scientific notes call the displays that winter
 * "few and rather poor", and a sky full of green curtains would be a lie told
 * for the sake of a pretty shot.
 *
 * The air is single-scattered, as in LIFTOFF, with one addition that matters
 * more here than anywhere else: ozone. When the sun is below the horizon, the
 * light reaching the zenith has come the long way round, through the ozone
 * layer edge-on, and ozone absorbs orange. That absorption, not Rayleigh
 * scattering, is why a twilight sky overhead is deep blue rather than grey --
 * leave it out and every shot of the polar night turns the colour of dust.
 */
#include "sky.h"

#include <stdlib.h>

#define RE          6371000.0
#define ATMO_TOP     100000.0
#define H_RAY          8000.0
#define H_MIE          1200.0

/* Rayleigh at sea level, per metre, at 680/550/440 nm. */
static const v3 BETA_R = { 5.8e-6f, 13.5e-6f, 33.1e-6f };
/* Aerosol. Clean polar air has very little. */
#define BETA_M  3.0e-6f
/* Ozone absorption, per metre at peak density: the Chappuis band, which takes
 * out orange and green and leaves the blue. */
static const v3 BETA_O = { 0.65e-6f, 1.88e-6f, 0.085e-6f };

/* ---- the sun ----------------------------------------------------------------- */

v3 en_sun_dir(f32 lat_deg, f32 day_of_year, f32 solar_hour)
{
    f32 lat  = lat_deg * DM_D2R;
    /* Declination: how far north of the equator the sun is that day. The
     * offset of 284 days puts the June solstice on day 172. */
    f32 decl = 23.44f * DM_D2R * sinf(DM_TAU * (284.0f + day_of_year) / 365.0f);
    f32 H    = (solar_hour - 12.0f) * 15.0f * DM_D2R;

    f32 sin_el = sinf(lat) * sinf(decl) + cosf(lat) * cosf(decl) * cosf(H);
    f32 el     = asinf(dm_clamp(sin_el, -1.0f, 1.0f));

    /* Azimuth from north toward east. */
    f32 cos_az = (sinf(decl) - sin_el * sinf(lat)) / DM_MAX(cosf(el) * cosf(lat), 1e-6f);
    f32 az     = acosf(dm_clamp(cos_az, -1.0f, 1.0f));
    if (H > 0.0f) az = DM_TAU - az;

    return v3_norm(V3(sinf(az) * cosf(el), sin_el, cosf(az) * cosf(el)));
}

/* ---- the sun and the moon, for a date ------------------------------------ */

f64 en_julian(int year, int month, int day, f64 hour_ut)
{
    if (month <= 2) { year -= 1; month += 12; }
    int A = year / 100;
    int B = 2 - A + A / 4;
    return floor(365.25 * (year + 4716)) + floor(30.6001 * (month + 1)) + day + B - 1524.5
           + hour_ut / 24.0;
}

static f64 wrap360(f64 a) { a = fmod(a, 360.0); return a < 0.0 ? a + 360.0 : a; }

/* Right ascension and declination to a direction in the local east, up,
 * north frame of an observer, through the local sidereal time. */
static v3 radec_to_local(f64 ra, f64 dec, f64 jd, f32 lat_deg, f32 lon_deg)
{
    f64 d    = jd - 2451545.0;
    f64 gmst = wrap360(280.46061837 + 360.98564736629 * d);
    f64 H    = (gmst + lon_deg) * (DM_PI / 180.0) - ra;
    f64 phi  = lat_deg * (DM_PI / 180.0);
    f64 e = -cos(dec) * sin(H);
    f64 n = sin(dec) * cos(phi) - cos(dec) * cos(H) * sin(phi);
    f64 u = sin(dec) * sin(phi) + cos(dec) * cos(H) * cos(phi);
    return v3_norm(V3((f32)e, (f32)u, (f32)n));
}

static void ecl_to_radec(f64 lam, f64 beta, f64 d, f64 *ra, f64 *dec)
{
    f64 eps = (23.439 - 0.0000004 * d) * (DM_PI / 180.0);
    f64 x = cos(beta) * cos(lam);
    f64 y = cos(eps) * cos(beta) * sin(lam) - sin(eps) * sin(beta);
    f64 z = sin(eps) * cos(beta) * sin(lam) + cos(eps) * sin(beta);
    *ra  = atan2(y, x);
    *dec = asin(z);
}

static f64 sun_longitude(f64 d)
{
    f64 L = wrap360(280.460 + 0.9856474 * d);
    f64 g = wrap360(357.528 + 0.9856003 * d) * (DM_PI / 180.0);
    return (L + 1.915 * sin(g) + 0.020 * sin(2.0 * g)) * (DM_PI / 180.0);
}

v3 en_sun_at(f64 jd, f32 lat_deg, f32 lon_deg)
{
    f64 d = jd - 2451545.0, ra, dec;
    ecl_to_radec(sun_longitude(d), 0.0, d, &ra, &dec);
    return radec_to_local(ra, dec, jd, lat_deg, lon_deg);
}

v3 en_moon_at(f64 jd, f32 lat_deg, f32 lon_deg, f32 *illum)
{
    const f64 R = DM_PI / 180.0;
    f64 d  = jd - 2451545.0;
    f64 L  = wrap360(218.316 + 13.176396 * d);
    f64 M  = wrap360(134.963 + 13.064993 * d) * R;
    f64 F  = wrap360(93.272 + 13.229350 * d) * R;
    f64 lam  = (L + 6.289 * sin(M)) * R;
    f64 beta = 5.128 * sin(F) * R;

    f64 ra, dec;
    ecl_to_radec(lam, beta, d, &ra, &dec);
    v3 dir = radec_to_local(ra, dec, jd, lat_deg, lon_deg);

    /* Seen from the surface rather than the centre of the Earth, the moon
     * sits almost a degree lower: it is only sixty Earth radii away. */
    f32 par = 0.95f * DM_D2R * sqrtf(DM_MAX(0.0f, 1.0f - dir.y * dir.y));
    v3 hor = v3_norm(V3(dir.x, 0.0f, dir.z));
    f32 el = asinf(dm_clamp(dir.y, -1.0f, 1.0f)) - par;
    dir = v3_norm(v3_add(v3_scl(hor, cosf(el)), V3(0.0f, sinf(el), 0.0f)));

    if (illum) {
        /* Elongation from the sun, and the lit fraction that follows. */
        f64 ls = sun_longitude(d);
        f64 ce = cos(beta) * cos(lam - ls);
        *illum = (f32)(0.5 * (1.0 - ce));
    }
    return dir;
}
/* ---- geometry ---------------------------------------------------------------- */

static f32 sin_dip(f64 alt)
{
    if (alt <= 0.0) return 0.0f;
    return (f32)(sqrt(alt * (2.0 * RE + alt)) / (RE + alt));
}

/* Is a point at this height, with this local up, in sunlight? The planet's
 * shadow, softened over the width of the sun. */
static f32 sun_lit(f64 alt, v3 up, v3 sun)
{
    f32 e = v3_dot(up, sun);
    f32 d = -sin_dip(alt);
    return dm_smoothstep(d - 0.012f, d + 0.012f, e);
}

/* Air mass toward the sun, in units of the vertical column: Kasten and
 * Young's fit, which is 1 overhead and 38 on the horizon. The 38 is the
 * reddening of every low sun there is. */
static f32 air_mass(f32 mu)
{
    f32 m = dm_clamp(mu, 0.0f, 1.0f);
    f32 z = acosf(m) * (180.0f / DM_PI);
    return 1.0f / (m + 0.50572f * powf(96.07995f - z, -1.6364f));
}

/* Ozone: a tent of density centred at 25 km, 15 km either side. The column
 * above a height, in metres of peak density. */
static f32 ozone_density(f64 h)
{
    return (f32)DM_MAX(0.0, 1.0 - fabs(h - 25000.0) / 15000.0);
}

static f32 ozone_above(f64 h)
{
    f64 a = 10000.0, m = 25000.0, b = 40000.0, full = 15000.0;
    if (h <= a) return (f32)full;
    if (h >= b) return 0.0f;
    if (h <= m) { f64 x = h - a; return (f32)(full - x * x / (2.0 * 15000.0)); }
    f64 x = b - h;
    return (f32)(x * x / (2.0 * 15000.0));
}

v3 en_sun_transmittance(const en_sky *s)
{
    f32 mu = s->sun.y;
    f32 m  = air_mass(mu);
    f32 cr = (f32)H_RAY * m, cm = (f32)H_MIE * m, co = ozone_above(s->alt) * m;
    v3 tr = V3(expf(-(BETA_R.x * cr + BETA_M * cm + BETA_O.x * co)),
               expf(-(BETA_R.y * cr + BETA_M * cm + BETA_O.y * co)),
               expf(-(BETA_R.z * cr + BETA_M * cm + BETA_O.z * co)));
    return v3_scl(tr, sun_lit(s->alt, V3(0, 1, 0), s->sun));
}

/* ---- the air ---------------------------------------------------------------- */

/* One light at a time: the sun, or the moon, which is the sun's light again at
 * two millionths of the strength and from somewhere else in the sky. The
 * transmittance back to the camera is the same for both. */
static v3 atmosphere(const en_sky *s, v3 rd, v3 sun, f32 power, v3 *trans_out)
{
    f64 alt = s->alt;
    f64 a   = RE + alt;
    f64 b   = a * (f64)rd.y;
    f64 ra  = RE + ATMO_TOP;
    f64 disc = b * b + (ra - a) * (ra + a);
    if (disc <= 0.0) { if (trans_out) *trans_out = V3s(1.0f); return V3(0, 0, 0); }
    f64 t_top = -b + sqrt(disc);

    /* The samples crowd toward the camera, where the air is: spaced by the
     * square of their index, with each one weighted by its own share. */
    const int N = 10;
    f32 cos_s = v3_dot(rd, sun);
    f32 ph_r  = 0.0596831f * (1.0f + cos_s * cos_s);
    const f32 g = 0.80f;
    f32 den   = 1.0f + g * g - 2.0f * g * cos_s;
    f32 ph_m  = 0.1193662f * (1.0f - g * g) / (den * sqrtf(DM_MAX(den, 1e-6f)));

    v3  ins = V3(0, 0, 0);
    f32 odr = 0.0f, odm = 0.0f, odo = 0.0f;

    for (int i = 0; i < N; i++) {
        f64 u0 = (f64)i / N, u1 = (f64)(i + 1) / N;
        f64 d0 = t_top * u0 * u0, d1 = t_top * u1 * u1;
        f64 d  = 0.5 * (d0 + d1), seg = d1 - d0;

        f64 px = d * rd.x, py = alt + d * rd.y, pz = d * rd.z;
        f64 rr = sqrt(px * px + (py + RE) * (py + RE) + pz * pz);
        f64 h  = rr - RE;
        if (h < 0.0) h = 0.0;

        f32 rho_r = expf(-(f32)(h / H_RAY));
        f32 rho_m = expf(-(f32)(h / H_MIE));
        f32 rho_o = ozone_density(h);

        /* Half of this segment's own depth, so a sample is attenuated by the
         * air in front of it and not by the air it is itself standing in. */
        f32 hr = rho_r * (f32)seg * 0.5f, hm = rho_m * (f32)seg * 0.5f, ho = rho_o * (f32)seg * 0.5f;

        v3  up = V3((f32)(px / rr), (f32)((py + RE) / rr), (f32)(pz / rr));
        f32 lit = power > 0.0f ? sun_lit(h, up, sun) : 0.0f;
        if (lit > 0.0f) {
            f32 mu = v3_dot(up, sun);
            f32 m  = air_mass(mu);
            f32 cr = rho_r * (f32)H_RAY * m, cm = rho_m * (f32)H_MIE * m;
            f32 co = ozone_above(h) * m;
            v3 tau = V3(BETA_R.x * (cr + odr + hr) + BETA_M * (cm + odm + hm) + BETA_O.x * (co + odo + ho),
                        BETA_R.y * (cr + odr + hr) + BETA_M * (cm + odm + hm) + BETA_O.y * (co + odo + ho),
                        BETA_R.z * (cr + odr + hr) + BETA_M * (cm + odm + hm) + BETA_O.z * (co + odo + ho));
            v3 tr = V3(expf(-tau.x), expf(-tau.y), expf(-tau.z));
            v3 sc = V3(BETA_R.x * rho_r * ph_r + BETA_M * rho_m * ph_m,
                       BETA_R.y * rho_r * ph_r + BETA_M * rho_m * ph_m,
                       BETA_R.z * rho_r * ph_r + BETA_M * rho_m * ph_m);
            ins = v3_add(ins, v3_scl(v3_mul(tr, sc), (f32)seg * lit));
        }
        odr += rho_r * (f32)seg;
        odm += rho_m * (f32)seg;
        odo += rho_o * (f32)seg;
    }

    if (trans_out)
        *trans_out = V3(expf(-(BETA_R.x * odr + BETA_M * odm + BETA_O.x * odo)),
                        expf(-(BETA_R.y * odr + BETA_M * odm + BETA_O.y * odo)),
                        expf(-(BETA_R.z * odr + BETA_M * odm + BETA_O.z * odo)));

    return v3_scl(ins, power);
}

/* The full moon at the top of the atmosphere is about two millionths as bright
 * as the sun, and it fades with phase much faster than its lit fraction does:
 * a half moon gives a tenth of the light of a full one, not a half, because a
 * half moon is lit from the side and its craters are full of shadow. */
static f32 moon_ratio(const en_sky *s)
{
    if (s->moon_illum <= 0.0f) return 0.0f;
    f32 elong = acosf(dm_clamp(1.0f - 2.0f * s->moon_illum, -1.0f, 1.0f)) * (180.0f / DM_PI);
    f32 alpha = 180.0f - elong;
    f32 mag   = 0.026f * alpha + 4.0e-9f * alpha * alpha * alpha * alpha;
    return 2.1e-6f * powf(10.0f, -0.4f * mag);
}

/* Moonlight is sunlight off a grey-brown surface: a little redder. */
static const v3 MOON_TINT = { 1.0f, 0.93f, 0.82f };

v3 en_moonlight(const en_sky *s)
{
    f32 k = moon_ratio(s);
    if (k <= 0.0f || s->moon.y <= -0.01f) return V3(0, 0, 0);
    en_sky tmp = *s;
    tmp.sun = s->moon;
    return v3_mul(v3_scl(en_sun_transmittance(&tmp), s->sun_power * k), MOON_TINT);
}

/* ---- stars ------------------------------------------------------------------- */

/* Stars, in the same units as the sun. A star of magnitude zero delivers
 * about two and a half millionths of a lux, and the sun a hundred and thirty
 * thousand, so a star is a flux of 1.9e-11 suns times ten to the minus 0.4 of
 * its magnitude -- and the image of that flux is a point, spread only by the
 * pixel it falls in. Everything else about how bright the stars look is the
 * exposure, which is how it works in a camera too. */
static v3 stars(const en_sky *s, v3 rd, f32 cone)
{
    v3 col = V3(0, 0, 0);
    /* Two layers: a sparse one of bright stars and a dense one of faint ones,
     * because the number of stars brighter than a magnitude roughly triples
     * with every magnitude, and one grid cannot hold both. Roughly two hundred
     * stars brighter than third magnitude, and five thousand more to 6.5. */
    static const f32 SCALE[2] = { 40.0f, 110.0f };
    static const u32 KEEP[2]  = { 3u, 8u };
    static const f32 MAG0[2]  = { -1.0f, 3.0f };
    static const f32 MAG1[2]  = { 3.0f, 6.5f };

    for (int layer = 0; layer < 2; layer++) {
        f32 S  = SCALE[layer];
        v3  q  = v3_scl(rd, S);
        v3  ip = v3_floor(q);
        v3  fp = v3_sub(q, ip);
        f32 sig_rad = DM_MAX(cone * 0.6f, 0.00008f);
        f32 sig  = sig_rad * S;
        f32 norm = 1.0f / (DM_TAU * sig_rad * sig_rad);
        f32 e0 = powf(10.0f, 0.46f * MAG0[layer]), e1 = powf(10.0f, 0.46f * MAG1[layer]);

        for (int dz = -1; dz <= 1; dz++)
        for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            u32 h = dm_hash3i((i32)ip.x + dx, (i32)ip.y + dy, (i32)ip.z + dz, 0x5eedu + (u32)layer);
            if ((h & 255u) >= KEEP[layer]) continue;
            v3 c = V3((f32)dx + dm_u32_to_f32(dm_hash_u32(h ^ 0x1234u)),
                      (f32)dy + dm_u32_to_f32(dm_hash_u32(h ^ 0x9876u)),
                      (f32)dz + dm_u32_to_f32(dm_hash_u32(h ^ 0x4321u)));
            f32 d2 = v3_len2(v3_sub(c, fp));
            if (d2 > 9.0f * sig * sig) continue;
            /* A magnitude drawn so that faint stars are as much more common
             * than bright ones as they are in the sky. */
            f32 u   = dm_u32_to_f32(dm_hash_u32(h ^ 0xabcdu));
            f32 mag = log10f(e0 + u * (e1 - e0)) / 0.46f;
            f32 flux = 1.9e-11f * powf(10.0f, -0.4f * mag) * s->sun_power;
            f32 tt  = dm_u32_to_f32(dm_hash_u32(h ^ 0x77u));
            v3 tint = v3_lerp(V3(1.25f, 0.95f, 0.70f), V3(0.82f, 0.94f, 1.22f), tt);
            col = v3_add(col, v3_scl(tint, flux * norm * expf(-d2 / (2.0f * sig * sig))));
        }
    }
    return col;
}

/* ---- the aurora ----------------------------------------------------------------
 *
 * Seventy-five degrees south and forty west is about sixty-four degrees of
 * geomagnetic latitude, under the edge of the southern auroral oval, and the
 * oval is centred on the geomagnetic pole -- which from there lies across the
 * geographic one, a little east of due south. So the curtains stand in the
 * southern sky and run roughly east to west, and the twilight at noon is in
 * the north: the two lights of the polar winter come from opposite sides, and
 * a shot looking one way is lit by the other.
 *
 * Each curtain is a sheet a few kilometres thick and hundreds long, hanging
 * between about a hundred and three hundred kilometres up. Oxygen at the
 * bottom of it glows green at 557.7 nm and the thin oxygen above glows red at
 * 630; the rays in it are field lines, so they are vertical, and they are the
 * only fine detail in the whole thing.
 */
#define AUR_BEARING (167.0f * DM_D2R)
#define AUR_CURTAINS 3

static f32 curtain_offset(f32 u, f32 t, int i)
{
    f32 base  = 250000.0f + 110000.0f * (f32)i;
    f32 slow  = 42000.0f * sinf(u / 330000.0f + 0.9f * (f32)i + t * 0.012f);
    f32 folds = 26000.0f * dm_fbm3(V3(u / 110000.0f + (f32)i * 5.3f, t * 0.006f, 0.5f), 3, 2.1f, 0.5f, 0xa11u);
    return base + slow + folds;
}

/* A curtain is thin, so a ray does not wander through it: it crosses the
 * sheet once, at one point and one height, and what it collects is the glow
 * at that point times the length of ray that lies inside the sheet. That
 * length is the sheet's thickness over the sine of the crossing angle, which
 * is the whole reason a fold seen edge-on is the brightest thing in the sky. */
static v3 aurora(const en_sky *s, v3 rd, u32 seed)
{
    (void)seed;
    if (s->aurora <= 0.0f || rd.y < -0.02f) return V3(0, 0, 0);

    v3  toward = V3(sinf(AUR_BEARING), 0.0f, cosf(AUR_BEARING));
    v3  along  = V3(toward.z, 0.0f, -toward.x);
    f32 hv = v3_dot(rd, toward), hu = v3_dot(rd, along);
    if (hv < 0.05f) return V3(0, 0, 0);

    f32 t = (f32)s->t;
    v3 green = V3(0.18f, 1.00f, 0.42f);
    v3 red   = V3(1.00f, 0.10f, 0.22f);
    v3 blue  = V3(0.45f, 0.20f, 1.00f);
    v3 acc = V3(0, 0, 0);

    for (int i = 0; i < AUR_CURTAINS; i++) {
        /* Where along the ray the sheet is: the distance at which the ray's
         * poleward progress equals the curtain's offset at the ray's own
         * along-curtain position. A handful of fixed-point steps. */
        f32 d = curtain_offset(0.0f, t, i) / hv;
        for (int it = 0; it < 3; it++)
            d = curtain_offset(d * hu, t, i) / hv;
        f32 u = d * hu;

        f64 px = d * rd.x, py = s->alt + d * rd.y, pz = d * rd.z;
        f64 h  = sqrt(px * px + (py + RE) * (py + RE) + pz * pz) - RE;
        f32 hk = (f32)(h / 1000.0);
        if (hk < 90.0f || hk > 400.0f) continue;

        f32 du = 2000.0f;
        f32 slope = (curtain_offset(u + du, t, i) - curtain_offset(u - du, t, i)) / (2.0f * du);
        f32 cross = fabsf(hv - hu * slope);
        f32 width = 3500.0f + 2000.0f * (f32)i;
        /* Edge-on, the path is bounded by how much of the glowing height the
         * ray can stay inside, not by the sheet thickness. */
        f32 path = DM_MIN(1.7725f * width / DM_MAX(cross, 1e-3f),
                          120000.0f / DM_MAX(rd.y, 0.02f));

        f32 ray   = dm_ridge3(V3(u / 6500.0f + t * 0.05f, (f32)i * 7.0f, t * 0.015f), 3, 2.3f, 0.55f, 0x5a7u);
        f32 along_fade = dm_smoothstep(-900000.0f, -500000.0f, u) * (1.0f - dm_smoothstep(500000.0f, 900000.0f, u));
        f32 pulse = 0.70f + 0.30f * sinf(t * 0.40f + (f32)i * 2.0f + u / 140000.0f);
        f32 lum   = (0.20f + 1.8f * ray * ray) * pulse * along_fade * (i == 0 ? 1.0f : 0.5f);

        f32 fg = dm_smoothstep(96.0f, 112.0f, hk) * expf(-DM_MAX(hk - 112.0f, 0.0f) / 40.0f);
        f32 fr = expf(-dm_sq((hk - 235.0f) / 75.0f)) * 0.20f;
        f32 fb = expf(-dm_sq((hk - 101.0f) / 5.0f)) * 0.25f;

        v3 e = v3_add(v3_add(v3_scl(green, fg), v3_scl(red, fr)), v3_scl(blue, fb));
        acc = v3_add(acc, v3_scl(e, lum * path));
    }
    /* Calibrated so that aurora = 1 peaks at about ten times the radiance of
     * a moonlit sky: a modest display. The one this act shows is a tenth of
     * that, level with the sky it hangs in. */
    return v3_scl(acc, s->aurora * 1.4e-10f);
}
/* ---- combined ----------------------------------------------------------------- */

static v3 moon_disc(const en_sky *s, v3 dir)
{
    const f32 R = 0.2595f * DM_D2R;
    f32 cd = v3_dot(dir, s->moon);
    if (cd < cosf(R * 1.2f)) return V3(0, 0, 0);
    /* Where on the disc: a point on the sphere facing us, lit by the sun. */
    m3  B  = m3_basis(s->moon);
    f32 ux = v3_dot(dir, B.c[0]) / R, uy = v3_dot(dir, B.c[1]) / R;
    f32 r2 = ux * ux + uy * uy;
    f32 edge = dm_smoothstep(1.02f, 0.98f, sqrtf(r2));
    if (edge <= 0.0f) return V3(0, 0, 0);
    f32 uz = sqrtf(DM_MAX(0.0f, 1.0f - r2));
    v3  n  = v3_norm(v3_sub(v3_add(v3_scl(B.c[0], ux), v3_scl(B.c[1], uy)), v3_scl(B.c[2], uz)));
    f32 lam = dm_sat(v3_dot(n, s->sun));
    /* Maria: the dark basalt plains, as a low-frequency field on the sphere. */
    f32 maria = dm_sat(dm_fbm3(v3_scl(n, 2.2f), 3, 2.0f, 0.5f, 0x300bu) * 1.8f + 0.2f);
    f32 alb = 0.14f - 0.06f * maria;
    return v3_scl(MOON_TINT, s->sun_power * alb * lam * 0.33f * edge);
}

/* The air alone, the sun's light and the moon's, and what of the background
 * survives it. */
static v3 air_direct(const en_sky *s, v3 dir, v3 *trans)
{
    /* Below about twenty degrees under the horizon the sun lights no air the
     * camera can see, and its integral is skipped rather than summed to zero. */
    f32 sp  = s->sun.y > -0.35f ? s->sun_power : 0.0f;
    v3  col = atmosphere(s, dir, s->sun, sp, trans);
    f32 mk = moon_ratio(s);
    if (mk > 0.0f && s->moon.y > -0.05f)
        col = v3_add(col, v3_mul(atmosphere(s, dir, s->moon, s->sun_power * mk, NULL), MOON_TINT));
    return col;
}

/* ---- the air, tabulated -------------------------------------------------------
 *
 * Ten samples of scattering for two lights is a lot to spend on a pixel of sky
 * that varies over degrees, not pixels. So once a frame the air is integrated
 * over a table of directions -- azimuth across, and the square root of the
 * elevation down, which puts most of the rows near the horizon where the
 * colour changes fastest -- and a pixel reads four entries of it.
 */
#define ENV_W 512
#define ENV_H 256

static void env_dir(int i, int j, v3 *d)
{
    f32 az = DM_TAU * ((f32)i + 0.5f) / (f32)ENV_W;
    f32 v  = ((f32)j + 0.5f) / (f32)ENV_H;
    f32 el = v * v * DM_HALFPI;
    *d = V3(sinf(az) * cosf(el), sinf(el), cosf(az) * cosf(el));
}

typedef struct { en_sky *s; } env_job;

static void env_row(int j, int thread, void *ud)
{
    (void)thread;
    en_sky *s = ((env_job *)ud)->s;
    for (int i = 0; i < ENV_W; i++) {
        v3 d, tr;
        env_dir(i, j, &d);
        v3 c = air_direct(s, d, &tr);
        f32 *e = s->env + ((size_t)j * ENV_W + i) * 6;
        e[0] = c.x; e[1] = c.y; e[2] = c.z;
        e[3] = tr.x; e[4] = tr.y; e[5] = tr.z;
    }
}

static void env_build(en_sky *s)
{
    if (!s->env) s->env = (f32 *)malloc(sizeof(f32) * 6 * ENV_W * ENV_H);
    if (!s->env) return;
    /* The table only has to be redone when a light has moved or changed by
     * enough to see: a tenth of a degree, which the sun takes half a second
     * of film to cover, or half a per cent of the moon's phase. */
    if (s->env_valid && v3_dot(s->env_sun, s->sun) > 0.9999985f && v3_dot(s->env_moon, s->moon) > 0.9999985f
        && s->env_power == s->sun_power && fabsf(s->env_illum - s->moon_illum) < 0.005f)
        return;
    s->env_valid = 0;
    env_job j = { s };
    dm_job_for(ENV_H, env_row, &j);
    s->env_sun = s->sun; s->env_moon = s->moon;
    s->env_power = s->sun_power; s->env_illum = s->moon_illum;
    s->env_valid = 1;
}

static v3 air_lookup(const en_sky *s, v3 dir, v3 *trans)
{
    f32 az = atan2f(dir.x, dir.z);
    if (az < 0.0f) az += DM_TAU;
    f32 u = az / DM_TAU * (f32)ENV_W - 0.5f;
    f32 el = asinf(dm_clamp(dir.y, 0.0f, 1.0f));
    f32 v = sqrtf(el / DM_HALFPI) * (f32)ENV_H - 0.5f;
    if (v < 0.0f) v = 0.0f;
    if (v > (f32)ENV_H - 1.001f) v = (f32)ENV_H - 1.001f;
    f32 fu = floorf(u), fv = floorf(v);
    f32 tu = u - fu, tv = v - fv;
    int i0 = ((int)fu % ENV_W + ENV_W) % ENV_W, i1 = (i0 + 1) % ENV_W;
    int j0 = (int)fv, j1 = j0 + 1;
    const f32 *a = s->env + ((size_t)j0 * ENV_W + i0) * 6, *b = s->env + ((size_t)j0 * ENV_W + i1) * 6;
    const f32 *c = s->env + ((size_t)j1 * ENV_W + i0) * 6, *d = s->env + ((size_t)j1 * ENV_W + i1) * 6;
    f32 r[6];
    for (int k = 0; k < 6; k++)
        r[k] = dm_lerp(dm_lerp(a[k], b[k], tu), dm_lerp(c[k], d[k], tu), tv);
    *trans = V3(r[3], r[4], r[5]);
    return V3(r[0], r[1], r[2]);
}

static v3 sky_core(const en_sky *s, v3 rd, f32 cone, u32 seed, int with_stars)
{
    v3 dir = rd;
    if (dir.y < 0.0f) dir = v3_norm(V3(dir.x, 0.0f, dir.z));

    v3 trans;
    v3 col = s->env_valid ? air_lookup(s, dir, &trans) : air_direct(s, dir, &trans);
    f32 mk = moon_ratio(s);

    v3 back = V3(0, 0, 0);
    if (with_stars && s->stars > 0.0f) back = v3_scl(stars(s, dir, cone), s->stars);

    /* The sun itself, when it is up, and the moon. */
    f32 sd = v3_dot(dir, s->sun);
    if (sd > 0.9999f) back = v3_add(back, v3_scl(V3s(14000.0f), s->sun_power * dm_smoothstep(0.99988f, 0.99991f, sd)));
    if (mk > 0.0f && s->moon.y > -0.01f) back = v3_add(back, moon_disc(s, dir));

    back = v3_add(back, aurora(s, dir, seed));
    return v3_add(col, v3_mul(back, trans));
}

v3 en_sky_radiance(const en_sky *s, v3 rd, u32 seed)
{
    return sky_core(s, rd, 0.0004f, seed, 1);
}

void en_sky_prepare(en_sky *s)
{
    static const v3 AX[6] = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };
    env_build(s);
    for (int i = 0; i < 6; i++) s->amb[i] = V3(0, 0, 0);

    v3  aur_acc = V3(0, 0, 0), aur_dir = V3(0, 0, 0);
    const int N = 160;
    for (int j = 0; j < N; j++) {
        /* Fibonacci points over the upper hemisphere. The ground below the
         * horizon is snow, and contributes its own share of the sky back. */
        f32 z   = 1.0f - ((f32)j + 0.5f) / (f32)N;
        f32 r   = sqrtf(DM_MAX(0.0f, 1.0f - z * z));
        f32 phi = (f32)j * 2.39996323f;
        v3  d   = V3(r * cosf(phi), z, r * sinf(phi));

        v3 L = sky_core(s, d, 0.01f, (u32)j * 2654435761u, 0);
        v3 A = aurora(s, d, (u32)j * 97u);
        aur_acc = v3_add(aur_acc, A);
        aur_dir = v3_add(aur_dir, v3_scl(d, dm_luma(A)));

        for (int i = 0; i < 6; i++) {
            f32 c = v3_dot(d, AX[i]);
            if (c > 0.0f) s->amb[i] = v3_add(s->amb[i], v3_scl(L, c));
            /* Light reflected off the snow, from below. */
            if (-c > 0.0f) s->amb[i] = v3_add(s->amb[i], v3_scl(L, -c * d.y * 0.75f));
        }
    }
    /* Each hemisphere sample stands for 2 pi / N steradians. */
    f32 w = DM_TAU / (f32)N;
    for (int i = 0; i < 6; i++) s->amb[i] = v3_scl(s->amb[i], w);
    s->aur_col = v3_scl(aur_acc, w * 0.5f);
    s->aur_dir = v3_len(aur_dir) > 1e-9f ? v3_norm(aur_dir) : V3(0, 1, 0);

    /* The horizon, all the way round, for the haze in front of distant ice. */
    for (int k = 0; k < EN_SKY_RING; k++) {
        f32 a = DM_TAU * (f32)k / (f32)EN_SKY_RING;
        s->ring[k] = sky_core(s, v3_norm(V3(sinf(a), 0.02f, cosf(a))), 0.01f, 0u, 0);
    }
}

v3 en_sky_ambient(const en_sky *s, v3 n)
{
    v3 n2 = v3_mul(n, n);
    v3 c = v3_scl(n.x >= 0.0f ? s->amb[0] : s->amb[1], n2.x);
    c = v3_add(c, v3_scl(n.y >= 0.0f ? s->amb[2] : s->amb[3], n2.y));
    c = v3_add(c, v3_scl(n.z >= 0.0f ? s->amb[4] : s->amb[5], n2.z));
    return v3_scl(c, DM_INVPI);
}

v3 en_air(const en_sky *s, v3 rd, f32 dist, v3 behind)
{
    /* Near the ground the air in front of a far object glows with the colour
     * of the horizon sky behind it, and hides the object by the same share. */
    v3 tr = V3(expf(-(BETA_R.x + BETA_M * 4.0f) * dist),
               expf(-(BETA_R.y + BETA_M * 4.0f) * dist),
               expf(-(BETA_R.z + BETA_M * 4.0f) * dist));
    f32 a = atan2f(rd.x, rd.z);
    if (a < 0.0f) a += DM_TAU;
    f32 u = a / DM_TAU * (f32)EN_SKY_RING;
    int i0 = (int)u % EN_SKY_RING, i1 = (i0 + 1) % EN_SKY_RING;
    v3 hz = v3_lerp(s->ring[i0], s->ring[i1], u - floorf(u));
    return v3_add(v3_mul(behind, tr), v3_mul(hz, v3_sub(V3s(1.0f), tr)));
}
