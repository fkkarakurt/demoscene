/* dm_fb.h -- HDR floating point framebuffer.
 *
 * Three 32-bit floats per pixel, linear light, no clamping. Scenes render into
 * one of these, the post chain reads and writes them, and only dm_fb_resolve()
 * at the very end turns light into bytes.
 */
#ifndef DM_FB_H
#define DM_FB_H

#include "dm_color.h"

typedef struct {
    int  w, h;
    f32 *px;   /* w * h * 3, row major, linear RGB */
} dm_fb;

dm_fb *dm_fb_new(int w, int h);
void   dm_fb_free(dm_fb *f);

void dm_fb_clear(dm_fb *f, v3 c);
void dm_fb_copy(dm_fb *dst, const dm_fb *src);      /* same dimensions */
void dm_fb_scale(dm_fb *f, f32 s);
void dm_fb_mul(dm_fb *f, v3 s);
void dm_fb_add_fb(dm_fb *dst, const dm_fb *src, f32 weight);
void dm_fb_mix_fb(dm_fb *dst, const dm_fb *src, f32 t);

/* Box filter into a smaller buffer. Handles non-integer ratios. */
void dm_fb_downsample(dm_fb *dst, const dm_fb *src);
/* Bilinear magnify into a larger buffer. */
void dm_fb_upsample(dm_fb *dst, const dm_fb *src);

/* Tone map + sRGB encode + dither into an 8-bit RGB buffer of w*h*3 bytes.
 * `frame` seeds the dither so it does not sit still and read as fixed grain. */
void dm_fb_resolve(const dm_fb *f, u8 *rgb24, f32 exposure, u32 frame);

/* ---- inline pixel access ----------------------------------------------- */

static inline v3 dm_fb_get(const dm_fb *f, int x, int y)
{
    const f32 *p = f->px + ((size_t)y * f->w + x) * 3;
    return V3(p[0], p[1], p[2]);
}

static inline void dm_fb_set(dm_fb *f, int x, int y, v3 c)
{
    f32 *p = f->px + ((size_t)y * f->w + x) * 3;
    p[0] = c.x; p[1] = c.y; p[2] = c.z;
}

static inline void dm_fb_add(dm_fb *f, int x, int y, v3 c)
{
    f32 *p = f->px + ((size_t)y * f->w + x) * 3;
    p[0] += c.x; p[1] += c.y; p[2] += c.z;
}

/* Clamped edge addressing. */
static inline v3 dm_fb_getc(const dm_fb *f, int x, int y)
{
    x = x < 0 ? 0 : (x >= f->w ? f->w - 1 : x);
    y = y < 0 ? 0 : (y >= f->h ? f->h - 1 : y);
    return dm_fb_get(f, x, y);
}

/* Bilinear sample in pixel coordinates (pixel centres at .5). */
static inline v3 dm_fb_sample(const dm_fb *f, f32 x, f32 y)
{
    x -= 0.5f; y -= 0.5f;
    f32 fx = floorf(x), fy = floorf(y);
    f32 tx = x - fx,    ty = y - fy;
    int x0 = (int)fx,   y0 = (int)fy;

    v3 a = dm_fb_getc(f, x0,     y0);
    v3 b = dm_fb_getc(f, x0 + 1, y0);
    v3 c = dm_fb_getc(f, x0,     y0 + 1);
    v3 d = dm_fb_getc(f, x0 + 1, y0 + 1);
    return v3_lerp(v3_lerp(a, b, tx), v3_lerp(c, d, tx), ty);
}

/* Sample with UVs in 0..1, y up. This is what scene code should use so that
 * effects stay resolution independent between preview and final render. */
static inline v3 dm_fb_sample_uv(const dm_fb *f, v2 uv)
{
    return dm_fb_sample(f, uv.x * (f32)f->w, (1.0f - uv.y) * (f32)f->h);
}

/* Energy-conserving point splat with bilinear weights -- the primitive behind
 * every particle, star and bob in the demo. */
static inline void dm_fb_splat(dm_fb *f, f32 x, f32 y, v3 c)
{
    x -= 0.5f; y -= 0.5f;
    f32 fx = floorf(x), fy = floorf(y);
    f32 tx = x - fx,    ty = y - fy;
    int x0 = (int)fx,   y0 = (int)fy;

    for (int j = 0; j < 2; j++) {
        int yy = y0 + j;
        if (yy < 0 || yy >= f->h) continue;
        f32 wy = j ? ty : 1.0f - ty;
        for (int i = 0; i < 2; i++) {
            int xx = x0 + i;
            if (xx < 0 || xx >= f->w) continue;
            f32 w = wy * (i ? tx : 1.0f - tx);
            dm_fb_add(f, xx, yy, v3_scl(c, w));
        }
    }
}

#endif /* DM_FB_H */
