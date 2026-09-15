/* sc_deep.c -- the end of act three, and act four: down.
 *
 * Under the ice as she comes through the lead; then following her down,
 * through the depth at which the last red goes, and the last green, and the
 * last of the blue; then nothing but the dark and the snow of dead things
 * falling through it, three thousand and eight metres of it, until the
 * bottom.
 *
 * The camera sinks faster than anything real could -- a hundred and six
 * years and three kilometres go by in thirty seconds -- but the light at each
 * depth is the light at that depth, so the colour of the water is a gauge as
 * honest as the number in the corner.
 */
#include "scenes.h"
#include "../under.h"
#include "../sky.h"

#include <stdlib.h>
#include <string.h>

#define HEADING   (150.0f * DM_D2R)
#define NOV_LAT   -68.66f
#define NOV_LON   -52.44f

f32 g_en_ev_offset = 0.0f;

static v3 november_sun(const en_ctx *c)
{
    f64 ut = 16.83 - NOV_LON / 15.0;
    f64 jd = en_julian(1915, 11, 21, ut) + c->t / 86400.0;
    return en_sun_at(jd, NOV_LAT, NOV_LON);
}

/* The pack she went down through: the same lead as from the camp. */
static void lead_ice(under_view *v)
{
    v2 along = V2(sinf(HEADING), cosf(HEADING));
    v->has_ice = 1;
    v->ice.snow_cover = 0.8f;
    v->ice.pressure = 0.8f;
    v->ice.lead_a = v2_scl(along, -34.0f);
    v->ice.lead_b = v2_scl(along, 30.0f);
    v->ice.lead_w = 7.0f;
}

/* Her, going down: bow first and steep, with nothing left above deck but the
 * funnel's stump and the stumps of her masts. */
static void falling_ship(under_view *v, v3 centre, f32 pitch)
{
    v->has_ship = 1;
    v->st = en_ship_state_default();
    for (int s = 0; s < RIG_COUNT; s++) v->st.gone[s] = 1;
    v->st.wheelhouse = 0.0f;
    v->st.no_boats = 1;
    v->pose.yaw  = HEADING - DM_HALFPI;
    v->pose.trim = pitch;
    v->pose.heel = 6.0f * DM_D2R;
    en_ship_pose_make(&v->pose);
    v3 mid = V3(0.0f, 3.0f, 0.0f);
    v->pose.pos = v3_sub(centre, m3_mul_v3(v->pose.rot, mid));
}

/* ---- shot six: under the ice ------------------------------------------------------
 *
 * Nine metres down, beneath the floe beside the lead, looking up the shafts of
 * light as her bow comes through. */
#define UNDER_START_BAR CUE_UNDER

static void under_setup(under_view *v, const en_ctx *c, f64 t, void *ud)
{
    (void)ud;
    under_view_daylight(v, november_sun(c), 20.0f);
    lead_ice(v);
    v->god_rays = 2.5f;

    f32 ts = (f32)(t - song_bar(UNDER_START_BAR));
    v2  along = V2(sinf(HEADING), cosf(HEADING));
    v2  side  = V2(along.y, -along.x);
    /* Through the lead and down past the camera, gathering speed toward the
     * few metres a second a waterlogged hull sinks at. She comes into the
     * water already standing on her bow, so the stern is still above the ice
     * when the shot opens. */
    f32 fall = 2.6f * ts + 0.30f * ts * ts;
    f32 run  = 4.0f + 0.9f * ts;
    v3  centre = V3(along.x * run, 12.0f - fall, along.y * run);
    falling_ship(v, centre, -64.0f * DM_D2R + 0.03f * ts);

    /* Thirty metres down, ahead of where her bow will go, looking up the
     * shafts of light into the slot of open water -- steeply enough to see the
     * sky through it, which from any lower angle the surface reflects away --
     * and tilting down with her as she comes. */
    v3 eye = V3(side.x * 20.0f + along.x * 28.0f, -42.0f, side.y * 20.0f + along.y * 28.0f);
    f32 follow = dm_smoothstep(1.0f, 7.5f, ts);
    v3 at  = v3_lerp(V3(along.x * 3.0f, -2.0f, along.y * 3.0f), v3_add(centre, V3(0.0f, -10.0f, 0.0f)), 0.1f + 0.6f * follow);
    v->v.cam = en_cam_look(eye, at, 0.0f, 28.0f, 2.8f, 45.0f);}

void shot_sinking_under(dm_fb *fb, f32 *depth, const en_ctx *c)
{
    g_en_ev_offset = 0.0f;
    under_shot s = { under_setup, NULL, 6.0f, 0.02f, 1, V2(0.0f, 0.0f), 0x0dee01u };
    under_render(fb, depth, c, &s);
}

/* ---- the depth, as the counter reads it --------------------------------------------
 *
 * From the surface at the start of act four to 3,008 metres at its end,
 * slowly at first and then fast, and slowly again to arrive. */
f32 en_deep_depth(f64 t)
{
    f64 a = song_section_start(SEC_DEEP), b = song_section_end(SEC_DEEP);
    f32 u = dm_clamp((f32)((t - a) / (b - a)), 0.0f, 1.0f);
    /* An ease that spends its time near the top, where the colour changes. */
    f32 e = u * u * (3.0f - 2.0f * u);
    e = powf(e, 1.6f);
    return 12.0f + (3008.0f - 12.0f) * e;
}

/* ---- the fall ---------------------------------------------------------------------
 *
 * From here to the bottom the scene is the few hundred metres of water round
 * her, which she sinks through at the speed a hull sinks, and the light in it
 * is the light of the depth on the gauge. */
#define FALL_SPEED 4.5f

static f32 fall_time(f64 t) { return (f32)(t - song_section_start(SEC_DEEP)); }

/* Where she is: bow down, gliding a little forward along her own length as
 * a sinking hull does, and turning slowly as she goes. */
static void fall_pose(f64 t, en_ship_pose *P, v3 *centre)
{
    f32 s = fall_time(t);
    P->yaw  = HEADING - DM_HALFPI + 0.015f * s;
    P->trim = (-56.0f + 4.0f * sinf(s * 0.19f)) * DM_D2R;
    P->heel = (6.0f + 5.0f * sinf(s * 0.11f + 1.0f)) * DM_D2R;
    en_ship_pose_make(P);
    v2 ahead = V2(sinf(HEADING), cosf(HEADING));
    *centre = V3(ahead.x * 1.4f * s, -FALL_SPEED * s, ahead.y * 1.4f * s);
    P->pos = v3_sub(*centre, m3_mul_v3(P->rot, V3(0.0f, 3.0f, 0.0f)));
}

static v3 fall_velocity(f64 t)
{
    en_ship_pose P;
    v3 a, b;
    fall_pose(t - 0.05, &P, &a);
    fall_pose(t + 0.05, &P, &b);
    return v3_scl(v3_sub(b, a), 10.0f);
}

static void fall_ship(under_view *v, f64 t)
{
    v3 centre;
    v->has_ship = 1;
    v->st = en_ship_state_default();
    for (int s = 0; s < RIG_COUNT; s++) v->st.gone[s] = 1;
    v->st.wheelhouse = 0.0f;
    v->st.no_boats = 1;
    fall_pose(t, &v->pose, &centre);
    /* The gauge reads her depth: y = 0 is as deep as her middle less how far
     * the scene has sunk. */
    v->depth_bias = en_deep_depth(t) + centre.y;
}

/* ---- bioluminescence ----------------------------------------------------------------
 *
 * Nothing makes light down here but animals: siphonophores, ctenophores,
 * krill, the larvaceans' houses, which glow blue-green, around 480 nm, when
 * something touches them. A hull falling through them touches them all. So the
 * last sight of her is her leading edges drawn in sparks that stream back
 * over her and are left glowing in the water behind.
 *
 * The sparks start as points scattered over her surface, found once by
 * projecting random points onto the distance field. */
#define SPARK_MAX 16000
static v3  g_spark_p[SPARK_MAX], g_spark_n[SPARK_MAX];
static int g_nspark = 0;

static void sparks_init(void)
{
    if (g_nspark) return;
    en_ship_state st = en_ship_state_default();
    for (int s = 0; s < RIG_COUNT; s++) st.gone[s] = 1;
    st.wheelhouse = 0.0f;
    st.no_boats = 1;
    en_ship_state_prepare(&st);
    u32 h = 0x5bacu;
    for (int tries = 0; tries < 800000 && g_nspark < SPARK_MAX; tries++) {
        h = dm_hash_u32(h + 0x9e3779b9u);
        f32 r0 = dm_u32_to_f32(h), r1 = dm_u32_to_f32(dm_hash_u32(h ^ 1u)), r2 = dm_u32_to_f32(dm_hash_u32(h ^ 2u));
        v3 p = V3(dm_lerp(-24.0f, 25.0f, r0), dm_lerp(-0.2f, 8.5f, r1), dm_lerp(-4.2f, 4.2f, r2));
        f32 d = en_ship_sdf(p, &st, NULL);
        if (fabsf(d) > 0.6f) continue;
        for (int k = 0; k < 3; k++) {
            p = v3_sub(p, v3_scl(en_ship_normal(p, &st), d));
            d = en_ship_sdf(p, &st, NULL);
        }
        if (fabsf(d) > 0.015f) continue;
        g_spark_p[g_nspark] = p;
        g_spark_n[g_nspark] = en_ship_normal(p, &st);
        g_nspark++;
    }
}

typedef void (*cam_fn)(f64 t, en_cam *cam);

static void hull_sparks(dm_fb *fb, f32 *depth, const en_ctx *c, cam_fn camera, f32 fade)
{
    if (fade <= 0.0f) return;
    sparks_init();
    const v3  BIO  = V3(0.06f, 0.55f, 1.0f);
    const f32 SLOT = 0.7f;
    int n = c->spp;
    for (int k = 0; k < n; k++) {
        f64 t = en_slice_time(c, k, n);
        en_cam cam;
        camera(t, &cam);
        en_ship_pose P;
        v3 centre;
        fall_pose(t, &P, &centre);
        v3 vel  = fall_velocity(t);
        f32 speed = v3_len(vel);
        v3 vloc = v3_scl(en_ship_dir_to_local(&P, vel), 1.0f / DM_MAX(speed, 1e-4f));

        for (int i = 0; i < g_nspark; i++) {
            v3  nl = g_spark_n[i];
            /* Only what she is falling into is touched. */
            f32 lead = dm_sat(v3_dot(nl, vloc) * 1.6f - 0.15f);
            if (lead <= 0.0f) continue;
            u32 hi = dm_hash_u32((u32)i * 0x9e3779b1u ^ 0xb10u);
            f32 phase = dm_u32_to_f32(hi);
            f32 sl = fall_time(t) / SLOT + phase;
            i32 slot = (i32)floorf(sl);
            for (int back = 0; back < 3; back++) {
                u32 hs = dm_hash2i(i, slot - back, 0x5fa7u);
                if (dm_u32_to_f32(dm_hash_u32(hs ^ 0x77u)) > 0.40f * lead) continue;
                f32 age = (sl - (f32)(slot - back) - dm_u32_to_f32(hs)) * SLOT;
                if (age < 0.0f || age > 1.9f) continue;
                /* Swept back over the hull by the water going past her, at her
                 * own speed, and off the edge into her wake. */
                v3 flow = v3_neg(vloc);
                flow = v3_sub(flow, v3_scl(nl, v3_dot(flow, nl)));
                v3 pl = v3_add(v3_add(g_spark_p[i], v3_scl(nl, 0.06f + 0.05f * age)),
                               v3_scl(flow, speed * age * 0.8f));
                v3 pw = en_ship_to_world(&P, pl);
                f32 b = age < 0.05f ? age / 0.05f : expf(-(age - 0.05f) / 0.40f);
                f32 kind = dm_u32_to_f32(dm_hash_u32(hs ^ 0x99u));
                f32 dist = v3_dist(pw, cam.pos);
                v3 rad = v3_mul(v3_scl(BIO, 0.030f * fade * b * (0.3f + 1.4f * kind * kind)), en_water_trans(dist));
                en_splat(fb, depth, &cam, pw, 0.015f + 0.02f * kind, rad, 1.0f / (f32)n);
            }
        }
    }
}

/* ---- shot seven: following her down -------------------------------------------------
 *
 * Beside her and a little below, sinking with her, looking up at her against
 * what light is left above. */
static void fall_camera(f64 t, en_cam *cam)
{
    en_ship_pose P;
    v3 centre;
    fall_pose(t, &P, &centre);
    f32 s = fall_time(t);
    f32 a = HEADING + DM_HALFPI + 0.5f + 0.012f * s;
    v3 eye = v3_add(centre, V3(sinf(a) * 30.0f, -12.0f, cosf(a) * 30.0f));
    v3 at  = v3_add(centre, V3(0.0f, 3.0f, 0.0f));
    *cam = en_cam_look(eye, at, 0.0f, 32.0f, 2.0f, v3_dist(eye, at));
}

static void fall_setup(under_view *v, const en_ctx *c, f64 t, void *ud)
{
    (void)ud;
    under_view_daylight(v, november_sun(c), 20.0f);
    fall_ship(v, t);
    fall_camera(t, &v->v.cam);
    /* By 350 m a hundred-millionth of the blue is left, and the exposure has
     * stopped opening at nine stops: black. */
    v->dark = en_deep_depth(t) > 350.0f;
}

void shot_deep_fall(dm_fb *fb, f32 *depth, const en_ctx *c)
{
    /* The camera opens up as the light goes, the way an operator would, but
     * not as fast as the light goes: by the end of the shot it has lost. */
    f32 d = en_deep_depth(c->t);
    g_en_ev_offset = DM_MIN(d * 0.030f, 9.0f);
    under_shot s = { fall_setup, NULL, 4.0f, 0.0f, 0, V2(0.0f, 0.0f), 0 };
    under_render(fb, depth, c, &s);
    hull_sparks(fb, depth, c, fall_camera, dm_smoothstep(120.0f, 260.0f, d));
}

/* ---- shot eight: the dark ---------------------------------------------------------------
 *
 * Under her bow, as close as the fall of sparks off it allows, all the way to
 * the bottom. */
static void dark_camera(f64 t, en_cam *cam)
{
    en_ship_pose P;
    v3 centre;
    fall_pose(t, &P, &centre);
    f32 s = fall_time(t);
    v3 bow  = en_ship_to_world(&P, V3(14.0f, 3.5f, 0.0f));
    f32 a = HEADING - DM_HALFPI - 0.9f - 0.01f * s;
    v3 eye = v3_add(bow, V3(sinf(a) * 16.0f, -9.0f, cosf(a) * 16.0f));
    v3 at  = en_ship_to_world(&P, V3(6.0f, 3.5f, 0.0f));
    *cam = en_cam_look(eye, at, 0.0f, 28.0f, 2.0f, v3_dist(eye, at));
}

static void dark_setup(under_view *v, const en_ctx *c, f64 t, void *ud)
{
    (void)ud;
    under_view_daylight(v, november_sun(c), 20.0f);
    fall_ship(v, t);
    dark_camera(t, &v->v.cam);
    v->dark = 1;
}

void shot_deep_dark(dm_fb *fb, f32 *depth, const en_ctx *c)
{
    g_en_ev_offset = 9.0f;
    under_shot s = { dark_setup, NULL, 0.0f, 0.0f, 0, V2(0.0f, 0.0f), 0 };
    under_render(fb, depth, c, &s);
    /* She reaches the bottom as the act ends, and the sparks with her. */
    f32 end = (f32)(song_section_end(SEC_DEEP) - c->t);
    hull_sparks(fb, depth, c, dark_camera, dm_smoothstep(0.2f, 1.4f, end));
}
