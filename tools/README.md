# tools

Programs that link the engine but belong to no demo. Each is a single file.

```powershell
..\build.ps1 -Tool smoke        # -> tools\out\smoke.exe
```

## `smoke.c` — engine shakedown

Renders a domain-warped noise field at full output resolution and answers the
only question worth asking before writing any scenes: how many seconds does one
frame cost on this machine, and does the whole pipeline — threads, HDR buffer,
tone map, ffmpeg pipe — hold together end to end.

```powershell
out\smoke.exe [frames] [width] [height]
```

## `fontsheet.c` — the typeface, on one page

Draws every glyph the stroke font knows at several sizes and weights. The font
is a list of line segments typed out by hand, so this is how a mistyped
coordinate gets caught: on a sheet, not in a finished render.

## `brand.c` — channel art

Renders the avatar, banner, watermark and video thumbnails using the same noise,
palette and font as the demos. A project whose whole claim is that nothing was
downloaded cannot go and download its own logo.

Driven by [`brand.ps1`](../brand.ps1) at the repository root, which also converts
the output to PNG. To render one piece by hand:

```powershell
out\brand.exe avatar
out\brand.exe banner
out\brand.exe watermark
out\brand.exe thumb <plate.ppm> <name> "LINE ONE" "LINE TWO"
```

Output is PPM, plus a PGM alpha mask for the watermark, written to `brand/work`.
Writing PPM is twelve lines of C; PNG is not, and ffmpeg already has to be
installed to render video.

The wordmark, monogram and straplines are `#define`s at the top of the file, so
a rename is one edit rather than four scattered string literals.

The thumbnail caption is an argument, not a constant, because the caption is the
part worth iterating on: it should say what the video title cannot, and which
claim that is only becomes clear once the title is written.
