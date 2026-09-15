/* demo.h -- ENDURANCE: shared context, the camera, and the scene list.
 *
 * Two minutes, landscape, and a story rather than a subject. In January 1915
 * the Weddell Sea closed around Shackleton's ship; in October it crushed her;
 * on 21 November 1915 she sank; and on 5 March 2022 she was found, upright,
 * three thousand metres down, with her name still on her stern. The piece
 * follows her the whole way, and the light follows the calendar: polar night
 * in the winter she was trapped through, a low sun the afternoon she went
 * down, and then less and less of it with every metre of water.
 *
 * Shot the way a film is shot rather than the way the earlier pieces were: 24
 * frames a second through a 180 degree shutter, a real lens with a focal
 * length and an f-number, and a 2.39:1 picture inside a 16:9 frame. Every one
 * of those is a number the renderer uses, not a look applied afterwards --
 * the blur is the exposure, and the out-of-focus ice is the aperture.
 *
 * As in every piece before it, a frame is a pure function of time.
 */
#ifndef ENDURANCE_DEMO_H
#define ENDURANCE_DEMO_H

#include "../../engine/dm_fb.h"
#include "../../engine/dm_font.h"
#include "../../engine/dm_job.h"
#include "../../engine/dm_post.h"
#include "../../engine/dm_rand.h"
#include "song.h"

#define EN_FPS      24
/* Half the frame interval: the 180 degree shutter that film has been shot
 * with for a century, and the reason 24 frames a second does not stutter. */
#define EN_SHUTTER  (0.5 / EN_FPS)
/* The picture's shape inside the 16:9 frame. */
#define EN_SCOPE    2.39f

#define EN_MAX_SLICES 16

/* ---- context ------------------------------------------------------------- */

typedef struct {
    f64 t;              /* video time: the opening of the shutter            */
    int frame;
    int w, h;           /* the picture, not the letterboxed frame around it  */
    f32 aspect;
    int spp;            /* samples per pixel, and exposure slices per frame  */

    f64 bar, beat;

    song_section sec;
    f64 sec_t;          /* seconds into the section                          */
    f64 sec_phase;      /* 0..1 through it                                   */

    int shot;           /* index into the shot list                          */
    f64 shot_t;
    f64 shot_phase;
} en_ctx;

en_ctx en_ctx_make(f64 t, int frame, int w, int h, int spp);

/* The instant exposure slice `k` of `n` stands for: the middle of its share
 * of the open shutter. */
static inline f64 en_slice_time(const en_ctx *c, int k, int n)
{
    return c->t + EN_SHUTTER * ((f64)k + 0.5) / (f64)n;
}

/* ---- the camera -----------------------------------------------------------
 *
 * A thin lens in front of a Super 35 gate. The gate is 24.89 mm wide, and the
 * picture is that width cut to 2.39:1, so a focal length means here what it
 * means on a set: 35 mm is a wide shot, 135 mm is a long lens, and the depth of
 * field at f/2 on the long lens is the same few centimetres it would be.
 */
typedef struct {
    v3  pos;
    v3  rt, up, fw;     /* right, up, forward: orthonormal                   */
    f32 focal_mm;
    f32 fstop;
    f32 focus_m;        /* distance to the plane of focus, along `fw`        */
    f32 gate_mm;        /* horizontal gate width                             */
} en_cam;

#define EN_GATE_S35 24.89f

en_cam en_cam_look(v3 pos, v3 target, f32 roll, f32 focal_mm, f32 fstop, f32 focus_m);

/* Everything an exposure slice needs to know about the world at one instant.
 * A scene's own view struct starts with one of these, so the sampler can find
 * the camera without knowing anything else about the scene. */
typedef struct {
    const en_ctx *c;
    f64    t;
    en_cam cam;
} en_view;

typedef struct {
    v3  ro, rd;
    f32 cone;           /* ray width per metre of distance: one pixel's angle */
    u32 seed;
    int x, y, k;        /* pixel and sample index                            */
} en_ray;

/* A scene's pixel: light arriving along one ray. `t_hit` receives the
 * distance along the ray to whatever stopped it, left alone if nothing did;
 * the sampler turns it into depth along the camera axis, so the lines and
 * particles drawn afterwards can be hidden behind what is nearer. */
typedef v3 (*en_pixel_fn)(const en_ray *r, const void *view, f32 *t_hit);

/* Renders the picture. `views` is an array of `nviews` scene view structs,
 * one per exposure slice, each `view_size` bytes and each starting with an
 * en_view. Each sample of each pixel goes through a different slice, a
 * different point of the pixel and a different point of the lens, so motion
 * blur, antialiasing and depth of field all come out of the same samples. */
void en_shade(dm_fb *fb, f32 *depth, const en_ctx *c, en_pixel_fn fn,
              const void *views, size_t view_size, int nviews);

/* Where a world point lands on the picture, in pixels, and how far along the
 * camera axis it is. Returns 0 behind the camera. */
int en_project(const en_cam *cam, int w, int h, v3 p, v2 *px, f32 *z);

/* Radius in pixels of the blur circle a point at distance `z` makes. */
f32 en_coc_px(const en_cam *cam, int w, f32 z);

/* ---- things too thin to march ----------------------------------------------
 *
 * A ship's rigging is several hundred ropes a few centimetres thick, and at a
 * hundred metres a rope is a pixel wide. Marching them would spend most of a
 * frame converging on things smaller than a sample, so they are drawn the way
 * a plotter would draw them: projected, widened to at least a pixel with their
 * brightness scaled down to match, blurred by the lens like everything else,
 * and hidden wherever the marched picture is nearer. */
typedef struct {
    v3  a, b;
    f32 radius;         /* metres                                            */
    v3  ca, cb;         /* light leaving the rope at each end                */
    f32 alpha;          /* how opaque, 0..1                                  */
} en_line;

void en_lines_draw(dm_fb *fb, const f32 *depth, const en_cam *cam,
                   const en_line *lines, int n, f32 weight);

/* A small glowing or lit particle -- a snowflake, a bubble, marine snow. It
 * spreads over its blur circle with its energy conserved, which is what turns
 * a particle near the lens into a soft disc instead of a bright speck. */
void en_splat(dm_fb *fb, const f32 *depth, const en_cam *cam,
              v3 p, f32 radius, v3 radiance, f32 weight);

/* ---- captions -------------------------------------------------------------
 *
 * Two kinds. A slate names a place and a date, small and wide-spaced in the
 * lower left, the way a documentary does. A line is the story, centred, one
 * sentence at a time, and only where the picture has room for it. */
typedef struct { f32 x, y, w, h; } en_rect;

void en_slate(dm_fb *fb, en_rect pic, const en_ctx *c, const char *top,
              const char *bottom, f64 t0, f64 t1);
/* `y` and `size` are fractions of the picture height. */
void en_line_text(dm_fb *fb, en_rect pic, const en_ctx *c, const char *s,
                  f32 y, f32 size, f64 t0, f64 t1);

/* ---- the frame -------------------------------------------------------------- */

void demo_frame(dm_fb *fb, f32 *depth, const en_ctx *c);
/* Captions, drawn over the finished frame after the lens and the grain, so
 * type stays type. `pic` is where the picture sits inside the letterbox. */
void demo_overlay(dm_fb *frame, en_rect pic, const en_ctx *c);
/* Exposure in stops, applied before bloom so thresholds mean display light. */
f32  demo_exposure(const en_ctx *c);
/* White balance, applied with the exposure. */
v3   demo_grade(const en_ctx *c);
dm_post_params demo_post(const en_ctx *c);

/* ---- scenes -------------------------------------------------------------- */

typedef void (*en_scene_fn)(dm_fb *fb, f32 *depth, const en_ctx *c);

typedef struct {
    f64         bar;    /* where the shot starts                             */
    en_scene_fn fn;
    const char *name;
    f32         ev;     /* exposure, in stops                                */
    v3          grade;  /* white balance: a multiplier in linear light       */
} en_shot_def;

extern const en_shot_def EN_SHOTS[];
extern const int         EN_SHOT_COUNT;

f64 en_shot_start(int i);
f64 en_shot_end(int i);

/* ---- helpers shared by scene code ---------------------------------------- */

static inline f32 en_ease(f32 a, f32 b, f32 x) { return dm_smootherstep(a, b, x); }

static inline v3 en_rot_y(v3 p, f32 a)
{
    f32 c = cosf(a), s = sinf(a);
    return V3(c * p.x + s * p.z, p.y, -s * p.x + c * p.z);
}

static inline v3 en_rot_x(v3 p, f32 a)
{
    f32 c = cosf(a), s = sinf(a);
    return V3(p.x, c * p.y - s * p.z, s * p.y + c * p.z);
}

static inline v3 en_rot_z(v3 p, f32 a)
{
    f32 c = cosf(a), s = sinf(a);
    return V3(c * p.x - s * p.y, s * p.x + c * p.y, p.z);
}

static inline f32 en_sd_box(v3 p, v3 b)
{
    v3 q = v3_sub(v3_abs(p), b);
    return v3_len(v3_max(q, V3(0, 0, 0))) + DM_MIN(v3_maxc(q), 0.0f);
}

static inline f32 en_sd_capsule(v3 p, v3 a, v3 b, f32 r)
{
    v3  pa = v3_sub(p, a), ba = v3_sub(b, a);
    f32 h  = dm_sat(v3_dot(pa, ba) / DM_MAX(v3_dot(ba, ba), 1e-12f));
    return v3_len(v3_sub(pa, v3_scl(ba, h))) - r;
}

/* A capsule whose radius runs from ra at a to rb at b: a mast, a yard. Not an
 * exact distance, but a bound, which is all a march needs. */
static inline f32 en_sd_taper(v3 p, v3 a, v3 b, f32 ra, f32 rb)
{
    v3  pa = v3_sub(p, a), ba = v3_sub(b, a);
    f32 h  = dm_sat(v3_dot(pa, ba) / DM_MAX(v3_dot(ba, ba), 1e-12f));
    return v3_len(v3_sub(pa, v3_scl(ba, h))) - dm_lerp(ra, rb, h);
}

/* Ray against a sphere: the two distances, or 0 if it misses. */
static inline int en_ray_sphere(v3 ro, v3 rd, v3 c, f32 r, f32 *t0, f32 *t1)
{
    v3  oc = v3_sub(ro, c);
    f32 b  = v3_dot(oc, rd);
    f32 cc = v3_dot(oc, oc) - r * r;
    f32 d  = b * b - cc;
    if (d < 0.0f) return 0;
    f32 s = sqrtf(d);
    *t0 = -b - s;
    *t1 = -b + s;
    return *t1 > 0.0f;
}

/* Ray against an axis-aligned box. */
static inline int en_ray_box(v3 ro, v3 rd, v3 lo, v3 hi, f32 *t0, f32 *t1)
{
    f32 a = -1e30f, b = 1e30f;
    f32 o[3] = { ro.x, ro.y, ro.z }, d[3] = { rd.x, rd.y, rd.z };
    f32 l[3] = { lo.x, lo.y, lo.z }, h[3] = { hi.x, hi.y, hi.z };
    for (int i = 0; i < 3; i++) {
        if (fabsf(d[i]) < 1e-9f) {
            if (o[i] < l[i] || o[i] > h[i]) return 0;
            continue;
        }
        f32 inv = 1.0f / d[i];
        f32 u = (l[i] - o[i]) * inv, v = (h[i] - o[i]) * inv;
        if (u > v) { f32 s = u; u = v; v = s; }
        if (u > a) a = u;
        if (v < b) b = v;
    }
    if (b < DM_MAX(a, 0.0f)) return 0;
    *t0 = a;
    *t1 = b;
    return 1;
}

#endif /* ENDURANCE_DEMO_H */
