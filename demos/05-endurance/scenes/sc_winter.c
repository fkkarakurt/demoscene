/* sc_winter.c -- Act one: midwinter, 1915.
 *
 * The ship has been held by the pack since January and has drifted with it
 * for five months. The sun went down on the first of May and will not come
 * back until the end of July, so what lights these two shots is the moon of
 * the night of 22 June 1915 -- eleven days old and three quarters lit, a third
 * of the way up the north-western sky at nine in the evening, which is where
 * sky.c puts it -- the starlight, the faint aurora the expedition's notes
 * complained of, and the ship's own light: lamplight at her windows and the
 * two electric lamps Hurley rigged on poles over the side to light the dogs.
 */
#include "scenes.h"
#include "../polar.h"

#include <stdlib.h>

/* 74 degrees south, 47.5 west: the drift position around Midwinter's Day. */
#define WINTER_LAT  -74.0f
#define WINTER_LON  -47.5f
#define WINTER_HOUR  21.0       /* local mean time */

/* The ship heads south-south-west, and sits in the ice at her waterline. */
#define SHIP_HEADING (200.0f * DM_D2R)

static en_sky g_sky;

void scenes_init(void)
{
    en_ship_init();
}

static void winter_sky(const en_ctx *c)
{
    f64 ut = WINTER_HOUR - WINTER_LON / 15.0;
    f64 jd = en_julian(1915, 6, 22, ut) + c->t / 86400.0;
    g_sky.sun        = en_sun_at(jd, WINTER_LAT, WINTER_LON);
    g_sky.moon       = en_moon_at(jd, WINTER_LAT, WINTER_LON, &g_sky.moon_illum);
    g_sky.sun_power  = 20.0f;
    g_sky.aurora     = 0.15f;
    g_sky.stars      = 1.0f;
    g_sky.t          = c->t;
    g_sky.alt        = 2.0f;
    en_sky_prepare(&g_sky);
}

/* ---- Dog Town ----------------------------------------------------------------
 *
 * The dogs lived on the floe in a line of "dogloos" either side of the ship:
 * ice-block kennels roofed and banked with snow. Their number is not recorded;
 * thirteen a side is room for the fifty dogs, a few to a kennel.
 */
static f32 kennels(f32 x, f32 z, void *ud, int *mat, f32 *free_dist)
{
    const polar_view *v = (const polar_view *)ud;
    v3  l  = en_ship_to_local(&v->pose, V3(x, 0.0f, z));
    f32 az = fabsf(l.z) - 10.5f;
    f32 fx = fabsf(l.x + 1.0f) - 16.0f;
    *mat = ICE_SNOW;
    *free_dist = DM_MAX(DM_MAX(fabsf(az) - 1.2f, fx), 0.0f);
    if (fabsf(az) > 1.2f || fx > 0.8f) return -1.0f;

    f32 cell = 2.5f;
    f32 k  = floorf((l.x + 1.0f) / cell + 0.5f);
    f32 cx = k * cell - 1.0f;
    u32 h  = dm_hash2i((i32)k, l.z > 0.0f ? 1 : -1, 0xd06u);
    f32 jz = (dm_u32_to_f32(h) - 0.5f) * 0.4f;
    /* A block hut with its roof snowed over into a dome: a superellipse in
     * plan, so it keeps its corners, and rounded over the top. */
    f32 qx = fabsf(l.x - cx) / 0.80f, qz = fabsf(az - jz) / 0.72f;
    f32 r  = powf(powf(qx, 4.0f) + powf(qz, 4.0f), 0.25f);
    f32 top = 1.05f + 0.15f * dm_u32_to_f32(dm_hash_u32(h ^ 7u));
    f32 dome = top * sqrtf(DM_MAX(0.0f, 1.0f - powf(DM_MIN(r, 1.0f), 3.0f)));
    /* And the drift the wind has banked against its southern side. */
    f32 drift = top * 0.55f * dm_sat(1.0f - (r - 0.6f) / 0.9f) *
                dm_smoothstep(-0.4f, 0.8f, (az - jz) * (l.z > 0.0f ? 1.0f : -1.0f));
    return DM_MAX(dome, drift) + 0.28f;
}

/* ---- the ship as she was that winter ------------------------------------------ */

static void winter_ship(polar_view *v)
{
    v->has_ship = 1;
    v->pose.pos = V3(0.0f, -SHIP_DRAUGHT, 0.0f);
    v->pose.yaw = SHIP_HEADING - DM_HALFPI;
    en_ship_pose_make(&v->pose);

    v->st = en_ship_state_default();
    v->st.rime   = 0.9f;
    v->st.lights = 1.0f;

    v->ice.snow_cover = 1.0f;
    v->ice.hull_fn  = polar_hull_fn;
    v->ice.extra_fn = kennels;
    v->air_glow = 1.5e-4f;
}

/* ---- shot one: the approach ----------------------------------------------------
 *
 * From three hundred metres out and forty up, drifting in over the pack
 * toward her port quarter, with the moon over the camera's right shoulder: the
 * hull a black shape, the rigging picked out white by moonlight and frost, and
 * the lamps on the ice either side of her.
 */
static void wide_setup(polar_view *v, const en_ctx *c, f64 t, void *ud)
{
    (void)ud;
    f32 u = polar_shot_u(c, t);
    f32 e = u * u * (3.0f - 2.0f * u) * 0.35f + u * 0.65f;
    f32 az   = (62.0f - 12.0f * e) * DM_D2R;
    f32 dist = dm_lerp(260.0f, 150.0f, e);
    f32 alt  = dm_lerp(24.0f, 7.0f, e);
    v->v.cam = en_cam_look(V3(sinf(az) * dist, alt, cosf(az) * dist),
                           V3(-2.0f, dm_lerp(7.0f, 9.0f, e), 0.0f), 0.0f, 50.0f, 5.6f, dist);
    winter_ship(v);
}

void shot_winter_wide(dm_fb *fb, f32 *depth, const en_ctx *c)
{
    winter_sky(c);
    polar_shot s = { &g_sky, wide_setup, NULL, V2(60.0f, 60.0f), 0x3a17u, 0, { 0.0f, 0.0f }, 0 };
    polar_render(fb, depth, c, &s);
}

/* ---- shot two: on the ice beside her ----------------------------------------------
 *
 * On her port bow, looking back toward the moon. The tilt starts on the
 * lamplit ice and the hull, and ends with the moon standing just clear of the
 * trucks -- which is where the moon of that night was from here: 32 degrees
 * up, a little west of north.
 */
#define SHIP_CAM_AZ  (163.0f * DM_D2R)

static void ship_setup(polar_view *v, const en_ctx *c, f64 t, void *ud)
{
    (void)ud;
    f32 u = polar_shot_u(c, t);
    f32 e = u * u * (3.0f - 2.0f * u);
    v3  eye  = V3(sinf(SHIP_CAM_AZ) * 42.0f, 1.6f, cosf(SHIP_CAM_AZ) * 42.0f);
    v3  side = V3(cosf(SHIP_CAM_AZ), 0.0f, -sinf(SHIP_CAM_AZ));
    eye = v3_add(eye, v3_scl(side, dm_lerp(-2.5f, 2.5f, u)));
    f32 tilt = dm_lerp(7.0f, 24.0f, e) * DM_D2R;
    v3  look = v3_norm(V3(-eye.x, 0.0f, -eye.z));
    v->v.cam = en_cam_look(eye, v3_add(eye, V3(look.x * cosf(tilt), sinf(tilt), look.z * cosf(tilt))),
                           0.0f, 24.0f, 2.8f, 40.0f);
    winter_ship(v);
}

void shot_winter_ship(dm_fb *fb, f32 *depth, const en_ctx *c)
{
    winter_sky(c);
    polar_shot s = { &g_sky, ship_setup, NULL,
                     V2(sinf(SHIP_CAM_AZ) * 36.0f, cosf(SHIP_CAM_AZ) * 36.0f), 0x3a18u, 0, { 0.0f, 0.0f }, 0 };
    polar_render(fb, depth, c, &s);
}
