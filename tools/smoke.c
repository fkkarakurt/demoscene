/* smoke.c -- engine shakedown.
 *
 * Renders a domain-warped noise field at full output resolution to answer the
 * only question that matters before writing any scenes: how many seconds does
 * one 1080p frame cost on this machine, and does the whole pipeline (threads,
 * HDR buffer, tone map, ffmpeg pipe) hold together end to end.
 *
 *   smoke.exe [frames] [width] [height]
 */
#include "../engine/dm_fb.h"
#include "../engine/dm_job.h"
#include "../engine/dm_post.h"
#include "../engine/dm_rand.h"
#include "../engine/dm_video.h"

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

typedef struct {
    dm_fb *fb;
    f32    t;
} ctx;

static void render_row(int y, int thread, void *ud)
{
    (void)thread;
    ctx   *c  = (ctx *)ud;
    dm_fb *fb = c->fb;
    f32 aspect = (f32)fb->w / (f32)fb->h;

    for (int x = 0; x < fb->w; x++) {
        /* Screen space normalised to -1..1 vertically, y up. */
        v2 uv = V2((((f32)x + 0.5f) / (f32)fb->w * 2.0f - 1.0f) * aspect,
                   1.0f - ((f32)y + 0.5f) / (f32)fb->h * 2.0f);

        v3 p = V3(uv.x * 1.4f, uv.y * 1.4f, c->t * 0.18f);

        /* Warped fBm is the slow billowing base. Raised to a high power it
         * stays near black almost everywhere, which is the discipline the
         * first pass lacked: darkness is the default, light is earned. */
        f32 n = dm_warp3(p, 1.1f, 5, 1u);
        f32 m = dm_sat(n * 0.5f + 0.5f);
        v3 col = v3_scl(dm_pal_house(0.56f + n * 0.10f), powf(m, 3.0f) * 0.55f);

        /* The ridged layer supplies the only values allowed above 1.0 --
         * thin filaments for the tone mapper and the bloom to work on. */
        f32 r = dm_ridge3(v3_scl(p, 2.3f), 5, 2.0f, 0.5f, 7u);
        col = v3_add(col, v3_scl(dm_pal_house(0.16f), powf(r, 9.0f) * 26.0f));

        dm_fb_set(fb, x, y, col);
    }
}

static f64 now_sec(void)
{
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (f64)c.QuadPart / (f64)f.QuadPart;
}

int main(int argc, char **argv)
{
    int frames = argc > 1 ? atoi(argv[1]) : 30;
    int W      = argc > 2 ? atoi(argv[2]) : 1920;
    int H      = argc > 3 ? atoi(argv[3]) : 1080;
    const int FPS = 60;

    dm_job_init(0);
    printf("[smoke] %dx%d, %d frames, %d threads\n", W, H, frames, dm_job_threads());

    dm_fb *fb   = dm_fb_new(W, H);
    u8    *rgb  = (u8 *)malloc((size_t)W * H * 3);
    if (!fb || !rgb) { fprintf(stderr, "out of memory\n"); return 1; }

    dm_post   *post = dm_post_new(W, H, 6);
    dm_video  *vid  = dm_video_open("tools/out/smoke.mp4", W, H, FPS, NULL, 16);

    f64 t_render = 0.0, t_post = 0.0, t0 = now_sec();

    for (int i = 0; i < frames; i++) {
        ctx c;
        c.fb = fb;
        c.t  = (f32)i / (f32)FPS;

        f64 a = now_sec();
        dm_job_for(H, render_row, &c);
        t_render += now_sec() - a;

        a = now_sec();
        dm_post_params prm = dm_post_defaults();
        prm.frame = (u32)i;
        dm_post_apply(post, fb, &prm);
        t_post += now_sec() - a;

        dm_fb_resolve(fb, rgb, 1.0f, (u32)i);
        if (vid) dm_video_write(vid, rgb);

        if (i == 0) {
            FILE *f = fopen("tools/out/smoke_frame0.ppm", "wb");
            if (f) {
                fprintf(f, "P6\n%d %d\n255\n", W, H);
                fwrite(rgb, 1, (size_t)W * H * 3, f);
                fclose(f);
            }
        }
        printf("\r[smoke] frame %d/%d", i + 1, frames);
        fflush(stdout);
    }

    f64 total = now_sec() - t0;
    printf("\n[smoke] scene %.1f ms/frame, post %.1f ms/frame, wall %.3f s\n",
           t_render * 1000.0 / frames, t_post * 1000.0 / frames, total);
    printf("[smoke] projected for a 2 min 60fps film: %.1f min of rendering\n",
           ((t_render + t_post) / frames) * 7200.0 / 60.0);

    dm_post_free(post);
    dm_video_close(vid);
    free(rgb);
    dm_fb_free(fb);
    dm_job_shutdown();
    return 0;
}
