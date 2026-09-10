/* sc_tune.c -- bars 0 and 1: a dot, and then a chord assembling itself.
 *
 * The camera is on the axis of time, looking straight down it, so depth
 * collapses completely and the picture is exactly the XY display. Silence on
 * that axis is a single point -- the beam parked in the middle, the whole of
 * its history stacked behind it -- which is why frame one is a dot.
 */
#include "../demo.h"

void scene_tune(dm_fb *fb, const tr_ctx *c)
{
    f32 u = (f32)c->sec_phase;

    tr_camera cam = tr_camera_front(dm_lerp(2.30f, 2.05f, tr_ease(0.0f, 1.0f, u)));

    tr_beam b = tr_beam_default(c);
    b.speed = 2.4f;
    /* The quietest section, so the gain is up: the intro is mixed to sit
     * under what follows it and framed to fill the screen anyway. */
    b.scale = 1.8f;

    tr_expose(fb, c, &cam, &b);
}
