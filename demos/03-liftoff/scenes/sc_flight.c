/* sc_flight.c -- the three shots that are not on the ground.
 *
 * One camera rig serves all of them. The world origin is put at the vehicle's
 * engine plane and the camera is placed relative to that, so nothing has to
 * carry an absolute position around; altitude and downrange are passed to the
 * sky separately, which is the only place they matter.
 *
 * The vehicle is rotated by the pitch angle out of the trajectory, so it tips
 * over through the climb because a gravity turn tips it over -- by cut-off it
 * is fifty-seven degrees from vertical and flying almost sideways, which is
 * what an ascent actually looks like and is nothing like what an animated one
 * usually does.
 */
#include "../demo.h"

#define Y_S2  (LO_H_STAGE1 + LO_H_INTER)

/* World to vehicle: the vehicle leans by `pitch` toward downrange. */
static inline v3 to_veh(v3 p, f32 pitch) { return lo_rot_z(p, pitch); }
static inline v3 to_world(v3 p, f32 pitch) { return lo_rot_z(p, -pitch); }

/* How far the two halves have drawn apart. Four and a half metres a second is
 * what a set of pneumatic pushers gives you, and it is the only number in the
 * separation: everything else about that shot follows from it. */
static f32 separation(const lo_ctx *c)
{
    f64 d = c->mt - LO_T_SEP;
    return d <= 0.0 ? 0.0f : (f32)d * 4.5f;
}

/* ---- shading ------------------------------------------------------------- */

static f32 shadow(v3 p, v3 ld, f32 sep)
{
    f32 t = 0.35f, res = 1.0f;
    for (int i = 0; i < 26; i++) {
        f32 h = lo_vehicle_sdf(v3_add(p, v3_scl(ld, t)), sep, 0);
        if (h < 0.004f) return 0.0f;
        f32 s = 10.0f * h / t;
        if (s < res) res = s;
        t += dm_clamp(h, 0.10f, 4.0f);
        if (t > 90.0f) break;
    }
    return dm_sat(res);
}

static f32 occlusion(v3 p, v3 n, f32 sep)
{
    f32 occ = 0.0f, w = 1.0f;
    for (int i = 1; i <= 5; i++) {
        f32 h = 0.16f * (f32)i * (f32)i;
        f32 d = lo_vehicle_sdf(v3_add(p, v3_scl(n, h)), sep, 0);
        occ += (h - d) * w;
        w   *= 0.62f;
    }
    return dm_sat(1.0f - 0.65f * occ);
}

/* The vehicle above the weather. Three sources and no more: the sun, if the
 * vehicle is high enough to see it; the planet below, which is a very large
 * dim blue reflector; and the engines. */
static v3 flight_shade(v3 pv, v3 nv, int part, v3 rdv, const lo_ctx *c,
                       f32 sep, u32 seed)
{
    f32 pitch = c->fl.pitch;
    v3  alb   = lo_vehicle_albedo(pv, part, seed);

    v3  sun_v = to_veh(LO_SUN_DIR, pitch);
    f32 vis   = lo_sun_visible(c->fl.alt);

    f32 lam = dm_sat(v3_dot(nv, sun_v));
    f32 sh  = lam > 0.001f ? shadow(v3_add(pv, v3_scl(nv, 0.02f)), sun_v, sep) : 0.0f;
    v3  col = v3_mul(alb, v3_scl(dm_kelvin(5700.0f), lam * sh * vis * 2.9f));

    /* Earthshine. The normal is asked which way is down in the world, not in
     * the vehicle, because the planet does not tilt when the vehicle does. */
    v3  nw   = to_world(nv, pitch);
    f32 down = dm_sat(-nw.y * 0.5f + 0.5f);
    col = v3_add(col, v3_mul(alb, v3_scl(dm_rgb8(46, 78, 140), down * 0.10f)));
    col = v3_add(col, v3_mul(alb, v3_scl(dm_rgb8(120, 150, 200), 0.022f)));

    /* The engines, from below and close. */
    v3 pl = lo_plume_light(c);
    if (v3_maxc(pl) > 0.0f) {
        v3  lp = V3(0.0f, c->fl.stage == 2 ? (f32)Y_S2 - 1.0f : -2.0f, 0.0f);
        v3  dv = v3_sub(lp, pv);
        f32 d2 = v3_dot(dv, dv) + 6.0f;
        v3  ld = v3_scl(dv, 1.0f / sqrtf(d2));
        col = v3_add(col, v3_mul(alb, v3_scl(pl, dm_sat((v3_dot(nv, ld) + 0.3f) / 1.3f) / d2)));
    }

    /* A rim off the sun, which on something this specular reads as the edge of
     * a metal tank rather than as a glow. */
    f32 rim = powf(1.0f - dm_sat(v3_dot(nv, v3_neg(rdv))), 4.0f);
    col = v3_add(col, v3_scl(dm_kelvin(6200.0f), rim * 0.55f * vis * dm_sat(v3_dot(nv, sun_v) + 0.6f)));

    return v3_scl(col, occlusion(pv, nv, sep));
}

/* ---- the rig ------------------------------------------------------------- */

/* `cam` and `tgt` are offsets from the engine plane. `mounted` says which
 * frame they are given in, and it is a real distinction rather than a
 * convenience: a camera bolted to the vehicle is fixed in the vehicle's frame
 * and swings with it, and a camera watching from outside is fixed in the
 * world's. The climb and the separation are shot on mounted cameras, which is
 * how those two shots exist at all; the closing wide is not. */
static v3 flight_px(v2 uv, const lo_ctx *c, u32 seed,
                    v3 cam, v3 tgt, f32 fov, int mounted)
{
    f32 pitch = c->fl.pitch;
    f32 sep   = separation(c);

    v3 rr, uu, ff;
    v3 rov, rdv, rdw;

    if (mounted) {
        /* Built in the vehicle frame, and rotated out of it only to ask the
         * sky what is there. Note the up vector: `lo_basis` uses the frame's
         * own up, so a mounted camera holds the vehicle level in shot and lets
         * the horizon roll, which is exactly what onboard footage looks like. */
        lo_basis(cam, tgt, &rr, &uu, &ff);
        v3 rdl = lo_ray(uv, fov);
        rov = cam;
        rdv = v3_norm(v3_add(v3_add(v3_scl(rr, rdl.x), v3_scl(uu, rdl.y)),
                             v3_scl(ff, rdl.z)));
        rdw = to_world(rdv, pitch);
    } else {
        lo_basis(cam, tgt, &rr, &uu, &ff);
        v3 rdl = lo_ray(uv, fov);
        rdw = v3_norm(v3_add(v3_add(v3_scl(rr, rdl.x), v3_scl(uu, rdl.y)),
                             v3_scl(ff, rdl.z)));
        rov = to_veh(cam, pitch);
        rdv = to_veh(rdw, pitch);
    }

    /* Bounded, for the same reason the pad shot is: from a hundred metres out
     * the vehicle covers a small part of a frame this tall, and every other
     * pixel would otherwise march ninety-six steps into nothing. The
     * sphere has to grow with `sep`, because after separation the thing being
     * bounded is two objects drifting apart. */
    f32 t = 0.5f;
    int part = 0, hit = 0;
    {
        v3  centre = V3(0.0f, (LO_HEIGHT - sep) * 0.5f, 0.0f);
        f32 radius = (LO_HEIGHT + sep) * 0.5f + 8.0f;
        v3  oc = v3_sub(rov, centre);
        f32 b  = v3_dot(oc, rdv);
        f32 cc = v3_dot(oc, oc) - radius * radius;
        f32 disc = b * b - cc;
        if (disc >= 0.0f) {
            f32 s   = sqrtf(disc);
            f32 t1  = -b + s;
            if (t1 > 0.0f) {
                t = DM_MAX(t, -b - s);
                for (int i = 0; i < 96 && t < t1; i++) {
                    f32 d = lo_vehicle_sdf(v3_add(rov, v3_scl(rdv, t)), sep, &part);
                    if (d < 0.0018f * t) { hit = 1; break; }
                    t += d * 0.94f;
                }
            }
        }
    }

    v3  col;
    f32 t_hit;
    if (hit) {
        v3 p = v3_add(rov, v3_scl(rdv, t));
        v3 n = lo_vehicle_normal(p, sep);
        col   = flight_shade(p, n, part, rdv, c, sep, seed);
        t_hit = t;
    } else {
        col   = lo_world(rdw, c->fl.alt, c->fl.downrange, seed);
        t_hit = 1e5f;
    }

    col = v3_add(col, lo_plume(rov, rdv, DM_MIN(t_hit, 400.0f), c, seed));
    return v3_scl(col, lo_vignette(uv, 0.24f));
}

/* ---- climb ----------------------------------------------------------------
 *
 * Off the side and above, looking back down the body at the exhaust and past
 * it at the planet. The camera holds still relative to the vehicle for the
 * whole shot, so everything that changes in it -- the vehicle tipping over,
 * the plume opening out as the air thins, the sky going from blue to black and
 * the horizon acquiring a curve -- is the flight changing and not the camera.
 */
static v3 climb_px(v2 uv, const lo_ctx *c, u32 seed)
{
    f32 k = (f32)c->sec_phase;
    v3 cam = V3(15.0f, 33.0f, 19.0f + k * 5.0f);
    v3 tgt = V3(0.0f, 4.0f, 0.0f);
    return flight_px(uv, c, seed, cam, tgt, 1.22f + k * 0.16f, 1);
}

/* ---- separation -----------------------------------------------------------
 *
 * From the top of the interstage, looking down at a stage that is no longer
 * attached. It falls away at four and a half metres a second and rolls,
 * because nothing is holding its attitude any more.
 */
static v3 stage_px(v2 uv, const lo_ctx *c, u32 seed)
{
    f32 k = (f32)c->sec_phase;
    v3 cam = V3(13.0f, 61.0f, 16.0f);
    v3 tgt = V3(0.0f, 30.0f - k * 30.0f, 0.0f);
    return flight_px(uv, c, seed, cam, tgt, 1.16f, 1);
}

/* ---- orbit ----------------------------------------------------------------
 *
 * Wide, and pulling further out. The subject is no longer the vehicle: it is
 * the fact that the horizon is bent and that there is a line of daylight along
 * it that was not there thirty seconds ago.
 */
static v3 orbit_px(v2 uv, const lo_ctx *c, u32 seed)
{
    /* Sixty degrees round from the sun, so the daylight comes up along the
     * limb off to one side and the sun itself stays out of the frame -- point
     * a camera at a sun this close and there is nothing else in the shot.
     *
     * The axis is tilted up rather than down, which is what puts the horizon
     * at the very bottom of a frame this tall: from two hundred kilometres the
     * horizon is already fifteen degrees below level, and it only needs to be
     * kept there while the type has the rest of the frame. */
    f32 k = dm_ease_out_cubic((f32)c->sec_phase);
    f32 D = 112.0f + k * 120.0f;
    v3 cam = V3(0.986f * D, 40.0f, 0.143f * D);
    v3 tgt = V3(0.0f, 62.0f, 0.0f);
    return flight_px(uv, c, seed, cam, tgt, 1.34f + k * 0.06f, 0);
}

/* ---- scenes -------------------------------------------------------------- */

void scene_climb(dm_fb *fb, const lo_ctx *c)
{
    lo_shade(fb, c, climb_px, lo_aa);

    /* Maximum dynamic pressure, captioned at the mission time the integration
     * puts it at rather than at one chosen to look good. If the model changes,
     * the caption moves with it. */
    f64 maxq_video = lo_video_time(lo_flight_maxq());
    lo_caption(fb, c, "MAX Q", maxq_video - 0.55, maxq_video + 0.85);
    lo_caption(fb, c, "NOT KEYFRAMED", song_bar(6.5), song_bar(8.4));
}

void scene_stage(dm_fb *fb, const lo_ctx *c)
{
    lo_shade(fb, c, stage_px, lo_aa);

    lo_caption(fb, c, "CUT-OFF", song_bar(9.05), song_bar(9.95));
    lo_caption(fb, c, "SEPARATION", song_bar(10.05), song_bar(11.10));
}

void scene_orbit(dm_fb *fb, const lo_ctx *c)
{
    lo_shade(fb, c, orbit_px, lo_aa);

    f32 W = (f32)fb->w, H = (f32)fb->h;
    f64 t0 = song_bar(SONG_SECTION_BAR[SEC_ORBIT]);

    /* Three lines, a beat apart. Read in order they are the whole claim the
     * channel makes, and they fit on a phone held at arm's length. */
    static const char *LINE[3] = { "NO IMAGES", "NO SAMPLES", "ONLY CODE" };

    for (int i = 0; i < 3; i++) {
        f64 on = t0 + song_beat(0.5 + 0.5 * i);
        if (c->t < on) continue;
        f32 a = dm_sat((f32)((c->t - on) / 0.13));
        f32 e = dm_ease_out_back(dm_sat((f32)((c->t - on) / 0.32)));

        dm_text st = dm_text_default();
        st.size      = W * (0.068f + 0.026f * e);
        st.tracking  = 0.16f;
        st.weight    = st.size * 0.082f;
        st.color     = v3_scl(dm_rgb8(255, 250, 242), 1.45f * a);
        st.glow      = st.size * 0.14f;
        st.glow_gain = 0.38f * a;

        f32 want = W * 0.86f, have = dm_text_width(LINE[i], &st);
        if (have > want) { st.size *= want / have; st.weight = st.size * 0.082f; }

        dm_text_draw_centered(fb, V2(W * 0.5f, H * (0.30f + 0.078f * (f32)i)),
                              LINE[i], &st);
    }

    f64 on = t0 + song_beat(2.6);
    if (c->t >= on) {
        f32 a = dm_sat((f32)((c->t - on) / 0.25));
        dm_text st = dm_text_default();
        st.size      = W * 0.046f;
        st.tracking  = 0.62f;
        st.weight    = st.size * 0.095f;
        st.color     = v3_scl(dm_rgb8(150, 195, 250), 1.10f * a);
        st.glow      = st.size * 0.22f;
        st.glow_gain = 0.28f * a;
        dm_text_draw_centered(fb, V2(W * 0.5f, H * 0.505f), "KORMOS", &st);
    }
}
