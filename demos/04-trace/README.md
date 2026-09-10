# TRACE

Thirty seconds, vertical, in which nothing is drawn. The picture is the
soundtrack, plotted: the left channel across and the right channel up, which is
what an oscilloscope in XY mode shows when a stereo signal is fed into it, with
the past laid out in depth behind the present so that a camera has something to
move around.

Composed for 1080×1920 and rendered at 1440×2560, 60 fps, 120 BPM, 15 bars —
exactly 30.000 seconds, so the video is cut to the bar rather than the bar
padded to the video.

## Structure

| Bar | Time | Section | On screen |
|---|---|---|---|
| 0 | 0:00 | TUNE | A dot, then a circle. The harmonic series of A comes in a partial at a time, each one sliding into tune, and the figure turns until the interval is pure and then stops. On the last beat everything bends out of tune again and it spins. |
| 2 | 0:04 | PULSE | The kick on the beat, which is a spiral, and the chord on the off-beat, which is a star. They take turns, so each has the screen to itself. |
| 5 | 0:10 | CHAOS | The bass is the Lorenz system and the butterfly is its trajectory. Two bars of riff, then a climb, a snare roll and a push in. |
| 8 | 0:16 | DROP | Everything at once, seen from the side: each kick a funnel on a tower of time. |
| 13 | 0:26 | NAME | The channel's name, traced by the beam fifty-five times a second and heard as a low A. Then silence, which is a dot. |

The piece loops. Silence is a dot, and it ends on one exactly as it begins.

## The one idea

Every frame is computed from the finished, mastered soundtrack — the same
buffer that is written to the WAV — and from nothing else. A sample is a point:
left channel `x`, right channel `y`. Play the soundtrack into a real
oscilloscope in XY mode and it draws the same figures: the front of the frame,
without the past laid out behind it.

The only thing added is a third axis. A sample of age `a` is placed at
`z = -a × speed`, so the present is a plane and the last second and a half lies
behind it. Seen from straight in front, depth collapses and what is left is the
XY display. Seen from the side, one channel collapses into depth and what is
left is the other channel against time — the oscilloscope's other mode. **An
oscilloscope has two modes, and they are two sides of one object.**

Silence, seen from in front, is the whole time axis stacked onto one point:
the beam parked in the middle. That is why frame one is a dot.

## What that does to the music

Every sound is also a shape, and the two are not related by a mapping — they
are the same numbers. So the arrangement is written as much for the eye as for
the ear, and some ordinary facts about sound turn out to be facts about
geometry:

- **A pitch is how many times a second a shape is traced.** A sine in both
  channels a quarter cycle apart is a circle, and it is the same circle at
  every pitch. Only its size says anything.
- **A timbre is the shape itself.** A closed path is a sum of rotating circles
  — its Fourier series — and each circle is one harmonic. A figure with
  *n*-fold symmetry keeps only the harmonics one more than a multiple of *n*:
  a square has the odd harmonics, which is why a square wave does, and a circle
  has the fundamental alone.
- **A filter rounds corners off.** The bass in the drop is a square traced at
  the root, and its filter envelope opens it from a circle into corners and
  closes it again on every note. Resonance puts a small loop at each corner.
- **The tuning is visible.** Two voices in a small whole-number ratio trace a
  figure that closes and stands still. Two that are not trace one that never
  closes and turns instead, at the rate the interval beats: a tempered major
  third against 110 Hz turns 4.4 times a second. So the piece is in just
  intonation over A = 55 Hz, and the opening is the harmonic series itself —
  partials 4, 6, 10 and 14, the last of which no piano can play.
- **A kick drum is a spiral.** It is a sine falling from 190 Hz to 44 Hz in
  thirty milliseconds; put it in both channels a quarter cycle apart and the
  radius is the envelope.
- **A snare is a ring whose radius is noise** — a drum tone with the noise
  riding on it rather than beside it. Small and high, the same ring is a hat.

Two things follow that a normal mix would never have to think about.

Everything that sounds at once is drawn at once, on top of itself, and the
persistence stacks the last second of it behind. So the arrangement takes
turns: the kick has the downbeat to itself, the chord the off-beat, and noise —
which draws nothing but scribble — is kept off the screen except where it is
wrapped around a tone.

And there is no high-pass on the master, which the other pieces all have. A
filter is a phase shift, and a phase shift on a picture is a warp: a 12 Hz
high-pass turns the 55 Hz fundamental of the word by twenty-five degrees against
its harmonics and squeezes the end of the word back into its beginning. Every
instrument is built with no average offset instead, so there is nothing to take
out.

## The gain knob

The picture's size and the music's loudness are decoupled the way they are on a
bench: by the volts-per-division knob. Each section sets its own display gain,
so the mix is balanced by ear and the frame by eye. The intro is mixed to sit
under what follows it and shown at 1.8×; the drop is the loudest thing in the
piece and shown at 0.85×.

```
section   loudness    display gain
TUNE      -13.2 LUFS  1.80
PULSE     -11.5 LUFS  1.20
CHAOS     -12.6 LUFS  1.15    rising through the section
DROP      -10.4 LUFS  0.85
NAME      -11.9 LUFS  1.35
whole     -11.5 LUFS integrated, -1.0 dBFS peak
```

The mix is stereo in the most literal sense, which carries one risk a normal mix
does not: a figure lying along the other diagonal is two channels in opposite
phase, and summed to one speaker it cancels to nothing. `trace check` measures
it. Every section folds down to mono at −3 dB — the level of a circle — and
none comes anywhere near silence.

## The Lorenz system

`xy_lorenz` integrates σ = 10, ρ = 28, β = 8/3 with fourth-order Runge–Kutta at
audio rate and sends `x` to the left channel and `z` to the right. From in front
that is the butterfly — not a drawing of it, the trajectory itself, heard as a
growl.

To play it like a bass, its time has to be scaled to a pitch, and the rate
at which it orbits a wing is not something the equations give away. It was
measured: 1.3315 orbits per unit of the system's own time, with `z` averaging
23.55. Scale time by `hz / 1.3315` and the wing comes round at the note asked
for.

It is the only figure in the piece that never closes, so it is the build.
Everything before it settles and everything after it locks.

## The word

The engine's stroke font — every glyph a handful of line segments typed in by
hand — is exactly what a beam can follow. `xy_table_text` walks the strokes,
takes the nearest stroke end next so the pen-up moves are short, and traces the
lot 55 times a second. The shape of the letters is the timbre of the note.

The pen-up moves are drawn. There is no way to blank the beam of a scope in XY
mode, and this does not pretend otherwise: they are traced six times faster than
the strokes, and since the light a beam leaves is proportional to how long it
stays, a move six times faster is a line six times fainter.

## The beam

`beam.c` turns each sample interval into a short curve — Catmull–Rom through the
samples either side, which is roughly what a DAC's reconstruction filter does —
cut into pieces three pixels long. Each piece carries the light the beam
deposits in that time, whatever its length, which is what makes a slow stretch
of trace bright and a fast one faint. It is drawn analytically: a gaussian
across the line and a difference of two error functions along it, which rounds
the ends off by exactly the amount the next piece needs to continue without a
seam. A piece's pixels add up to its energy however it is blurred — checked to
within 1.5% over points, long diagonals and a 25-pixel blur, the loss being the
gaussian's tail past three sigma.

The gaussian and its integral are read from tables rather than computed: they
are all a pixel needs and both are smooth functions of one number, and as calls
to `expf` they were nine tenths of the cost of a frame. Far down the time axis,
where a sample moves half a pixel under a blur six pixels wide, consecutive
pieces are gathered into one for as long as the whole run fits inside its own
blur. Between the two, a frame went from about three seconds to about a quarter
of one. Against the exact version the picture is 60 dB PSNR away, and fewer than
one byte in a thousand differs by more than four levels.

The spot's width is the beam's own width projected, plus a thin-lens disc of
confusion for anything off the plane of focus, so the far end of the time axis
goes soft the way it would through a real lens.

The phosphor is three exponentials with a colour each: a white flash gone in two
milliseconds, which marks where the beam is *now*; a cyan persistence over a
quarter of a second that carries the figure; and an indigo tail three times as
long, so the time axis cools as it recedes rather than just dimming. The light
of each is integrated over the shutter, not sampled at an instant, and the
result saturates channel by channel, so the core of a very bright trace runs to
white.

## Building and running

```powershell
..\..\build.ps1 04-trace
```

```powershell
out\trace.exe song                            # the soundtrack, as a WAV
out\trace.exe check                           # level, width and mono fold-down per section
out\trace.exe still 1.6 1080 1920 2           # one frame, as a PPM
out\trace.exe sheet 10 3 216                  # the whole piece as a contact sheet
out\trace.exe clip 16 26 540 960 1 20         # one stretch, as video with its sound
out\trace.exe render 1440 2560 2 14           # the film
```

The soundtrack renders in under two seconds and is rendered first in every
mode, because it is the picture.

`render` takes width, height, the number of slices the exposure of each frame is
cut into, and the H.264 CRF. The camera and the time axis both move between
slices, which is the motion blur.

The film is rendered above the 1080×1920 it was composed for. It is thin lines
of light on black, which is the hardest picture there is for a video codec, and
an upload above 1080p is encoded better even for the viewers who watch it at
1080p. `publish.ps1` then remuxes it and stamps it for upload, as with LIFTOFF:

```powershell
..\..\publish.ps1 04-trace
```

## What it costs

Thirty-two and a half minutes for the 1800 frames at 1440×2560 with the
exposure cut in two — about 1.1 s a frame, of which the picture is:

```
TUNE    0.88 s    the largest figure in the piece, closest to the lens
PULSE   0.62 s
CHAOS   0.64 s
DROP    0.65 s
NAME    0.30 s    one short word and a short persistence
```

and the rest is x264 at its slower preset on 3.7 million pixels a frame. The
soundtrack takes 1.2 s.

A trace is cheap in a way a raymarched scene is not: its cost is the length of
the line on screen, not the number of pixels it might cover, and nearly every
pixel of this film is black.
