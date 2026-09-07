# brand

Finished channel art, rendered by [`tools/brand.c`](../tools/brand.c) from the
same noise, palette and stroke font as the demos. Nothing here was drawn in an
image editor or downloaded from anywhere.

Every file below is ready to download and upload as-is.

## `channel/`

| File | Size | Where it goes |
|---|---|---|
| `avatar.png` | 800×800 | Profile picture. Cropped to a circle when displayed, so the rim sits well inside the inscribed circle and nothing of it is cut away. |
| `banner.png` | 2560×1440 | Channel art. All 2560×1440 shows on a TV, a 2560×423 strip on desktop, and 1546×423 on a phone. Every word lives inside that innermost box; the rest of the frame is background. |
| `watermark.png` | 300×300, RGBA | The corner watermark stamped onto video. Alpha follows the antialiased glyph edges and the glow, which is what keeps the mark out of a black square. |

## `thumbnails/`

| File | Plate | Caption |
|---|---|---|
| `coldstart-lines.png` | Mandelbox, t = 83 s | `5228 LINES` / `NO ASSETS` |
| `coldstart-assets.png` | Mandelbox, t = 83 s | `NO ASSETS` / `JUST C` |
| `coldstart-tunnel.png` | Tunnel, t = 19 s | `5228 LINES` / `NO ASSETS` |

All 1280×720, under the 2 MB an upload form will accept.

A thumbnail is looked at for a third of a second at about 210 px wide, so the
plate is pushed harder than the film ever would be — more saturation, and a
scrim sunk far enough at the bottom left for type to sit on it. Check any new
caption at 210 px before anything else: a caption that fails there has already
failed, however good it looks at full size.

## Regenerating

```powershell
..\brand.ps1                 # everything
..\brand.ps1 -Skip Thumbs    # channel art only
```

Thumbnails composite over stills the demo renderer produced, so they need those
stills first:

```powershell
..\demos\01-coldstart\out\coldstart.exe still 83 3840 2160 2
```

Intermediates land in `work/`, which is not tracked. The PNGs in `channel/` and
`thumbnails/` are tracked on purpose: they are the deliverable.

## House style

- Ground: deep indigo, `rgb(13, 20, 44)`. Darkness is the default and light is
  earned — every layer above the ground has to justify itself.
- Filaments: `dm_pal_house(0.56)`, the cold end of the palette. The magenta mids
  read as a plasma-ball stock image once they cover a whole frame, which is a
  background turning into the show.
- Type: warm white, `rgb(255, 246, 230)`. Second thumbnail line in amber,
  `rgb(255, 196, 120)`.
- Legibility at small sizes comes from sinking the background under the text,
  not from making the letters brighter. Brighter letters just bloom into a blob.

Adjust the gain constants in `tools/brand.c` rather than retouching the output —
otherwise the next regeneration silently undoes the edit.
