#include "xy.h"
#include "../../engine/dm_font.h"
#include "../../engine/dm_rand.h"
#include "../../engine/dm_synth.h"

#include <stdlib.h>
#include <string.h>

static int frames_at(f64 t) { return dm_sec_to_frames(t); }

static void span(const dm_audio *bus, f64 t0, f64 t1, int *i0, int *i1)
{
    *i0 = frames_at(t0);
    *i1 = frames_at(t1);
    if (*i0 < 0) *i0 = 0;
    if (*i1 > bus->n) *i1 = bus->n;
}

static inline f32 white(u32 *s)
{
    *s = dm_hash_u32(*s);
    return dm_u32_to_f32(*s) * 2.0f - 1.0f;
}

/* The same shaper dm_synth puts before its filter. On one channel it adds odd
 * harmonics; on two channels at once it pushes a curve out toward the square
 * that bounds it, which is the same thing seen from the other side. */
static inline f32 shape_drive(f32 x, f32 drive)
{
    if (drive <= 0.0f) return x;
    return tanhf(x * (1.0f + drive * 5.0f)) / (1.0f + drive * 1.2f);
}

/* Linear attack, exponential decay to sustain, exponential release from
 * wherever the envelope was at note-off -- dm_synth's shape. */
static f32 adsr(f32 A, f32 D, f32 S, f32 R, f64 t, f64 len)
{
    if (t < 0.0) return 0.0f;
    A = DM_MAX(A, 1e-4f); D = DM_MAX(D, 1e-4f); R = DM_MAX(R, 1e-4f);
    if (t < len) {
        if (t < A) return (f32)(t / A);
        return S + (1.0f - S) * expf(-(f32)(t - A) / D);
    }
    f32 level = (len < A) ? (f32)(len / A)
                          : S + (1.0f - S) * expf(-(f32)(len - A) / D);
    return level * expf(-(f32)(t - len) / R);
}

/* ---- paths ---------------------------------------------------------------- */

v2 xy_path_circle(f64 phase, f64 t, const void *ud)
{
    (void)t; (void)ud;
    f64 a = (phase - floor(phase)) * 6.283185307179586;
    return V2((f32)cos(a), (f32)sin(a));
}

static xy_table *table_new(int n)
{
    xy_table *tb = (xy_table *)malloc(sizeof(xy_table));
    if (!tb) return NULL;
    tb->n = n;
    tb->p = (v2 *)calloc((size_t)n, sizeof(v2));
    if (!tb->p) { free(tb); return NULL; }
    return tb;
}

void xy_table_free(xy_table *tb)
{
    if (!tb) return;
    free(tb->p);
    free(tb);
}

v2 xy_path_table(f64 phase, f64 t, const void *ud)
{
    (void)t;
    const xy_table *tb = (const xy_table *)ud;
    f64 u = (phase - floor(phase)) * (f64)tb->n;
    int i = (int)u;
    if (i >= tb->n) i = tb->n - 1;
    int j = i + 1 == tb->n ? 0 : i + 1;
    return v2_lerp(tb->p[i], tb->p[j], (f32)(u - (f64)i));
}

/* Resample a closed polyline to `samples` points spaced evenly in time. Edge i
 * runs from pts[i] to pts[i + 1] -- the last one back to pts[0] -- and takes
 * dur[i] units of time to draw. */
static xy_table *resample(const v2 *pts, const f32 *dur, int n, int samples)
{
    xy_table *tb = table_new(samples);
    if (!tb || n < 1) return tb;

    f64 total = 0.0;
    for (int i = 0; i < n; i++) total += dur[i];
    if (total <= 0.0) total = 1.0;

    int e = 0;
    f64 at = 0.0;                       /* time at which edge e begins */
    for (int k = 0; k < samples; k++) {
        f64 tk = total * (f64)k / (f64)samples;
        while (e < n - 1 && at + dur[e] <= tk) { at += dur[e]; e++; }
        f32 f = dur[e] > 0.0f ? (f32)((tk - at) / dur[e]) : 0.0f;
        tb->p[k] = v2_lerp(pts[e], pts[(e + 1) % n], dm_sat(f));
    }
    return tb;
}

/* Subtract the time average. Uniform samples in time make that the plain
 * mean of the table. */
static void centre(xy_table *tb)
{
    v2 m = V2(0.0f, 0.0f);
    for (int i = 0; i < tb->n; i++) m = v2_add(m, tb->p[i]);
    m = v2_scl(m, 1.0f / (f32)tb->n);
    for (int i = 0; i < tb->n; i++) tb->p[i] = v2_sub(tb->p[i], m);
}

xy_table *xy_table_polygon(int n, int step, int samples)
{
    v2  pts[64];
    f32 dur[64];
    if (n < 2)  n = 2;
    if (n > 64) n = 64;
    /* A vertex straight up, so a triangle reads as a triangle and not as an
     * arrow. Equal edges at equal speed: every edge takes the same time. */
    for (int k = 0; k < n; k++) {
        f32 a = DM_HALFPI + DM_TAU * (f32)(k * step) / (f32)n;
        pts[k] = V2(cosf(a), sinf(a));
        dur[k] = 1.0f;
    }
    xy_table *tb = resample(pts, dur, n, samples);
    if (tb) centre(tb);
    return tb;
}

xy_table *xy_table_text(const char *s, f32 half_width, f32 jump, int samples)
{
    int nch = (int)strlen(s);
    if (nch < 1) return NULL;

    int cap = nch * 40 + 8;
    v2  *pts  = (v2 *)malloc((size_t)cap * sizeof(v2));
    f32 *dur  = (f32 *)malloc((size_t)cap * sizeof(f32));
    u8  *isj  = (u8 *)malloc((size_t)cap);
    if (!pts || !dur || !isj) { free(pts); free(dur); free(isj); return NULL; }

    f32 wgrid = (f32)(nch - 1) * DM_GLYPH_ADVANCE + DM_GLYPH_W;
    f32 sc    = 2.0f * half_width / wgrid;

    /* Build one long polyline: every stroke of every glyph, left to right,
     * each stroke joined to the next by a pen-up move. Within a glyph the
     * next stroke is whichever one has an end nearest the pen, and it is
     * drawn from that end -- the shortest jumps are the faintest lines. */
    int n = 0;
    v2  pen = V2(0.0f, DM_GLYPH_H * 0.5f);
    for (int ci = 0; ci < nch; ci++) {
        v2 g[32];
        u8 gd[32];
        int gn = dm_glyph_points((unsigned char)s[ci], g, gd, 32);
        f32 ox = (f32)ci * DM_GLYPH_ADVANCE;

        /* Split into strokes. */
        int st0[16], stn[16], ns = 0;
        for (int i = 0; i < gn && ns < 16; i++) {
            if (!gd[i]) { st0[ns] = i; stn[ns] = 1; ns++; }
            else if (ns > 0) stn[ns - 1]++;
        }

        u8 used[16] = { 0 };
        for (int k = 0; k < ns; k++) {
            int best = -1, rev = 0;
            f32 bd = 1e30f;
            for (int j = 0; j < ns; j++) {
                if (used[j]) continue;
                v2 a = V2(g[st0[j]].x + ox, g[st0[j]].y);
                v2 b = V2(g[st0[j] + stn[j] - 1].x + ox, g[st0[j] + stn[j] - 1].y);
                f32 da = v2_dist(pen, a), db = v2_dist(pen, b);
                if (da < bd) { bd = da; best = j; rev = 0; }
                if (db < bd) { bd = db; best = j; rev = 1; }
            }
            used[best] = 1;

            for (int m = 0; m < stn[best] && n < cap; m++) {
                int src = rev ? st0[best] + stn[best] - 1 - m : st0[best] + m;
                pts[n] = V2(g[src].x + ox, g[src].y);
                isj[n] = (u8)(m == 0);    /* the edge arriving here was a jump */
                n++;
            }
            pen = pts[n - 1];
        }
    }
    if (n < 2) { free(pts); free(dur); free(isj); return NULL; }

    /* Durations. The edge leaving point i arrives at point i + 1, so it is a
     * jump when point i + 1 starts a stroke, and the edge closing the loop is
     * always one. A stroke of zero length is a dot, and a dot is the beam
     * resting there for a moment. */
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        f32 len = v2_dist(pts[i], pts[j]);
        if (isj[j] || j == 0) dur[i] = len / DM_MAX(jump, 1.0f);
        else                  dur[i] = len > 1e-4f ? len : 0.6f;
    }

    for (int i = 0; i < n; i++)
        pts[i] = V2((pts[i].x - wgrid * 0.5f) * sc, (pts[i].y - DM_GLYPH_H * 0.5f) * sc);

    xy_table *tb = resample(pts, dur, n, samples);
    if (tb) centre(tb);
    free(pts); free(dur); free(isj);
    return tb;
}

v2 xy_path_knot(f64 phase, f64 t, const void *ud)
{
    const xy_knot *k = (const xy_knot *)ud;
    f64 a  = (phase - floor(phase)) * 6.283185307179586;
    f32 rr = k->R + k->r * (f32)cos((f64)k->q * a);
    f32 x  = rr * (f32)cos((f64)k->p * a);
    f32 y  = rr * (f32)sin((f64)k->p * a);
    f32 z  = k->r * (f32)sin((f64)k->q * a);
    f32 th = k->tilt + k->tilt_rate * (f32)t;
    return V2(x, y * cosf(th) - z * sinf(th));
}

/* ---- a voice --------------------------------------------------------------- */

xy_voice xy_voice_default(void)
{
    xy_voice v;
    memset(&v, 0, sizeof v);
    v.path  = xy_path_circle;
    v.dir   = 1.0f;
    v.size  = 0.5f;
    v.amp_a = 0.005f; v.amp_d = 0.30f; v.amp_s = 1.0f; v.amp_r = 0.08f;
    v.flt_a = 0.002f; v.flt_d = 0.20f; v.flt_s = 0.3f; v.flt_r = 0.15f;
    v.resonance = 0.2f;
    return v;
}

void xy_note(dm_audio *bus, const xy_voice *v, f64 t0, f64 len, f32 hz, f32 vel)
{
    if (len <= 0.0 || hz <= 0.0f) return;

    f64 tail = (f64)DM_MAX(v->amp_r, 0.005f) * 6.0;
    int i0, i1;
    span(bus, t0, t0 + len + tail, &i0, &i1);
    if (i1 <= i0) return;

    int filtered = v->cutoff > 0.0f;
    dm_svf fx = { 0.0f, 0.0f }, fy = { 0.0f, 0.0f };
    dm_svf_coef coef = dm_svf_make(filtered ? v->cutoff : 1000.0f, v->resonance);

    f64 ph  = (f64)v->phase;
    f64 dir = v->dir < 0.0f ? -1.0 : 1.0;

    for (int i = i0; i < i1; i++) {
        f64 t   = (f64)(i - i0) / DM_SR;
        f32 amp = adsr(v->amp_a, v->amp_d, v->amp_s, v->amp_r, t, len);
        if (t > len && amp < 1e-5f) break;

        v2 p = v->path(dir * ph, t, v->ud);

        /* The glide is a square law that reaches zero at exactly `glide`, so
         * the figure stops turning on a definite frame. The bend is the same
         * law run the other way. */
        f64 f = hz;
        if (v->glide > 0.0f && t < v->glide) {
            f64 g = 1.0 - t / v->glide;
            f *= pow(2.0, -(f64)v->detune * g * g / 12.0);
        }
        if (v->bend_len > 0.0f && t > len - v->bend_len) {
            f64 g = DM_MIN(1.0, (t - (len - v->bend_len)) / v->bend_len);
            f *= pow(2.0, (f64)v->bend * g * g / 12.0);
        }
        ph += f / DM_SR;

        f32 ang = DM_TAU * (v->angle + v->spin * (f32)t);
        if (ang != 0.0f) {
            f32 c = cosf(ang), s = sinf(ang);
            p = V2(c * p.x - s * p.y, s * p.x + c * p.y);
        }
        p = v2_scl(p, v->size);

        p.x = shape_drive(p.x, v->drive);
        p.y = shape_drive(p.y, v->drive);

        if (filtered) {
            if (((i - i0) & 15) == 0) {
                f32 fe = adsr(v->flt_a, v->flt_d, v->flt_s, v->flt_r, t, len);
                coef = dm_svf_make(v->cutoff + v->env_amount * fe, v->resonance);
            }
            p.x = dm_svf_tick(&fx, p.x, &coef, DM_LP);
            p.y = dm_svf_tick(&fy, p.y, &coef, DM_LP);
        }

        f32 k = amp * vel;
        bus->l[i] += p.x * k;
        bus->r[i] += p.y * k;
    }
}

/* ---- percussion ------------------------------------------------------------ */

void xy_kick(dm_audio *bus, f64 t, f32 vel, f32 size, f32 decay)
{
    int i0, i1;
    span(bus, t, t + decay * 7.0, &i0, &i1);

    f64 ph = 0.0;
    for (int i = i0; i < i1; i++) {
        f32 tt = (f32)(i - i0) / (f32)DM_SR;

        /* dm_drum_kick's pitch curve, unchanged: it is what makes a sine read
         * as something being struck. There is no click on top of it. A click
         * is noise, and noise on this screen is a scribble across the one
         * clean shape a kick gets to make. */
        f32 hz = 44.0f + 148.0f * expf(-tt / 0.028f);
        ph += hz / (f64)DM_SR;

        f32 env = expf(-tt / decay) * (1.0f - expf(-tt / 0.0015f));
        f32 a   = (f32)((ph - floor(ph)) * 6.283185307179586);
        f32 r   = env * vel;

        /* Driven a little on each axis: the punch of the drum, and the reason
         * the spiral is not quite round. */
        bus->l[i] += size * shape_drive(r * cosf(a), 0.12f);
        bus->r[i] += size * shape_drive(r * sinf(a), 0.12f);
    }
}

void xy_spark(dm_audio *bus, f64 t, f32 vel, f32 radius, f32 hz, f32 fizz,
              f32 decay, u32 seed)
{
    int i0, i1;
    span(bus, t, t + decay * 7.0, &i0, &i1);

    /* The noise that moves the radius is kept below a few kilohertz, and
     * scaled up to match. Faster than that it no longer reads as a ring
     * shaking but as a ring crumpled, a hundred kinks a turn. */
    f32 bright = DM_MIN(hz * 14.0f, 9000.0f);
    dm_svf h = { 0, 0 }, l = { 0, 0 };
    dm_svf_coef hp = dm_svf_make(DM_MIN(hz * 1.5f, 1200.0f), 0.1f);
    dm_svf_coef lp = dm_svf_make(bright, 0.1f);
    f32 gain = sqrtf(9000.0f / bright);
    u32 s = seed * 2654435761u + 0x165667b1u;

    f64 ph = 0.0;
    for (int i = i0; i < i1; i++) {
        f32 tt  = (f32)(i - i0) / (f32)DM_SR;
        f32 env = expf(-tt / decay) * (1.0f - expf(-tt / 0.0006f));

        /* The tone drops a little as it is struck, like any drum head. */
        ph += (f64)hz * (1.0 + 0.45 * exp(-tt / 0.018)) / DM_SR;
        f32 a = (f32)((ph - floor(ph)) * 6.283185307179586);

        f32 n  = dm_svf_tick(&l, dm_svf_tick(&h, white(&s), &hp, DM_HP), &lp, DM_LP) * gain;
        f32 rr = radius * vel * env * (1.0f + fizz * n);
        bus->l[i] += rr * cosf(a);
        bus->r[i] += rr * sinf(a);
    }
}

/* ---- chaos ----------------------------------------------------------------- */

/* Measured, not derived: at sigma 10, rho 28, beta 8/3 the orbit around a wing
 * comes round 1.3315 times per unit of the system's own time, and z averages
 * 23.55. Scaling time by hz / 1.3315 therefore puts the wing at the pitch. */
#define LZ_ORBIT 1.3315
#define LZ_ZMEAN 23.55

static void lz_deriv(const f64 *s, f64 *d)
{
    d[0] = 10.0 * (s[1] - s[0]);
    d[1] = s[0] * (28.0 - s[2]) - s[1];
    d[2] = s[0] * s[1] - (8.0 / 3.0) * s[2];
}

static void lz_step(f64 *s, f64 dt)
{
    f64 k1[3], k2[3], k3[3], k4[3], q[3];
    lz_deriv(s, k1);
    for (int i = 0; i < 3; i++) q[i] = s[i] + 0.5 * dt * k1[i];
    lz_deriv(q, k2);
    for (int i = 0; i < 3; i++) q[i] = s[i] + 0.5 * dt * k2[i];
    lz_deriv(q, k3);
    for (int i = 0; i < 3; i++) q[i] = s[i] + dt * k3[i];
    lz_deriv(q, k4);
    for (int i = 0; i < 3; i++)
        s[i] += dt / 6.0 * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);
}

void xy_lorenz(dm_audio *bus, f64 t0, f64 len, const xy_lorenz_part *p)
{
    int i0, i1;
    span(bus, t0, t0 + len, &i0, &i1);
    if (i1 <= i0 || p->nsteps < 1) return;

    /* Start on the attractor rather than falling onto it: a few hundred units
     * of the system's time from an arbitrary point is plenty. */
    f64 s[3] = { 1.0, 1.0, 20.0 };
    for (int k = 0; k < 40000; k++) lz_step(s, 0.005);

    f64 f   = p->hz[0];
    f64 sc  = p->size / 24.0;       /* one scale for both axes: the true shape */
    f64 rel = len - 0.03;

    for (int i = i0; i < i1; i++) {
        f64 tt = (f64)(i - i0) / DM_SR;
        int st = (int)(tt / p->step);
        if (st >= p->nsteps) st = p->nsteps - 1;

        /* Portamento over a few milliseconds, so a change of note is a bend
         * in the orbit and not a kink. */
        f += ((f64)p->hz[st] - f) * (1.0 - exp(-1.0 / (0.012 * DM_SR)));
        lz_step(s, f / LZ_ORBIT / DM_SR);

        f32 env = dm_sat((f32)(tt / 0.006));
        if (tt > rel) env *= dm_sat((f32)((len - tt) / 0.03));

        /* Struck on every step and falling away until the next. The floor
         * keeps the attractor alive between strikes: a Lorenz system that is
         * silent is a dot, and a dot in the middle of a bar reads as a fault. */
        f64 into = tt - floor(tt / p->strike) * p->strike;
        f32 hit  = dm_sat((f32)(into / 0.004));
        env *= (1.0f - p->pump) + p->pump * hit * expf(-(f32)(into / (p->strike * 0.45)));
        env *= dm_lerp(p->vel0, p->vel1, (f32)(tt / len));

        bus->l[i] += (f32)(s[0] * sc) * env;
        bus->r[i] += (f32)((s[2] - LZ_ZMEAN) * sc) * env;
    }
}
