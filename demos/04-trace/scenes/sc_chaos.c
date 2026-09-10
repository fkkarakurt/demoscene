/* sc_chaos.c -- bars 5 to 7: the bass is the Lorenz system.
 *
 * Its x goes to the left channel and its z to the right, so from in front the
 * picture is the butterfly -- not a drawing of it, the trajectory itself,
 * integrated at audio rate and heard as a growl. From in front because that is
 * the view in which the claim can be checked.
 *
 * Everything else in the piece closes on itself. This is the one figure that
 * never does, so it is the build: the camera drifts across it for two bars and
 * then, under the snare roll, pushes in until the wings are wider than the
 * frame.
 */
#include "../demo.h"

void scene_chaos(dm_fb *fb, const tr_ctx *c)
{
    f64 bar = c->bar - SONG_SECTION_BAR[SEC_CHAOS];
    tr_beam b = tr_beam_default(c);
    b.speed   = 2.0f;
    b.mid_tau = 0.20f;
    b.scale   = 1.15f;

    f32 drift = tr_ease(0.0f, 3.0f, (f32)bar);
    f32 push  = tr_ease(2.0f, 3.0f, (f32)bar);
    f32 yaw   = dm_lerp(-12.0f, 12.0f, drift) * DM_D2R;
    f32 dist  = dm_lerp(dm_lerp(2.15f, 1.85f, drift), 1.30f, push * push);

    tr_camera cam = tr_camera_front(dist);
    cam.eye    = tr_orbit(dist, yaw, 3.0f * DM_D2R, 0.0f);
    cam.target = V3(0.0f, 0.0f, -0.3f);
    cam.roll   = -0.10f * push;
    cam.focus  = dist;

    tr_expose(fb, c, &cam, &b);
}
