/* song.h -- the arrangement of ENDURANCE.
 *
 * 60 BPM, 4/4, thirty bars: exactly two minutes. At twenty-four frames a
 * second a beat is twenty-four frames and a bar is ninety-six, so every cut
 * that lands on a beat lands on a frame boundary as well, with nothing rounded.
 *
 * The acts are sections, and each one is a different light as well as a
 * different part of the story: the winter night the ship was trapped through,
 * the spring the ice broke her, the afternoon she sank, the water she sank
 * into, and the lamps that found her.
 */
#ifndef ENDURANCE_SONG_H
#define ENDURANCE_SONG_H

#include "../../engine/dm_audio.h"

#define SONG_BPM      60.0
#define SONG_BARS     30
#define SONG_BEAT_SEC (60.0 / SONG_BPM)
#define SONG_BAR_SEC  (4.0 * SONG_BEAT_SEC)
#define SONG_LEN_SEC  (SONG_BARS * SONG_BAR_SEC)

static inline f64 song_bar(f64 bar)   { return bar * SONG_BAR_SEC; }
static inline f64 song_beat(f64 beat) { return beat * SONG_BEAT_SEC; }

typedef enum {
    SEC_WINTER,     /* bar 0  : polar night, the ship held fast            */
    SEC_PRESSURE,   /* bar 8  : the ice closes                             */
    SEC_SINKING,    /* bar 14 : 21 November 1915                           */
    SEC_DEEP,       /* bar 19 : three thousand metres of water             */
    SEC_FOUND,      /* bar 25 : 5 March 2022                               */
    SEC_COUNT
} song_section;

extern const int SONG_SECTION_BAR[SEC_COUNT + 1];

/* Events the picture and the sound both have to hit, in bars. */
#define CUE_SQUEEZE      10.55  /* the squeeze that cuts the stern light      */
#define CUE_FORE_FALL    11.55  /* the fore topmast goes                      */
#define CUE_MAIN_FALL    12.35  /* the mainmast snaps                         */
#define CUE_MAIN_LAND    2.4    /* seconds later, it is down on the ice       */
#define CUE_GOING        14.9   /* her stern starts to rise                   */
#define CUE_DIVE         3.6    /* seconds later, the dive                    */
#define CUE_UNDER        17.0   /* under the ice                              */

song_section song_section_at(f64 t);
f64          song_section_start(song_section s);
f64          song_section_end(song_section s);
f64          song_section_phase(f64 t);

dm_audio *song_render(void);

#endif /* ENDURANCE_SONG_H */
