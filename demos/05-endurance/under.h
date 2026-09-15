/* under.h -- everything below the surface: under the ice, the fall, the
 * dark, and the seabed where she lies.
 *
 * Same axes as above: x east, y up, z north, with the sea surface at y = 0.
 */
#ifndef ENDURANCE_UNDER_H
#define ENDURANCE_UNDER_H

#include "demo.h"
#include "sea.h"
#include "ship.h"
#include "ice.h"

#define UNDER_MAX_LAMPS 4

typedef struct {
    en_view      v;

    /* Daylight. `sun_irr` is sunlight just under the surface of open water,
     * `sky_irr` the diffuse light from the rest of the sky, and `sun_up` the
     * direction toward the sun after refraction, which is steeper than the
     * sun itself: Snell's law folds the whole sky into a cone 97 degrees
     * across. Zero both in the deep. */
    v3           sun_irr, sky_irr;
    v3           sun_up;

    /* The ice overhead, for the shot beneath it. */
    int          has_ice;
    en_ice       ice;

    int          has_ship;
    en_ship_pose pose;
    en_ship_state st;

    /* The seabed, a plane with silt on it, for the wreck. */
    int          has_seabed;
    f32          seabed_y;

    /* Anything else a scene puts in the water -- the life on the wreck, the
     * things around it on the bottom -- as a distance function inside a box. */
    f32        (*extra_sdf)(v3 p, void *ud, int *mat);
    v3         (*extra_albedo)(v3 p, v3 n, int mat, void *ud);
    void        *extra_ud;
    v3           extra_lo, extra_hi;

    en_lamp      lamps[UNDER_MAX_LAMPS];
    int          nlamps;

    f32          god_rays;      /* 0..1: how much the shafts of light count  */

    /* How deep y = 0 is. Zero for a scene at the surface; on the way down the
     * scene keeps its own few hundred metres of water round the ship while the
     * light is the light of the depth the gauge reads. */
    f32          depth_bias;

    /* Below the last of the daylight, with no lamps: nothing can be seen, and
     * the ship is marched only for the depth that hides what is behind it. */
    int          dark;
} under_view;

/* Fills in the daylight terms for a sun at `sun` (in air) over open water. */
void under_view_daylight(under_view *v, v3 sun, f32 sun_power);

v3 under_px(const en_ray *r, const void *vp, f32 *t_hit);

typedef void (*under_setup_fn)(under_view *v, const en_ctx *c, f64 t, void *ud);

typedef struct {
    under_setup_fn setup;
    void          *ud;
    f32            snow;        /* marine snow per cubic metre               */
    f32            snow_drift;  /* how fast it moves past the lens, m/s up   */
    int            bake_ice;
    v2             bake_centre;
    u64            bake_key;
} under_shot;

void under_render(dm_fb *fb, f32 *depth, const en_ctx *c, const under_shot *shot);

#endif /* ENDURANCE_UNDER_H */
