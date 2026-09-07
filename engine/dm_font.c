#include "dm_font.h"

#include <string.h>

/* Glyphs live in a 6 wide by 10 tall grid: x 0..6, y 0..10 with the baseline
 * at y=0 and the cap height at y=10. Coordinates are stored as signed byte
 * pairs; two sentinels break the stream into strokes and end the glyph. */
#define P 100   /* pen up -- start a new stroke */
#define E 101   /* end of glyph */

#define GRID_W  6.0f
#define GRID_H 10.0f
#define ADVANCE 8.0f   /* 6 of glyph plus 2 of side bearing */

static const i8 G_SP[] = { E,0 };
static const i8 G_EXCL[] = { 3,10, 3,3, P,0, 3,0, 3,0, E,0 };
static const i8 G_DQUO[] = { 2,10, 2,7, P,0, 4,10, 4,7, E,0 };
static const i8 G_HASH[] = { 1,7, 5,7, P,0, 1,3, 5,3, P,0, 2,10, 1,0, P,0, 5,10, 4,0, E,0 };
static const i8 G_DOLR[] = { 3,10, 3,0, P,0, 6,9, 4,10, 2,10, 0,8, 0,7, 2,5, 4,5, 6,3, 6,2, 4,0, 2,0, 0,1, E,0 };
static const i8 G_PCT[]  = { 0,0, 6,10, P,0, 1,8, 1,8, P,0, 5,2, 5,2, E,0 };
static const i8 G_AMP[]  = { 6,0, 1,7, 1,9, 3,10, 5,8, 0,3, 0,1, 2,0, 4,1, 6,4, E,0 };
static const i8 G_QUOT[] = { 3,10, 3,7, E,0 };
static const i8 G_LPAR[] = { 4,10, 2,7, 2,3, 4,0, E,0 };
static const i8 G_RPAR[] = { 2,10, 4,7, 4,3, 2,0, E,0 };
static const i8 G_STAR[] = { 3,3, 3,7, P,0, 1,4, 5,6, P,0, 5,4, 1,6, E,0 };
static const i8 G_PLUS[] = { 1,5, 5,5, P,0, 3,3, 3,7, E,0 };
static const i8 G_COMM[] = { 3,1, 2,-1, E,0 };
static const i8 G_DASH[] = { 1,5, 5,5, E,0 };
static const i8 G_DOT[]  = { 3,0, 3,0, E,0 };
static const i8 G_SLSH[] = { 0,0, 6,10, E,0 };

static const i8 G_0[] = { 0,2, 2,0, 4,0, 6,2, 6,8, 4,10, 2,10, 0,8, 0,2, P,0, 1,3, 5,7, E,0 };
static const i8 G_1[] = { 1,8, 3,10, 3,0, P,0, 1,0, 5,0, E,0 };
static const i8 G_2[] = { 0,8, 2,10, 4,10, 6,8, 6,7, 0,1, 0,0, 6,0, E,0 };
static const i8 G_3[] = { 0,9, 2,10, 4,10, 6,8, 3,5, 6,3, 4,0, 2,0, 0,1, E,0 };
static const i8 G_4[] = { 4,0, 4,10, 0,3, 6,3, E,0 };
static const i8 G_5[] = { 6,10, 0,10, 0,6, 4,6, 6,4, 6,2, 4,0, 2,0, 0,1, E,0 };
static const i8 G_6[] = { 5,10, 2,10, 0,7, 0,2, 2,0, 4,0, 6,2, 6,4, 4,6, 2,6, 0,4, E,0 };
static const i8 G_7[] = { 0,10, 6,10, 2,0, E,0 };
static const i8 G_8[] = { 2,5, 0,7, 0,8, 2,10, 4,10, 6,8, 6,7, 4,5, 2,5, P,0,
                          4,5, 6,3, 6,2, 4,0, 2,0, 0,2, 0,3, 2,5, E,0 };
static const i8 G_9[] = { 1,0, 4,0, 6,3, 6,8, 4,10, 2,10, 0,8, 0,6, 2,4, 4,4, 6,6, E,0 };

static const i8 G_COLN[] = { 3,7, 3,7, P,0, 3,2, 3,2, E,0 };
static const i8 G_SEMI[] = { 3,7, 3,7, P,0, 3,2, 2,0, E,0 };
static const i8 G_LT[]   = { 5,9, 1,5, 5,1, E,0 };
static const i8 G_EQ[]   = { 1,6, 5,6, P,0, 1,4, 5,4, E,0 };
static const i8 G_GT[]   = { 1,9, 5,5, 1,1, E,0 };
static const i8 G_QUES[] = { 0,8, 2,10, 4,10, 6,8, 6,7, 3,5, 3,3, P,0, 3,0, 3,0, E,0 };
static const i8 G_AT[]   = { 4,4, 3,3, 2,4, 3,5, 4,4, 4,2, 6,3, 6,8, 4,10, 2,10, 0,8, 0,2, 2,0, 5,0, E,0 };

static const i8 G_A[] = { 0,0, 3,10, 6,0, P,0, 1,3, 5,3, E,0 };
static const i8 G_B[] = { 0,0, 0,10, 4,10, 6,8, 4,5, 0,5, P,0, 4,5, 6,3, 4,0, 0,0, E,0 };
static const i8 G_C[] = { 6,8, 4,10, 2,10, 0,8, 0,2, 2,0, 4,0, 6,2, E,0 };
static const i8 G_D[] = { 0,0, 0,10, 3,10, 6,7, 6,3, 3,0, 0,0, E,0 };
static const i8 G_E[] = { 6,10, 0,10, 0,0, 6,0, P,0, 0,5, 4,5, E,0 };
static const i8 G_F[] = { 6,10, 0,10, 0,0, P,0, 0,5, 4,5, E,0 };
static const i8 G_G[] = { 6,8, 4,10, 2,10, 0,8, 0,2, 2,0, 4,0, 6,2, 6,4, 3,4, E,0 };
static const i8 G_H[] = { 0,10, 0,0, P,0, 6,10, 6,0, P,0, 0,5, 6,5, E,0 };
static const i8 G_I[] = { 1,10, 5,10, P,0, 3,10, 3,0, P,0, 1,0, 5,0, E,0 };
static const i8 G_J[] = { 6,10, 6,2, 4,0, 2,0, 0,2, E,0 };
static const i8 G_K[] = { 0,10, 0,0, P,0, 6,10, 0,5, P,0, 2,7, 6,0, E,0 };
static const i8 G_L[] = { 0,10, 0,0, 6,0, E,0 };
static const i8 G_M[] = { 0,0, 0,10, 3,6, 6,10, 6,0, E,0 };
static const i8 G_N[] = { 0,0, 0,10, 6,0, 6,10, E,0 };
static const i8 G_O[] = { 0,2, 2,0, 4,0, 6,2, 6,8, 4,10, 2,10, 0,8, 0,2, E,0 };
static const i8 G_P[] = { 0,0, 0,10, 4,10, 6,8, 6,7, 4,5, 0,5, E,0 };
static const i8 G_Q[] = { 0,2, 2,0, 4,0, 6,2, 6,8, 4,10, 2,10, 0,8, 0,2, P,0, 3,3, 6,0, E,0 };
static const i8 G_R[] = { 0,0, 0,10, 4,10, 6,8, 6,7, 4,5, 0,5, P,0, 3,5, 6,0, E,0 };
static const i8 G_S[] = { 6,9, 4,10, 2,10, 0,8, 0,7, 2,5, 4,5, 6,3, 6,2, 4,0, 2,0, 0,1, E,0 };
static const i8 G_T[] = { 0,10, 6,10, P,0, 3,10, 3,0, E,0 };
static const i8 G_U[] = { 0,10, 0,2, 2,0, 4,0, 6,2, 6,10, E,0 };
static const i8 G_V[] = { 0,10, 3,0, 6,10, E,0 };
static const i8 G_W[] = { 0,10, 1,0, 3,6, 5,0, 6,10, E,0 };
static const i8 G_X[] = { 0,10, 6,0, P,0, 0,0, 6,10, E,0 };
static const i8 G_Y[] = { 0,10, 3,5, 6,10, P,0, 3,5, 3,0, E,0 };
static const i8 G_Z[] = { 0,10, 6,10, 0,0, 6,0, E,0 };

static const i8 G_LBRK[] = { 4,10, 2,10, 2,0, 4,0, E,0 };
static const i8 G_BSLH[] = { 0,10, 6,0, E,0 };
static const i8 G_RBRK[] = { 2,10, 4,10, 4,0, 2,0, E,0 };
static const i8 G_CARE[] = { 1,7, 3,10, 5,7, E,0 };
static const i8 G_USCR[] = { 0,0, 6,0, E,0 };

/* Indexed by (character - 32), covering ASCII 32..95. Lowercase folds up. */
static const i8 *const GLYPHS[64] = {
    G_SP,   G_EXCL, G_DQUO, G_HASH, G_DOLR, G_PCT,  G_AMP,  G_QUOT,
    G_LPAR, G_RPAR, G_STAR, G_PLUS, G_COMM, G_DASH, G_DOT,  G_SLSH,
    G_0,    G_1,    G_2,    G_3,    G_4,    G_5,    G_6,    G_7,
    G_8,    G_9,    G_COLN, G_SEMI, G_LT,   G_EQ,   G_GT,   G_QUES,
    G_AT,   G_A,    G_B,    G_C,    G_D,    G_E,    G_F,    G_G,
    G_H,    G_I,    G_J,    G_K,    G_L,    G_M,    G_N,    G_O,
    G_P,    G_Q,    G_R,    G_S,    G_T,    G_U,    G_V,    G_W,
    G_X,    G_Y,    G_Z,    G_LBRK, G_BSLH, G_RBRK, G_CARE, G_USCR
};

dm_text dm_text_default(void)
{
    dm_text t;
    t.size      = 64.0f;
    t.weight    = 3.0f;
    t.tracking  = 0.0f;
    t.color     = V3(1.0f, 1.0f, 1.0f);
    t.glow      = 0.0f;
    t.glow_gain = 0.0f;
    t.additive  = 1;
    return t;
}

f32 dm_text_advance(const dm_text *st)
{
    return st->size * (ADVANCE / GRID_H) + st->size * st->tracking;
}

f32 dm_text_width(const char *s, const dm_text *st)
{
    size_t n = strlen(s);
    return n ? (f32)n * dm_text_advance(st) - st->size * st->tracking : 0.0f;
}

/* Distance from p to the segment ab. Degenerate segments (a == b) fall out as
 * a distance to the point, which is how dots and periods are drawn. */
static f32 seg_dist(v2 p, v2 a, v2 b)
{
    v2  pa = v2_sub(p, a), ba = v2_sub(b, a);
    f32 bb = v2_dot(ba, ba);
    f32 h  = bb > 1e-12f ? dm_sat(v2_dot(pa, ba) / bb) : 0.0f;
    return v2_len(v2_sub(pa, v2_scl(ba, h)));
}

#define MAX_SEGS 32

void dm_char_draw(dm_fb *fb, int c, v2 pen, f32 angle, const dm_text *st)
{
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c < 32 || c > 95) return;

    const i8 *g = GLYPHS[c - 32];
    f32 scale = st->size / GRID_H;
    f32 ca = cosf(angle), sa = sinf(angle);

    /* Transform the glyph into pixel space once, then rasterise from that. */
    v2  A[MAX_SEGS], B[MAX_SEGS];
    int nseg = 0;
    int have_prev = 0;
    v2  prev = V2(0, 0);

    for (int i = 0; g[i] != E && nseg < MAX_SEGS; i += 2) {
        if (g[i] == P) { have_prev = 0; continue; }

        /* Glyph y is up, screen y is down. */
        f32 gx = (f32)g[i]     * scale;
        f32 gy = -(f32)g[i + 1] * scale;
        v2  q  = V2(pen.x + gx * ca - gy * sa, pen.y + gx * sa + gy * ca);

        if (have_prev) { A[nseg] = prev; B[nseg] = q; nseg++; }
        prev = q;
        have_prev = 1;
    }
    if (nseg == 0) return;

    /* The halo is exponential, so it never truly reaches zero. Clipping it at
     * the bounding box would leave a visible rectangular seam around every
     * word, so pick a generous cutoff and subtract the value there, which
     * lands the halo exactly on zero at the boundary. */
    f32 glow_reach = st->glow > 0.0f ? st->glow * 7.0f : 0.0f;
    f32 reach      = st->weight + 1.5f + glow_reach;
    f32 pedestal   = glow_reach > 0.0f ? expf(-glow_reach / st->glow) : 0.0f;
    f32 halo_norm  = 1.0f / DM_MAX(1.0f - pedestal, 1e-6f);

    /* Only touch the pixels this glyph can reach. */
    f32 minx = A[0].x, maxx = A[0].x, miny = A[0].y, maxy = A[0].y;
    for (int i = 0; i < nseg; i++) {
        minx = DM_MIN(minx, DM_MIN(A[i].x, B[i].x));
        maxx = DM_MAX(maxx, DM_MAX(A[i].x, B[i].x));
        miny = DM_MIN(miny, DM_MIN(A[i].y, B[i].y));
        maxy = DM_MAX(maxy, DM_MAX(A[i].y, B[i].y));
    }
    int x0 = (int)floorf(minx - reach), x1 = (int)ceilf(maxx + reach);
    int y0 = (int)floorf(miny - reach), y1 = (int)ceilf(maxy + reach);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > fb->w - 1) x1 = fb->w - 1;
    if (y1 > fb->h - 1) y1 = fb->h - 1;

    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            v2  p = V2((f32)x + 0.5f, (f32)y + 0.5f);
            f32 d = 1e30f;
            for (int i = 0; i < nseg; i++) {
                f32 s = seg_dist(p, A[i], B[i]);
                if (s < d) d = s;
            }

            /* One pixel of transition either side of the stroke edge is the
             * width a box filter would see, so the edge reads as clean. */
            f32 cov = dm_smoothstep(st->weight + 0.7f, st->weight - 0.7f, d);
            f32 halo = 0.0f;
            if (st->glow > 0.0f) {
                f32 e = DM_MAX(d - st->weight, 0.0f);
                halo = (expf(-e / st->glow) - pedestal) * halo_norm * st->glow_gain;
                if (halo < 0.0f) halo = 0.0f;
            }

            if (cov <= 0.0f && halo <= 0.0f) continue;

            if (st->additive) {
                dm_fb_add(fb, x, y, v3_scl(st->color, cov + halo));
            } else {
                v3 dst = dm_fb_get(fb, x, y);
                v3 lit = v3_add(dst, v3_scl(st->color, halo));
                dm_fb_set(fb, x, y, v3_lerp(lit, st->color, cov));
            }
        }
    }
}

void dm_text_draw(dm_fb *fb, v2 pen, const char *s, const dm_text *st)
{
    f32 adv = dm_text_advance(st);
    for (const char *p = s; *p; p++, pen.x += adv)
        dm_char_draw(fb, (unsigned char)*p, pen, 0.0f, st);
}

void dm_text_draw_centered(dm_fb *fb, v2 center, const char *s, const dm_text *st)
{
    v2 pen = V2(center.x - dm_text_width(s, st) * 0.5f, center.y + st->size * 0.5f);
    dm_text_draw(fb, pen, s, st);
}
