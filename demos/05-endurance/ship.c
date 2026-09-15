/* ship.c -- the hull, the deck, the rig, and the name on her stern.
 *
 * The hull is a lofted shape rather than a union of primitives: a plan, a
 * profile and a family of sections, and the surface is wherever the distance
 * across the ship equals the half-breadth those three give at that length and
 * height. The plan is full for most of her length and fine at the bow; the
 * sections are round-bilged amidships and sharpen toward the ends; the profile
 * has a raked stem with its forefoot cut away, and aft, above the rudder, the
 * counter stern -- the overhang her name was written across.
 *
 * Everything above the deck follows the builder's rig and deck plan for
 * Polaris and Hurley's photographs of her, which between them fix the order of
 * things from bow to stern and the heights to within about a tenth: the
 * fore and main trucks some twenty-four metres above the deck, the mizzen
 * nineteen, a fore yard fourteen and a half metres across. Where neither
 * source says, the numbers are what a Norwegian yard would have built in 1912
 * and are marked as such.
 */
#include "ship.h"
#include "../../engine/dm_font.h"

#include <string.h>

#define L2        (SHIP_LOA * 0.5f)
#define HB_MAX    (SHIP_BEAM * 0.5f)
#define KEEL_Y    0.35f
#define BULWARK   1.05f
#define STERNPOST -17.6f

/* Along the deck, bow to stern. */
#define FOCSLE_X    16.0f       /* aft edge of the raised fo'c'sle           */
#define FOCSLE_H     1.90f
#define FORE_X      12.8f
#define FWD_HOUSE0   3.4f       /* wardroom, galley and pantry               */
#define FWD_HOUSE1  10.4f
#define MAIN_X       1.2f
#define FUNNEL_X    -3.1f
#define AFT_HOUSE0  -2.0f       /* ten cabins, the full width of the deck    */
#define AFT_HOUSE1 -16.4f
#define HOUSE_H      2.15f
#define MIZ_X      -13.2f
#define WHEEL_X    -18.3f

/* ---- the lines -------------------------------------------------------------- */

/* Main deck height at the side: 4.80 amidships, rising toward both ends. */
static f32 deck_y(f32 x)
{
    f32 u = x / L2;
    return SHIP_DEPTH + (u > 0.0f ? 1.05f : 0.55f) * u * u;
}

/* The bulwark rail, which steps up over the fo'c'sle. */
static f32 rail_y(f32 x)
{
    return deck_y(x) + BULWARK + (FOCSLE_H - 0.40f) * dm_smoothstep(15.6f, 16.4f, x);
}

/* The stem: raked, and cut away below so the forefoot rides up onto ice
 * rather than into it. */
static f32 x_stem(f32 y)
{
    f32 s = dm_sat((y - KEEL_Y) / (6.4f - KEEL_Y));
    return L2 - 3.4f * (1.0f - s) * (1.0f - s);
}

/* The stern: the sternpost below, and above it a counter that swells aft in a
 * quarter ellipse to the full length at the deck. */
static f32 x_stern(f32 y)
{
    if (y <= 2.3f) return STERNPOST;
    f32 q = dm_sat((y - 2.3f) / 3.0f);
    f32 over = -L2 - STERNPOST;            /* negative: how far past the post */
    return STERNPOST + over * sqrtf(DM_MAX(0.0f, 1.0f - (1.0f - q) * (1.0f - q)));
}

/* The length of the counter's ellipse in plan: three and a half metres at the
 * deck, shrinking to a rounded end of a metre where the counter meets the
 * sternpost below. */
static f32 counter_len(f32 y)
{
    return dm_lerp(1.0f, 3.5f, dm_smoothstep(2.3f, 4.6f, y));
}

/* Half-breadth forward of the counter, before the sections are applied: the
 * full breadth through the middle, a fine entry that meets the stem at a
 * finite angle, and aft a slight narrowing toward the counter. */
static f32 plan_main(f32 x, f32 y)
{
    f32 xf = x_stem(y);
    if (x > 0.0f) {
        f32 u = x / xf;
        if (u <= 0.10f) return HB_MAX;
        f32 t = dm_sat((u - 0.10f) / 0.90f);
        return HB_MAX * (1.0f - powf(t, 2.2f));
    }
    return HB_MAX - 0.45f * dm_smoothstep(0.0f, -16.0f, x);
}

/* The sections: how much of the plan's breadth there is at a height. Round
 * bilged amidships, sharper toward the ends; every shape here has a finite
 * slope, so a march can trust the distance built from it. */
static f32 section(f32 x, f32 y)
{
    if (y <= KEEL_Y) return 0.0f;
    f32 au   = fabsf(x) / L2;
    f32 ybil = dm_lerp(2.9f, 5.0f, dm_smoothstep(0.40f, 1.0f, au));
    f32 k    = dm_lerp(4.0f, 1.7f, dm_smoothstep(0.35f, 1.0f, au));
    f32 yn   = dm_sat((y - KEEL_Y) / (ybil - KEEL_Y));
    f32 s    = 1.0f - powf(1.0f - yn, k);
    /* A little tumblehome above the turn of the bilge. */
    if (y > ybil) s -= 0.022f * (y - ybil);
    return s;
}

/* The approximate distance to an ellipse, good on both sides of it. */
static f32 sd_ellipse(v2 p, v2 r)
{
    f32 k0 = v2_len(V2(p.x / r.x, p.y / r.y));
    f32 k1 = v2_len(V2(p.x / (r.x * r.x), p.y / (r.y * r.y)));
    return k1 > 1e-9f ? k0 * (k0 - 1.0f) / k1 : -DM_MIN(r.x, r.y);
}

f32 en_hull_half_breadth(f32 x, f32 y)
{
    if (y <= KEEL_Y) return 0.0f;
    f32 xa = x_stern(y), xf = x_stem(y);
    if (x <= xa || x >= xf) return 0.0f;
    f32 xc = xa + counter_len(y);
    if (x < xc) {
        f32 B = plan_main(xc, y) * section(xc, y);
        f32 q = (x - xc) / (xc - xa);
        return B * sqrtf(DM_MAX(0.0f, 1.0f - q * q));
    }
    return DM_MAX(plan_main(x, y) * section(x, y), 0.0f);
}

/* Signed distance across the horizontal plane at height y to the hull's
 * outline there. Over the counter it is the distance to an ellipse; forward
 * of it, the breadth difference over its own slope along the ship; past the
 * stem, the distance to the stem. `aft` receives how squarely the nearest
 * surface faces aft. */
static f32 outline_2d(f32 x, f32 y, f32 z, f32 *aft)
{
    f32 yy = DM_MAX(y, KEEL_Y + 0.01f);
    f32 xa = x_stern(yy), xf = x_stem(yy);
    f32 xc = xa + counter_len(yy);
    f32 az = fabsf(z);

    if (x < xc) {
        f32 A = xc - xa;
        f32 B = DM_MAX(plan_main(xc, yy) * section(xc, yy), 0.05f);
        v2  p = V2(x - xc, az);
        if (aft) {
            v2 g = V2(p.x / (A * A), p.y / (B * B));
            f32 gl = v2_len(g);
            *aft = gl > 1e-9f ? dm_sat(-g.x / gl) : 0.0f;
        }
        return sd_ellipse(p, V2(A, B));
    }

    if (aft) *aft = 0.0f;
    f32 hb = plan_main(x, yy) * section(x, yy);
    const f32 e = 0.05f;
    f32 gx = (plan_main(x + e, yy) * section(x + e, yy) - plan_main(x - e, yy) * section(x - e, yy)) / (2.0f * e);
    f32 d = (az - hb) / sqrtf(1.0f + gx * gx);
    if (x > xf) d = DM_MAX(d, x - xf);
    return d;
}

f32 en_hull_waterline_distance(f32 x, f32 z, f32 y)
{
    return outline_2d(x, y, z, NULL);
}

/* The horizontal distance corrected for how fast the outline moves with
 * height, which is what turns it into a distance in three dimensions: the
 * underside of the counter, the turn of the bilge, the rake of the stem. */
static f32 hull_side(v3 p, f32 *aft)
{
    f32 d = outline_2d(p.x, p.y, p.z, aft);
    const f32 e = 0.05f;
    f32 gy = (outline_2d(p.x, p.y + e, p.z, NULL) - outline_2d(p.x, p.y - e, p.z, NULL)) / (2.0f * e);
    d /= sqrtf(1.0f + gy * gy);
    if (p.y < KEEL_Y) d = DM_MAX(d, KEEL_Y - p.y);
    return d;
}

/* ---- the name --------------------------------------------------------------
 *
 * ENDURANCE, arced across the counter directly under the taffrail, and below
 * it the five-pointed star that Polaris was named for, with a long pointed
 * flourish either side. Built once as two-dimensional strokes across the
 * stern -- across and up, as seen from astern -- and raised a centimetre and a
 * half proud of the planking.
 *
 * The letters are the engine's stroke font, given what a signwriter of 1912
 * would have given them: thick strokes where they run up and down, thin ones
 * across, and serifs on the free ends.
 */
#define MAX_NAME_SEGS 256
#define NAME_BASE_Y   5.55f
#define STAR_R        0.19f

typedef struct { v2 a, b; f32 w; } seg2;

static seg2 g_name[MAX_NAME_SEGS];
static int  g_name_n;
static v2   g_name_lo, g_name_hi;
static v2   g_star_c;

/* A five-pointed star, exactly: fold the plane into one tenth of it and take
 * the distance to a single edge (after Inigo Quilez). `rf` is the ratio of
 * the inner radius to the outer. */
static f32 sd_star5(v2 p, f32 r, f32 rf)
{
    const v2 k1 = { 0.809016994f, -0.587785252f };
    const v2 k2 = { -0.809016994f, -0.587785252f };
    p.x = fabsf(p.x);
    p = v2_sub(p, v2_scl(k1, 2.0f * DM_MAX(v2_dot(k1, p), 0.0f)));
    p = v2_sub(p, v2_scl(k2, 2.0f * DM_MAX(v2_dot(k2, p), 0.0f)));
    p.x = fabsf(p.x);
    p.y -= r;
    v2  ba = v2_sub(v2_scl(V2(-k1.y, k1.x), rf), V2(0.0f, 1.0f));
    f32 h  = dm_clamp(v2_dot(p, ba) / v2_dot(ba, ba), 0.0f, r);
    f32 s  = p.y * ba.x - p.x * ba.y;
    return v2_len(v2_sub(p, v2_scl(ba, h))) * (s < 0.0f ? -1.0f : 1.0f);
}

static void name_seg(v2 a, v2 b, f32 w)
{
    if (g_name_n >= MAX_NAME_SEGS) return;
    g_name[g_name_n].a = a;
    g_name[g_name_n].b = b;
    g_name[g_name_n].w = w;
    g_name_n++;
}

static void build_name(void)
{
    const char *name = "ENDURANCE";
    int n = (int)strlen(name);

    /* Letter height and the arc they sit on, in metres across the stern. */
    const f32 H   = 0.36f;
    const f32 ADV = H * 0.84f;
    const f32 ARC_R = 5.4f;
    f32 total = ADV * (f32)(n - 1);
    f32 cy = NAME_BASE_Y - ARC_R;

    g_name_n = 0;
    for (int i = 0; i < n; i++) {
        v2  pts[32];
        u8  pen[32];
        int np = dm_glyph_points(name[i], pts, pen, 32);

        /* Where along the arc this letter's centre falls, and which way up. */
        f32 s   = -total * 0.5f + ADV * (f32)i;
        f32 ang = s / ARC_R;
        v2  centre = V2(sinf(ang) * ARC_R, cy + cosf(ang) * ARC_R);
        v2  right  = V2(cosf(ang), -sinf(ang));
        v2  up     = V2(sinf(ang), cosf(ang));

        for (int k = 1; k < np; k++) {
            if (!pen[k]) continue;
            /* Glyph grid is 6 wide and 10 tall; centre it on the letter. */
            v2 ga = V2((pts[k - 1].x - 3.0f) / 10.0f * H, pts[k - 1].y / 10.0f * H);
            v2 gb = V2((pts[k].x - 3.0f) / 10.0f * H, pts[k].y / 10.0f * H);
            v2 a  = v2_add(centre, v2_add(v2_scl(right, ga.x), v2_scl(up, ga.y)));
            v2 b  = v2_add(centre, v2_add(v2_scl(right, gb.x), v2_scl(up, gb.y)));
            v2 d  = v2_norm(v2_sub(gb, ga));
            f32 w = dm_lerp(0.010f, 0.026f, fabsf(d.y));
            name_seg(a, b, w);

            /* A serif on a free end of an upright stroke, at the cap height or
             * on the baseline, where no other stroke of the letter meets it. */
            for (int e = 0; e < 2; e++) {
                int idx = e ? k : k - 1;
                int is_start = !pen[idx];
                int is_end   = (idx + 1 >= np) || !pen[idx + 1];
                if (e == 0 && !is_start) continue;
                if (e == 1 && !is_end) continue;
                v2 pe = pts[idx];
                if (pe.y > 0.5f && pe.y < 9.5f) continue;
                int shared = 0;
                for (int m = 0; m < np; m++) {
                    if (m == idx) continue;
                    if (v2_dist(pts[m], pe) < 0.01f) { shared = 1; break; }
                }
                if (shared || fabsf(d.y) < 0.5f) continue;
                v2 gp = V2((pe.x - 3.0f) / 10.0f * H, pe.y / 10.0f * H);
                v2 c  = v2_add(centre, v2_add(v2_scl(right, gp.x), v2_scl(up, gp.y)));
                name_seg(v2_sub(c, v2_scl(right, 0.05f)), v2_add(c, v2_scl(right, 0.05f)), 0.006f);
            }
        }
    }

    /* The star, a filled shape name_dist2d draws, and its flourishes: a long
     * blade either side that sweeps up into a curl and tapers to a point. */
    v2 sc = V2(0.0f, NAME_BASE_Y - 0.38f);
    g_star_c = sc;
    for (int side = -1; side <= 1; side += 2) {
        v2 last = V2(sc.x + (f32)side * 0.25f, sc.y - 0.01f);
        for (int k = 1; k <= 36; k++) {
            f32 t = (f32)k / 36.0f;
            f32 x = 0.25f + 0.78f * t;
            f32 y = -0.01f + 0.10f * sinf(t * DM_PI * 1.15f) * (1.0f - 0.4f * t) - 0.05f * t * t;
            v2  q = V2(sc.x + (f32)side * x, sc.y + y);
            name_seg(last, q, dm_lerp(0.022f, 0.003f, powf(t, 0.8f)));
            last = q;
        }
        /* A shorter blade below, sweeping the other way. */
        last = V2(sc.x + (f32)side * 0.22f, sc.y - 0.06f);
        for (int k = 1; k <= 20; k++) {
            f32 t = (f32)k / 20.0f;
            v2  q = V2(sc.x + (f32)side * (0.22f + 0.42f * t),
                       sc.y - 0.06f - 0.07f * sinf(t * DM_PI) + 0.02f * t);
            name_seg(last, q, dm_lerp(0.014f, 0.003f, t));
            last = q;
        }
    }

    g_name_lo = V2(1e9f, 1e9f);
    g_name_hi = V2(-1e9f, -1e9f);
    for (int i = 0; i < g_name_n; i++) {
        g_name_lo.x = DM_MIN(g_name_lo.x, DM_MIN(g_name[i].a.x, g_name[i].b.x) - 0.05f);
        g_name_lo.y = DM_MIN(g_name_lo.y, DM_MIN(g_name[i].a.y, g_name[i].b.y) - 0.05f);
        g_name_hi.x = DM_MAX(g_name_hi.x, DM_MAX(g_name[i].a.x, g_name[i].b.x) + 0.05f);
        g_name_hi.y = DM_MAX(g_name_hi.y, DM_MAX(g_name[i].a.y, g_name[i].b.y) + 0.05f);
    }
    g_name_lo.y = DM_MIN(g_name_lo.y, g_star_c.y - STAR_R - 0.05f);
}

/* `p` is (across, up) as seen from astern: the ship's z runs the other way. */
static f32 name_dist2d(v2 p)
{
    if (p.x < g_name_lo.x || p.y < g_name_lo.y || p.x > g_name_hi.x || p.y > g_name_hi.y)
        return 1.0f;
    f32 d = sd_star5(v2_sub(p, g_star_c), STAR_R, 0.42f);
    for (int i = 0; i < g_name_n; i++) {
        v2  pa = v2_sub(p, g_name[i].a), ba = v2_sub(g_name[i].b, g_name[i].a);
        f32 h  = dm_sat(v2_dot(pa, ba) / DM_MAX(v2_dot(ba, ba), 1e-9f));
        f32 s  = v2_len(v2_sub(pa, v2_scl(ba, h))) - g_name[i].w;
        if (s < d) d = s;
    }
    return d;
}

/* ---- the rig ------------------------------------------------------------------
 *
 * Every spar belongs to a section, and is written down where it stands on the
 * undamaged ship. A section's placement moves all of its spars together.
 */
enum { SP_TAPER, SP_BOX, SP_CYL };

typedef struct {
    u8  kind;
    u8  sec;            /* RIG_*, or RIG_COUNT for the hull                  */
    u8  part;
    v3  a, b;           /* TAPER: ends. BOX: centre, half size. CYL: centre, (r, half height, 0) */
    f32 ra, rb;
} spar;

#define SEC_HULL  RIG_COUNT
#define MAX_SPARS 96

static spar g_spars[MAX_SPARS];
static int  g_nspars;
/* The spars sorted by section, so the distance only visits a section's own. */
static int  g_sec_first[RIG_COUNT + 1], g_sec_count[RIG_COUNT + 1];

typedef struct {
    u8  parent;         /* RIG_*, or SEC_HULL                                */
    v3  pivot;
    v3  lo, hi;         /* bound of its spars, where they stand              */
} rig_sec;

static rig_sec g_sec[RIG_COUNT];

static void add_taper(u8 sec, u8 part, v3 a, v3 b, f32 ra, f32 rb)
{
    if (g_nspars >= MAX_SPARS) return;
    spar *s = &g_spars[g_nspars++];
    s->kind = SP_TAPER; s->sec = sec; s->part = part;
    s->a = a; s->b = b; s->ra = ra; s->rb = rb;
}

static void add_box(u8 sec, u8 part, v3 c, v3 half)
{
    if (g_nspars >= MAX_SPARS) return;
    spar *s = &g_spars[g_nspars++];
    s->kind = SP_BOX; s->sec = sec; s->part = part;
    s->a = c; s->b = half; s->ra = s->rb = 0.0f;
}

static void add_cyl(u8 sec, u8 part, v3 c, f32 r, f32 half_h)
{
    if (g_nspars >= MAX_SPARS) return;
    spar *s = &g_spars[g_nspars++];
    s->kind = SP_CYL; s->sec = sec; s->part = part;
    s->a = c; s->b = V3(r, half_h, 0.0f); s->ra = s->rb = 0.0f;
}

/* Heights above the deck at each mast, from the plan. */
#define FORE_TOP_H     11.9f
#define FORE_TM_H      19.9f
#define FORE_TRUCK_H   23.8f
#define MAIN_BREAK_H    3.05f   /* ten feet: where she snapped on 28 Oct 1915 */
#define MAIN_TOP_H     12.8f
#define MAIN_TRUCK_H   23.8f
#define MIZ_TOP_H      11.2f
#define MIZ_TRUCK_H    19.2f

static void build_rig(void)
{
    g_nspars = 0;
    f32 fd = deck_y(FORE_X), md = deck_y(MAIN_X), zd = deck_y(MIZ_X);

    /* -- foremast: lower mast, topmast, topgallant, three yards ------------ */
    add_taper(SEC_HULL, SHIP_SPAR, V3(FORE_X, fd - 0.3f, 0), V3(FORE_X, fd + 1.6f, 0), 0.31f, 0.30f);
    add_taper(RIG_FORE_LOWER, SHIP_SPAR, V3(FORE_X, fd + 1.6f, 0), V3(FORE_X, fd + FORE_TOP_H + 1.0f, 0), 0.30f, 0.25f);
    add_box(RIG_FORE_LOWER, SHIP_SPAR_PALE, V3(FORE_X + 0.2f, fd + FORE_TOP_H, 0), V3(0.85f, 0.07f, 1.45f));
    /* The fore yard hangs just under the top, 14.6 m from arm to arm. */
    add_taper(RIG_FORE_LOWER, SHIP_SPAR_PALE, V3(FORE_X + 0.45f, fd + 11.2f, 0), V3(FORE_X + 0.45f, fd + 11.2f, 7.3f), 0.15f, 0.07f);
    add_taper(RIG_FORE_LOWER, SHIP_SPAR_PALE, V3(FORE_X + 0.45f, fd + 11.2f, 0), V3(FORE_X + 0.45f, fd + 11.2f, -7.3f), 0.15f, 0.07f);

    add_taper(RIG_FORE_TOP, SHIP_SPAR_PALE, V3(FORE_X, fd + FORE_TOP_H - 0.8f, 0), V3(FORE_X, fd + FORE_TM_H + 0.6f, 0), 0.19f, 0.14f);
    add_box(RIG_FORE_TOP, SHIP_SPAR_PALE, V3(FORE_X + 0.1f, fd + FORE_TM_H - 0.4f, 0), V3(0.35f, 0.05f, 1.0f));
    add_taper(RIG_FORE_TOP, SHIP_SPAR_PALE, V3(FORE_X + 0.35f, fd + 15.6f, 0), V3(FORE_X + 0.35f, fd + 15.6f, 5.8f), 0.12f, 0.06f);
    add_taper(RIG_FORE_TOP, SHIP_SPAR_PALE, V3(FORE_X + 0.35f, fd + 15.6f, 0), V3(FORE_X + 0.35f, fd + 15.6f, -5.8f), 0.12f, 0.06f);

    add_taper(RIG_FORE_TGALLANT, SHIP_SPAR_PALE, V3(FORE_X, fd + FORE_TM_H - 0.8f, 0), V3(FORE_X, fd + FORE_TRUCK_H, 0), 0.11f, 0.06f);
    add_taper(RIG_FORE_TGALLANT, SHIP_SPAR_PALE, V3(FORE_X + 0.25f, fd + 21.0f, 0), V3(FORE_X + 0.25f, fd + 21.0f, 4.1f), 0.08f, 0.04f);
    add_taper(RIG_FORE_TGALLANT, SHIP_SPAR_PALE, V3(FORE_X + 0.25f, fd + 21.0f, 0), V3(FORE_X + 0.25f, fd + 21.0f, -4.1f), 0.08f, 0.04f);

    /* -- mainmast: lower mast, topmast with the crow's nest, gaff and boom -- */
    add_taper(SEC_HULL, SHIP_SPAR, V3(MAIN_X, md - 0.3f, 0), V3(MAIN_X, md + MAIN_BREAK_H, 0), 0.31f, 0.30f);
    add_taper(RIG_MAIN_LOWER, SHIP_SPAR, V3(MAIN_X, md + MAIN_BREAK_H, 0), V3(MAIN_X, md + MAIN_TOP_H + 0.9f, 0), 0.30f, 0.24f);
    add_box(RIG_MAIN_LOWER, SHIP_SPAR_PALE, V3(MAIN_X, md + MAIN_TOP_H, 0), V3(0.45f, 0.06f, 0.9f));
    add_taper(RIG_MAIN_LOWER, SHIP_SPAR_PALE, V3(MAIN_X - 0.35f, md + HOUSE_H + 1.25f, 0), V3(MAIN_X - 11.0f, md + HOUSE_H + 1.45f, 0), 0.15f, 0.10f);
    add_taper(RIG_MAIN_LOWER, SHIP_SPAR_PALE, V3(MAIN_X - 0.35f, md + 11.8f, 0), V3(MAIN_X - 7.6f, md + 16.6f, 0), 0.13f, 0.08f);

    add_taper(RIG_MAIN_TOP, SHIP_SPAR_PALE, V3(MAIN_X, md + MAIN_TOP_H - 0.8f, 0), V3(MAIN_X, md + MAIN_TRUCK_H, 0), 0.18f, 0.08f);
    /* The crow's nest: a barrel high on the topmast. */
    add_cyl(RIG_MAIN_TOP, SHIP_HOUSE, V3(MAIN_X, md + 17.6f, 0), 0.46f, 0.62f);

    /* -- mizzen: stepped on the after deckhouse ---------------------------- */
    add_taper(SEC_HULL, SHIP_SPAR, V3(MIZ_X, zd + HOUSE_H - 0.2f, 0), V3(MIZ_X, zd + HOUSE_H + 1.0f, 0), 0.27f, 0.26f);
    add_taper(RIG_MIZ_LOWER, SHIP_SPAR, V3(MIZ_X, zd + HOUSE_H + 1.0f, 0), V3(MIZ_X, zd + MIZ_TOP_H + 0.8f, 0), 0.26f, 0.21f);
    add_box(RIG_MIZ_LOWER, SHIP_SPAR_PALE, V3(MIZ_X, zd + MIZ_TOP_H, 0), V3(0.40f, 0.06f, 0.8f));
    /* The mizzen boom runs out past the counter. */
    add_taper(RIG_MIZ_LOWER, SHIP_SPAR_PALE, V3(MIZ_X - 0.3f, zd + HOUSE_H + 1.4f, 0), V3(-23.4f, zd + HOUSE_H + 1.7f, 0), 0.14f, 0.09f);
    add_taper(RIG_MIZ_LOWER, SHIP_SPAR_PALE, V3(MIZ_X - 0.3f, zd + 10.4f, 0), V3(MIZ_X - 6.4f, zd + 14.4f, 0), 0.12f, 0.07f);
    add_taper(RIG_MIZ_TOP, SHIP_SPAR_PALE, V3(MIZ_X, zd + MIZ_TOP_H - 0.7f, 0), V3(MIZ_X, zd + MIZ_TRUCK_H, 0), 0.16f, 0.07f);

    /* -- bowsprit and jib-boom: six metres past the stem ------------------- */
    f32 by = deck_y(17.0f) + FOCSLE_H + 0.15f;
    add_taper(RIG_BOWSPRIT, SHIP_SPAR_PALE, V3(17.0f, by, 0), V3(24.0f, by + 1.25f, 0), 0.30f, 0.20f);
    add_taper(RIG_BOWSPRIT, SHIP_SPAR_PALE, V3(22.8f, by + 1.25f, 0), V3(28.1f, by + 2.20f, 0), 0.15f, 0.08f);

    /* -- the funnel, rising through the after deckhouse --------------------- */
    f32 fy = deck_y(FUNNEL_X);
    add_cyl(RIG_FUNNEL, SHIP_FUNNEL, V3(FUNNEL_X, fy + HOUSE_H + 2.3f, 0), 0.45f, 2.3f);
    add_cyl(RIG_FUNNEL, SHIP_FUNNEL, V3(FUNNEL_X, fy + HOUSE_H + 4.55f, 0), 0.50f, 0.08f);

    /* Sort by section: a counting sort, stable, in place through a copy. */
    {
        static spar tmp[MAX_SPARS];
        int k = 0;
        for (int s = 0; s <= RIG_COUNT; s++) {
            g_sec_first[s] = k;
            for (int i = 0; i < g_nspars; i++)
                if (g_spars[i].sec == s) tmp[k++] = g_spars[i];
            g_sec_count[s] = k - g_sec_first[s];
        }
        memcpy(g_spars, tmp, sizeof(spar) * (size_t)g_nspars);
    }

    /* Break points, and parents. */
    static const u8 PARENT[RIG_COUNT] = {
        SEC_HULL, RIG_FORE_LOWER, RIG_FORE_TOP, SEC_HULL, RIG_MAIN_LOWER,
        SEC_HULL, RIG_MIZ_LOWER, SEC_HULL, SEC_HULL,
    };
    v3 PIVOT[RIG_COUNT] = {
        V3(FORE_X, fd + 1.6f, 0), V3(FORE_X, fd + FORE_TOP_H, 0), V3(FORE_X, fd + FORE_TM_H - 0.4f, 0),
        V3(MAIN_X, md + MAIN_BREAK_H, 0), V3(MAIN_X, md + MAIN_TOP_H, 0),
        V3(MIZ_X, zd + HOUSE_H + 1.0f, 0), V3(MIZ_X, zd + MIZ_TOP_H, 0),
        V3(21.6f, by + 0.8f, 0), V3(FUNNEL_X, fy + HOUSE_H, 0),
    };
    for (int s = 0; s < RIG_COUNT; s++) {
        g_sec[s].parent = PARENT[s];
        g_sec[s].pivot  = PIVOT[s];
        g_sec[s].lo = V3(1e9f, 1e9f, 1e9f);
        g_sec[s].hi = V3(-1e9f, -1e9f, -1e9f);
    }
    for (int i = 0; i < g_nspars; i++) {
        const spar *sp = &g_spars[i];
        if (sp->sec == SEC_HULL) continue;
        v3 lo, hi;
        if (sp->kind == SP_TAPER) {
            f32 r = DM_MAX(sp->ra, sp->rb);
            lo = v3_adds(v3_min(sp->a, sp->b), -r);
            hi = v3_adds(v3_max(sp->a, sp->b), r);
        } else if (sp->kind == SP_BOX) {
            lo = v3_sub(sp->a, sp->b);
            hi = v3_add(sp->a, sp->b);
        } else {
            lo = v3_sub(sp->a, V3(sp->b.x, sp->b.y, sp->b.x));
            hi = v3_add(sp->a, V3(sp->b.x, sp->b.y, sp->b.x));
        }
        g_sec[sp->sec].lo = v3_min(g_sec[sp->sec].lo, lo);
        g_sec[sp->sec].hi = v3_max(g_sec[sp->sec].hi, hi);
    }
}

/* ---- state ------------------------------------------------------------------- */

en_ship_state en_ship_state_default(void)
{
    en_ship_state S;
    memset(&S, 0, sizeof S);
    S.wheelhouse = 1.0f;
    for (int i = 0; i < RIG_COUNT; i++) S.axis[i] = V3(1, 0, 0);
    en_ship_state_prepare(&S);
    return S;
}

void en_ship_state_prepare(en_ship_state *S)
{
    /* Parents come before children in the enum, so one pass is enough. */
    for (int s = 0; s < RIG_COUNT; s++) {
        m3 R = S->angle[s] != 0.0f ? m3_axis_angle(S->axis[s], S->angle[s]) : m3_identity();
        v3 pv = g_sec[s].pivot;
        v3 own_off = v3_add(v3_sub(pv, m3_mul_v3(R, pv)), S->shift[s]);
        u8 par = g_sec[s].parent;
        if (par == SEC_HULL) {
            S->rot[s] = R;
            S->off[s] = own_off;
        } else {
            S->rot[s] = m3_mul(S->rot[par], R);
            S->off[s] = v3_add(m3_mul_v3(S->rot[par], own_off), S->off[par]);
            if (S->gone[par]) S->gone[s] = 1;
        }
    }

    /* The bound of everything, wherever the sections now are. */
    v3 lo = V3(-L2 - 2.0f, -0.2f, -HB_MAX - 2.2f), hi = V3(L2 + 1.0f, 12.0f, HB_MAX + 2.2f);
    for (int s = 0; s < RIG_COUNT; s++) {
        if (S->gone[s] || g_sec[s].lo.x > g_sec[s].hi.x) continue;
        for (int c = 0; c < 8; c++) {
            v3 q = V3((c & 1) ? g_sec[s].hi.x : g_sec[s].lo.x,
                      (c & 2) ? g_sec[s].hi.y : g_sec[s].lo.y,
                      (c & 4) ? g_sec[s].hi.z : g_sec[s].lo.z);
            v3 w = v3_add(m3_mul_v3(S->rot[s], q), S->off[s]);
            lo = v3_min(lo, w);
            hi = v3_max(hi, w);
        }
    }
    S->bound_lo = v3_adds(lo, -0.3f);
    S->bound_hi = v3_adds(hi, 0.3f);
}

void en_ship_bounds(const en_ship_state *S, v3 *lo, v3 *hi)
{
    *lo = S->bound_lo;
    *hi = S->bound_hi;
}

/* ---- the whole ship ------------------------------------------------------------ */

void en_ship_init(void)
{
    build_name();
    build_rig();
}

void en_ship_pose_make(en_ship_pose *P)
{
    /* Heading, then trim about the ship's own z, then heel about its keel. */
    m3 R = m3_mul(m3_rot_y(P->yaw), m3_mul(m3_rot_z(P->trim), m3_rot_x(P->heel)));
    P->rot = R;
}

v3 en_ship_to_local(const en_ship_pose *P, v3 w)
{
    v3 d = v3_sub(w, P->pos);
    return V3(v3_dot(d, P->rot.c[0]), v3_dot(d, P->rot.c[1]), v3_dot(d, P->rot.c[2]));
}

v3 en_ship_dir_to_local(const en_ship_pose *P, v3 d)
{
    return V3(v3_dot(d, P->rot.c[0]), v3_dot(d, P->rot.c[1]), v3_dot(d, P->rot.c[2]));
}

v3 en_ship_to_world(const en_ship_pose *P, v3 l)
{
    return v3_add(P->pos, m3_mul_v3(P->rot, l));
}

v3 en_ship_dir_to_world(const en_ship_pose *P, v3 d)
{
    return m3_mul_v3(P->rot, d);
}

/* ---- the boats ------------------------------------------------------------------
 *
 * Three boats in iron davits over the side, between the fore rigging and the
 * funnel. Which side each hung is not recorded; two to port and one to
 * starboard is a guess, and so are their exact stations.
 */
typedef struct { f32 x, side, len; } boat;
static const boat BOATS[3] = {
    {  7.4f,  1.0f, 6.9f },     /* James Caird, 22 ft 6 in                   */
    { -0.2f,  1.0f, 6.1f },     /* Stancomb Wills                            */
    {  4.6f, -1.0f, 6.7f },     /* Dudley Docker                             */
};

static f32 boat_sdf(v3 p, int *part)
{
    f32 d = 1e9f;
    int pt = SHIP_BOAT;
    for (int i = 0; i < 3; i++) {
        const boat *b = &BOATS[i];
        f32 hb = en_hull_half_breadth(b->x, rail_y(b->x) - 0.4f);
        v3  c  = V3(b->x, rail_y(b->x) + 0.75f, b->side * (hb + 1.05f));
        v3  q  = v3_sub(p, c);
        /* Everything of this boat and its davits is inside this box; far from
         * it, the box is distance enough. */
        f32 bb = en_sd_box(v3_sub(q, V3(0.0f, -0.30f, 0.0f)), V3(b->len * 0.5f + 0.9f, 1.75f, 1.25f));
        if (bb > 0.25f) {
            if (bb < d) d = bb;
            continue;
        }
        /* A whaler's hull: half an ellipsoid, open at the gunwale. */
        v3 r = V3(b->len * 0.5f, 0.78f, 0.92f);
        f32 k0 = v3_len(V3(q.x / r.x, q.y / r.y, q.z / r.z));
        f32 k1 = v3_len(V3(q.x / (r.x * r.x), q.y / (r.y * r.y), q.z / (r.z * r.z)));
        f32 outer = k1 > 1e-9f ? k0 * (k0 - 1.0f) / k1 : -0.5f;
        f32 shell = DM_MAX(DM_MAX(outer, -outer - 0.05f), q.y);
        if (shell < d) { d = shell; pt = SHIP_BOAT; }

        /* The davits: an iron post at each end, curving out over the boat. */
        for (int e = -1; e <= 1; e += 2) {
            f32 dx = (f32)e * (b->len * 0.5f - 0.7f);
            v3 foot = V3(b->x + dx, rail_y(b->x) - 1.1f, b->side * (hb + 0.12f));
            v3 bend = V3(b->x + dx, rail_y(b->x) + 1.5f, b->side * (hb + 0.25f));
            v3 head = V3(b->x + dx, rail_y(b->x) + 2.0f, b->side * (hb + 1.05f));
            f32 dv = DM_MIN(en_sd_capsule(p, foot, bend, 0.06f), en_sd_capsule(p, bend, head, 0.05f));
            if (dv < d) { d = dv; pt = SHIP_IRON; }
        }
    }
    if (part) *part = pt;
    return d;
}

/* ---- the deck ------------------------------------------------------------------- */

static f32 houses_sdf(v3 p, const en_ship_state *S, int *part)
{
    f32 d = 1e9f;
    int pt = SHIP_HOUSE;

    /* The fo'c'sle: the deck raised over the bow, as far aft as FOCSLE_X, with
     * a front bulkhead facing aft. */
    {
        f32 top = deck_y(p.x) + FOCSLE_H;
        f32 f = DM_MAX(DM_MAX(p.y - top, FOCSLE_X - p.x), deck_y(p.x) - 0.2f - p.y);
        f = DM_MAX(f, outline_2d(p.x, p.y, p.z, NULL) + 0.02f);
        if (f < d) { d = f; pt = SHIP_HOUSE; }
    }

    /* The forward deckhouse: wardroom, galley and pantry. */
    {
        f32 h = HOUSE_H - 0.05f;
        v3 c = V3((FWD_HOUSE0 + FWD_HOUSE1) * 0.5f, deck_y(MAIN_X + 5.0f) + h * 0.5f, 0.0f);
        f32 f = en_sd_box(v3_sub(p, c), V3((FWD_HOUSE1 - FWD_HOUSE0) * 0.5f - 0.08f, h * 0.5f - 0.08f, 2.12f)) - 0.08f;
        if (f < d) { d = f; pt = SHIP_HOUSE; }
    }

    /* The after deckhouse, the full width of the after deck. On the wreck it
     * is gone but for its forward bulkhead. */
    {
        f32 cx = (AFT_HOUSE0 + AFT_HOUSE1) * 0.5f, hx = (AFT_HOUSE0 - AFT_HOUSE1) * 0.5f;
        f32 top = deck_y(p.x) + HOUSE_H;
        f32 box = DM_MAX(fabsf(p.x - cx) - hx, p.y - top);
        if (S->wreck) box = DM_MAX(box, fabsf(p.x - AFT_HOUSE0 + 0.15f) - 0.15f);
        /* Inside the bulwarks: the hull's own side, moved in. */
        box = DM_MAX(box, deck_y(p.x) - 0.2f - p.y);
        f32 side = outline_2d(p.x, p.y, p.z, NULL) + 0.30f;
        f32 f = DM_MAX(box, side);
        if (f < d) { d = f; pt = SHIP_HOUSE; }
    }

    /* The wheel-house over the wheel, until it was carried onto the ice. */
    if (S->wheelhouse > 0.5f) {
        v3 c = V3(WHEEL_X - 0.2f, deck_y(WHEEL_X) + 1.1f, 0.0f);
        f32 f = en_sd_box(v3_sub(p, c), V3(1.05f, 1.05f, 1.15f)) - 0.05f;
        if (f < d) { d = f; pt = SHIP_HOUSE; }
    } else {
        /* The wheel itself: a rim, eight spokes, a boss and a pedestal. */
        v3 c = V3(WHEEL_X, deck_y(WHEEL_X) + 1.15f, 0.0f);
        v3 q = v3_sub(p, c);
        f32 bound = en_sd_box(q, V3(0.4f, 1.9f, 1.0f));
        if (bound < d) {
            f32 rr = sqrtf(q.y * q.y + q.z * q.z);
            f32 rim = sqrtf(dm_sq(rr - 0.62f) + q.x * q.x) - 0.035f;
            f32 ang = atan2f(q.z, q.y);
            const f32 sect = DM_TAU / 8.0f;
            f32 a2 = dm_mod(ang + sect * 0.5f, sect) - sect * 0.5f;
            v2  sp = V2(rr * cosf(a2), rr * sinf(a2));
            f32 spoke = DM_MAX(sqrtf(sp.y * sp.y + q.x * q.x) - 0.022f, sp.x - 0.86f);
            f32 handle = sqrtf(dm_sq(DM_MAX(sp.x - 0.86f, 0.0f)) + sp.y * sp.y + q.x * q.x) - 0.03f;
            spoke = DM_MIN(spoke, DM_MAX(handle, sp.x - 0.98f));
            f32 boss = en_sd_capsule(q, V3(-0.12f, 0, 0), V3(0.12f, 0, 0), 0.09f);
            f32 ped  = en_sd_box(v3_sub(q, V3(0.35f, -0.62f, 0.0f)), V3(0.16f, 0.55f, 0.20f));
            f32 w = DM_MIN(DM_MIN(rim, spoke), DM_MIN(boss, ped));
            if (w < d) { d = w; pt = (boss < rim && boss < spoke) ? SHIP_IRON : SHIP_WHEEL; }
        }
    }

    /* Skylights on the house tops, and the capstan on the fo'c'sle. */
    {
        f32 sk = en_sd_box(v3_sub(p, V3(6.8f, deck_y(6.8f) + HOUSE_H + 0.25f, 0.0f)), V3(0.9f, 0.25f, 0.7f)) - 0.03f;
        f32 sk2 = en_sd_box(v3_sub(p, V3(-8.5f, deck_y(-8.5f) + HOUSE_H + 0.22f, 0.0f)), V3(1.2f, 0.22f, 0.8f)) - 0.03f;
        if (S->wreck) sk2 = 1e9f;
        f32 cap = en_sd_box(v3_sub(p, V3(19.0f, deck_y(19.0f) + FOCSLE_H + 0.4f, 0.0f)), V3(0.34f, 0.40f, 0.34f)) - 0.05f;
        if (sk < d)  { d = sk;  pt = SHIP_GLASS; }
        if (sk2 < d) { d = sk2; pt = SHIP_GLASS; }
        if (cap < d) { d = cap; pt = SHIP_IRON; }
    }

    if (part) *part = pt;
    return d;
}

/* ---- the distance ---------------------------------------------------------------- */

static f32 rig_sdf(v3 p, const en_ship_state *S, f32 best, int *part)
{
    f32 d = best;
    int pt = -1;
    for (int s = 0; s <= RIG_COUNT; s++) {
        v3 q = p;
        if (s < RIG_COUNT) {
            if (S->gone[s]) continue;
            v3 r = v3_sub(p, S->off[s]);
            q = V3(v3_dot(r, S->rot[s].c[0]), v3_dot(r, S->rot[s].c[1]), v3_dot(r, S->rot[s].c[2]));
            v3 c = v3_scl(v3_add(g_sec[s].lo, g_sec[s].hi), 0.5f);
            v3 h = v3_scl(v3_sub(g_sec[s].hi, g_sec[s].lo), 0.5f);
            if (en_sd_box(v3_sub(q, c), h) > d) continue;
        }
        for (int i = g_sec_first[s]; i < g_sec_first[s] + g_sec_count[s]; i++) {
            const spar *sp = &g_spars[i];
            f32 v;
            if (sp->kind == SP_TAPER)    v = en_sd_taper(q, sp->a, sp->b, sp->ra, sp->rb);
            else if (sp->kind == SP_BOX) v = en_sd_box(v3_sub(q, sp->a), sp->b) - 0.02f;
            else {
                v3 o = v3_sub(q, sp->a);
                f32 dr = sqrtf(o.x * o.x + o.z * o.z) - sp->b.x;
                f32 dy = fabsf(o.y) - sp->b.y;
                v = DM_MIN(DM_MAX(dr, dy), 0.0f) + sqrtf(dm_sq(DM_MAX(dr, 0.0f)) + dm_sq(DM_MAX(dy, 0.0f)));
            }
            if (v < d) { d = v; pt = sp->part; }
        }
    }
    if (part && pt >= 0) *part = pt;
    return d;
}

/* The hull, the name and everything built on the deck: one component. */
static f32 hull_part_sdf(v3 p, const en_ship_state *S, int *part)
{
    int pt = SHIP_HULL;

    /* Hull, cut off at the rail, with the space inside the bulwarks and above
     * the deck taken out of it. */
    f32 aft;
    f32 side = hull_side(p, &aft);
    f32 top  = p.y - rail_y(p.x);
    f32 hull = DM_MAX(side, top);
    f32 inner = DM_MAX(side + 0.20f, deck_y(p.x) - p.y);
    hull = DM_MAX(hull, -inner);

    /* Keel, and the rudder -- which on the wreck lies under the counter. */
    f32 keel = en_sd_box(v3_sub(p, V3((x_stem(0.4f) + STERNPOST) * 0.5f, 0.25f, 0.0f)),
                         V3((x_stem(0.4f) - STERNPOST) * 0.5f, 0.25f, 0.19f));
    v3  rq = v3_sub(p, V3(STERNPOST - 0.85f, 2.3f, 0.0f));
    if (S->wreck) rq = en_rot_x(v3_sub(p, V3(STERNPOST - 2.2f, 0.3f, 1.2f)), 1.45f);
    f32 rudder = en_sd_box(rq, V3(0.85f, 1.9f, 0.10f));
    hull = DM_MIN(hull, DM_MIN(keel, rudder));

    f32 d = hull;

    if (d < 0.05f && p.y > rail_y(p.x) - 0.10f) pt = SHIP_RAIL;

    /* The name, raised off the face of the counter. */
    if (p.x < -19.5f && p.y > 4.6f && p.y < 6.2f && d < 0.12f && aft > 0.30f && side > -0.08f) {
        f32 dn = name_dist2d(V2(-p.z, p.y));
        f32 relief = DM_MAX(hull - 0.016f, dn);
        if (relief < d) { d = relief; pt = SHIP_GILT; }
    }

    int hp;
    f32 dh = houses_sdf(p, S, &hp);
    if (dh < d) { d = dh; pt = hp; }

    if (part) *part = pt;
    return d;
}

/* The boxes each component lives in, in the frame it is evaluated in. */
static const v3 HULL_LO = { -22.8f, -0.3f, -4.2f }, HULL_HI = { 23.3f, 8.6f, 4.2f };
static const v3 BOAT_LO = { -4.2f, 4.3f, -6.6f },  BOAT_HI = { 11.6f, 9.2f, 6.6f };

f32 en_ship_sdf(v3 p, const en_ship_state *S, int *part)
{
    int pt = SHIP_HULL;
    f32 d;

    /* The hull and everything on its deck fit in one box. Far outside it, the
     * box is a good enough distance, and the lofting is not worth doing. */
    f32 hull_box = en_sd_box(v3_sub(p, v3_scl(v3_add(HULL_LO, HULL_HI), 0.5f)), v3_scl(v3_sub(HULL_HI, HULL_LO), 0.5f));
    if (hull_box > 0.6f) d = hull_box;
    else d = hull_part_sdf(p, S, &pt);

    /* The boats hang outside that box. A bound is only a distance while the
     * point is outside the bound; inside it, ask the boats. */
    if (!S->wreck && !S->no_boats) {
        f32 bb = en_sd_box(v3_sub(p, v3_scl(v3_add(BOAT_LO, BOAT_HI), 0.5f)), v3_scl(v3_sub(BOAT_HI, BOAT_LO), 0.5f));
        if (bb > 0.3f) { if (bb < d) d = bb; }
        else { int bp; f32 db = boat_sdf(p, &bp); if (db < d) { d = db; pt = bp; } }
    }

    d = rig_sdf(p, S, d, &pt);
    if (part) *part = pt;
    return d;
}

/* ---- marching one ray -----------------------------------------------------------
 *
 * A distance function answers for every point in space, and most of what it
 * adds up for a given ray is things the ray will never go near: the mizzen, for
 * a ray that passes between the fore and main masts. So before marching, the
 * ray is tested against the box of each component -- the hull, the boats, each
 * section of the rig -- and at every step only the components whose boxes the
 * ray is inside are asked. The rest contribute the distance along the ray to
 * their box, which is not a distance in space but is exactly as far as this ray
 * can safely go, and that is all a march needs.
 */
#define COMP_HULL  RIG_COUNT
#define COMP_BOATS (RIG_COUNT + 1)
#define NCOMP      (RIG_COUNT + 2)

typedef struct {
    f32 t0[NCOMP], t1[NCOMP];
    v3  o[RIG_COUNT], d[RIG_COUNT];     /* the ray in each section's frame     */
} ship_ray;

static int ship_ray_setup(const en_ship_state *S, v3 ro, v3 dir, f32 t_max, ship_ray *R)
{
    const f32 PAD = 0.15f;
    int live = 0;
    for (int c = 0; c < NCOMP; c++) { R->t0[c] = 1e30f; R->t1[c] = -1e30f; }

    f32 a, b;
    if (en_ray_box(ro, dir, v3_adds(HULL_LO, -PAD), v3_adds(HULL_HI, PAD), &a, &b) && a < t_max) {
        R->t0[COMP_HULL] = DM_MAX(a, 0.0f); R->t1[COMP_HULL] = b; live++;
    }
    if (!S->wreck && !S->no_boats && en_ray_box(ro, dir, v3_adds(BOAT_LO, -PAD), v3_adds(BOAT_HI, PAD), &a, &b) && a < t_max) {
        R->t0[COMP_BOATS] = DM_MAX(a, 0.0f); R->t1[COMP_BOATS] = b; live++;
    }
    for (int s = 0; s < RIG_COUNT; s++) {
        if (S->gone[s] || g_sec[s].lo.x > g_sec[s].hi.x) continue;
        v3 r  = v3_sub(ro, S->off[s]);
        v3 so = V3(v3_dot(r, S->rot[s].c[0]), v3_dot(r, S->rot[s].c[1]), v3_dot(r, S->rot[s].c[2]));
        v3 sd = V3(v3_dot(dir, S->rot[s].c[0]), v3_dot(dir, S->rot[s].c[1]), v3_dot(dir, S->rot[s].c[2]));
        if (en_ray_box(so, sd, v3_adds(g_sec[s].lo, -PAD), v3_adds(g_sec[s].hi, PAD), &a, &b) && a < t_max) {
            R->t0[s] = DM_MAX(a, 0.0f); R->t1[s] = b;
            R->o[s] = so; R->d[s] = sd;
            live++;
        }
    }
    return live > 0;
}

static f32 spar_dist(const spar *sp, v3 q)
{
    if (sp->kind == SP_TAPER) return en_sd_taper(q, sp->a, sp->b, sp->ra, sp->rb);
    if (sp->kind == SP_BOX)   return en_sd_box(v3_sub(q, sp->a), sp->b) - 0.02f;
    v3 o = v3_sub(q, sp->a);
    f32 dr = sqrtf(o.x * o.x + o.z * o.z) - sp->b.x;
    f32 dy = fabsf(o.y) - sp->b.y;
    return DM_MIN(DM_MAX(dr, dy), 0.0f) + sqrtf(dm_sq(DM_MAX(dr, 0.0f)) + dm_sq(DM_MAX(dy, 0.0f)));
}

/* Distance for this ray at t to the components it is inside, and in *skip
 * the distance along the ray to the nearest box still ahead. The two are kept
 * apart because only the first is a surface: a ray closing on the edge of a
 * box has not hit anything. Sets *done once every box is behind. */
static f32 ship_ray_dist(const en_ship_state *S, const ship_ray *R, v3 ro, v3 dir, f32 t,
                         int *part, f32 *skip, int *done)
{
    f32 d = 1e30f;
    int pt = SHIP_HULL;
    int ahead = 0;
    v3  p = v3_add(ro, v3_scl(dir, t));
    *skip = 1e30f;

    for (int c = 0; c < NCOMP; c++) {
        if (R->t1[c] < t) continue;
        ahead = 1;
        if (t < R->t0[c]) { *skip = DM_MIN(*skip, R->t0[c] - t); continue; }

        if (c == COMP_HULL) {
            int hp; f32 v = hull_part_sdf(p, S, &hp);
            if (v < d) { d = v; pt = hp; }
            /* The mast stumps belong to the hull. */
            for (int i = g_sec_first[SEC_HULL]; i < g_sec_first[SEC_HULL] + g_sec_count[SEC_HULL]; i++) {
                f32 w = spar_dist(&g_spars[i], p);
                if (w < d) { d = w; pt = g_spars[i].part; }
            }
        } else if (c == COMP_BOATS) {
            int bp; f32 v = boat_sdf(p, &bp);
            if (v < d) { d = v; pt = bp; }
        } else {
            v3 q = v3_add(R->o[c], v3_scl(R->d[c], t));
            for (int i = g_sec_first[c]; i < g_sec_first[c] + g_sec_count[c]; i++) {
                f32 w = spar_dist(&g_spars[i], q);
                if (w < d) { d = w; pt = g_spars[i].part; }
            }
        }
    }
    *done = !ahead;
    if (part) *part = pt;
    return d;
}

int en_ship_march(const en_ship_state *S, v3 ro, v3 dir, f32 t_max, f32 cone,
                  f32 *t_out, int *part_out)
{
    ship_ray R;
    if (!ship_ray_setup(S, ro, dir, t_max, &R)) return 0;
    f32 t = 1e30f;
    for (int c = 0; c < NCOMP; c++) t = DM_MIN(t, R.t0[c]);
    for (int i = 0; i < 240 && t < t_max; i++) {
        int part, done;
        f32 skip;
        f32 d = ship_ray_dist(S, &R, ro, dir, t, &part, &skip, &done);
        if (done) return 0;
        if (d < t * cone * 0.6f + 0.0004f) {
            *t_out = t;
            *part_out = part;
            return 1;
        }
        t += DM_MIN(d * 0.92f, skip + 0.001f);
    }
    return 0;
}

f32 en_ship_shadow(const en_ship_state *S, v3 p, v3 dir, f32 soft)
{
    ship_ray R;
    if (!ship_ray_setup(S, p, dir, 1e9f, &R)) return 1.0f;
    f32 t = 1e30f;
    for (int c = 0; c < NCOMP; c++) t = DM_MIN(t, R.t0[c]);
    t = DM_MAX(t, 0.02f);
    f32 res = 1.0f;
    for (int i = 0; i < 64; i++) {
        int done;
        f32 skip;
        f32 h = ship_ray_dist(S, &R, p, dir, t, NULL, &skip, &done);
        if (done) break;
        if (h < 0.001f) return 0.0f;
        if (h < 1e29f) res = DM_MIN(res, h / (t * soft + 0.002f));
        t += DM_MIN(DM_MAX(h, 0.02f), skip + 0.001f);
    }
    return dm_sat(res);
}

/* Four taps on a tetrahedron rather than six on the axes: the same gradient to
 * first order for two thirds of the distance evaluations. */
v3 en_ship_normal(v3 p, const en_ship_state *S)
{
    const f32 e = 0.003f;
    v3 k0 = V3( 1, -1, -1), k1 = V3(-1, -1,  1), k2 = V3(-1,  1, -1), k3 = V3( 1,  1,  1);
    v3 n = v3_scl(k0, en_ship_sdf(v3_add(p, v3_scl(k0, e)), S, 0));
    n = v3_add(n, v3_scl(k1, en_ship_sdf(v3_add(p, v3_scl(k1, e)), S, 0)));
    n = v3_add(n, v3_scl(k2, en_ship_sdf(v3_add(p, v3_scl(k2, e)), S, 0)));
    n = v3_add(n, v3_scl(k3, en_ship_sdf(v3_add(p, v3_scl(k3, e)), S, 0)));
    return v3_norm(n);
}

/* ---- surfaces ---------------------------------------------------------------- */

static v3 weathered(v3 base, v3 p, f32 amount, u32 seed)
{
    f32 k = dm_fbm3(v3_scl(p, 2.2f), 3, 2.0f, 0.5f, seed);
    return v3_scl(base, 1.0f + amount * k);
}

v3 en_ship_albedo(v3 p, v3 n, int part, const en_ship_state *S, f32 *rough)
{
    f32 r = 0.6f;
    v3  a;
    if (part == SHIP_HULL && n.y > 0.85f && fabsf(p.y - deck_y(p.x)) < 0.08f) part = SHIP_DECK;
    if (part == SHIP_HOUSE && n.y > 0.85f) part = SHIP_DECK;

    switch (part) {
    case SHIP_GILT:
        a = V3(0.80f, 0.58f, 0.24f);
        r = 0.25f;
        break;
    case SHIP_RAIL:
        a = weathered(V3(0.46f, 0.40f, 0.31f), p, 0.3f, 0x7a11u);
        break;
    case SHIP_DECK: {
        /* Planks a sixth of a metre wide running fore and aft, each its own
         * shade of weathered pine. */
        f32 plank = floorf(p.z / 0.16f);
        f32 k = dm_u32_to_f32(dm_hash2i((i32)plank, (i32)floorf(p.x / 7.3f + plank * 0.37f), 0xdec4u));
        a = v3_scl(V3(0.46f, 0.40f, 0.32f), 0.75f + 0.35f * k);
        f32 seam = dm_smoothstep(0.004f, 0.0f, fabsf(dm_fract(p.z / 0.16f) - 0.5f) * 0.16f - 0.074f);
        a = v3_scl(a, 1.0f - 0.6f * seam);
        break;
    }
    case SHIP_HOUSE:
        a = weathered(V3(0.30f, 0.20f, 0.12f), p, 0.35f, 0x4055eu);
        break;
    case SHIP_SPAR:
        a = weathered(V3(0.30f, 0.21f, 0.13f), p, 0.25f, 0x5a4u);
        break;
    case SHIP_SPAR_PALE:
        a = weathered(V3(0.47f, 0.41f, 0.32f), p, 0.25f, 0x5a5u);
        break;
    case SHIP_FUNNEL:
        a = weathered(V3(0.04f, 0.04f, 0.04f), p, 0.4f, 0xf00u);
        r = 0.5f;
        break;
    case SHIP_BOAT:
        a = weathered(V3(0.60f, 0.60f, 0.57f), p, 0.2f, 0xb0a7u);
        break;
    case SHIP_IRON:
        a = V3(0.07f, 0.07f, 0.075f);
        r = 0.45f;
        break;
    case SHIP_GLASS:
        a = V3(0.03f, 0.035f, 0.04f);
        r = 0.1f;
        break;
    case SHIP_WHEEL:
        a = weathered(V3(0.36f, 0.22f, 0.12f), p, 0.3f, 0x3eeu);
        break;
    default: {
        /* Black paint over planking. The strakes show through as faint lines;
         * a pale band runs along the sheer and another, a rubbing strake, a
         * metre below it, as Hurley's photographs show. */
        f32 strake = dm_smoothstep(0.012f, 0.0f, fabsf(dm_fract(p.y / 0.24f) - 0.5f) * 0.24f - 0.108f);
        a = V3(0.035f, 0.034f, 0.033f);
        f32 wear = dm_sat(dm_fbm3(V3(p.x * 1.6f, p.y * 5.0f, p.z * 1.6f), 3, 2.0f, 0.5f, 0x8a11u) * 1.2f)
                   * dm_smoothstep(SHIP_DRAUGHT - 0.2f, SHIP_DRAUGHT + 0.5f, p.y)
                   * dm_smoothstep(SHIP_DRAUGHT + 1.6f, SHIP_DRAUGHT + 0.8f, p.y);
        a = v3_lerp(a, V3(0.16f, 0.13f, 0.10f), wear * 0.35f);
        a = v3_scl(a, 1.0f - 0.30f * strake);
        f32 ry = rail_y(p.x);
        f32 band = dm_pulse(ry - 0.34f, ry - 0.22f, 0.01f, p.y)
                 + dm_pulse(ry - 1.40f, ry - 1.30f, 0.01f, p.y);
        a = v3_lerp(a, V3(0.55f, 0.52f, 0.46f), dm_sat(band) * (p.x > -20.5f ? 1.0f : 0.0f));
        r = 0.45f;
        break;
    }
    }

    /* Rime: frost on whatever faces up and out, heaviest on thin things. */
    if (S->rime > 0.0f && part != SHIP_GLASS && part != SHIP_GILT) {
        /* Frost settles on what faces up and grows on what is thin. A wall
         * takes a bloom of hoar that dulls the paint without hiding it. */
        f32 up = dm_sat(n.y);
        f32 k = S->rime * (0.12f + 0.80f * up * up) *
                (0.75f + 0.25f * dm_sat(dm_fbm3(v3_scl(p, 3.0f), 2, 2.0f, 0.5f, 0x7131u) + 0.5f));
        if (part == SHIP_SPAR || part == SHIP_SPAR_PALE || part == SHIP_IRON) k = DM_MAX(k, S->rime * 0.55f);
        a = v3_lerp(a, V3(0.86f, 0.90f, 0.94f), dm_sat(k));
    }

    /* A hundred years at the bottom. No shipworm has ever reached these
     * waters, so the timber is sound; what has changed is the paint, which is
     * mostly gone to bare dark oak, and the fine grey silt that has settled on
     * every surface that faces up. The gilt is still gilt. */
    if (S->wreck && part != SHIP_GILT) {
        f32 k = dm_fbm3(v3_scl(p, 1.7f), 3, 2.0f, 0.5f, 0x3ec4u);
        v3 oak = v3_scl(V3(0.20f, 0.14f, 0.09f), 0.8f + 0.4f * dm_sat(k + 0.5f));
        f32 bare = part == SHIP_HULL ? dm_sat(0.55f + 0.9f * k) : 0.85f;
        a = v3_lerp(a, oak, bare);
        f32 silt = dm_smoothstep(0.2f, 0.9f, n.y) * (0.65f + 0.35f * dm_sat(dm_fbm3(v3_scl(p, 5.0f), 2, 2.0f, 0.5f, 0x5117u) + 0.5f));
        a = v3_lerp(a, V3(0.46f, 0.43f, 0.37f), silt * 0.85f);
        if (part == SHIP_RAIL) a = v3_lerp(a, V3(0.52f, 0.48f, 0.40f), 0.6f);
    }
    if (rough) *rough = r;
    return a;
}

v3 en_ship_emission(v3 p, v3 n, int part, const en_ship_state *S)
{
    if (S->lights <= 0.0f) return V3(0, 0, 0);
    v3 warm = V3(1.00f, 0.62f, 0.30f);

    /* The skylights over the wardroom and the cabins: lamplight from below. */
    if (part == SHIP_GLASS && n.y > 0.5f)
        return v3_scl(warm, 3.0e-5f * S->lights);

    /* Windows along the sides of the deckhouses, a pane every metre and a
     * half, where the house wall faces out. */
    if (part == SHIP_HOUSE && fabsf(n.z) > 0.7f && fabsf(n.y) < 0.3f) {
        f32 base = deck_y(p.x);
        f32 wy = p.y - base;
        int in_fwd = p.x > FWD_HOUSE0 + 0.4f && p.x < FWD_HOUSE1 - 0.4f;
        int in_aft = p.x < AFT_HOUSE0 - 0.4f && p.x > AFT_HOUSE1 + 0.4f;
        if ((in_fwd || in_aft) && wy > 1.05f && wy < 1.55f) {
            f32 u = dm_fract(p.x / 1.5f);
            if (u > 0.35f && u < 0.65f) {
                u32 h = dm_hash2i((i32)floorf(p.x / 1.5f), p.z > 0.0f ? 1 : 0, 0x3a3u);
                f32 lit = (h & 3u) ? 1.0f : 0.15f;
                return v3_scl(warm, 5.0e-5f * lit * S->lights);
            }
        }
    }
    return V3(0, 0, 0);
}

/* ---- rigging --------------------------------------------------------------- */

typedef struct { u8 sa, sb; v3 a, b; f32 r; } rope;

#define MAX_ROPES 640
static rope g_ropes[MAX_ROPES];
static int  g_nropes = -1;

static void add_rope(u8 sa, v3 a, u8 sb, v3 b, f32 r)
{
    if (g_nropes >= MAX_ROPES) return;
    g_ropes[g_nropes].sa = sa; g_ropes[g_nropes].a = a;
    g_ropes[g_nropes].sb = sb; g_ropes[g_nropes].b = b;
    g_ropes[g_nropes].r = r;
    g_nropes++;
}

/* Shrouds from the channels at the rail to a masthead, with ratlines across
 * them, on both sides. */
static void shroud_set(u8 sec, f32 mx, f32 head_y, f32 head_z, int count, f32 spread, int ratlines)
{
    f32 foot_y = rail_y(mx) - 0.25f;
    f32 hb = en_hull_half_breadth(mx, foot_y) + 0.28f;
    for (int side = -1; side <= 1; side += 2) {
        v3 feet[6];
        for (int k = 0; k < count && k < 6; k++) {
            feet[k] = V3(mx - spread * 0.7f + spread * (f32)k / (f32)DM_MAX(count - 1, 1), foot_y, (f32)side * hb);
            add_rope(SEC_HULL, feet[k], sec, V3(mx, head_y, (f32)side * head_z), 0.014f);
        }
        if (ratlines) {
            v3 head = V3(mx, head_y, (f32)side * head_z);
            for (f32 y = foot_y + 0.45f; y < head_y - 1.4f; y += 0.40f) {
                f32 t = (y - foot_y) / (head_y - foot_y);
                v3 a = v3_lerp(feet[0], head, t);
                v3 b = v3_lerp(feet[count - 1], head, t);
                add_rope(SEC_HULL, a, SEC_HULL, b, 0.008f);
            }
        }
    }
}

static void build_ropes(void)
{
    g_nropes = 0;
    f32 fd = deck_y(FORE_X), md = deck_y(MAIN_X), zd = deck_y(MIZ_X);
    f32 by = deck_y(17.0f) + FOCSLE_H + 0.15f;

    /* Lower shrouds. Ratlines are drawn as belonging to the hull, so a mast
     * that falls leaves them standing -- which is what shrouds did until the
     * ice snapped them. */
    shroud_set(RIG_FORE_LOWER, FORE_X, fd + FORE_TOP_H - 0.3f, 0.32f, 4, 2.4f, 1);
    shroud_set(RIG_MAIN_LOWER, MAIN_X, md + MAIN_TOP_H - 0.3f, 0.32f, 4, 2.4f, 1);
    shroud_set(RIG_MIZ_LOWER,  MIZ_X,  zd + MIZ_TOP_H - 0.3f,  0.28f, 3, 1.8f, 0);

    /* Topmast and topgallant shrouds, from the rim of the top up. */
    for (int side = -1; side <= 1; side += 2) {
        f32 s = (f32)side;
        for (int k = 0; k < 3; k++) {
            add_rope(RIG_FORE_LOWER, V3(FORE_X - 0.4f + 0.4f * (f32)k, fd + FORE_TOP_H, s * 1.40f),
                     RIG_FORE_TOP, V3(FORE_X, fd + FORE_TM_H - 0.5f, s * 0.20f), 0.011f);
        }
        for (int k = 0; k < 2; k++) {
            add_rope(RIG_FORE_TOP, V3(FORE_X - 0.2f + 0.4f * (f32)k, fd + FORE_TM_H - 0.4f, s * 0.95f),
                     RIG_FORE_TGALLANT, V3(FORE_X, fd + FORE_TRUCK_H - 1.3f, s * 0.10f), 0.009f);
            add_rope(RIG_MAIN_LOWER, V3(MAIN_X - 0.2f + 0.4f * (f32)k, md + MAIN_TOP_H, s * 0.85f),
                     RIG_MAIN_TOP, V3(MAIN_X, md + MAIN_TRUCK_H - 1.2f, s * 0.12f), 0.010f);
            add_rope(RIG_MIZ_LOWER, V3(MIZ_X - 0.2f + 0.4f * (f32)k, zd + MIZ_TOP_H, s * 0.75f),
                     RIG_MIZ_TOP, V3(MIZ_X, zd + MIZ_TRUCK_H - 1.0f, s * 0.10f), 0.009f);
        }
        /* Topmast backstays to the channels abaft each mast. */
        f32 hbf = en_hull_half_breadth(FORE_X - 3.5f, rail_y(FORE_X - 3.5f) - 0.3f) + 0.28f;
        add_rope(RIG_FORE_TOP, V3(FORE_X, fd + FORE_TM_H - 0.3f, 0), SEC_HULL, V3(FORE_X - 3.5f, rail_y(FORE_X - 3.5f) - 0.25f, s * hbf), 0.012f);
        f32 hbm = en_hull_half_breadth(MAIN_X - 3.5f, rail_y(MAIN_X - 3.5f) - 0.3f) + 0.28f;
        add_rope(RIG_MAIN_TOP, V3(MAIN_X, md + MAIN_TRUCK_H - 1.0f, 0), SEC_HULL, V3(MAIN_X - 3.5f, rail_y(MAIN_X - 3.5f) - 0.25f, s * hbm), 0.012f);
        f32 hbz = en_hull_half_breadth(MIZ_X - 3.0f, rail_y(MIZ_X - 3.0f) - 0.3f) + 0.28f;
        add_rope(RIG_MIZ_TOP, V3(MIZ_X, zd + MIZ_TRUCK_H - 0.8f, 0), SEC_HULL, V3(MIZ_X - 3.0f, rail_y(MIZ_X - 3.0f) - 0.25f, s * hbz), 0.011f);

        /* Braces: from each yardarm aft to the mainmast. */
        add_rope(RIG_FORE_LOWER, V3(FORE_X + 0.45f, fd + 11.2f, s * 7.2f), RIG_MAIN_LOWER, V3(MAIN_X, md + 9.0f, s * 0.3f), 0.010f);
        add_rope(RIG_FORE_TOP, V3(FORE_X + 0.35f, fd + 15.6f, s * 5.7f), RIG_MAIN_LOWER, V3(MAIN_X, md + 12.0f, s * 0.3f), 0.009f);
        add_rope(RIG_FORE_TGALLANT, V3(FORE_X + 0.25f, fd + 21.0f, s * 4.0f), RIG_MAIN_TOP, V3(MAIN_X, md + 17.0f, s * 0.2f), 0.008f);
        /* Lifts: yardarm to the mast above it. */
        add_rope(RIG_FORE_LOWER, V3(FORE_X + 0.45f, fd + 11.2f, s * 7.1f), RIG_FORE_LOWER, V3(FORE_X, fd + FORE_TOP_H + 0.6f, s * 0.2f), 0.008f);
        add_rope(RIG_FORE_TOP, V3(FORE_X + 0.35f, fd + 15.6f, s * 5.6f), RIG_FORE_TOP, V3(FORE_X, fd + FORE_TM_H - 0.2f, s * 0.15f), 0.007f);
        /* Footropes under each yard. */
        add_rope(RIG_FORE_LOWER, V3(FORE_X + 0.75f, fd + 10.5f, s * 0.8f), RIG_FORE_LOWER, V3(FORE_X + 0.75f, fd + 10.65f, s * 6.8f), 0.007f);
        add_rope(RIG_FORE_TOP, V3(FORE_X + 0.65f, fd + 14.95f, s * 0.6f), RIG_FORE_TOP, V3(FORE_X + 0.65f, fd + 15.05f, s * 5.4f), 0.006f);

        /* The davit falls, down to each boat. */
        for (int i = 0; i < 3; i++) {
            if (BOATS[i].side != s) continue;
            f32 hb = en_hull_half_breadth(BOATS[i].x, rail_y(BOATS[i].x) - 0.4f);
            for (int e = -1; e <= 1; e += 2) {
                f32 dx = (f32)e * (BOATS[i].len * 0.5f - 0.7f);
                add_rope(SEC_HULL, V3(BOATS[i].x + dx, rail_y(BOATS[i].x) + 2.0f, s * (hb + 1.05f)),
                         SEC_HULL, V3(BOATS[i].x + dx, rail_y(BOATS[i].x) + 0.8f, s * (hb + 1.05f)), 0.012f);
            }
        }
    }

    /* Stays, fore and aft. */
    add_rope(RIG_FORE_LOWER, V3(FORE_X, fd + FORE_TOP_H - 0.2f, 0), RIG_BOWSPRIT, V3(21.4f, by + 0.75f, 0), 0.022f);
    add_rope(RIG_FORE_TOP, V3(FORE_X, fd + FORE_TM_H - 0.3f, 0), RIG_BOWSPRIT, V3(24.2f, by + 1.30f, 0), 0.016f);
    add_rope(RIG_FORE_TOP, V3(FORE_X, fd + FORE_TM_H - 1.2f, 0), RIG_BOWSPRIT, V3(26.2f, by + 1.70f, 0), 0.013f);
    add_rope(RIG_FORE_TGALLANT, V3(FORE_X, fd + FORE_TRUCK_H - 1.2f, 0), RIG_BOWSPRIT, V3(28.0f, by + 2.18f, 0), 0.011f);
    add_rope(RIG_MAIN_LOWER, V3(MAIN_X, md + MAIN_TOP_H - 0.3f, 0), SEC_HULL, V3(FORE_X - 0.6f, fd + 1.4f, 0), 0.020f);
    add_rope(RIG_MAIN_TOP, V3(MAIN_X, md + MAIN_TRUCK_H - 1.5f, 0), RIG_FORE_LOWER, V3(FORE_X, fd + FORE_TOP_H + 0.5f, 0), 0.014f);
    add_rope(RIG_MIZ_LOWER, V3(MIZ_X, zd + MIZ_TOP_H - 0.3f, 0), RIG_MAIN_LOWER, V3(MAIN_X, md + 5.5f, 0), 0.017f);
    add_rope(RIG_MIZ_TOP, V3(MIZ_X, zd + MIZ_TRUCK_H - 1.3f, 0), RIG_MAIN_TOP, V3(MAIN_X, md + MAIN_TOP_H + 2.0f, 0), 0.012f);

    /* Bobstay and martingale under the bowsprit. */
    add_rope(RIG_BOWSPRIT, V3(24.0f, by + 1.25f, 0), SEC_HULL, V3(21.6f, 3.6f, 0), 0.020f);
    add_rope(RIG_BOWSPRIT, V3(28.0f, by + 2.18f, 0), RIG_BOWSPRIT, V3(23.8f, by - 0.4f, 0), 0.014f);

    /* Gaff peaks: the peak halyards and vangs. */
    add_rope(RIG_MAIN_LOWER, V3(MAIN_X - 7.6f, md + 16.6f, 0), RIG_MAIN_TOP, V3(MAIN_X, md + 16.0f, 0), 0.010f);
    add_rope(RIG_MAIN_LOWER, V3(MAIN_X - 4.0f, md + 14.1f, 0), RIG_MAIN_LOWER, V3(MAIN_X, md + 13.3f, 0), 0.010f);
    add_rope(RIG_MIZ_LOWER, V3(MIZ_X - 6.4f, zd + 14.4f, 0), RIG_MIZ_TOP, V3(MIZ_X, zd + 13.8f, 0), 0.009f);
    add_rope(RIG_MAIN_LOWER, V3(MAIN_X - 7.6f, md + 16.6f, 0), SEC_HULL, V3(MAIN_X - 9.5f, rail_y(MAIN_X - 9.5f), -3.3f), 0.008f);
    add_rope(RIG_MIZ_LOWER, V3(MIZ_X - 6.4f, zd + 14.4f, 0), SEC_HULL, V3(MIZ_X - 7.5f, rail_y(MIZ_X - 7.5f), 3.0f), 0.008f);
    /* Topping lifts to the boom ends. */
    add_rope(RIG_MAIN_LOWER, V3(MAIN_X, md + MAIN_TOP_H - 0.4f, 0), RIG_MAIN_LOWER, V3(MAIN_X - 10.8f, md + HOUSE_H + 1.45f, 0), 0.010f);
    add_rope(RIG_MIZ_LOWER, V3(MIZ_X, zd + MIZ_TOP_H - 0.4f, 0), RIG_MIZ_LOWER, V3(-23.2f, zd + HOUSE_H + 1.7f, 0), 0.010f);
}

static v3 place(const en_ship_state *S, u8 sec, v3 p)
{
    if (sec == SEC_HULL) return p;
    return v3_add(m3_mul_v3(S->rot[sec], p), S->off[sec]);
}

int en_ship_rigging(const en_ship_state *S, en_line *out, int cap)
{
    if (g_nropes < 0) build_ropes();
    f32 thick = 1.0f + 1.8f * S->rime;
    int n = 0;
    for (int i = 0; i < g_nropes && n < cap; i++) {
        const rope *r = &g_ropes[i];
        if ((r->sa != SEC_HULL && S->gone[r->sa]) || (r->sb != SEC_HULL && S->gone[r->sb])) continue;
        v3 a = place(S, r->sa, r->a), b = place(S, r->sb, r->b);
        /* A rope stretched past its length has parted. */
        f32 rest = v3_dist(r->a, r->b), now = v3_dist(a, b);
        if (now > rest * 1.03f + 0.1f) continue;
        out[n].a = a; out[n].b = b;
        out[n].radius = r->r * thick;
        out[n].ca = out[n].cb = V3(1, 1, 1);
        out[n].alpha = 1.0f;
        n++;
    }
    /* The poles the two lamps hang from, when they are rigged. */
    if (S->lights > 0.0f) {
        for (int side = -1; side <= 1; side += 2) {
            f32 x = 1.0f + 0.0f * (f32)side;
            f32 hb = en_hull_half_breadth(x, rail_y(x) - 0.3f);
            if (n < cap) {
                out[n].a = V3(x, rail_y(x) - 0.2f, (f32)side * (hb + 0.1f));
                out[n].b = V3(x, rail_y(x) + 2.6f, (f32)side * (hb + 4.4f));
                out[n].radius = 0.05f; out[n].ca = out[n].cb = V3(1, 1, 1); out[n].alpha = 1.0f;
                n++;
            }
        }
    }
    return n;
}

int en_ship_lamps(const en_ship_state *S, en_ship_lamp *out, int cap)
{
    int n = 0;
    if (S->lights > 0.0f) {
        /* Two lamps on poles over the side, to port and starboard, "which
         * would illuminate the dogloos brilliantly on the darkest winter's
         * day". Incandescent, so warm. */
        for (int side = -1; side <= 1 && n < cap; side += 2) {
            f32 x = 1.0f;
            f32 hb = en_hull_half_breadth(x, rail_y(x) - 0.3f);
            out[n].pos = V3(x, rail_y(x) + 2.5f, (f32)side * (hb + 4.5f));
            out[n].dir = v3_norm(V3(0.0f, -0.80f, (f32)side * 0.60f));
            out[n].cos_outer = cosf(70.0f * DM_D2R);
            out[n].cos_inner = cosf(35.0f * DM_D2R);
            /* In the units the sun is 20 in. Bright enough to throw a pool on the ice
             * thirty times the moonlight, which is how a lamp reads at night. */
            out[n].power = v3_scl(V3(1.00f, 0.70f, 0.40f), 0.010f * S->lights);
            n++;
        }
    }
    if (S->stern_light > 0.0f && n < cap) {
        /* The electric light at the stern, the night she was abandoned. */
        out[n].pos = V3(-21.0f, rail_y(-21.0f) + 0.9f, 0.0f);
        out[n].dir = V3(0.0f, -1.0f, 0.0f);
        out[n].cos_outer = -0.3f;
        out[n].cos_inner = 0.2f;
        out[n].power = v3_scl(V3(1.00f, 0.66f, 0.34f), 0.06f * S->stern_light);
        n++;
    }
    return n;
}
