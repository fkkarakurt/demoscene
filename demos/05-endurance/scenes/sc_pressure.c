/* sc_pressure.c -- Act two: October 1915.
 *
 * The sun has been back since July, and by late October there are twenty-two
 * and a half hours of daylight; what is left of night is a band of twilight
 * low in the south. The floes have been working against each other all month.
 * On the 24th the pressure reached her; on the 27th she was abandoned, and her
 * crew moved onto the ice two hundred yards away.
 *
 * Two shots, both from Shackleton's own account. That night, "the electric
 * light gleamed from the stern of the dying Endurance" until a squeeze cut the
 * connection. The next day the ice snapped her shrouds: "the foretop and
 * topgallant-mast came down with a run", and "the main-mast followed
 * immediately, snapping off about 10 ft. above the main deck". Hurley filmed
 * it. Nothing here is choreographed beyond those sentences; the masts fall
 * the way a spar pivoting about its break falls.
 */
#include "scenes.h"
#include "../polar.h"

/* The abandonment position, 27 October 1915. */
#define OCT_LAT  -69.08f
#define OCT_LON  -51.50f

/* She had been turned by the pack since winter; south-west is as good as any. */
#define HEADING  (225.0f * DM_D2R)

static en_sky g_sky;

static void october_sky(int day, f64 hour, const en_ctx *c)
{
    f64 ut = hour - OCT_LON / 15.0;
    f64 jd = en_julian(1915, 10, day, ut) + c->shot_t / 86400.0;
    g_sky.sun        = en_sun_at(jd, OCT_LAT, OCT_LON);
    g_sky.moon       = en_moon_at(jd, OCT_LAT, OCT_LON, &g_sky.moon_illum);
    g_sky.sun_power  = 20.0f;
    g_sky.aurora     = 0.0f;
    g_sky.stars      = 1.0f;
    g_sky.t          = c->t;
    g_sky.alt        = 2.0f;
    en_sky_prepare(&g_sky);
}

/* The ship in the ice that last week: heeled to port, lifted, her bowsprit
 * and jib-boom snapped off in the night and lying at right angles to her,
 * dragged by their chains. */
static void october_ship(polar_view *v, f32 heel_extra, f32 lift)
{
    v->has_ship = 1;
    v->pose.pos  = V3(0.0f, -SHIP_DRAUGHT + 0.45f + lift, 0.0f);
    v->pose.yaw  = HEADING - DM_HALFPI;
    v->pose.heel = (10.0f + heel_extra) * DM_D2R;
    v->pose.trim = -1.2f * DM_D2R;
    en_ship_pose_make(&v->pose);

    v->st = en_ship_state_default();
    v->st.rime = 0.25f;
    v->st.angle[RIG_BOWSPRIT] = 1.35f;
    v->st.axis[RIG_BOWSPRIT]  = v3_norm(V3(0.15f, 1.0f, 0.25f));
    v->st.shift[RIG_BOWSPRIT] = V3(0.0f, -2.6f, 1.2f);

    v->ice.snow_cover = 0.65f;
    v->ice.hull_fn = polar_hull_fn;
}

/* ---- shot three: the light at her stern ----------------------------------------- */

#define SQUEEZE_BAR CUE_SQUEEZE

static void night_setup(polar_view *v, const en_ctx *c, f64 t, void *ud)
{
    (void)ud;
    f32 u = polar_shot_u(c, t);
    f64 tsq = t - song_bar(SQUEEZE_BAR);

    /* The squeeze: a shove of the floes that heels her a further two degrees
     * in half a second and lifts her, and a shudder through the camera. */
    f32 jolt = tsq > 0.0 ? (f32)(1.0 - exp(-tsq / 0.18)) : 0.0f;
    f32 shake = tsq > 0.0 ? (f32)(exp(-tsq / 0.35)) : 0.0f;
    october_ship(v, 2.2f * jolt, 0.12f * jolt);

    /* The light stays on until the squeeze, stutters, and goes out. */
    f32 light = 1.0f;
    if (tsq > 0.0) {
        f32 k = (f32)tsq;
        light = k < 0.45f ? (dm_hash1f(floorf(k * 22.0f), 0x11u) > 0.5f ? 0.9f : 0.05f) * (1.0f - k / 0.45f) : 0.0f;
    }
    v->st.stern_light = light;

    /* The pack is re-baked eight times a second, not every frame -- the
     * ridges rise by millimetres in an eighth of a second -- so the pressure
     * is held for the eighth the bake stands for. */
    {
        f64 tq = floor(t * 8.0) / 8.0, sq = tq - song_bar(SQUEEZE_BAR);
        f32 jq = sq > 0.0 ? (f32)(1.0 - exp(-sq / 0.18)) : 0.0f;
        v->ice.pressure = dm_lerp(0.75f, 1.0f, polar_shot_u(c, tq)) + 0.08f * jq;
    }

    f32 az = 30.0f * DM_D2R;
    f32 dist = dm_lerp(74.0f, 58.0f, u);
    v3 eye = V3(sinf(az) * dist, 2.2f, cosf(az) * dist);
    v3 at  = V3(sinf(HEADING + DM_PI) * 16.0f, 4.0f, cosf(HEADING + DM_PI) * 16.0f);
    f32 s = shake * 0.06f;
    eye = v3_add(eye, V3(s * dm_hash1f((f32)t * 90.0f, 1u) - s * 0.5f, s * dm_hash1f((f32)t * 90.0f, 2u) - s * 0.5f, 0.0f));
    v->v.cam = en_cam_look(eye, at, 0.0f, 50.0f, 2.0f, dist - 12.0f);
    v->air_glow = 2.5e-3f;
}

void shot_pressure_night(dm_fb *fb, f32 *depth, const en_ctx *c)
{
    october_sky(27, 23.6, c);
    /* The pack is moving in this shot, so it is baked afresh as it moves:
     * once round the camera's feet, and once round her. */
    polar_shot s = { &g_sky, night_setup, NULL, V2(sinf(30.0f * DM_D2R) * 56.0f, cosf(30.0f * DM_D2R) * 56.0f),
                     0x7a0000u + (u64)floor(c->t * 8.0), 1, V2(0.0f, 0.0f), 0 };
    polar_render(fb, depth, c, &s);
}

/* ---- shot four: the masts ------------------------------------------------------------ */

#define FORE_BAR CUE_FORE_FALL
#define MAIN_BAR CUE_MAIN_FALL
#define MASTS_DIST 62.0f

/* Astern of her, where the crew's path from the camp came in, looking along
 * her toward the bow, with the afternoon sun across her from the right. */
static f32 masts_az(void) { return 40.0f * DM_D2R; }

static void masts_setup(polar_view *v, const en_ctx *c, f64 t, void *ud)
{
    (void)ud;
    f32 u = polar_shot_u(c, t);
    october_ship(v, 2.2f, 0.12f);
    v->ice.pressure = 1.0f;

    /* The fore topmast and topgallant, about the lower masthead, to port --
     * until they hang in the wreckage against the lower mast. */
    f32 tf = (f32)(t - song_bar(FORE_BAR));
    /* Gravity pulls on the spar as it stands in the world, and she is already
     * heeled twelve degrees to port, so the fall starts from that lean. */
    const f32 HEEL = 12.2f * DM_D2R;
    v->st.angle[RIG_FORE_TOP] = polar_fall_angle(tf, 12.0f, HEEL + 0.03f, 2.45f) - HEEL;
    v->st.axis[RIG_FORE_TOP]  = V3(1.0f, 0.0f, 0.0f);

    /* The mainmast, snapped ten feet above the deck, over the port side and
     * down onto the ice. */
    f32 tm = (f32)(t - song_bar(MAIN_BAR));
    v->st.angle[RIG_MAIN_LOWER] = polar_fall_angle(tm, 21.0f, HEEL + 0.03f, 1.62f) - HEEL;
    v->st.axis[RIG_MAIN_LOWER]  = V3(1.0f, 0.0f, 0.12f);

    /* Low, behind the ridge the crew had to cross to reach her: a mast going
     * over to port comes down across the picture from right to left, and the
     * frame leaves it room to fall into. */
    f32 az = masts_az();
    v2  dir = V2(sinf(az), cosf(az));
    v->ice.ridge_a = v2_add(v2_scl(dir, MASTS_DIST - 30.0f), V2(-dir.y * 30.0f, dir.x * 30.0f));
    v->ice.ridge_b = v2_add(v2_scl(dir, MASTS_DIST - 30.0f), V2(dir.y * 30.0f, -dir.x * 30.0f));
    v->ice.ridge_h = 1.8f;
    v3 eye = V3(dir.x * MASTS_DIST, 2.6f, dir.y * MASTS_DIST);
    /* A hand on the camera: slow drift, and the landing of the mainmast in it. */
    f32 land = tm > CUE_MAIN_LAND ? (f32)exp(-(tm - CUE_MAIN_LAND) / 0.4f) : 0.0f;
    eye = v3_add(eye, V3(0.0f, 0.4f * u + 0.03f * sinf((f32)t * 0.7f) + 0.06f * land * sinf((f32)t * 40.0f), 0.0f));
    v3 port = V3(sinf(HEADING - DM_HALFPI), 0.0f, cosf(HEADING - DM_HALFPI));
    v->v.cam = en_cam_look(eye, v3_add(V3(0.0f, 8.0f, 0.0f), v3_scl(port, 7.0f)), 0.0f, 32.0f, 5.6f, MASTS_DIST);
}

void shot_pressure_masts(dm_fb *fb, f32 *depth, const en_ctx *c)
{
    october_sky(28, 14.5, c);
    f32 az = masts_az();
    polar_shot s = { &g_sky, masts_setup, NULL, V2(sinf(az) * MASTS_DIST * 0.85f, cosf(az) * MASTS_DIST * 0.85f), 0x7b0004u,
                     1, V2(0.0f, 0.0f), 0x7b0005u };
    polar_render(fb, depth, c, &s);
}
