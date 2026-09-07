#include "dm_video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>

struct dm_video {
    FILE *pipe;         /* ffmpeg stdin, or NULL when falling back to PPM */
    int   w, h;
    int   frames;
    char  ppm_dir[512];
};

int dm_have_ffmpeg(void)
{
    static int cached = -1;
    if (cached < 0)
        cached = (system("ffmpeg -version >nul 2>&1") == 0);
    return cached;
}

dm_video *dm_video_open(const char *out_path, int w, int h, int fps,
                        const char *audio_wav, int crf)
{
    dm_video *v = (dm_video *)calloc(1, sizeof(dm_video));
    if (!v) return NULL;
    v->w = w;
    v->h = h;

    if (dm_have_ffmpeg()) {
        char cmd[2048];
        char audio_in[600]  = "";
        char audio_out[128] = "";

        if (audio_wav && audio_wav[0]) {
            snprintf(audio_in, sizeof audio_in, "-i \"%s\" ", audio_wav);
            snprintf(audio_out, sizeof audio_out, "-c:a aac -b:a 320k -shortest ");
        }

        /* yuv420p + bt709 tags: what YouTube's transcoder expects. Anything
         * else and the colours come back shifted after their re-encode. */
        snprintf(cmd, sizeof cmd,
                 "ffmpeg -hide_banner -loglevel error -nostats -y "
                 "-f rawvideo -pix_fmt rgb24 -s %dx%d -framerate %d -i - "
                 "%s"
                 "-c:v libx264 -preset slower -crf %d -pix_fmt yuv420p "
                 "-color_primaries bt709 -color_trc bt709 -colorspace bt709 "
                                  /* Fragmented rather than faststart: a render this long must
                  * survive being interrupted. A fragmented file is playable up
                  * to whatever was written, and remuxing it to a normal
                  * faststart MP4 once it finishes is a copy, not a re-encode. */
                 "-movflags +frag_keyframe+empty_moov "
                 "%s\"%s\"",
                 w, h, fps, audio_in, crf, audio_out, out_path);

        printf("[video] %s\n", cmd);
        v->pipe = _popen(cmd, "wb");
        if (v->pipe) {
            /* 8 MB buffer: without it every frame turns into thousands of tiny
             * writes and the pipe syscall overhead shows up in the profile. */
            setvbuf(v->pipe, NULL, _IOFBF, 8 << 20);
            return v;
        }
        fprintf(stderr, "[video] could not start ffmpeg, falling back to PPM\n");
    } else {
        fprintf(stderr, "[video] ffmpeg not found, falling back to PPM\n");
    }

    snprintf(v->ppm_dir, sizeof v->ppm_dir, "%s.frames", out_path);
    _mkdir(v->ppm_dir);
    return v;
}

void dm_video_write(dm_video *v, const u8 *rgb24)
{
    size_t n = (size_t)v->w * v->h * 3;

    if (v->pipe) {
        fwrite(rgb24, 1, n, v->pipe);
    } else {
        char path[640];
        snprintf(path, sizeof path, "%s/f%06d.ppm", v->ppm_dir, v->frames);
        FILE *f = fopen(path, "wb");
        if (f) {
            fprintf(f, "P6\n%d %d\n255\n", v->w, v->h);
            fwrite(rgb24, 1, n, f);
            fclose(f);
        }
    }
    v->frames++;
}

int dm_video_frames(const dm_video *v) { return v->frames; }

void dm_video_close(dm_video *v)
{
    if (!v) return;
    if (v->pipe) _pclose(v->pipe);
    free(v);
}
