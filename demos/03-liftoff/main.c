/* main.c -- LIFTOFF driver.
 *
 *   liftoff song                                  the soundtrack only
 *   liftoff profile                               the trajectory, as a table
 *   liftoff still <t> [w] [h] [aa]                one frame, as a PPM
 *   liftoff sheet [cols] [rows] [cell_w] [aa]     a contact sheet of the piece
 *   liftoff render [w] [h] [aa] [sub] [crf]       the film
 *
 * Defaults are 1080x1920: composed for a phone held upright, and a landscape
 * preview would flatter shots that do not work in the real frame.
 *
 * `profile` is the mode this demo could not be made without. The trajectory
 * drives every camera, so a camera that looks wrong is usually a trajectory
 * that is wrong, and reading it as numbers takes a second where rendering it
 * takes an hour.
 */
#include "demo.h"
#include "../../engine/dm_video.h"
#include "../../engine/dm_synth.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define OUT_DIR "demos/03-liftoff/out"
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

static dm_audio *g_song = NULL;

static int need_audio(void)
{
    if (g_song) return 1;
    g_song = song_render();
    if (!g_song) { fprintf(stderr, "song render failed\n"); return 0; }
    return 1;
}

static void render_frame(dm_fb *fb, dm_fb *acc, dm_post *post,
                         f64 t, int frame, int sub)
{
    if (sub <= 1) {
        lo_ctx c = lo_ctx_make(t, frame, fb->w, fb->h);
        demo_frame(fb, &c);
    } else {
        dm_fb_clear(acc, V3(0, 0, 0));
        for (int k = 0; k < sub; k++) {
            f64 tt = t + ((f64)k / (f64)sub) * 0.75 / FPS;
            lo_ctx c = lo_ctx_make(tt, frame * sub + k, fb->w, fb->h);
            demo_frame(fb, &c);
            dm_fb_add_fb(acc, fb, 1.0f / (f32)sub);
        }
        dm_fb_copy(fb, acc);
    }

    lo_ctx c = lo_ctx_make(t, frame, fb->w, fb->h);
    dm_post_params p = demo_post(&c);
    dm_post_apply(post, fb, &p);
}

/* ---- modes --------------------------------------------------------------- */

static int mode_song(void)
{
    if (!need_audio()) return 1;
    f32 peak, rms;
    dm_audio_stats(g_song, &peak, &rms);
    printf("[song] %.1f s, peak %.2f dBFS, rms %.2f dBFS\n", SONG_LEN_SEC, peak, rms);
    dm_wav_write(OUT_DIR "/liftoff.wav", g_song);
    printf("[out] " OUT_DIR "/liftoff.wav\n");
    return 0;
}

static int mode_profile(void)
{
    printf("  video   mission     alt        vel   mach   q(kPa)  acc(g)  thr  st\n");
    for (int i = 0; i <= 60; i++) {
        f64 t = SONG_LEN_SEC * (f64)i / 60.0;
        if (t >= SONG_LEN_SEC) t = SONG_LEN_SEC - 1e-6;
        f64 mt = lo_mission_time(t);
        lo_state s = lo_flight_at(mt);
        printf("%7.2f %9.1f %9.0f %9.0f %6.2f %8.1f %7.2f %5.2f %3d\n",
               t, mt, (double)s.alt, (double)s.vel, (double)s.mach,
               (double)s.q / 1000.0, (double)s.accel / 9.80665,
               (double)s.throttle, s.stage);
    }
    printf("\n[maxq] T+%.1f s, %.1f kPa\n", lo_flight_maxq(),
           (double)lo_flight_maxq_pa() / 1000.0);

    /* The four figures the piece stands or falls on. */
    lo_state meco = lo_flight_at(LO_T_MECO);
    lo_state seco = lo_flight_at(LO_T_SECO);
    printf("[meco] T+%.0f  alt %.1f km  vel %.0f m/s  pitch %.0f deg  mass %.0f t\n",
           LO_T_MECO, (double)meco.alt / 1000.0, (double)meco.vel,
           (double)meco.pitch * 57.2958, (double)meco.mass / 1000.0);
    printf("[seco] T+%.0f  alt %.1f km  vel %.0f m/s  pitch %.0f deg  mass %.0f t\n",
           LO_T_SECO, (double)seco.alt / 1000.0, (double)seco.vel,
           (double)seco.pitch * 57.2958, (double)seco.mass / 1000.0);
    return 0;
}

static int mode_still(f64 t, int w, int h, int aa)
{
    lo_aa = aa;
    dm_fb   *fb   = dm_fb_new(w, h);
    dm_post *post = dm_post_new(w, h, 6);
    u8      *rgb  = (u8 *)malloc((size_t)w * h * 3);
    if (!fb || !post || !rgb) return 1;

    f64 a = now_sec();
    render_frame(fb, NULL, post, t, (int)(t * FPS), 1);
    printf("[still] t=%.2f  mt=%+.1f  %dx%d  aa=%d  %.0f ms\n",
           t, lo_mission_time(t), w, h, aa, (now_sec() - a) * 1000.0);

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
    lo_aa = aa;
    int cell_h = cell_w * 16 / 9;                  /* portrait cells */
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

static int mode_render(int w, int h, int aa, int sub, int crf)
{
    if (!need_audio()) return 1;
    lo_aa = aa;
    int frames = (int)(SONG_LEN_SEC * FPS);

    dm_fb   *fb   = dm_fb_new(w, h);
    dm_fb   *acc  = sub > 1 ? dm_fb_new(w, h) : NULL;
    dm_post *post = dm_post_new(w, h, 6);
    u8      *rgb  = (u8 *)malloc((size_t)w * h * 3);
    if (!fb || !post || !rgb || (sub > 1 && !acc)) return 1;

    dm_wav_write(OUT_DIR "/liftoff.wav", g_song);

    dm_video *vid = dm_video_open(OUT_DIR "/liftoff.mp4", w, h, FPS,
                                  OUT_DIR "/liftoff.wav", crf);
    if (!vid) return 1;

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
    /* Before anything else: every camera in the piece asks the trajectory
     * where the vehicle is, including the one in frame zero. */
    lo_flight_init();

    const char *mode = argc > 1 ? argv[1] : "sheet";
    int rc;

    if (!strcmp(mode, "song")) {
        rc = mode_song();
    } else if (!strcmp(mode, "profile")) {
        rc = mode_profile();
    } else if (!strcmp(mode, "still")) {
        f64 t  = argc > 2 ? atof(argv[2]) : 0.0;
        int w  = argc > 3 ? atoi(argv[3]) : 1080;
        int h  = argc > 4 ? atoi(argv[4]) : 1920;
        int aa = argc > 5 ? atoi(argv[5]) : 2;
        rc = mode_still(t, w, h, aa);
    } else if (!strcmp(mode, "sheet")) {
        int cols = argc > 2 ? atoi(argv[2]) : 7;
        int rows = argc > 3 ? atoi(argv[3]) : 2;
        int cw   = argc > 4 ? atoi(argv[4]) : 200;
        int aa   = argc > 5 ? atoi(argv[5]) : 1;
        rc = mode_sheet(cols, rows, cw, aa);
    } else if (!strcmp(mode, "render")) {
        int w   = argc > 2 ? atoi(argv[2]) : 1080;
        int h   = argc > 3 ? atoi(argv[3]) : 1920;
        int aa  = argc > 4 ? atoi(argv[4]) : 2;
        int sub = argc > 5 ? atoi(argv[5]) : 2;
        int crf = argc > 6 ? atoi(argv[6]) : 15;
        rc = mode_render(w, h, aa, sub, crf);
    } else {
        fprintf(stderr,
                "usage: liftoff song\n"
                "       liftoff profile\n"
                "       liftoff still <t> [w] [h] [aa]\n"
                "       liftoff sheet [cols] [rows] [cell_w] [aa]\n"
                "       liftoff render [w] [h] [aa] [sub] [crf]\n");
        rc = 2;
    }

    if (g_song) dm_audio_free(g_song);
    dm_job_shutdown();
    return rc;
}
