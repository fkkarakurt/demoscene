/* dm_post.h -- the post-processing chain.
 *
 * This is where a software renderer stops looking like a software renderer.
 * Raw scene output is technically correct and visually flat; the passes below
 * are what add the physical-camera cues the eye reads as "filmed", not "drawn":
 * light bleeding out of bright areas, colour splitting toward the lens edge,
 * falloff in the corners, and grain that keeps flat areas from banding.
 *
 * Everything runs on the HDR buffer in linear light, before tone mapping.
 */
#ifndef DM_POST_H
#define DM_POST_H

#include "dm_fb.h"

typedef struct {
    /* Bloom: energy above `threshold` is spread across a mip chain. `knee`
     * softens the cut-off so a pixel does not pop into bloom as it brightens. */
    f32 bloom_threshold;
    f32 bloom_knee;
    f32 bloom_intensity;
    f32 bloom_radius;    /* 0..1, how much wide mips contribute vs tight ones */

    f32 barrel;          /* lens distortion, ~0.02 is subtle, 0.15 is a fisheye */
    f32 chroma;          /* radial channel split, in fractions of the frame */
    f32 vignette;        /* 0 = off, 1 = heavy corners */
    f32 grain;           /* additive film grain amplitude in linear light */
    u32 frame;           /* animates the grain and the dither */
} dm_post_params;

typedef struct dm_post dm_post;

dm_post_params dm_post_defaults(void);

/* `levels` is the length of the bloom mip chain; 6 covers a 1080p frame with
 * the widest mip about 30 px across, which is a wide, soft, filmic bloom. */
dm_post *dm_post_new(int w, int h, int levels);
void     dm_post_free(dm_post *p);

/* Applies the whole chain to `fb` in place. */
void dm_post_apply(dm_post *p, dm_fb *fb, const dm_post_params *prm);

#endif /* DM_POST_H */
