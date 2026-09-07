/* dm_video.h -- frame sink.
 *
 * The demo writes raw RGB24 frames into a pipe; ffmpeg on the other end turns
 * them into an H.264 file and muxes in the WAV the synth produced. ffmpeg is a
 * tool in the same sense the C compiler is -- no content comes from it. If it
 * is not installed we fall back to a numbered PPM sequence so a render never
 * fails for want of an encoder.
 */
#ifndef DM_VIDEO_H
#define DM_VIDEO_H

#include "dm_base.h"

typedef struct dm_video dm_video;

int dm_have_ffmpeg(void);

/* audio_wav may be NULL. crf 12..18 is the sane range; lower is bigger/better.
 * Returns NULL only if neither the pipe nor the fallback directory could open. */
dm_video *dm_video_open(const char *out_path, int w, int h, int fps,
                        const char *audio_wav, int crf);

void dm_video_write(dm_video *v, const u8 *rgb24);
int  dm_video_frames(const dm_video *v);
void dm_video_close(dm_video *v);

#endif /* DM_VIDEO_H */
