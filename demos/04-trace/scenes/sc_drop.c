/* sc_drop.c -- bars 8 to 12: everything at once, seen from beside.
 *
 * Turn the camera a quarter turn and one channel collapses into depth. What is
 * left is the other channel against time -- the oscilloscope's other mode --
 * and the kick, which from in front is a spiral, is from here a funnel. The
 * frame is upright, so the time axis is too: the present near the foot of the
 * frame and the last second and a half climbing away above it.
 *
 * Five bars, five views of the same object: from the right, where it is the
 * right channel against time; from three quarters, where both channels show
 * and the funnels read as solids; from the left, mirrored; three quarters
 * again from the other side; and last from high overhead, the left channel
 * against time, the whole of it turning slowly as the drop runs out.
 */
#include "../demo.h"

void scene_drop(dm_fb *fb, const tr_ctx *c)
{
    f64 bar = c->bar - SONG_SECTION_BAR[SEC_DROP];
    tr_beam b = tr_beam_default(c);
    b.speed   = 2.6f;
    b.history = 1.5f;
    b.scale   = 0.85f;

    tr_camera cam = tr_camera_front(2.3f);
    f32 u = (f32)(bar - floor(bar));
    int k = (int)bar;

    /* Where the middle of the frame sits on the time axis. At 2.25 units the
     * frame is 1.7 units tall either side of centre, so this puts the present
     * at 72% of the height -- low, but clear of the bottom fifth, which on a
     * phone is the title and the channel row. */
    f32 zc = -0.75f;

    switch (k) {
    case 0:
    case 2: {
        f32 side = k == 0 ? 1.0f : -1.0f;
        f32 yaw  = side * dm_lerp(86.0f, 78.0f, u) * DM_D2R;
        cam.eye    = tr_orbit(2.25f, yaw, 4.0f * DM_D2R, zc);
        cam.target = V3(0.0f, 0.0f, zc);
        cam.roll   = -side * DM_HALFPI;
        cam.focus  = 2.25f;
        break;
    }
    case 1:
    case 3: {
        /* Three quarters, from one side and then the other: both channels at
         * once, and the funnels with enough depth to read as solids. */
        f32 side = k == 1 ? -1.0f : 1.0f;
        f32 yaw  = side * dm_lerp(52.0f, 44.0f, u) * DM_D2R;
        cam.eye    = tr_orbit(2.5f, yaw, -6.0f * DM_D2R, zc);
        cam.target = V3(0.0f, 0.0f, zc);
        cam.roll   = -side * DM_HALFPI * 0.80f;
        cam.focus  = 2.4f;
        break;
    }
    default:
        /* Straight down from high above: screen up is the past. */
        b.speed = 2.6f;
        cam.eye    = V3(0.0f, 3.2f, -1.6f);
        cam.target = V3(0.0f, 0.0f, -1.6f);
        cam.roll   = dm_lerp(0.0f, 0.5f, tr_ease(0.0f, 1.0f, u));
        cam.focus  = 3.2f;
        break;
    }

    tr_expose(fb, c, &cam, &b);
}
