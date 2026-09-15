/* sc_wreck.c -- Act five: 5 March 2022, 3,008 metres.
 *
 * Found at 16:05 GMT by a tethered Sabertooth vehicle working from the S.A.
 * Agulhas II. The Conservation Management Plan describes what its cameras
 * saw, and that description is what is built here: the hull upright and well
 * proud of the mud, lying roughly north and south; the broken lower foremast
 * across the deck; the mainmast on the seabed to port and the mizzen across
 * the deck to starboard; the funnel collapsed onto the poop; the after
 * deckhouse gone but for its forward bulkhead; the rudder off, under the
 * counter; the wheel and the taffrail standing. On the counter, ENDURANCE, and
 * the star. Anemones on her -- fat white ones and slimmer orange ones -- and
 * stalked sea lilies, and brittle stars on the mud around her.
 *
 * Nothing lights any of it but the vehicle's own lamps.
 */
#include "scenes.h"
#include "../under.h"

#include <string.h>

/* ---- her, as she lies -------------------------------------------------------- */

static void wreck_ship(under_view *v)
{
    v->has_ship = 1;
    v->st = en_ship_state_default();
    v->st.wreck = 1;
    v->st.no_boats = 1;
    v->st.wheelhouse = 0.0f;

    /* The broken lower foremast, across the deck. */
    v->st.angle[RIG_FORE_LOWER] = 1.42f;
    v->st.axis[RIG_FORE_LOWER]  = V3(1.0f, 0.0f, 0.15f);
    v->st.shift[RIG_FORE_LOWER] = V3(0.0f, -0.9f, 0.0f);
    v->st.gone[RIG_FORE_TOP] = 1;

    /* The mainmast, over the port side and down onto the seabed. */
    v->st.angle[RIG_MAIN_LOWER] = 1.83f;
    v->st.axis[RIG_MAIN_LOWER]  = V3(0.95f, 0.0f, -0.3f);
    v->st.shift[RIG_MAIN_LOWER] = V3(0.0f, -6.2f, 1.2f);
    v->st.gone[RIG_MAIN_TOP] = 1;

    /* The mizzen, across the deck to starboard. */
    v->st.angle[RIG_MIZ_LOWER] = -1.45f;
    v->st.axis[RIG_MIZ_LOWER]  = V3(0.77f, 0.0f, 0.64f);
    v->st.shift[RIG_MIZ_LOWER] = V3(0.0f, -1.1f, 0.0f);
    v->st.gone[RIG_MIZ_TOP] = 1;

    /* The bowsprit, on the seabed to port; the funnel, down on the poop. */
    v->st.angle[RIG_BOWSPRIT] = 1.2f;
    v->st.axis[RIG_BOWSPRIT]  = V3(0.0f, 1.0f, 0.3f);
    v->st.shift[RIG_BOWSPRIT] = V3(-1.0f, -6.5f, 3.0f);
    v->st.angle[RIG_FUNNEL] = -1.45f;
    v->st.axis[RIG_FUNNEL]  = V3(1.0f, 0.0f, 0.0f);
    v->st.shift[RIG_FUNNEL] = V3(0.0f, -0.3f, 0.0f);

    /* Upright, heading north, sunk to her draught marks in the mud. */
    v->pose.yaw  = -DM_HALFPI;
    v->pose.heel = 2.5f * DM_D2R;
    v->pose.trim = 0.8f * DM_D2R;
    en_ship_pose_make(&v->pose);
    v->pose.pos = V3(0.0f, -1.25f, 0.0f);

    v->has_seabed = 1;
    v->seabed_y = 0.0f;
}

/* ---- the life on her ------------------------------------------------------------
 *
 * Placed by hand along the stern, in the ship's frame: an anchor point, the
 * way the surface faces there, a size and a kind.
 */
enum { LIFE_WHITE = 100, LIFE_ORANGE, LIFE_LILY, LIFE_STAR };

typedef struct { v3 at, up; f32 size; int kind; } growth;

static const growth LIFE[] = {
    /* The big white anemone on the taffrail, over the middle of the name. */
    { { -21.86f, 6.42f,  0.10f }, { 0.0f, 1.0f, 0.0f }, 1.35f, LIFE_WHITE  },
    { { -21.70f, 6.40f,  1.35f }, { 0.0f, 1.0f, 0.0f }, 0.80f, LIFE_WHITE  },
    { { -20.60f, 6.36f, -3.05f }, { 0.0f, 1.0f, 0.0f }, 0.90f, LIFE_WHITE  },
    { { -21.94f, 4.95f, -1.95f }, {-1.0f, 0.1f, 0.0f }, 0.70f, LIFE_ORANGE },
    { { -21.80f, 4.55f, -2.45f }, {-0.9f, 0.2f,-0.3f }, 0.60f, LIFE_ORANGE },
    { { -21.88f, 4.30f, -1.60f }, {-1.0f, 0.0f, 0.0f }, 0.55f, LIFE_WHITE  },
    { { -21.20f, 6.39f, -1.55f }, { 0.0f, 1.0f, 0.0f }, 1.00f, LIFE_LILY   },
    { { -20.10f, 6.30f,  2.30f }, { 0.0f, 1.0f, 0.0f }, 0.90f, LIFE_LILY   },
    { { -18.60f, 6.25f, -3.20f }, { 0.0f, 1.0f, 0.0f }, 0.80f, LIFE_ORANGE },
};

/* An anemone: a stout column, and on top of it a disc fringed with short
 * tentacles -- twenty of them, drawn as one through a fold of the angle. */
static f32 anemone(v3 q, f32 s)
{
    f32 rc = 0.075f * s, hc = 0.11f * s;
    f32 rr = sqrtf(q.x * q.x + q.z * q.z);
    f32 flare = rc * (1.0f + 0.35f * dm_sat(q.y / hc));
    f32 col = DM_MAX(rr - flare, DM_MAX(-q.y, q.y - hc));
    f32 ang = atan2f(q.z, q.x);
    const f32 sect = DM_TAU / 20.0f;
    f32 a2 = dm_mod(ang + sect * 0.5f, sect) - sect * 0.5f;
    v3  tq = V3(rr * cosf(a2), q.y - hc, rr * sinf(a2));
    f32 tent = en_sd_capsule(tq, V3(rc * 0.6f, 0.0f, 0.0f), V3(rc * 1.9f, 0.035f * s, 0.0f), 0.011f * s);
    return DM_MIN(col, tent);
}

/* A stalked crinoid: a thin stem and a crown of feathery arms, opened upward. */
static f32 sea_lily(v3 q, f32 s)
{
    f32 h = 0.42f * s;
    f32 stem = en_sd_capsule(q, V3(0, 0, 0), V3(0.02f * s, h, 0.0f), 0.006f * s);
    f32 rr = sqrtf(q.x * q.x + q.z * q.z);
    f32 ang = atan2f(q.z, q.x);
    const f32 sect = DM_TAU / 10.0f;
    f32 a2 = dm_mod(ang + sect * 0.5f, sect) - sect * 0.5f;
    v3  aq = V3(rr * cosf(a2), q.y, rr * sinf(a2));
    f32 arm = en_sd_capsule(aq, V3(0.01f * s, h, 0.0f), V3(0.10f * s, h + 0.10f * s, 0.0f), 0.005f * s);
    arm = DM_MIN(arm, en_sd_capsule(aq, V3(0.10f * s, h + 0.10f * s, 0.0f), V3(0.13f * s, h + 0.05f * s, 0.0f), 0.004f * s));
    return DM_MIN(stem, arm);
}

static f32 life_sdf(v3 p, void *ud, int *mat)
{
    const under_view *v = (const under_view *)ud;
    v3 l = en_ship_to_local(&v->pose, p);
    f32 d = 1e9f;
    int m = LIFE_WHITE;
    for (int i = 0; i < DM_COUNT(LIFE); i++) {
        const growth *g = &LIFE[i];
        v3 rel = v3_sub(l, g->at);
        if (v3_len2(rel) > 0.6f * g->size * g->size + 0.1f) {
            f32 b = v3_len(rel) - 0.6f * g->size;
            if (b < d) d = b;
            continue;
        }
        m3 B = m3_basis(v3_norm(g->up));
        v3 q = V3(v3_dot(rel, B.c[0]), v3_dot(rel, B.c[2]), v3_dot(rel, B.c[1]));
        f32 s = g->kind == LIFE_LILY ? sea_lily(q, g->size) : anemone(q, g->size);
        if (s < d) { d = s; m = g->kind; }
    }
    *mat = m;
    return d;
}

static v3 life_albedo(v3 p, v3 n, int mat, void *ud)
{
    (void)n; (void)ud;
    f32 k = dm_fbm3(v3_scl(p, 30.0f), 2, 2.0f, 0.5f, 0x11feu);
    switch (mat) {
    case LIFE_ORANGE: return v3_scl(V3(0.85f, 0.38f, 0.16f), 0.9f + 0.2f * k);
    case LIFE_LILY:   return v3_scl(V3(0.80f, 0.70f, 0.18f), 0.9f + 0.2f * k);
    default:          return v3_scl(V3(0.86f, 0.84f, 0.78f), 0.92f + 0.12f * k);
    }
}

static void life_bounds(under_view *v)
{
    v3 lo = V3(1e9f, 1e9f, 1e9f), hi = V3(-1e9f, -1e9f, -1e9f);
    for (int c = 0; c < 8; c++) {
        v3 q = V3((c & 1) ? -17.5f : -22.6f, (c & 2) ? 7.2f : 3.8f, (c & 4) ? 4.0f : -4.0f);
        v3 w = en_ship_to_world(&v->pose, q);
        lo = v3_min(lo, w);
        hi = v3_max(hi, w);
    }
    v->extra_lo = lo;
    v->extra_hi = hi;
    v->extra_sdf = life_sdf;
    v->extra_albedo = life_albedo;
    v->extra_ud = v;
}

/* ---- the vehicle's lamps ---------------------------------------------------------- */

static void vehicle_lamps(under_view *v, f32 on)
{
    const en_cam *cam = &v->v.cam;
    v->nlamps = 2;
    for (int i = 0; i < 2; i++) {
        f32 s = i ? 1.0f : -1.0f;
        en_lamp *L = &v->lamps[i];
        L->pos = v3_add(cam->pos, v3_add(v3_scl(cam->rt, 0.9f * s), v3_scl(cam->up, -0.35f)));
        L->dir = v3_norm(v3_add(cam->fw, v3_add(v3_scl(cam->rt, -0.08f * s), v3_scl(cam->up, -0.10f))));
        L->cos_outer = cosf(48.0f * DM_D2R);
        L->cos_inner = cosf(22.0f * DM_D2R);
        /* LED lamps, a little cooler than daylight. */
        L->power = v3_scl(V3(0.90f, 0.97f, 1.00f), 60.0f * on);
    }
}

/* ---- shot nine: the lamps find her ------------------------------------------------ */

static void find_setup(under_view *v, const en_ctx *c, f64 t, void *ud)
{
    (void)ud;
    wreck_ship(v);
    f32 u = (f32)((t - en_shot_start(c->shot)) / (en_shot_end(c->shot) - en_shot_start(c->shot)));
    u = dm_clamp(u, 0.0f, 1.0f);
    f32 e = u * u * (3.0f - 2.0f * u);

    /* Along her port quarter a few metres off the mud, then up over the rail
     * to look down on the wheel, still standing where the helmsman stood. */
    v3 a0 = V3(-4.0f, 6.5f, 13.0f), a1 = V3(-15.8f, 10.6f, 5.8f);
    v3 b0 = V3(-15.0f, 5.2f, 2.5f), b1 = V3(-18.3f, 6.2f, 0.0f);
    v3 eye = en_ship_to_world(&v->pose, v3_lerp(a0, a1, e));
    v3 at  = en_ship_to_world(&v->pose, v3_lerp(b0, b1, e));
    v->v.cam = en_cam_look(eye, at, 0.0f, 24.0f, 2.8f, v3_dist(eye, at));
    /* The lamps come up out of nothing in the first second. */
    vehicle_lamps(v, dm_smoothstep(0.0f, 0.12f, u));
    life_bounds(v);
}

void shot_found_wheel(dm_fb *fb, f32 *depth, const en_ctx *c)
{
    under_shot s = { find_setup, NULL, 5.0f, 0.08f, 0, V2(0.0f, 0.0f), 0 };
    under_render(fb, depth, c, &s);
}

/* ---- shot ten: her name ------------------------------------------------------------ */

static void stern_setup(under_view *v, const en_ctx *c, f64 t, void *ud)
{
    (void)ud;
    wreck_ship(v);
    f32 u = (f32)((t - en_shot_start(c->shot)) / (en_shot_end(c->shot) - en_shot_start(c->shot)));
    u = dm_clamp(u, 0.0f, 1.0f);
    f32 e = u * (2.0f - u);
    v3 eye = en_ship_to_world(&v->pose, V3(dm_lerp(-30.5f, -27.2f, e), dm_lerp(4.6f, 5.45f, e), dm_lerp(2.4f, 0.25f, e)));
    v3 at  = en_ship_to_world(&v->pose, V3(-21.9f, 5.35f, 0.0f));
    v->v.cam = en_cam_look(eye, at, 0.0f, 32.0f, 2.8f, v3_dist(eye, at));
    vehicle_lamps(v, 1.0f);
    life_bounds(v);
}

void shot_found_stern(dm_fb *fb, f32 *depth, const en_ctx *c)
{
    under_shot s = { stern_setup, NULL, 5.0f, 0.06f, 0, V2(0.0f, 0.0f), 0 };
    under_render(fb, depth, c, &s);
}
