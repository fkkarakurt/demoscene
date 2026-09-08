/* flight.h -- the trajectory.
 *
 * Nothing on screen is keyframed. The vehicle's altitude, speed, attitude,
 * mass and the light its engines throw all come out of one integration of a
 * two-degree-of-freedom launch, run once at startup and sampled by every
 * camera and every readout after that.
 *
 * That is the point of doing it this way. A keyframed rocket has to be told to
 * pitch over, told to slow down in the thick air and told to speed up when it
 * gets thin. This one pitches over because a gravity turn does that, loses a
 * hundred metres a second to drag because drag is in the sum, and its
 * acceleration climbs through the burn because the tanks are emptying. The
 * numbers in the corner of the frame are the same numbers steering the camera.
 *
 * The vehicle is a generic medium-lift two-stage kerosene/oxygen launcher --
 * 549 t, 3.66 m across, nine engines on the first stage and one vacuum engine
 * on the second. Those are ordinary numbers for the class and not a model of
 * anyone's actual rocket.
 */
#ifndef LIFTOFF_FLIGHT_H
#define LIFTOFF_FLIGHT_H

#include "../../engine/dm_base.h"

/* ---- the vehicle, in metres and kilograms -------------------------------- */

#define LO_RADIUS     1.83f      /* body radius                              */
#define LO_H_STAGE1   42.6f      /* engine plane to the top of the tanks     */
#define LO_H_INTER     6.4f      /* interstage: black, and where it splits   */
#define LO_H_STAGE2   13.5f      /* second stage and fairing                 */
#define LO_HEIGHT     (LO_H_STAGE1 + LO_H_INTER + LO_H_STAGE2)   /* 62.5 m   */

#define LO_T_IGNITION   (-3.0)   /* engines light three seconds before release */
#define LO_T_LIFTOFF      0.0
#define LO_T_MECO       152.0
#define LO_T_SEP        155.0
#define LO_T_S2_START   160.0
#define LO_T_SECO       536.0

typedef struct {
    f64 t;           /* mission time, seconds; negative before release      */
    f32 alt;         /* altitude above the pad, metres                      */
    f32 downrange;   /* metres                                              */
    f32 vel;         /* speed, m/s                                          */
    f32 vy, vx;      /* vertical and downrange components                   */
    f32 pitch;       /* radians from vertical, thrust direction             */
    f32 mass;        /* kg                                                  */
    f32 q;           /* dynamic pressure, Pa                                */
    f32 mach;
    f32 accel;       /* thrust minus drag over mass, m/s^2                  */
    f32 throttle;    /* 0..1                                                */
    int stage;       /* 1 before separation, 2 after                        */
} lo_state;

/* Runs the integration. Cheap (a few million floating point operations) but
 * not thread safe, so main() calls it before any rendering starts. */
void lo_flight_init(void);

/* Linear interpolation into the integrated table. Pure, so scene code may
 * call it from any thread and at any time in any order. */
lo_state lo_flight_at(f64 mission_t);

/* Mission time of maximum dynamic pressure, found in the table rather than
 * assumed -- if the model changes, the caption moves with it. */
f64 lo_flight_maxq(void);
f32 lo_flight_maxq_pa(void);

/* The integration read the other way round: the mission time at which the
 * vehicle passed a given altitude. The exhaust trail needs it -- a parcel of
 * smoke hanging at nine hundred metres is as old as the time since the vehicle
 * was there, and how old it is decides how wide and how faint it has become. */
f64 lo_flight_time_of_alt(f32 alt);
f32 lo_flight_downrange_of_alt(f32 alt);

/* Atmosphere, shared with the sky: density in kg/m^3 at `alt` metres. */
f32 lo_air_density(f32 alt);

#endif /* LIFTOFF_FLIGHT_H */
