/* ice.h -- the pack.
 *
 * Sea ice in the Weddell Sea is not a sheet. It is floes -- plates tens to
 * hundreds of metres across -- jammed together, with the seams between them
 * refrozen into thin grey new ice, and wherever two floes have been driven
 * into each other, a pressure ridge: a wall of broken blocks pushed up out of
 * the collision, a metre or two high and as long as the seam. Snow lies on
 * top of all of it in drifts the wind has carved into ribs.
 *
 * Heights are metres above the sea surface, in the sky's axes: x east, y up,
 * z north.
 */
#ifndef ENDURANCE_ICE_H
#define ENDURANCE_ICE_H

#include "demo.h"

enum { ICE_SNOW = 0, ICE_NEW, ICE_BLOCK, ICE_WATER };

typedef struct {
    f64 t;
    /* Pressure: 0 is the winter pack at rest; 1 is October, with the ridges
     * working and the floes driven together. */
    f32 pressure;
    /* How much snow has had time to fall on everything: 0 in a new lead, 1 by
     * midwinter. */
    f32 snow_cover;
    /* The hull, where the ice has to make room for it. `hull_fn` returns the
     * signed horizontal distance from a point on the sea surface to the hull's
     * outline at the waterline, positive outside. NULL for no ship. */
    f32 (*hull_fn)(f32 x, f32 z, void *ud);
    void *hull_ud;
    /* Things built on the floe -- kennels, snow walls -- as extra height:
     * returns the height there (or less than the ice to mean nothing), writes
     * the material, and the horizontal distance to the nearest of them. */
    f32 (*extra_fn)(f32 x, f32 z, void *ud, int *mat, f32 *free_dist);
    void *extra_ud;
    /* A lead: open water along a line, for the sinking. Width 0 for none. */
    v2  lead_a, lead_b;
    f32 lead_w;
    /* A pressure ridge a scene has put where it wants one: along a line,
     * with its sail this high. Height 0 for none. */
    v2  ridge_a, ridge_b;
    f32 ridge_h;
    /* The surface baked into maps, if a scene has made them. */
    const struct en_ice_bake *bake;
    const struct en_ice_bake *bake2;    /* a second, centred elsewhere     */
} en_ice;

/* ---- the baked pack -----------------------------------------------------------
 *
 * The pack as a stack of height maps, each twice the size and half the detail
 * of the last, all centred on one point: a clipmap. A ray a kilometre out asks a
 * map with ten-metre texels, a ray at the camera's feet asks the function
 * itself, and nothing in between costs more than four texels.
 */
typedef struct en_ice_bake en_ice_bake;

en_ice_bake *en_ice_bake_new(void);
void         en_ice_bake_free(en_ice_bake *B);

/* Bakes `I` around (cx, cz), unless the bake already holds `key` there. */
void en_ice_bake_build(en_ice_bake *B, const en_ice *I, f32 cx, f32 cz, u64 key);

/* Height of the surface, and what it is made of there. `lod` is the size in
 * metres of the smallest detail worth computing at this distance. */
f32 en_ice_height(const en_ice *I, f32 x, f32 z, f32 lod, int *mat);

/* Marches a ray to the surface. Returns 1 on a hit, with the distance, the
 * normal and the material. */
int en_ice_trace(const en_ice *I, v3 ro, v3 rd, f32 t_max, f32 cone,
                 f32 *t_out, v3 *n_out, int *mat_out);

/* Albedo at a surface point, and how much of the light that reaches it is
 * carried under the surface and back out again, tinted blue -- which is what
 * ice does and snow does not. */
v3 en_ice_albedo(const en_ice *I, v3 p, v3 n, int mat, f32 *translucency);

/* The sparkle of snow: a few crystals in every square decimetre happen to be
 * turned exactly between the light and the eye. */
f32 en_snow_glint(v3 p, v3 n, v3 l, v3 v, f32 cone_t);

#endif /* ENDURANCE_ICE_H */
