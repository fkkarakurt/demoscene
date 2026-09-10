/* sc_pulse.c -- bars 2 to 5: the kick arrives, and every kick is a spiral.
 *
 * Two bars from in front, with the time axis running fast: the present fills
 * the frame and each beat's spiral is pulled away down the middle before the
 * next one lands, so what reads is one kick and one chord at a time, a ring
 * flying off behind them. Then a cut to three quarters, and the same rings are
 * seen from outside -- funnels, one a beat, strung along the time axis.
 */
#include "../demo.h"

void scene_pulse(dm_fb *fb, const tr_ctx *c)
{
    f64 bar = c->bar - SONG_SECTION_BAR[SEC_PULSE];
    tr_beam b = tr_beam_default(c);
    b.scale = 1.2f;
    tr_camera cam = tr_camera_front(2.2f);

    if (bar < 2.0) {
        f32 u = (f32)(bar / 2.0);
        b.speed = 9.0f;
        f32 yaw = dm_lerp(0.0f, 10.0f, tr_ease(0.0f, 1.0f, u)) * DM_D2R;
        cam.eye    = tr_orbit(2.2f, yaw, 3.0f * DM_D2R * u, 0.0f);
        cam.target = V3(0.0f, 0.0f, -0.4f * u);
    } else {
        f32 u = (f32)((bar - 2.0) / 2.0);
        b.speed = 4.0f;
        f32 yaw = dm_lerp(34.0f, 46.0f, u) * DM_D2R;
        f32 zc  = -1.3f;
        cam.eye    = tr_orbit(2.9f, yaw, 12.0f * DM_D2R, zc);
        cam.target = V3(0.0f, 0.0f, zc);
        cam.focus  = v3_len(v3_sub(cam.eye, V3(0.0f, 0.0f, -0.4f)));
    }

    tr_expose(fb, c, &cam, &b);
}
