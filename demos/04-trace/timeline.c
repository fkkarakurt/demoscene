#include "demo.h"

/* ---- the soundtrack -------------------------------------------------------- */

static const dm_audio *g_audio = NULL;

void            tr_set_audio(const dm_audio *a) { g_audio = a; }
const dm_audio *tr_audio(void)                  { return g_audio; }

/* ---- context --------------------------------------------------------------- */

tr_ctx tr_ctx_make(f64 t, f64 t0, f64 t1, f64 shutter, int frame, int w, int h)
{
    tr_ctx c;
    c.t       = t;
    c.t0      = t0;
    c.t1      = t1;
    c.shutter = shutter;
    c.frame   = frame;
    c.w       = w;
    c.h       = h;
    c.aspect  = (f32)w / (f32)h;

    /* The musical clocks run off the middle of the slice, so a camera that
     * moves on the beat moves during the exposure and blurs as it should. */
    f64 tm = 0.5 * (t0 + t1);
    c.bar  = tm / SONG_BAR_SEC;
    c.beat = tm / SONG_BEAT_SEC;

    f64 bf = c.beat - floor(c.beat);
    f64 rf = c.bar  - floor(c.bar);
    c.beat_hit = expf(-(f32)bf * 8.0f);
    c.bar_hit  = expf(-(f32)rf * 3.2f);

    c.sec       = song_section_at(tm);
    c.sec_t     = tm - song_section_start(c.sec);
    c.sec_phase = song_section_phase(tm);
    return c;
}

/* ---- defaults -------------------------------------------------------------- */

tr_beam tr_beam_default(const tr_ctx *c)
{
    tr_beam b;
    b.t0      = c->t0;
    b.t1      = c->t1;
    b.shutter = c->shutter;

    b.scale   = 1.0f;

    /* Three units of depth per second: at the default distance the present
     * fills the frame and a second ago is a third of its size. */
    b.speed   = 3.0f;
    b.history = 1.4f;
    b.gain    = 5.0e5f;
    b.width   = 0.0022f;

    /* The flash is cold and white and gone in two milliseconds, so what it
     * marks is the beam, now. The persistence is the cyan of the house
     * palette and fades over a quarter of a second -- the last few dozen
     * cycles -- and under it an indigo that takes three times as long, so the
     * far end of the time axis cools as it recedes instead of just dimming. */
    b.fast_col = v3_scl(V3(0.85f, 0.92f, 1.00f), 26.0f);
    b.fast_tau = 0.002f;
    b.mid_col  = V3(0.06f, 0.60f, 0.60f);
    b.mid_tau  = 0.25f;
    b.slow_col = V3(0.15f, 0.05f, 0.48f);
    b.slow_tau = 0.75f;
    return b;
}

tr_camera tr_camera_front(f32 dist)
{
    tr_camera cam;
    cam.eye      = V3(0.0f, 0.0f, dist);
    cam.target   = V3(0.0f, 0.0f, 0.0f);
    cam.roll     = 0.0f;
    /* Wide enough that full scale on the left channel, one unit either side,
     * sits just inside a 9:16 frame at 2.6 units away. */
    cam.fov      = 74.0f * DM_D2R;
    cam.focus    = dist;
    cam.aperture = 0.05f;
    cam.shift    = V2(0.0f, 0.0f);
    return cam;
}

void tr_expose(dm_fb *fb, const tr_ctx *c, const tr_camera *cam, const tr_beam *b)
{
    (void)c;
    tr_beam_draw(fb, tr_audio(), cam, b);
}

/* ---- dispatch -------------------------------------------------------------- */

void demo_frame(dm_fb *fb, const tr_ctx *c)
{
    switch (c->sec) {
    case SEC_TUNE:  scene_tune(fb, c);  break;
    case SEC_PULSE: scene_pulse(fb, c); break;
    case SEC_CHAOS: scene_chaos(fb, c); break;
    case SEC_DROP:  scene_drop(fb, c);  break;
    default:        scene_name(fb, c);  break;
    }
}

void demo_finish(dm_fb *fb, const tr_ctx *c)
{
    (void)c;
    tr_phosphor_saturate(fb, 7.0f);
}

dm_post_params demo_post(const tr_ctx *c)
{
    dm_post_params p = dm_post_defaults();

    /* Lines of light on black. The bloom is most of what makes a trace look
     * like light rather than like a drawing of it, and grain has nothing to
     * sit on: it is scaled by brightness, and nearly everything is black. */
    p.bloom_threshold = 0.9f;
    p.bloom_knee      = 0.7f;
    p.bloom_intensity = 0.20f;
    p.bloom_radius    = 0.80f;
    p.barrel          = 0.012f;
    p.chroma          = 0.0018f;
    p.vignette        = 0.30f;
    p.grain           = 0.008f;

    p.bloom_intensity *= 1.0f + 0.25f * c->beat_hit * (c->sec == SEC_PULSE || c->sec == SEC_DROP);
    p.frame = (u32)c->frame;
    return p;
}
