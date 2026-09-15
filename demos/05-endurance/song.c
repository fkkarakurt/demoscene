#include "song.h"
#include "../../engine/dm_synth.h"
#include "../../engine/dm_rand.h"

#include <stdlib.h>

const int SONG_SECTION_BAR[SEC_COUNT + 1] = { 0, 8, 14, 19, 25, 30 };

song_section song_section_at(f64 t)
{
    f64 bar = t / SONG_BAR_SEC;
    for (int s = SEC_COUNT - 1; s >= 0; s--)
        if (bar >= SONG_SECTION_BAR[s]) return (song_section)s;
    return SEC_WINTER;
}

f64 song_section_start(song_section s) { return SONG_SECTION_BAR[s] * SONG_BAR_SEC; }
f64 song_section_end  (song_section s) { return SONG_SECTION_BAR[s + 1] * SONG_BAR_SEC; }

f64 song_section_phase(f64 t)
{
    song_section s = song_section_at(t);
    f64 a = song_section_start(s), b = song_section_end(s);
    return b > a ? dm_clamp((f32)((t - a) / (b - a)), 0.0f, 1.0f) : 0.0f;
}

/* ---- the score ----------------------------------------------------------------
 *
 * Two layers, mixed separately. The music is a small string orchestra, a solo
 * voice for the one theme, and a celesta for the cold -- in D minor for the
 * ice and the sinking, and D major for the finding. The sound is the story
 * told without notes: wind over the pack, the ice working against itself and
 * against her, the timber going, the water closing over, and a sonar ping in
 * the dark of 2022. Everything that happens on screen at a known moment is a
 * cue in song.h, which the picture reads too.
 *
 * Under the water the sound is muffled and the music is not: the camera went
 * under, the orchestra did not.
 */

#define T(bar) song_bar(bar)

/* ---- small tools --------------------------------------------------------------- */

static int span(const dm_audio *a, f64 t0, f64 t1, int *i0, int *i1)
{
    *i0 = t0 > 0.0 ? dm_sec_to_frames(t0) : 0;
    *i1 = dm_sec_to_frames(t1);
    if (*i1 > a->n) *i1 = a->n;
    return *i1 > *i0;
}

static inline f32 white(u32 *s)
{
    *s = dm_hash_u32(*s + 0x9e3779b9u);
    return dm_u32_to_f32(*s) * 2.0f - 1.0f;
}

/* A smooth random value in [0, 1] that wanders `rate` times a second. */
static f32 drift(f64 t, f32 rate, u32 seed)
{
    f64 x  = t * rate;
    f64 fx = floor(x);
    f32 u  = (f32)(x - fx);
    u = u * u * (3.0f - 2.0f * u);
    i32 k = (i32)fx;
    return dm_lerp(dm_u32_to_f32(dm_hash2i(k, 0, seed)), dm_u32_to_f32(dm_hash2i(k + 1, 0, seed)), u);
}

static inline void pan_gains(f32 pan, f32 *gl, f32 *gr)
{
    f32 a = (dm_clamp(pan, -1.0f, 1.0f) * 0.5f + 0.5f) * DM_HALFPI;
    *gl = cosf(a);
    *gr = sinf(a);
}

static inline void put(dm_audio *bus, dm_audio *send, int i, f32 l, f32 r, f32 amt)
{
    bus->l[i] += l;
    bus->r[i] += r;
    if (send && amt > 0.0f) { send->l[i] += l * amt; send->r[i] += r * amt; }
}

/* ---- sound -------------------------------------------------------------------- */

/* Wind over the pack: noise through a band-pass that wanders, in gusts. */
static void wind(dm_audio *bus, f64 t0, f64 t1, f32 level, f32 hz, f32 edge, u32 seed)
{
    int i0, i1;
    if (!span(bus, t0, t1, &i0, &i1)) return;
    u32 sl = seed, sr = seed ^ 0x5bd1e995u;
    dm_svf bl = { 0, 0 }, br = { 0, 0 }, hl = { 0, 0 }, hr = { 0, 0 };
    dm_svf_coef cl = dm_svf_make(hz, 0.4f), cr = cl, hp = dm_svf_make(140.0f, 0.0f);
    f32 gust = 0.0f;
    for (int i = i0; i < i1; i++) {
        f64 t = (f64)i / DM_SR;
        if (((i - i0) & 63) == 0) {
            gust = drift(t, 0.3f, seed ^ 6u) * 0.7f + drift(t, 1.1f, seed ^ 7u) * 0.3f;
            cl = dm_svf_make(hz * (0.55f + 0.9f * gust + 0.3f * drift(t, 2.3f, seed ^ 8u)), 0.5f);
            cr = dm_svf_make(hz * (0.55f + 0.9f * gust + 0.3f * drift(t, 2.3f, seed ^ 9u)), 0.5f);
        }
        f32 e = dm_sat((f32)((t - t0) / edge)) * dm_sat((f32)((t1 - t) / edge));
        f32 g = level * e * (0.2f + 1.3f * gust * gust);
        f32 l = dm_svf_tick(&hl, dm_svf_tick(&bl, white(&sl), &cl, DM_BP), &hp, DM_HP);
        f32 r = dm_svf_tick(&hr, dm_svf_tick(&br, white(&sr), &cr, DM_BP), &hp, DM_HP);
        bus->l[i] += l * g;
        bus->r[i] += r * g;
    }
}

/* Ice working against ice, or against a hull: stick-slip friction. A train of
 * small slips, quicker as the load builds, each one ringing the floe or the
 * planking at a pitch that wanders as the load moves. */
static void groan(dm_audio *bus, dm_audio *send, f64 t0, f64 len, f32 hz, f32 vel, f32 pan, u32 seed)
{
    int i0, i1;
    if (!span(bus, t0, t0 + len + 0.8, &i0, &i1)) return;
    u32 s = seed;
    dm_svf a = { 0, 0 }, b = { 0, 0 };
    dm_svf_coef ca = dm_svf_make(hz, 0.95f), cb = dm_svf_make(hz * 2.71f, 0.9f);
    f32 gl, gr;
    pan_gains(pan, &gl, &gr);
    f32 ph = 0.0f;
    for (int i = i0; i < i1; i++) {
        f64 tt = (f64)(i - i0) / DM_SR;
        f32 u  = (f32)(tt / len);
        if (((i - i0) & 31) == 0) {
            f32 w = 1.0f + 0.35f * (drift(tt, 2.2f, seed ^ 3u) - 0.5f) - 0.12f * u;
            ca = dm_svf_make(hz * w, 0.95f);
            cb = dm_svf_make(hz * 2.71f * w, 0.90f);
        }
        f32 env = u < 1.0f ? dm_smoothstep(0.0f, 0.35f, u) * (1.0f - 0.7f * dm_smoothstep(0.75f, 1.0f, u))
                           : 0.3f * expf(-(f32)(tt - len) / 0.12f);
        f32 rate = 7.0f + 38.0f * env * drift(tt, 4.0f, seed ^ 4u);
        ph += rate / (f32)DM_SR;
        f32 exc = white(&s) * 0.03f * env;
        if (ph >= 1.0f) { ph -= 1.0f; exc += (0.4f + 0.6f * dm_u32_to_f32(dm_hash_u32(s))) * env; }
        f32 v = (dm_svf_tick(&a, exc, &ca, DM_BP) + 0.45f * dm_svf_tick(&b, exc, &cb, DM_BP)) * vel * 0.5f;
        v = tanhf(v * 1.5f);
        put(bus, send, i, v * gl, v * gr, 0.35f);
    }
}

/* Timber breaking: one sharp report, the splinters after it, and the thump of
 * the spar's own weight in the wood. `size` stretches all of it. */
static void crack(dm_audio *bus, dm_audio *send, f64 t, f32 vel, f32 size, u32 seed)
{
    f64 len = 0.25 + 1.2 * size;
    int i0, i1;
    if (!span(bus, t, t + len, &i0, &i1)) return;
    enum { NS = 40 };
    f32 at[NS], amp[NS], pan[NS];
    u32 h = seed;
    int ns = DM_MIN(NS, 8 + (int)(26.0f * size));
    for (int k = 0; k < ns; k++) {
        f32 r0 = dm_u32_to_f32(h = dm_hash_u32(h + 1u));
        at[k]  = 0.004f + powf(r0, 1.8f) * 0.55f * size;
        amp[k] = (0.25f + 0.75f * dm_u32_to_f32(h = dm_hash_u32(h + 1u))) * expf(-at[k] / (0.25f * size));
        pan[k] = dm_u32_to_f32(h = dm_hash_u32(h + 1u)) * 1.6f - 0.8f;
    }
    u32 s = seed ^ 0xc4acu;
    dm_svf hpl = { 0, 0 }, hpr = { 0, 0 }, lp = { 0, 0 };
    dm_svf_coef hp = dm_svf_make(900.0f, 0.2f), lo = dm_svf_make(160.0f, 0.3f);
    f32 bph = 0.0f;
    for (int i = i0; i < i1; i++) {
        f32 tt = (f32)(i - i0) / (f32)DM_SR;
        f32 n  = white(&s);
        /* The report. */
        f32 l = n * expf(-tt / 0.004f) * 1.4f, r = l;
        /* The splinters: bursts a millisecond or two long. */
        for (int k = 0; k < ns; k++) {
            f32 d = tt - at[k];
            if (d < 0.0f || d > 0.012f) continue;
            f32 e = amp[k] * expf(-d / 0.0018f);
            l += n * e * (1.0f - pan[k]) * 0.6f;
            r += n * e * (1.0f + pan[k]) * 0.6f;
        }
        l = dm_svf_tick(&hpl, l, &hp, DM_HP);
        r = dm_svf_tick(&hpr, r, &hp, DM_HP);
        /* The body. */
        f32 bhz = 62.0f / DM_MAX(size, 0.3f) + 50.0f * expf(-tt / 0.03f);
        bph += bhz / (f32)DM_SR;
        if (bph >= 1.0f) bph -= 1.0f;
        f32 body = (sinf(bph * DM_TAU) * 0.8f + dm_svf_tick(&lp, n, &lo, DM_LP) * 1.5f) *
                   expf(-tt / (0.10f + 0.25f * size)) * (1.0f - expf(-tt / 0.002f));
        l = tanhf((l + body) * 1.2f) * vel * 0.45f;
        r = tanhf((r + body) * 1.2f) * vel * 0.45f;
        put(bus, send, i, l, r, 0.45f);
    }
}

/* A heavy thing landing: a falling spar on the ice, a hull on the bottom. */
static void boom(dm_audio *bus, dm_audio *send, f64 t, f32 vel, f32 hz, f32 decay, u32 seed)
{
    int i0, i1;
    if (!span(bus, t, t + decay * 5.0, &i0, &i1)) return;
    u32 sl = seed, sr = seed ^ 0x2545f491u;
    dm_svf l1 = { 0, 0 }, l2 = { 0, 0 }, r1 = { 0, 0 }, r2 = { 0, 0 };
    dm_svf_coef lo = dm_svf_make(220.0f, 0.1f);
    f32 ph = 0.0f;
    for (int i = i0; i < i1; i++) {
        f32 tt = (f32)(i - i0) / (f32)DM_SR;
        ph += hz * (1.0f + 1.4f * expf(-tt / 0.06f)) / (f32)DM_SR;
        if (ph >= 1.0f) ph -= 1.0f;
        f32 env  = expf(-tt / decay) * (1.0f - expf(-tt / 0.003f));
        f32 tone = sinf(ph * DM_TAU) * env;
        f32 nl = dm_svf_tick(&l2, dm_svf_tick(&l1, white(&sl), &lo, DM_LP), &lo, DM_LP);
        f32 nr = dm_svf_tick(&r2, dm_svf_tick(&r1, white(&sr), &lo, DM_LP), &lo, DM_LP);
        f32 ne = expf(-tt / (decay * 0.7f)) * 1.5f;
        put(bus, send, i, tanhf((tone + nl * ne) * 1.6f) * vel * 0.32f,
                          tanhf((tone + nr * ne) * 1.6f) * vel * 0.32f, 0.3f);
    }
}

/* The stern light failing: the mains hum of its dynamo line and the crackle of
 * a parted cable, switched by exactly the flicker the picture shows. */
static void buzz(dm_audio *bus, f64 t0, f32 vel)
{
    int i0, i1;
    if (!span(bus, t0, t0 + 0.5, &i0, &i1)) return;
    u32 s = 0xb022u;
    f32 ph = 0.0f;
    for (int i = i0; i < i1; i++) {
        f32 k = (f32)(i - i0) / (f32)DM_SR;
        f32 on = k < 0.45f ? (dm_hash1f(floorf(k * 22.0f), 0x11u) > 0.5f ? 1.0f : 0.0f) * (1.0f - k / 0.45f) : 0.0f;
        ph += 100.0f / (f32)DM_SR;
        if (ph >= 1.0f) ph -= 1.0f;
        f32 hum = (ph < 0.5f ? 1.0f : -1.0f) * 0.25f + sinf(ph * DM_TAU * 3.0f) * 0.15f;
        f32 crackle = white(&s) * (dm_u32_to_f32(dm_hash_u32(s)) > 0.93f ? 1.0f : 0.1f);
        f32 v = (hum + crackle * 0.5f) * on * vel * 0.25f;
        bus->l[i] += v;
        bus->r[i] += v;
    }
}

/* Water closing over a hull: bubbles, each a note that rises as it is born --
 * a bubble rings at a pitch set by its size, and a small one rings high. */
static void bubbles(dm_audio *bus, dm_audio *send, f64 t0, f64 t1, f32 rate0, f32 rate1, f32 vel, u32 seed)
{
    u32 h = seed;
    f64 t = t0;
    while (t < t1) {
        f32 u = (f32)((t - t0) / (t1 - t0));
        f32 rate = dm_lerp(rate0, rate1, u);
        f32 r0 = dm_u32_to_f32(h = dm_hash_u32(h + 1u));
        t += -log(DM_MAX(1.0 - r0, 1e-6)) / rate;
        f32 size = dm_u32_to_f32(h = dm_hash_u32(h + 1u));
        f32 hz   = dm_lerp(1500.0f, 180.0f, powf(size, 0.7f));
        f32 tau  = dm_lerp(0.006f, 0.035f, size);
        f32 amp  = vel * dm_lerp(0.25f, 1.0f, size) * (0.5f + 0.5f * dm_u32_to_f32(h = dm_hash_u32(h + 1u)));
        f32 pan  = dm_u32_to_f32(h = dm_hash_u32(h + 1u)) * 1.8f - 0.9f;
        f32 gl, gr;
        pan_gains(pan, &gl, &gr);
        int i0, i1;
        if (!span(bus, t, t + tau * 7.0f, &i0, &i1)) continue;
        f32 ph = 0.0f;
        for (int i = i0; i < i1; i++) {
            f32 tt = (f32)(i - i0) / (f32)DM_SR;
            ph += hz * (1.0f + 0.9f * tt / (tau * 4.0f)) / (f32)DM_SR;
            if (ph >= 1.0f) ph -= 1.0f;
            f32 v = sinf(ph * DM_TAU) * expf(-tt / tau) * (1.0f - expf(-tt / 0.0008f)) * amp * 0.3f;
            put(bus, send, i, v * gl, v * gr, 0.5f);
        }
    }
}

/* Deep water: pressure you hear as a rumble below the note of anything. */
static void rumble(dm_audio *bus, f64 t0, f64 t1, f32 level, f32 hz, u32 seed)
{
    int i0, i1;
    if (!span(bus, t0, t1, &i0, &i1)) return;
    u32 sl = seed, sr = seed ^ 0x9e37u;
    dm_svf a = { 0, 0 }, b = { 0, 0 }, c = { 0, 0 }, d = { 0, 0 };
    dm_svf_coef lo = dm_svf_make(hz, 0.2f);
    for (int i = i0; i < i1; i++) {
        f64 t = (f64)i / DM_SR;
        f32 e = dm_sat((f32)((t - t0) / 2.0)) * dm_sat((f32)((t1 - t) / 2.0));
        f32 g = level * e * (0.6f + 0.8f * drift(t, 0.4f, seed));
        bus->l[i] += dm_svf_tick(&b, dm_svf_tick(&a, white(&sl), &lo, DM_LP), &lo, DM_LP) * g * 3.0f;
        bus->r[i] += dm_svf_tick(&d, dm_svf_tick(&c, white(&sr), &lo, DM_LP), &lo, DM_LP) * g * 3.0f;
    }
}

/* The vehicle: the whine of its thrusters and the hum of its lamps. */
static void hum(dm_audio *bus, f64 t0, f64 t1, f32 level, u32 seed)
{
    int i0, i1;
    if (!span(bus, t0, t1, &i0, &i1)) return;
    u32 s = seed;
    dm_svf bp = { 0, 0 };
    dm_svf_coef c = dm_svf_make(420.0f, 0.6f);
    f32 ph = 0.0f;
    for (int i = i0; i < i1; i++) {
        f64 t = (f64)i / DM_SR;
        f32 e = dm_smoothstep(0.0f, 1.2f, (f32)(t - t0)) * dm_sat((f32)((t1 - t) / 1.5));
        f32 thrust = 0.4f + 0.6f * drift(t, 0.25f, seed ^ 1u);
        ph += (58.0f + 1.5f * thrust) / (f32)DM_SR;
        if (ph >= 1.0f) ph -= 1.0f;
        f32 tone = sinf(ph * DM_TAU) * 0.5f + sinf(ph * DM_TAU * 2.0f) * 0.25f + sinf(ph * DM_TAU * 5.0f) * 0.05f;
        f32 v = (tone * 0.35f + dm_svf_tick(&bp, white(&s), &c, DM_BP) * 0.6f * thrust) * level * e;
        bus->l[i] += v;
        bus->r[i] += v;
    }
}

/* A sonar ping, and its echo off a hull that should not be there. */
static void ping(dm_audio *bus, dm_audio *send, f64 t, f32 vel, f32 hz)
{
    int i0, i1;
    if (!span(bus, t, t + 2.5, &i0, &i1)) return;
    f32 p1 = 0.0f, p2 = 0.0f;
    for (int i = i0; i < i1; i++) {
        f32 tt = (f32)(i - i0) / (f32)DM_SR;
        p1 += hz * (1.0f - 0.01f * tt) / (f32)DM_SR;
        p2 += hz * 2.005f / (f32)DM_SR;
        if (p1 >= 1.0f) p1 -= 1.0f;
        if (p2 >= 1.0f) p2 -= 1.0f;
        f32 atk = 1.0f - expf(-tt / 0.004f);
        f32 v = (sinf(p1 * DM_TAU) * expf(-tt / 0.45f) + 0.15f * sinf(p2 * DM_TAU) * expf(-tt / 0.15f)) * atk;
        /* The echo, 0.55 s later, fainter and duller. */
        f32 te = tt - 0.55f;
        if (te > 0.0f) v += 0.22f * sinf((f32)((f64)hz * te) * DM_TAU) * expf(-te / 0.3f) * (1.0f - expf(-te / 0.01f));
        v *= vel * 0.35f;
        put(bus, send, i, v * 0.8f, v, 0.8f);
    }
}

/* Glints: the sparks of the fourth act as sound, very high and very quiet. */
static void glints(dm_audio *bus, dm_audio *send, f64 t0, f64 t1, f32 rate, f32 vel, u32 seed)
{
    u32 h = seed;
    f64 t = t0;
    while (t < t1) {
        t += -log(DM_MAX(1.0 - dm_u32_to_f32(h = dm_hash_u32(h + 1u)), 1e-6)) / rate;
        f32 hz  = dm_midi_hz(96.0f + (f32)(dm_hash_u32(h + 7u) % 5u) * 2.5f);
        f32 pan = dm_u32_to_f32(h = dm_hash_u32(h + 1u)) * 1.8f - 0.9f;
        f32 e   = dm_sat((f32)((t - t0) / 3.0)) * dm_sat((f32)((t1 - t) / 2.0));
        f32 gl, gr;
        pan_gains(pan, &gl, &gr);
        int i0, i1;
        if (!span(bus, t, t + 0.4, &i0, &i1)) continue;
        f32 ph = 0.0f;
        for (int i = i0; i < i1; i++) {
            f32 tt = (f32)(i - i0) / (f32)DM_SR;
            ph += hz / (f32)DM_SR;
            if (ph >= 1.0f) ph -= 1.0f;
            f32 v = sinf(ph * DM_TAU) * expf(-tt / 0.07f) * (1.0f - expf(-tt / 0.002f)) * vel * e * 0.12f;
            put(bus, send, i, v * gl, v * gr, 0.9f);
        }
    }
}

/* What the water does to sound between two times: takes the top off it. The
 * cut in is a cut, because the camera goes under at a cut. */
static void muffle(dm_audio *a, f64 t0, f64 t1, f32 hz)
{
    int i0, i1;
    if (!span(a, t0, t1 + 1.0, &i0, &i1)) return;
    f32 k = 1.0f - expf(-DM_TAU * hz / (f32)DM_SR);
    f32 l1 = 0, l2 = 0, r1 = 0, r2 = 0;
    for (int i = i0; i < i1; i++) {
        f64 t = (f64)i / DM_SR;
        l1 += (a->l[i] - l1) * k; l2 += (l1 - l2) * k;
        r1 += (a->r[i] - r1) * k; r2 += (r1 - r2) * k;
        f32 w = dm_sat((f32)((t - t0) / 0.03)) * dm_sat((f32)((t1 + 1.0 - t) / 1.0));
        a->l[i] = dm_lerp(a->l[i], l2 * 1.3f, w);
        a->r[i] = dm_lerp(a->r[i], r2 * 1.3f, w);
    }
}

/* ---- the instruments -------------------------------------------------------------- */

static dm_patch strings(void)
{
    dm_patch p = dm_patch_default();
    p.kind = DM_OSC_SAW;
    p.unison = 6; p.detune_cents = 11.0f; p.spread = 0.9f;
    p.amp_a = 2.0f; p.amp_d = 2.5f; p.amp_s = 0.85f; p.amp_r = 2.6f;
    p.flt_a = 2.4f; p.flt_d = 3.0f; p.flt_s = 0.6f;  p.flt_r = 2.5f;
    p.cutoff = 420.0f; p.env_amount = 900.0f; p.key_track = 0.5f; p.resonance = 0.05f;
    p.vib_rate = 4.7f; p.vib_depth = 0.04f;
    p.gain = 0.045f; p.send = 0.55f;
    return p;
}

/* The same section, bowed hard: for the moments the ice wins. */
static dm_patch stab(void)
{
    dm_patch p = strings();
    p.amp_a = 0.03f; p.amp_d = 1.4f; p.amp_s = 0.35f; p.amp_r = 1.8f;
    p.flt_a = 0.01f; p.flt_d = 0.7f; p.flt_s = 0.25f;
    p.cutoff = 300.0f; p.env_amount = 2600.0f; p.drive = 0.25f;
    p.gain = 0.07f; p.send = 0.65f;
    return p;
}

/* And swelling out of nothing into the dive. */
static dm_patch swell(void)
{
    dm_patch p = strings();
    p.amp_a = 3.5f; p.amp_s = 1.0f; p.flt_a = 3.6f; p.env_amount = 1800.0f;
    p.gain = 0.05f;
    return p;
}

/* The solo voice that carries the theme: something between a cello and a
 * horn, with a slow vibrato that only arrives once the note has settled. */
static dm_patch voice(void)
{
    dm_patch p = dm_patch_default();
    p.kind = DM_OSC_SAW;
    p.unison = 3; p.detune_cents = 6.0f; p.spread = 0.25f;
    p.amp_a = 0.22f; p.amp_d = 1.5f; p.amp_s = 0.8f; p.amp_r = 0.9f;
    p.flt_a = 0.30f; p.flt_d = 1.2f; p.flt_s = 0.45f; p.flt_r = 0.8f;
    p.cutoff = 520.0f; p.env_amount = 1300.0f; p.key_track = 0.6f; p.resonance = 0.12f;
    p.vib_rate = 5.1f; p.vib_depth = 0.09f;
    p.gain = 0.085f; p.send = 0.5f;
    return p;
}

static dm_patch celesta(void)
{
    dm_patch p = dm_patch_default();
    p.kind = DM_OSC_TRI;
    p.amp_a = 0.002f; p.amp_d = 0.7f; p.amp_s = 0.0f; p.amp_r = 0.9f;
    p.flt_a = 0.001f; p.flt_d = 0.4f; p.flt_s = 0.0f;
    p.cutoff = 2500.0f; p.env_amount = 3500.0f; p.resonance = 0.0f;
    p.gain = 0.05f; p.send = 0.85f;
    return p;
}

static dm_patch drone(void)
{
    dm_patch p = dm_patch_default();
    p.kind = DM_OSC_SAW;
    p.unison = 3; p.detune_cents = 5.0f; p.spread = 0.5f; p.sub_level = 0.3f;
    p.amp_a = 5.0f; p.amp_d = 2.0f; p.amp_s = 1.0f; p.amp_r = 4.0f;
    p.cutoff = 150.0f; p.env_amount = 0.0f; p.resonance = 0.0f;
    p.gain = 0.06f; p.send = 0.25f;
    return p;
}

/* A low bowed pulse under the second act, the one rhythm in the piece. */
static dm_patch pulse(void)
{
    dm_patch p = dm_patch_default();
    p.kind = DM_OSC_SAW;
    p.unison = 2; p.detune_cents = 6.0f; p.sub_level = 0.5f;
    p.amp_a = 0.02f; p.amp_d = 0.30f; p.amp_s = 0.0f; p.amp_r = 0.25f;
    p.flt_a = 0.005f; p.flt_d = 0.20f; p.flt_s = 0.0f;
    p.cutoff = 120.0f; p.env_amount = 600.0f; p.resonance = 0.15f;
    p.gain = 0.12f; p.send = 0.15f;
    return p;
}

typedef struct { f32 bar, bars, vel; f32 note[6]; } chord;
typedef struct { f32 sec, len, note, vel; } note;

static void play_chords(dm_audio *bus, dm_audio *send, const dm_patch *p, const chord *c, int n, u32 *seed)
{
    for (int i = 0; i < n; i++)
        for (int k = 0; k < 6 && c[i].note[k] > 0.0f; k++)
            dm_synth_note(bus, send, p, T(c[i].bar), T(c[i].bars), c[i].note[k], c[i].vel, (*seed)++);
}

static void play_notes(dm_audio *bus, dm_audio *send, const dm_patch *p, const note *m, int n, u32 *seed)
{
    for (int i = 0; i < n; i++)
        dm_synth_note(bus, send, p, m[i].sec, m[i].len, m[i].note, m[i].vel, (*seed)++);
}

/* ---- the arrangement ----------------------------------------------------------------- */

/* D minor and its neighbours, then D major. Voicings are written out; there
 * is no chord logic, only notes. */
static const chord STRINGS[] = {
    /* WINTER: the long night. */
    {  0.0f, 2.25f, 0.55f, { 50, 57, 65 } },                /* Dm        */
    {  2.0f, 2.25f, 0.60f, { 46, 53, 57, 62 } },            /* Bbmaj7    */
    {  4.0f, 2.25f, 0.62f, { 43, 50, 58, 65 } },            /* Gm7       */
    {  6.0f, 1.10f, 0.66f, { 45, 52, 62, 64 } },            /* Asus4     */
    {  7.0f, 1.10f, 0.66f, { 45, 52, 61, 64 } },            /* A         */
    /* PRESSURE. */
    {  8.0f, 2.10f, 0.50f, { 38, 50, 53, 57 } },            /* Dm        */
    { 10.0f, 0.60f, 0.55f, { 36, 48, 53, 57 } },            /* Dm/C      */
    { 11.0f, 1.40f, 0.60f, { 34, 46, 50, 53, 64 } },        /* Bb(#11)   */
    { 12.4f, 0.70f, 0.62f, { 31, 43, 50, 58 } },            /* Gm        */
    { 13.0f, 1.05f, 0.70f, { 33, 45, 52, 55, 61, 64 } },    /* A7        */
    /* SINKING: a mile and a half away, then under. */
    { 14.0f, 0.95f, 0.35f, { 69, 76, 81 } },                /* high A    */
    { 15.8f, 1.00f, 0.70f, { 41, 53, 57, 60, 65, 69 } },    /* F         */
    { 16.8f, 0.30f, 0.60f, { 33, 45, 53, 57, 60, 65 } },    /* F/A       */
    { 17.0f, 2.10f, 0.45f, { 38, 50, 57, 62, 64 } },        /* Dm(add9)  */
    /* DEEP. */
    { 19.0f, 1.10f, 0.45f, { 38, 50, 57, 65 } },            /* Dm        */
    { 20.0f, 1.10f, 0.55f, { 41, 53, 57, 60, 65 } },        /* F         */
    { 21.0f, 1.10f, 0.55f, { 34, 46, 53, 57, 62 } },        /* Bbmaj7    */
    { 22.0f, 0.60f, 0.45f, { 36, 48, 55, 64 } },            /* C         */
    { 22.5f, 1.10f, 0.40f, { 38, 50, 57, 62 } },            /* Dm        */
    { 23.5f, 0.80f, 0.35f, { 34, 50, 53, 58 } },            /* Bb        */
    { 24.2f, 0.45f, 0.30f, { 33, 45, 52, 57 } },            /* A (open)  */
    /* FOUND. */
    { 25.3f, 1.10f, 0.45f, { 38, 50, 57, 62, 66 } },        /* D         */
    { 26.3f, 0.80f, 0.45f, { 35, 47, 55, 59, 62 } },        /* G/B       */
    { 27.0f, 0.55f, 0.50f, { 33, 45, 52, 57, 64 } },        /* Asus      */
    { 27.5f, 1.05f, 0.65f, { 38, 50, 57, 62, 66, 69 } },    /* D         */
    { 28.5f, 0.55f, 0.55f, { 35, 47, 54, 59, 62 } },        /* Bm        */
    { 29.0f, 0.55f, 0.50f, { 31, 43, 55, 59, 62 } },        /* G         */
    { 29.5f, 0.45f, 0.45f, { 38, 50, 57, 62, 66 } },        /* D         */
};

static const chord STABS[] = {
    { (f32)CUE_SQUEEZE,   0.8f, 0.90f, { 34, 46, 53, 58, 62 } },
    { (f32)CUE_MAIN_FALL, 0.8f, 1.00f, { 33, 45, 52, 55, 61 } },
    { 15.8f,              0.6f, 0.80f, { 34, 46, 53, 58, 62 } },
};

static const chord SWELLS[] = {
    { (f32)CUE_GOING, 0.93f, 0.9f, { 34, 46, 53, 58, 62, 65 } },
};

/* The theme: four notes that rise and fall back, and an answer. In minor on
 * the ice, in major at the stern. */
static const note THEME[] = {
    /* WINTER, bar 4: under the ship shot. */
    { 16.0f, 2.0f, 62, 0.70f }, { 18.0f, 1.0f, 65, 0.72f }, { 19.0f, 1.0f, 64, 0.70f }, { 20.0f, 3.8f, 57, 0.68f },
    { 24.0f, 2.0f, 62, 0.74f }, { 26.0f, 1.0f, 65, 0.76f }, { 27.0f, 1.0f, 67, 0.78f }, { 28.0f, 2.0f, 69, 0.80f },
    { 30.0f, 2.4f, 64, 0.70f },
    /* SINKING: falling as her stern rises, landing on the dive. */
    { 60.0f, 1.6f, 69, 0.75f }, { 61.6f, 0.8f, 67, 0.75f }, { 62.4f, 0.8f, 65, 0.78f }, { 63.2f, 3.0f, 62, 0.85f },
    { 66.2f, 0.8f, 60, 0.70f }, { 67.0f, 2.5f, 57, 0.65f },
    /* DEEP: "every one of her men survived" -- up; "no one saw her again" -- down. */
    { 81.0f, 2.0f, 57, 0.60f }, { 83.0f, 1.0f, 62, 0.62f }, { 84.0f, 3.5f, 65, 0.65f },
    { 92.0f, 2.0f, 62, 0.50f }, { 94.0f, 1.0f, 60, 0.48f }, { 95.0f, 3.5f, 57, 0.45f },
    /* FOUND: the whole theme, in D major, as her name comes up. */
    { 110.0f, 1.5f, 62, 0.72f }, { 111.5f, 0.5f, 66, 0.72f }, { 112.0f, 1.0f, 64, 0.72f }, { 113.0f, 3.0f, 57, 0.70f },
    { 116.0f, 1.0f, 62, 0.74f }, { 117.0f, 0.5f, 66, 0.74f }, { 117.5f, 0.5f, 67, 0.76f }, { 118.0f, 1.8f, 69, 0.78f },
};

static const note CELESTA[] = {
    {  3.0f, 0.3f, 81, 0.5f }, {  5.0f, 0.3f, 76, 0.4f }, {  6.4f, 0.3f, 74, 0.45f }, {  9.6f, 0.3f, 77, 0.5f },
    { 11.6f, 0.3f, 81, 0.4f }, { 13.2f, 0.3f, 84, 0.35f }, { 15.2f, 0.3f, 79, 0.4f }, { 18.4f, 0.3f, 77, 0.35f },
    { 20.4f, 0.3f, 74, 0.35f }, { 22.0f, 0.3f, 81, 0.35f }, { 24.8f, 0.3f, 76, 0.35f }, { 26.8f, 0.3f, 73, 0.35f },
    { 29.2f, 0.3f, 76, 0.35f }, { 30.8f, 0.3f, 81, 0.3f },
    { 102.3f, 0.3f, 78, 0.35f }, { 104.1f, 0.3f, 81, 0.35f }, { 106.4f, 0.3f, 74, 0.35f }, { 108.2f, 0.3f, 76, 0.35f },
    { 110.2f, 0.3f, 86, 0.40f }, { 111.2f, 0.3f, 81, 0.35f }, { 113.5f, 0.3f, 78, 0.35f }, { 115.1f, 0.3f, 83, 0.35f },
    { 117.2f, 0.3f, 79, 0.30f }, { 118.4f, 0.3f, 81, 0.30f },
};

static void music(dm_audio *bus, dm_audio *send)
{
    u32 seed = 101u;
    dm_patch p;

    p = strings(); play_chords(bus, send, &p, STRINGS, DM_COUNT(STRINGS), &seed);
    p = stab();    play_chords(bus, send, &p, STABS, DM_COUNT(STABS), &seed);
    p = swell();   play_chords(bus, send, &p, SWELLS, DM_COUNT(SWELLS), &seed);
    p = voice();   play_notes(bus, send, &p, THEME, DM_COUNT(THEME), &seed);
    p = celesta(); play_notes(bus, send, &p, CELESTA, DM_COUNT(CELESTA), &seed);

    /* The drone: D under the ice, silent for the dive, D again for the fall,
     * and D for the finding. */
    p = drone();
    dm_synth_note(bus, send, &p, T(0.0),  T(8.2) + 2.0, 26.0f, 0.8f, seed++);
    dm_synth_note(bus, send, &p, T(8.0),  T(6.0),       26.0f, 0.9f, seed++);
    dm_synth_note(bus, send, &p, T(17.0), T(7.6),       26.0f, 0.9f, seed++);
    dm_synth_note(bus, send, &p, T(25.5), T(4.5),       26.0f, 0.7f, seed++);

    /* The pulse of the second act: a figure on D that turns toward C sharp as
     * the pressure comes, and quickens after the squeeze. */
    p = pulse();
    for (int b = 9; b < 14; b++) {
        static const f32 at[5] = { 0.0f, 0.5f, 1.5f, 2.0f, 3.0f };
        f32 v = dm_lerp(0.45f, 0.95f, (f32)(b - 9) / 4.0f);
        for (int k = 0; k < 5; k++) {
            f64 t = T(b) + at[k];
            f32 m = (b >= 11 && k == 4) ? 37.0f : 38.0f;
            dm_synth_note(bus, send, &p, t, 0.35, m, v * (k == 0 ? 1.0f : 0.75f), seed++);
            if (b >= 11 && t > T(CUE_SQUEEZE)) dm_synth_note(bus, send, &p, t + 0.25, 0.2, 38.0f, v * 0.5f, seed++);
        }
    }
}

static void sound(dm_audio *bus, dm_audio *send)
{
    /* WINTER: wind, and the pack settling a long way off. */
    wind(bus, 0.0, T(8.2), 0.55f, 700.0f, 3.0f, 0x31u);
    groan(bus, send, 9.0, 2.5, 180.0f, 0.22f, -0.6f, 0x41u);
    groan(bus, send, 21.0, 3.0, 140.0f, 0.18f, 0.5f, 0x42u);
    groan(bus, send, 28.5, 2.0, 230.0f, 0.18f, -0.2f, 0x43u);

    /* PRESSURE, the night: the ice at her, closer and closer, until the
     * squeeze -- and the light going out with it. */
    wind(bus, T(8.0), T(11.2), 0.35f, 600.0f, 1.5f, 0x32u);
    groan(bus, send, 34.0, 3.0, 160.0f, 0.35f, 0.3f, 0x44u);
    groan(bus, send, 37.5, 2.5, 230.0f, 0.42f, -0.4f, 0x45u);
    groan(bus, send, 40.0, 2.4, 125.0f, 0.55f, 0.1f, 0x46u);
    f64 sq = T(CUE_SQUEEZE);
    boom(bus, send, sq, 1.0f, 44.0f, 1.4f, 0x51u);
    crack(bus, send, sq + 0.02, 0.7f, 0.8f, 0x61u);
    groan(bus, send, sq, 1.4, 105.0f, 0.8f, 0.0f, 0x47u);
    buzz(bus, sq, 1.0f);

    /* PRESSURE, the day: the masts. */
    wind(bus, T(11.0), T(14.2), 0.5f, 950.0f, 1.0f, 0x33u);
    f64 fore = T(CUE_FORE_FALL), main = T(CUE_MAIN_FALL);
    groan(bus, send, fore - 1.4, 1.5, 300.0f, 0.45f, 0.4f, 0x48u);
    crack(bus, send, fore, 0.9f, 1.0f, 0x62u);
    boom(bus, send, fore + 1.45, 0.55f, 62.0f, 0.8f, 0x52u);
    crack(bus, send, fore + 1.45, 0.45f, 0.6f, 0x63u);
    groan(bus, send, main - 1.6, 1.7, 190.0f, 0.5f, -0.3f, 0x49u);
    crack(bus, send, main, 1.0f, 1.6f, 0x64u);
    groan(bus, send, main + 0.2, 2.1, 90.0f, 0.55f, -0.5f, 0x4au);
    boom(bus, send, main + CUE_MAIN_LAND, 1.0f, 38.0f, 2.0f, 0x53u);
    crack(bus, send, main + CUE_MAIN_LAND + 0.03, 0.7f, 1.2f, 0x65u);

    /* SINKING: from the camp, a long way off -- wind and, faint, her going. */
    wind(bus, T(14.0), T(17.0) + 0.2, 0.32f, 1100.0f, 1.0f, 0x34u);
    f64 going = T(CUE_GOING);
    groan(bus, send, going + 0.3, 3.0, 85.0f, 0.30f, 0.0f, 0x4bu);
    rumble(bus, going + CUE_DIVE - 0.2, T(17.0) + 0.5, 0.22f, 260.0f, 0x71u);

    /* Under the ice: the water closing, and all of it coming down. */
    f64 under = T(CUE_UNDER);
    boom(bus, send, under, 0.8f, 50.0f, 1.2f, 0x54u);
    bubbles(bus, send, under, T(19.2), 45.0f, 6.0f, 0.9f, 0x81u);
    groan(bus, send, under + 1.0, 3.0, 70.0f, 0.55f, -0.3f, 0x4cu);
    groan(bus, send, under + 4.5, 2.8, 95.0f, 0.45f, 0.4f, 0x4du);
    rumble(bus, under, T(24.5), 0.18f, 70.0f, 0x72u);

    /* DEEP: the hull working as the pressure mounts, the sparks, the bottom. */
    groan(bus, send, 84.0, 3.0, 58.0f, 0.35f, 0.2f, 0x4eu);
    groan(bus, send, 92.0, 3.5, 48.0f, 0.30f, -0.2f, 0x4fu);
    glints(bus, send, 81.0, 99.0, 3.0f, 1.0f, 0x91u);
    boom(bus, send, T(24.65), 0.9f, 32.0f, 2.6f, 0x55u);

    /* FOUND: lamps, thrusters, sonar. */
    hum(bus, T(25.0) + 0.2, SONG_LEN_SEC, 0.10f, 0xa1u);
    for (int k = 0; k < 6; k++)
        ping(bus, send, T(25.0) + 0.6 + 2.0 * k, k < 5 ? 0.55f : 0.35f, 1320.0f);
}

dm_audio *song_render(void)
{
    int frames = dm_sec_to_frames(SONG_LEN_SEC + 2.0);
    dm_audio *out   = dm_audio_new(frames);
    dm_audio *mus   = dm_audio_new(frames);
    dm_audio *msend = dm_audio_new(frames);
    dm_audio *sfx   = dm_audio_new(frames);
    dm_audio *ssend = dm_audio_new(frames);
    if (!out || !mus || !msend || !sfx || !ssend) return NULL;

    music(mus, msend);
    sound(sfx, ssend);

    /* The camera is under the water from the cut under the ice to the end of
     * the fall; in 2022 it is a microphone on a vehicle, and hears clearly. */
    muffle(sfx,   T(CUE_UNDER), T(25.0), 520.0f);
    muffle(ssend, T(CUE_UNDER), T(25.0), 520.0f);

    dm_fx_reverb(msend, 0.93f, 0.45f, 1.0f);
    dm_fx_reverb(ssend, 0.80f, 0.55f, 0.8f);

    dm_audio_mix(out, mus, 2.4f);
    dm_audio_mix(out, msend, 1.35f);
    dm_audio_mix(out, sfx, 1.3f);
    dm_audio_mix(out, ssend, 0.5f);

    dm_fx_highpass(out, 24.0f);
    dm_fx_fade(out, SONG_LEN_SEC - 1.5, SONG_LEN_SEC + 2.0, 1.0f, 0.0f);
    dm_fx_limiter(out, -1.0f, 0.004, 0.15);

    dm_audio_free(ssend);
    dm_audio_free(sfx);
    dm_audio_free(msend);
    dm_audio_free(mus);
    return out;
}
