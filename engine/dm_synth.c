#include "dm_synth.h"
#include "dm_rand.h"

#include <stdlib.h>
#include <string.h>

/* ---- oscillators -------------------------------------------------------- */

/* PolyBLEP: adds a two-sample polynomial correction around each discontinuity
 * of a naive saw or square. It does not remove aliasing entirely, but it kills
 * the loud low-order images, which is the difference between a saw that sounds
 * like an analogue synth and one that sounds like a broken CD. */
static inline f32 polyblep(f32 t, f32 dt)
{
    if (t < dt)            { t /= dt;             return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt)     { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

static inline f32 osc_value(dm_osc_kind k, f32 ph, f32 dt, f32 pw, u32 *nseed)
{
    switch (k) {
    case DM_OSC_SAW:
        return 2.0f * ph - 1.0f - polyblep(ph, dt);
    case DM_OSC_SQUARE: {
        f32 v  = ph < pw ? 1.0f : -1.0f;
        f32 p2 = ph + 1.0f - pw;
        if (p2 >= 1.0f) p2 -= 1.0f;
        return v + polyblep(ph, dt) - polyblep(p2, dt);
    }
    case DM_OSC_TRI:
        return 4.0f * fabsf(ph - 0.5f) - 1.0f;
    case DM_OSC_SINE:
        return sinf(ph * DM_TAU);
    default:
        *nseed = dm_hash_u32(*nseed);
        return dm_u32_to_f32(*nseed) * 2.0f - 1.0f;
    }
}

/* ---- state variable filter (topology preserving transform) -------------- */

/* Zavalishin's TPT form. Unlike the classic Chamberlin SVF it stays stable and
 * keeps its tuning all the way up to Nyquist, which matters because the filter
 * envelopes here sweep several octaves in a few milliseconds. */
typedef struct { f32 ic1, ic2; } svf;

typedef struct { f32 a1, a2, a3, k; } svf_coef;

static svf_coef svf_make(f32 cutoff_hz, f32 resonance)
{
    f32 fc = dm_clamp(cutoff_hz, 20.0f, DM_SR * 0.45f);
    f32 g  = tanf(DM_PI * fc / (f32)DM_SR);
    f32 k  = 2.0f - 1.96f * dm_clamp(resonance, 0.0f, 0.98f);
    svf_coef c;
    c.k  = k;
    c.a1 = 1.0f / (1.0f + g * (g + k));
    c.a2 = g * c.a1;
    c.a3 = g * c.a2;
    return c;
}

static inline f32 svf_tick(svf *s, f32 in, const svf_coef *c, dm_filter_mode mode)
{
    f32 v3 = in - s->ic2;
    f32 v1 = c->a1 * s->ic1 + c->a2 * v3;
    f32 v2 = s->ic2 + c->a2 * s->ic1 + c->a3 * v3;
    s->ic1 = 2.0f * v1 - s->ic1;
    s->ic2 = 2.0f * v2 - s->ic2;
    switch (mode) {
    case DM_LP: return v2;
    case DM_BP: return v1;
    default:    return in - c->k * v1 - v2;
    }
}

/* ---- envelopes ---------------------------------------------------------- */

/* Linear attack, exponential decay to a sustain level, exponential release
 * from wherever the envelope actually was at note-off. */
static f32 adsr_at(f32 A, f32 D, f32 S, f32 R, f64 t, f64 len)
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

static inline f32 saturate(f32 x, f32 drive)
{
    if (drive <= 0.0f) return x;
    return tanhf(x * (1.0f + drive * 5.0f)) / (1.0f + drive * 1.2f);
}

/* ---- patches ------------------------------------------------------------ */

dm_patch dm_patch_default(void)
{
    dm_patch p;
    p.kind = DM_OSC_SAW;
    p.unison = 1;
    p.detune_cents = 0.0f;
    p.pulse_width  = 0.5f;
    p.sub_level    = 0.0f;

    p.amp_a = 0.005f; p.amp_d = 0.25f; p.amp_s = 0.7f; p.amp_r = 0.15f;
    p.flt_a = 0.002f; p.flt_d = 0.20f; p.flt_s = 0.3f; p.flt_r = 0.15f;

    p.mode        = DM_LP;
    p.cutoff      = 700.0f;
    p.env_amount  = 3000.0f;
    p.key_track   = 0.0f;
    p.resonance   = 0.3f;

    p.drive  = 0.0f;
    p.gain   = 0.3f;
    p.pan    = 0.0f;
    p.spread = 0.0f;
    p.vib_rate = 5.0f;
    p.vib_depth = 0.0f;
    p.send   = 0.0f;
    return p;
}

/* ---- voice -------------------------------------------------------------- */

void dm_synth_note(dm_audio *bus, dm_audio *send, const dm_patch *p,
                   f64 t_start, f64 t_len, f32 midi, f32 velocity, u32 seed)
{
    if (t_len <= 0.0) return;

    f64 tail = (f64)DM_MAX(p->amp_r, 0.005f) * 6.0;
    int i0 = dm_sec_to_frames(t_start);
    int i1 = dm_sec_to_frames(t_start + t_len + tail);
    if (i0 < 0) i0 = 0;
    if (i1 > bus->n) i1 = bus->n;
    if (i1 <= i0) return;

    int nu = p->unison < 1 ? 1 : (p->unison > 8 ? 8 : p->unison);
    f32 base = dm_midi_hz(midi);

    f32 ph[8], det[8], panl[8], panr[8];
    dm_rng rng;
    dm_rng_seed(&rng, (u64)seed * 2654435761u + 17u);

    for (int u = 0; u < nu; u++) {
        /* Random start phases stop the unison stack from summing into one
         * huge transient click on every note. */
        ph[u] = dm_rng_f(&rng);

        f32 pos = nu > 1 ? ((f32)u / (f32)(nu - 1)) * 2.0f - 1.0f : 0.0f;
        det[u] = powf(2.0f, pos * p->detune_cents / 1200.0f);

        f32 pan = dm_clamp(p->pan + pos * p->spread, -1.0f, 1.0f);
        f32 ang = (pan * 0.5f + 0.5f) * DM_HALFPI;   /* equal power */
        panl[u] = cosf(ang);
        panr[u] = sinf(ang);
    }

    f32 norm  = 1.0f / sqrtf((f32)nu);
    f32 subph = 0.0f;
    u32 nseed = seed ^ 0x9e3779b9u;

    svf fl = { 0.0f, 0.0f }, fr = { 0.0f, 0.0f };
    svf_coef coef = svf_make(p->cutoff, p->resonance);
    f32 keyhz = (base - 261.63f) * p->key_track;

    for (int i = i0; i < i1; i++) {
        f64 t   = (f64)(i - i0) / DM_SR;
        f32 amp = adsr_at(p->amp_a, p->amp_d, p->amp_s, p->amp_r, t, t_len);
        if (t > t_len && amp < 1e-5f) break;

        /* Filter coefficients move at control rate. tanf per sample would cost
         * more than the entire oscillator stack, and nobody can hear 1 ms of
         * staircase in a cutoff sweep. */
        if (((i - i0) & 15) == 0) {
            f32 fenv = adsr_at(p->flt_a, p->flt_d, p->flt_s, p->flt_r, t, t_len);
            coef = svf_make(p->cutoff + p->env_amount * fenv + keyhz, p->resonance);
        }

        f32 fmul = 1.0f;
        if (p->vib_depth > 0.0f)
            fmul = powf(2.0f, sinf((f32)t * DM_TAU * p->vib_rate) * p->vib_depth / 12.0f);

        f32 sl = 0.0f, sr = 0.0f;
        for (int u = 0; u < nu; u++) {
            f32 f  = base * det[u] * fmul;
            f32 dt = f / (f32)DM_SR;
            ph[u] += dt;
            if (ph[u] >= 1.0f) ph[u] -= 1.0f;
            f32 v = osc_value(p->kind, ph[u], dt, p->pulse_width, &nseed);
            sl += v * panl[u];
            sr += v * panr[u];
        }
        sl *= norm;
        sr *= norm;

        if (p->sub_level > 0.0f) {
            subph += base * 0.5f / (f32)DM_SR;
            if (subph >= 1.0f) subph -= 1.0f;
            f32 s = sinf(subph * DM_TAU) * p->sub_level;
            sl += s;
            sr += s;
        }

        sl = saturate(sl, p->drive);
        sr = saturate(sr, p->drive);

        f32 g  = amp * velocity * p->gain;
        f32 ol = svf_tick(&fl, sl, &coef, p->mode) * g;
        f32 orr = svf_tick(&fr, sr, &coef, p->mode) * g;

        bus->l[i] += ol;
        bus->r[i] += orr;
        if (send && p->send > 0.0f) {
            send->l[i] += ol * p->send;
            send->r[i] += orr * p->send;
        }
    }
}

/* ---- drums -------------------------------------------------------------- */

static void emit(dm_audio *bus, dm_audio *send, int i, f32 v, f32 send_amt)
{
    bus->l[i] += v;
    bus->r[i] += v;
    if (send && send_amt > 0.0f) {
        send->l[i] += v * send_amt;
        send->r[i] += v * send_amt;
    }
}

void dm_drum_kick(dm_audio *bus, dm_audio *send, f64 t, f32 vel, f32 send_amt)
{
    int i0 = dm_sec_to_frames(t), i1 = dm_sec_to_frames(t + 0.75);
    if (i0 < 0) i0 = 0;
    if (i1 > bus->n) i1 = bus->n;

    u32 s  = 0x9d2c5680u;
    f32 ph = 0.0f;

    for (int i = i0; i < i1; i++) {
        f32 tt = (f32)(i - i0) / (f32)DM_SR;

        /* The pitch falling from 190 Hz to 44 Hz in about 30 ms is the whole
         * trick: it is what makes a sine wave read as a drum being struck. */
        f32 hz = 44.0f + 148.0f * expf(-tt / 0.028f);
        ph += hz / (f32)DM_SR;
        if (ph >= 1.0f) ph -= 1.0f;

        f32 body = sinf(ph * DM_TAU);
        f32 env  = expf(-tt / 0.20f) * (1.0f - expf(-tt / 0.0015f));
        s = dm_hash_u32(s);
        f32 click = (dm_u32_to_f32(s) * 2.0f - 1.0f) * expf(-tt / 0.0035f) * 0.5f;

        emit(bus, send, i, tanhf((body * env * 1.75f + click) * 1.45f) * vel * 0.85f, send_amt);
    }
}

void dm_drum_snare(dm_audio *bus, dm_audio *send, f64 t, f32 vel, f32 send_amt)
{
    int i0 = dm_sec_to_frames(t), i1 = dm_sec_to_frames(t + 0.45);
    if (i0 < 0) i0 = 0;
    if (i1 > bus->n) i1 = bus->n;

    u32 s = 0x27d4eb2fu;
    svf bp = { 0, 0 };
    svf_coef c = svf_make(1850.0f, 0.55f);
    f32 ph = 0.0f;

    for (int i = i0; i < i1; i++) {
        f32 tt = (f32)(i - i0) / (f32)DM_SR;
        s = dm_hash_u32(s);
        f32 n = dm_u32_to_f32(s) * 2.0f - 1.0f;

        f32 noise = svf_tick(&bp, n, &c, DM_BP) * expf(-tt / 0.085f);

        /* Two detuned tones under the noise give the shell its pitch. */
        ph += 188.0f / (f32)DM_SR;
        if (ph >= 1.0f) ph -= 1.0f;
        f32 tone = (sinf(ph * DM_TAU) + 0.6f * sinf(ph * DM_TAU * 1.48f))
                   * expf(-tt / 0.055f) * 0.45f;

        f32 att = 1.0f - expf(-tt / 0.0008f);
        emit(bus, send, i, tanhf((noise * 1.4f + tone) * 1.2f) * att * vel * 0.5f, send_amt);
    }
}

void dm_drum_clap(dm_audio *bus, dm_audio *send, f64 t, f32 vel, f32 send_amt)
{
    int i0 = dm_sec_to_frames(t), i1 = dm_sec_to_frames(t + 0.42);
    if (i0 < 0) i0 = 0;
    if (i1 > bus->n) i1 = bus->n;

    u32 s = 0x165667b1u;
    svf bp = { 0, 0 };
    svf_coef c = svf_make(1450.0f, 0.42f);

    /* Four bursts a few milliseconds apart, then a longer tail. A single burst
     * sounds like a snare; the stutter is what makes it a room full of hands. */
    static const f32 taps[4] = { 0.0f, 0.010f, 0.019f, 0.027f };

    for (int i = i0; i < i1; i++) {
        f32 tt = (f32)(i - i0) / (f32)DM_SR;
        f32 env = 0.0f;
        for (int k = 0; k < 4; k++)
            if (tt >= taps[k]) env += expf(-(tt - taps[k]) / 0.0075f);
        if (tt >= taps[3]) env += expf(-(tt - taps[3]) / 0.16f) * 0.55f;

        s = dm_hash_u32(s);
        f32 n = dm_u32_to_f32(s) * 2.0f - 1.0f;
        emit(bus, send, i, svf_tick(&bp, n, &c, DM_BP) * env * vel * 0.4f, send_amt);
    }
}

void dm_drum_hat(dm_audio *bus, dm_audio *send, f64 t, f32 vel, f32 open, f32 send_amt)
{
    f32 decay = dm_lerp(0.032f, 0.30f, dm_sat(open));
    int i0 = dm_sec_to_frames(t), i1 = dm_sec_to_frames(t + decay * 6.0f);
    if (i0 < 0) i0 = 0;
    if (i1 > bus->n) i1 = bus->n;

    u32 s = 0x2545f491u;
    svf hp = { 0, 0 }, lp = { 0, 0 };
    svf_coef chp = svf_make(7600.0f, 0.15f);
    /* A real cymbal has almost nothing left above 13 kHz. Without this cap the
     * high-pass passes noise flat to Nyquist and the whole mix hisses. */
    svf_coef clp = svf_make(12500.0f, 0.0f);

    for (int i = i0; i < i1; i++) {
        f32 tt = (f32)(i - i0) / (f32)DM_SR;
        s = dm_hash_u32(s);
        f32 n = dm_u32_to_f32(s) * 2.0f - 1.0f;
        f32 v = svf_tick(&lp, svf_tick(&hp, n, &chp, DM_HP), &clp, DM_LP)
                * expf(-tt / decay) * vel * 0.30f;
        /* Hats sit slightly off centre so they do not fight the kick. */
        bus->l[i] += v * 0.9f;
        bus->r[i] += v * 1.1f;
        if (send && send_amt > 0.0f) { send->l[i] += v * send_amt; send->r[i] += v * send_amt; }
    }
}

void dm_drum_tom(dm_audio *bus, dm_audio *send, f64 t, f32 vel, f32 hz, f32 send_amt)
{
    int i0 = dm_sec_to_frames(t), i1 = dm_sec_to_frames(t + 0.6);
    if (i0 < 0) i0 = 0;
    if (i1 > bus->n) i1 = bus->n;

    f32 ph = 0.0f;
    u32 s = 0x7f4a7c15u;

    for (int i = i0; i < i1; i++) {
        f32 tt = (f32)(i - i0) / (f32)DM_SR;
        f32 f  = hz * (1.0f + 0.55f * expf(-tt / 0.045f));
        ph += f / (f32)DM_SR;
        if (ph >= 1.0f) ph -= 1.0f;
        s = dm_hash_u32(s);
        f32 n = (dm_u32_to_f32(s) * 2.0f - 1.0f) * expf(-tt / 0.006f) * 0.25f;
        f32 env = expf(-tt / 0.16f) * (1.0f - expf(-tt / 0.001f));
        emit(bus, send, i, tanhf((sinf(ph * DM_TAU) * env + n) * 1.3f) * vel * 0.55f, send_amt);
    }
}

/* ---- transition effects -------------------------------------------------- */

void dm_fx_sweep(dm_audio *bus, f64 t, f64 len, f32 vel, int rising)
{
    int i0 = dm_sec_to_frames(t), i1 = dm_sec_to_frames(t + len);
    if (i0 < 0) i0 = 0;
    if (i1 > bus->n) i1 = bus->n;
    if (i1 <= i0) return;

    u32 s = 0xb5297a4du;
    svf fl = { 0, 0 }, fr = { 0, 0 };
    svf_coef c = svf_make(500.0f, 0.7f);

    for (int i = i0; i < i1; i++) {
        f32 u = (f32)(i - i0) / (f32)(i1 - i0);
        if (((i - i0) & 31) == 0) {
            /* Sweeping in octaves rather than hertz is what makes the motion
             * sound linear to the ear. */
            f32 oct = rising ? dm_lerp(4.6f, 12.6f, u) : dm_lerp(12.6f, 4.6f, u);
            c = svf_make(powf(2.0f, oct), 0.72f);
        }
        s = dm_hash_u32(s);
        f32 nl = dm_u32_to_f32(s) * 2.0f - 1.0f;
        s = dm_hash_u32(s);
        f32 nr = dm_u32_to_f32(s) * 2.0f - 1.0f;

        f32 env = sinf(u * DM_PI);   /* fades in and out, no edges */
        bus->l[i] += svf_tick(&fl, nl, &c, DM_BP) * env * vel * 0.35f;
        bus->r[i] += svf_tick(&fr, nr, &c, DM_BP) * env * vel * 0.35f;
    }
}

void dm_fx_riser(dm_audio *bus, f64 t_land, f64 len, f32 vel)
{
    f64 t0 = t_land - len;
    int i0 = dm_sec_to_frames(t0), i1 = dm_sec_to_frames(t_land);
    if (i0 < 0) i0 = 0;
    if (i1 > bus->n) i1 = bus->n;
    if (i1 <= i0) return;

    u32 s = 0x68e31da4u;
    svf fl = { 0, 0 }, fr = { 0, 0 };
    svf_coef c = svf_make(300.0f, 0.9f);

    for (int i = i0; i < i1; i++) {
        f32 u = (f32)(i - i0) / (f32)(i1 - i0);
        if (((i - i0) & 31) == 0)
            c = svf_make(powf(2.0f, dm_lerp(5.5f, 13.2f, u * u)), 0.93f);

        s = dm_hash_u32(s);
        f32 nl = dm_u32_to_f32(s) * 2.0f - 1.0f;
        s = dm_hash_u32(s);
        f32 nr = dm_u32_to_f32(s) * 2.0f - 1.0f;

        f32 env = u * u * vel * 0.5f;
        bus->l[i] += svf_tick(&fl, nl, &c, DM_BP) * env;
        bus->r[i] += svf_tick(&fr, nr, &c, DM_BP) * env;
    }
}

/* ---- reverb (Schroeder comb + allpass network) --------------------------- */

/* Eight parallel comb filters build the density, four series allpasses smear
 * the remaining metallic ring. The delay lengths are mutually prime so their
 * echoes never line up into an audible pitch; the right channel is offset by a
 * further prime so the two sides decorrelate into a stereo image. */
#define NCOMB 8
#define NALLP 4

static const int COMB_LEN[NCOMB] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
static const int ALLP_LEN[NALLP] = { 556, 441, 341, 225 };
#define REV_SPREAD 23

typedef struct { f32 *buf; int len, idx; f32 store; } comb;
typedef struct { f32 *buf; int len, idx; } allp;

static int scale_len(int l, int extra)
{
    return (int)((f64)(l + extra) * (f64)DM_SR / 44100.0);
}

static f32 comb_tick(comb *c, f32 in, f32 feedback, f32 damp)
{
    f32 out = c->buf[c->idx];
    c->store = out * (1.0f - damp) + c->store * damp;
    c->buf[c->idx] = in + c->store * feedback;
    if (++c->idx >= c->len) c->idx = 0;
    return out;
}

static f32 allp_tick(allp *a, f32 in)
{
    f32 buf = a->buf[a->idx];
    f32 out = buf - in;
    a->buf[a->idx] = in + buf * 0.5f;
    if (++a->idx >= a->len) a->idx = 0;
    return out;
}

/* Wet only: this is a send effect, the dry path never passes through it. */
void dm_fx_reverb(dm_audio *a, f32 room, f32 damp, f32 width)
{
    comb cl[NCOMB], cr[NCOMB];
    allp al[NALLP], ar[NALLP];

    for (int i = 0; i < NCOMB; i++) {
        cl[i].len = scale_len(COMB_LEN[i], 0);
        cr[i].len = scale_len(COMB_LEN[i], REV_SPREAD);
        cl[i].buf = (f32 *)calloc((size_t)cl[i].len, sizeof(f32));
        cr[i].buf = (f32 *)calloc((size_t)cr[i].len, sizeof(f32));
        cl[i].idx = cr[i].idx = 0;
        cl[i].store = cr[i].store = 0.0f;
    }
    for (int i = 0; i < NALLP; i++) {
        al[i].len = scale_len(ALLP_LEN[i], 0);
        ar[i].len = scale_len(ALLP_LEN[i], REV_SPREAD);
        al[i].buf = (f32 *)calloc((size_t)al[i].len, sizeof(f32));
        ar[i].buf = (f32 *)calloc((size_t)ar[i].len, sizeof(f32));
        al[i].idx = ar[i].idx = 0;
    }

    f32 feedback = 0.70f + 0.28f * dm_sat(room);
    f32 d        = dm_sat(damp) * 0.4f;
    f32 w        = dm_sat(width);

    for (int i = 0; i < a->n; i++) {
        f32 in = (a->l[i] + a->r[i]) * 0.015f;   /* input gain, keeps it stable */
        f32 ol = 0.0f, orr = 0.0f;

        for (int k = 0; k < NCOMB; k++) {
            ol  += comb_tick(&cl[k], in, feedback, d);
            orr += comb_tick(&cr[k], in, feedback, d);
        }
        for (int k = 0; k < NALLP; k++) {
            ol  = allp_tick(&al[k], ol);
            orr = allp_tick(&ar[k], orr);
        }

        /* Cross-feed controls how wide the tail sits. */
        a->l[i] = ol * w + orr * (1.0f - w);
        a->r[i] = orr * w + ol * (1.0f - w);
    }

    for (int i = 0; i < NCOMB; i++) { free(cl[i].buf); free(cr[i].buf); }
    for (int i = 0; i < NALLP; i++) { free(al[i].buf); free(ar[i].buf); }
}

/* ---- delay --------------------------------------------------------------- */

/* Wet only, and ping-pong: the left tap feeds the right buffer and vice versa,
 * so a single hit bounces across the stereo field as it decays. */
void dm_fx_delay(dm_audio *a, f64 time_l, f64 time_r, f32 feedback, f32 damp)
{
    int nl = DM_MAX(1, dm_sec_to_frames(time_l));
    int nr = DM_MAX(1, dm_sec_to_frames(time_r));
    f32 *bl = (f32 *)calloc((size_t)nl, sizeof(f32));
    f32 *br = (f32 *)calloc((size_t)nr, sizeof(f32));
    if (!bl || !br) { free(bl); free(br); return; }

    int il = 0, ir = 0;
    f32 sl = 0.0f, sr = 0.0f;          /* one-pole damping state */
    f32 d  = dm_sat(damp) * 0.85f;
    f32 fb = dm_clamp(feedback, 0.0f, 0.95f);

    for (int i = 0; i < a->n; i++) {
        f32 outl = bl[il];
        f32 outr = br[ir];

        sl = outl * (1.0f - d) + sl * d;
        sr = outr * (1.0f - d) + sr * d;

        bl[il] = a->r[i] + sr * fb;
        br[ir] = a->l[i] + sl * fb;

        if (++il >= nl) il = 0;
        if (++ir >= nr) ir = 0;

        a->l[i] = outl;
        a->r[i] = outr;
    }
    free(bl);
    free(br);
}

/* ---- mix utilities ------------------------------------------------------- */

void dm_fx_sidechain(dm_audio *a, const f64 *triggers, int count,
                     f32 depth, f64 attack, f64 release)
{
    f32 *gain = (f32 *)malloc((size_t)a->n * sizeof(f32));
    if (!gain) return;
    for (int i = 0; i < a->n; i++) gain[i] = 1.0f;

    int span = dm_sec_to_frames(attack + release * 6.0);

    for (int t = 0; t < count; t++) {
        int i0 = dm_sec_to_frames(triggers[t]);
        for (int k = 0; k < span; k++) {
            int i = i0 + k;
            if (i < 0) continue;
            if (i >= a->n) break;
            f64 tt = (f64)k / DM_SR;
            /* Duck in over `attack`, then let it breathe back over `release`.
             * The pumping this creates is not a side effect of the genre, it
             * is the genre. */
            f32 e = tt < attack ? (f32)(tt / attack)
                                : expf(-(f32)(tt - attack) / (f32)release);
            f32 g = 1.0f - depth * e;
            if (g < gain[i]) gain[i] = g;
        }
    }

    for (int i = 0; i < a->n; i++) { a->l[i] *= gain[i]; a->r[i] *= gain[i]; }
    free(gain);
}

void dm_fx_saturate(dm_audio *a, f32 drive)
{
    for (int i = 0; i < a->n; i++) {
        a->l[i] = saturate(a->l[i], drive);
        a->r[i] = saturate(a->r[i], drive);
    }
}

static void filter_inplace(dm_audio *a, f32 hz, dm_filter_mode mode)
{
    svf fl = { 0, 0 }, fr = { 0, 0 };
    svf_coef c = svf_make(hz, 0.0f);
    for (int i = 0; i < a->n; i++) {
        a->l[i] = svf_tick(&fl, a->l[i], &c, mode);
        a->r[i] = svf_tick(&fr, a->r[i], &c, mode);
    }
}

void dm_fx_highpass(dm_audio *a, f32 hz) { filter_inplace(a, hz, DM_HP); }
void dm_fx_lowpass (dm_audio *a, f32 hz) { filter_inplace(a, hz, DM_LP); }

void dm_fx_widen(dm_audio *a, f32 amount)
{
    for (int i = 0; i < a->n; i++) {
        f32 mid  = (a->l[i] + a->r[i]) * 0.5f;
        f32 side = (a->l[i] - a->r[i]) * 0.5f * (1.0f + amount);
        a->l[i] = mid + side;
        a->r[i] = mid - side;
    }
}

void dm_fx_fade(dm_audio *a, f64 t0, f64 t1, f32 g0, f32 g1)
{
    int i0 = dm_sec_to_frames(t0), i1 = dm_sec_to_frames(t1);
    if (i0 < 0) i0 = 0;
    if (i1 > a->n) i1 = a->n;
    for (int i = i0; i < i1; i++) {
        f32 u = (f32)(i - i0) / (f32)DM_MAX(1, i1 - i0);
        f32 g = dm_lerp(g0, g1, u);
        a->l[i] *= g;
        a->r[i] *= g;
    }
    /* Hold the end level for the rest of the buffer when fading out. */
    if (g1 == 0.0f)
        for (int i = i1; i < a->n; i++) { a->l[i] = 0.0f; a->r[i] = 0.0f; }
}

/* Two-pass peak limiter. The backward pass is the lookahead: it pulls the gain
 * down *before* a transient arrives, so peaks are caught without the click a
 * purely reactive limiter makes. */
void dm_fx_limiter(dm_audio *a, f32 ceiling_db, f64 attack, f64 release)
{
    f32 ceiling = powf(10.0f, ceiling_db / 20.0f);
    f32 *gain = (f32 *)malloc((size_t)a->n * sizeof(f32));
    if (!gain) return;

    for (int i = 0; i < a->n; i++) {
        f32 peak = DM_MAX(fabsf(a->l[i]), fabsf(a->r[i]));
        gain[i] = peak > ceiling ? ceiling / peak : 1.0f;
    }

    f32 up = 1.0f / (f32)DM_MAX(1.0, attack  * DM_SR);
    f32 dn = 1.0f / (f32)DM_MAX(1.0, release * DM_SR);

    for (int i = a->n - 2; i >= 0; i--)
        if (gain[i] > gain[i + 1] + up) gain[i] = gain[i + 1] + up;

    for (int i = 1; i < a->n; i++)
        if (gain[i] > gain[i - 1] + dn) gain[i] = gain[i - 1] + dn;

    for (int i = 0; i < a->n; i++) { a->l[i] *= gain[i]; a->r[i] *= gain[i]; }
    free(gain);
}
