/* demo.h -- TRACE: shared context and scene list.
 *
 * Thirty seconds, vertical, in which nothing is drawn. Every frame is the
 * soundtrack, plotted: left channel across, right channel up, the way an
 * oscilloscope in XY mode shows a stereo signal, with the past laid out
 * behind the present so the camera has something to move around.
 *
 * The scenes here do not render anything of their own. Each one is a camera
 * and a phosphor -- where to stand, what to focus on, how long the trace
 * persists -- and the picture is whatever the music was.
 *
 * As in the other pieces, a frame is a pure function of time: the audio is
 * rendered once, before any picture, and every frame reads it.
 */
#ifndef TRACE_DEMO_H
#define TRACE_DEMO_H

#include "../../engine/dm_fb.h"
#include "../../engine/dm_job.h"
#include "../../engine/dm_post.h"
#include "../../engine/dm_rand.h"
#include "beam.h"
#include "song.h"

typedef struct {
    f64 t;              /* video time: the start of the frame's exposure     */
    f64 t0, t1;         /* the slice of the exposure this pass draws         */
    f64 shutter;        /* the whole exposure                                */
    int frame;
    int w, h;
    f32 aspect;

    f64 bar;
    f64 beat;
    f32 beat_hit;
    f32 bar_hit;

    song_section sec;
    f64 sec_t;
    f64 sec_phase;
} tr_ctx;

/* `t` is the start of the frame, [t0, t1] the slice of its exposure. */
tr_ctx tr_ctx_make(f64 t, f64 t0, f64 t1, f64 shutter, int frame, int w, int h);

/* The soundtrack, which is also the picture. Set once by main.c. */
void            tr_set_audio(const dm_audio *a);
const dm_audio *tr_audio(void);

/* A phosphor and a time base with defaults that every scene starts from. */
tr_beam   tr_beam_default(const tr_ctx *c);
tr_camera tr_camera_front(f32 dist);

/* Draws the trace for this slice with this camera. */
void tr_expose(dm_fb *fb, const tr_ctx *c, const tr_camera *cam, const tr_beam *b);

/* ---- scenes -------------------------------------------------------------- */

void scene_tune (dm_fb *fb, const tr_ctx *c);   /* bar 0  : dot, circle, lock */
void scene_pulse(dm_fb *fb, const tr_ctx *c);   /* bar 2  : spirals           */
void scene_chaos(dm_fb *fb, const tr_ctx *c);   /* bar 5  : the butterfly     */
void scene_drop (dm_fb *fb, const tr_ctx *c);   /* bar 8  : time, from beside */
void scene_name (dm_fb *fb, const tr_ctx *c);   /* bar 13 : the word          */

/* Adds this slice's light into `fb`. The caller clears it once per frame. */
void demo_frame(dm_fb *fb, const tr_ctx *c);
/* Once per frame, after every slice is in. */
void demo_finish(dm_fb *fb, const tr_ctx *c);
dm_post_params demo_post(const tr_ctx *c);

/* ---- helpers shared by scene code ---------------------------------------- */

static inline f32 tr_ease(f32 a, f32 b, f32 x) { return dm_smootherstep(a, b, x); }

/* A point on a circle of radius r about the time axis, at depth z: where an
 * orbiting camera stands. Angle 0 is straight in front. */
static inline v3 tr_orbit(f32 r, f32 yaw, f32 pitch, f32 z)
{
    return V3(r * sinf(yaw) * cosf(pitch), r * sinf(pitch), z + r * cosf(yaw) * cosf(pitch));
}

#endif /* TRACE_DEMO_H */
