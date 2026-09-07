/* dm_vec.h -- 2D/3D vector and 3x3 matrix math.
 *
 * Value semantics throughout: everything is passed and returned by value and
 * declared static inline, so the optimiser keeps it all in registers. This is
 * the hot path of the whole renderer -- every ray, every pixel goes through it.
 */
#ifndef DM_VEC_H
#define DM_VEC_H

#include "dm_base.h"

typedef struct { f32 x, y; }       v2;
typedef struct { f32 x, y, z; }    v3;
typedef struct { f32 x, y, z, w; } v4;

/* Column-major basis: c[0]=right, c[1]=up, c[2]=forward. */
typedef struct { v3 c[3]; } m3;

/* ---- constructors ----------------------------------------------------- */

static inline v2 V2(f32 x, f32 y)                { v2 r; r.x = x; r.y = y; return r; }
static inline v3 V3(f32 x, f32 y, f32 z)         { v3 r; r.x = x; r.y = y; r.z = z; return r; }
static inline v4 V4(f32 x, f32 y, f32 z, f32 w)  { v4 r; r.x = x; r.y = y; r.z = z; r.w = w; return r; }
static inline v2 V2s(f32 s)                      { return V2(s, s); }
static inline v3 V3s(f32 s)                      { return V3(s, s, s); }
static inline v3 v3_xy(v2 a, f32 z)              { return V3(a.x, a.y, z); }
static inline v2 v3_to_v2(v3 a)                  { return V2(a.x, a.y); }

/* ---- v2 --------------------------------------------------------------- */

static inline v2 v2_add(v2 a, v2 b)   { return V2(a.x + b.x, a.y + b.y); }
static inline v2 v2_sub(v2 a, v2 b)   { return V2(a.x - b.x, a.y - b.y); }
static inline v2 v2_mul(v2 a, v2 b)   { return V2(a.x * b.x, a.y * b.y); }
static inline v2 v2_scl(v2 a, f32 s)  { return V2(a.x * s, a.y * s); }
static inline v2 v2_neg(v2 a)         { return V2(-a.x, -a.y); }
static inline f32 v2_dot(v2 a, v2 b)  { return a.x * b.x + a.y * b.y; }
static inline f32 v2_cross(v2 a, v2 b){ return a.x * b.y - a.y * b.x; }
static inline f32 v2_len2(v2 a)       { return v2_dot(a, a); }
static inline f32 v2_len(v2 a)        { return sqrtf(v2_dot(a, a)); }
static inline f32 v2_dist(v2 a, v2 b) { return v2_len(v2_sub(a, b)); }
static inline v2 v2_lerp(v2 a, v2 b, f32 t) { return V2(dm_lerp(a.x, b.x, t), dm_lerp(a.y, b.y, t)); }
static inline v2 v2_abs(v2 a)         { return V2(fabsf(a.x), fabsf(a.y)); }
static inline v2 v2_fract(v2 a)       { return V2(dm_fract(a.x), dm_fract(a.y)); }
static inline v2 v2_floor(v2 a)       { return V2(floorf(a.x), floorf(a.y)); }

static inline v2 v2_norm(v2 a)
{
    f32 l = v2_len(a);
    return l > 1e-20f ? v2_scl(a, 1.0f / l) : V2(0.0f, 0.0f);
}

static inline v2 v2_rot(v2 a, f32 rad)
{
    f32 c = cosf(rad), s = sinf(rad);
    return V2(a.x * c - a.y * s, a.x * s + a.y * c);
}

/* ---- v3 --------------------------------------------------------------- */

static inline v3 v3_add(v3 a, v3 b)   { return V3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline v3 v3_sub(v3 a, v3 b)   { return V3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline v3 v3_mul(v3 a, v3 b)   { return V3(a.x * b.x, a.y * b.y, a.z * b.z); }
static inline v3 v3_div(v3 a, v3 b)   { return V3(a.x / b.x, a.y / b.y, a.z / b.z); }
static inline v3 v3_scl(v3 a, f32 s)  { return V3(a.x * s, a.y * s, a.z * s); }
static inline v3 v3_adds(v3 a, f32 s) { return V3(a.x + s, a.y + s, a.z + s); }
static inline v3 v3_neg(v3 a)         { return V3(-a.x, -a.y, -a.z); }
static inline f32 v3_dot(v3 a, v3 b)  { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline f32 v3_len2(v3 a)       { return v3_dot(a, a); }
static inline f32 v3_len(v3 a)        { return sqrtf(v3_dot(a, a)); }
static inline f32 v3_dist(v3 a, v3 b) { return v3_len(v3_sub(a, b)); }
static inline f32 v3_maxc(v3 a)       { return DM_MAX(a.x, DM_MAX(a.y, a.z)); }
static inline f32 v3_minc(v3 a)       { return DM_MIN(a.x, DM_MIN(a.y, a.z)); }
static inline f32 v3_sum(v3 a)        { return a.x + a.y + a.z; }

static inline v3 v3_cross(v3 a, v3 b)
{
    return V3(a.y * b.z - a.z * b.y,
              a.z * b.x - a.x * b.z,
              a.x * b.y - a.y * b.x);
}

static inline v3 v3_norm(v3 a)
{
    f32 l = v3_len(a);
    return l > 1e-20f ? v3_scl(a, 1.0f / l) : V3(0.0f, 0.0f, 0.0f);
}

static inline v3 v3_lerp(v3 a, v3 b, f32 t)
{
    return V3(dm_lerp(a.x, b.x, t), dm_lerp(a.y, b.y, t), dm_lerp(a.z, b.z, t));
}

static inline v3 v3_min(v3 a, v3 b)   { return V3(DM_MIN(a.x, b.x), DM_MIN(a.y, b.y), DM_MIN(a.z, b.z)); }
static inline v3 v3_max(v3 a, v3 b)   { return V3(DM_MAX(a.x, b.x), DM_MAX(a.y, b.y), DM_MAX(a.z, b.z)); }
static inline v3 v3_abs(v3 a)         { return V3(fabsf(a.x), fabsf(a.y), fabsf(a.z)); }
static inline v3 v3_floor(v3 a)       { return V3(floorf(a.x), floorf(a.y), floorf(a.z)); }
static inline v3 v3_fract(v3 a)       { return V3(dm_fract(a.x), dm_fract(a.y), dm_fract(a.z)); }
static inline v3 v3_sqrt(v3 a)        { return V3(sqrtf(fabsf(a.x)), sqrtf(fabsf(a.y)), sqrtf(fabsf(a.z))); }
static inline v3 v3_exp(v3 a)         { return V3(expf(a.x), expf(a.y), expf(a.z)); }
static inline v3 v3_pow(v3 a, f32 e)  { return V3(powf(a.x, e), powf(a.y, e), powf(a.z, e)); }
static inline v3 v3_sat(v3 a)         { return V3(dm_sat(a.x), dm_sat(a.y), dm_sat(a.z)); }
static inline v3 v3_clamp(v3 a, f32 lo, f32 hi) { return V3(dm_clamp(a.x, lo, hi), dm_clamp(a.y, lo, hi), dm_clamp(a.z, lo, hi)); }
static inline v3 v3_mod(v3 a, f32 m)  { return V3(dm_mod(a.x, m), dm_mod(a.y, m), dm_mod(a.z, m)); }

static inline v3 v3_reflect(v3 i, v3 n) { return v3_sub(i, v3_scl(n, 2.0f * v3_dot(i, n))); }

/* Snell refraction; returns the zero vector on total internal reflection. */
static inline v3 v3_refract(v3 i, v3 n, f32 eta)
{
    f32 ni = v3_dot(n, i);
    f32 k  = 1.0f - eta * eta * (1.0f - ni * ni);
    if (k < 0.0f) return V3(0.0f, 0.0f, 0.0f);
    return v3_sub(v3_scl(i, eta), v3_scl(n, eta * ni + sqrtf(k)));
}

/* ---- m3 --------------------------------------------------------------- */

static inline m3 m3_identity(void)
{
    m3 m;
    m.c[0] = V3(1, 0, 0);
    m.c[1] = V3(0, 1, 0);
    m.c[2] = V3(0, 0, 1);
    return m;
}

static inline v3 m3_mul_v3(m3 m, v3 v)
{
    return v3_add(v3_add(v3_scl(m.c[0], v.x), v3_scl(m.c[1], v.y)), v3_scl(m.c[2], v.z));
}

static inline m3 m3_mul(m3 a, m3 b)
{
    m3 r;
    r.c[0] = m3_mul_v3(a, b.c[0]);
    r.c[1] = m3_mul_v3(a, b.c[1]);
    r.c[2] = m3_mul_v3(a, b.c[2]);
    return r;
}

static inline m3 m3_transpose(m3 m)
{
    m3 r;
    r.c[0] = V3(m.c[0].x, m.c[1].x, m.c[2].x);
    r.c[1] = V3(m.c[0].y, m.c[1].y, m.c[2].y);
    r.c[2] = V3(m.c[0].z, m.c[1].z, m.c[2].z);
    return r;
}

static inline m3 m3_rot_x(f32 a) { f32 c = cosf(a), s = sinf(a); m3 m; m.c[0] = V3(1,0,0); m.c[1] = V3(0,c,s); m.c[2] = V3(0,-s,c); return m; }
static inline m3 m3_rot_y(f32 a) { f32 c = cosf(a), s = sinf(a); m3 m; m.c[0] = V3(c,0,-s); m.c[1] = V3(0,1,0); m.c[2] = V3(s,0,c); return m; }
static inline m3 m3_rot_z(f32 a) { f32 c = cosf(a), s = sinf(a); m3 m; m.c[0] = V3(c,s,0); m.c[1] = V3(-s,c,0); m.c[2] = V3(0,0,1); return m; }

static inline m3 m3_euler(f32 rx, f32 ry, f32 rz)
{
    return m3_mul(m3_rot_z(rz), m3_mul(m3_rot_y(ry), m3_rot_x(rx)));
}

/* Rodrigues rotation about an arbitrary unit axis. */
static inline m3 m3_axis_angle(v3 axis, f32 a)
{
    v3 u = v3_norm(axis);
    f32 c = cosf(a), s = sinf(a), t = 1.0f - c;
    m3 m;
    m.c[0] = V3(t*u.x*u.x + c,      t*u.x*u.y + s*u.z, t*u.x*u.z - s*u.y);
    m.c[1] = V3(t*u.x*u.y - s*u.z,  t*u.y*u.y + c,     t*u.y*u.z + s*u.x);
    m.c[2] = V3(t*u.x*u.z + s*u.y,  t*u.y*u.z - s*u.x, t*u.z*u.z + c);
    return m;
}

/* Camera basis looking from `eye` at `target`. Column 2 is the view direction. */
static inline m3 m3_look_at(v3 eye, v3 target, f32 roll)
{
    v3 fwd   = v3_norm(v3_sub(target, eye));
    v3 world = V3(sinf(roll), cosf(roll), 0.0f);
    v3 right = v3_norm(v3_cross(fwd, world));
    v3 up    = v3_cross(right, fwd);
    m3 m;
    m.c[0] = right;
    m.c[1] = up;
    m.c[2] = fwd;
    return m;
}

/* Build an orthonormal basis around n without a branch-heavy special case. */
static inline m3 m3_basis(v3 n)
{
    f32 s = n.z >= 0.0f ? 1.0f : -1.0f;
    f32 a = -1.0f / (s + n.z);
    f32 b = n.x * n.y * a;
    m3 m;
    m.c[0] = V3(1.0f + s * n.x * n.x * a, s * b, -s * n.x);
    m.c[1] = V3(b, s + n.y * n.y * a, -n.y);
    m.c[2] = n;
    return m;
}

#endif /* DM_VEC_H */
