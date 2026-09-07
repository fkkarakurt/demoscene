/* main.c -- COLD START driver.
 *
 *   coldstart song                                  the soundtrack only
 *   coldstart still <t> [w] [h] [aa]                one frame, as a PPM
 *   coldstart sheet [cols] [rows] [cell_w] [aa]     a contact sheet of the whole demo
 *   coldstart clip <t0> <t1> [w] [h] [aa] [sub]     one range, as video
 *   coldstart render [w] [h] [aa] [sub] [crf]       the film
 *
 * `sheet` is the one that earns its keep: two minutes of demo reviewed in a
 * single image, so a scene that is too dark or too similar to its neighbour
 * shows up immediately instead of after a two-hour render.
 */
#include "demo.h"
#include "../../engine/dm_video.h"
#include "../../engine/dm_synth.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define OUT_DIR "demos/01-coldstart/out"
#define FPS     60

static f64 now_sec(void)
{
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (f64)c.QuadPart / (f64)f.QuadPart;
}

static void write_ppm(const char *path, const u8 *rgb, int w, int h)
{
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", path); return; }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    fwrite(rgb, 1, (size_t)w * h * 3, f);
    fclose(f);
    printf("[out] %s\n", path);
}

/* One finished frame: scene, optional motion blur, post, all in `fb`. */
static void render_frame(dm_fb *fb, dm_fb *acc, dm_post *post,
                         f64 t, int frame, int sub)
{
    if (sub <= 1) {
        yk_ctx c = yk_ctx_make(t, frame, fb->w, fb->h);
        demo_frame(fb, &c);
    } else {
        /* Motion blur by supersampling in time. A 0.75 shutter means the
         * samples span three quarters of the frame interval, which is close
         * to the 270-degree shutter most film is shot at. */
        dm_fb_clear(acc, V3(0, 0, 0));
        for (int k = 0; k < sub; k++) {
            f64 tt = t + ((f64)k / (f64)sub) * 0.75 / FPS;
            yk_ctx c = yk_ctx_make(tt, frame * sub + k, fb->w, fb->h);
            demo_frame(fb, &c);
            dm_fb_add_fb(acc, fb, 1.0f / (f32)sub);
        }
        dm_fb_copy(fb, acc);
    }

    yk_ctx c = yk_ctx_make(t, frame, fb->w, fb->h);
    dm_post_params p = demo_post(&c);
    dm_post_apply(post, fb, &p);
}

/* ---- modes --------------------------------------------------------------- */

static int mode_song(void)
{
    printf("[song] rendering %.1f s...\n", SONG_LEN_SEC);
    dm_audio *au = song_render();
    if (!au) return 1;
    f32 peak, rms;
    dm_audio_stats(au, &peak, &rms);
    printf("[song] peak %.2f dBFS, rms %.2f dBFS\n", peak, rms);
    dm_wav_write(OUT_DIR "/coldstart.wav", au);
    printf("[out] " OUT_DIR "/coldstart.wav\n");
    dm_audio_free(au);
    return 0;
}

static int mode_still(f64 t, int w, int h, int aa)
{
    yk_aa = aa;
    dm_fb   *fb   = dm_fb_new(w, h);
    dm_post *post = dm_post_new(w, h, 6);
    u8      *rgb  = (u8 *)malloc((size_t)w * h * 3);
    if (!fb || !post || !rgb) return 1;

    f64 a = now_sec();
    render_frame(fb, NULL, post, t, (int)(t * FPS), 1);
    printf("[still] t=%.2f  %dx%d  aa=%d  %.0f ms\n", t, w, h, aa, (now_sec() - a) * 1000.0);

    dm_fb_resolve(fb, rgb, 1.0f, (u32)(t * FPS));

    char path[256];
    snprintf(path, sizeof path, OUT_DIR "/still_%06.2f.ppm", t);
    write_ppm(path, rgb, w, h);

    free(rgb);
    dm_post_free(post);
    dm_fb_free(fb);
    return 0;
}

static int mode_sheet(int cols, int rows, int cell_w, int aa)
{
    yk_aa = aa;
    int cell_h = cell_w * 9 / 16;
    int W = cols * cell_w, H = rows * cell_h;
    int n = cols * rows;

    dm_fb   *cell = dm_fb_new(cell_w, cell_h);
    dm_post *post = dm_post_new(cell_w, cell_h, 5);
    u8      *rgb  = (u8 *)malloc((size_t)W * H * 3);
    u8      *tile = (u8 *)malloc((size_t)cell_w * cell_h * 3);
    if (!cell || !post || !rgb || !tile) return 1;
    memset(rgb, 0, (size_t)W * H * 3);

    f64 a = now_sec();
    for (int i = 0; i < n; i++) {
        /* Sample just inside each slot so the very first frame (still black)
         * never occupies a cell. */
        f64 t = SONG_LEN_SEC * ((f64)i + 0.5) / (f64)n;
        render_frame(cell, NULL, post, t, (int)(t * FPS), 1);
        dm_fb_resolve(cell, tile, 1.0f, (u32)i);

        int cx = (i % cols) * cell_w, cy = (i / cols) * cell_h;
        for (int y = 0; y < cell_h; y++)
            memcpy(rgb + (((size_t)(cy + y) * W) + cx) * 3,
                   tile + (size_t)y * cell_w * 3, (size_t)cell_w * 3);

        printf("\r[sheet] %d/%d  t=%.1f s", i + 1, n, t);
        fflush(stdout);
    }
    printf("\n[sheet] %.1f s total\n", now_sec() - a);

    write_ppm(OUT_DIR "/sheet.ppm", rgb, W, H);

    free(tile);
    free(rgb);
    dm_post_free(post);
    dm_fb_free(cell);
    return 0;
}

/* Video-only render of one time range, for checking motion and cuts without
 * committing to the whole film. Mux the matching slice of audio afterwards. */
static int mode_clip(f64 t0, f64 t1, int w, int h, int aa, int sub)
{
    yk_aa = aa;
    int f0 = (int)(t0 * FPS), f1 = (int)(t1 * FPS);
    if (f1 <= f0) return 1;

    dm_fb   *fb   = dm_fb_new(w, h);
    dm_fb   *acc  = sub > 1 ? dm_fb_new(w, h) : NULL;
    dm_post *post = dm_post_new(w, h, 6);
    u8      *rgb  = (u8 *)malloc((size_t)w * h * 3);
    if (!fb || !post || !rgb || (sub > 1 && !acc)) return 1;

    dm_video *vid = dm_video_open(OUT_DIR "/clip.mp4", w, h, FPS, NULL, 14);
    if (!vid) return 1;

    printf("[clip] %.2f..%.2f s  %dx%d  aa=%d sub=%d  %d frames\n",
           t0, t1, w, h, aa, sub, f1 - f0);

    f64 a = now_sec();
    for (int i = f0; i < f1; i++) {
        render_frame(fb, acc, post, (f64)i / FPS, i, sub);
        dm_fb_resolve(fb, rgb, 1.0f, (u32)i);
        dm_video_write(vid, rgb);
        printf("\r[clip] %d/%d  %.2f s/frame  ", i - f0 + 1, f1 - f0,
               (now_sec() - a) / (f64)(i - f0 + 1));
        fflush(stdout);
    }
    printf("\n[clip] %.1f s total\n", now_sec() - a);

    dm_video_close(vid);
    free(rgb);
    dm_post_free(post);
    dm_fb_free(acc);
    dm_fb_free(fb);
    return 0;
}

static int mode_render(int w, int h, int aa, int sub, int crf)
{
    yk_aa = aa;
    int frames = (int)(SONG_LEN_SEC * FPS + 0.5);

    dm_fb   *fb   = dm_fb_new(w, h);
    dm_fb   *acc  = sub > 1 ? dm_fb_new(w, h) : NULL;
    dm_post *post = dm_post_new(w, h, 6);
    u8      *rgb  = (u8 *)malloc((size_t)w * h * 3);
    if (!fb || !post || !rgb || (sub > 1 && !acc)) return 1;

    /* Render the soundtrack first so ffmpeg can mux it in one pass. */
    printf("[render] soundtrack...\n");
    dm_audio *au = song_render();
    if (au) { dm_wav_write(OUT_DIR "/coldstart.wav", au); dm_audio_free(au); }

    dm_video *vid = dm_video_open(OUT_DIR "/coldstart.mp4", w, h, FPS,
                                  OUT_DIR "/coldstart.wav", crf);
    if (!vid) return 1;

    /* Keep the machine awake for the duration. Scoped to this process and
     * reverted on exit, rather than touching the user's power settings. */
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);

    printf("[render] %dx%d  %d frames  aa=%d  sub=%d  threads=%d\n",
           w, h, frames, aa, sub, dm_job_threads());

    f64 t0 = now_sec();
    for (int i = 0; i < frames; i++) {
        render_frame(fb, acc, post, (f64)i / FPS, i, sub);
        dm_fb_resolve(fb, rgb, 1.0f, (u32)i);
        dm_video_write(vid, rgb);

        if ((i & 7) == 0 || i == frames - 1) {
            f64 el = now_sec() - t0;
            f64 per = el / (f64)(i + 1);
            printf("\r[render] %d/%d  %.2f s/frame  elapsed %.1f min  eta %.1f min   ",
                   i + 1, frames, per, el / 60.0, per * (frames - i - 1) / 60.0);
            fflush(stdout);
        }
    }
    printf("\n[render] done in %.1f min\n", (now_sec() - t0) / 60.0);

    dm_video_close(vid);
    free(rgb);
    dm_post_free(post);
    dm_fb_free(acc);
    dm_fb_free(fb);
    return 0;
}

/* ---- entry --------------------------------------------------------------- */

int main(int argc, char **argv)
{
    dm_job_init(0);

    const char *mode = argc > 1 ? argv[1] : "sheet";
    int rc;

    if (!strcmp(mode, "song")) {
        rc = mode_song();
    } else if (!strcmp(mode, "still")) {
        f64 t = argc > 2 ? atof(argv[2]) : 0.0;
        int w = argc > 3 ? atoi(argv[3]) : 1920;
        int h = argc > 4 ? atoi(argv[4]) : 1080;
        int aa = argc > 5 ? atoi(argv[5]) : 2;
        rc = mode_still(t, w, h, aa);
    } else if (!strcmp(mode, "sheet")) {
        int cols = argc > 2 ? atoi(argv[2]) : 6;
        int rows = argc > 3 ? atoi(argv[3]) : 5;
        int cw   = argc > 4 ? atoi(argv[4]) : 480;
        int aa   = argc > 5 ? atoi(argv[5]) : 1;
        rc = mode_sheet(cols, rows, cw, aa);
    } else if (!strcmp(mode, "clip")) {
        f64 t0  = argc > 2 ? atof(argv[2]) : 0.0;
        f64 t1  = argc > 3 ? atof(argv[3]) : t0 + 6.0;
        int w   = argc > 4 ? atoi(argv[4]) : 1280;
        int h   = argc > 5 ? atoi(argv[5]) : 720;
        int aa  = argc > 6 ? atoi(argv[6]) : 1;
        int sub = argc > 7 ? atoi(argv[7]) : 3;
        rc = mode_clip(t0, t1, w, h, aa, sub);
    } else if (!strcmp(mode, "render")) {
        int w   = argc > 2 ? atoi(argv[2]) : 1920;
        int h   = argc > 3 ? atoi(argv[3]) : 1080;
        int aa  = argc > 4 ? atoi(argv[4]) : 3;
        int sub = argc > 5 ? atoi(argv[5]) : 2;
        int crf = argc > 6 ? atoi(argv[6]) : 15;
        rc = mode_render(w, h, aa, sub, crf);
    } else {
        fprintf(stderr,
                "usage: coldstart song\n"
                "       coldstart still <t> [w] [h] [aa]\n"
                "       coldstart sheet [cols] [rows] [cell_w] [aa]\n"
                "       coldstart clip <t0> <t1> [w] [h] [aa] [sub]\n"
                "       coldstart render [w] [h] [aa] [sub] [crf]\n");
        rc = 2;
    }

    dm_job_shutdown();
    return rc;
}
