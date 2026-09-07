/* dm_audio.h -- stereo float audio buffer and a WAV writer.
 *
 * The whole soundtrack is synthesised offline into one of these and written
 * out once, then ffmpeg muxes it against the video. Nothing is sampled or
 * recorded: every waveform in the file comes out of dm_synth.c.
 */
#ifndef DM_AUDIO_H
#define DM_AUDIO_H

#include "dm_base.h"

#define DM_SR 48000

typedef struct {
    f32 *l, *r;
    int  n;        /* frames */
} dm_audio;

dm_audio *dm_audio_new(int frames);
void      dm_audio_free(dm_audio *a);
void      dm_audio_clear(dm_audio *a);

/* dst += src * gain */
void dm_audio_mix(dm_audio *dst, const dm_audio *src, f32 gain);

/* Peak and RMS in dBFS, for sanity-checking a render without listening. */
void dm_audio_stats(const dm_audio *a, f32 *peak_db, f32 *rms_db);

/* 16-bit PCM, the format every tool accepts without argument. Values are
 * dithered on the way down for the same reason the video frames are. */
int dm_wav_write(const char *path, const dm_audio *a);

static inline int dm_sec_to_frames(f64 sec) { return (int)(sec * DM_SR + 0.5); }

#endif /* DM_AUDIO_H */
