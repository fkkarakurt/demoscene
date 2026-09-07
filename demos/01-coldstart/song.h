/* song.h -- the arrangement of COLD START.
 *
 * The music is written first and the visuals are hung off it, not the other
 * way round. Every scene change in timeline.c lands on one of the section
 * boundaries declared here, which is why the cuts feel like they belong to
 * the track instead of merely coinciding with it.
 *
 * 120 BPM, 4/4. One bar is exactly 2 seconds, so 60 bars is 120 seconds.
 */
#ifndef COLDSTART_SONG_H
#define COLDSTART_SONG_H

#include "../../engine/dm_audio.h"

#define SONG_BPM      120.0
#define SONG_BARS     60
#define SONG_BAR_SEC  (4.0 * 60.0 / SONG_BPM)          /* 2.0 s */
#define SONG_STEP_SEC (SONG_BAR_SEC / 16.0)            /* one sixteenth */
#define SONG_LEN_SEC  (SONG_BARS * SONG_BAR_SEC)       /* 120.0 s */

static inline f64 song_bar(f64 bar)   { return bar * SONG_BAR_SEC; }
static inline f64 song_beat(f64 beat) { return beat * 60.0 / SONG_BPM; }

/* Sections, in order. The bar each one starts on is SONG_SECTION_BAR[i]. */
typedef enum {
    SEC_INTRO_PAD,   /* bar 0  : darkness, one pad, nothing else            */
    SEC_INTRO_KICK,  /* bar 4  : the pulse arrives                          */
    SEC_GROOVE,      /* bar 8  : bass and hats, the machine starts moving   */
    SEC_LEAD_A,      /* bar 16 : arp and the first statement of the theme   */
    SEC_BREAK,       /* bar 24 : everything drops away                      */
    SEC_BUILD,       /* bar 28 : snare roll and riser                       */
    SEC_DROP,        /* bar 32 : full arrangement, the centrepiece          */
    SEC_CLIMAX,      /* bar 48 : octave lead, toms, maximum density         */
    SEC_OUTRO,       /* bar 56 : stripped back, fade                        */
    SEC_COUNT
} song_section;

extern const int SONG_SECTION_BAR[SEC_COUNT + 1];   /* +1 = end sentinel (60) */

song_section song_section_at(f64 t);
f64          song_section_start(song_section s);
f64          song_section_end(song_section s);

/* Progress 0..1 through the section containing t. */
f64 song_section_phase(f64 t);

/* Renders the whole track. Caller owns the buffer. */
dm_audio *song_render(void);

#endif /* COLDSTART_SONG_H */
