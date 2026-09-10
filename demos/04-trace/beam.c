#include "beam.h"
#include "../../engine/dm_job.h"

#include <stdlib.h>
#include <string.h>

/* ---- limits and units ------------------------------------------------------ */

#define BAND     8             /* rows per work item                          */
#define MAX_SEG  1250000       /* 1.5 s of history cut at most 16 ways, with room */
#define MAX_SUB  16            /* pieces per sample interval                   */
#define MAX_ROWS 8192
#define NEAR     0.04f

/* Light is specified for a frame 1920 pixels tall. A trace drawn into half
 * the height covers half the length at half the width, which is four times
 * the light per pixel; scaling energy by the square of the height keeps a
 * preview and a final render the same brightness. */
#define REF_H 1920.0f

typedef struct {
    f32 x0, y0, x1, y1;        /* screen pixels                               */
    f32 sig;                   /* spot sigma, pixels                           */
    f32 r, g, b;               /* energy                                       */
    int ya, yb;                /* rows touched, [ya, yb)                       */
} seg;

static seg *S;
static int  NS;
static int *IDX;
static int  IDX_CAP;
static int  BSTART[MAX_ROWS / BAND + 2];
static int  BFILL[MAX_ROWS / BAND + 2];

static void tables_init(void);

int tr_beam_init(void)
{
    if (S) return 1;
    tables_init();
    S = (seg *)malloc((size_t)MAX_SEG * sizeof(seg));
    IDX_CAP = 4 << 20;
    IDX = (int *)malloc((size_t)IDX_CAP * sizeof(int));
    return S && IDX;
}

void tr_beam_free(void)
{
    free(S);
    free(IDX);
    S = NULL;
    IDX = NULL;
}

/* ---- the lens -------------------------------------------------------------- */

typedef struct {
    v3  eye, rt, up, fw;
    f32 fpx;                   /* focal length in pixels                      */
    f32 cx, cy;                /* where the optical axis lands                */
    f32 inv_focus, coc_k;
} lens;

static void lens_make(lens *L, const tr_camera *c, int w, int h)
{
    v3 fw = v3_norm(v3_sub(c->target, c->eye));
    v3 wu = fabsf(fw.y) > 0.999f ? V3(0.0f, 0.0f, -1.0f) : V3(0.0f, 1.0f, 0.0f);
    v3 rt = v3_norm(v3_cross(fw, wu));
    v3 up = v3_cross(rt, fw);

    f32 cr = cosf(c->roll), sr = sinf(c->roll);
    L->rt  = v3_add(v3_scl(rt, cr), v3_scl(up, sr));
    L->up  = v3_sub(v3_scl(up, cr), v3_scl(rt, sr));
    L->fw  = fw;
    L->eye = c->eye;

    L->fpx = 0.5f * (f32)h / tanf(0.5f * c->fov);
    L->cx  = 0.5f * (f32)w + c->shift.x * (f32)h;
    L->cy  = 0.5f * (f32)h - c->shift.y * (f32)h;

    L->inv_focus = 1.0f / DM_MAX(c->focus, 1e-3f);
    L->coc_k     = c->aperture * L->fpx;
}

static inline int lens_project(const lens *L, v3 p, f32 *sx, f32 *sy, f32 *z)
{
    v3  d  = v3_sub(p, L->eye);
    f32 zc = v3_dot(d, L->fw);
    if (zc < NEAR) return 0;
    f32 k = L->fpx / zc;
    *sx = L->cx + v3_dot(d, L->rt) * k;
    *sy = L->cy - v3_dot(d, L->up) * k;
    *z  = zc;
    return 1;
}

/* The spot's width on screen: the beam's own width, projected, and the disc of
 * confusion for anything off the plane of focus. A disc of diameter c has a
 * standard deviation of c / 4 along each axis, and a gaussian of that sigma
 * stands in for it. */
static inline f32 lens_sigma(const lens *L, f32 z, f32 width)
{
    f32 beam = width * L->fpx / z;
    f32 coc  = L->coc_k * fabsf(L->inv_focus - 1.0f / z);
    return DM_MAX(sqrtf(beam * beam + dm_sq(coc * 0.25f)), 0.6f);
}

/* ---- the phosphor ---------------------------------------------------------- */

/* Light captured during the slice [t0, t1] from trace laid down at time s, by
 * a component that decays with time constant tau: the integral of
 * exp(-(t - s) / tau) over the part of the slice after s. Old trace gives
 * about exp(-age / tau) times the slice length; trace drawn during the slice
 * gives up to tau, all at once, which is the flash. */
static inline f32 captured(f64 s, f64 t0, f64 t1, f32 tau)
{
    if (s > t1) return 0.0f;
    f64 a = s < t0 ? t0 - s : 0.0;
    f64 b = t1 - s;
    return (f32)((f64)tau * (exp(-a / tau) - exp(-b / tau)));
}

/* ---- building the segments ------------------------------------------------- */

static inline v2 catmull(v2 p0, v2 p1, v2 p2, v2 p3, f32 u)
{
    f32 u2 = u * u, u3 = u2 * u;
    return V2(0.5f * (2.0f * p1.x + (p2.x - p0.x) * u
                      + (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * u2
                      + (3.0f * p1.x - p0.x - 3.0f * p2.x + p3.x) * u3),
              0.5f * (2.0f * p1.y + (p2.y - p0.y) * u
                      + (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * u2
                      + (3.0f * p1.y - p0.y - 3.0f * p2.y + p3.y) * u3));
}

static inline v2 sample_at(const dm_audio *a, int i)
{
    if (i < 0) i = 0;
    if (i >= a->n) i = a->n - 1;
    return V2(a->l[i], a->r[i]);
}

static void emit(f32 x0, f32 y0, f32 x1, f32 y1, f32 sig, v3 e, int w, int h)
{
    if (NS >= MAX_SEG) return;
    f32 r = 3.0f * sig;
    f32 lx = DM_MIN(x0, x1) - r, hx = DM_MAX(x0, x1) + r;
    f32 ly = DM_MIN(y0, y1) - r, hy = DM_MAX(y0, y1) + r;
    if (hx < 0.0f || lx > (f32)w || hy < 0.0f || ly > (f32)h) return;

    seg *s = &S[NS++];
    s->x0 = x0; s->y0 = y0; s->x1 = x1; s->y1 = y1;
    s->sig = sig;
    s->r = e.x; s->g = e.y; s->b = e.z;
    s->ya = (int)floorf(DM_MAX(ly, 0.0f));
    s->yb = (int)ceilf(DM_MIN(hy, (f32)h));
    if (s->yb <= s->ya) NS--;
}

/* Far down the time axis the trace is small and heavily out of focus: a
 * sample moves half a pixel under a blur six pixels wide, and drawing each of
 * those as its own segment paints the same disc a dozen times over. So
 * consecutive pieces are gathered into a run for as long as the whole run fits
 * well inside its own blur, and the run is drawn once with their summed
 * energy. Whatever it smooths over is finer than the blur that would have
 * smoothed it anyway. */
static struct {
    int on;
    f32 x0, y0, x1, y1, sig, path;
    v3  e;
} R;

static void flush(int w, int h)
{
    if (R.on) emit(R.x0, R.y0, R.x1, R.y1, R.sig, R.e, w, h);
    R.on = 0;
}

/* `build` walks the trace from the newest sample back, so each piece offered
 * here ends where the run so far begins, and joining it moves the start. */
static void gather(f32 x0, f32 y0, f32 x1, f32 y1, f32 sig, v3 e, int w, int h)
{
    f32 len = sqrtf(dm_sq(x1 - x0) + dm_sq(y1 - y0));
    if (R.on && fabsf(sig - R.sig) < 0.2f * R.sig
             && R.path + len < 0.9f * R.sig
             && sqrtf(dm_sq(R.x1 - x0) + dm_sq(R.y1 - y0)) < 0.6f * R.sig) {
        R.x0 = x0; R.y0 = y0;
        R.path += len;
        R.e = v3_add(R.e, e);
        return;
    }
    flush(w, h);
    if (len >= 0.6f * sig) { emit(x0, y0, x1, y1, sig, e, w, h); return; }
    R.on = 1;
    R.x0 = x0; R.y0 = y0; R.x1 = x1; R.y1 = y1;
    R.sig = sig; R.path = len; R.e = e;
}

static void build(const dm_audio *a, const lens *L, const tr_beam *b, int w, int h)
{
    NS = 0;
    R.on = 0;
    f64 sr = (f64)DM_SR;
    int hi = (int)floor(b->t1 * sr);
    int lo = (int)ceil((b->t1 - (f64)b->history) * sr);
    if (hi > a->n - 1) hi = a->n - 1;
    if (lo < 0) lo = 0;
    if (hi <= lo) return;

    f32 res = dm_sq((f32)h / REF_H);
    f32 per = b->gain / (f32)sr / (f32)b->shutter * res;

    /* Newest first, so if the budget ever runs out it is the oldest and
     * faintest trace that goes without. */
    for (int i = hi - 1; i >= lo; i--) {
        f64 sm = ((f64)i + 0.5) / sr;
        f32 cf = captured(sm, b->t0, b->t1, b->fast_tau);
        f32 cm = captured(sm, b->t0, b->t1, b->mid_tau);
        f32 cs = captured(sm, b->t0, b->t1, b->slow_tau);
        v3  e  = v3_add(v3_add(v3_scl(b->fast_col, cf), v3_scl(b->mid_col, cm)),
                        v3_scl(b->slow_col, cs));
        e = v3_scl(e, per);
        if (v3_maxc(e) < 1e-9f) continue;

        v2  p0 = v2_scl(sample_at(a, i - 1), b->scale), p1 = v2_scl(sample_at(a, i), b->scale);
        v2  p2 = v2_scl(sample_at(a, i + 1), b->scale), p3 = v2_scl(sample_at(a, i + 2), b->scale);
        f32 z1 = -(f32)((b->t1 - (f64)i / sr) * (f64)b->speed);
        f32 z2 = -(f32)((b->t1 - (f64)(i + 1) / sr) * (f64)b->speed);

        f32 ax, ay, az, bx, by, bz;
        if (!lens_project(L, V3(p1.x, p1.y, z1), &ax, &ay, &az)) continue;
        if (!lens_project(L, V3(p2.x, p2.y, z2), &bx, &by, &bz)) continue;

        f32 sa = lens_sigma(L, az, b->width), sb = lens_sigma(L, bz, b->width);
        f32 len = sqrtf(dm_sq(bx - ax) + dm_sq(by - ay));

        /* A straight piece three pixels long cannot be told from the curve it
         * stands for; under a wide blur, a piece as long as the blur cannot
         * either. */
        int k = (int)ceilf(len / DM_MAX(3.0f, 0.8f * DM_MIN(sa, sb)));
        if (k < 1) k = 1;
        if (k > MAX_SUB) k = MAX_SUB;

        if (k == 1) {
            gather(ax, ay, bx, by, 0.5f * (sa + sb), e, w, h);
            continue;
        }

        flush(w, h);
        v3  ek = v3_scl(e, 1.0f / (f32)k);
        f32 px = ax, py = ay, ps = sa;
        for (int j = 1; j <= k; j++) {
            f32 qx, qy, qz, qs;
            if (j == k) {
                qx = bx; qy = by; qs = sb;
            } else {
                f32 u = (f32)j / (f32)k;
                v2  c = catmull(p0, p1, p2, p3, u);
                if (!lens_project(L, V3(c.x, c.y, dm_lerp(z1, z2, u)), &qx, &qy, &qz)) break;
                qs = lens_sigma(L, qz, b->width);
            }
            emit(px, py, qx, qy, 0.5f * (ps + qs), ek, w, h);
            px = qx; py = qy; ps = qs;
        }
    }
    flush(w, h);
}

/* Every segment is filed under each band of rows it touches, so a band can be
 * drawn by one worker without anyone else writing to its rows. */
static void bucket(int h)
{
    int nb = (h + BAND - 1) / BAND;
    memset(BSTART, 0, sizeof(int) * (size_t)(nb + 1));
    for (int i = 0; i < NS; i++)
        for (int b = S[i].ya / BAND; b <= (S[i].yb - 1) / BAND; b++) BSTART[b + 1]++;
    for (int b = 0; b < nb; b++) BSTART[b + 1] += BSTART[b];

    int need = BSTART[nb];
    if (need > IDX_CAP) {
        int *grown = (int *)realloc(IDX, (size_t)need * sizeof(int));
        if (!grown) { NS = 0; memset(BSTART, 0, sizeof(int) * (size_t)(nb + 1)); return; }
        IDX = grown;
        IDX_CAP = need;
    }

    memcpy(BFILL, BSTART, sizeof(int) * (size_t)nb);
    for (int i = 0; i < NS; i++)
        for (int b = S[i].ya / BAND; b <= (S[i].yb - 1) / BAND; b++) IDX[BFILL[b]++] = i;
}

/* ---- drawing them ---------------------------------------------------------- */

/* Every pixel of every segment needs a gaussian across the line and two
 * cumulative gaussians along it. As calls to expf that is most of the cost of
 * a frame, and all three are smooth functions of one number, so they are
 * tables, filled once and read with linear interpolation -- good to about
 * 1e-5, which is below anything the 8-bit output can show. */
#define GTAB 1024
#define GMAX 4.5f              /* exp(-u) out to u = 4.5: three sigma          */
#define PTAB 2048
#define PMAX 5.0f              /* the normal CDF over [-5, 5]                  */

static f32 GT[GTAB + 2], PT[PTAB + 2];

static void tables_init(void)
{
    for (int i = 0; i <= GTAB + 1; i++)
        GT[i] = expf(-GMAX * (f32)i / (f32)GTAB);
    for (int i = 0; i <= PTAB + 1; i++) {
        f32 x = -PMAX + 2.0f * PMAX * (f32)i / (f32)PTAB;
        PT[i] = 0.5f * erfcf(-x * 0.70710678f);
    }
}

static inline f32 gauss(f32 u)                 /* exp(-u), u >= 0 */
{
    if (u >= GMAX) return 0.0f;
    f32 f = u * ((f32)GTAB / GMAX);
    int i = (int)f;
    return GT[i] + (GT[i + 1] - GT[i]) * (f - (f32)i);
}

static inline f32 phi(f32 x)                   /* the standard normal CDF */
{
    if (x <= -PMAX) return 0.0f;
    if (x >=  PMAX) return 1.0f;
    f32 f = (x + PMAX) * ((f32)PTAB / (2.0f * PMAX));
    int i = (int)f;
    return PT[i] + (PT[i + 1] - PT[i]) * (f - (f32)i);
}

/* A segment carrying energy E spread evenly along its length, convolved with a
 * gaussian spot. Across the segment that is a gaussian; along it, the
 * difference of two error functions, which rounds the ends off by exactly
 * the right amount for the next segment to continue without a seam. Energy is
 * conserved: the pixels of a segment add up to E, however it is blurred. */
static void raster(dm_fb *fb, const seg *s, int y0, int y1)
{
    int W = fb->w;
    f32 sig = s->sig, inv = 1.0f / sig, reach = 3.0f * sig;

    int xa0 = (int)floorf(DM_MIN(s->x0, s->x1) - reach);
    int xb0 = (int)ceilf(DM_MAX(s->x0, s->x1) + reach);
    if (xa0 < 0) xa0 = 0;
    if (xb0 > W - 1) xb0 = W - 1;
    if (xb0 < xa0) return;

    f32 dx = s->x1 - s->x0, dy = s->y1 - s->y0;
    f32 L2 = dx * dx + dy * dy;

    f32 h2 = 0.5f * inv * inv;

    if (L2 < 1e-4f) {
        f32 k = 1.0f / (DM_TAU * sig * sig);
        for (int y = y0; y < y1; y++) {
            f32 py = (f32)y + 0.5f - s->y0;
            f32 gy = gauss(py * py * h2) * k;
            if (gy <= 0.0f) continue;
            f32 *row = fb->px + (size_t)y * W * 3;
            for (int x = xa0; x <= xb0; x++) {
                f32 px = (f32)x + 0.5f - s->x0;
                f32 g  = gy * gauss(px * px * h2);
                row[x * 3 + 0] += s->r * g;
                row[x * 3 + 1] += s->g * g;
                row[x * 3 + 2] += s->b * g;
            }
        }
        return;
    }

    f32 L  = sqrtf(L2);
    f32 ux = dx / L, uy = dy / L;
    f32 nx = -uy, ny = ux;
    f32 ak = 1.0f / (2.5066283f * sig * L);

    /* Further than five sigma from both ends, the along term is one. */
    f32 edge = PMAX * sig;

    for (int y = y0; y < y1; y++) {
        f32 py = (f32)y + 0.5f - s->y0;
        int xa = xa0, xb = xb0;

        /* Only the pixels within three sigma of the line: for a diagonal
         * segment that is a sliver of its bounding box. */
        if (fabsf(nx) > 1e-3f) {
            f32 c  = py * ny;
            f32 u0 = (-reach - c) / nx, u1 = (reach - c) / nx;
            if (u0 > u1) { f32 t = u0; u0 = u1; u1 = t; }
            int a = (int)floorf(s->x0 + u0 - 0.5f), b = (int)ceilf(s->x0 + u1 - 0.5f);
            if (a > xa) xa = a;
            if (b < xb) xb = b;
        } else if (fabsf(py * ny) > reach) {
            continue;
        }

        f32 *row = fb->px + (size_t)y * W * 3;
        for (int x = xa; x <= xb; x++) {
            f32 px = (f32)x + 0.5f - s->x0;
            f32 d  = px * nx + py * ny;
            f32 g  = gauss(d * d * h2);
            if (g <= 0.0f) continue;
            f32 al = px * ux + py * uy;
            if (al < edge || al > L - edge)
                g *= phi((L - al) * inv) - phi(-al * inv);
            g *= ak;
            row[x * 3 + 0] += s->r * g;
            row[x * 3 + 1] += s->g * g;
            row[x * 3 + 2] += s->b * g;
        }
    }
}

static void raster_band(int band, int thread, void *ud)
{
    (void)thread;
    dm_fb *fb = (dm_fb *)ud;
    int ya = band * BAND, yb = DM_MIN(ya + BAND, fb->h);
    for (int k = BSTART[band]; k < BSTART[band + 1]; k++) {
        const seg *s = &S[IDX[k]];
        raster(fb, s, DM_MAX(ya, s->ya), DM_MIN(yb, s->yb));
    }
}

void tr_beam_draw(dm_fb *fb, const dm_audio *a, const tr_camera *cam, const tr_beam *b)
{
    if (!S || !a || fb->h > MAX_ROWS) return;

    lens L;
    lens_make(&L, cam, fb->w, fb->h);
    build(a, &L, b, fb->w, fb->h);
    bucket(fb->h);
    dm_job_for((fb->h + BAND - 1) / BAND, raster_band, fb);
}

/* ---- saturation ------------------------------------------------------------ */

static void saturate_row(int y, int thread, void *ud)
{
    (void)thread;
    const f32 *lv = (const f32 *)((void **)ud)[1];
    dm_fb *fb = (dm_fb *)((void **)ud)[0];
    f32 level = *lv, inv = 1.0f / level;
    f32 *p = fb->px + (size_t)y * fb->w * 3;
    for (int i = 0; i < fb->w * 3; i++)
        p[i] = level * (1.0f - expf(-p[i] * inv));
}

void tr_phosphor_saturate(dm_fb *fb, f32 level)
{
    if (level <= 0.0f) return;
    void *ud[2] = { fb, &level };
    dm_job_for(fb->h, saturate_row, ud);
}
