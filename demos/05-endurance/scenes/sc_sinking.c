/* sc_sinking.c -- Act three: 21 November 1915.
 *
 * A month on the ice. The crew's camp -- Ocean Camp -- was about a mile and a
 * half from the wreck. Just before five in the afternoon, in full daylight and
 * after a cyclone had blown through, someone saw her going. A diary Shackleton
 * quotes in South: "She went down bows first, her stern raised in the air.
 * She then gave one quick dive and the ice closed over her for ever." She was
 * mastless by then; her funnel was still standing.
 *
 * The sun is where it was: 23 degrees up in the west at 16:50, from 68.7 S.
 */
#include "scenes.h"
#include "../polar.h"

#define NOV_LAT  -68.66f
#define NOV_LON  -52.44f
#define HEADING  (150.0f * DM_D2R)

/* Where the camp is, from the ship: a mile and a half, to the east. */
#define CAMP_DIST 2400.0f
#define CAMP_AZ   (98.0f * DM_D2R)

/* The bar she starts to go on, and the moment of the dive. */
#define GOING_BAR CUE_GOING
#define DIVE_SEC  ((f32)CUE_DIVE)

static en_sky g_sky;

static void november_sky(const en_ctx *c)
{
    f64 ut = 16.83 - NOV_LON / 15.0;
    f64 jd = en_julian(1915, 11, 21, ut) + c->shot_t / 86400.0;
    g_sky.sun        = en_sun_at(jd, NOV_LAT, NOV_LON);
    g_sky.moon       = en_moon_at(jd, NOV_LAT, NOV_LON, &g_sky.moon_illum);
    g_sky.sun_power  = 20.0f;
    g_sky.aurora     = 0.0f;
    g_sky.stars      = 0.0f;
    g_sky.t          = c->t;
    g_sky.alt        = 2.0f;
    en_sky_prepare(&g_sky);
}

/* ---- her going --------------------------------------------------------------------
 *
 * The stern comes up about a pivot forward of amidships, at the waterline, as
 * the flooded bow drags her down; then she slides down along her own length.
 */
static void sinking_ship(polar_view *v, f64 t)
{
    f32 ts = (f32)(t - song_bar(GOING_BAR));
    v->has_ship = 1;
    v->st = en_ship_state_default();
    for (int s = 0; s < RIG_COUNT; s++) if (s != RIG_FUNNEL) v->st.gone[s] = 1;
    v->st.wheelhouse = 0.0f;
    v->st.no_boats = 1;
    v->st.rime = 0.1f;

    f32 trim = -2.0f * DM_D2R, heel = 7.0f * DM_D2R, drop = 0.0f, slide = 0.0f;
    if (ts > 0.0f) {
        trim -= 24.0f * DM_D2R * dm_smoothstep(0.0f, DIVE_SEC, ts);
        if (ts > DIVE_SEC - 0.8f) {
            f32 k = ts - (DIVE_SEC - 0.8f);
            trim -= 20.0f * DM_D2R * dm_smoothstep(0.0f, 2.5f, k);
            slide = 0.9f * k * k;           /* metres, along her length   */
        }
        drop = 0.25f * ts;
    }
    v->pose.yaw  = HEADING - DM_HALFPI;
    v->pose.heel = heel;
    v->pose.trim = trim;
    en_ship_pose_make(&v->pose);

    /* Place her so the pivot -- forward of amidships, at the waterline --
     * sits where the waterline was, less what she has settled and slid. */
    v3 pivot_l = V3(6.0f, SHIP_DRAUGHT, 0.0f);
    v3 fwd = en_ship_dir_to_world(&v->pose, V3(1, 0, 0));
    v3 pivot_w = v3_add(V3(0.0f, -0.9f - drop, 0.0f), v3_scl(fwd, slide));
    v->pose.pos = v3_sub(pivot_w, m3_mul_v3(v->pose.rot, pivot_l));

    /* The lead she lies in, and the white water where she went. */
    v2 along = V2(sinf(HEADING), cosf(HEADING));
    f32 close = dm_smoothstep(DIVE_SEC + 2.0f, DIVE_SEC + 9.0f, ts);
    v->ice.lead_a = v2_scl(along, -34.0f);
    v->ice.lead_b = v2_scl(along, 30.0f);
    v->ice.lead_w = dm_lerp(9.0f, 2.5f, close);
    v->foam = ts > DIVE_SEC - 1.0f ? dm_sat((ts - DIVE_SEC + 1.0f) / 1.5f) * (1.0f - 0.7f * close) : 0.0f;
    v->foam_at = v2_scl(along, 12.0f);
    v->foam_radius = 14.0f + 3.0f * DM_MAX(ts - DIVE_SEC, 0.0f);

    v->ice.snow_cover = 0.8f;
    v->ice.pressure = 0.8f;
    v->ice.hull_fn = polar_hull_fn;
}

/* ---- shot five: "She's going, boys!" ----------------------------------------------------
 *
 * From Ocean Camp, a mile and a half off, looking west into the afternoon sun,
 * on the longest lens the expedition could have wished for -- from the
 * lookout Shackleton had built on the camp's highest block of ice -- creeping
 * in as she goes. */
static v2 camp_point(void) { return V2(sinf(CAMP_AZ) * CAMP_DIST, cosf(CAMP_AZ) * CAMP_DIST); }

static void far_setup(polar_view *v, const en_ctx *c, f64 t, void *ud)
{
    (void)ud;
    sinking_ship(v, t);
    f32 u = polar_shot_u(c, t);
    v2  cp = camp_point();
    v3 eye = V3(cp.x, 3.5f, cp.y);
    /* The operator keeps her in the middle of the frame as she goes, a
     * beat behind. */
    f32 ts = (f32)(t - song_bar(GOING_BAR));
    v3 at  = V3(-2.0f + 1.5f * u, 2.5f - 1.5f * dm_smoothstep(DIVE_SEC - 0.5f, DIVE_SEC + 2.5f, ts), 0.0f);
    f32 wob = 0.00004f;
    at = v3_add(at, V3(0.0f, CAMP_DIST * wob * sinf((f32)t * 1.3f), CAMP_DIST * wob * sinf((f32)t * 0.9f + 1.0f)));
    f32 zoom = dm_smoothstep(0.0f, 0.75f, u);
    v->v.cam = en_cam_look(eye, at, 0.0f, dm_lerp(180.0f, 400.0f, zoom), 11.0f, CAMP_DIST);
}

void shot_sinking_far(dm_fb *fb, f32 *depth, const en_ctx *c)
{
    november_sky(c);
    v2 cp = camp_point();
    /* Round her the pack changes every frame once she starts to go; round the
     * camp it never does. */
    u64 key = c->t < song_bar(GOING_BAR) - 0.2 ? 0x5a0fffu : 0x5a1000u + (u64)c->frame;
    polar_shot s = { &g_sky, far_setup, NULL, V2(0.0f, 0.0f), key, 1,
                     V2(cp.x * 0.95f, cp.y * 0.95f), 0x5a2001u };
    polar_render(fb, depth, c, &s);
}
