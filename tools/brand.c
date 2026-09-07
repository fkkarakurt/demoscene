/* brand.c -- the channel identity, rendered by the demo engine.
 *
 *   brand avatar                              800x800    profile picture
 *   brand banner                              2560x1440  channel art
 *   brand watermark                           300x300    video watermark, + alpha
 *   brand thumb <plate.ppm> <tag> <l1> <l2>   1280x720   video thumbnail
 *
 * A channel whose whole claim is that nothing was downloaded cannot go and
 * download its own logo. Everything below is built from the same noise, the
 * same palette and the same stroke font as the demo -- which is also the
 * cheapest way to make the avatar, the banner and the video read as one thing
 * instead of three.
 *
 * Output is PPM (plus a PGM alpha for the watermark). brand.ps1 hands those to
 * ffmpeg for the PNGs an upload form will accept.
 */
#include "../engine/dm_fb.h"
#include "../engine/dm_font.h"
#include "../engine/dm_job.h"
#include "../engine/dm_post.h"
#include "../engine/dm_rand.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OUT_DIR "brand/work"

/* ---- the words -----------------------------------------------------------
 *
 * Every string the identity puts on screen lives here, so renaming the channel
 * is one edit instead of four files and a guess at which one was missed. The
 * stroke font covers ASCII 32..95: uppercase, digits and punctuation only, no
 * lowercase and no diacritics. */

#define WORDMARK  "KORMOS"
#define MONOGRAM  "FK"
#define STRAPLINE "EVERY PIXEL COMPUTED AT RUNTIME. NO ASSETS. JUST C."
#define TAGLINE   "DEMOSCENE / SOFTWARE RENDERED / WRITTEN IN C"

/* ---- io ------------------------------------------------------------------ */

static void write_ppm(const char *path, const u8 *rgb, int w, int h)
{
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", path); return; }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    fwrite(rgb, 1, (size_t)w * h * 3, f);
    fclose(f);
    printf("[out] %s\n", path);
}

static void write_pgm(const char *path, const u8 *g, int w, int h)
{
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", path); return; }
    fprintf(f, "P5\n%d %d\n255\n", w, h);
    fwrite(g, 1, (size_t)w * h, f);
    fclose(f);
    printf("[out] %s\n", path);
}

/* Reads the next decimal in a PPM header, skipping whitespace and comments.
 * It stops on the delimiter after the digits, which is exactly the single
 * whitespace byte the format puts between the maxval and the pixel data. */
static int ppm_int(FILE *f)
{
    int c, v = 0, got = 0;
    for (;;) {
        c = fgetc(f);
        if (c == EOF) return got ? v : -1;
        if (c == '#') { while (c != '\n' && c != EOF) c = fgetc(f); continue; }
        if (c >= '0' && c <= '9') { v = v * 10 + (c - '0'); got = 1; continue; }
        if (got) return v;
    }
}

/* A still off the demo renderer, back into linear light. The file is sRGB
 * encoded 8-bit, so decoding is not optional: downsampling gamma-encoded
 * pixels darkens every edge in the image. */
static dm_fb *read_ppm(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); return NULL; }
    if (fgetc(f) != 'P' || fgetc(f) != '6') {
        fprintf(stderr, "%s: not a binary PPM\n", path); fclose(f); return NULL;
    }
    int w = ppm_int(f), h = ppm_int(f), maxv = ppm_int(f);
    if (w <= 0 || h <= 0 || maxv != 255) {
        fprintf(stderr, "%s: unsupported header (%d x %d, max %d)\n", path, w, h, maxv);
        fclose(f); return NULL;
    }

    u8    *row = (u8 *)malloc((size_t)w * 3);
    dm_fb *fb  = dm_fb_new(w, h);
    if (!row || !fb) { fclose(f); free(row); return NULL; }

    for (int y = 0; y < h; y++) {
        if (fread(row, 1, (size_t)w * 3, f) != (size_t)w * 3) {
            fprintf(stderr, "%s: truncated at row %d\n", path, y);
            break;
        }
        for (int x = 0; x < w; x++)
            dm_fb_set(fb, x, y, V3(dm_srgb_decode((f32)row[x * 3 + 0] / 255.0f),
                                   dm_srgb_decode((f32)row[x * 3 + 1] / 255.0f),
                                   dm_srgb_decode((f32)row[x * 3 + 2] / 255.0f)));
    }
    free(row);
    fclose(f);
    printf("[plate] %s  %dx%d\n", path, w, h);
    return fb;
}

static void save(dm_fb *fb, dm_post *post, const dm_post_params *p, const char *path)
{
    if (post) dm_post_apply(post, fb, p);
    u8 *rgb = (u8 *)malloc((size_t)fb->w * fb->h * 3);
    if (!rgb) return;
    dm_fb_resolve(fb, rgb, 1.0f, 0u);
    write_ppm(path, rgb, fb->w, fb->h);
    free(rgb);
}

/* ---- the shared look ----------------------------------------------------- */

/* One dark warped nebula with a warm core. Every asset here goes through this
 * function, which is what stops the avatar and the banner drifting apart the
 * moment either one is touched again. */
typedef struct {
    dm_fb *fb;
    f32    scale;      /* noise frequency */
    f32    core;       /* warm central glow */
    f32    band;       /* wide horizontal glow, for the banner's text strip */
    f32    ring;       /* rim circle radius in uv units, 0 = none */
    f32    hollow;     /* darkens the middle so type has ground to stand on */
    f32    vign;
    u32    seed;
} field;

static void field_row(int y, int thread, void *ud)
{
    (void)thread;
    field *j   = (field *)ud;
    dm_fb *fb  = j->fb;
    f32 aspect = (f32)fb->w / (f32)fb->h;

    for (int x = 0; x < fb->w; x++) {
        v2 uv = V2((((f32)x + 0.5f) / (f32)fb->w * 2.0f - 1.0f) * aspect,
                   1.0f - ((f32)y + 0.5f) / (f32)fb->h * 2.0f);
        f32 r2 = v2_dot(uv, uv);

        /* Darkness is the default; everything below has to earn its light. */
        v3 col = v3_scl(dm_rgb8(13, 20, 44), 0.032f);

        v3 p = V3(uv.x * j->scale, uv.y * j->scale, 0.61f);

        f32 n = dm_warp3(p, 1.05f, 5, j->seed);
        f32 m = dm_sat(n * 0.5f + 0.5f);
        col = v3_add(col, v3_scl(dm_pal_house(0.56f + n * 0.10f), powf(m, 3.6f) * 0.15f));

        /* The ridged layer is the only thing allowed above 1.0: thin filaments
         * for the tone mapper and the bloom to chew on. Taken up the palette
         * to the cold end -- the magenta mids look like a plasma ball once
         * they cover a whole frame, and this is a background, not the show. */
        f32 r = dm_ridge3(v3_scl(p, 2.15f), 5, 2.0f, 0.5f, j->seed + 17u);
        col = v3_add(col, v3_scl(dm_pal_house(0.56f), powf(r, 11.0f) * 2.6f));

        /* Sink the middle before anything is drawn on it. Legibility at 24 px
         * comes from this, not from making the letters brighter. */
        if (j->hollow > 0.0f)
            col = v3_scl(col, 1.0f - j->hollow * expf(-r2 * 2.2f));

        /* The kernel: the warm core the whole layout hangs off. */
        if (j->core > 0.0f)
            col = v3_add(col, v3_scl(dm_rgb8(255, 172, 96), j->core * expf(-r2 * 5.2f)));

        if (j->band > 0.0f)
            col = v3_add(col, v3_scl(dm_rgb8(120, 165, 255),
                                     j->band * expf(-dm_sq(uv.y * 2.4f))
                                             * expf(-dm_sq(uv.x * 0.42f))));

        /* A rim that survives YouTube's circular crop: it sits well inside the
         * inscribed circle, so none of it is cut away. */
        if (j->ring > 0.0f) {
            f32 d = fabsf(sqrtf(r2) - j->ring);
            col = v3_add(col, v3_scl(dm_rgb8(120, 200, 255),
                                     0.55f * expf(-dm_sq(d / 0.011f))));
        }

        col = v3_scl(col, 1.0f - j->vign * dm_smoothstep(0.25f, 2.0f, r2));
        dm_fb_set(fb, x, y, col);
    }
}

/* Faint stars, thinned out wherever text is going to land. */
static void stars(dm_fb *fb, int n, f32 gain, f32 clear_y, u32 seed)
{
    dm_rng rg;
    dm_rng_seed(&rg, seed);
    for (int i = 0; i < n; i++) {
        f32 x = dm_rng_f(&rg) * (f32)fb->w;
        f32 y = dm_rng_f(&rg) * (f32)fb->h;
        f32 m = powf(dm_rng_f(&rg), 3.4f);          /* mostly faint, rarely bright */

        if (clear_y > 0.0f) {
            f32 d = fabsf(y / (f32)fb->h - 0.5f) / clear_y;
            m *= dm_smoothstep(0.35f, 1.0f, d);
        }
        v3 c = v3_lerp(dm_rgb8(150, 190, 255), dm_rgb8(255, 218, 176), dm_rng_f(&rg));
        dm_fb_splat(fb, x, y, v3_scl(c, m * gain));
    }
}

/* The monogram, sized off the frame so the avatar and the watermark are the
 * same drawing at two resolutions. */
static void monogram(dm_fb *fb, f32 cy, f32 size, f32 gain)
{
    dm_text st = dm_text_default();
    st.size      = size;
    st.weight    = size * 0.105f;
    st.tracking  = -0.055f;                          /* the letters lock up */
    st.color     = v3_scl(dm_rgb8(255, 242, 224), 1.30f * gain);
    st.glow      = size * 0.055f;
    st.glow_gain = 0.20f;
    dm_text_draw_centered(fb, V2((f32)fb->w * 0.5f, cy), MONOGRAM, &st);
}

/* The point size that makes `s` exactly `target` pixels wide.
 *
 * dm_text_width is size * (n * (ADVANCE/GRID + tracking) - tracking), so this
 * is that solved for size. Worth the three lines: a six-letter wordmark and an
 * eleven-letter one need very different point sizes to read as the same design,
 * and hard-coding one means the other looks like a mistake. */
static f32 fit_size(const char *s, f32 tracking, f32 target)
{
    f32 n = (f32)strlen(s);
    f32 k = n * (0.8f + tracking) - tracking;
    return k > 0.0f ? target / k : 0.0f;
}

/* ---- targets ------------------------------------------------------------- */

static int mode_avatar(void)
{
    const int S = 800;
    dm_fb *fb = dm_fb_new(S, S);
    if (!fb) return 1;

    field j = { .fb = fb, .scale = 1.30f, .core = 0.16f, .band = 0.0f,
                .ring = 0.855f, .hollow = 0.74f, .vign = 0.30f, .seed = 3u };
    dm_job_for(S, field_row, &j);
    stars(fb, 260, 0.70f, 0.0f, 91u);
    monogram(fb, (f32)S * 0.50f, (f32)S * 0.330f, 1.0f);

    dm_post_params p = dm_post_defaults();
    p.bloom_threshold = 1.10f;
    p.bloom_intensity = 0.30f;
    p.bloom_radius    = 0.70f;
    p.barrel          = 0.0f;
    p.chroma          = 0.0012f;
    p.vignette        = 0.26f;
    p.grain           = 0.004f;

    dm_post *post = dm_post_new(S, S, 6);
    save(fb, post, &p, OUT_DIR "/avatar.ppm");
    dm_post_free(post);
    dm_fb_free(fb);
    return 0;
}

static int mode_banner(void)
{
    const int W = 2560, H = 1440;
    dm_fb *fb = dm_fb_new(W, H);
    if (!fb) return 1;

    /* YouTube shows all 2560x1440 on a TV, a 2560x423 strip on desktop and
     * 1546x423 on a phone. Only the innermost box is guaranteed, so every word
     * lives inside it and the rest of the frame is field. */
    const f32 SAFE_W = 1546.0f, SAFE_H = 423.0f;
    const f32 cx = W * 0.5f, cy = H * 0.5f;

    field j = { .fb = fb, .scale = 1.05f, .core = 0.10f, .band = 0.16f,
                .ring = 0.0f, .hollow = 0.50f, .vign = 0.42f, .seed = 3u };
    dm_job_for(H, field_row, &j);
    stars(fb, 2600, 0.70f, SAFE_H / (f32)H, 17u);

    dm_text st = dm_text_default();
    st.tracking  = 0.26f;
    /* Fill most of the safe width, but stop short of the cap height that would
     * crowd the strapline underneath. */
    st.size      = dm_clamp(fit_size(WORDMARK, st.tracking, SAFE_W * 0.87f),
                            90.0f, 170.0f);
    st.weight    = st.size * 0.058f;
    st.color     = v3_scl(dm_rgb8(255, 246, 230), 1.32f);
    st.glow      = st.size * 0.085f;
    st.glow_gain = 0.22f;
    dm_text_draw_centered(fb, V2(cx, cy - 82.0f), WORDMARK, &st);

    dm_text sub = dm_text_default();
    sub.size      = 26.0f;
    sub.weight    = sub.size * 0.085f;
    sub.tracking  = 0.24f;
    sub.color     = v3_scl(dm_rgb8(158, 196, 252), 0.92f);
    sub.glow      = sub.size * 0.18f;
    sub.glow_gain = 0.24f;
    const char *strap = STRAPLINE;
    f32 sw = dm_text_width(strap, &sub);

    /* A hairline under the name, exactly as wide as the strapline, so the
     * three lines read as one object instead of a stack. */
    for (int y = (int)(cy + 6.0f); y < (int)(cy + 9.0f); y++)
        for (int x = (int)(cx - sw * 0.5f); x < (int)(cx + sw * 0.5f); x++) {
            f32 e = 1.0f - fabsf(((f32)x - cx) / (sw * 0.5f));
            dm_fb_add(fb, x, y, v3_scl(dm_rgb8(96, 148, 228),
                                       0.95f * dm_smoothstep(0.0f, 0.35f, e)));
        }

    dm_text_draw_centered(fb, V2(cx, cy + 62.0f), strap, &sub);

    dm_text tag = dm_text_default();
    tag.size      = 19.0f;
    tag.weight    = tag.size * 0.095f;
    tag.tracking  = 0.62f;
    tag.color     = v3_scl(dm_rgb8(120, 150, 200), 0.62f);
    tag.glow      = 0.0f;
    dm_text_draw_centered(fb, V2(cx, cy + 150.0f), TAGLINE, &tag);

    printf("[banner] safe box %.0fx%.0f -- wordmark %.0f px at %.0f pt, strap %.0f px\n",
           SAFE_W, SAFE_H, dm_text_width(WORDMARK, &st), st.size, sw);

    dm_post_params p = dm_post_defaults();
    p.bloom_threshold = 1.05f;
    p.bloom_intensity = 0.35f;
    p.bloom_radius    = 0.80f;
    p.barrel          = 0.0f;
    p.chroma          = 0.0010f;
    p.vignette        = 0.30f;
    p.grain           = 0.0045f;

    dm_post *post = dm_post_new(W, H, 7);
    save(fb, post, &p, OUT_DIR "/banner.ppm");
    dm_post_free(post);
    dm_fb_free(fb);
    return 0;
}

/* The corner watermark sits on top of arbitrary video, so it is drawn on black
 * and shipped with a matching alpha mask: ink where the glyphs are, nothing
 * anywhere else. */
static int mode_watermark(void)
{
    const int S = 300;
    dm_fb *fb = dm_fb_new(S, S);
    if (!fb) return 1;
    dm_fb_clear(fb, V3(0, 0, 0));

    dm_text st = dm_text_default();
    st.size      = (f32)S * 0.46f;
    st.weight    = st.size * 0.115f;
    st.tracking  = -0.055f;
    st.color     = v3_scl(dm_rgb8(255, 250, 240), 1.55f);
    st.glow      = st.size * 0.07f;
    st.glow_gain = 0.22f;
    dm_text_draw_centered(fb, V2((f32)S * 0.5f, (f32)S * 0.5f), MONOGRAM, &st);

    u8 *rgb = (u8 *)malloc((size_t)S * S * 3);
    u8 *a   = (u8 *)malloc((size_t)S * S);
    if (!rgb || !a) { free(rgb); free(a); dm_fb_free(fb); return 1; }
    dm_fb_resolve(fb, rgb, 1.0f, 0u);

    /* Alpha is the brightest channel: it follows the antialiased edge and the
     * glow exactly, which is what keeps the mark out of a black box. */
    for (int i = 0; i < S * S; i++) {
        u8 m = rgb[i * 3];
        if (rgb[i * 3 + 1] > m) m = rgb[i * 3 + 1];
        if (rgb[i * 3 + 2] > m) m = rgb[i * 3 + 2];
        a[i] = m;
    }
    write_ppm(OUT_DIR "/watermark.ppm", rgb, S, S);
    write_pgm(OUT_DIR "/watermark_a.pgm", a, S, S);

    free(a);
    free(rgb);
    dm_fb_free(fb);
    return 0;
}

/* `l1` and `l2` are the two lines of the caption. They are arguments rather
 * than constants because the caption is the one thing worth iterating on: the
 * thumbnail should say what the title cannot, and which claim that is only
 * becomes clear once the title is written. The stroke font covers ASCII 32..95,
 * so the caption is uppercase and diacritic-free by construction. */
static int mode_thumb(const char *plate_path, const char *tag,
                      const char *l1, const char *l2)
{
    const int W = 1280, H = 720;

    dm_fb *src = read_ppm(plate_path);
    if (!src) return 1;
    dm_fb *fb = dm_fb_new(W, H);
    if (!fb) { dm_fb_free(src); return 1; }

    if (src->w == W && src->h == H) dm_fb_copy(fb, src);
    else                            dm_fb_downsample(fb, src);
    dm_fb_free(src);

    /* A thumbnail is looked at for a third of a second at 210 px wide, so the
     * plate gets pushed harder than the film ever would: more saturation, and
     * a scrim that sinks the bottom left far enough for type to sit on it. */
    for (int y = 0; y < H; y++) {
        f32 fy = (f32)y / (f32)H;
        for (int x = 0; x < W; x++) {
            f32 fx = (f32)x / (f32)W;
            v3  c  = dm_saturate_col(dm_fb_get(fb, x, y), 1.14f);
            f32 k  = 1.0f - 0.80f * dm_smoothstep(0.30f, 1.0f, fy)
                                  * dm_smoothstep(0.95f, 0.15f, fx);
            k *= 1.0f - 0.30f * dm_smoothstep(0.55f, 0.02f, fy)
                              * dm_smoothstep(0.75f, 0.00f, fx);
            dm_fb_set(fb, x, y, v3_scl(c, k));
        }
    }

    /* Alpha blended rather than additive: over a bright fractal, additive
     * white text is the one thing that reliably disappears. */
    dm_text big = dm_text_default();
    big.size      = 96.0f;
    big.weight    = big.size * 0.072f;
    big.tracking  = 0.09f;
    big.color     = v3_scl(dm_rgb8(255, 248, 236), 1.30f);
    big.glow      = big.size * 0.10f;
    big.glow_gain = 0.30f;
    big.additive  = 0;

    dm_text_draw(fb, V2(66.0f, 528.0f), l1, &big);
    big.color = v3_scl(dm_rgb8(255, 196, 120), 1.45f);
    dm_text_draw(fb, V2(66.0f, 648.0f), l2, &big);

    dm_text kick = dm_text_default();
    kick.size      = 28.0f;
    kick.weight    = kick.size * 0.090f;
    kick.tracking  = 0.55f;
    kick.color     = v3_scl(dm_rgb8(170, 205, 255), 1.25f);
    kick.glow      = kick.size * 0.20f;
    kick.glow_gain = 0.35f;
    dm_text_draw(fb, V2(70.0f, 84.0f), "COLD START / KORMOS", &kick);

    printf("[thumb] %s: \"%s\" %.0f px, \"%s\" %.0f px of 1280\n",
           tag, l1, dm_text_width(l1, &big), l2, dm_text_width(l2, &big));

    dm_post_params p = dm_post_defaults();
    p.bloom_threshold = 1.05f;
    p.bloom_intensity = 0.40f;
    p.bloom_radius    = 0.65f;
    p.barrel          = 0.0f;
    p.chroma          = 0.0008f;
    p.vignette        = 0.34f;
    p.grain           = 0.005f;

    char path[256];
    snprintf(path, sizeof path, OUT_DIR "/thumb_%s.ppm", tag);
    dm_post *post = dm_post_new(W, H, 6);
    save(fb, post, &p, path);
    dm_post_free(post);
    dm_fb_free(fb);
    return 0;
}

int main(int argc, char **argv)
{
    dm_job_init(0);

    const char *mode = argc > 1 ? argv[1] : "all";
    int rc = 0;

    if      (strcmp(mode, "avatar")    == 0) rc = mode_avatar();
    else if (strcmp(mode, "banner")    == 0) rc = mode_banner();
    else if (strcmp(mode, "watermark") == 0) rc = mode_watermark();
    else if (strcmp(mode, "thumb")     == 0) {
        if (argc < 6) {
            fprintf(stderr, "usage: brand thumb <plate.ppm> <tag> <line1> <line2>\n");
            rc = 1;
        } else {
            rc = mode_thumb(argv[2], argv[3], argv[4], argv[5]);
        }
    } else if (strcmp(mode, "all") == 0) {
        rc = mode_avatar();
        if (!rc) rc = mode_banner();
        if (!rc) rc = mode_watermark();
    } else {
        fprintf(stderr, "unknown mode: %s\n", mode);
        rc = 1;
    }

    dm_job_shutdown();
    return rc;
}
