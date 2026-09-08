/* demo.h -- RUNTIME: shared context and scene list.
 *
 * A thirty-second vertical piece whose subject is the renderer itself. The
 * picture is not simply shown; it is shown being computed, in the order the
 * thread pool actually walks it. Everything else in this file exists to serve
 * that one idea.
 *
 * As in the film, a scene is a pure function of time and nothing carries
 * between frames, so any timestamp can be rendered on its own.
 */
#ifndef RUNTIME_DEMO_H
#define RUNTIME_DEMO_H

#include "../../engine/dm_fb.h"
#include "../../engine/dm_font.h"
#include "../../engine/dm_job.h"
#include "../../engine/dm_post.h"
#include "../../engine/dm_rand.h"
#include "song.h"

typedef struct {
    f64 t;
    int frame;
    int w, h;
    f32 aspect;      /* < 1 here: this piece is composed for 1080x1920 */

    f64 bar;
    f64 beat;
    f32 beat_hit;
    f32 bar_hit;

    song_section sec;
    f64 sec_t;
    f64 sec_phase;
} rt_ctx;

rt_ctx rt_ctx_make(f64 t, int frame, int w, int h);

extern int rt_aa;

typedef v3 (*rt_pixel_fn)(v2 uv, const rt_ctx *c, u32 seed);

void rt_shade(dm_fb *fb, const rt_ctx *c, rt_pixel_fn fn, int samples);

/* ---- the reveal ----------------------------------------------------------
 *
 * The engine renders one row per work item, and eight workers pull row indices
 * off a shared counter. So at any instant during a real render there is a
 * ragged front a few rows deep with finished pixels above it and untouched
 * memory below -- which is exactly what this draws, slowed from milliseconds
 * to seconds.
 *
 * `progress` is 0..1 over the frame height. Rows past the front are cleared,
 * rows at the front are lit, and the raggedness comes from giving each row a
 * small deterministic cost, the way a row of sky costs less than a row of
 * fractal.
 */
void rt_reveal(dm_fb *fb, f32 progress);

/* A caption, in the demo's own face, held between `t0` and `t1` with a short
 * fade at each end. Placed in the lower third: on a phone the top of a short
 * is covered by the title and the bottom by the channel row, and the lower
 * third is the widest band that survives both. */
void rt_caption(dm_fb *fb, const rt_ctx *c, const char *s, f64 t0, f64 t1);

/* ---- scenes -------------------------------------------------------------- */

/* One structure, two cameras. A short has no time to establish a second world,
 * and cutting between two views of the same one reads as intent rather than as
 * a compilation. */
void scene_assemble(dm_fb *fb, const rt_ctx *c);   /* bar 0  : rising, revealed  */
void scene_lock    (dm_fb *fb, const rt_ctx *c);   /* bar 2  : complete, drifting*/
void scene_drive   (dm_fb *fb, const rt_ctx *c);   /* bar 6  : low camera, up    */
void scene_expose  (dm_fb *fb, const rt_ctx *c);   /* bar 11 : the waveform      */
void scene_sign    (dm_fb *fb, const rt_ctx *c);   /* bar 14 : the closing card  */

void demo_frame(dm_fb *fb, const rt_ctx *c);
dm_post_params demo_post(const rt_ctx *c);

/* The audio the piece is playing, so scene_expose can draw the samples that
 * are about to be heard rather than a decorative sine. Set once by main(). */
void rt_set_audio(const dm_audio *a);
const dm_audio *rt_audio(void);

/* ---- helpers shared by scene code ---------------------------------------- */

/* not used outside this file any more */

static inline v3 rt_ray(v2 uv, f32 fov_mul)
{
    return v3_norm(V3(uv.x, uv.y, fov_mul));
}

/* Camera basis looking from `ro` at `ta`. */
static inline void rt_basis(v3 ro, v3 ta, v3 *rt, v3 *up, v3 *fw)
{
    *fw = v3_norm(v3_sub(ta, ro));
    *rt = v3_norm(v3_cross(V3(0, 1, 0), *fw));
    *up = v3_cross(*fw, *rt);
}

static inline f32 rt_vignette(v2 uv, f32 amount)
{
    return 1.0f - amount * dm_smoothstep(0.35f, 2.4f, v2_dot(uv, uv));
}

#endif /* RUNTIME_DEMO_H */
