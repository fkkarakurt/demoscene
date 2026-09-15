/* sea.h -- the water column: how light gets through it, and how it does not.
 *
 * Underwater axes are the same as above: x east, y up, z north, metres, with
 * y = 0 at the surface and depth increasing downward as y goes negative.
 */
#ifndef ENDURANCE_SEA_H
#define ENDURANCE_SEA_H

#include "demo.h"

/* Absorption, scattering and their sum, per metre, in the three channels the
 * renderer carries: red, green and blue light standing for about 610, 540 and
 * 460 nanometres. */
extern const v3 EN_WATER_ABS;
extern const v3 EN_WATER_SCAT;
extern const v3 EN_WATER_EXT;

/* Diffuse attenuation of daylight with depth, per metre. */
extern const v3 EN_WATER_KD;

/* Daylight at a depth, relative to just under the surface. What this returns
 * at two hundred metres -- blue, and a few thousandths -- is the whole story
 * of the fourth act. */
v3 en_daylight_at(f32 depth_m);

/* How much of the light from something `dist` metres away survives the trip. */
static inline v3 en_water_trans(f32 dist)
{
    return V3(expf(-EN_WATER_EXT.x * dist), expf(-EN_WATER_EXT.y * dist), expf(-EN_WATER_EXT.z * dist));
}

/* A lamp on a vehicle: a point with a cone. */
typedef struct {
    v3  pos;
    v3  dir;
    f32 cos_inner, cos_outer;
    v3  power;          /* radiant intensity on the axis, per steradian      */
} en_lamp;

/* Light a lamp puts onto a surface point, with the water between them taken
 * into account. Shadows are the caller's business. */
v3 en_lamp_irradiance(const en_lamp *L, v3 p, v3 n);

/* Light scattered toward the camera by the water along a ray, from a set of
 * lamps: the beams you see in murky water and the reason a lamp underwater
 * shows its own cone. Also returns the ambient in-scatter from daylight at
 * the ray's depth, which is `depth_bias - ro.y`. */
v3 en_water_inscatter(v3 ro, v3 rd, f32 t_end, f32 depth_bias, const en_lamp *lamps, int nlamps,
                      v3 ambient, u32 seed);

#endif /* ENDURANCE_SEA_H */
