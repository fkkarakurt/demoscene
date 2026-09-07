#include "dm_audio.h"
#include "dm_rand.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

dm_audio *dm_audio_new(int frames)
{
    dm_audio *a = (dm_audio *)malloc(sizeof(dm_audio));
    if (!a) return NULL;
    a->n = frames;
    a->l = (f32 *)calloc((size_t)frames, sizeof(f32));
    a->r = (f32 *)calloc((size_t)frames, sizeof(f32));
    if (!a->l || !a->r) { free(a->l); free(a->r); free(a); return NULL; }
    return a;
}

void dm_audio_free(dm_audio *a)
{
    if (!a) return;
    free(a->l);
    free(a->r);
    free(a);
}

void dm_audio_clear(dm_audio *a)
{
    memset(a->l, 0, (size_t)a->n * sizeof(f32));
    memset(a->r, 0, (size_t)a->n * sizeof(f32));
}

void dm_audio_mix(dm_audio *dst, const dm_audio *src, f32 gain)
{
    int n = DM_MIN(dst->n, src->n);
    for (int i = 0; i < n; i++) {
        dst->l[i] += src->l[i] * gain;
        dst->r[i] += src->r[i] * gain;
    }
}

void dm_audio_stats(const dm_audio *a, f32 *peak_db, f32 *rms_db)
{
    f64 peak = 0.0, sum = 0.0;
    for (int i = 0; i < a->n; i++) {
        f64 l = a->l[i], r = a->r[i];
        f64 m = DM_MAX(fabs(l), fabs(r));
        if (m > peak) peak = m;
        sum += l * l + r * r;
    }
    f64 rms = sqrt(sum / DM_MAX(1.0, (f64)a->n * 2.0));
    if (peak_db) *peak_db = (f32)(20.0 * log10(DM_MAX(peak, 1e-9)));
    if (rms_db)  *rms_db  = (f32)(20.0 * log10(DM_MAX(rms,  1e-9)));
}

/* ---- WAV ---------------------------------------------------------------- */

static void put_u32(FILE *f, u32 v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); fputc((v >> 16) & 255, f); fputc((v >> 24) & 255, f); }
static void put_u16(FILE *f, u16 v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }

int dm_wav_write(const char *path, const dm_audio *a)
{
    FILE *f = fopen(path, "wb");
    if (!f) return 0;

    u32 data_bytes = (u32)a->n * 2u * 2u;   /* stereo, 16 bit */

    fwrite("RIFF", 1, 4, f);
    put_u32(f, 36 + data_bytes);
    fwrite("WAVE", 1, 4, f);

    fwrite("fmt ", 1, 4, f);
    put_u32(f, 16);            /* PCM chunk size */
    put_u16(f, 1);             /* format: PCM */
    put_u16(f, 2);             /* channels */
    put_u32(f, DM_SR);
    put_u32(f, DM_SR * 2 * 2); /* byte rate */
    put_u16(f, 4);             /* block align */
    put_u16(f, 16);            /* bits per sample */

    fwrite("data", 1, 4, f);
    put_u32(f, data_bytes);

    dm_rng rng;
    dm_rng_seed(&rng, 0x5eed17u);

    for (int i = 0; i < a->n; i++) {
        f32 s[2] = { a->l[i], a->r[i] };
        for (int c = 0; c < 2; c++) {
            /* TPDF dither at one LSB: the quantisation error becomes flat
             * noise instead of harmonic distortion in the quiet passages. */
            f32 d = (dm_rng_f(&rng) - dm_rng_f(&rng)) * (1.0f / 32768.0f);
            f32 v = dm_clamp(s[c] + d, -1.0f, 32767.0f / 32768.0f);
            i32 q = (i32)lrintf(v * 32767.0f);
            put_u16(f, (u16)(i16)q);
        }
    }

    fclose(f);
    return 1;
}
