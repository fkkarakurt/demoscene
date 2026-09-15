/* sky.h -- the sun, the air, the stars and the aurora, over the Weddell Sea.
 *
 * World axes for every scene above the water: x east, y up, z north, metres,
 * with the camera's patch of sea ice at the origin.
 */
#ifndef ENDURANCE_SKY_H
#define ENDURANCE_SKY_H

#include "demo.h"

/* Where the sun is, from nothing but a latitude, a day of the year and the
 * local solar time. */
v3 en_sun_dir(f32 lat_deg, f32 day_of_year, f32 solar_hour);

/* The Julian day of a Gregorian date and a time in UT. */
f64 en_julian(int year, int month, int day, f64 hour_ut);

/* The sun and the moon for a moment and a place, from the low-precision
 * formulae of the Astronomical Almanac: good to a fraction of a degree, which
 * is a great deal better than the eye can tell. Longitude is east-positive.
 * `illum` receives the lit fraction of the moon's disc. The light in every
 * shot on the ice comes from these, and the dates in the captions are the
 * dates they are given. */
v3 en_sun_at(f64 jd, f32 lat_deg, f32 lon_deg);
v3 en_moon_at(f64 jd, f32 lat_deg, f32 lon_deg, f32 *illum);

#define EN_SKY_RING 64

typedef struct {
    v3  sun;            /* unit vector toward the sun                        */
    f32 sun_power;      /* scales every sunlit thing                         */
    v3  moon;           /* unit vector toward the moon                       */
    f32 moon_illum;     /* lit fraction of its disc; 0 leaves it out         */
    f32 aurora;         /* 0 = none                                          */
    f32 stars;          /* 0..1, how many survive the exposure               */
    f64 t;              /* animates the aurora                               */
    f32 alt;            /* viewer height, metres                             */
    /* Irradiance on a surface, filled in by en_sky_prepare: the sky
     * integrated over the hemisphere around each of six axes. */
    v3  amb[6];
    v3  aur_dir;        /* where the aurora's light comes from, and its colour */
    v3  aur_col;
    v3  ring[EN_SKY_RING];   /* the horizon, all the way round           */

    /* The air, tabulated once a frame by en_sky_prepare. Zero the struct
     * before first use; the table is kept and reused while the lights are
     * where they were. */
    f32 *env;
    int  env_valid;
    v3   env_sun, env_moon;
    f32  env_power, env_illum;
} en_sky;

/* Integrates the ambient terms. Once per frame, not per pixel. */
void en_sky_prepare(en_sky *s);

/* Light arriving from direction rd, which must point above the horizon for
 * anything but black to come back. */
v3 en_sky_radiance(const en_sky *s, v3 rd, u32 seed);

/* Light from the whole sky onto a surface with normal n. */
v3 en_sky_ambient(const en_sky *s, v3 n);

/* How much sunlight survives the air along a direction toward the sun, per
 * channel: white overhead, red on the horizon, nothing below it. */
v3 en_sun_transmittance(const en_sky *s);

/* Moonlight arriving at the ground, as a colour and a brightness in the same
 * units as sunlight: the sun's light, reflected by a grey rock at a quarter of
 * a million miles, and dimmed by phase far faster than the lit fraction. */
v3 en_moonlight(const en_sky *s);

/* Fog: what a ray of length `dist` along `rd` picks up from the air between
 * the camera and what it hit, and how much of that thing survives. */
v3 en_air(const en_sky *s, v3 rd, f32 dist, v3 behind);

#endif /* ENDURANCE_SKY_H */
