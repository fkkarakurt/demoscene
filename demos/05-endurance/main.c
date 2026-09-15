/* main.c -- ENDURANCE driver.
 *
 *   endurance song                                the soundtrack, as a WAV
 *   endurance still <t> [w] [spp]                 one frame, as a PPM
 *   endurance plate <t> [w] [spp]                 the same, filling 16:9, uncaptioned: a thumbnail's plate
 *   endurance sheet [cols] [rows] [cell_w] [spp]  the piece at even intervals
 *   endurance shots [cell_w] [spp]                one cell from the middle of every shot
 *   endurance clip <t0> <t1> [w] [spp] [crf]      one stretch, as video with its sound
 *   endurance render [w] [spp] [first] [last]     the film, as numbered frames
 *   endurance encode [w] [crf]                    those frames and the WAV, as an MP4
 *
 * `w` is the width of the frame; the frame is 16:9 and the picture inside it
 * is 2.39:1, with black above and below. `spp` is samples per pixel, and each
 * sample is a different instant of the open shutter as well as a different
 * point of the pixel and of the lens.
 *
 * The film is rendered to frames on disk rather than piped into an encoder,
 * because at 3840 wide it takes the better part of a day. A frame is written
 * under a temporary name and renamed once complete, and `render` skips every
 * frame that already exists -- so a render that is stopped, by a crash or a
 * reboot or on purpose, carries on from where it was when started again.
 * Encoding is a separate step, and can be repeated at another quality without
 * rendering anything.
 */
#include "demo.h"
#include "scenes/scenes.h"
#include "../../engine/dm_video.h"
#include "../../engine/dm_synth.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include <windows.h>

#define OUT_DIR    "demos/05-endurance/out"
#define FRAMES_DIR OUT_DIR "/frames"

static f64 now_sec(void)
{
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (f64)c.QuadPart / (f64)f.QuadPart;
}

static int write_ppm(const char *path, const u8 *rgb, int w, int h)
{
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", path); return 0; }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    size_t n = fwrite(rgb, 1, (size_t)w * h * 3, f);
    int ok = fclose(f) == 0 && n == (size_t)w * h * 3;
    return ok;
}

static dm_audio *g_song = NULL;

static int need_audio(void)
{
    if (g_song) return 1;
    f64 a = now_sec();
    g_song = song_render();
    if (!g_song) { fprintf(stderr, "song render failed\n"); return 0; }
    printf("[song] rendered in %.1f s\n", now_sec() - a);
    return 1;
}

/* ---- one frame -------------------------------------------------------------- */

typedef struct {
    int      W, H;          /* the whole frame                               */
    int      pw, ph, py;    /* the picture inside it                         */
    dm_fb   *pic;
    f32     *depth;
    dm_post *post;
    dm_fb   *frame;
    u8      *rgb;
    int      plate;         /* the whole 16:9 frame, and no captions         */
} target;

static int target_make_ex(target *T, int w, int levels, int plate)
{
    memset(T, 0, sizeof *T);
    T->W  = w & ~1;
    T->H  = (T->W * 9 / 16) & ~1;
    T->pw = T->W;
    T->ph = plate ? T->H : ((int)((f32)T->W / EN_SCOPE / 2.0f + 0.5f)) * 2;
    T->py = (T->H - T->ph) / 2;
    T->plate = plate;

    T->pic   = dm_fb_new(T->pw, T->ph);
    T->depth = (f32 *)malloc(sizeof(f32) * (size_t)T->pw * T->ph);
    T->post  = dm_post_new(T->pw, T->ph, levels);
    T->frame = dm_fb_new(T->W, T->H);
    T->rgb   = (u8 *)malloc((size_t)T->W * T->H * 3);
    return T->pic && T->depth && T->post && T->frame && T->rgb;
}

static int target_make(target *T, int w, int levels) { return target_make_ex(T, w, levels, 0); }

static void target_free(target *T)
{
    free(T->rgb);
    dm_fb_free(T->frame);
    dm_post_free(T->post);
    free(T->depth);
    dm_fb_free(T->pic);
}

/* dm_fb_resolve, a row per job: at 3840×2160 the tone map and dither of 8.3
 * million pixels on one core took longer than the post-processing on all of
 * them. Every pixel is its own function, so the bytes are the same. */
typedef struct { const dm_fb *f; u8 *rgb; u32 frame; } resolve_job;

static void resolve_row(int y, int thread, void *ud)
{
    (void)thread;
    const resolve_job *j = (const resolve_job *)ud;
    const dm_fb *f = j->f;
    for (int x = 0; x < f->w; x++) {
        v3 c = dm_fb_get(f, x, y);
        if (!(c.x == c.x)) c.x = 0.0f;
        if (!(c.y == c.y)) c.y = 0.0f;
        if (!(c.z == c.z)) c.z = 0.0f;
        c = dm_to_srgb(dm_tonemap_aces(c));
        f32 d = dm_dither_tpdf(x, y, j->frame);
        u8 *o = j->rgb + ((size_t)y * f->w + x) * 3;
        o[0] = (u8)(dm_sat(c.x + d) * 255.0f + 0.5f);
        o[1] = (u8)(dm_sat(c.y + d) * 255.0f + 0.5f);
        o[2] = (u8)(dm_sat(c.z + d) * 255.0f + 0.5f);
    }
}

static void render_frame(target *T, int frame, int spp)
{
    f64 t = (f64)frame / EN_FPS;
    en_ctx c = en_ctx_make(t, frame, T->pw, T->ph, spp);

    f64 p0 = now_sec();
    demo_frame(T->pic, T->depth, &c);
    f64 p1 = now_sec();

    f32 ev = demo_exposure(&c);
    v3  wb = demo_grade(&c);
    dm_fb_mul(T->pic, v3_scl(wb, powf(2.0f, ev)));

    /* A negative saturates: past a point, more light makes the highlight
     * hardly denser and the halation round it hardly wider. Rolled off
     * logarithmically, in hue, before the bloom sees it -- or a lamp or the
     * moon, a hundred thousand times brighter than the scene they light,
     * flares a quarter of the picture white. */
    {
        const f32 K = 24.0f;
        size_t np = (size_t)T->pw * T->ph;
        for (size_t i = 0; i < np; i++) {
            f32 *q = T->pic->px + i * 3;
            f32 m = DM_MAX(q[0], DM_MAX(q[1], q[2]));
            if (m <= K) continue;
            f32 s = K * (1.0f + logf(m / K)) / m;
            q[0] *= s; q[1] *= s; q[2] *= s;
        }
    }

    dm_post_params p = demo_post(&c);
    dm_post_apply(T->post, T->pic, &p);
    f64 p2 = now_sec();

    dm_fb_clear(T->frame, V3(0.0f, 0.0f, 0.0f));
    for (int y = 0; y < T->ph; y++)
        memcpy(T->frame->px + ((size_t)(y + T->py) * T->W) * 3,
               T->pic->px + (size_t)y * T->pw * 3, sizeof(f32) * 3 * (size_t)T->pw);

    en_rect pic = { 0.0f, (f32)T->py, (f32)T->pw, (f32)T->ph };
    if (!T->plate) demo_overlay(T->frame, pic, &c);

    resolve_job rj = { T->frame, T->rgb, (u32)frame };
    dm_job_for(T->H, resolve_row, &rj);
    if (getenv("PROFILE"))
        printf("[profile] frame %.2f s  post %.2f s  overlay+resolve %.2f s\n", p1 - p0, p2 - p1, now_sec() - p2);

    /* The bars are black, not black plus dither. */
    for (int y = 0; y < T->H; y++) {
        if (y >= T->py && y < T->py + T->ph) continue;
        memset(T->rgb + (size_t)y * T->W * 3, 0, (size_t)T->W * 3);
    }
}

/* ---- modes --------------------------------------------------------------- */

static int mode_song(void)
{
    if (!need_audio()) return 1;
    f32 peak, rms;
    dm_audio_stats(g_song, &peak, &rms);
    printf("[song] %.1f s, peak %.2f dBFS, rms %.2f dBFS\n", SONG_LEN_SEC, peak, rms);
    dm_wav_write(OUT_DIR "/endurance.wav", g_song);
    printf("[out] " OUT_DIR "/endurance.wav\n");
    return 0;
}

static int mode_still(f64 t, int w, int spp, int plate)
{
    target T;
    if (!target_make_ex(&T, w, 7, plate)) return 1;

    int frame = (int)(t * EN_FPS + 0.5);
    f64 a = now_sec();
    render_frame(&T, frame, spp);
    en_ctx c = en_ctx_make((f64)frame / EN_FPS, frame, T.pw, T.ph, spp);
    printf("[still] t=%.3f  frame %d  shot %s  %dx%d  spp=%d  %.2f s\n",
           (f64)frame / EN_FPS, frame, EN_SHOTS[c.shot].name, T.W, T.H, spp, now_sec() - a);

    char path[256];
    snprintf(path, sizeof path, OUT_DIR "/%s_%07.3f.ppm", plate ? "plate" : "still", (f64)frame / EN_FPS);
    if (write_ppm(path, T.rgb, T.W, T.H)) printf("[out] %s\n", path);
    target_free(&T);
    return 0;
}

/* A grid of pictures without their bars. `times` may be NULL for even
 * spacing over the whole piece. */
static int sheet(const char *name, int cols, int n, const f64 *times, int cell_w, int spp)
{
    target T;
    if (!target_make(&T, cell_w, 5)) return 1;

    int rows = (n + cols - 1) / cols;
    int W = cols * T.pw, H = rows * T.ph;
    u8 *out = (u8 *)calloc((size_t)W * H * 3, 1);
    if (!out) return 1;

    f64 a = now_sec();
    for (int i = 0; i < n; i++) {
        f64 t = times ? times[i] : SONG_LEN_SEC * ((f64)i + 0.5) / (f64)n;
        render_frame(&T, (int)(t * EN_FPS), spp);

        int cx = (i % cols) * T.pw, cy = (i / cols) * T.ph;
        for (int y = 0; y < T.ph; y++)
            memcpy(out + ((size_t)(cy + y) * W + cx) * 3,
                   T.rgb + ((size_t)(y + T.py) * T.W) * 3, (size_t)T.pw * 3);

        printf("\r[sheet] %d/%d  t=%.2f s", i + 1, n, t);
        fflush(stdout);
    }
    printf("\n[sheet] %.1f s total\n", now_sec() - a);

    char path[256];
    snprintf(path, sizeof path, OUT_DIR "/%s.ppm", name);
    if (write_ppm(path, out, W, H)) printf("[out] %s\n", path);
    free(out);
    target_free(&T);
    return 0;
}

static int mode_shots(int cell_w, int spp)
{
    f64 times[64];
    int n = EN_SHOT_COUNT < 64 ? EN_SHOT_COUNT : 64;
    for (int i = 0; i < n; i++)
        times[i] = 0.5 * (en_shot_start(i) + en_shot_end(i));
    return sheet("shots", n < 4 ? n : 4, n, times, cell_w, spp);
}

static int mode_clip(f64 t0, f64 t1, int w, int spp, int crf)
{
    if (!need_audio()) return 1;
    int f0 = (int)(t0 * EN_FPS + 0.5), f1 = (int)(t1 * EN_FPS + 0.5);
    int last = (int)(SONG_LEN_SEC * EN_FPS);
    if (f1 > last) f1 = last;
    if (f1 <= f0) return 1;

    int s0 = dm_sec_to_frames((f64)f0 / EN_FPS);
    int s1 = dm_sec_to_frames((f64)f1 / EN_FPS);
    if (s1 > g_song->n) s1 = g_song->n;
    dm_audio *part = dm_audio_new(s1 - s0);
    if (!part) return 1;
    memcpy(part->l, g_song->l + s0, sizeof(f32) * (size_t)(s1 - s0));
    memcpy(part->r, g_song->r + s0, sizeof(f32) * (size_t)(s1 - s0));
    dm_wav_write(OUT_DIR "/clip.wav", part);
    dm_audio_free(part);

    target T;
    if (!target_make(&T, w, 6)) return 1;
    dm_video *vid = dm_video_open(OUT_DIR "/clip.mp4", T.W, T.H, EN_FPS, OUT_DIR "/clip.wav", crf);
    if (!vid) return 1;

    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
    printf("[clip] %dx%d  frames %d..%d  spp=%d  threads=%d\n",
           T.W, T.H, f0, f1, spp, dm_job_threads());

    f64 a = now_sec();
    int total = f1 - f0;
    for (int i = f0; i < f1; i++) {
        render_frame(&T, i, spp);
        dm_video_write(vid, T.rgb);
        int done = i - f0 + 1;
        f64 el = now_sec() - a, per = el / (f64)done;
        printf("\r[clip] %d/%d  %.2f s/frame  eta %.1f min   ", done, total, per,
               per * (f64)(total - done) / 60.0);
        fflush(stdout);
    }
    printf("\n[clip] done in %.1f min\n", (now_sec() - a) / 60.0);

    dm_video_close(vid);
    target_free(&T);
    return 0;
}

static int frame_done(const char *path, long long want)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    _fseeki64(f, 0, SEEK_END);
    long long n = _ftelli64(f);
    fclose(f);
    return n >= want;
}

static int mode_render(int w, int spp, int first, int last)
{
    target T;
    if (!target_make(&T, w, 7)) return 1;

    int frames = (int)(SONG_LEN_SEC * EN_FPS);
    if (last < 0 || last >= frames) last = frames - 1;
    if (first < 0) first = 0;

    _mkdir(FRAMES_DIR);
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);

    char header[64];
    int  hl = snprintf(header, sizeof header, "P6\n%d %d\n255\n", T.W, T.H);
    long long want = (long long)hl + (long long)T.W * T.H * 3;

    printf("[render] %dx%d (picture %dx%d)  frames %d..%d  spp=%d  threads=%d\n",
           T.W, T.H, T.pw, T.ph, first, last, spp, dm_job_threads());

    f64 a = now_sec();
    int done = 0, skipped = 0;
    for (int i = first; i <= last; i++) {
        char path[256], tmp[256];
        snprintf(path, sizeof path, FRAMES_DIR "/f%05d.ppm", i);
        if (frame_done(path, want)) { skipped++; continue; }

        f64 fa = now_sec();
        render_frame(&T, i, spp);
        snprintf(tmp, sizeof tmp, FRAMES_DIR "/f%05d.tmp", i);
        if (!write_ppm(tmp, T.rgb, T.W, T.H)) return 1;
        remove(path);
        if (rename(tmp, path) != 0) { fprintf(stderr, "cannot rename %s\n", tmp); return 1; }
        done++;

        f64 el = now_sec() - a, per = el / (f64)done;
        int left = (last - i);
        printf("\r[render] frame %d  (%d done, %d skipped)  this %.1f s  mean %.1f s  eta %.1f h   ",
               i, done, skipped, now_sec() - fa, per, per * (f64)left / 3600.0);
        fflush(stdout);
    }
    printf("\n[render] %d frames in %.1f h, %d already there\n", done, (now_sec() - a) / 3600.0, skipped);
    target_free(&T);
    return 0;
}

static int mode_encode(int w, int crf)
{
    if (!need_audio()) return 1;
    dm_wav_write(OUT_DIR "/endurance.wav", g_song);

    int W = w & ~1, H = (W * 9 / 16) & ~1;
    int frames = (int)(SONG_LEN_SEC * EN_FPS);
    char probe[256];
    snprintf(probe, sizeof probe, FRAMES_DIR "/f%05d.ppm", frames - 1);
    FILE *f = fopen(probe, "rb");
    if (!f) { fprintf(stderr, "[encode] %s is missing -- the render is not finished\n", probe); return 1; }
    fclose(f);

    /* x264 at a low CRF with the film tune: the grain in these frames is
     * texture, and the default tune spends its bits smoothing it away. The
     * tags say BT.709 because that is what the frames are and what YouTube's
     * transcoder assumes when nothing says otherwise. */
    char cmd[2048];
    snprintf(cmd, sizeof cmd,
             "ffmpeg -hide_banner -y -framerate %d -i \"" FRAMES_DIR "/f%%05d.ppm\" "
             "-i \"" OUT_DIR "/endurance.wav\" -frames:v %d "
             "-c:v libx264 -preset slow -tune film -crf %d -pix_fmt yuv420p "
             "-color_primaries bt709 -color_trc bt709 -colorspace bt709 "
             "-c:a aac -b:a 320k -shortest -movflags +faststart "
             "-s %dx%d \"" OUT_DIR "/endurance.mp4\"",
             EN_FPS, frames, crf, W, H);
    printf("[encode] %s\n", cmd);
    return system(cmd) == 0 ? 0 : 1;
}

/* ---- entry --------------------------------------------------------------- */

int main(int argc, char **argv)
{
    /* Long renders run detached with their output going to a log; unbuffered,
     * the log says how far they have got. */
    setvbuf(stdout, NULL, _IONBF, 0);
    dm_job_init(getenv("JOBS") ? atoi(getenv("JOBS")) : 0);
    scenes_init();

    const char *mode = argc > 1 ? argv[1] : "shots";
    int rc;

    if (!strcmp(mode, "song")) {
        rc = mode_song();
    } else if (!strcmp(mode, "still") || !strcmp(mode, "plate")) {
        f64 t   = argc > 2 ? atof(argv[2]) : 0.0;
        int w   = argc > 3 ? atoi(argv[3]) : 1920;
        int spp = argc > 4 ? atoi(argv[4]) : 4;
        rc = mode_still(t, w, spp, !strcmp(mode, "plate"));
    } else if (!strcmp(mode, "sheet")) {
        int cols = argc > 2 ? atoi(argv[2]) : 5;
        int rows = argc > 3 ? atoi(argv[3]) : 6;
        int cw   = argc > 4 ? atoi(argv[4]) : 384;
        int spp  = argc > 5 ? atoi(argv[5]) : 1;
        rc = sheet("sheet", cols, cols * rows, NULL, cw, spp);
    } else if (!strcmp(mode, "shots")) {
        int cw  = argc > 2 ? atoi(argv[2]) : 480;
        int spp = argc > 3 ? atoi(argv[3]) : 2;
        rc = mode_shots(cw, spp);
    } else if (!strcmp(mode, "clip")) {
        f64 t0  = argc > 2 ? atof(argv[2]) : 0.0;
        f64 t1  = argc > 3 ? atof(argv[3]) : 8.0;
        int w   = argc > 4 ? atoi(argv[4]) : 960;
        int spp = argc > 5 ? atoi(argv[5]) : 2;
        int crf = argc > 6 ? atoi(argv[6]) : 18;
        rc = mode_clip(t0, t1, w, spp, crf);
    } else if (!strcmp(mode, "render")) {
        int w     = argc > 2 ? atoi(argv[2]) : 3840;
        int spp   = argc > 3 ? atoi(argv[3]) : 4;
        int first = argc > 4 ? atoi(argv[4]) : 0;
        int last  = argc > 5 ? atoi(argv[5]) : -1;
        rc = mode_render(w, spp, first, last);
    } else if (!strcmp(mode, "encode")) {
        int w   = argc > 2 ? atoi(argv[2]) : 3840;
        int crf = argc > 3 ? atoi(argv[3]) : 14;
        rc = mode_encode(w, crf);
    } else {
        fprintf(stderr,
                "usage: endurance song\n"
                "       endurance still <t> [w] [spp]\n"
                "       endurance plate <t> [w] [spp]\n"
                "       endurance sheet [cols] [rows] [cell_w] [spp]\n"
                "       endurance shots [cell_w] [spp]\n"
                "       endurance clip <t0> <t1> [w] [spp] [crf]\n"
                "       endurance render [w] [spp] [first] [last]\n"
                "       endurance encode [w] [crf]\n");
        rc = 2;
    }

    if (g_song) dm_audio_free(g_song);
    dm_job_shutdown();
    return rc;
}
