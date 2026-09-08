/* flight.c -- one integration, sampled by everything else.
 *
 * A two-degree-of-freedom launch in the plane of the trajectory. The only
 * liberty is that the model is written flat with the curvature terms added
 * back in -- `vx*vx/r` holding the vehicle up as it goes sideways, `vx*vy/r`
 * rotating the local vertical under it -- rather than being written in polar
 * coordinates. That is the usual way to do it and it is exact for an ascent
 * in one plane, which is what this is.
 */
#include "flight.h"

#include <stdlib.h>
#include <string.h>

/* ---- constants ----------------------------------------------------------- */

#define G0        9.80665
#define MU        3.986004418e14      /* Earth GM, m^3/s^2        */
#define RE        6371000.0           /* mean radius, m           */
#define P0        101325.0            /* sea level pressure, Pa   */
#define RHO0      1.225               /* sea level density        */
#define H_SCALE   8500.0              /* atmospheric scale height */

#define M_LIFTOFF     549000.0        /* fuelled, on the pad      */
#define M_STAGE1_DRY   25600.0        /* first stage, empty       */

#define S1_MDOT         2741.0        /* kg/s at full throttle       */
#define S1_THRUST_VAC 8387000.0       /* N                           */
#define S1_AEXIT           7.70       /* m^2, so sea level = 7607 kN */

#define S2_MDOT          287.4
#define S2_THRUST      981000.0

#define A_REF           10.52         /* pi * 1.83^2, m^2           */
#define A_LIMIT     (4.0 * G0)        /* structural / payload limit */

/* The pitch kick, and the angle the upper stage is steering to. These four
 * numbers are the only steering in the whole file; everything else follows. */
#define KICK_T0         13.0          /* the kick starts, seconds                  */
#define KICK_T1         19.0          /* and ends                                  */
#define KICK_ANGLE      0.110         /* 6.3 degrees                               */
#define TAN_START       0.300         /* tan of the upper stage thrust angle above */
#define TAN_FINAL     (-0.020)        /* the horizontal, at ignition and at SECO   */

#define T_FIRST     (-5.0)
#define T_LAST      545.0
#define T_STEP      0.02
#define N_SAMPLES   ((int)((T_LAST - T_FIRST) / T_STEP) + 2)

#define ALT_STEP     50.0f            /* inverse table resolution, metres */
#define N_ALT        4200             /* to 210 km                        */

static lo_state *g_tab     = NULL;
static f32      *g_t_alt   = NULL;    /* mission time at each altitude    */
static f32      *g_dr_alt  = NULL;    /* and downrange, for the same       */
static f64       g_maxq_t  = 0.0;
static f32       g_maxq_pa = 0.0f;

/* ---- atmosphere ---------------------------------------------------------- */

/* Isothermal, which is wrong in detail and right where it matters: it puts
 * virtually all of the mass below 60 km, so drag dies away at the altitude it
 * really does. */
f32 lo_air_density(f32 alt)
{
    if (alt < 0.0f) alt = 0.0f;
    return (f32)(RHO0 * exp(-(f64)alt / H_SCALE));
}

static f64 air_pressure(f64 alt)
{
    return alt < 0.0 ? P0 : P0 * exp(-alt / H_SCALE);
}

/* Standard lapse rate to the tropopause, constant above it. This only feeds a
 * drag coefficient lookup, so it is plenty. */
static f64 sound_speed(f64 alt)
{
    f64 T = alt < 11000.0 ? 288.15 - 0.0065 * alt : 216.65;
    return 20.046 * sqrt(T);
}

/* Drag coefficient against Mach number. The hump either side of Mach one is
 * the wave drag rise, and it is why maximum dynamic pressure happens where it
 * does instead of at the altitude of highest speed. */
static f64 drag_coefficient(f64 mach)
{
    static const f64 M[]  = { 0.00, 0.60, 0.90, 1.10, 1.30, 2.00, 3.00, 5.00, 8.00 };
    static const f64 CD[] = { 0.28, 0.30, 0.40, 0.62, 0.58, 0.42, 0.32, 0.26, 0.24 };
    const int n = (int)(sizeof M / sizeof M[0]);

    if (mach <= M[0])     return CD[0];
    if (mach >= M[n - 1]) return CD[n - 1];
    for (int i = 1; i < n; i++)
        if (mach <= M[i]) {
            f64 u = (mach - M[i - 1]) / (M[i] - M[i - 1]);
            return CD[i - 1] + (CD[i] - CD[i - 1]) * u;
        }
    return CD[n - 1];
}

/* ---- the throttle program ------------------------------------------------ */

/* Three things move the first stage throttle, and all three are real: the
 * engines take a moment to come up, the vehicle throttles down through the
 * transonic region to keep the airframe loads sane, and late in the burn it
 * throttles again because an almost empty stage under full thrust would pull
 * far more than four g. Only the first two are scheduled; the third is a
 * consequence and starts biting whenever the tanks happen to be empty enough. */
static f64 throttle_at(f64 t, f64 mass, f64 thrust_full)
{
    f64 th;

    if (t < LO_T_IGNITION) return 0.0;
    if (t > LO_T_MECO)     th = 0.0;
    else if (t < LO_T_LIFTOFF)
        th = (t - LO_T_IGNITION) / (LO_T_LIFTOFF - LO_T_IGNITION - 0.4);
    else
        th = 1.0;

    if (th > 1.0) th = 1.0;

    f64 dip = 0.0;
    if (t > 52.0 && t < 88.0) {
        f64 a = (t - 52.0) / 5.0, b = (88.0 - t) / 5.0;
        a = a < 0.0 ? 0.0 : (a > 1.0 ? 1.0 : a);
        b = b < 0.0 ? 0.0 : (b > 1.0 ? 1.0 : b);
        dip = a * b;
    }
    th *= 1.0 - 0.26 * dip;

    if (th > 0.0 && thrust_full > 0.0) {
        f64 cap = A_LIMIT * mass / thrust_full;
        if (cap < th) th = cap;
    }
    return th < 0.0 ? 0.0 : th;
}

/* ---- the integration ----------------------------------------------------- */

void lo_flight_init(void)
{
    if (g_tab) return;
    g_tab = (lo_state *)malloc(sizeof(lo_state) * (size_t)N_SAMPLES);
    if (!g_tab) return;

    const f64 dt = 0.002;

    f64 t  = T_FIRST;
    f64 y  = 0.0, x  = 0.0;
    f64 vy = 0.0, vx = 0.0;
    f64 m  = M_LIFTOFF;

    int idx  = 0;
    f64 next = T_FIRST;

    while (t <= T_LAST + dt) {
        f64 r    = RE + y;
        f64 g    = MU / (r * r);
        f64 rho  = y < 0.0 ? RHO0 : RHO0 * exp(-y / H_SCALE);
        f64 pa   = air_pressure(y);
        f64 v    = sqrt(vx * vx + vy * vy);
        f64 mach = v / sound_speed(y);

        int stage = t >= LO_T_SEP ? 2 : 1;

        /* The first stage gains most of a meganewton on the way out purely
         * because there is less air pushing back into the nozzles. That is the
         * `- Aexit * pa` term, and it is why the acceleration limiter engages
         * when it does rather than when it was told to. */
        f64 thrust_full, mdot_full, th;
        if (stage == 1) {
            thrust_full = S1_THRUST_VAC - S1_AEXIT * pa;
            mdot_full   = S1_MDOT;
            th          = throttle_at(t, m, thrust_full);
        } else {
            thrust_full = (t >= LO_T_S2_START && t < LO_T_SECO) ? S2_THRUST : 0.0;
            mdot_full   = S2_MDOT;
            th          = thrust_full > 0.0 ? 1.0 : 0.0;
        }

        f64 thrust = thrust_full * th;
        f64 mdot   = mdot_full   * th;

        /* Attitude. Vertical off the pad, one small pitch kick, and after that
         * the thrust simply follows the velocity vector: a gravity turn, where
         * the trajectory is bent by weight rather than by steering.
         *
         * The kick is six and a third degrees, and it is worth being precise
         * about why that number and not another. A gravity turn amplifies
         * whatever it is given: the rate the velocity vector rotates is
         * g*sin(pitch)/v, so a degree put in at fifty metres a second is still
         * being paid for at Mach eight. Measured on this model, one degree of
         * kick is worth about seven kilometres of cut-off altitude -- four and
         * a half degrees puts MECO at 94 km and eight degrees puts it at 70. */
        f64 pitch;
        if (t < KICK_T0) {
            pitch = 0.0;
        } else if (t < KICK_T1) {
            pitch = KICK_ANGLE * ((t - KICK_T0) / (KICK_T1 - KICK_T0));
        } else if (stage == 1) {
            pitch = atan2(vx, vy > 1.0 ? vy : 1.0);
        } else {
            /* An upper stage cannot simply follow its velocity; that ends in
             * the atmosphere on the far side of the planet. It flies linear
             * tangent steering instead: the tangent of the thrust angle above
             * the local horizontal falls linearly with time.
             *
             * That one line is not a convenience. It is the optimal steering
             * law for constant thrust in a uniform field -- it falls out of
             * the calculus of variations -- and it is close enough to optimal
             * for a real ascent that upper stages genuinely fly it. There is
             * no feedback in it at all. Both ends of the tangent are constants
             * chosen for the target orbit, and the trajectory that comes out
             * is whatever the physics makes of those two numbers. An earlier
             * version steered on altitude error instead and oscillated: it
             * lofted to 260 km, dived back to 190, and arrived a kilometre a
             * second short. Feedback was the problem, not the fix. */
            f64 B = (TAN_START - TAN_FINAL) / (LO_T_SECO - LO_T_S2_START);
            f64 theta = atan(TAN_START - B * (t - LO_T_S2_START));
            pitch = 1.5707963 - theta;
            if (pitch > 1.545) pitch = 1.545;
            if (pitch < 0.05)  pitch = 0.05;
        }

        f64 cd   = drag_coefficient(mach);
        f64 q    = 0.5 * rho * v * v;
        f64 drag = q * cd * A_REF;

        f64 ax = 0.0, ay = 0.0;
        if (v > 1e-6) {
            ax -= drag * (vx / v) / m;
            ay -= drag * (vy / v) / m;
        }
        ax += thrust * sin(pitch) / m;
        ay += thrust * cos(pitch) / m;
        ay -= g;

        /* Curvature. Without these two terms nothing ever reaches orbit; it
         * only gets very fast and then comes down again. */
        ay += vx * vx / r;
        ax -= vx * vy / r;

        /* Held down until release. The engines are lit and the vehicle is not
         * going anywhere, which is exactly what the last three seconds of a
         * count look like. */
        if (t < LO_T_LIFTOFF) { ax = 0.0; ay = 0.0; vx = 0.0; vy = 0.0; }

        while (next <= t && idx < N_SAMPLES) {
            lo_state *s = &g_tab[idx++];
            s->t         = next;
            s->alt       = (f32)y;
            s->downrange = (f32)x;
            s->vy        = (f32)vy;
            s->vx        = (f32)vx;
            s->vel       = (f32)v;
            s->pitch     = (f32)pitch;
            s->mass      = (f32)m;
            s->q         = (f32)q;
            s->mach      = (f32)mach;
            s->accel     = (f32)((thrust - drag) / m);
            s->throttle  = (f32)th;
            s->stage     = stage;
            next += T_STEP;
        }

        if (q > (f64)g_maxq_pa && t > 0.0 && t < LO_T_MECO) {
            g_maxq_pa = (f32)q;
            g_maxq_t  = t;
        }

        vx += ax * dt;
        vy += ay * dt;
        x  += vx * dt;
        y  += vy * dt;
        if (y < 0.0) { y = 0.0; if (vy < 0.0) vy = 0.0; }

        m -= mdot * dt;

        /* Separation: the empty first stage and the interstage leave. */
        if (t < LO_T_SEP && t + dt >= LO_T_SEP) {
            m -= M_STAGE1_DRY;
        }

        t += dt;
    }

    while (idx < N_SAMPLES) { g_tab[idx] = g_tab[idx - 1]; idx++; }

    /* And the same trajectory read the other way round. Altitude is monotone
     * until the coast at the top of the second stage burn, which is well past
     * anything that asks. */
    g_t_alt  = (f32 *)malloc(sizeof(f32) * (size_t)N_ALT);
    g_dr_alt = (f32 *)malloc(sizeof(f32) * (size_t)N_ALT);
    if (!g_t_alt || !g_dr_alt) return;

    int k = 0;
    for (int i = 0; i < N_SAMPLES && k < N_ALT; i++) {
        while (k < N_ALT && (f32)k * ALT_STEP <= g_tab[i].alt) {
            g_dr_alt[k]  = g_tab[i].downrange;
            g_t_alt[k++] = (f32)g_tab[i].t;
        }
    }
    while (k < N_ALT) {
        g_t_alt[k]  = (f32)T_LAST;
        g_dr_alt[k] = g_tab[N_SAMPLES - 1].downrange;
        k++;
    }
}

f32 lo_flight_downrange_of_alt(f32 alt)
{
    if (!g_dr_alt || alt <= 0.0f) return 0.0f;
    f32 u = alt / ALT_STEP;
    int i = (int)u;
    if (i >= N_ALT - 1) return g_dr_alt[N_ALT - 1];
    return dm_lerp(g_dr_alt[i], g_dr_alt[i + 1], u - (f32)i);
}

f64 lo_flight_time_of_alt(f32 alt)
{
    if (!g_t_alt || alt <= 0.0f) return LO_T_LIFTOFF;
    f32 u = alt / ALT_STEP;
    int i = (int)u;
    if (i >= N_ALT - 1) return (f64)g_t_alt[N_ALT - 1];
    return (f64)dm_lerp(g_t_alt[i], g_t_alt[i + 1], u - (f32)i);
}

lo_state lo_flight_at(f64 mission_t)
{
    if (!g_tab) { lo_state z; memset(&z, 0, sizeof z); z.stage = 1; return z; }

    f64 u = (mission_t - T_FIRST) / T_STEP;
    if (u < 0.0) u = 0.0;
    if (u > (f64)(N_SAMPLES - 2)) u = (f64)(N_SAMPLES - 2);

    int i = (int)u;
    f32 f = (f32)(u - (f64)i);

    const lo_state *a = &g_tab[i], *b = &g_tab[i + 1];
    lo_state s;
    s.t         = mission_t;
    s.alt       = dm_lerp(a->alt,       b->alt,       f);
    s.downrange = dm_lerp(a->downrange, b->downrange, f);
    s.vy        = dm_lerp(a->vy,        b->vy,        f);
    s.vx        = dm_lerp(a->vx,        b->vx,        f);
    s.vel       = dm_lerp(a->vel,       b->vel,       f);
    s.pitch     = dm_lerp(a->pitch,     b->pitch,     f);
    s.mass      = dm_lerp(a->mass,      b->mass,      f);
    s.q         = dm_lerp(a->q,         b->q,         f);
    s.mach      = dm_lerp(a->mach,      b->mach,      f);
    s.accel     = dm_lerp(a->accel,     b->accel,     f);
    s.throttle  = dm_lerp(a->throttle,  b->throttle,  f);
    s.stage     = f < 0.5f ? a->stage : b->stage;
    return s;
}

f64 lo_flight_maxq(void)    { return g_maxq_t; }
f32 lo_flight_maxq_pa(void) { return g_maxq_pa; }
