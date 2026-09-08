/* world.c -- sky, sun, stars, planet.
 *
 * Everything a ray that misses the vehicle can hit. One function serves a
 * camera standing on the pad and one two hundred kilometres above it, because
 * the only thing that changes between them is a number.
 *
 * The sky is single-scattered air: eight samples along the ray, each one asked
 * how dense the air is there and whether the sun can see it. That second
 * question is the whole reason this piece looks the way it does. The sun is
 * five and a half degrees below the horizon at the pad, so at ground level
 * almost every sample is in the planet's shadow and the sky is nearly black.
 * Climb, and the horizon drops away -- nine degrees at eighty kilometres --
 * until the samples come out into the light and the sky goes orange along the
 * limb. Nothing in this piece animates a sunrise. The vehicle flies into one.
 */
#include "../demo.h"

#define RE        6371000.0
#define H_AIR        8500.0
#define ATMO_TOP   100000.0
#define CLOUD_H      9000.0

/* Rayleigh scattering at sea level, per metre, at 680/550/440 nm. The ratio is
 * the inverse fourth power of wavelength, which is the whole reason the sky is
 * blue and the reason a low sun is red: the blue has already been scattered
 * out of the beam by the time it has crossed that much air. */
static const v3 BETA_R = { 5.8e-6f, 13.5e-6f, 33.1e-6f };
/* Mie, from aerosol. Almost achromatic, strongly forward scattering, and what
 * makes the band just above a sunrise white rather than blue. */
#define BETA_M  21.0e-6f

/* ---- geometry -------------------------------------------------------------
 *
 * The camera sits at (0, alt, 0) with the planet centred at (0, -RE, 0). Every
 * quantity below is written to avoid subtracting two numbers near 6 371 000
 * from each other, which in single precision would leave nothing behind.
 */

/* Sine of the horizon dip: how far below the geometric horizontal the visible
 * horizon has fallen at this altitude. Zero on the ground, 0.157 -- nine
 * degrees -- at eighty kilometres. */
static f32 sin_dip(f64 alt)
{
    if (alt <= 0.0) return 0.0f;
    return (f32)(sqrt(alt * (2.0 * RE + alt)) / (RE + alt));
}

/* Is a point at this altitude, with this local up, in sunlight? Softened over
 * about a degree, which is the sun's own width plus what refraction does to
 * the edge of the shadow. */
static f32 sun_lit(f64 alt, v3 up, v3 sun)
{
    f32 e = v3_dot(up, sun);
    f32 d = -sin_dip(alt);
    return dm_smoothstep(d - 0.016f, d + 0.016f, e);
}

/* Air column toward the sun from a point, in units of the vertical column, via
 * the usual cheap stand-in for the Chapman function. Grows without bound as
 * the sun approaches the horizon, which is exactly the reddening. */
static f32 sun_path(f32 mu)
{
    f32 m = mu < 0.0f ? 0.0f : mu;
    return 1.0f / (m + 0.15f * expf(-11.0f * m) + 0.003f);
}

/* The same test for the vehicle itself, which is on the axis, so its local up
 * is the world up and the whole thing collapses to one comparison. */
f32 lo_sun_visible(f32 alt)
{
    return sun_lit((f64)alt, V3(0.0f, 1.0f, 0.0f), LO_SUN_DIR);
}

/* ---- stars ---------------------------------------------------------------
 *
 * A hash per cell of direction. No catalogue, no texture: the sky has as many
 * stars as the grid has cells, and they hold still because the hash is a
 * function of direction and nothing else. */
static v3 stars(v3 rd)
{
    v3  q  = v3_scl(rd, 180.0f);
    v3  ip = v3_floor(q);
    v3  fp = v3_sub(q, ip);
    v3  col = V3(0, 0, 0);

    for (int dz = -1; dz <= 1; dz++)
    for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++) {
        u32 h = dm_hash3i((i32)ip.x + dx, (i32)ip.y + dy, (i32)ip.z + dz, 0x5eedu);
        if ((h & 255u) > 7u) continue;              /* three per cent of cells */

        v3 c = V3((f32)dx + dm_u32_to_f32(h),
                  (f32)dy + dm_u32_to_f32(dm_hash_u32(h ^ 0x1234u)),
                  (f32)dz + dm_u32_to_f32(dm_hash_u32(h ^ 0x9876u)));
        f32 d = v3_len(v3_sub(c, fp));

        f32 mag = dm_u32_to_f32(dm_hash_u32(h ^ 0xabcdu));
        f32 b   = mag * mag * mag * mag * mag * 2.4f;
        /* Hotter stars are bluer and the bright ones are rarer, which is the
         * one bit of astronomy worth spending two lines on. The ends of the
         * ramp are the blackbody locus at 3 400 K and 10 400 K, worked out
         * once and written down: calling `dm_kelvin` once per star and up to
         * twenty-seven stars per pixel is five transcendentals a time for a
         * colour nobody can name. */
        f32 t   = dm_u32_to_f32(dm_hash_u32(h ^ 0x77u));
        v3 tint = v3_lerp(V3(1.46f, 0.90f, 0.42f), V3(0.72f, 0.92f, 1.44f), t);
        col = v3_add(col, v3_scl(tint, b * expf(-d * d * 60.0f)));
    }
    return col;
}

/* ---- the ground -----------------------------------------------------------
 *
 * Seen from above and, for most of this piece, at night. So it is mostly what
 * a dark planet actually shows: nothing, and then a scatter of light where the
 * people are, which clings to the coasts because that is where they live.
 */
static v3 ground(v3 u, f32 lit, v3 sun_col, u32 seed)
{
    (void)seed;

    /* Continents. One field decides land or sea; the coastline is its contour
     * and nothing has to draw it. */
    f32 c = dm_fbm3(v3_scl(u, 2.6f), 5, 2.1f, 0.55f, 0x1eaf);
    f32 land = dm_smoothstep(0.02f, 0.10f, c);

    v3 sea  = v3_scl(dm_rgb8(10, 26, 58), 0.85f);
    v3 soil = v3_lerp(v3_scl(dm_rgb8(96, 84, 62), 0.75f),
                      v3_scl(dm_rgb8(58, 74, 48), 0.75f),
                      dm_sat(dm_fbm3(v3_scl(u, 9.0f), 3, 2.0f, 0.5f, 0x2bee) * 1.6f + 0.5f));
    v3 alb = v3_lerp(sea, soil, land);

    v3 col = v3_scl(v3_mul(alb, sun_col), lit * 0.62f);

    /* Starlight and airglow. A moonless night side is not black; it is about
     * a thousandth of the day side, and putting that floor in is what keeps
     * the planet reading as a solid object rather than as a hole. */
    col = v3_add(col, v3_scl(alb, 0.010f));

    /* Cities, on the dark side, and clustered near a coast: the field is
     * strongest where `c` is just past the waterline, because that is where
     * people put themselves. */
    f32 coast = expf(-dm_sq((c - 0.10f) * 9.0f));
    f32 pop   = dm_sat(dm_fbm3(v3_scl(u, 26.0f), 3, 2.3f, 0.5f, 0x3cab) * 2.2f + 0.40f);
    f32 grain = 1.0f - dm_worley3(v3_scl(u, 780.0f), 0x4dceu);
    f32 lamps = powf(dm_sat(grain), 4.5f) * coast * pop * land;
    col = v3_add(col, v3_scl(dm_rgb8(255, 190, 118), lamps * 5.0f * (1.0f - lit)));

    return col;
}

/* ---- the world ----------------------------------------------------------- */

v3 lo_world(v3 rd, f32 alt, f32 dr, u32 seed)
{
    const v3 sun = LO_SUN_DIR;

    f64 a  = RE + (f64)alt;
    f64 b  = a * (f64)rd.y;

    /* Written as products of sums rather than differences of squares: at this
     * radius the difference of two squares has no significant figures left. */
    f64 disc_g = b * b - (f64)alt * (2.0 * RE + (f64)alt);
    f64 ra     = RE + ATMO_TOP;
    f64 disc_a = b * b + (ra - a) * (ra + a);

    int hit_ground = (rd.y < 0.0f) && (disc_g > 0.0);
    f64 t_ground   = hit_ground ? -b - sqrt(disc_g) : 0.0;
    f64 t_top      = disc_a > 0.0 ? -b + sqrt(disc_a) : 0.0;
    if (t_top < 0.0) t_top = 0.0;

    f64 t_end = hit_ground && t_ground > 0.0 && t_ground < t_top ? t_ground : t_top;

    /* --- what is behind the air ------------------------------------------ */

    v3 back;
    if (hit_ground && t_ground > 0.0) {
        v3 p = V3((f32)(t_ground * rd.x),
                  (f32)((f64)alt + t_ground * rd.y),
                  (f32)(t_ground * rd.z));
        /* Local up at the hit, and the same vector used to look up the
         * surface: on a sphere those are the same thing. */
        v3 u = v3_norm(V3(p.x, p.y + (f32)RE, p.z));
        /* Downrange rolls the planet under the camera. The vehicle covers
         * fifteen hundred kilometres by cut-off and the ground has to show it. */
        u = lo_rot_x(u, dr / (f32)RE);

        f32 lit = sun_lit(0.0, u, sun);
        f32 lam = dm_sat(v3_dot(u, sun));
        v3  sc  = dm_kelvin(5600.0f);
        back = ground(u, lit * lam, sc, seed);

        /* The cloud deck, as its own shell. A ray that reaches the ground has
         * already crossed it. */
        f64 rc     = RE + CLOUD_H;
        f64 disc_c = b * b + (rc - a) * (rc + a);
        if (disc_c > 0.0) {
            f64 tc = -b - sqrt(disc_c);
            if (tc < 0.0) tc = -b + sqrt(disc_c);
            if (tc > 0.0 && tc < t_ground) {
                v3 pc = V3((f32)(tc * rd.x), (f32)((f64)alt + tc * rd.y), (f32)(tc * rd.z));
                v3 uc = lo_rot_x(v3_norm(V3(pc.x, pc.y + (f32)RE, pc.z)), dr / (f32)RE);
                f32 d = dm_fbm3(v3_scl(uc, 14.0f), 5, 2.3f, 0.55f, 0x77a1);
                f32 cover = dm_sat(d * 2.4f + 0.30f);
                f32 clit  = sun_lit(CLOUD_H, uc, sun) * (0.25f + 0.75f * dm_sat(v3_dot(uc, sun)));
                v3  ccol  = v3_scl(dm_kelvin(5800.0f), clit * 0.46f);
                back = v3_lerp(back, ccol, cover * 0.92f);
            }
        }
    } else {
        back = stars(rd);

        /* The sun, a quarter of a degree across and far brighter than anything
         * the tone mapper will let through. */
        f32 sd = v3_dot(rd, sun);
        if (sd > 0.99999f) back = v3_add(back, V3s(240.0f));
        back = v3_add(back, v3_scl(dm_kelvin(5700.0f),
                                   powf(dm_sat(sd), 2200.0f) * 30.0f));
    }

    /* --- the air ---------------------------------------------------------
     *
     * The slab of air the ray actually crosses, which is not the same as the
     * air near the camera. From two hundred kilometres up the camera is above
     * all of it and the only part that matters is the few hundred kilometres
     * of ray that grazes the limb -- so the integral is taken between where
     * the ray enters the shell and where it leaves or hits the ground, not
     * from the camera. That one distinction is the difference between a
     * planet with an atmosphere on it and a flat disc. */
    if (disc_a <= 0.0) return back;

    f64 s_a = sqrt(disc_a);
    f64 t_a = -b - s_a;
    f64 t_b = -b + s_a;
    if (t_a < 0.0) t_a = 0.0;
    if (t_end > 0.0 && t_end < t_b) t_b = t_end;
    if (t_b <= t_a) return back;

    const int N = 8;
    f64 seg = (t_b - t_a) / (f64)N;

    v3  inscat = V3(0, 0, 0);
    f32 tau_r  = 0.0f, tau_m = 0.0f;

    f32 cos_s = v3_dot(rd, sun);
    /* Rayleigh is symmetric front to back; Mie is not, and the difference is
     * why the sky near a low sun is bright and the sky away from it is not. */
    f32 ph_r  = 0.0596831f * (1.0f + cos_s * cos_s);
    f32 g     = 0.76f;
    f32 den   = 1.0f + g * g - 2.0f * g * cos_s;
    f32 ph_m  = 0.1193662f * (1.0f - g * g) / (den * sqrtf(DM_MAX(den, 1e-6f)));

    for (int i = 0; i < N; i++) {
        f64 d = t_a + ((f64)i + 0.5) * seg;
        f64 px = d * rd.x, py = (f64)alt + d * rd.y, pz = d * rd.z;
        f64 rr = sqrt(px * px + (py + RE) * (py + RE) + pz * pz);
        f64 h  = rr - RE;
        if (h < 0.0) h = 0.0;

        f32 rho = expf(-(f32)(h / H_AIR));
        v3  up  = V3((f32)(px / rr), (f32)((py + RE) / rr), (f32)(pz / rr));

        f32 lit = sun_lit(h, up, sun);
        f32 mu  = v3_dot(up, sun);

        /* Extinction between the sample and the sun, and between the sample
         * and the camera. Both are the same integral done cheaply. */
        f32 col_sun = rho * (f32)H_AIR * sun_path(mu);
        v3  t_sun   = V3(expf(-(BETA_R.x + BETA_M) * col_sun),
                         expf(-(BETA_R.y + BETA_M) * col_sun),
                         expf(-(BETA_R.z + BETA_M) * col_sun));
        v3  t_view  = V3(expf(-tau_r * BETA_R.x - tau_m * BETA_M),
                         expf(-tau_r * BETA_R.y - tau_m * BETA_M),
                         expf(-tau_r * BETA_R.z - tau_m * BETA_M));

        f32 ds = (f32)seg * rho;
        v3  contrib = v3_mul(t_sun, t_view);
        contrib = v3_mul(contrib,
                         V3(BETA_R.x * ph_r + BETA_M * ph_m,
                            BETA_R.y * ph_r + BETA_M * ph_m,
                            BETA_R.z * ph_r + BETA_M * ph_m));
        inscat = v3_add(inscat, v3_scl(contrib, ds * lit * 17.0f));

        tau_r += ds;
        tau_m += ds;
    }

    v3 trans = V3(expf(-tau_r * BETA_R.x - tau_m * BETA_M),
                  expf(-tau_r * BETA_R.y - tau_m * BETA_M),
                  expf(-tau_r * BETA_R.z - tau_m * BETA_M));

    /* Airglow: the thin green line the crews photograph, from oxygen at about
     * ninety kilometres. It costs two lines and it is the single detail that
     * says "this is a real limb" to anyone who has seen one. */
    if (alt > 30000.0f) {
        f32 lim = expf(-dm_sq((rd.y + sin_dip(alt)) * 26.0f));
        inscat = v3_add(inscat, v3_scl(dm_rgb8(90, 255, 170),
                                       lim * 0.030f * dm_smoothstep(30000.0f, 80000.0f, alt)));
    }

    return v3_add(v3_mul(back, trans), inscat);
}
