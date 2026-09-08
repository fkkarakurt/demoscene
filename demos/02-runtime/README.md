# RUNTIME

Thirty seconds, vertical, whose subject is the renderer itself. The picture is
not simply shown; it is shown being computed, in the order the thread pool
actually walks it.

1080×1920, 60 fps, 128 BPM, 16 bars — exactly 30.000 seconds, so the video is
cut to the bar rather than the bar padded to the video.

## Structure

| Bar | Time | Section | On screen |
|---|---|---|---|
| 0 | 0:00 | ASSEMBLE | The frame fills in row by row. No drums. |
| 2 | 0:03.8 | LOCK | Kick and bass land on the finished picture. |
| 6 | 0:11.3 | DRIVE | Cut to a low camera, full arrangement. |
| 11 | 0:20.6 | EXPOSE | Everything drops. The waveform draws itself. |
| 14 | 0:26.3 | SIGN | One chord, three lines, out to black. |

The piece loops: it ends on black and silence at exactly the point it begins,
because a short is watched twice before it is watched once.

## The reveal

The engine renders one row per work item and eight workers pull row indices off
a shared counter, so during a real render there is a ragged front a few rows
deep with finished pixels above it and untouched memory below. `rt_reveal` in
`timeline.c` draws precisely that, slowed from milliseconds to seconds: rows
past the front are cleared to the colour of nothing, rows inside the band are
faded up, and the leading edge is lit.

The only liberty taken is the hot edge itself. Everything else is a description
of what the renderer is doing.

## The waveform

`sc_expose.c` reads the audio buffer the piece is playing and draws one column
of pixels per 1.7 ms, spanning exactly one bar, with the playhead where the
sound you are hearing is. Minimum and maximum per column, the same thing an
audio editor draws. Nothing is stylised: what is on screen is what is in the
WAV, which is the point — the claim that the music is computed too is otherwise
invisible in a video.

The arrangement drops to a single bell tone for those three bars so there is
something legible to look at.

## The structure

A Menger sponge with a small rotation applied between levels, repeated every
two units of height. The sponge spans [-1, 1], so the repeat stacks copies face
to face into a column with no gap and no overlap; the seam every two units
reads as a storey and is what gives the ascent its sense of speed.

The sponge was chosen over the folded fractals in `01-coldstart` for one
reason: it is crisp. Folded distance estimators drift toward the organic, and
an organic blob does not say "this was computed" — it says the opposite. Square
openings, flat faces and right angles all the way down do.

Two cameras, one world. Cutting between two views of the same structure reads
as intent; cutting between two worlds in thirty seconds reads as a compilation.

## Building and running

```powershell
..\..\build.ps1 02-runtime
```

```powershell
out\runtime.exe song                        # the soundtrack, as a WAV
out\runtime.exe still 9.5 1080 1920 2       # one frame, as a PPM
out\runtime.exe sheet 8 2 200 1             # the whole piece as a contact sheet
out\runtime.exe render 1080 1920 2 2 15     # the film
```

The soundtrack is rendered before any picture in every mode, because the
waveform scene draws the audio buffer and would otherwise be handed a silent
one.

## What it costs

2.4 s per frame at 1080×1920 with two anti-aliasing samples; doubled again for
two temporal subsamples, so about 4.8 s per frame and a little over two hours
for the 1800 frames.
