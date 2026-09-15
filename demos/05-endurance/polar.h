/* polar.h -- everything above the water: the pack, the ship on it, and the
 * light that falls on both.
 *
 * Every shot of the first three acts is this one pixel function with a
 * different camera, a different date and a different state of the ship. The
 * light is never set per shot: it is the sun and the moon for that date and
 * hour, and the ship's own lamps when they were burning.
 */
#ifndef ENDURANCE_POLAR_H
#define ENDURANCE_POLAR_H

#include "demo.h"
#include "sky.h"
#include "ice.h"
#include "ship.h"
#include "sea.h"

#define POLAR_MAX_LAMPS 6

typedef struct {
    en_view      v;
    const en_sky *sky;
    en_ice       ice;

    int          has_ship;
    en_ship_pose pose;
    en_ship_state st;

    en_lamp      lamps[POLAR_MAX_LAMPS];   /* world space                  */
    int          nlamps;
    f32          air_glow;      /* how much the lamps light the air around them */

    /* White water where something has gone down through a lead. */
    f32          foam;
    v2           foam_at;
    f32          foam_radius;

    v3           sun_irr;       /* direct sunlight at the ground           */
    v3           moon_irr;      /* direct moonlight at the ground          */
} polar_view;

/* Fills in the lighting terms that depend only on the sky. */
void polar_view_light(polar_view *v);

/* The ship's lamps, moved into the world. */
void polar_view_ship_lamps(polar_view *v);

v3 polar_px(const en_ray *r, const void *vp, f32 *t_hit);

/* For en_ice.hull_fn, with the view as its user data. */
f32 polar_hull_fn(f32 x, f32 z, void *ud);

/* The rigging, moved into the world and lit, for en_lines_draw. */
int polar_rigging(const polar_view *v, en_line *out, int cap);

/* ---- a whole shot ------------------------------------------------------------ */

/* Fills in one exposure slice's view: camera, ship, ice, lamps. The helper has
 * already zeroed it and pointed its sky at `sky`. */
typedef void (*polar_setup_fn)(polar_view *v, const en_ctx *c, f64 t, void *ud);

typedef struct {
    en_sky       *sky;
    polar_setup_fn setup;
    void         *ud;
    v2            bake_centre;
    u64           bake_key;     /* change it whenever the pack has changed    */
    int           two_bakes;    /* a second bake, for a subject far off       */
    v2            bake2_centre;
    /* The second bake's key, when what is near it changes less often than
     * what is near the first; 0 to follow the first. */
    u64           bake2_key;
} polar_shot;

void polar_render(dm_fb *fb, f32 *depth, const en_ctx *c, const polar_shot *shot);

/* How far a mast has fallen, t seconds after it let go: a uniform spar
 * pivoting about its foot under gravity, from a small initial lean, stopped
 * where it meets whatever it lands on. */
f32 polar_fall_angle(f32 t, f32 length, f32 lean0, f32 stop);

/* A shot's position in its own time, 0..1, for slice time t. */
f32 polar_shot_u(const en_ctx *c, f64 t);

#endif /* ENDURANCE_POLAR_H */
