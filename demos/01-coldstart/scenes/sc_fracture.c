/* sc_fracture.c -- FRACTURE (bar 28).
 *
 * The build. Eight bars of snare roll, and the picture has to come apart at
 * the same rate.
 *
 * The trick that makes this cheap: the underlying image is a function of uv,
 * so "displace a block of the picture" is just "evaluate the function at a
 * different uv". Nothing is buffered, nothing is copied, and the glitch can
 * be as violent as the section needs without costing a second render.
 *
 * Underneath it is the other icon of the genre -- a perspective grid running
 * to a banded sun -- so there is something recognisable to destroy.
 */
#include "../demo.h"

static v3 grid_scene(v2 uv, f32 t, f32 beat)
{
    const f32 HORIZON = -0.06f;

    if (uv.y < HORIZON) {
        /* Perspective by inversion: depth is one over the distance below the
         * horizon, which is exactly the projection a flat plane produces. */
        f32 z = 1.0f / (HORIZON - uv.y);
        f32 x = uv.x * z;

        f32 gz = fabsf(dm_fract(z * 0.55f + t * 0.85f) - 0.5f) * 2.0f;
        f32 gx = fabsf(dm_fract(x * 0.42f) - 0.5f) * 2.0f;

        /* Line width in screen space has to shrink with depth or the far
         * grid turns into a solid sheet of aliasing. */
        f32 wz = dm_clamp(0.020f * z, 0.010f, 0.55f);
        f32 wx = dm_clamp(0.014f * z, 0.008f, 0.55f);

        f32 lines = powf(1.0f - gz, 1.0f / wz) + powf(1.0f - gx, 1.0f / wx);
        f32 fade  = expf(-z * 0.055f);

        v3 col = v3_scl(dm_rgb8(20, 8, 40), 0.10f * fade);
        col = v3_add(col, v3_scl(dm_pal_house(0.14f), lines * fade * (1.1f + 1.7f * beat)));
        return col;
    }

    /* Sky: a vertical ramp with the sun sitting on the horizon, sliced by the
     * horizontal bands that make it read as a sunset and not a lightbulb. */
    f32 up = dm_sat((uv.y - HORIZON) * 1.1f);
    v3 col = v3_lerp(v3_scl(dm_rgb8(90, 20, 80), 0.30f),
                     v3_scl(dm_rgb8(16, 10, 46), 0.30f), up);

    v2  sc = V2(uv.x, uv.y - HORIZON - 0.30f);
    f32 sr = v2_len(V2(sc.x, sc.y * 1.0f));
    f32 sun = dm_smoothstep(0.34f, 0.32f, sr);
    f32 band = dm_step(0.5f, dm_fract((uv.y - HORIZON) * 22.0f));
    band = dm_lerp(1.0f, band, dm_sat(1.0f - (uv.y - HORIZON) * 2.6f));

    col = v3_add(col, v3_scl(v3_lerp(dm_rgb8(255, 200, 80), dm_rgb8(255, 60, 140),
                                     dm_sat((uv.y - HORIZON) * 1.6f)),
                             sun * band * (2.2f + 1.4f * beat)));
    col = v3_add(col, v3_scl(dm_rgb8(255, 90, 160), expf(-sr * sr * 5.0f) * 0.35f));
    return col;
}

static v3 fracture_px(v2 uv, const yk_ctx *c, u32 seed)
{
    (void)seed;
    f32 t = (f32)c->t;
    f32 p = (f32)c->sec_phase;

    /* Everything below ramps on the same curve the drum roll does. */
    f32 chaos = dm_ease_in_quad(p);
    u32 tick  = (u32)(c->t / (SONG_STEP_SEC * 2.0));   /* changes every eighth */

    v2 g = uv;

    /* Block displacement. Blocks get smaller and jump further as the section
     * runs, so the picture goes from "wobbling" to "shredded". */
    f32 bs = dm_lerp(0.34f, 0.10f, chaos);
    v2  cell = V2(floorf(uv.x / bs), floorf(uv.y / bs));
    u32 h = dm_hash3i((i32)cell.x, (i32)cell.y, (i32)tick, 0x9e37u);
    if (dm_u32_to_f32(h) < chaos * 0.55f) {
        h = dm_hash_u32(h);
        f32 ox = (dm_u32_to_f32(h) * 2.0f - 1.0f) * 0.22f * chaos;
        h = dm_hash_u32(h);
        f32 oy = (dm_u32_to_f32(h) * 2.0f - 1.0f) * 0.07f * chaos;
        g = v2_add(g, V2(ox, oy));
    }

    /* Horizontal tearing: whole scanline bands slide sideways. */
    u32 sh = dm_hash3i((i32)(uv.y * 90.0f), (i32)tick, 0, 0x51a7u);
    if (dm_u32_to_f32(sh) < chaos * 0.30f)
        g.x += (dm_u32_to_f32(dm_hash_u32(sh)) * 2.0f - 1.0f) * 0.30f * chaos;

    /* Channel separation, sampled as three independent passes. */
    f32 split = 0.006f + 0.055f * chaos;
    f32 beat  = c->beat_hit;
    v3 r = grid_scene(V2(g.x + split, g.y), t, beat);
    v3 gg = grid_scene(g, t, beat);
    v3 b = grid_scene(V2(g.x - split, g.y), t, beat);
    v3 col = V3(r.x, gg.y, b.z);

    /* Strobe on the sixteenths, following the roll as it doubles up. */
    f32 st = dm_fract((f32)(c->t / SONG_STEP_SEC));
    col = v3_scl(col, 1.0f + chaos * 2.2f * powf(1.0f - st, 8.0f));

    /* Scanline dropouts. */
    u32 dh = dm_hash3i((i32)(uv.y * 160.0f), (i32)tick, 7, 0x2f1bu);
    if (dm_u32_to_f32(dh) < chaos * 0.10f) col = v3_scl(col, 0.15f);

    /* And the whole thing whites out into the drop. */
    col = v3_add(col, V3s(dm_smootherstep(0.90f, 1.0f, p) * 2.6f));

    return v3_scl(col, yk_vignette(uv, 0.30f));
}

void scene_fracture(dm_fb *fb, const yk_ctx *c)
{
    yk_shade(fb, c, fracture_px, yk_aa);
}
