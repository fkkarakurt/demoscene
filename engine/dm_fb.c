#include "dm_fb.h"
#include <stdlib.h>
#include <string.h>

dm_fb *dm_fb_new(int w, int h)
{
    dm_fb *f = (dm_fb *)malloc(sizeof(dm_fb));
    if (!f) return NULL;
    f->w  = w;
    f->h  = h;
    f->px = (f32 *)calloc((size_t)w * h * 3, sizeof(f32));
    if (!f->px) { free(f); return NULL; }
    return f;
}

void dm_fb_free(dm_fb *f)
{
    if (!f) return;
    free(f->px);
    free(f);
}

void dm_fb_clear(dm_fb *f, v3 c)
{
    size_t n = (size_t)f->w * f->h;
    if (c.x == 0.0f && c.y == 0.0f && c.z == 0.0f) {
        memset(f->px, 0, n * 3 * sizeof(f32));
        return;
    }
    for (size_t i = 0; i < n; i++) {
        f->px[i * 3 + 0] = c.x;
        f->px[i * 3 + 1] = c.y;
        f->px[i * 3 + 2] = c.z;
    }
}

void dm_fb_copy(dm_fb *dst, const dm_fb *src)
{
    memcpy(dst->px, src->px, (size_t)src->w * src->h * 3 * sizeof(f32));
}

void dm_fb_scale(dm_fb *f, f32 s)
{
    size_t n = (size_t)f->w * f->h * 3;
    for (size_t i = 0; i < n; i++) f->px[i] *= s;
}

void dm_fb_mul(dm_fb *f, v3 s)
{
    size_t n = (size_t)f->w * f->h;
    for (size_t i = 0; i < n; i++) {
        f->px[i * 3 + 0] *= s.x;
        f->px[i * 3 + 1] *= s.y;
        f->px[i * 3 + 2] *= s.z;
    }
}

void dm_fb_add_fb(dm_fb *dst, const dm_fb *src, f32 weight)
{
    size_t n = (size_t)dst->w * dst->h * 3;
    for (size_t i = 0; i < n; i++) dst->px[i] += src->px[i] * weight;
}

void dm_fb_mix_fb(dm_fb *dst, const dm_fb *src, f32 t)
{
    size_t n = (size_t)dst->w * dst->h * 3;
    for (size_t i = 0; i < n; i++) dst->px[i] += (src->px[i] - dst->px[i]) * t;
}

void dm_fb_downsample(dm_fb *dst, const dm_fb *src)
{
    f32 sx = (f32)src->w / (f32)dst->w;
    f32 sy = (f32)src->h / (f32)dst->h;

    for (int y = 0; y < dst->h; y++) {
        int y0 = (int)(y * sy), y1 = (int)((y + 1) * sy);
        if (y1 <= y0) y1 = y0 + 1;
        if (y1 > src->h) y1 = src->h;

        for (int x = 0; x < dst->w; x++) {
            int x0 = (int)(x * sx), x1 = (int)((x + 1) * sx);
            if (x1 <= x0) x1 = x0 + 1;
            if (x1 > src->w) x1 = src->w;

            v3 acc = V3(0, 0, 0);
            for (int j = y0; j < y1; j++)
                for (int i = x0; i < x1; i++)
                    acc = v3_add(acc, dm_fb_get(src, i, j));

            f32 inv = 1.0f / (f32)((x1 - x0) * (y1 - y0));
            dm_fb_set(dst, x, y, v3_scl(acc, inv));
        }
    }
}

void dm_fb_upsample(dm_fb *dst, const dm_fb *src)
{
    f32 sx = (f32)src->w / (f32)dst->w;
    f32 sy = (f32)src->h / (f32)dst->h;

    for (int y = 0; y < dst->h; y++) {
        f32 v = ((f32)y + 0.5f) * sy;
        for (int x = 0; x < dst->w; x++) {
            f32 u = ((f32)x + 0.5f) * sx;
            dm_fb_set(dst, x, y, dm_fb_sample(src, u, v));
        }
    }
}

void dm_fb_resolve(const dm_fb *f, u8 *rgb24, f32 exposure, u32 frame)
{
    for (int y = 0; y < f->h; y++) {
        for (int x = 0; x < f->w; x++) {
            v3 c = v3_scl(dm_fb_get(f, x, y), exposure);

            /* NaNs from a divide-by-zero in scene code would otherwise become
             * permanent black or white specks in the encoded video. */
            if (!(c.x == c.x)) c.x = 0.0f;
            if (!(c.y == c.y)) c.y = 0.0f;
            if (!(c.z == c.z)) c.z = 0.0f;

            c = dm_tonemap_aces(c);
            c = dm_to_srgb(c);

            f32 d = dm_dither_tpdf(x, y, frame);
            u8 *o = rgb24 + ((size_t)y * f->w + x) * 3;
            o[0] = (u8)(dm_sat(c.x + d) * 255.0f + 0.5f);
            o[1] = (u8)(dm_sat(c.y + d) * 255.0f + 0.5f);
            o[2] = (u8)(dm_sat(c.z + d) * 255.0f + 0.5f);
        }
    }
}
