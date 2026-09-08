/* demo.h -- LIFTOFF: shared context and scene list.
 *
 * Thirty seconds, vertical, of something that actually exists. The two pieces
 * before this one were abstract on purpose -- a fractal owes nobody an
 * explanation. A rocket does. Every proportion here is a real proportion, the
 * trajectory comes out of an integration rather than a curve editor, and the
 * numbers in the corner of the frame are the numbers steering the camera.
 *
 * As in the other two, a scene is a pure function of time and nothing carries
 * between frames, so any timestamp can be rendered on its own.
 */
#ifndef LIFTOFF_DEMO_H
#define LIFTOFF_DEMO_H

#include "../../engine/dm_fb.h"
#include "../../engine/dm_font.h"
#include "../../engine/dm_job.h"
#include "../../engine/dm_post.h"
#include "../../engine/dm_rand.h"
#include "flight.h"
#include "song.h"

typedef struct {
    f64 t;              /* video time, seconds                              */
    int frame;
    int w, h;
    f32 aspect;         /* < 1: composed for 1080x1920                      */

    f64 bar;
    f64 beat;
    f32 beat_hit;
    f32 bar_hit;

    song_section sec;
    f64 sec_t;
    f64 sec_phase;

    /* The flight. `mt` is mission time -- negative during the count -- and it
     * runs faster than video time by a factor that changes only on a cut. */
    f64      mt;
    lo_state fl;
} lo_ctx;

lo_ctx lo_ctx_make(f64 t, int frame, int w, int h);

/* Video seconds to mission seconds, and back. The inverse is what lets a
 * caption be pinned to an event -- maximum dynamic pressure, say -- by asking
 * the trajectory when it happens rather than by hard-coding a timestamp that
 * silently stops being true the moment the model changes. */
f64 lo_mission_time(f64 t);
f64 lo_video_time(f64 mission_t);

extern int lo_aa;

typedef v3 (*lo_pixel_fn)(v2 uv, const lo_ctx *c, u32 seed);

void lo_shade(dm_fb *fb, const lo_ctx *c, lo_pixel_fn fn, int samples);

/* ---- the sun -------------------------------------------------------------
 *
 * Five and a half degrees below the horizon at the pad: late nautical
 * twilight. Dark enough that the vehicle has to be lit by floodlights and then
 * by its own engines, and stars are out; light enough that there is a band of
 * colour along the horizon toward it.
 *
 * The angle is not a mood, it is a schedule. The visible horizon drops below
 * the geometric one by roughly sqrt(2h/R), so the altitude at which a vehicle
 * first sees a sun this far down is h = R*sin(5.5 deg)^2/2 -- twenty nine
 * kilometres, which the trajectory reaches ninety-five seconds in. Put the sun
 * at eight degrees and sunrise arrives at sixty-two kilometres, and most of the
 * climb is played out against black; put it at four and the pad is not dark
 * any more. Everything about how this piece is lit is downstream of that one
 * number. */
extern const v3 LO_SUN_DIR;

/* ---- the world ------------------------------------------------------------
 *
 * Everything a ray that misses the vehicle can hit: sky, stars, the sun, the
 * planet and its cloud deck. `alt` is metres above the pad and `dr` metres
 * downrange, so the same function serves a camera on the ground and one two
 * hundred kilometres up. */
v3 lo_world(v3 rd, f32 alt, f32 dr, u32 seed);

/* Can a vehicle at this altitude see the sun? Zero on the pad and one above
 * about seventy kilometres, and the transition between them is the whole
 * lighting arc of the piece. It is pure geometry: the horizon drops away as
 * the square root of altitude, and at some point it drops past the sun. */
f32 lo_sun_visible(f32 alt);

/* ---- the vehicle ----------------------------------------------------------
 *
 * In its own frame: the engine plane at y = 0, the nose at y = LO_HEIGHT, and
 * the split at the top of the interstage. `sep` is how far the two halves have
 * drawn apart, in metres, and is zero for the whole first stage burn. */
enum { LO_PART_TANK = 0, LO_PART_BLACK, LO_PART_ENGINE, LO_PART_NOSE };

f32 lo_vehicle_sdf(v3 p, f32 sep, int *part);
v3  lo_vehicle_normal(v3 p, f32 sep);
v3  lo_vehicle_albedo(v3 p, int part, u32 seed);

/* The exhaust, marched as a volume from `ro` along `rd` up to `t_max`, in the
 * vehicle frame. Thin and violet in vacuum, a hundred metres of white fire at
 * sea level -- the shape is a function of the air, not of the section. */
v3 lo_plume(v3 ro, v3 rd, f32 t_max, const lo_ctx *c, u32 seed);

/* Light the plume throws on everything else. Intensity falls with the square
 * of the distance from the engine plane, so the pad, the tower and the vehicle
 * are all lit by the same source. */
v3 lo_plume_light(const lo_ctx *c);

/* ---- captions and the readout -------------------------------------------- */

/* A caption in the demo's own face, held between `t0` and `t1`. Lower third:
 * on a phone the top of a short is covered by the title and the bottom by the
 * channel row. */
void lo_caption(dm_fb *fb, const lo_ctx *c, const char *s, f64 t0, f64 t1);

/* The telemetry strip. Mission clock, altitude, speed, and the event that is
 * happening -- all read out of `c->fl`, none of it typed in by hand. */
void lo_hud(dm_fb *fb, const lo_ctx *c);

/* ---- scenes -------------------------------------------------------------- */

void scene_hold (dm_fb *fb, const lo_ctx *c);   /* bar 0  : the count        */
void scene_lift (dm_fb *fb, const lo_ctx *c);   /* bar 2  : release          */
void scene_climb(dm_fb *fb, const lo_ctx *c);   /* bar 5  : out of the air   */
void scene_stage(dm_fb *fb, const lo_ctx *c);   /* bar 9  : cut-off, split   */
void scene_orbit(dm_fb *fb, const lo_ctx *c);   /* bar 12 : the limb, a card */

void demo_frame(dm_fb *fb, const lo_ctx *c);
dm_post_params demo_post(const lo_ctx *c);

/* ---- helpers shared by scene code ---------------------------------------- */

static inline v3 lo_ray(v2 uv, f32 fov_mul)
{
    return v3_norm(V3(uv.x, uv.y, fov_mul));
}

/* Camera basis looking from `ro` at `ta`. */
static inline void lo_basis(v3 ro, v3 ta, v3 *rt, v3 *up, v3 *fw)
{
    *fw = v3_norm(v3_sub(ta, ro));
    v3 world_up = fabsf(fw->y) > 0.995f ? V3(0, 0, 1) : V3(0, 1, 0);
    *rt = v3_norm(v3_cross(world_up, *fw));
    *up = v3_cross(*fw, *rt);
}

static inline f32 lo_vignette(v2 uv, f32 amount)
{
    return 1.0f - amount * dm_smoothstep(0.35f, 2.4f, v2_dot(uv, uv));
}

static inline v3 lo_rot_y(v3 p, f32 a)
{
    f32 c = cosf(a), s = sinf(a);
    return V3(c * p.x - s * p.z, p.y, s * p.x + c * p.z);
}

static inline v3 lo_rot_x(v3 p, f32 a)
{
    f32 c = cosf(a), s = sinf(a);
    return V3(p.x, c * p.y - s * p.z, s * p.y + c * p.z);
}

static inline v3 lo_rot_z(v3 p, f32 a)
{
    f32 c = cosf(a), s = sinf(a);
    return V3(c * p.x - s * p.y, s * p.x + c * p.y, p.z);
}

#endif /* LIFTOFF_DEMO_H */
