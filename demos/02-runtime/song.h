/* song.h -- the arrangement of RUNTIME.
 *
 * Thirty seconds, which is a different problem from two minutes. A film can
 * spend eight bars earning its first kick; a short has to be interesting in
 * the first second and has to survive being watched twice in a row, because it
 * loops. So this track is written tight: four sections, no intro to speak of,
 * and a tail that lands on silence exactly where the picture returns to black.
 *
 * 128 BPM, 4/4. One bar is 1.875 seconds, so 16 bars is 30.0 seconds on the
 * nose -- the video is cut to the bar, not the bar to the video.
 */
#ifndef RUNTIME_SONG_H
#define RUNTIME_SONG_H

#include "../../engine/dm_audio.h"

#define SONG_BPM      128.0
#define SONG_BARS     16
#define SONG_BAR_SEC  (4.0 * 60.0 / SONG_BPM)          /* 1.875 s */
#define SONG_STEP_SEC (SONG_BAR_SEC / 16.0)            /* one sixteenth */
#define SONG_LEN_SEC  (SONG_BARS * SONG_BAR_SEC)       /* 30.0 s */

static inline f64 song_bar(f64 bar)   { return bar * SONG_BAR_SEC; }
static inline f64 song_beat(f64 beat) { return beat * 60.0 / SONG_BPM; }

/* Sections, in order. Each one is a different job on screen, so the cut and
 * the arrangement change at the same instant by construction. */
typedef enum {
    SEC_ASSEMBLE,   /* bar 0  : no drums. The picture is being computed.     */
    SEC_LOCK,       /* bar 2  : kick and bass land on the finished frame.    */
    SEC_DRIVE,      /* bar 6  : full arrangement, the second camera.         */
    SEC_EXPOSE,     /* bar 11 : everything drops. The waveform draws itself. */
    SEC_SIGN,       /* bar 14 : one last chord, decaying into the loop point.*/
    SEC_COUNT
} song_section;

extern const int SONG_SECTION_BAR[SEC_COUNT + 1];   /* +1 = end sentinel (16) */

song_section song_section_at(f64 t);
f64          song_section_start(song_section s);
f64          song_section_end(song_section s);
f64          song_section_phase(f64 t);

/* Renders the whole track. Caller owns the buffer. */
dm_audio *song_render(void);

#endif /* RUNTIME_SONG_H */
