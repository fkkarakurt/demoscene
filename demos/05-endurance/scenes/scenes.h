/* scenes.h -- every shot in the film, in order. */
#ifndef ENDURANCE_SCENES_H
#define ENDURANCE_SCENES_H

#include "../demo.h"

/* Anything computed once, before the first frame. */
void scenes_init(void);

void shot_winter_wide(dm_fb *fb, f32 *depth, const en_ctx *c);
void shot_winter_ship(dm_fb *fb, f32 *depth, const en_ctx *c);
void shot_pressure_night(dm_fb *fb, f32 *depth, const en_ctx *c);
void shot_pressure_masts(dm_fb *fb, f32 *depth, const en_ctx *c);
void shot_sinking_far(dm_fb *fb, f32 *depth, const en_ctx *c);
void shot_sinking_under(dm_fb *fb, f32 *depth, const en_ctx *c);
void shot_deep_fall(dm_fb *fb, f32 *depth, const en_ctx *c);
void shot_deep_dark(dm_fb *fb, f32 *depth, const en_ctx *c);
void shot_found_wheel(dm_fb *fb, f32 *depth, const en_ctx *c);
void shot_found_stern(dm_fb *fb, f32 *depth, const en_ctx *c);

/* The depth the camera is at during act four, for the gauge. */
f32 en_deep_depth(f64 t);

#endif /* ENDURANCE_SCENES_H */
