/* demo.h -- COLD START: shared context and scene list.
 *
 * A scene is a pure function of time. Nothing carries state between frames,
 * which is what lets the renderer jump straight to any timestamp (invaluable
 * when iterating on one shot) and what guarantees the preview and the final
 * render show exactly the same picture.
 */
#ifndef COLDSTART_DEMO_H
#define COLDSTART_DEMO_H

#include "../../engine/dm_fb.h"
#include "../../engine/dm_font.h"
#include "../../engine/dm_job.h"
#include "../../engine/dm_post.h"
#include "../../engine/dm_rand.h"
#include "song.h"

typedef struct {
    f64 t;          /* seconds from the top of the demo */
    int frame;
    int w, h;
    f32 aspect;

    /* Musical clocks. Everything that moves in this demo is driven off one of
     * these rather than off wall time, which is what keeps it in step. */
    f64 bar;        /* fractional bar number */
    f64 beat;       /* fractional beat number */
    f32 beat_hit;   /* 1.0 on each beat, decaying exponentially */
    f32 bar_hit;    /* the same, once per bar */
    f32 phrase;     /* 0..1 across each four-bar phrase */

    song_section sec;
    f64 sec_t;      /* seconds into the current section */
    f64 sec_phase;  /* 0..1 across the current section */
} yk_ctx;

yk_ctx yk_ctx_make(f64 t, int frame, int w, int h);

/* Anti-aliasing samples per pixel, set once by main(). Scenes read it rather
 * than hard-coding a count, so a preview can run at 1 and the final render at
 * whatever the schedule allows without touching any scene code. */
extern int yk_aa;

/* Per-pixel shader. `uv` is centred, -1..1 vertically, y up, x scaled by the
 * aspect ratio. `seed` is unique per pixel and per sample. */
typedef v3 (*yk_pixel_fn)(v2 uv, const yk_ctx *c, u32 seed);

/* Runs `fn` over the whole framebuffer on the thread pool, averaging
 * `samples` jittered samples per pixel. */
void yk_shade(dm_fb *fb, const yk_ctx *c, yk_pixel_fn fn, int samples);

/* Scenes, in running order. Each one owns the whole frame. */
void scene_void    (dm_fb *fb, const yk_ctx *c);   /* bar 0  */
void scene_ignite  (dm_fb *fb, const yk_ctx *c);   /* bar 4  */
void scene_tunnel  (dm_fb *fb, const yk_ctx *c);   /* bar 8  */
void scene_plasma  (dm_fb *fb, const yk_ctx *c);   /* bar 16 */
void scene_mirror  (dm_fb *fb, const yk_ctx *c);   /* bar 24 */
void scene_fracture(dm_fb *fb, const yk_ctx *c);   /* bar 28 */
void scene_fractal (dm_fb *fb, const yk_ctx *c);   /* bar 32 */
void scene_lattice (dm_fb *fb, const yk_ctx *c);   /* bar 48 */
void scene_end     (dm_fb *fb, const yk_ctx *c);   /* bar 56 */

/* The greetings scroller. It runs from the top of the climax through the end
 * card, over whatever scene is playing -- text over an effect is the oldest
 * layout in the scene, and the only one that lets a two-minute demo say
 * anything at all. */
void yk_scroller(dm_fb *fb, const yk_ctx *c);

/* Dispatches to the right scene and lays the overlays on top. */
void demo_frame(dm_fb *fb, const yk_ctx *c);

/* Post parameters vary by section -- the breakdown wants a different lens
 * than the drop does. */
dm_post_params demo_post(const yk_ctx *c);

/* ---- drawing helpers ---------------------------------------------------- */

/* Centred uv (as handed to a pixel shader) back to pixel coordinates. */
v2 yk_to_screen(const yk_ctx *c, v2 uv);

/* Soft round additive dot -- the primitive behind every particle and star. */
void yk_blob(dm_fb *fb, v2 p, f32 radius, v3 col);

/* Additive capsule between two points, for motion-blurred star trails. */
void yk_streak(dm_fb *fb, v2 a, v2 b, f32 width, v3 col);

/* ---- helpers shared by scene code -------------------------------------- */

/* Ray against a sphere at the origin. Returns 0 if the ray misses or the
 * sphere is entirely behind it.
 *
 * This is the single most valuable thing to put in front of a raymarcher for
 * a bounded object. Without it, every ray that misses the object still walks
 * its full step budget through empty space -- and when the object is small in
 * frame, that is most of the rays and most of the render time. With it, a
 * miss costs one dot product and the marcher starts at the surface of the
 * bound rather than at the camera. */
static inline int yk_sphere(v3 ro, v3 rd, f32 radius, f32 *t_near, f32 *t_far)
{
    f32 b = v3_dot(ro, rd);
    f32 c = v3_dot(ro, ro) - radius * radius;
    f32 h = b * b - c;
    if (h < 0.0f) return 0;
    h = sqrtf(h);
    *t_near = -b - h;
    *t_far  = -b + h;
    return *t_far > 0.0f;
}

/* Standard pinhole ray for a given screen position. */
static inline v3 yk_ray(v2 uv, f32 fov_mul)
{
    return v3_norm(V3(uv.x, uv.y, fov_mul));
}

/* Exponential height/distance fog. */
static inline v3 yk_fog(v3 col, v3 fog_col, f32 dist, f32 density)
{
    f32 f = 1.0f - expf(-dist * density);
    return v3_lerp(col, fog_col, dm_sat(f));
}

/* Screen-space radial falloff, the cheapest way to keep the eye centred. */
static inline f32 yk_vignette(v2 uv, f32 amount)
{
    return 1.0f - amount * dm_smoothstep(0.4f, 2.2f, v2_dot(uv, uv));
}

#endif /* COLDSTART_DEMO_H */
