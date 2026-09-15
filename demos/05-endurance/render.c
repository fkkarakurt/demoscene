/* render.c -- the camera: how a ray leaves it, and how things too small to
 * march are put back into the picture it took.
 *
 * Nothing here knows what is in front of the lens. A scene hands over one view
 * of the world per exposure slice and a function that says what light arrives
 * along a ray; this file decides which rays to ask about.
 */
#include "demo.h"

#include <stdlib.h>
#include <string.h>

/* ---- the lens -------------------------------------------------------------- */

en_cam en_cam_look(v3 pos, v3 target, f32 roll, f32 focal_mm, f32 fstop, f32 focus_m)
{
    en_cam c;
    c.pos = pos;
    c.fw  = v3_norm(v3_sub(target, pos));

    v3 world_up = fabsf(c.fw.y) > 0.999f ? V3(0.0f, 0.0f, 1.0f) : V3(0.0f, 1.0f, 0.0f);
    v3 rt = v3_norm(v3_cross(world_up, c.fw));
    v3 up = v3_cross(c.fw, rt);

    /* A camera operator's roll: turning the frame about the lens axis. */
    f32 cr = cosf(roll), sr = sinf(roll);
    c.rt = v3_add(v3_scl(rt, cr), v3_scl(up, sr));
    c.up = v3_sub(v3_scl(up, cr), v3_scl(rt, sr));

    c.focal_mm = focal_mm;
    c.fstop    = fstop;
    c.focus_m  = focus_m;
    c.gate_mm  = EN_GATE_S35;
    return c;
}

int en_project(const en_cam *cam, int w, int h, v3 p, v2 *px, f32 *z)
{
    v3  v  = v3_sub(p, cam->pos);
    f32 zz = v3_dot(v, cam->fw);
    if (zz < 0.02f) return 0;

    f32 gate_h = cam->gate_mm * (f32)h / (f32)w;
    f32 sx = v3_dot(v, cam->rt) / zz * cam->focal_mm;
    f32 sy = v3_dot(v, cam->up) / zz * cam->focal_mm;
    px->x = (sx / cam->gate_mm + 0.5f) * (f32)w;
    px->y = (0.5f - sy / gate_h) * (f32)h;
    *z = zz;
    return 1;
}

/* The thin lens equation, with the circle measured in pixels of this picture.
 * The aperture is the focal length over the f-number; everything else is
 * similar triangles. */
f32 en_coc_px(const en_cam *cam, int w, f32 z)
{
    if (cam->fstop <= 0.0f) return 0.0f;
    f32 f  = cam->focal_mm;
    f32 A  = f / cam->fstop;
    f32 zm = DM_MAX(z, 0.01f) * 1000.0f;
    f32 fm = DM_MAX(cam->focus_m * 1000.0f, f * 1.01f);
    f32 c  = A * f * fabsf(zm - fm) / (zm * (fm - f));
    f32 pitch = cam->gate_mm / (f32)w;
    return 0.5f * c / pitch;
}

/* Shirley's concentric map: square to disc without bunching at the centre,
 * so a stratified square stays stratified on the aperture. */
static v2 square_to_disc(v2 u)
{
    f32 a = 2.0f * u.x - 1.0f, b = 2.0f * u.y - 1.0f;
    if (a == 0.0f && b == 0.0f) return V2(0.0f, 0.0f);
    f32 r, phi;
    if (a * a > b * b) { r = a; phi = (DM_PI * 0.25f) * (b / a); }
    else               { r = b; phi = DM_HALFPI - (DM_PI * 0.25f) * (a / b); }
    return V2(r * cosf(phi), r * sinf(phi));
}

/* ---- the sampler ------------------------------------------------------------
 *
 * Two passes. The first puts one sample through every pixel. The second looks
 * at the result and puts the rest of the samples only where a pixel differs
 * from its neighbours by more than the eye would forgive -- edges, texture,
 * the noise of a first sample through an out-of-focus lens or a moving
 * shutter. A patch of flat snow in focus needs one sample; the rope in front
 * of it needs all of them. On a frame of ice and sky that is most of the cost
 * of a frame saved, and on a frame of detail it is none of it, which is the
 * right way round.
 */

typedef struct {
    dm_fb        *fb;
    f32          *depth;
    const en_ctx *c;
    en_pixel_fn   fn;
    const void   *views;
    size_t        view_size;
    int           nviews;
    u8           *mask;         /* pass two: which pixels get the rest       */
    int           k0, k1;       /* the sample indices this pass takes        */
} shade_job;

static v3 sample_px(const shade_job *s, int x, int y, int k, f32 *t_axis)
{
    const en_ctx *c = s->c;
    int W = s->fb->w, H = s->fb->h;

    /* One hash per pixel decorrelates its samples from its neighbours': the
     * same low-discrepancy points, rotated by a different amount, and the
     * exposure slices dealt out in a different order. Without it every pixel
     * would see the same instant through the same corner of the lens, and
     * motion blur would come out as a row of ghosts. */
    u32 h  = dm_hash3i(x, y, c->frame, 0xe4d0u);
    v2  r1 = V2(dm_u32_to_f32(h), dm_u32_to_f32(dm_hash_u32(h ^ 0x1b873593u)));
    v2  r2 = V2(dm_u32_to_f32(dm_hash_u32(h ^ 0xcc9e2d51u)),
                dm_u32_to_f32(dm_hash_u32(h ^ 0x85ebca6bu)));
    int off = (int)(dm_hash_u32(h ^ 0x27d4eb2fu) % (u32)s->nviews);

    int slice = (k + off) % s->nviews;
    const en_view *view = (const en_view *)((const char *)s->views + s->view_size * (size_t)slice);
    const en_cam  *cam  = &view->cam;

    v2 j = v2_fract(v2_add(dm_r2((u32)k), r1));
    v2 l = v2_fract(v2_add(dm_r2((u32)k + 57u), r2));

    f32 gate_h = cam->gate_mm * (f32)H / (f32)W;
    f32 sx = (((f32)x + j.x) / (f32)W - 0.5f) * cam->gate_mm;
    f32 sy = (0.5f - ((f32)y + j.y) / (f32)H) * gate_h;

    v3 d = v3_norm(v3_add(v3_add(v3_scl(cam->rt, sx), v3_scl(cam->up, sy)),
                          v3_scl(cam->fw, cam->focal_mm)));

    en_ray r;
    r.ro = cam->pos;
    r.rd = d;
    if (cam->fstop > 0.0f) {
        /* Every ray through the lens meets the chief ray again on the plane
         * of focus; that is the definition of focus. */
        f32 ar = cam->focal_mm / (2.0f * cam->fstop) * 0.001f;
        v2  q  = square_to_disc(l);
        v3  fp = v3_add(cam->pos, v3_scl(d, cam->focus_m / DM_MAX(v3_dot(d, cam->fw), 1e-4f)));
        r.ro = v3_add(cam->pos, v3_add(v3_scl(cam->rt, q.x * ar), v3_scl(cam->up, q.y * ar)));
        r.rd = v3_norm(v3_sub(fp, r.ro));
    }
    r.cone = (cam->gate_mm / (f32)W) / cam->focal_mm;
    r.seed = dm_hash_u32(h + (u32)k * 0x9e3779b9u);
    r.x = x; r.y = y; r.k = k;

    f32 t = 1e9f;
    v3 col = s->fn(&r, view, &t);
    if (!(col.x == col.x) || !(col.y == col.y) || !(col.z == col.z))
        col = V3(0.0f, 0.0f, 0.0f);
    if (t_axis) *t_axis = t < 1e8f ? t * v3_dot(r.rd, cam->fw) : 1e9f;
    return col;
}

static void shade_row(int y, int thread, void *ud)
{
    (void)thread;
    shade_job *s = (shade_job *)ud;
    int W = s->fb->w;
    int spp = s->c->spp < 1 ? 1 : s->c->spp;

    for (int x = 0; x < W; x++) {
        size_t i = (size_t)y * W + x;
        if (s->mask) {
            if (!s->mask[i]) continue;
            /* The first sample is already in the buffer. */
            v3 acc = dm_fb_get(s->fb, x, y);
            for (int k = s->k0; k < s->k1; k++)
                acc = v3_add(acc, sample_px(s, x, y, k, NULL));
            dm_fb_set(s->fb, x, y, v3_scl(acc, 1.0f / (f32)spp));
        } else {
            f32 dep = 1e9f;
            v3  acc = V3(0.0f, 0.0f, 0.0f);
            for (int k = s->k0; k < s->k1; k++) {
                f32 tk;
                acc = v3_add(acc, sample_px(s, x, y, k, &tk));
                if (k == s->k0) dep = tk;
            }
            dm_fb_set(s->fb, x, y, v3_scl(acc, 1.0f / (f32)(s->k1 - s->k0)));
            if (s->depth) s->depth[i] = dep;
        }
    }
}

typedef struct {
    const dm_fb *fb;
    u8          *mask;
    f32          floor_l;
} mask_job;

/* Contrast is judged in the log of luminance, with a floor under it so the
 * grain of near-black does not count as detail. */
static void mask_row(int y, int thread, void *ud)
{
    (void)thread;
    mask_job *m = (mask_job *)ud;
    int W = m->fb->w, H = m->fb->h;
    const f32 TH = 0.10f;

    for (int x = 0; x < W; x++) {
        f32 c = logf(dm_luma(dm_fb_get(m->fb, x, y)) + m->floor_l);
        int need = 0;
        for (int dy = -1; dy <= 1 && !need; dy++) {
            int yy = y + dy;
            if (yy < 0 || yy >= H) continue;
            for (int dx = -1; dx <= 1; dx++) {
                int xx = x + dx;
                if ((dx | dy) == 0 || xx < 0 || xx >= W) continue;
                f32 n = logf(dm_luma(dm_fb_get(m->fb, xx, yy)) + m->floor_l);
                if (fabsf(n - c) > TH) { need = 1; break; }
            }
        }
        m->mask[(size_t)y * W + x] = (u8)need;
    }
}

void en_shade(dm_fb *fb, f32 *depth, const en_ctx *c, en_pixel_fn fn,
              const void *views, size_t view_size, int nviews)
{
    shade_job s;
    s.fb        = fb;
    s.depth     = depth;
    s.c         = c;
    s.fn        = fn;
    s.views     = views;
    s.view_size = view_size;
    s.nviews    = nviews < 1 ? 1 : nviews;
    s.mask      = NULL;

    int spp = c->spp < 1 ? 1 : c->spp;
    if (spp == 1 || getenv("EN_FULL")) {
        s.k0 = 0; s.k1 = spp;
        dm_job_for(fb->h, shade_row, &s);
        return;
    }

    s.k0 = 0; s.k1 = 1;
    dm_job_for(fb->h, shade_row, &s);

    size_t n = (size_t)fb->w * fb->h;
    u8 *mask = (u8 *)malloc(n);
    u8 *grow = (u8 *)malloc(n);
    if (!mask || !grow) {
        free(mask); free(grow);
        s.k0 = 0; s.k1 = spp;
        dm_job_for(fb->h, shade_row, &s);
        return;
    }

    f64 mean = 0.0;
    for (size_t i = 0; i < n; i += 97) mean += dm_luma(V3(fb->px[i * 3], fb->px[i * 3 + 1], fb->px[i * 3 + 2]));
    mean /= (f64)(n / 97 + 1);

    mask_job mj;
    mj.fb = fb;
    mj.mask = mask;
    mj.floor_l = (f32)DM_MAX(mean * 0.04, 1e-6);
    dm_job_for(fb->h, mask_row, &mj);

    /* Grow the mask by a pixel: a flagged pixel's neighbour is usually half
     * across the same edge. */
    for (int y = 0; y < fb->h; y++)
        for (int x = 0; x < fb->w; x++) {
            u8 v = 0;
            for (int dy = -1; dy <= 1 && !v; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    int xx = x + dx, yy = y + dy;
                    if (xx < 0 || yy < 0 || xx >= fb->w || yy >= fb->h) continue;
                    if (mask[(size_t)yy * fb->w + xx]) { v = 1; break; }
                }
            grow[(size_t)y * fb->w + x] = v;
        }

    s.mask = grow;
    s.k0 = 1; s.k1 = spp;
    dm_job_for(fb->h, shade_row, &s);

    free(mask);
    free(grow);
}
/* ---- lines ------------------------------------------------------------------ */

static void blend(dm_fb *fb, int x, int y, v3 col, f32 a)
{
    if (a <= 0.0f) return;
    if (a > 1.0f) a = 1.0f;
    v3 d = dm_fb_get(fb, x, y);
    dm_fb_set(fb, x, y, v3_add(v3_scl(d, 1.0f - a), v3_scl(col, a)));
}

typedef struct {
    v2  A, B;
    f32 za, zb;
    f32 wa, wb;         /* drawn half-width, px                              */
    f32 ea, eb;         /* opacity scale: true width over drawn width        */
    v3  ca, cb;
    f32 alpha;
} proj_line;

static void draw_one(dm_fb *fb, const f32 *depth, const proj_line *L, f32 weight)
{
    int W = fb->w, H = fb->h;
    v2  dv  = v2_sub(L->B, L->A);
    f32 len = v2_len(dv);
    v2  dir = len > 1e-6f ? v2_scl(dv, 1.0f / len) : V2(1.0f, 0.0f);
    f32 wmax = DM_MAX(L->wa, L->wb) + 1.5f;

    /* Walk the longer axis one pixel at a time and cover the short axis across
     * the width, so each pixel is visited once and the blend is honest. */
    int steep = fabsf(dv.y) > fabsf(dv.x);
    f32 a0 = steep ? L->A.y : L->A.x, a1 = steep ? L->B.y : L->B.x;
    if (a0 > a1) { f32 s = a0; a0 = a1; a1 = s; }
    int i0 = (int)floorf(a0 - wmax), i1 = (int)ceilf(a1 + wmax);
    int lim = steep ? H - 1 : W - 1;
    if (i0 < 0) i0 = 0;
    if (i1 > lim) i1 = lim;

    f32 slope = steep ? (fabsf(dv.y) > 1e-6f ? dv.x / dv.y : 0.0f)
                      : (fabsf(dv.x) > 1e-6f ? dv.y / dv.x : 0.0f);
    f32 span = wmax * sqrtf(1.0f + slope * slope) + 1.0f;

    for (int i = i0; i <= i1; i++) {
        f32 fi = (f32)i + 0.5f;
        f32 base = steep ? L->A.x + (fi - L->A.y) * slope
                         : L->A.y + (fi - L->A.x) * slope;
        int j0 = (int)floorf(base - span), j1 = (int)ceilf(base + span);
        int jlim = steep ? W - 1 : H - 1;
        if (j0 < 0) j0 = 0;
        if (j1 > jlim) j1 = jlim;

        for (int j = j0; j <= j1; j++) {
            int x = steep ? j : i, y = steep ? i : j;
            v2  p = V2((f32)x + 0.5f, (f32)y + 0.5f);
            v2  pa = v2_sub(p, L->A);
            f32 t = len > 1e-6f ? dm_sat(v2_dot(pa, dir) / len) : 0.0f;
            f32 d = v2_len(v2_sub(pa, v2_scl(dir, t * len)));

            f32 w = dm_lerp(L->wa, L->wb, t);
            f32 cov = dm_sat(w + 0.5f - d);
            if (cov <= 0.0f) continue;

            f32 z = dm_lerp(L->za, L->zb, t);
            if (depth && z > depth[(size_t)y * W + x] * 1.004f + 0.02f) continue;

            f32 a = cov * dm_lerp(L->ea, L->eb, t) * L->alpha * weight;
            blend(fb, x, y, v3_lerp(L->ca, L->cb, t), a);
        }
    }
}

void en_lines_draw(dm_fb *fb, const f32 *depth, const en_cam *cam,
                   const en_line *lines, int n, f32 weight)
{
    const f32 NEAR = 0.05f;
    f32 pitch = cam->gate_mm / (f32)fb->w;

    for (int i = 0; i < n; i++) {
        const en_line *s = &lines[i];
        v3 a = s->a, b = s->b;
        v3 ca = s->ca, cb = s->cb;

        /* Clip to just in front of the lens. A shroud running past the camera
         * has one end behind it, and projecting that end throws the line
         * across the whole frame. */
        f32 za = v3_dot(v3_sub(a, cam->pos), cam->fw);
        f32 zb = v3_dot(v3_sub(b, cam->pos), cam->fw);
        if (za < NEAR && zb < NEAR) continue;
        if (za < NEAR || zb < NEAR) {
            f32 u = (NEAR - za) / (zb - za);
            v3  m = v3_lerp(a, b, u);
            v3  cm = v3_lerp(ca, cb, u);
            if (za < NEAR) { a = m; ca = cm; } else { b = m; cb = cm; }
        }

        proj_line L;
        if (!en_project(cam, fb->w, fb->h, a, &L.A, &L.za)) continue;
        if (!en_project(cam, fb->w, fb->h, b, &L.B, &L.zb)) continue;

        f32 ra = s->radius * cam->focal_mm / (L.za * pitch);
        f32 rb = s->radius * cam->focal_mm / (L.zb * pitch);
        f32 ka = en_coc_px(cam, fb->w, L.za), kb = en_coc_px(cam, fb->w, L.zb);

        /* Never thinner than a pixel, and a line that had to be widened to get
         * there is dimmed by the same factor: a rope a third of a pixel wide
         * covers a third of the pixel, which is exactly how bright it is. */
        L.wa = DM_MAX(ra, 0.5f) + ka;
        L.wb = DM_MAX(rb, 0.5f) + kb;
        L.ea = dm_sat(ra / L.wa);
        L.eb = dm_sat(rb / L.wb);
        L.ca = ca;
        L.cb = cb;
        L.alpha = s->alpha;

        f32 minx = DM_MIN(L.A.x, L.B.x), maxx = DM_MAX(L.A.x, L.B.x);
        f32 miny = DM_MIN(L.A.y, L.B.y), maxy = DM_MAX(L.A.y, L.B.y);
        f32 wm = DM_MAX(L.wa, L.wb) + 2.0f;
        if (maxx < -wm || minx > (f32)fb->w + wm || maxy < -wm || miny > (f32)fb->h + wm)
            continue;

        draw_one(fb, depth, &L, weight);
    }
}

/* ---- particles ---------------------------------------------------------------- */

void en_splat(dm_fb *fb, const f32 *depth, const en_cam *cam,
              v3 p, f32 radius, v3 radiance, f32 weight)
{
    v2  c;
    f32 z;
    if (!en_project(cam, fb->w, fb->h, p, &c, &z)) return;

    f32 pitch = cam->gate_mm / (f32)fb->w;
    f32 r  = radius * cam->focal_mm / (z * pitch);
    f32 k  = en_coc_px(cam, fb->w, z);
    f32 R  = DM_MAX(sqrtf(r * r + k * k), 0.6f);
    f32 R2 = R + 1.0f;

    if (c.x < -R2 || c.y < -R2 || c.x > (f32)fb->w + R2 || c.y > (f32)fb->h + R2) return;

    /* The energy of a disc of radius r spread over one of radius R. */
    f32 e = (r * r) / (R * R) * weight;
    if (e * v3_maxc(radiance) < 1e-5f) return;

    int x0 = (int)floorf(c.x - R2), x1 = (int)ceilf(c.x + R2);
    int y0 = (int)floorf(c.y - R2), y1 = (int)ceilf(c.y + R2);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > fb->w - 1) x1 = fb->w - 1;
    if (y1 > fb->h - 1) y1 = fb->h - 1;

    /* A pixel-wide splat has to be normalised by what the rasterised disc
     * actually covers, or a particle flickers in brightness as it crosses the
     * pixel grid. */
    f32 sum = 0.0f;
    if (R < 2.5f) {
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) {
                f32 d = v2_len(V2((f32)x + 0.5f - c.x, (f32)y + 0.5f - c.y));
                sum += dm_sat(R + 0.5f - d);
            }
        if (sum <= 0.0f) return;
    } else {
        sum = DM_PI * R * R;
    }
    v3 val = v3_scl(radiance, e * DM_PI * R * R / sum);

    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            if (depth && z > depth[(size_t)y * fb->w + x] * 1.004f + 0.02f) continue;
            f32 d = v2_len(V2((f32)x + 0.5f - c.x, (f32)y + 0.5f - c.y));
            f32 cov = dm_sat(R + 0.5f - d);
            if (cov > 0.0f) dm_fb_add(fb, x, y, v3_scl(val, cov));
        }
}
