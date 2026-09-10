/* sc_name.c -- bars 13 and 14: a word, traced by the beam, and heard.
 *
 * The stroke face the engine draws every caption with is nothing but line
 * segments, which is exactly what a beam can follow. Traced fifty-five times a
 * second the word is a low A, and the shape of the letters is its timbre.
 *
 * It arrives side-on, as the bar of light a repeating shape makes when time is
 * laid out behind it, and the camera swings round until the bar is end-on and
 * the word can be read. Then the sound stops and it is a dot again, which is
 * where the piece began.
 */
#include "../demo.h"

void scene_name(dm_fb *fb, const tr_ctx *c)
{
    f64 st = c->sec_t;
    tr_beam b = tr_beam_default(c);

    /* Seen end-on, a word's past recedes toward the middle of the frame and
     * crosses its own letters on the way. So the persistence here is short
     * and the time axis long: a few copies, pulled away fast. */
    b.speed    = 6.0f;
    b.mid_tau  = 0.08f;
    b.slow_tau = 0.22f;
    b.history  = 0.6f;
    b.scale    = 1.35f;

    f32 swing = tr_ease(0.15f, 1.6f, (f32)st);
    f32 yaw   = dm_lerp(78.0f, 0.0f, swing) * DM_D2R;
    f32 zc    = dm_lerp(-1.2f, 0.0f, swing);

    tr_camera cam = tr_camera_front(2.6f);
    cam.eye    = tr_orbit(2.6f, yaw, dm_lerp(10.0f, 0.0f, swing) * DM_D2R, zc);
    cam.target = V3(0.0f, 0.0f, zc);

    tr_expose(fb, c, &cam, &b);
}
