#include "dm_post.h"
#include "dm_job.h"
#include "dm_rand.h"

#include <stdlib.h>
#include <string.h>

#define DM_POST_MAX_LEVELS 8
#define DM_BLUR_MAX_TAPS   33

struct dm_post {
    int    w, h, levels;
    dm_fb *mip[DM_POST_MAX_LEVELS];  /* bloom chain, mip[0] is half resolution */
    dm_fb *tmp[DM_POST_MAX_LEVELS];  /* scratch for the separable blur */
    dm_fb *full;                     /* full-res scratch for the lens pass */
};

dm_post_params dm_post_defaults(void)
{
    dm_post_params p;
    p.bloom_threshold = 1.0f;
    p.bloom_knee      = 0.6f;
    p.bloom_intensity = 0.09f;
    p.bloom_radius    = 0.75f;
    p.barrel          = 0.020f;
    p.chroma          = 0.0022f;
    p.vignette        = 0.42f;
    p.grain           = 0.012f;
    p.frame           = 0;
    return p;
}

dm_post *dm_post_new(int w, int h, int levels)
{
    if (levels < 1) levels = 1;
    if (levels > DM_POST_MAX_LEVELS) levels = DM_POST_MAX_LEVELS;

    dm_post *p = (dm_post *)calloc(1, sizeof(dm_post));
    if (!p) return NULL;
    p->w = w; p->h = h; p->levels = levels;

    p->full = dm_fb_new(w, h);
    if (!p->full) { free(p); return NULL; }

    int mw = w, mh = h;
    for (int i = 0; i < levels; i++) {
        mw = mw > 1 ? mw / 2 : 1;
        mh = mh > 1 ? mh / 2 : 1;
        p->mip[i] = dm_fb_new(mw, mh);
        p->tmp[i] = dm_fb_new(mw, mh);
        if (!p->mip[i] || !p->tmp[i]) { dm_post_free(p); return NULL; }
    }
    return p;
}

void dm_post_free(dm_post *p)
{
    if (!p) return;
    for (int i = 0; i < DM_POST_MAX_LEVELS; i++) {
        dm_fb_free(p->mip[i]);
        dm_fb_free(p->tmp[i]);
    }
    dm_fb_free(p->full);
    free(p);
}

/* ---- bright pass ------------------------------------------------------- */

typedef struct {
    const dm_fb *src;
    dm_fb       *dst;
    f32          threshold, knee;
} bright_ctx;

/* Quadratic soft knee: below (threshold-knee) nothing passes, above
 * (threshold+knee) everything does, and the transition between them is smooth
 * so a slowly brightening pixel eases into the bloom instead of snapping in. */
static f32 knee_weight(f32 x, f32 threshold, f32 knee)
{
    f32 soft = x - threshold + knee;
    soft = dm_clamp(soft, 0.0f, 2.0f * knee);
    soft = soft * soft / (4.0f * knee + 1e-6f);
    f32 contribution = DM_MAX(soft, x - threshold);
    return contribution / DM_MAX(x, 1e-6f);
}

static void bright_row(int y, int thread, void *ud)
{
    (void)thread;
    bright_ctx *c = (bright_ctx *)ud;

    for (int x = 0; x < c->dst->w; x++) {
        /* Box-filter 2x2 of the source while extracting: halves the resolution
         * and antialiases the bright pass in one step. */
        v3 sum = V3(0, 0, 0);
        for (int j = 0; j < 2; j++)
            for (int i = 0; i < 2; i++)
                sum = v3_add(sum, dm_fb_getc(c->src, x * 2 + i, y * 2 + j));
        v3 col = v3_scl(sum, 0.25f);

        f32 w = knee_weight(v3_maxc(col), c->threshold, c->knee);
        dm_fb_set(c->dst, x, y, v3_scl(col, w));
    }
}

/* ---- separable gaussian ------------------------------------------------ */

typedef struct {
    const dm_fb *src;
    dm_fb       *dst;
    const f32   *kernel;
    int          radius;
    int          horizontal;
} blur_ctx;

static void blur_row(int y, int thread, void *ud)
{
    (void)thread;
    blur_ctx *c = (blur_ctx *)ud;

    for (int x = 0; x < c->dst->w; x++) {
        v3 acc = V3(0, 0, 0);
        for (int k = -c->radius; k <= c->radius; k++) {
            v3 s = c->horizontal ? dm_fb_getc(c->src, x + k, y)
                                 : dm_fb_getc(c->src, x, y + k);
            acc = v3_add(acc, v3_scl(s, c->kernel[k + c->radius]));
        }
        dm_fb_set(c->dst, x, y, acc);
    }
}

static int build_gaussian(f32 *kernel, f32 sigma)
{
    int radius = (int)(sigma * 3.0f + 0.5f);
    if (radius < 1) radius = 1;
    if (radius > DM_BLUR_MAX_TAPS / 2) radius = DM_BLUR_MAX_TAPS / 2;

    f32 sum = 0.0f;
    for (int i = -radius; i <= radius; i++) {
        f32 v = expf(-(f32)(i * i) / (2.0f * sigma * sigma));
        kernel[i + radius] = v;
        sum += v;
    }
    for (int i = 0; i < 2 * radius + 1; i++) kernel[i] /= sum;
    return radius;
}

static void blur_level(dm_post *p, int level, f32 sigma)
{
    f32 kernel[DM_BLUR_MAX_TAPS];
    int radius = build_gaussian(kernel, sigma);

    blur_ctx c;
    c.kernel = kernel;
    c.radius = radius;

    c.src = p->mip[level]; c.dst = p->tmp[level]; c.horizontal = 1;
    dm_job_for(c.dst->h, blur_row, &c);

    c.src = p->tmp[level]; c.dst = p->mip[level]; c.horizontal = 0;
    dm_job_for(c.dst->h, blur_row, &c);
}

/* ---- upsample and accumulate ------------------------------------------- */

typedef struct {
    const dm_fb *src;
    dm_fb       *dst;
    f32          weight;
} up_ctx;

static void up_row(int y, int thread, void *ud)
{
    (void)thread;
    up_ctx *c = (up_ctx *)ud;
    f32 sx = (f32)c->src->w / (f32)c->dst->w;
    f32 sy = (f32)c->src->h / (f32)c->dst->h;

    for (int x = 0; x < c->dst->w; x++) {
        v3 s = dm_fb_sample(c->src, ((f32)x + 0.5f) * sx, ((f32)y + 0.5f) * sy);
        dm_fb_add(c->dst, x, y, v3_scl(s, c->weight));
    }
}

/* ---- final lens pass --------------------------------------------------- */

typedef struct {
    const dm_fb *src;
    dm_fb       *dst;
    const dm_fb *bloom;
    dm_post_params prm;
    f32          zoom;   /* pre-scale that keeps every sample inside the frame */
} lens_ctx;

/* Barrel distortion and chromatic aberration both push sample coordinates
 * outward, so at the border they read past the edge of the source and the
 * clamped texels smear into visible bands. Solving for the pre-scale that maps
 * the frame corner exactly onto itself removes the reads instead of hiding
 * them. The equation k * (1 + b*r2) * (1 + c*r2) = 1 with r2 = (k*R)^2 has no
 * pleasant closed form, so bisect it -- once per frame, not per pixel. */
static f32 fit_zoom(f32 aspect, f32 barrel, f32 chroma)
{
    f32 R2 = aspect * aspect + 1.0f;
    f32 lo = 0.1f, hi = 1.0f;
    for (int i = 0; i < 28; i++) {
        f32 k  = 0.5f * (lo + hi);
        f32 r2 = k * k * R2;
        f32 s  = k * (1.0f + barrel * r2) * (1.0f + fabsf(chroma) * r2);
        if (s > 1.0f) hi = k; else lo = k;
    }
    return 0.5f * (lo + hi);
}

static void lens_row(int y, int thread, void *ud)
{
    (void)thread;
    lens_ctx *c = (lens_ctx *)ud;
    const dm_post_params *P = &c->prm;

    int   W = c->dst->w, H = c->dst->h;
    f32   aspect = (f32)W / (f32)H;
    f32   bsx = (f32)c->bloom->w / (f32)W;
    f32   bsy = (f32)c->bloom->h / (f32)H;

    for (int x = 0; x < W; x++) {
        /* Centred coordinates, -1..1 on the short axis, pre-scaled to fit. */
        f32 cx = (((f32)x + 0.5f) / (f32)W * 2.0f - 1.0f) * aspect * c->zoom;
        f32 cy = (((f32)y + 0.5f) / (f32)H * 2.0f - 1.0f) * c->zoom;
        f32 r2 = cx * cx + cy * cy;

        /* Barrel distortion pushes the corners out; the same r^2 term drives
         * the chromatic split, which is why real lenses show both together. */
        f32 d  = 1.0f + P->barrel * r2;
        f32 ux = cx * d, uy = cy * d;

        v3 col;
        if (P->chroma > 0.0f) {
            f32 ca = P->chroma * r2;
            f32 sc[3] = { 1.0f + ca, 1.0f, 1.0f - ca };
            f32 ch[3];
            for (int k = 0; k < 3; k++) {
                f32 px = (ux * sc[k] / aspect * 0.5f + 0.5f) * (f32)W;
                f32 py = (uy * sc[k]          * 0.5f + 0.5f) * (f32)H;
                v3 s = dm_fb_sample(c->src, px, py);
                ch[k] = k == 0 ? s.x : (k == 1 ? s.y : s.z);
            }
            col = V3(ch[0], ch[1], ch[2]);
        } else {
            col = dm_fb_sample(c->src,
                               (ux / aspect * 0.5f + 0.5f) * (f32)W,
                               (uy * 0.5f + 0.5f) * (f32)H);
        }

        /* Bloom is composited after distortion so it does not smear with it. */
        v3 bl = dm_fb_sample(c->bloom, ((f32)x + 0.5f) * bsx, ((f32)y + 0.5f) * bsy);
        col = v3_add(col, v3_scl(bl, P->bloom_intensity));

        if (P->vignette > 0.0f) {
            f32 v = 1.0f - P->vignette * dm_smoothstep(0.25f, 2.1f, r2);
            col = v3_scl(col, DM_MAX(v, 0.0f));
        }

        if (P->grain > 0.0f) {
            /* Scaled by sqrt(luma): shot noise is strongest in the mid tones,
             * and leaving blacks clean keeps the frame from looking dirty. */
            f32 n = dm_u32_to_f32(dm_hash3i(x, y, (i32)P->frame, 0x51ed270bu)) - 0.5f;
            col = v3_adds(col, n * P->grain * sqrtf(dm_sat(dm_luma(col)) + 0.02f));
        }

        dm_fb_set(c->dst, x, y, col);
    }
}

/* ---- driver ------------------------------------------------------------ */

void dm_post_apply(dm_post *p, dm_fb *fb, const dm_post_params *prm)
{
    /* 1. bright pass into the half-res head of the chain */
    bright_ctx bc;
    bc.src = fb;
    bc.dst = p->mip[0];
    bc.threshold = prm->bloom_threshold;
    bc.knee      = DM_MAX(prm->bloom_knee, 1e-4f);
    dm_job_for(p->mip[0]->h, bright_row, &bc);
    blur_level(p, 0, 1.6f);

    /* 2. downsample and blur each successive level. Halving the resolution
     *    each time means a fixed blur radius covers twice the screen area, so
     *    six levels give a bloom that reaches across the whole frame. */
    for (int i = 1; i < p->levels; i++) {
        dm_fb_downsample(p->mip[i], p->mip[i - 1]);
        blur_level(p, i, 1.6f);
    }

    /* 3. walk back up, adding each wide level into the next tighter one */
    for (int i = p->levels - 1; i >= 1; i--) {
        up_ctx uc;
        uc.src    = p->mip[i];
        uc.dst    = p->mip[i - 1];
        uc.weight = prm->bloom_radius;
        dm_job_for(uc.dst->h, up_row, &uc);
    }

    /* 4. distortion, chroma, bloom composite, vignette and grain in one pass */
    lens_ctx lc;
    lc.src   = fb;
    lc.dst   = p->full;
    lc.bloom = p->mip[0];
    lc.prm   = *prm;
    lc.zoom  = fit_zoom((f32)fb->w / (f32)fb->h, prm->barrel, prm->chroma);
    dm_job_for(p->full->h, lens_row, &lc);

    dm_fb_copy(fb, p->full);
}
