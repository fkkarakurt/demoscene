/* song.h -- the arrangement of LIFTOFF.
 *
 * 112 BPM, 4/4. One bar is 2.142857 s, so 14 bars is 30.000 seconds on the
 * nose. Slower than RUNTIME on purpose: this piece is about mass leaving the
 * ground, and a fast tempo makes a heavy thing look light.
 *
 * The tempo is also the clock. Mission time advances by a fixed number of
 * seconds per beat -- one second per beat during the count, two during the
 * ascent, eight while climbing out of the atmosphere -- and the rate changes
 * only where the picture cuts. So every event in the flight lands on a beat
 * because it was placed in beats, not because it was nudged there.
 */
#ifndef LIFTOFF_SONG_H
#define LIFTOFF_SONG_H

#include "../../engine/dm_audio.h"

#define SONG_BPM      112.0
#define SONG_BARS     14
#define SONG_BEAT_SEC (60.0 / SONG_BPM)                /* 0.535714 s */
#define SONG_BAR_SEC  (4.0 * SONG_BEAT_SEC)            /* 2.142857 s */
#define SONG_STEP_SEC (SONG_BAR_SEC / 16.0)            /* one sixteenth */
#define SONG_LEN_SEC  (SONG_BARS * SONG_BAR_SEC)       /* 30.0 s */

static inline f64 song_bar(f64 bar)   { return bar * SONG_BAR_SEC; }
static inline f64 song_beat(f64 beat) { return beat * SONG_BEAT_SEC; }

/* Sections, in order. Each one is a different shot and a different clock rate,
 * so the cut, the arrangement and the change of pace all happen on the same
 * downbeat by construction. */
typedef enum {
    SEC_HOLD,    /* bar 0  : the count. Ignition three seconds before the drop. */
    SEC_LIFT,    /* bar 2  : release. Kick and bass land on the vehicle moving. */
    SEC_CLIMB,   /* bar 5  : full arrangement, out through the atmosphere.      */
    SEC_STAGE,   /* bar 9  : cut-off. Everything drops -- there is no air.      */
    SEC_ORBIT,   /* bar 12 : one chord over the limb, decaying into the loop.   */
    SEC_COUNT
} song_section;

extern const int SONG_SECTION_BAR[SEC_COUNT + 1];   /* +1 = end sentinel (14) */

song_section song_section_at(f64 t);
f64          song_section_start(song_section s);
f64          song_section_end(song_section s);
f64          song_section_phase(f64 t);

/* Renders the whole track. Caller owns the buffer. */
dm_audio *song_render(void);

#endif /* LIFTOFF_SONG_H */
