# COLD START

A two-minute demo. 1920×1080, 60 fps, and a soundtrack, all computed from
source at render time.

Watch it: https://youtu.be/6soV2AfSZq8

## Scenes

Each scene owns the whole frame and is a pure function of time, so any timestamp
can be rendered on its own without playing what came before.

| Time | Scene | Technique |
|---|---|---|
| 0:00 | VOID | Volumetric dust, domain-warped fBm |
| 0:08 | IGNITE | Accelerating starfield, motion-streaked trails |
| 0:16 | TUNNEL | Raymarched tunnel, curved spine, six-fold fluting |
| 0:32 | PLASMA | Smooth-minimum metaballs, glass refraction |
| 0:48 | MIRROR | Analytic mirror plane, procedural sky and stars |
| 0:56 | FRACTURE | Block displacement, RGB separation, scanline tearing |
| 1:04 | FRACTAL | Mandelbox, orbit-trap colouring, soft shadows and AO |
| 1:36 | LATTICE | Infinite crystal lattice, copper bars |
| 1:52 | END | Logo, sine scroller, greetings |

`timeline.c` dispatches to the scene that owns the current bar and lays the
overlays on top. `demo.h` carries the shared context: the time, the musical
clocks derived from it, and the beat and bar envelopes every animated value in
the demo is driven off. Nothing moves on wall time, which is what keeps the
picture in step with the music.

## Soundtrack

`song.c` — 120 BPM, 60 bars, synthesised sample by sample. PolyBLEP oscillators
into a TPT state-variable filter, ADSR envelopes, a comb-and-allpass reverb
network, ping-pong delay, and a sidechain compressor keyed off the kick. Master
sits at −10.6 LUFS with 13.1 LU of range.

## Building and running

```powershell
..\..\build.ps1 01-coldstart
```

```powershell
out\coldstart.exe song                        # the soundtrack only, as a WAV
out\coldstart.exe still 83 1920 1080 2        # one frame at t=83s, as a PPM
out\coldstart.exe sheet 4 3 640 1             # a contact sheet of the whole demo
out\coldstart.exe clip 60 70 1280 720 1 1     # one range, as video
out\coldstart.exe render 1920 1080 2 2 14     # the film
```

`render` takes width, height, anti-aliasing samples, temporal subsamples and the
H.264 CRF. Two temporal subsamples give real motion blur: the frame is rendered
twice across a 0.75 shutter and averaged, which is close to the 270° shutter
most film is shot at.

## What it costs

The published render was 1920×1080 at 60 fps with 2 AA samples and 2 temporal
subsamples: **3.38 seconds per frame, 6 hours 45 minutes total** on a four-core
laptop. 4K measures at 4.71 s/frame.

That is not real time and does not need to be. A frame computed in three seconds
still plays at 60 fps in a video file, which is what buys the sample counts and
the motion blur.

## Adding a scene

1. Write `scenes/sc_yours.c` with one `void scene_yours(dm_fb *fb, const yk_ctx *c)`.
2. Declare it in `demo.h` next to the others.
3. Give it a bar range in `timeline.c`.

`build.ps1` globs `scenes\*.c`, so there is nothing else to register.
