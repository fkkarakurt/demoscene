/* song.h -- the arrangement of TRACE.
 *
 * 120 BPM, 4/4. A bar is exactly two seconds, so fifteen bars is 30.000 s on
 * the nose and every beat lands on a half second -- a round number the camera
 * code can reason about without a calculator.
 *
 * The soundtrack of this piece is also its picture. The left channel is a
 * horizontal position and the right channel a vertical one, so everything
 * written here is drawn as well as heard, and a section is as much a choice of
 * shapes as a choice of sounds.
 */
#ifndef TRACE_SONG_H
#define TRACE_SONG_H

#include "../../engine/dm_audio.h"

#define SONG_BPM      120.0
#define SONG_BARS     15
#define SONG_BEAT_SEC (60.0 / SONG_BPM)                /* 0.5 s   */
#define SONG_BAR_SEC  (4.0 * SONG_BEAT_SEC)            /* 2.0 s   */
#define SONG_STEP_SEC (SONG_BAR_SEC / 16.0)            /* 0.125 s */
#define SONG_LEN_SEC  (SONG_BARS * SONG_BAR_SEC)       /* 30.0 s  */

static inline f64 song_bar(f64 bar)   { return bar * SONG_BAR_SEC; }
static inline f64 song_beat(f64 beat) { return beat * SONG_BEAT_SEC; }

/* Order, chaos, order. Everything in the piece closes on itself and stands
 * still except the Lorenz system, which never repeats -- so it is the build,
 * the one stretch in which nothing settles, and the drop is everything locking
 * back into place at once. */
typedef enum {
    SEC_TUNE,    /* bar 0  : a dot opens into a circle; voices enter and lock   */
    SEC_PULSE,   /* bar 2  : the kick, which is a spiral; chords on the off-beat */
    SEC_CHAOS,   /* bar 5  : the bass is a Lorenz system, and so is the picture */
    SEC_DROP,    /* bar 8  : everything, and the camera turns to show the time  */
    SEC_NAME,    /* bar 13 : a word, traced by the beam; then the dot again     */
    SEC_COUNT
} song_section;

extern const int SONG_SECTION_BAR[SEC_COUNT + 1];   /* +1 = end sentinel (15) */

song_section song_section_at(f64 t);
f64          song_section_start(song_section s);
f64          song_section_end(song_section s);
f64          song_section_phase(f64 t);

/* Renders the whole track, mastered. What this returns is what is drawn, so
 * nothing may be done to the audio after it -- no gain, no filter, no
 * limiter -- that is not also done to the picture. */
dm_audio *song_render(void);

#endif /* TRACE_SONG_H */
