# engine

The shared code every demo and tool in this repository is built on. It is a
software renderer and a synthesiser, and nothing in it knows about any
particular demo.

Everything is C11. There are no third-party dependencies: libc, and Win32 for
the two files that touch the platform.

## Files

| File | What it does |
|---|---|
| `dm_base.h` | Scalar maths: clamps, easings, smoothstep, smooth min/max, the constants. |
| `dm_vec.h` | `v2` / `v3` / `v4` / `m3`, and the operations on them. |
| `dm_color.h` | sRGB encode and decode, ACES and Reinhard tone mapping, the house palette, HSV, blackbody, dither. |
| `dm_rand.h` | Hashes, a small PRNG, Perlin and ridged noise, domain warping, Worley, low-discrepancy sequences. |
| `dm_fb.c/.h` | The HDR framebuffer: three floats per pixel in linear light, with sampling, splatting and resolve. |
| `dm_font.c/.h` | A stroke font rasterised from a distance field. Every glyph is a handful of line segments typed out by hand, and the segments themselves are available to code that wants lines rather than pixels. |
| `dm_post.c/.h` | The post chain: bloom over a mip pyramid, chromatic aberration, barrel distortion, vignette, grain. |
| `dm_job.c/.h` | A work-stealing parallel-for over Win32 threads. Workers pull row indices off an atomic counter, which self-balances. |
| `dm_audio.c/.h` | Interleaved stereo float buffers, statistics, and a WAV writer. |
| `dm_synth.c/.h` | PolyBLEP oscillators, a TPT state-variable filter (exposed on its own for instruments that are not a patch), ADSR envelopes, a comb-and-allpass reverb, delay, compression. |
| `dm_video.c/.h` | The frame sink: raw RGB24 into an ffmpeg pipe, falling back to a numbered PPM sequence if ffmpeg is missing. |

## The two ideas worth knowing

**Everything renders in linear light.** `dm_fb` holds unclamped floats;
tone mapping and sRGB encoding happen once, in `dm_fb_resolve`, at the very end.
Anything that averages, blurs or downsamples gamma-encoded pixels darkens every
edge it touches, so nothing does.

**A scene is a pure function of time.** No state carries between frames. That is
what lets the renderer jump straight to any timestamp — invaluable when
iterating on one shot — and what guarantees a preview and a final render show
exactly the same picture.

## Portability

`dm_job.c` (threads) and `dm_video.c` (the pipe to ffmpeg) are the only files
that use Win32. Everything else is portable C11; a pthreads and `popen` port of
those two files is the whole job.
