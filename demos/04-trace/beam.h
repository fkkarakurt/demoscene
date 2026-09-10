/* beam.h -- the soundtrack, drawn the way the beam of an oscilloscope draws it,
 * with one addition: the past is given somewhere to lie.
 *
 * World space is the face of the scope. x is the left channel and y the
 * right, one unit to full scale. The present is the plane z = 0 and the past
 * lies behind it, a sample of age a at z = -a * speed. So:
 *
 *   - seen from straight in front, depth collapses and what is left is exactly
 *     the XY display: left channel across, right channel up;
 *   - seen from the side, x collapses and what is left is the right channel
 *     plotted against time, which is the scope's other mode;
 *   - seen from above, the left channel against time.
 *
 * One object, and every view of it is a true plot of the audio.
 *
 * The light is a beam's light. It is deposited in proportion to how long the
 * beam spends somewhere, not to how far it goes, so a slow stretch of trace is
 * bright and a fast one is faint and a beam that is not moving at all burns a
 * point. The phosphor is a sum of exponential decays with a colour each, as a
 * real one is: a fast flash that is gone in milliseconds and marks where the
 * beam is now, a persistence that carries the figure, and a long faint tail
 * in a different colour, so age can be read off the trace as well as depth.
 */
#ifndef TRACE_BEAM_H
#define TRACE_BEAM_H

#include "../../engine/dm_fb.h"
#include "../../engine/dm_audio.h"

typedef struct {
    v3  eye, target;
    f32 roll;          /* radians about the view direction                  */
    f32 fov;           /* vertical field of view, radians                   */
    f32 focus;         /* distance to the plane of sharp focus              */
    f32 aperture;      /* lens diameter in world units; 0 is a pinhole      */
    v2  shift;         /* lens shift, in fractions of the frame height      */
} tr_camera;

typedef struct {
    f64 t0, t1;        /* this slice of the exposure, audio seconds          */
    f64 shutter;       /* the whole exposure the slices add up to           */

    /* World units per unit of signal, on both axes alike: the volts-per-
     * division knob. It is what lets the mix be balanced by ear and the
     * picture be framed by eye -- a quiet section is not a small one, it is
     * a section with the gain turned up, exactly as it would be on a bench. */
    f32 scale;
    f32 speed;         /* depth per second of age, world units              */
    f32 history;       /* seconds of the past drawn at all                  */
    f32 gain;          /* beam current: light per second of trace           */
    f32 width;         /* spot sigma on the face, world units               */

    v3  fast_col;      /* the flash, as light captured per second of trace  */
    f32 fast_tau;      /*   ... and how quickly it dies, seconds            */
    v3  mid_col;       /* the persistence, as brightness when fresh         */
    f32 mid_tau;
    v3  slow_col;      /* and a longer, dimmer tail under it, which is what */
    f32 slow_tau;      /* colours the far end of the time axis              */
} tr_beam;

/* Scratch for the largest frame that will be drawn. Allocated once. */
int  tr_beam_init(void);
void tr_beam_free(void);

/* Adds one slice of exposure to `fb`. Slices are summed, not averaged: light
 * captured with the shutter open for twice as long is twice the light, and
 * the division by the shutter happens inside, once, per slice. */
void tr_beam_draw(dm_fb *fb, const dm_audio *a, const tr_camera *cam, const tr_beam *b);

/* A phosphor cannot get brighter without limit. Applied once to the finished
 * exposure: light approaches `level` and never passes it, channel by channel,
 * so the core of a very bright trace runs to white. */
void tr_phosphor_saturate(dm_fb *fb, f32 level);

#endif /* TRACE_BEAM_H */
