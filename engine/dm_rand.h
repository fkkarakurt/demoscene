/* dm_rand.h -- deterministic randomness and procedural noise.
 *
 * There is no noise texture and no permutation table anywhere in this project.
 * Every value below is derived from an integer hash, so the whole demo is a
 * pure function of time: the same frame index always renders the same pixels,
 * which is what lets the preview window and the offline render agree exactly.
 */
#ifndef DM_RAND_H
#define DM_RAND_H

#include "dm_vec.h"

/* ---- integer hashing --------------------------------------------------- */

/* Chris Wellons' "lowbias32" finaliser: excellent avalanche for 3 multiplies. */
static inline u32 dm_hash_u32(u32 x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static inline u32 dm_hash2i(i32 x, i32 y, u32 seed)
{
    return dm_hash_u32((u32)x * 0x8da6b343U ^ (u32)y * 0xd8163841U ^ seed);
}

static inline u32 dm_hash3i(i32 x, i32 y, i32 z, u32 seed)
{
    return dm_hash_u32((u32)x * 0x8da6b343U ^ (u32)y * 0xd8163841U ^ (u32)z * 0xcb1ab31fU ^ seed);
}

/* Uniform in [0,1). 24 bits of mantissa is exactly what a float can hold. */
static inline f32 dm_u32_to_f32(u32 h) { return (f32)(h >> 8) * (1.0f / 16777216.0f); }

static inline f32 dm_hash1f(f32 x, u32 seed)
{
    union { f32 f; u32 u; } c;
    c.f = x;
    return dm_u32_to_f32(dm_hash_u32(c.u ^ seed));
}

/* ---- stateful RNG (PCG32) ---------------------------------------------- */

typedef struct { u64 state; } dm_rng;

static inline void dm_rng_seed(dm_rng *r, u64 seed)
{
    r->state = seed * 6364136223846793005ULL + 1442695040888963407ULL;
}

static inline u32 dm_rng_u32(dm_rng *r)
{
    u64 x   = r->state;
    r->state = x * 6364136223846793005ULL + 1442695040888963407ULL;
    u32 xs  = (u32)(((x >> 18) ^ x) >> 27);
    u32 rot = (u32)(x >> 59);
    return (xs >> rot) | (xs << ((32u - rot) & 31u));
}

static inline f32 dm_rng_f(dm_rng *r)                { return dm_u32_to_f32(dm_rng_u32(r)); }
static inline f32 dm_rng_sf(dm_rng *r)               { return dm_rng_f(r) * 2.0f - 1.0f; }
static inline f32 dm_rng_range(dm_rng *r, f32 a, f32 b) { return a + (b - a) * dm_rng_f(r); }
static inline int dm_rng_int(dm_rng *r, int n)       { return (int)(dm_rng_u32(r) % (u32)n); }

/* Uniform direction on the unit sphere, via the cylindrical-projection trick. */
static inline v3 dm_rng_dir(dm_rng *r)
{
    f32 z   = dm_rng_sf(r);
    f32 phi = dm_rng_f(r) * DM_TAU;
    f32 s   = sqrtf(DM_MAX(0.0f, 1.0f - z * z));
    return V3(s * cosf(phi), s * sinf(phi), z);
}

/* Cosine-weighted hemisphere sample around n -- the diffuse bounce direction. */
static inline v3 dm_rng_cosine(dm_rng *r, v3 n)
{
    return v3_norm(v3_add(n, dm_rng_dir(r)));
}

/* ---- gradient (Perlin) noise ------------------------------------------- */

/* Ken Perlin's improved gradient set: the 12 edge midpoints of a cube, picked
 * by four bits of hash. No lookup table, no normalisation needed. */
static inline f32 dm_grad3(u32 h, f32 x, f32 y, f32 z)
{
    h &= 15u;
    f32 u = h < 8u ? x : y;
    f32 v = h < 4u ? y : ((h == 12u || h == 14u) ? x : z);
    return ((h & 1u) ? -u : u) + ((h & 2u) ? -v : v);
}

/* Perlin noise in [-1,1]. */
static inline f32 dm_pnoise3(v3 p, u32 seed)
{
    v3  i = v3_floor(p);
    v3  f = v3_sub(p, i);
    i32 X = (i32)i.x, Y = (i32)i.y, Z = (i32)i.z;

    /* Quintic fade keeps the second derivative continuous -- no grid creases. */
    f32 u = f.x * f.x * f.x * (f.x * (f.x * 6.0f - 15.0f) + 10.0f);
    f32 v = f.y * f.y * f.y * (f.y * (f.y * 6.0f - 15.0f) + 10.0f);
    f32 w = f.z * f.z * f.z * (f.z * (f.z * 6.0f - 15.0f) + 10.0f);

    f32 n000 = dm_grad3(dm_hash3i(X,   Y,   Z,   seed), f.x,        f.y,        f.z);
    f32 n100 = dm_grad3(dm_hash3i(X+1, Y,   Z,   seed), f.x - 1.0f, f.y,        f.z);
    f32 n010 = dm_grad3(dm_hash3i(X,   Y+1, Z,   seed), f.x,        f.y - 1.0f, f.z);
    f32 n110 = dm_grad3(dm_hash3i(X+1, Y+1, Z,   seed), f.x - 1.0f, f.y - 1.0f, f.z);
    f32 n001 = dm_grad3(dm_hash3i(X,   Y,   Z+1, seed), f.x,        f.y,        f.z - 1.0f);
    f32 n101 = dm_grad3(dm_hash3i(X+1, Y,   Z+1, seed), f.x - 1.0f, f.y,        f.z - 1.0f);
    f32 n011 = dm_grad3(dm_hash3i(X,   Y+1, Z+1, seed), f.x,        f.y - 1.0f, f.z - 1.0f);
    f32 n111 = dm_grad3(dm_hash3i(X+1, Y+1, Z+1, seed), f.x - 1.0f, f.y - 1.0f, f.z - 1.0f);

    f32 x00 = dm_lerp(n000, n100, u), x10 = dm_lerp(n010, n110, u);
    f32 x01 = dm_lerp(n001, n101, u), x11 = dm_lerp(n011, n111, u);
    return dm_lerp(dm_lerp(x00, x10, v), dm_lerp(x01, x11, v), w);
}

static inline f32 dm_pnoise2(v2 p, u32 seed) { return dm_pnoise3(V3(p.x, p.y, 0.0f), seed); }

/* ---- fractal layering -------------------------------------------------- */

/* Classic fBm. `lac` > 1 spreads octaves in frequency, `gain` < 1 in amplitude. */
static inline f32 dm_fbm3(v3 p, int octaves, f32 lac, f32 gain, u32 seed)
{
    f32 sum = 0.0f, amp = 1.0f, norm = 0.0f;
    for (int i = 0; i < octaves; i++) {
        sum  += amp * dm_pnoise3(p, seed + (u32)i * 0x9e3779b9U);
        norm += amp;
        amp  *= gain;
        p     = v3_scl(p, lac);
    }
    return norm > 0.0f ? sum / norm : 0.0f;
}

/* Ridged multifractal: |noise| inverted, the shape that reads as mountains,
 * lightning and smoke filaments. Returns 0..1. */
static inline f32 dm_ridge3(v3 p, int octaves, f32 lac, f32 gain, u32 seed)
{
    f32 sum = 0.0f, amp = 0.5f, norm = 0.0f, prev = 1.0f;
    for (int i = 0; i < octaves; i++) {
        f32 n = 1.0f - fabsf(dm_pnoise3(p, seed + (u32)i * 0x9e3779b9U));
        n     = n * n * prev;
        prev  = n;
        sum  += amp * n;
        norm += amp;
        amp  *= gain;
        p     = v3_scl(p, lac);
    }
    return norm > 0.0f ? sum / norm : 0.0f;
}

/* Domain-warped fBm: noise used to displace the lookup of more noise. This is
 * the single cheapest trick for making procedural fields look organic. */
static inline f32 dm_warp3(v3 p, f32 amount, int octaves, u32 seed)
{
    v3 q = V3(dm_fbm3(p, 4, 2.0f, 0.5f, seed + 11u),
              dm_fbm3(v3_adds(p, 5.2f), 4, 2.0f, 0.5f, seed + 23u),
              dm_fbm3(v3_adds(p, 1.3f), 4, 2.0f, 0.5f, seed + 37u));
    return dm_fbm3(v3_add(p, v3_scl(q, amount)), octaves, 2.0f, 0.5f, seed);
}

/* ---- cellular noise ---------------------------------------------------- */

/* Worley F1 distance (nearest feature point). Cells are unit sized. */
static inline f32 dm_worley3(v3 p, u32 seed)
{
    v3  ip = v3_floor(p);
    v3  fp = v3_sub(p, ip);
    f32 best = 8.0f;

    for (int dz = -1; dz <= 1; dz++)
    for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++) {
        v3  off = V3((f32)dx, (f32)dy, (f32)dz);
        u32 h   = dm_hash3i((i32)ip.x + dx, (i32)ip.y + dy, (i32)ip.z + dz, seed);
        v3  fpt = V3(dm_u32_to_f32(h),
                     dm_u32_to_f32(dm_hash_u32(h ^ 0x68bc21ebU)),
                     dm_u32_to_f32(dm_hash_u32(h ^ 0x02e5be93U)));
        f32 d = v3_len2(v3_sub(v3_add(off, fpt), fp));
        if (d < best) best = d;
    }
    return sqrtf(best);
}

/* Smooth Worley: soft-min over the neighbourhood, no hard cell edges. */
static inline f32 dm_worley3_smooth(v3 p, f32 k, u32 seed)
{
    v3  ip = v3_floor(p);
    v3  fp = v3_sub(p, ip);
    f32 acc = 0.0f;

    for (int dz = -1; dz <= 1; dz++)
    for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++) {
        v3  off = V3((f32)dx, (f32)dy, (f32)dz);
        u32 h   = dm_hash3i((i32)ip.x + dx, (i32)ip.y + dy, (i32)ip.z + dz, seed);
        v3  fpt = V3(dm_u32_to_f32(h),
                     dm_u32_to_f32(dm_hash_u32(h ^ 0x68bc21ebU)),
                     dm_u32_to_f32(dm_hash_u32(h ^ 0x02e5be93U)));
        acc += expf(-k * v3_len(v3_sub(v3_add(off, fpt), fp)));
    }
    return -logf(DM_MAX(acc, 1e-20f)) / k;
}

/* ---- sampling patterns ------------------------------------------------- */

/* R2 low-discrepancy sequence (Roberts). Better stratification than random
 * jitter for the same sample count -- used by the anti-aliasing sampler. */
static inline v2 dm_r2(u32 i)
{
    const f32 a1 = 0.7548776662466927f, a2 = 0.5698402909980532f;
    return V2(dm_fract(0.5f + a1 * (f32)i), dm_fract(0.5f + a2 * (f32)i));
}

/* Van der Corput radical inverse, base 2 -- the second Halton/Hammersley axis. */
static inline f32 dm_radical2(u32 bits)
{
    bits = (bits << 16) | (bits >> 16);
    bits = ((bits & 0x55555555u) << 1) | ((bits & 0xAAAAAAAAu) >> 1);
    bits = ((bits & 0x33333333u) << 2) | ((bits & 0xCCCCCCCCu) >> 2);
    bits = ((bits & 0x0F0F0F0Fu) << 4) | ((bits & 0xF0F0F0F0u) >> 4);
    bits = ((bits & 0x00FF00FFu) << 8) | ((bits & 0xFF00FF00u) >> 8);
    return (f32)bits * 2.3283064365386963e-10f;
}

#endif /* DM_RAND_H */
