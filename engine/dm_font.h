/* dm_font.h -- stroke font, rendered as a distance field.
 *
 * Every glyph is a handful of line segments typed out by hand in a 6x10 grid
 * (see the tables in dm_font.c). Nothing is loaded from disk, and there is no
 * bitmap: a pixel's coverage is a function of its distance to the nearest
 * segment. That buys three things a bitmap font cannot give:
 *
 *   - it stays crisp at any size, so the logo and the scroller share one face;
 *   - antialiasing is exact and free, straight out of the distance;
 *   - the same distance drives a glow halo, so text can be a light source.
 *
 * The face is deliberately all-straight-line: it reads as something engraved
 * rather than typeset, which is the register this series is going for.
 */
#ifndef DM_FONT_H
#define DM_FONT_H

#include "dm_fb.h"

typedef struct {
    f32 size;       /* cap height in pixels */
    f32 weight;     /* stroke half-width in pixels */
    f32 tracking;   /* extra letter spacing, in fractions of `size` */
    v3  color;      /* linear light; push past 1.0 to make it bloom */
    f32 glow;       /* halo falloff distance in pixels, 0 disables */
    f32 glow_gain;  /* halo brightness relative to `color` */
    int additive;   /* 1 = add light, 0 = alpha blend over the background */
} dm_text;

dm_text dm_text_default(void);

/* Horizontal advance of one character, in pixels. Monospaced by design --
 * proportional spacing would need kerning data this project has no room for,
 * and a fixed grid suits the engraved look. */
f32 dm_text_advance(const dm_text *st);
f32 dm_text_width(const char *s, const dm_text *st);

/* `pen` is the baseline origin in pixel coordinates, y pointing down.
 * `angle` rotates the glyph about that origin, in radians. */
void dm_char_draw(dm_fb *fb, int c, v2 pen, f32 angle, const dm_text *st);

void dm_text_draw(dm_fb *fb, v2 pen, const char *s, const dm_text *st);
void dm_text_draw_centered(dm_fb *fb, v2 center, const char *s, const dm_text *st);

#endif /* DM_FONT_H */
