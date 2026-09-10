/* main.c -- TRACE driver.
 *
 *   trace song                                   the soundtrack, as a WAV
 *   trace check                                  what the soundtrack is doing, in numbers
 *   trace still <t> [w] [h] [sub]                one frame, as a PPM
 *   trace sheet [cols] [rows] [cell_w]           a contact sheet of the piece
 *   trace clip <t0> <t1> [w] [h] [sub] [crf]     one stretch, as video with its sound
 *   trace render [w] [h] [sub] [crf]             the film
 *
 * The soundtrack is rendered before any picture in every mode, because the
 * soundtrack is the picture. `sub` is how many slices the exposure of one
 * frame is cut into: the camera and the time axis move between slices, which
 * is the motion blur.
 *
 * Defaults are 1080x1920: composed for a phone held upright. The published
 * film is rendered at 1440x2560 -- see the README for why.
 */
#include "demo.h"
#include "../../engine/dm_video.h"
#include "../../engine/dm_synth.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define OUT_DIR "demos/04-trace/out"
#define FPS     60
/* A 270 degree shutter, the one most film is shot at. */
#define SHUTTER (0.75 / FPS)

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
    f64 a = now_sec();
    g_song = song_render();
    if (!g_song) { fprintf(stderr, "song render failed\n"); return 0; }
    printf("[song] rendered in %.1f s\n", now_sec() - a);
    tr_set_audio(g_song);
    return 1;
}

static void render_frame(dm_fb *fb, dm_post *post, f64 t, int frame, int sub)
{
    if (sub < 1) sub = 1;
    dm_fb_clear(fb, V3(0.0f, 0.0f, 0.0f));
    for (int k = 0; k < sub; k++) {
        f64 a = t + SHUTTER * (f64)k / (f64)sub;
        f64 b = t + SHUTTER * (f64)(k + 1) / (f64)sub;
        tr_ctx c = tr_ctx_make(t, a, b, SHUTTER, frame, fb->w, fb->h);
        demo_frame(fb, &c);
    }
    tr_ctx c = tr_ctx_make(t, t, t + SHUTTER, SHUTTER, frame, fb->w, fb->h);
    demo_finish(fb, &c);
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
    dm_wav_write(OUT_DIR "/trace.wav", g_song);
    printf("[out] " OUT_DIR "/trace.wav\n");
    return 0;
}

/* Nobody can listen to a buffer from a terminal, so this says in numbers the
 * things an ear would notice first. Per section: how loud, how wide, and how
 * much survives being summed to one speaker -- the one way a picture drawn in
 * stereo can fail a listener without anyone seeing it, because a figure lying
 * along the other diagonal cancels itself out in mono. */
static int mode_check(void)
{
    if (!need_audio()) return 1;
    static const char *NAME[SEC_COUNT] = { "TUNE", "PULSE", "CHAOS", "DROP", "NAME" };
    printf("  section    peak     rms   mono-loss  corr   |x|max  |y|max\n");
    for (int s = 0; s < SEC_COUNT; s++) {
        int i0 = dm_sec_to_frames(song_section_start((song_section)s));
        int i1 = dm_sec_to_frames(song_section_end((song_section)s));
        if (i1 > g_song->n) i1 = g_song->n;
        f64 sl = 0, sr = 0, slr = 0, sm = 0, pk = 0, mx = 0, my = 0;
        for (int i = i0; i < i1; i++) {
            f64 l = g_song->l[i], r = g_song->r[i];
            sl += l * l; sr += r * r; slr += l * r;
            f64 m = 0.5 * (l + r);
            sm += m * m;
            if (fabs(l) > mx) mx = fabs(l);
            if (fabs(r) > my) my = fabs(r);
        }
        pk = DM_MAX(mx, my);
        f64 n = (f64)DM_MAX(1, i1 - i0);
        f64 st = sqrt((sl + sr) / (2.0 * n));
        f64 mo = sqrt(sm / n);
        f64 corr = slr / sqrt(DM_MAX(sl * sr, 1e-30));
        printf("  %-7s %7.2f %7.2f %9.2f %7.2f %7.3f %7.3f\n", NAME[s],
               20.0 * log10(DM_MAX(pk, 1e-9)), 20.0 * log10(DM_MAX(st, 1e-9)),
               20.0 * log10(DM_MAX(mo, 1e-9) / DM_MAX(st, 1e-9)), corr, mx, my);
    }
    f32 peak, rms;
    dm_audio_stats(g_song, &peak, &rms);
    printf("  whole   %7.2f %7.2f\n", (double)peak, (double)rms);
    printf("\nmono-loss is the level of (L+R)/2 against the stereo level. Quadrature --\n"
           "a circle -- is -3 dB; identical channels 0 dB; opposed channels, silence.\n");
    return 0;
}

static int mode_still(f64 t, int w, int h, int sub)
{
    if (!need_audio()) return 1;
    dm_fb   *fb   = dm_fb_new(w, h);
    dm_post *post = dm_post_new(w, h, 6);
    u8      *rgb  = (u8 *)malloc((size_t)w * h * 3);
    if (!fb || !post || !rgb) return 1;

    f64 a = now_sec();
    render_frame(fb, post, t, (int)(t * FPS), sub);
    printf("[still] t=%.3f  %dx%d  sub=%d  %.0f ms\n", t, w, h, sub, (now_sec() - a) * 1000.0);

    dm_fb_resolve(fb, rgb, 1.0f, (u32)(t * FPS));

    char path[256];
    snprintf(path, sizeof path, OUT_DIR "/still_%06.3f.ppm", t);
    write_ppm(path, rgb, w, h);

    free(rgb);
    dm_post_free(post);
    dm_fb_free(fb);
    return 0;
}

static int mode_sheet(int cols, int rows, int cell_w)
{
    if (!need_audio()) return 1;
    int cell_h = cell_w * 16 / 9;
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
        render_frame(cell, post, t, (int)(t * FPS), 1);
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

/* The film, or a stretch of it. A stretch gets the matching stretch of the
 * soundtrack in its own WAV, so what plays is what is drawn. */
static int film(const char *name, f64 t0, f64 t1, int w, int h, int sub, int crf)
{
    if (!need_audio()) return 1;
    int f0 = (int)(t0 * FPS + 0.5), f1 = (int)(t1 * FPS + 0.5);
    if (f1 > (int)(SONG_LEN_SEC * FPS)) f1 = (int)(SONG_LEN_SEC * FPS);
    if (f1 <= f0) return 1;

    char wav[256], mp4[256];
    snprintf(wav, sizeof wav, OUT_DIR "/%s.wav", name);
    snprintf(mp4, sizeof mp4, OUT_DIR "/%s.mp4", name);

    int s0 = dm_sec_to_frames((f64)f0 / FPS);
    int s1 = dm_sec_to_frames((f64)f1 / FPS);
    if (s1 > g_song->n) s1 = g_song->n;
    dm_audio *part = dm_audio_new(s1 - s0);
    if (!part) return 1;
    memcpy(part->l, g_song->l + s0, sizeof(f32) * (size_t)(s1 - s0));
    memcpy(part->r, g_song->r + s0, sizeof(f32) * (size_t)(s1 - s0));
    dm_wav_write(wav, part);
    dm_audio_free(part);

    dm_fb   *fb   = dm_fb_new(w, h);
    dm_post *post = dm_post_new(w, h, 6);
    u8      *rgb  = (u8 *)malloc((size_t)w * h * 3);
    if (!fb || !post || !rgb) return 1;

    dm_video *vid = dm_video_open(mp4, w, h, FPS, wav, crf);
    if (!vid) return 1;

    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
    printf("[render] %dx%d  frames %d..%d  sub=%d  threads=%d\n",
           w, h, f0, f1, sub, dm_job_threads());

    f64 a = now_sec();
    int total = f1 - f0;
    for (int i = f0; i < f1; i++) {
        render_frame(fb, post, (f64)i / FPS, i, sub);
        dm_fb_resolve(fb, rgb, 1.0f, (u32)i);
        dm_video_write(vid, rgb);

        int done = i - f0 + 1;
        if ((done & 7) == 0 || i == f1 - 1) {
            f64 el = now_sec() - a, per = el / (f64)done;
            printf("\r[render] %d/%d  %.2f s/frame  elapsed %.1f min  eta %.1f min   ",
                   done, total, per, el / 60.0, per * (f64)(total - done) / 60.0);
            fflush(stdout);
        }
    }
    printf("\n[render] done in %.1f min\n", (now_sec() - a) / 60.0);

    dm_video_close(vid);
    free(rgb);
    dm_post_free(post);
    dm_fb_free(fb);
    return 0;
}

/* ---- entry --------------------------------------------------------------- */

int main(int argc, char **argv)
{
    dm_job_init(0);
    if (!tr_beam_init()) { fprintf(stderr, "out of memory\n"); return 1; }

    const char *mode = argc > 1 ? argv[1] : "sheet";
    int rc;

    if (!strcmp(mode, "song")) {
        rc = mode_song();
    } else if (!strcmp(mode, "check")) {
        rc = mode_check();
    } else if (!strcmp(mode, "still")) {
        f64 t   = argc > 2 ? atof(argv[2]) : 0.0;
        int w   = argc > 3 ? atoi(argv[3]) : 1080;
        int h   = argc > 4 ? atoi(argv[4]) : 1920;
        int sub = argc > 5 ? atoi(argv[5]) : 2;
        rc = mode_still(t, w, h, sub);
    } else if (!strcmp(mode, "sheet")) {
        int cols = argc > 2 ? atoi(argv[2]) : 8;
        int rows = argc > 3 ? atoi(argv[3]) : 2;
        int cw   = argc > 4 ? atoi(argv[4]) : 216;
        rc = mode_sheet(cols, rows, cw);
    } else if (!strcmp(mode, "clip")) {
        f64 t0  = argc > 2 ? atof(argv[2]) : 0.0;
        f64 t1  = argc > 3 ? atof(argv[3]) : 4.0;
        int w   = argc > 4 ? atoi(argv[4]) : 540;
        int h   = argc > 5 ? atoi(argv[5]) : 960;
        int sub = argc > 6 ? atoi(argv[6]) : 1;
        int crf = argc > 7 ? atoi(argv[7]) : 18;
        rc = film("clip", t0, t1, w, h, sub, crf);
    } else if (!strcmp(mode, "render")) {
        int w   = argc > 2 ? atoi(argv[2]) : 1080;
        int h   = argc > 3 ? atoi(argv[3]) : 1920;
        int sub = argc > 4 ? atoi(argv[4]) : 4;
        int crf = argc > 5 ? atoi(argv[5]) : 14;
        rc = film("trace", 0.0, SONG_LEN_SEC, w, h, sub, crf);
    } else {
        fprintf(stderr,
                "usage: trace song\n"
                "       trace check\n"
                "       trace still <t> [w] [h] [sub]\n"
                "       trace sheet [cols] [rows] [cell_w]\n"
                "       trace clip <t0> <t1> [w] [h] [sub] [crf]\n"
                "       trace render [w] [h] [sub] [crf]\n");
        rc = 2;
    }

    if (g_song) dm_audio_free(g_song);
    tr_beam_free();
    dm_job_shutdown();
    return rc;
}
