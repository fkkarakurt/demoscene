/* sc_pad.c -- the count, and the release.
 *
 * Two shots on the ground. The first is a fixed camera fifty metres from the
 * base looking up the side of a floodlit vehicle that is doing nothing; the
 * second is the tracking camera, further back and on a long lens, from the
 * release until the vehicle is a kilometre and a half up.
 *
 * The light in these two shots is doing all of the work and it is worth being
 * explicit about where it comes from. Before ignition there are six xenon
 * floodlights on the pad and nothing else, because the sun is five and a half
 * degrees below the horizon. Three seconds before the release the engines light and
 * become, by a very wide margin, the brightest thing that has been on screen:
 * one source, at the engine plane, falling off with the square of the distance
 * like any other. The vehicle, the tower, the deck and every parcel of smoke
 * are all lit by that same source. Nothing is faded up by hand.
 */
#include "../demo.h"

#define PAD_H     8.5f      /* engine plane above the deck, metres */
#define TOWER_X  17.5f
#define TOWER_H  78.0f

/* ---- primitives ---------------------------------------------------------- */

static f32 sd_box(v3 p, v3 b)
{
    v3  q = v3_sub(v3_abs(p), b);
    v3  m = v3_max(q, V3(0, 0, 0));
    return v3_len(m) + DM_MIN(v3_maxc(q), 0.0f);
}

/* ---- the tower ------------------------------------------------------------
 *
 * Four columns, a horizontal frame every six metres and one access arm. The
 * frame is one box drawn through a repetition of the vertical axis, which is
 * how a lattice should be built: the repeat is what makes it a tower, and
 * spelling out thirteen storeys would be spelling out the same box thirteen
 * times.
 */
static f32 tower(v3 p)
{
    v3 q = v3_sub(p, V3(TOWER_X, 0.0f, 0.0f));

    v3  fold = V3(fabsf(q.x) - 3.7f, q.y - TOWER_H * 0.5f, fabsf(q.z) - 3.7f);
    f32 d    = sd_box(fold, V3(0.34f, TOWER_H * 0.5f, 0.34f));

    /* The frame. Clipped to the height of the tower after the repeat, or the
     * storeys would carry on into the sky and into the ground. */
    v3 r = q;
    r.y = dm_mod(q.y + 3.0f, 6.0f) - 3.0f;
    f32 beam = DM_MIN(sd_box(V3(r.x, r.y, fabsf(r.z) - 3.7f), V3(4.0f, 0.20f, 0.20f)),
                      sd_box(V3(fabsf(r.x) - 3.7f, r.y, r.z), V3(0.20f, 0.20f, 4.0f)));
    beam = DM_MAX(beam, sd_box(V3(q.x, q.y - TOWER_H * 0.5f, q.z),
                               V3(9.0f, TOWER_H * 0.5f - 2.0f, 9.0f)));
    d = DM_MIN(d, beam);

    /* The access arm, reaching across to the second stage hatch. */
    f32 arm = sd_box(V3(q.x + 6.5f, q.y - 52.0f, q.z), V3(7.0f, 0.85f, 1.5f));
    d = DM_MIN(d, arm);

    /* The lightning mast. Everything on a pad is grounded to one of these. */
    f32 mast = sd_box(V3(q.x, q.y - (TOWER_H + 13.0f), q.z), V3(0.16f, 13.0f, 0.16f));
    d = DM_MIN(d, mast);

    /* The launch mount the vehicle is standing on, and the deflector under it. */
    f32 mount = sd_box(v3_sub(p, V3(0.0f, PAD_H * 0.5f, 0.0f)),
                       V3(4.2f, PAD_H * 0.5f, 4.2f));
    mount = DM_MAX(mount, -sd_box(v3_sub(p, V3(0.0f, PAD_H * 0.5f, 0.0f)),
                                  V3(2.6f, PAD_H * 0.5f + 1.0f, 2.6f)));
    d = DM_MIN(d, mount);

    return d;
}

/* ---- the scene ----------------------------------------------------------- */

/* World to vehicle: the engine plane sits on the launch mount, rises by
 * whatever the trajectory says, moves downrange by the same, and leans by the
 * pitch angle. Points get all three; directions get only the rotation, and
 * keeping those two apart matters -- feeding a world direction to a rotated
 * frame puts the exhaust a hundred metres to one side of the engines that are
 * producing it, which is exactly what it did before this. */
static v3 to_vehicle(v3 p, const lo_ctx *c)
{
    v3 q = V3(p.x - c->fl.downrange, p.y - (PAD_H + c->fl.alt), p.z);
    return lo_rot_z(q, c->fl.pitch);
}

static v3 to_vehicle_dir(v3 d, const lo_ctx *c)
{
    return lo_rot_z(d, c->fl.pitch);
}

static f32 map(v3 p, const lo_ctx *c, int *part)
{
    f32 dv = lo_vehicle_sdf(to_vehicle(p, c), 0.0f, part);
    f32 dt = tower(p);
    if (dt < dv) { if (part) *part = -1; return dt; }
    return dv;
}

static v3 normal_at(v3 p, const lo_ctx *c)
{
    const f32 e = 0.012f;
    f32 dx = map(V3(p.x + e, p.y, p.z), c, 0) - map(V3(p.x - e, p.y, p.z), c, 0);
    f32 dy = map(V3(p.x, p.y + e, p.z), c, 0) - map(V3(p.x, p.y - e, p.z), c, 0);
    f32 dz = map(V3(p.x, p.y, p.z + e), c, 0) - map(V3(p.x, p.y, p.z - e), c, 0);
    return v3_norm(V3(dx, dy, dz));
}

static f32 ambient_occlusion(v3 p, v3 n, const lo_ctx *c)
{
    f32 occ = 0.0f, w = 1.0f;
    for (int i = 1; i <= 5; i++) {
        f32 h = 0.22f * (f32)i * (f32)i;
        f32 d = map(v3_add(p, v3_scl(n, h)), c, 0);
        occ += (h - d) * w;
        w   *= 0.62f;
    }
    return dm_sat(1.0f - 0.55f * occ);
}

/* ---- light ----------------------------------------------------------------
 *
 * Six xenon floodlights in a ring around the base, aimed up the vehicle, and
 * after ignition one very much larger source at the engine plane. Both fall
 * off with the square of the distance because that is what light does, and
 * that single fact is what makes the release read: the plume does not get
 * brighter, the vehicle simply gets closer to it and then further from it.
 */
static v3 lighting(v3 p, v3 n, v3 alb, const lo_ctx *c)
{
    v3 lit = V3(0, 0, 0);

    const f32 FLOOD_R = 27.0f;
    v3 flood_col = dm_kelvin(6200.0f);
    for (int i = 0; i < 6; i++) {
        f32 a = DM_TAU * ((f32)i + 0.25f) / 6.0f;
        v3  lp = V3(cosf(a) * FLOOD_R, 7.0f, sinf(a) * FLOOD_R);
        v3  dv = v3_sub(lp, p);
        f32 d2 = v3_dot(dv, dv) + 1.0f;
        v3  ld = v3_scl(dv, 1.0f / sqrtf(d2));
        f32 lam = dm_sat(v3_dot(n, ld));
        /* Aimed up the vehicle, not spread. `ld` points at the light, so the
         * beam direction is the other way, and the cone is how far up it is
         * pointing: that is what stops the deck being as bright as the tank. */
        f32 cone = powf(dm_sat(-ld.y + 0.32f), 1.5f);
        lit = v3_add(lit, v3_scl(flood_col, lam * cone * 2600.0f / d2));
    }

    v3 plume_col = lo_plume_light(c);
    if (v3_maxc(plume_col) > 0.0f) {
        v3  lp = V3(c->fl.downrange, PAD_H + c->fl.alt - 2.2f, 0.0f);
        v3  dv = v3_sub(lp, p);
        /* Both the floor on the distance and the wrap on the terminator are
         * the same admission: the source is thirty metres of fire and not a
         * point. Without the floor the deck under the engines divides by
         * almost nothing and goes to white; without the wrap a body this close
         * to something that large gets an edge it would not really have. */
        f32 d2  = v3_dot(dv, dv) + 420.0f;
        v3  ld  = v3_scl(dv, 1.0f / sqrtf(d2));
        f32 lam = dm_sat((v3_dot(n, ld) + 0.35f) / 1.35f);
        lit = v3_add(lit, v3_scl(plume_col, lam / d2));
    }

    /* The sky, as fill. Five degrees before dawn it is not much, and what
     * there is comes from one direction. */
    f32 sky = 0.5f + 0.5f * n.y;
    lit = v3_add(lit, v3_scl(dm_rgb8(28, 44, 86), sky * 0.055f));

    return v3_mul(alb, lit);
}

/* ---- the deck ------------------------------------------------------------ */

static v3 deck(v3 p, const lo_ctx *c)
{
    f32 rr = sqrtf(p.x * p.x + p.z * p.z);

    /* The flame trench: an opening in the deck under the mount, running out to
     * one side. Nothing comes back out of it, so it is simply black. */
    f32 trench = dm_smoothstep(3.4f, 5.2f, fabsf(p.x)) +
                 dm_smoothstep(26.0f, 30.0f, fabsf(p.z));
    trench = dm_sat(trench);

    f32 n = dm_fbm3(V3(p.x * 0.14f, 0.0f, p.z * 0.14f), 4, 2.1f, 0.5f, 0x2c1);
    v3  con = v3_scl(dm_rgb8(96, 94, 92), 0.30f + 0.30f * dm_sat(n + 0.5f));
    /* Scorched near the middle, and more so the longer the engines have run. */
    f32 burn = 1.0f - dm_smoothstep(6.0f, 34.0f, rr);
    con = v3_lerp(con, v3_scl(dm_rgb8(38, 32, 30), 0.6f), burn * 0.8f);

    v3 alb = v3_scl(con, trench);
    return lighting(p, V3(0, 1, 0), alb, c);
}

/* ---- smoke ----------------------------------------------------------------
 *
 * Two things at once, from one density function. The cloud at the base, which
 * starts at ignition and spreads outward; and the trail, which is the cloud
 * the vehicle has been leaving behind it. A parcel of the trail hanging at
 * nine hundred metres is exactly as old as the time since the vehicle passed
 * nine hundred metres -- `lo_flight_time_of_alt` answers that -- and how old
 * it is decides how far it has spread and how much of it is left.
 */
static f32 smoke_density(v3 p, const lo_ctx *c)
{
    if (p.y < -2.0f) return 0.0f;

    f32 rr = sqrtf(p.x * p.x + p.z * p.z);
    f32 d  = 0.0f;

    f32 age = (f32)(c->mt - LO_T_IGNITION);
    if (age > 0.0f) {
        /* Spreading, not billowing. The cloud on a pad goes sideways first --
         * it is being pushed out along the deck by a jet aimed into a trench --
         * and only starts climbing once there is enough of it. */
        f32 Rc = 7.0f + 6.2f * powf(age, 0.78f);
        f32 Hc = 3.5f + 4.4f * powf(age, 0.55f);
        f32 radial = 1.0f - dm_smoothstep(Rc * 0.40f, Rc, rr);
        f32 vert   = expf(-dm_sq(DM_MAX(p.y - 2.0f, 0.0f) / Hc) * 0.55f);
        /* Clear immediately around the jet: the gas there is moving too fast
         * to be smoke yet. */
        f32 hole = dm_smoothstep(2.5f, 8.0f, rr);
        d += radial * vert * hole * 1.25f;
    }

    f32 top = PAD_H + c->fl.alt;
    if (p.y > 10.0f && p.y < top) {
        /* The trail marks the path, not the axis. The parcel hanging at this
         * height was left by a vehicle that was at this height once, so the
         * trajectory is asked when that was and where it was: the column bends
         * downrange because the flight did. */
        f32 h      = p.y - PAD_H;
        f32 parcel = (f32)(c->mt - lo_flight_time_of_alt(h));
        if (parcel > 0.0f) {
            f32 dx = p.x - lo_flight_downrange_of_alt(h);
            f32 rt = sqrtf(dx * dx + p.z * p.z);
            f32 Rt = 5.5f + 1.25f * parcel;
            f32 core = expf(-dm_sq(rt / Rt) * 1.5f);
            d += core * expf(-parcel * 0.075f) * 1.05f;
        }
    }

    /* Venting. Before the release the vehicle is boiling off oxygen faster
     * than it is being topped up, and the vapour drifts off the side of the
     * tank -- the only thing moving anywhere in the first shot. */
    if (c->mt < 2.0) {
        f32 vy = p.y - (top + 33.0f);
        f32 vx = p.x + 4.0f + (f32)c->mt * 0.55f;
        f32 wisp = expf(-(vx * vx * 0.032f + vy * vy * 0.085f + p.z * p.z * 0.055f));
        d += wisp * 0.20f;
    }

    /* Below this the parcel cannot survive being multiplied by the turbulence
     * and composited, so it does not get to pay for a noise lookup. This one
     * threshold is worth about a fifth of the render: the march runs for very
     * nearly every pixel in the two shots on the ground. */
    if (d < 0.018f) return 0.0f;

    v3 w = v3_scl(p, 0.030f);
    w.y -= (f32)c->mt * 0.10f;
    f32 n = dm_fbm3(w, 2, 2.3f, 0.55f, 0x8ce1);
    return d * (0.35f + 1.05f * dm_sat(n + 0.5f));
}

/* Marches the smoke and composites it over whatever was behind. Emission is
 * not the point here -- smoke is lit, not glowing -- so this one tracks
 * transmittance properly rather than just adding light. */
static v3 smoke_march(v3 ro, v3 rd, f32 t_end, const lo_ctx *c, v3 back, u32 seed)
{
    f32 age = (f32)(c->mt - LO_T_IGNITION);
    if (age <= 0.0f && c->mt > 2.0) return back;

    /* Everything the density function can be non-zero inside fits in one
     * cylinder about the axis. Finding where the ray crosses that and marching
     * only there is not merely an optimisation: a fixed march from the camera
     * out to the sky would space its samples two dozen metres apart and smear
     * the cloud across the whole frame, which is what it did before this. */
    f32 grow = DM_MAX(age, 0.0f);
    f32 Rb   = 20.0f + 6.2f * powf(grow, 0.78f) + 2.1f * grow;
    f32 ytop = PAD_H + c->fl.alt + 48.0f;

    f32 t0 = 0.0f, t1 = DM_MIN(t_end, 6000.0f);
    {
        f32 a  = rd.x * rd.x + rd.z * rd.z;
        f32 b  = ro.x * rd.x + ro.z * rd.z;
        f32 cc = ro.x * ro.x + ro.z * ro.z - Rb * Rb;
        if (a > 1e-8f) {
            f32 disc = b * b - a * cc;
            if (disc <= 0.0f) return back;
            f32 s = sqrtf(disc);
            t0 = DM_MAX(t0, (-b - s) / a);
            t1 = DM_MIN(t1, (-b + s) / a);
        } else if (cc > 0.0f) {
            return back;
        }
        if (fabsf(rd.y) > 1e-6f) {
            f32 ta = (-3.0f - ro.y) / rd.y, tb = (ytop - ro.y) / rd.y;
            if (ta > tb) { f32 s = ta; ta = tb; tb = s; }
            t0 = DM_MAX(t0, ta);
            t1 = DM_MIN(t1, tb);
        } else if (ro.y < -3.0f || ro.y > ytop) {
            return back;
        }
        if (t1 <= t0) return back;
    }

    const int N = 20;
    f32 step = (t1 - t0) / (f32)N;
    f32 jit  = t0 + dm_u32_to_f32(dm_hash_u32(seed)) * step;

    v3  acc   = V3(0, 0, 0);
    f32 trans = 1.0f;

    v3 plume_col = lo_plume_light(c);
    v3 lp = V3(c->fl.downrange, PAD_H + c->fl.alt - 2.0f, 0.0f);

    /* Built once, above the march. `dm_kelvin` is two logarithms and three
     * powers and `dm_rgb8` is three more, and neither depends on the sample.
     * gcc hoists them out of the loop on its own -- they are pure and inline,
     * and lifting them by hand measured as no faster at all -- but the loop
     * reads better with the constants named where they are constant. */
    v3 xenon = dm_kelvin(6200.0f);
    v3 sky   = v3_scl(dm_rgb8(30, 48, 92), 0.012f);

    for (int i = 0; i < N; i++) {
        f32 t = jit + (f32)i * step;
        if (t >= t1) break;
        v3  q = v3_add(ro, v3_scl(rd, t));

        f32 dens = smoke_density(q, c);
        if (dens < 0.004f) continue;

        f32 a = 1.0f - expf(-dens * step * 0.085f);

        /* The source is thirty metres of fire rather than a point, so the
         * inverse square is floored: without that, smoke passing within a few
         * metres of the engine plane goes to infinity. */
        v3  dv = v3_sub(lp, q);
        f32 d2 = v3_dot(dv, dv) + 420.0f;
        v3  lit = v3_scl(plume_col, 0.55f / d2);

        /* The floodlights, which are bolted to the pad and obey the same
         * inverse square as everything else -- so the column of smoke a
         * kilometre up is lit by the engines alone and is otherwise as dark as
         * the sky it is standing in front of. Smoke this thick is also mostly
         * a shadow of itself, which is the exponential. */
        f32 pd2 = q.x * q.x + dm_sq(q.y - 8.0f) + q.z * q.z + 900.0f;
        lit = v3_add(lit, v3_scl(xenon, 54.0f / pd2 * expf(-dens * 1.4f)));
        lit = v3_add(lit, sky);

        acc   = v3_add(acc, v3_scl(lit, a * trans));
        trans *= 1.0f - a;
        if (trans < 0.01f) break;
    }

    return v3_add(v3_scl(back, trans), acc);
}

/* ---- the shot ------------------------------------------------------------ */

/* Does the ray come within `r` of `centre`, and if so between which two
 * distances? Two of these bound everything this scene marches. */
static int bound(v3 ro, v3 rd, v3 centre, f32 r, f32 *t0, f32 *t1)
{
    v3  oc = v3_sub(ro, centre);
    f32 b  = v3_dot(oc, rd);
    f32 cc = v3_dot(oc, oc) - r * r;
    f32 disc = b * b - cc;
    if (disc < 0.0f) return 0;
    f32 s = sqrtf(disc);
    *t0 = -b - s;
    *t1 = -b + s;
    return *t1 > 0.0f;
}

static v3 pad_px(v2 uv, const lo_ctx *c, u32 seed, v3 ro, v3 ta, f32 fov)
{
    v3 rr, uu, ff;
    lo_basis(ro, ta, &rr, &uu, &ff);
    v3 rdl = lo_ray(uv, fov);
    v3 rd  = v3_norm(v3_add(v3_add(v3_scl(rr, rdl.x), v3_scl(uu, rdl.y)),
                            v3_scl(ff, rdl.z)));

    /* The deck, analytically: it is a plane, and marching a plane is a way of
     * spending a hundred steps to find something a division would have found. */
    f32 t_plane = rd.y < -1e-4f ? -ro.y / rd.y : 1e9f;
    f32 far     = DM_MIN(t_plane, 4000.0f);

    /* Two spheres, one round the tower and one round the vehicle, and the
     * march is clipped to whichever of them the ray actually crosses. Most of
     * a frame this tall is sky and deck: without this, every one of those
     * pixels pays a hundred and ten distance evaluations to find out it was
     * never going to hit anything. */
    f32 ta0 = 0.0f, ta1 = 0.0f, tb0 = 0.0f, tb1 = 0.0f;
    int in_tower = bound(ro, rd, V3(TOWER_X, TOWER_H * 0.55f, 0.0f), 52.0f, &ta0, &ta1);
    int in_veh   = bound(ro, rd, V3(c->fl.downrange, PAD_H + c->fl.alt + LO_HEIGHT * 0.5f, 0.0f),
                         LO_HEIGHT * 0.62f, &tb0, &tb1);

    f32 t = 0.4f;
    int part = 0, hit = 0;

    if (in_tower || in_veh) {
        f32 lo = 1e9f, hi = 0.0f;
        if (in_tower) { lo = DM_MIN(lo, ta0); hi = DM_MAX(hi, ta1); }
        if (in_veh)   { lo = DM_MIN(lo, tb0); hi = DM_MAX(hi, tb1); }

        f32 end = DM_MIN(far, hi);
        t = DM_MAX(t, lo);

        for (int i = 0; i < 110 && t < end; i++) {
            f32 d = map(v3_add(ro, v3_scl(rd, t)), c, &part);
            if (d < 0.0016f * t) { hit = 1; break; }
            t += d * 0.92f;
        }
    }

    v3 col;
    f32 t_hit;

    if (hit) {
        v3 p = v3_add(ro, v3_scl(rd, t));
        v3 n = normal_at(p, c);
        v3 alb = part < 0 ? v3_scl(dm_rgb8(120, 122, 126), 0.42f)
                          : lo_vehicle_albedo(to_vehicle(p, c), part, seed);
        col   = lighting(p, n, alb, c);
        col   = v3_scl(col, ambient_occlusion(p, n, c));
        t_hit = t;
    } else if (t_plane < 4000.0f) {
        v3 p = v3_add(ro, v3_scl(rd, t_plane));
        col   = deck(p, c);
        t_hit = t_plane;
    } else {
        col   = lo_world(rd, 0.0f, 0.0f, seed);
        t_hit = 4000.0f;
    }

    /* The exhaust, in the vehicle's own frame, adding to whatever is behind
     * it and stopping where the ray hit something. */
    col = v3_add(col, lo_plume(to_vehicle(ro, c), to_vehicle_dir(rd, c),
                               t_hit, c, seed));

    /* And the smoke over the top of all of it. */
    col = smoke_march(ro, rd, t_hit, c, col, seed);

    return v3_scl(col, lo_vignette(uv, 0.26f));
}

/* Fifty metres out and low, looking up the side. The lens is wide enough that
 * the tower converges, which is the cue that says "this thing is large" more
 * cheaply than any amount of detail on it. */
static v3 hold_px(v2 uv, const lo_ctx *c, u32 seed)
{
    f32 k = (f32)c->sec_phase;
    v3 ro = V3(-31.0f + k * 2.4f, 4.6f + k * 1.4f, -37.0f + k * 2.8f);
    v3 ta = V3(0.0f, 25.0f + k * 2.0f, 0.0f);
    return pad_px(uv, c, seed, ro, ta, 1.30f);
}

/* The tracking camera. Fixed on the ground, on a long lens that gets longer:
 * real launch coverage is shot this way because the vehicle is a kilometre up
 * within twenty seconds and nothing else keeps it in frame. */
static v3 lift_px(v2 uv, const lo_ctx *c, u32 seed)
{
    f32 k = (f32)c->sec_phase;
    v3 ro = V3(-92.0f, 10.5f, -114.0f);
    v3 ta = V3(c->fl.downrange, PAD_H + c->fl.alt + 26.0f, 0.0f);
    /* Eleven times over the shot. The vehicle ends it a kilometre and a third
     * away and the lens is what keeps it the same size in frame the whole way:
     * real coverage is shot on a long lens that gets longer for exactly this
     * reason, and the give-away that it is happening is the pad furniture at
     * the start of the shot swelling as the zoom comes in. */
    f32 fov = 1.95f + 19.0f * dm_ease_in_quad(k);
    return pad_px(uv, c, seed, ro, ta, fov);
}

/* ---- scenes -------------------------------------------------------------- */

void scene_hold(dm_fb *fb, const lo_ctx *c)
{
    lo_shade(fb, c, hold_px, lo_aa);
    lo_caption(fb, c, "NO ASSETS", song_bar(0.25), song_bar(1.55));
}

void scene_lift(dm_fb *fb, const lo_ctx *c)
{
    lo_shade(fb, c, lift_px, lo_aa);
    lo_caption(fb, c, "LIFTOFF", song_bar(2.05), song_bar(3.30));
    lo_caption(fb, c, "WRITTEN IN C", song_bar(3.60), song_bar(4.85));
}
