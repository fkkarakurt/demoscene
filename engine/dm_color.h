/* dm_color.h -- colour spaces, tone mapping and palette generation.
 *
 * The renderer works entirely in unbounded linear light. Values above 1.0 are
 * normal and wanted: they are what makes bloom bloom and what gives the tone
 * mapper something to roll off. Conversion to 8-bit sRGB happens once, at the
 * very end of the post chain.
 */
#ifndef DM_COLOR_H
#define DM_COLOR_H

#include "dm_vec.h"
#include "dm_rand.h"

/* ---- sRGB transfer ----------------------------------------------------- */

static inline f32 dm_srgb_decode(f32 c)
{
    return c <= 0.04045f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f);
}

static inline f32 dm_srgb_encode(f32 c)
{
    c = dm_sat(c);
    return c <= 0.0031308f ? c * 12.92f : 1.055f * powf(c, 1.0f / 2.4f) - 0.055f;
}

static inline v3 dm_to_srgb(v3 c)
{
    return V3(dm_srgb_encode(c.x), dm_srgb_encode(c.y), dm_srgb_encode(c.z));
}

/* Convenience: author a colour as a familiar 0..255 sRGB triple, get linear. */
static inline v3 dm_rgb8(int r, int g, int b)
{
    return V3(dm_srgb_decode((f32)r / 255.0f),
              dm_srgb_decode((f32)g / 255.0f),
              dm_srgb_decode((f32)b / 255.0f));
}

static inline f32 dm_luma(v3 c) { return 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z; }

static inline v3 dm_saturate_col(v3 c, f32 s)
{
    f32 l = dm_luma(c);
    return v3_lerp(V3s(l), c, s);
}

/* ---- tone mapping ------------------------------------------------------ */

/* Krzysztof Narkowicz's ACES fit. Cheap, and it desaturates highlights the way
 * film does instead of clipping them to flat white the way Reinhard does. */
static inline v3 dm_tonemap_aces(v3 x)
{
    const f32 a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
    v3 num = v3_mul(x, v3_adds(v3_scl(x, a), b));
    v3 den = v3_add(v3_mul(x, v3_adds(v3_scl(x, c), d)), V3s(e));
    return v3_sat(v3_div(num, den));
}

/* Reinhard with a white point, for scenes where ACES crushes too much. */
static inline v3 dm_tonemap_reinhard(v3 x, f32 white)
{
    v3 num = v3_mul(x, v3_adds(v3_scl(x, 1.0f / (white * white)), 1.0f));
    return v3_sat(v3_div(num, v3_adds(x, 1.0f)));
}

/* ---- palettes ---------------------------------------------------------- */

/* Inigo Quilez's cosine palette: colour(t) = a + b * cos(TAU * (c*t + d)).
 * Four vectors describe an entire smooth, loopable colour ramp. */
static inline v3 dm_pal(f32 t, v3 a, v3 b, v3 c, v3 d)
{
    return V3(a.x + b.x * cosf(DM_TAU * (c.x * t + d.x)),
              a.y + b.y * cosf(DM_TAU * (c.y * t + d.y)),
              a.z + b.z * cosf(DM_TAU * (c.z * t + d.z)));
}

/* The house palette for this series: deep indigo shadows, magenta mids,
 * cyan/amber highlights. Defined once so every scene shares a look. */
static inline v3 dm_pal_house(f32 t)
{
    return dm_pal(t,
                  V3(0.42f, 0.28f, 0.46f),
                  V3(0.45f, 0.30f, 0.42f),
                  V3(1.00f, 1.05f, 0.92f),
                  V3(0.02f, 0.28f, 0.62f));
}

static inline v3 dm_hsv(f32 h, f32 s, f32 v)
{
    h = dm_fract(h) * 6.0f;
    f32 c = v * s;
    f32 x = c * (1.0f - fabsf(dm_mod(h, 2.0f) - 1.0f));
    f32 m = v - c;
    v3 r;
    if      (h < 1.0f) r = V3(c, x, 0);
    else if (h < 2.0f) r = V3(x, c, 0);
    else if (h < 3.0f) r = V3(0, c, x);
    else if (h < 4.0f) r = V3(0, x, c);
    else if (h < 5.0f) r = V3(x, 0, c);
    else               r = V3(c, 0, x);
    return v3_adds(r, m);
}

/* Planckian locus approximation -- lets lights be specified in kelvin.
 * Returns a linear colour normalised to luminance 1. */
static inline v3 dm_kelvin(f32 k)
{
    f32 t = dm_clamp(k, 1000.0f, 15000.0f) / 100.0f;
    f32 r, g, b;

    if (t <= 66.0f) {
        r = 255.0f;
        g = 99.4708025861f * logf(t) - 161.1195681661f;
        b = t <= 19.0f ? 0.0f : 138.5177312231f * logf(t - 10.0f) - 305.0447927307f;
    } else {
        r = 329.698727446f * powf(t - 60.0f, -0.1332047592f);
        g = 288.1221695283f * powf(t - 60.0f, -0.0755148492f);
        b = 255.0f;
    }
    v3 c = V3(dm_sat(r / 255.0f), dm_sat(g / 255.0f), dm_sat(b / 255.0f));
    c = V3(dm_srgb_decode(c.x), dm_srgb_decode(c.y), dm_srgb_decode(c.z));
    f32 l = dm_luma(c);
    return l > 1e-6f ? v3_scl(c, 1.0f / l) : V3s(1.0f);
}

/* ---- dithering --------------------------------------------------------- */

/* Triangular probability density function dither. Applied at quantisation time
 * it removes 8-bit banding in dark gradients -- the single most visible
 * quality difference between a naive renderer and a good one. */
static inline f32 dm_dither_tpdf(int x, int y, u32 frame)
{
    f32 a = dm_u32_to_f32(dm_hash3i(x, y, (i32)frame, 0x1234567u));
    f32 b = dm_u32_to_f32(dm_hash3i(x, y, (i32)frame, 0x89abcdeu));
    return (a - b) * (1.0f / 255.0f);
}

#endif /* DM_COLOR_H */
