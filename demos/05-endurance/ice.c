/* ice.c -- floes, seams, ridges and snow, as one height field.
 *
 * Three scales, and each is a different physical thing rather than another
 * octave of the same noise. Floes are Voronoi cells a hundred and forty metres
 * across, each with its own thickness, so the seams between them are steps.
 * A seam is one of three things, chosen by the pair of floes on either side of
 * it: a lead that has frozen over, a crack that has closed, or a ridge where
 * the two floes were driven together and the ice between them had nowhere to
 * go but up. And on top of every floe, snow, ribbed across the wind.
 *
 * The cost of a height field is in the rays that graze it, and from a camera
 * a metre and a half above the pack nearly every ray below the horizon grazes.
 * So the surface a ray marches against depends on how far away it is. Close
 * to the lens it is everything, down to the ribs in the snow. Further out it
 * is the floes, the drifts and the ridges, and the ribs are only in the
 * shading normal -- a rib ten centimetres high a kilometre away is smaller
 * than a pixel as a shape, but it still decides how bright that pixel is.
 */
#include "ice.h"
#include "../../engine/dm_job.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define FLOE_CELL   140.0f
#define H_MAX         3.8f      /* nothing on the pack is taller than this   */
#define SNOW_MAX      0.80f     /* nothing but a ridge is taller than this   */
#define RIDGE_REACH  10.5f      /* the widest a ridge's footprint can be     */
#define FAR_FLAT   5000.0f

/* The wind that shaped the snow: from the south-south-west. The ribs of the
 * sastrugi lie across it. */
static const v2 WIND = { 0.34f, 0.94f };

static u32 pair_hash(u32 a, u32 b)
{
    if (a > b) { u32 t = a; a = b; b = t; }
    return dm_hash_u32(a * 0x9e3779b1u ^ dm_hash_u32(b + 0x7f4a7c15u));
}

/* ---- two-dimensional noise ------------------------------------------------
 *
 * The engine's noise is three-dimensional. A height field only ever asks in
 * two, and four corners cost half of what eight do. */
static inline f32 grad2(u32 h, f32 x, f32 y)
{
    switch (h & 7u) {
    case 0: return  x + y;  case 1: return  x - y;
    case 2: return -x + y;  case 3: return -x - y;
    case 4: return  x;      case 5: return -x;
    case 6: return  y;      default: return -y;
    }
}

static f32 noise2(f32 x, f32 y, u32 seed)
{
    f32 fx = floorf(x), fy = floorf(y);
    i32 X = (i32)fx, Y = (i32)fy;
    f32 u = x - fx, v = y - fy;
    f32 su = u * u * u * (u * (u * 6.0f - 15.0f) + 10.0f);
    f32 sv = v * v * v * (v * (v * 6.0f - 15.0f) + 10.0f);
    f32 a = grad2(dm_hash2i(X,     Y,     seed), u,        v);
    f32 b = grad2(dm_hash2i(X + 1, Y,     seed), u - 1.0f, v);
    f32 c = grad2(dm_hash2i(X,     Y + 1, seed), u,        v - 1.0f);
    f32 d = grad2(dm_hash2i(X + 1, Y + 1, seed), u - 1.0f, v - 1.0f);
    return dm_lerp(dm_lerp(a, b, su), dm_lerp(c, d, su), sv) * 0.7f;
}

static f32 fbm2(f32 x, f32 y, int oct, u32 seed)
{
    f32 s = 0.0f, a = 0.5f;
    for (int i = 0; i < oct; i++) {
        s += a * noise2(x, y, seed + (u32)i * 0x9e3779b9u);
        x = x * 2.03f + 1.7f; y = y * 2.03f - 3.1f;
        a *= 0.5f;
    }
    return s;
}

static f32 ridge2(f32 x, f32 y, int oct, u32 seed)
{
    f32 s = 0.0f, a = 0.6f, prev = 1.0f;
    for (int i = 0; i < oct; i++) {
        f32 n = 1.0f - fabsf(noise2(x, y, seed + (u32)i * 0x9e3779b9u) * 1.4f);
        n = n * n * prev;
        prev = n;
        s += a * n;
        x = x * 2.1f + 5.3f; y = y * 2.1f - 1.9f;
        a *= 0.45f;
    }
    return s;
}

/* ---- floes ---------------------------------------------------------------- */

typedef struct {
    f32 edge;       /* distance to the seam with the nearest neighbour       */
    u32 id1, id2;   /* the floe we are on, and the one across the seam       */
} floe_hit;

static floe_hit floes(f32 x, f32 z)
{
    f32 px = x / FLOE_CELL, pz = z / FLOE_CELL;
    i32 ix = (i32)floorf(px), iz = (i32)floorf(pz);
    f32 d1 = 1e9f, d2 = 1e9f;
    f32 c1x = 0, c1z = 0, c2x = 0, c2z = 0;
    u32 h1 = 0, h2 = 0;

    /* Jitter kept to the middle seven tenths of a cell: no floe is a sliver,
     * and the two nearest centres are always inside the three by three block. */
    for (int j = -1; j <= 1; j++)
    for (int i = -1; i <= 1; i++) {
        u32 h  = dm_hash2i(ix + i, iz + j, 0xf10eu);
        f32 cx = (f32)(ix + i) + 0.15f + 0.7f * dm_u32_to_f32(h);
        f32 cz = (f32)(iz + j) + 0.15f + 0.7f * dm_u32_to_f32(dm_hash_u32(h ^ 0x68e31da4u));
        f32 dx = px - cx, dz = pz - cz;
        f32 d  = dx * dx + dz * dz;
        if (d < d1)      { d2 = d1; c2x = c1x; c2z = c1z; h2 = h1; d1 = d; c1x = cx; c1z = cz; h1 = h; }
        else if (d < d2) { d2 = d; c2x = cx; c2z = cz; h2 = h; }
    }

    floe_hit f;
    f32 len = sqrtf((c2x - c1x) * (c2x - c1x) + (c2z - c1z) * (c2z - c1z));
    /* The distance to the bisector of the two nearest centres, which is the
     * seam between those two floes, exactly. */
    f.edge = (d2 - d1) / DM_MAX(2.0f * len, 1e-6f) * FLOE_CELL;
    f.id1  = h1;
    f.id2  = h2;
    return f;
}

/* ---- rubble --------------------------------------------------------------------
 *
 * The blocks in a ridge, as a height field: on a grid of a metre and a half,
 * one block per cell, turned and tilted at random, and the surface is whichever
 * block is highest over the point. Their sides are steep because broken ice is.
 */
static f32 rubble(f32 x, f32 z, f32 sail, f32 *top_frac)
{
    const f32 CELL = 1.45f;
    f32 gx = x / CELL, gz = z / CELL;
    i32 ix = (i32)floorf(gx), iz = (i32)floorf(gz);
    f32 best = -1.0f;
    f32 frac = 0.0f;

    for (int j = -1; j <= 1; j++)
    for (int i = -1; i <= 1; i++) {
        u32 h  = dm_hash2i(ix + i, iz + j, 0xb10cu);
        f32 r0 = dm_u32_to_f32(h);
        f32 r1 = dm_u32_to_f32(dm_hash_u32(h ^ 0x1u));
        f32 r2 = dm_u32_to_f32(dm_hash_u32(h ^ 0x2u));
        f32 r3 = dm_u32_to_f32(dm_hash_u32(h ^ 0x3u));
        f32 r4 = dm_u32_to_f32(dm_hash_u32(h ^ 0x4u));

        f32 cx = ((f32)(ix + i) + 0.2f + 0.6f * r0) * CELL;
        f32 cz = ((f32)(iz + j) + 0.2f + 0.6f * r1) * CELL;
        f32 th = r2 * DM_PI;
        f32 ct = cosf(th), st = sinf(th);
        f32 qx = (x - cx) * ct - (z - cz) * st;
        f32 qz = (x - cx) * st + (z - cz) * ct;
        f32 ex = 0.45f + 0.55f * r3, ez = 0.30f + 0.40f * r4;
        f32 dx = fabsf(qx) - ex, dz = fabsf(qz) - ez;
        f32 ox = DM_MAX(dx, 0.0f), oz = DM_MAX(dz, 0.0f);
        f32 db = sqrtf(ox * ox + oz * oz) + DM_MIN(DM_MAX(dx, dz), 0.0f);
        if (db > 0.12f) continue;

        /* A slab stood on edge by the pressure, leaning, its top face tilted. */
        f32 tilt = (r3 - 0.5f) * 0.9f;
        f32 topv = sail * (0.35f + 0.85f * r4) + tilt * qx;
        f32 hh   = topv - DM_MAX(db, 0.0f) * 9.0f;
        if (hh > best) { best = hh; frac = dm_sat(-db / 0.05f); }
    }
    *top_frac = frac;
    return best;
}

/* ---- the surface ---------------------------------------------------------------- */

typedef struct {
    int mat;
    f32 slope;      /* the steepest the surface can be around here           */
    f32 free;       /* horizontal distance with no ridge in it               */
} surf_info;

/* `lod` is the size of the smallest shape worth having: the ribs in the snow
 * need a lod under five centimetres, the rubble under forty. */
static f32 surface(const en_ice *I, f32 x, f32 z, f32 lod, surf_info *si)
{
    int m = ICE_SNOW;
    f32 slope = 0.06f;

    floe_hit F = floes(x, z);
    f32 thick = 0.26f + 0.22f * dm_u32_to_f32(dm_hash_u32(F.id1 ^ 0x5a5au));
    f32 h = thick;

    f32 along  = x * WIND.x + z * WIND.y;
    f32 across = -x * WIND.y + z * WIND.x;
    h += 0.16f * fbm2(along / 38.0f, across / 14.0f, 2, 0xd41fu);

    if (lod < 0.06f) {
        f32 k = dm_smoothstep(0.06f, 0.03f, lod);
        h += 0.10f * k * ridge2(along / 5.5f, across / 0.9f, 2, 0x5a57u);
        slope = 0.40f;
        if (lod < 0.015f)
            h += 0.010f * noise2(x / 0.09f, z / 0.09f, 0x7e7u) * dm_smoothstep(0.015f, 0.008f, lod);
    }

    u32 ph = pair_hash(F.id1, F.id2);
    f32 kind = dm_u32_to_f32(ph);
    f32 e = F.edge;

    /* Floes of different thickness meet in a step. */
    if (e < 1.0f) slope = DM_MAX(slope, 1.5f);

    if (kind < 0.38f) {
        /* A refrozen lead: nilas a few metres wide, lower than the floes on
         * either side, which step down to it. Grey when it is new; a lead that
         * froze months ago has had the winter's snow drift across it, and is
         * only a shallow trough in white. */
        f32 half = 1.0f + 5.0f * dm_u32_to_f32(dm_hash_u32(ph ^ 0x11u));
        f32 k = dm_smoothstep(half + 0.6f + 1.5f * I->snow_cover, half, e);
        h = dm_lerp(h, dm_lerp(0.06f, thick - 0.12f, I->snow_cover), k);
        if (k > 0.5f && I->snow_cover < 0.5f) m = ICE_NEW;
        if (e < half + 0.8f) slope = DM_MAX(slope, 1.5f);
    } else if (kind < 0.84f) {
        /* A ridge. The sail is a mound of rubble along the seam, higher where
         * the pressure is working; the expedition measured one at fourteen
         * feet. */
        f32 hmax = (0.8f + 1.9f * dm_u32_to_f32(dm_hash_u32(ph ^ 0x22u))) *
                   (1.0f + 0.55f * I->pressure);
        f32 wid  = hmax * 2.6f + 1.0f;
        if (e < wid) {
            f32 prof = dm_sat(1.0f - e / wid);
            prof = prof * prof * (3.0f - 2.0f * prof);
            /* Ridges do not run the whole length of a seam; they are built
             * where the floes actually met. */
            f32 run = dm_sat(fbm2(x / 60.0f, z / 60.0f + (f32)(ph & 255u), 2, 0x33u) * 2.2f + 0.45f);
            f32 sail = hmax * prof * run;
            if (sail > 0.05f) {
                /* The winter's snow drifts into a ridge and buries all but
                 * the biggest blocks. */
                h = DM_MAX(h, thick + sail * (0.55f + 0.30f * I->snow_cover));
                slope = DM_MAX(slope, 0.8f);
                if (lod < 0.4f) {
                    f32 top;
                    f32 r = rubble(x, z, sail, &top) + thick;
                    if (r > h) { h = r; m = top > 0.5f ? ICE_SNOW : ICE_BLOCK; }
                    slope = 9.0f;
                }
            }
        }
    } else {
        h -= 0.08f * dm_smoothstep(0.5f, 0.0f, e);
    }

    /* Hummocks: fields of rafted blocks out in the middle of a floe, where it
     * was itself once two floes. Low and lumpy, and snowed over. */
    {
        f32 field = fbm2(x / 45.0f + 3.1f, z / 45.0f - 7.7f, 2, 0x4a3u) * 2.4f - 0.55f;
        if (field > 0.0f) {
            f32 sail = dm_sat(field) * 0.75f;
            h = DM_MAX(h, thick + sail * (0.35f + 0.45f * I->snow_cover) + 0.10f * sail * fbm2(x / 3.0f, z / 3.0f, 2, 0x55u));
            slope = DM_MAX(slope, 0.6f);
            if (lod < 0.4f) {
                f32 top;
                f32 r = rubble(x - 11.0f, z + 5.0f, sail, &top) + thick - 0.15f;
                if (r > h) { h = r; m = top > 0.4f || I->snow_cover > 0.5f ? ICE_SNOW : ICE_BLOCK; }
                slope = 9.0f;
            }
        }
    }

    f32 free = e - RIDGE_REACH;

    /* The scene's own ridge. */
    if (I->ridge_h > 0.0f) {
        v2  ab = v2_sub(I->ridge_b, I->ridge_a);
        v2  pa = V2(x - I->ridge_a.x, z - I->ridge_a.y);
        f32 u  = dm_sat(v2_dot(pa, ab) / DM_MAX(v2_dot(ab, ab), 1e-6f));
        f32 dr = v2_len(v2_sub(pa, v2_scl(ab, u)));
        f32 wid = I->ridge_h * 2.2f + 1.5f;
        free = DM_MIN(free, dr - wid - 3.5f);
        f32 d  = dr + 2.0f * fbm2(x / 9.0f, z / 9.0f, 2, 0x71d6u);
        f32 ends = dm_smoothstep(0.0f, 0.12f, u) * dm_smoothstep(1.0f, 0.88f, u);
        if (d < wid) {
            f32 prof = dm_sat(1.0f - d / wid);
            prof = prof * prof * (3.0f - 2.0f * prof);
            f32 sail = I->ridge_h * prof * ends * (0.75f + 0.5f * fbm2(x / 14.0f, z / 14.0f, 2, 0x71d7u));
            if (sail > 0.05f) {
                h = DM_MAX(h, thick + sail * (0.45f + 0.30f * I->snow_cover));
                slope = DM_MAX(slope, 0.8f);
                if (lod < 0.4f) {
                    f32 top;
                    f32 r = rubble(x + 7.0f, z - 3.0f, sail, &top) + thick;
                    if (r > h) { h = r; m = top > 0.5f ? ICE_SNOW : ICE_BLOCK; }
                    slope = 9.0f;
                }
            }
        }
    }

    /* The hull, which the ice has closed round and heaped against. */
    if (I->hull_fn) {
        f32 d = I->hull_fn(x, z, I->hull_ud);
        if (d < 7.0f) {
            f32 heap = (0.55f + 0.55f * I->pressure) * dm_smoothstep(6.5f, 1.0f, d) * dm_smoothstep(-0.3f, 1.0f, d);
            if (heap > 0.05f) {
                if (lod < 0.4f) {
                    f32 top;
                    f32 r = rubble(x + 31.0f, z - 17.0f, heap * 1.3f, &top) + thick;
                    if (r > h) { h = r; m = top > 0.5f ? ICE_SNOW : ICE_BLOCK; }
                    slope = 9.0f;
                } else {
                    h += heap * 0.5f;
                    slope = DM_MAX(slope, 0.8f);
                }
            }
            if (d < 0.0f) { h = -0.5f; m = ICE_WATER; slope = 9.0f; }
        }
        free = DM_MIN(free, d - 7.0f);
    }

    /* Whatever the scene has built on the floe. */
    if (I->extra_fn) {
        int em = ICE_SNOW;
        f32 ef;
        f32 eh = I->extra_fn(x, z, I->extra_ud, &em, &ef);
        if (eh > h) { h = eh; m = em; slope = DM_MAX(slope, 3.0f); }
        free = DM_MIN(free, ef);
        if (ef < 0.5f) slope = DM_MAX(slope, 3.0f);
    }

    /* Open water. */
    if (I->lead_w > 0.0f) {
        v2  ab = v2_sub(I->lead_b, I->lead_a);
        v2  pa = V2(x - I->lead_a.x, z - I->lead_a.y);
        f32 u  = dm_sat(v2_dot(pa, ab) / DM_MAX(v2_dot(ab, ab), 1e-6f));
        f32 d  = v2_len(v2_sub(pa, v2_scl(ab, u)));
        f32 wig = I->lead_w * (1.0f + 0.35f * fbm2(x / 25.0f, z / 25.0f, 3, 0x1eadu));
        if (d < wig) { h = -0.02f; m = ICE_WATER; slope = 0.0f; }
        if (fabsf(d - wig) < 0.6f) slope = 9.0f;
        free = DM_MIN(free, fabsf(d - wig) - 1.0f);
    }

    if (si) { si->mat = m; si->slope = slope; si->free = free; }
    return h;
}

/* ---- the bake ------------------------------------------------------------------ */

#define BAKE_LEVELS 12
#define BAKE_BLOCK  16
#define BAKE_CELL0  0.01f
/* The finest detail the maps answer for. A texel four pixels across is still
 * smooth snow; only the first few metres in front of the lens ask for less. */
#define BAKE_MIN_LOD (BAKE_CELL0 * 0.25f)

#define BAKE_N      1024
#define BAKE_NB     (BAKE_N / BAKE_BLOCK)
/* Two coarser tiers of maxima over the blocks: four blocks a side, and
 * sixteen. A ray a metre over the snow and far from any ridge crosses a
 * kilometre of pack in a handful of steps instead of a thousand. */
#define TIER1       4
#define TIER2       16

struct en_ice_bake {
    int  n, nb;                     /* texels a side, blocks a side          */
    f32  cx, cz;
    u64  key;
    int  valid;
    f32 *h[BAKE_LEVELS];
    u8  *m[BAKE_LEVELS];
    f32 *bmax[BAKE_LEVELS];         /* highest point in each block           */
    f32 *bslope[BAKE_LEVELS];       /* steepest slope in each block          */
    f32 *tmax1[BAKE_LEVELS];        /* highest point in each TIER1 square    */
    f32 *tmax2[BAKE_LEVELS];        /* ... and each TIER2 square             */
};

en_ice_bake *en_ice_bake_new(void)
{
    en_ice_bake *B = (en_ice_bake *)calloc(1, sizeof *B);
    if (!B) return NULL;
    B->n  = BAKE_N;
    B->nb = BAKE_NB;
    size_t t1 = (size_t)(B->nb / TIER1), t2 = (size_t)(B->nb / TIER2);
    for (int k = 0; k < BAKE_LEVELS; k++) {
        B->h[k]      = (f32 *)malloc(sizeof(f32) * (size_t)B->n * B->n);
        B->m[k]      = (u8 *)malloc((size_t)B->n * B->n);
        B->bmax[k]   = (f32 *)malloc(sizeof(f32) * (size_t)B->nb * B->nb);
        B->bslope[k] = (f32 *)malloc(sizeof(f32) * (size_t)B->nb * B->nb);
        B->tmax1[k]  = (f32 *)malloc(sizeof(f32) * t1 * t1);
        B->tmax2[k]  = (f32 *)malloc(sizeof(f32) * t2 * t2);
        if (!B->h[k] || !B->m[k] || !B->bmax[k] || !B->bslope[k] || !B->tmax1[k] || !B->tmax2[k]) {
            en_ice_bake_free(B);
            return NULL;
        }
    }
    return B;
}

void en_ice_bake_free(en_ice_bake *B)
{
    if (!B) return;
    for (int k = 0; k < BAKE_LEVELS; k++) {
        free(B->h[k]); free(B->m[k]); free(B->bmax[k]); free(B->bslope[k]);
        free(B->tmax1[k]); free(B->tmax2[k]);
    }
    free(B);
}

static inline f32 bake_cell(int k) { return BAKE_CELL0 * (f32)(1 << k); }

typedef struct {
    en_ice_bake  *B;
    const en_ice *I;
} bake_job;

static void bake_row(int idx, int thread, void *ud)
{
    (void)thread;
    bake_job *j = (bake_job *)ud;
    en_ice_bake *B = j->B;
    int k = idx / B->n, row = idx % B->n;
    f32 cell = bake_cell(k);
    f32 ox = B->cx - cell * (f32)(B->n / 2), oz = B->cz - cell * (f32)(B->n / 2);
    f32 z = oz + ((f32)row + 0.5f) * cell;
    for (int i = 0; i < B->n; i++) {
        f32 x = ox + ((f32)i + 0.5f) * cell;
        surf_info si;
        B->h[k][(size_t)row * B->n + i] = surface(j->I, x, z, cell, &si);
        /* The high bit marks a texel with a cliff in it -- a block face, the
         * edge of a lead -- which a bilinear map turns into a staircase.
         * Close enough to see the stairs, the march asks the function. */
        B->m[k][(size_t)row * B->n + i] = (u8)(si.mat | (si.slope >= 3.0f ? 0x80 : 0));
    }
}

void en_ice_bake_build(en_ice_bake *B, const en_ice *I, f32 cx, f32 cz, u64 key)
{
    if (!B) return;
    if (B->valid && B->key == key && B->cx == cx && B->cz == cz) return;
    B->cx = cx; B->cz = cz; B->key = key;

    en_ice tmp = *I;
    tmp.bake = NULL;
    tmp.bake2 = NULL;
    bake_job j = { B, &tmp };
    clock_t c0 = clock();
    dm_job_for(BAKE_LEVELS * B->n, bake_row, &j);
    if (getenv("ICE_LOG")) printf("[ice] baked %d levels of %d in %.2f s cpu\n", BAKE_LEVELS, B->n, (double)(clock() - c0) / CLOCKS_PER_SEC);

    /* Per block: the highest point and the steepest slope, including the
     * texels around its edge, so a ray above a block's maximum can cross the
     * whole block in one step. */
    for (int k = 0; k < BAKE_LEVELS; k++) {
        f32 cell = bake_cell(k);
        for (int bj = 0; bj < B->nb; bj++)
            for (int bi = 0; bi < B->nb; bi++) {
                f32 mx = -1e9f, sl = 0.0f;
                int i0 = DM_MAX(bi * BAKE_BLOCK - 1, 0), i1 = DM_MIN((bi + 1) * BAKE_BLOCK + 1, B->n - 1);
                int j0 = DM_MAX(bj * BAKE_BLOCK - 1, 0), j1 = DM_MIN((bj + 1) * BAKE_BLOCK + 1, B->n - 1);
                for (int jj = j0; jj <= j1; jj++)
                    for (int ii = i0; ii <= i1; ii++) {
                        f32 h = B->h[k][(size_t)jj * B->n + ii];
                        if (h > mx) mx = h;
                        if (ii < i1) sl = DM_MAX(sl, fabsf(B->h[k][(size_t)jj * B->n + ii + 1] - h) / cell);
                        if (jj < j1) sl = DM_MAX(sl, fabsf(B->h[k][(size_t)(jj + 1) * B->n + ii] - h) / cell);
                    }
                /* A little over the texels' own highest point: the function
                 * the cliffs are handed back to peaks between samples. */
                B->bmax[k][(size_t)bj * B->nb + bi] = mx + 0.12f;
                B->bslope[k][(size_t)bj * B->nb + bi] = sl * 1.15f + 0.02f;
            }
        for (int tier = 0; tier < 2; tier++) {
            int s = tier ? TIER2 : TIER1, nt = B->nb / s;
            f32 *dst = tier ? B->tmax2[k] : B->tmax1[k];
            for (int tj = 0; tj < nt; tj++)
                for (int ti = 0; ti < nt; ti++) {
                    f32 mx = -1e9f;
                    for (int bj = tj * s; bj < (tj + 1) * s; bj++)
                        for (int bi = ti * s; bi < (ti + 1) * s; bi++)
                            mx = DM_MAX(mx, B->bmax[k][(size_t)bj * B->nb + bi]);
                    dst[(size_t)tj * nt + ti] = mx;
                }
        }
    }
    B->valid = 1;
}

/* Which level answers for a point at this detail, or -1 for none. The finest
 * level whose texels are no bigger than twice the detail wanted, and that
 * reaches out this far. */
static int bake_level(const en_ice_bake *B, f32 x, f32 z, f32 lod)
{
    int k = 0;
    f32 want = lod;
    while (k < BAKE_LEVELS - 1 && bake_cell(k + 1) <= want) k++;
    for (; k < BAKE_LEVELS; k++) {
        f32 half = bake_cell(k) * (f32)(B->n / 2 - 2);
        if (fabsf(x - B->cx) < half && fabsf(z - B->cz) < half) return k;
    }
    return -1;
}

static f32 bake_height(const en_ice_bake *B, int k, f32 x, f32 z, int *mat)
{
    f32 cell = bake_cell(k);
    f32 u = (x - B->cx) / cell + (f32)(B->n / 2) - 0.5f;
    f32 v = (z - B->cz) / cell + (f32)(B->n / 2) - 0.5f;
    f32 fu = floorf(u), fv = floorf(v);
    int i = (int)fu, j = (int)fv;
    f32 tu = u - fu, tv = v - fv;
    const f32 *h = B->h[k];
    size_t n = (size_t)B->n;
    f32 a = h[(size_t)j * n + i], b = h[(size_t)j * n + i + 1];
    f32 c = h[(size_t)(j + 1) * n + i], d = h[(size_t)(j + 1) * n + i + 1];
    if (mat) {
        int ni = tu < 0.5f ? i : i + 1, nj = tv < 0.5f ? j : j + 1;
        /* The cliff flag from any of the four corners, the material from the
         * nearest. */
        u8 f = (B->m[k][(size_t)j * n + i] | B->m[k][(size_t)j * n + i + 1] |
                B->m[k][(size_t)(j + 1) * n + i] | B->m[k][(size_t)(j + 1) * n + i + 1]) & 0x80;
        *mat = (B->m[k][(size_t)nj * n + ni] & 0x7f) | f;
    }
    return dm_lerp(dm_lerp(a, b, tu), dm_lerp(c, d, tu), tv);
}

/* The highest of the four texels round a point, plus what a tilted block top
 * can rise between them, plus the ribs in the snow a finer detail adds that
 * the map was baked without: a bound on the function over that texel. */
static f32 bake_cmax(const en_ice_bake *B, int k, f32 x, f32 z)
{
    f32 cell = bake_cell(k);
    f32 u = (x - B->cx) / cell + (f32)(B->n / 2) - 0.5f;
    f32 v = (z - B->cz) / cell + (f32)(B->n / 2) - 0.5f;
    int i = (int)floorf(u), j = (int)floorf(v);
    const f32 *h = B->h[k];
    size_t n = (size_t)B->n;
    f32 m = DM_MAX(DM_MAX(h[(size_t)j * n + i], h[(size_t)j * n + i + 1]),
                   DM_MAX(h[(size_t)(j + 1) * n + i], h[(size_t)(j + 1) * n + i + 1]));
    return m + 0.5f * cell + 0.13f;
}

/* A shot can carry two bakes -- one round the camera, one round what it is
 * looking at a few kilometres away -- and a point is answered by whichever
 * has the finer texels there. */
static const en_ice_bake *pick_bake(const en_ice *I, f32 x, f32 z, f32 lod, int *k)
{
    int k1 = I->bake  ? bake_level(I->bake,  x, z, lod) : -1;
    int k2 = I->bake2 ? bake_level(I->bake2, x, z, lod) : -1;
    if (k1 < 0 && k2 < 0) return NULL;
    if (k2 >= 0 && (k1 < 0 || k2 < k1)) { *k = k2; return I->bake2; }
    *k = k1;
    return I->bake;
}

f32 en_ice_height(const en_ice *I, f32 x, f32 z, f32 lod, int *mat)
{
    if (I->bake && lod >= BAKE_MIN_LOD) {
        int k;
        const en_ice_bake *B = pick_bake(I, x, z, lod, &k);
        if (B) {
            int m;
            f32 h = bake_height(B, k, x, z, &m);
            if (mat) *mat = m & 0x7f;
            return h;
        }
    }
    surf_info si;
    f32 h = surface(I, x, z, lod, &si);
    if (mat) *mat = si.mat;
    return h;
}

/* The shading normal always carries the finest detail that the pixel can
 * resolve as brightness, whatever the march used for the shape. */
static v3 normal_at(const en_ice *I, f32 x, f32 z, f32 lod)
{
    if (I->bake && lod >= BAKE_MIN_LOD) {
        int k;
        const en_ice_bake *B = pick_bake(I, x, z, lod * 0.5f, &k);
        if (B) {
            f32 e = bake_cell(k);
            f32 hx = bake_height(B, k, x + e, z, NULL) - bake_height(B, k, x - e, z, NULL);
            f32 hz = bake_height(B, k, x, z + e, NULL) - bake_height(B, k, x, z - e, NULL);
            return v3_norm(V3(-hx, 2.0f * e, -hz));
        }
    }
    f32 e = DM_MAX(0.006f, lod * 0.5f);
    f32 nl = DM_MIN(lod, 0.01f);
    f32 hx = surface(I, x + e, z, nl, NULL) - surface(I, x - e, z, nl, NULL);
    f32 hz = surface(I, x, z + e, nl, NULL) - surface(I, x, z - e, nl, NULL);
    return v3_norm(V3(-hx, 2.0f * e, -hz));
}

/* The march through the baked pack. Returns 1 hit, 0 miss, and -1 where the
 * ray has come close enough to the camera's own detail to need the function
 * itself -- which only happens at the very start of a ray. */
static int trace_baked(const en_ice *I, v3 ro, v3 rd, f32 t0, f32 t_end, f32 cone,
                       f32 *t_out, v3 *n_out, int *mat_out, f32 *t_resume)
{
    f32 hlen = sqrtf(DM_MAX(1.0f - rd.y * rd.y, 0.0f));
    f32 ady  = fabsf(rd.y);
    f32 t = t0, prev_t = t0;
    int mat = ICE_SNOW;

    /* A pixel's footprint on the ground is stretched along the ray by one
     * over the sine of the grazing angle. Asking for detail at the geometric
     * mean of its two sides keeps a long lens looking across a kilometre of
     * pack from aliasing every seam into a streak. */
    f32 aniso = DM_MIN(1.0f / sqrtf(DM_MAX(ady, 1e-4f)), 16.0f);

    for (int it = 0; it < 400 && t < t_end; it++) {
        v3  p   = v3_add(ro, v3_scl(rd, t));
        /* Near the lens the ray meets the faces of blocks, not the ground, and
         * the stretch does not apply. */
        f32 lod = DM_MAX(0.004f, t * cone * 2.0f * dm_lerp(1.0f, aniso, dm_smoothstep(60.0f, 250.0f, t)));
        if (lod < BAKE_MIN_LOD) { *t_resume = t; return -1; }
        int k;
        const en_ice_bake *B = pick_bake(I, p.x, p.z, lod, &k);
        if (!B) break;

        f32 cell = bake_cell(k);
        f32 u = (p.x - B->cx) / cell + (f32)(B->n / 2);
        f32 v = (p.z - B->cz) / cell + (f32)(B->n / 2);
        int bi = (int)floorf(u / BAKE_BLOCK), bj = (int)floorf(v / BAKE_BLOCK);
        bi = DM_MAX(0, DM_MIN(bi, B->nb - 1));
        bj = DM_MAX(0, DM_MIN(bj, B->nb - 1));
        f32 bmax = B->bmax[k][(size_t)bj * B->nb + bi];
        f32 bsl  = B->bslope[k][(size_t)bj * B->nb + bi];
        f32 minstep = DM_MAX(0.002f, t * cone * 1.2f);

        if (p.y > bmax + 0.002f) {
            /* Above everything in this block: go to where the ray leaves the
             * block, or comes down to its highest point, whichever is first --
             * or the same for the largest square of blocks it is above. */
            int span = 1;
            f32 top = bmax;
            {
                int ti = bi / TIER1, tj = bj / TIER1;
                f32 m1 = B->tmax1[k][(size_t)tj * (B->nb / TIER1) + ti];
                if (p.y > m1 + 0.002f) {
                    span = TIER1; top = m1;
                    int ui = bi / TIER2, uj = bj / TIER2;
                    f32 m2 = B->tmax2[k][(size_t)uj * (B->nb / TIER2) + ui];
                    if (p.y > m2 + 0.002f) { span = TIER2; top = m2; }
                }
            }
            bi = (bi / span) * span;
            bj = (bj / span) * span;
            f32 bx0 = ((f32)(bi * BAKE_BLOCK) - (f32)(B->n / 2)) * cell + B->cx;
            f32 bz0 = ((f32)(bj * BAKE_BLOCK) - (f32)(B->n / 2)) * cell + B->cz;
            f32 bs  = cell * (f32)(BAKE_BLOCK * span);
            bmax = top;
            f32 tx = 1e9f, tz = 1e9f;
            if (rd.x > 1e-6f) tx = (bx0 + bs - p.x) / rd.x; else if (rd.x < -1e-6f) tx = (bx0 - p.x) / rd.x;
            if (rd.z > 1e-6f) tz = (bz0 + bs - p.z) / rd.z; else if (rd.z < -1e-6f) tz = (bz0 - p.z) / rd.z;
            f32 exit = DM_MIN(tx, tz);
            f32 down = rd.y < -1e-6f ? (p.y - bmax) / ady : 1e9f;
            prev_t = t;
            t += DM_MAX(DM_MIN(exit + cell * 0.01f, down), minstep);
            continue;
        }

        f32 h = bake_height(B, k, p.x, p.z, &mat);
        /* Beyond a hundred metres a block's face is too small on screen for
         * its stairs to show, and asking the function there is most of the
         * cost of a frame of pack. */
        /* Only where a block's face spans enough pixels for its stairs to
         * show. Further off, the function's faces are finer than the ray's
         * own steps across them, and it gives back noise. */
        int exact = (mat & 0x80) && lod < cell * 1.5f && lod < 0.06f;
        if (exact) {
            /* The function is only asked once the ray is down among the
             * texels' own heights; above them the map bounds it. */
            f32 cm = bake_cmax(B, k, p.x, p.z);
            if (p.y > cm) {
                prev_t = t;
                t += DM_MAX((p.y - cm) / (ady + 9.0f * hlen), minstep);
                continue;
            }
            surf_info si;
            h = surface(I, p.x, p.z, lod, &si);
            mat = si.mat;
        }
        f32 gap = p.y - h;
        if (gap < 0.0f) {
            f32 a = prev_t, b = t;
            for (int s = 0; s < 6; s++) {
                f32 m  = 0.5f * (a + b);
                v3  pm = v3_add(ro, v3_scl(rd, m));
                f32 hm;
                if (exact) hm = surface(I, pm.x, pm.z, lod, NULL);
                else {
                    int km;
                    const en_ice_bake *Bm = pick_bake(I, pm.x, pm.z, lod, &km);
                    hm = Bm ? bake_height(Bm, km, pm.x, pm.z, NULL) : h;
                }
                if (pm.y - hm < 0.0f) b = m; else a = m;
            }
            t = 0.5f * (a + b);
            p = v3_add(ro, v3_scl(rd, t));
            if (exact) {
                surf_info si;
                surface(I, p.x, p.z, lod, &si);
                mat = si.mat;
                f32 e = DM_MAX(0.004f, lod * 0.5f);
                f32 hx = surface(I, p.x + e, p.z, lod, NULL) - surface(I, p.x - e, p.z, lod, NULL);
                f32 hz = surface(I, p.x, p.z + e, lod, NULL) - surface(I, p.x, p.z - e, lod, NULL);
                *n_out = v3_norm(V3(-hx, 2.0f * e, -hz));
            } else {
                bake_height(B, k, p.x, p.z, &mat);
                *n_out = normal_at(I, p.x, p.z, lod);
            }
            *t_out = t;
            *mat_out = mat & 0x7f;
            return 1;
        }
        prev_t = t;
        t += DM_MAX(gap / (ady + (exact ? 9.0f : bsl) * hlen), minstep);
    }

    if (rd.y < -1e-5f) {
        f32 tp = (ro.y - 0.37f) / ady;
        if (tp < t_end + 1e4f && tp >= prev_t) {
            *t_out = tp; *n_out = V3(0, 1, 0); *mat_out = ICE_SNOW;
            return 1;
        }
    }
    return 0;
}

int en_ice_trace(const en_ice *I, v3 ro, v3 rd, f32 t_max, f32 cone,
                 f32 *t_out, v3 *n_out, int *mat_out)
{
    f32 t0 = 0.0f;
    if (ro.y > H_MAX) {
        if (rd.y >= -1e-5f) return 0;
        t0 = (ro.y - H_MAX) / -rd.y;
    }
    f32 t_end = DM_MIN(t_max, FAR_FLAT);
    if (rd.y < -1e-5f) t_end = DM_MIN(t_end, (ro.y + 0.6f) / -rd.y);

    f32 hlen = sqrtf(DM_MAX(1.0f - rd.y * rd.y, 0.0f));
    f32 ady  = fabsf(rd.y);
    f32 t = t0, prev_t = t0;
    surf_info si;
    si.mat = ICE_SNOW;

    /* With a bake, the function is only asked where the maps are too coarse:
     * the first few metres in front of the lens. */
    f32 t_switch = I->bake ? (BAKE_MIN_LOD) / DM_MAX(cone * 2.0f, 1e-9f) : 1e30f;
    if (I->bake && t0 >= t_switch) {
        f32 tr = t0;
        int r = trace_baked(I, ro, rd, t0, t_end, cone, t_out, n_out, mat_out, &tr);
        if (r >= 0) return r == 1 && *t_out < t_max;
        t = prev_t = tr;
    }

    for (int i = 0; i < 220 && t < t_end; i++) {
        if (t >= t_switch) {
            f32 tr = t;
            int r = trace_baked(I, ro, rd, t, t_end, cone, t_out, n_out, mat_out, &tr);
            if (r >= 0) return r == 1 && *t_out < t_max;
        }
        v3  p   = v3_add(ro, v3_scl(rd, t));
        f32 lod = DM_MAX(0.004f, t * cone * 2.0f);
        f32 h   = surface(I, p.x, p.z, lod, &si);
        f32 gap = p.y - h;

        if (gap < 0.0f) {
            f32 a = prev_t, b = t;
            for (int k = 0; k < 6; k++) {
                f32 m  = 0.5f * (a + b);
                v3  pm = v3_add(ro, v3_scl(rd, m));
                if (pm.y - surface(I, pm.x, pm.z, lod, NULL) < 0.0f) b = m; else a = m;
            }
            t = 0.5f * (a + b);
            p = v3_add(ro, v3_scl(rd, t));
            surface(I, p.x, p.z, lod, &si);
            *t_out   = t;
            *n_out   = normal_at(I, p.x, p.z, lod);
            *mat_out = si.mat;
            return 1;
        }
        prev_t = t;

        f32 step = gap / (ady + si.slope * hlen);
        /* Above the snow and clear of any ridge, nothing can be hit until the
         * ray either comes down to the height of the snow or reaches a seam. */
        if (p.y > SNOW_MAX && si.free > 0.0f) {
            f32 down = rd.y < -1e-5f ? (p.y - SNOW_MAX) / ady : 1e9f;
            f32 side = si.free / DM_MAX(hlen, 1e-4f);
            step = DM_MAX(step, DM_MIN(down, side));
        }
        step = DM_MAX(step, DM_MAX(0.003f, t * cone * 1.5f));
        t += step;
    }

    /* Past the detailed pack, or out of steps: a flat surface at the mean
     * freeboard, which from this far is what the pack is. */
    if (rd.y < -1e-5f) {
        f32 tp = (ro.y - 0.37f) / ady;
        if (tp < t_max && tp >= prev_t) {
            *t_out = tp; *n_out = V3(0, 1, 0); *mat_out = ICE_SNOW;
            return 1;
        }
    }
    return 0;
}

v3 en_ice_albedo(const en_ice *I, v3 p, v3 n, int mat, f32 *translucency)
{
    (void)I;
    f32 tr = 0.0f;
    v3  a;
    switch (mat) {
    case ICE_NEW: {
        f32 k = dm_sat(fbm2(p.x / 3.0f, p.z / 3.0f, 3, 0x4e4u) + 0.5f);
        a = v3_lerp(V3(0.20f, 0.26f, 0.31f), V3(0.46f, 0.52f, 0.57f), k);
        tr = 0.15f;
        break;
    }
    case ICE_BLOCK: {
        /* A broken face of sea ice. It is full of brine and air, so it is
         * milky rather than the glass-blue of glacier ice, and months of wind
         * have packed snow into every face that is not vertical. */
        f32 k = dm_sat(dm_fbm3(v3_scl(p, 1.8f), 3, 2.0f, 0.5f, 0xb1eu) + 0.5f);
        a = v3_lerp(V3(0.44f, 0.60f, 0.67f), V3(0.70f, 0.80f, 0.85f), k);
        f32 crust = dm_smoothstep(0.35f, 0.8f, n.y + 0.35f * dm_fbm3(v3_scl(p, 3.1f), 2, 2.0f, 0.5f, 0xb1fu));
        a = v3_lerp(a, V3(0.90f, 0.93f, 0.95f), crust);
        tr = 0.40f * (1.0f - crust);
        break;
    }
    case ICE_WATER:
        a = V3(0.01f, 0.015f, 0.02f);
        break;
    default: {
        f32 k = fbm2(p.x / 7.0f, p.z / 7.0f, 2, 0x5e0u);
        a = v3_scl(V3(0.93f, 0.95f, 0.97f), 0.94f + 0.06f * k);
        tr = 0.08f;
        break;
    }
    }
    if (translucency) *translucency = tr;
    return a;
}

f32 en_snow_glint(v3 p, v3 n, v3 l, v3 v, f32 cone_t)
{
    /* A crystal is about a millimetre across, and a pixel far wider, so what
     * a pixel sees is the chance that one of the crystals under it is turned
     * the right way. Close up that chance is a speck of light; far off it is
     * averaged into a faint sheen. */
    f32 cell = DM_MAX(0.012f, cone_t * 1.5f);
    v3  q    = v3_scl(p, 1.0f / cell);
    u32 h    = dm_hash3i((i32)floorf(q.x), (i32)floorf(q.y), (i32)floorf(q.z), 0x61171u);
    v3  j    = V3(dm_u32_to_f32(h) - 0.5f, dm_u32_to_f32(dm_hash_u32(h ^ 1u)) - 0.5f,
                  dm_u32_to_f32(dm_hash_u32(h ^ 2u)) - 0.5f);
    v3  m    = v3_norm(v3_add(n, v3_scl(j, 1.3f)));
    v3  hv   = v3_norm(v3_add(l, v));
    f32 s    = powf(dm_sat(v3_dot(m, hv)), 900.0f);
    f32 area = (0.012f * 0.012f) / (cell * cell);
    return s * 90.0f * area;
}
