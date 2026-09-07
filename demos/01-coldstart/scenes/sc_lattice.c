/* sc_lattice.c -- LATTICE (bar 48).
 *
 * The climax, and the most openly nostalgic shot in the film.
 *
 * Behind: an infinite crystal lattice. Space is folded modulo a fixed period
 * before the distance function is evaluated, so one cell's worth of arithmetic
 * describes a structure that goes on forever in all six directions. The
 * camera can fly through it indefinitely and never leave and never repeat a
 * lighting condition.
 *
 * In front: copper bars. On an Amiga these were made by rewriting the palette
 * register between scanlines -- a whole effect built out of the fact that the
 * screen is drawn one line at a time. There is no palette register here, so
 * they are computed the honest way, but they are placed and coloured exactly
 * as they would have been in 1990.
 */
#include "../demo.h"

#define LAT_PERIOD 3.60f

static f32 lat_map(v3 p, f32 t)
{
    v3  q  = v3_sub(v3_mod(p, LAT_PERIOD), V3s(LAT_PERIOD * 0.5f));
    f32 rr = 0.105f + 0.030f * sinf(t * 1.7f);

    /* Three orthogonal rods through the cell, then a node where they cross. */
    f32 dx = v2_len(V2(q.y, q.z)) - rr;
    f32 dy = v2_len(V2(q.x, q.z)) - rr;
    f32 dz = v2_len(V2(q.x, q.y)) - rr;
    f32 d  = DM_MIN(dx, DM_MIN(dy, dz));
    return dm_smin(d, v3_len(q) - 0.27f, 0.20f);
}

static v3 lat_normal(v3 p, f32 t)
{
    const f32 e = 0.0035f;
    return v3_norm(V3(
        lat_map(V3(p.x + e, p.y, p.z), t) - lat_map(V3(p.x - e, p.y, p.z), t),
        lat_map(V3(p.x, p.y + e, p.z), t) - lat_map(V3(p.x, p.y - e, p.z), t),
        lat_map(V3(p.x, p.y, p.z + e), t) - lat_map(V3(p.x, p.y, p.z - e), t)));
}

static v3 lattice_px(v2 uv, const yk_ctx *c, u32 seed)
{
    (void)seed;
    f32 t = (f32)c->t;

    v3 ro = V3(sinf(t * 0.21f) * 1.30f,
               cosf(t * 0.17f) * 1.05f,
               t * 4.6f);
    m3 cam = m3_euler(sinf(t * 0.13f) * 0.28f,
                      t * 0.09f,
                      sinf(t * 0.11f) * 0.45f);
    v3 rd = m3_mul_v3(cam, v3_norm(V3(uv.x, uv.y, 1.60f)));

    f32 d = 0.02f;
    int hit = 0;
    v3  p = ro;
    for (int i = 0; i < 84; i++) {
        p = v3_add(ro, v3_scl(rd, d));
        f32 h = lat_map(p, t);
        if (h < 0.0016f) { hit = 1; break; }
        d += h * 0.90f;
        if (d > 30.0f) break;
    }

    v3 col = V3(0.0f, 0.0f, 0.0f);

    if (hit) {
        v3 n = lat_normal(p, t);

        /* Colour keyed to which cell of the lattice was hit, so the structure
         * reads as built out of discrete blocks rather than extruded. */
        v3  cell = v3_floor(v3_scl(p, 1.0f / LAT_PERIOD));
        u32 ch   = dm_hash3i((i32)cell.x, (i32)cell.y, (i32)cell.z, 0x77u);
        /* Two-tone rather than a continuous sweep. The house palette passes
         * through olive between 0.62 and 0.80, and with this many rods on
         * screen at once even a few cells landing there average the whole
         * frame into a pale mint wash. So: a cold band and a hot band, with
         * the dead zone between them skipped entirely. */
        f32 hv = dm_u32_to_f32(ch);
        f32 pt = hv < 0.32f ? (0.02f + hv * 0.28f)
                            : (0.40f + (hv - 0.32f) * 0.26f);
        v3  base = dm_pal_house(pt);

        /* Headlight, so the near cells are bright and the far ones fall away.
         * With a periodic structure this is the only depth cue there is. */
        f32 diff = dm_sat(v3_dot(n, v3_neg(rd))) * 0.85f + 0.15f;
        f32 spec = powf(dm_sat(v3_dot(v3_reflect(rd, n), v3_neg(rd))), 26.0f);

        col = v3_scl(base, diff * 0.58f);
        col = v3_add(col, v3_scl(V3(1.0f, 0.96f, 0.92f), spec * 0.55f));

        /* Nodes glow on the beat. */
        v3  q   = v3_sub(v3_mod(p, LAT_PERIOD), V3s(LAT_PERIOD * 0.5f));
        f32 nod = dm_smoothstep(0.42f, 0.24f, v3_len(q));
        col = v3_add(col, v3_scl(dm_pal_house(0.15f), nod * (0.5f + 2.4f * c->beat_hit)));

        col = yk_fog(col, v3_scl(dm_rgb8(16, 20, 52), 0.10f), d, 0.085f);
    } else {
        f32 r2 = v2_dot(uv, uv);
        col = v3_scl(dm_pal_house(0.62f), 0.035f + 0.06f * expf(-r2 * 1.4f));
    }

    /* --- copper bars ---------------------------------------------------- */
    /* Five bars sweeping on incommensurate sines, additive, with the soft
     * shoulder the original hardware got from its colour ramps. */
    for (int i = 0; i < 5; i++) {
        f32 fi = (f32)i;
        f32 yc = sinf(t * (0.55f + fi * 0.11f) + fi * 1.27f) * 0.72f;
        f32 dy = fabsf(uv.y - yc);
        f32 w  = 0.052f + 0.012f * sinf(t * 0.9f + fi);
        f32 v  = dm_sat(1.0f - dy / w);
        v = v * v * (3.0f - 2.0f * v);          /* smooth shoulder */
        col = v3_add(col, v3_scl(dm_pal_house(0.04f + fi * 0.085f + t * 0.02f),
                                 v * 0.38f * (0.65f + 0.7f * c->bar_hit)));
    }

    return v3_scl(col, yk_vignette(uv, 0.38f));
}

/* Sparks thrown off the beat, drifting outward from the centre of the frame. */
static void lattice_sparks(dm_fb *fb, const yk_ctx *c)
{
    const int N = 420;
    f32 t = (f32)c->t;

    for (int i = 0; i < N; i++) {
        u32 h  = dm_hash_u32((u32)i * 2246822519u + 3u);
        f32 a  = dm_u32_to_f32(h) * DM_TAU;   h = dm_hash_u32(h);
        f32 sp = 0.35f + dm_u32_to_f32(h) * 1.5f; h = dm_hash_u32(h);
        f32 ph = dm_u32_to_f32(h);            h = dm_hash_u32(h);
        f32 hue = dm_u32_to_f32(h);

        /* Each spark relives its life once per bar, staggered by phase. */
        f32 life = dm_fract((f32)(c->bar) + ph);
        f32 r    = life * sp * 1.9f;
        f32 fade = (1.0f - life) * (1.0f - life);

        v2 pos = V2(cosf(a + t * 0.2f) * r, sinf(a + t * 0.2f) * r);
        if (fabsf(pos.x) > c->aspect * 1.1f || fabsf(pos.y) > 1.1f) continue;

        v3 col = v3_lerp(dm_rgb8(255, 210, 160), dm_rgb8(160, 200, 255), hue);
        yk_blob(fb, yk_to_screen(c, pos), 1.1f + 2.2f * fade,
                v3_scl(col, fade * 0.5f));
    }
}

void scene_lattice(dm_fb *fb, const yk_ctx *c)
{
    yk_shade(fb, c, lattice_px, yk_aa);
    lattice_sparks(fb, c);
}
