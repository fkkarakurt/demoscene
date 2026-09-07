/* fontsheet.c -- renders every glyph plus a few specimens, so the hand-typed
 * stroke tables can actually be proofread instead of trusted. */
#include "../engine/dm_fb.h"
#include "../engine/dm_font.h"
#include "../engine/dm_job.h"
#include "../engine/dm_post.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    const int W = 1920, H = 1080;
    dm_job_init(0);

    dm_fb *fb = dm_fb_new(W, H);
    u8    *rgb = (u8 *)malloc((size_t)W * H * 3);
    dm_fb_clear(fb, dm_rgb8(6, 7, 12));

    /* Rows 1-4: the full printable range, small and plain. */
    dm_text t = dm_text_default();
    t.size     = 46.0f;
    t.weight   = 2.4f;
    t.color    = dm_rgb8(150, 175, 205);
    t.additive = 0;

    const char *rows[4] = {
        " !\"#$%&'()*+,-./",
        "0123456789:;<=>?",
        "@ABCDEFGHIJKLMNO",
        "PQRSTUVWXYZ[\\]^_"
    };
    for (int i = 0; i < 4; i++)
        dm_text_draw(fb, V2(90.0f, 130.0f + (f32)i * 78.0f), rows[i], &t);

    /* A big specimen with a glow, which is how the demo will actually use it. */
    dm_text big = dm_text_default();
    big.size      = 150.0f;
    big.weight    = 6.0f;
    big.tracking  = 0.18f;
    big.color     = v3_scl(dm_rgb8(255, 120, 235), 3.2f);
    big.glow      = 11.0f;
    big.glow_gain = 0.55f;
    dm_text_draw_centered(fb, V2(W * 0.5f, 560.0f), "KORMOS", &big);

    /* A thin, cool line under it: the tagline register. */
    dm_text sub = dm_text_default();
    sub.size     = 34.0f;
    sub.weight   = 1.9f;
    sub.tracking = 0.42f;
    sub.color    = v3_scl(dm_rgb8(110, 220, 255), 1.5f);
    sub.glow     = 5.0f;
    sub.glow_gain = 0.3f;
    dm_text_draw_centered(fb, V2(W * 0.5f, 690.0f), "NO ASSETS - ONLY CODE", &sub);

    /* A sine scroller, proving per-character placement and rotation. */
    dm_text scr = dm_text_default();
    scr.size      = 62.0f;
    scr.weight    = 3.2f;
    scr.color     = v3_scl(dm_rgb8(255, 205, 120), 2.4f);
    scr.glow      = 8.0f;
    scr.glow_gain = 0.4f;

    const char *msg = "GREETINGS TO EVERYONE STILL WRITING C IN 2026...";
    f32 adv = dm_text_advance(&scr);
    f32 x = 60.0f;
    for (const char *p = msg; *p; p++, x += adv) {
        f32 ph = x * 0.006f;
        f32 y  = 900.0f + sinf(ph) * 55.0f;
        dm_char_draw(fb, (unsigned char)*p, V2(x, y), cosf(ph) * 0.22f, &scr);
    }

    dm_post *post = dm_post_new(W, H, 6);
    dm_post_params prm = dm_post_defaults();
    prm.bloom_intensity = 0.11f;
    dm_post_apply(post, fb, &prm);

    dm_fb_resolve(fb, rgb, 1.0f, 0);
    FILE *f = fopen("tools/out/fontsheet.ppm", "wb");
    if (f) {
        fprintf(f, "P6\n%d %d\n255\n", W, H);
        fwrite(rgb, 1, (size_t)W * H * 3, f);
        fclose(f);
        printf("[font] wrote out/fontsheet.ppm\n");
    }

    dm_post_free(post);
    free(rgb);
    dm_fb_free(fb);
    dm_job_shutdown();
    return 0;
}
