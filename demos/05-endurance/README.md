# ENDURANCE

Two minutes, landscape, and a true story. Shackleton's ship, from the winter
she spent frozen into the Weddell Sea to the afternoon the ice sank her, down
three thousand metres of water, and up again in the lamps of the vehicle that
found her in 2022.

Composed and rendered at 3840×2160, 24 frames a second, 60 BPM, 30 bars —
exactly 120.000 seconds. The picture is 2.39:1 inside the 16:9 frame. Every
frame is shot through a model of a film camera: a Super 35 gate, a real focal
length and f-stop, and a 180° shutter.

## Structure

| Bar | Time | Act | On screen |
|---|---:|---|---|
| 0 | 0:00 | WINTER | 22 June 1915, nine in the evening, 74° S. The ship held fast in the pack, lit by the moon, the stars and her own lamps. Then down on the ice beside her, tilting up the frozen rigging to the moon. |
| 8 | 0:32 | PRESSURE | 27 October: the light at her stern, until a squeeze of the floes cuts it. 28 October: the fore topmast and then the mainmast come down. |
| 14 | 0:56 | SINKING | 21 November, just before five in the afternoon, a mile and a half away across the pack. Her stern rises, she dives, the ice closes. Then under the ice as she comes through. |
| 19 | 1:16 | DEEP | Following her down. The light goes red first, then green, then blue, then all of it, and the gauge runs to 3,008 metres. At the end only the animals she touches make any light. |
| 25 | 1:40 | FOUND | 5 March 2022. The lamps find her wheel, still standing, and then her name. |

## What is true

The film takes nothing on screen further than the record goes, and where the
record stops it says so here. Sources are at the end.

- **The date, place and hour of every shot are real, and so is the light.**
  The sun and moon are placed by an ephemeris for that date, hour and position
  — the drift position around Midwinter's Day, the abandonment position on
  27 October, Worsley's position for the sinking — and the sky is computed
  from where they were. On the night of 22 June 1915 the moon was eleven days
  old and a third of the way up the north-western sky. The sun was still 23°
  up in the west when she sank.
- **What lights the winter is what lit it.** The expedition's auroras were
  "few and rather poor", so the aurora is there but faint. Hurley rigged "two
  powerful lights on poles projecting from the ship to port and starboard" to
  light the dogs' kennels on the floe; they are in the first two shots, and so
  are the kennels. That the lamps were lit on that particular night is not
  recorded.
- **The second act is two sentences from *South*.** "The electric light
  gleamed from the stern of the dying *Endurance*" on the night of the 27th,
  until a squeeze of the ice cut the connection. The next day "the foretop and
  topgallant-mast came down with a run", and "the main-mast followed
  immediately, snapping off about 10 ft. above the main deck". The spars fall
  the way a rod pivoting about its break falls under gravity — θ'' = (3g/2L)
  sin θ, integrated in millisecond steps, from the twelve degrees she was
  already heeled.
- **The sinking follows the diary Shackleton quotes.** "She's going, boys!" —
  "She went down bows first, her stern raised in the air. She then gave one
  quick dive and the ice closed over her for ever." She was mastless by then
  and her funnel was still standing, and she is both here. The camp was about a
  mile and a half off; the camera is there, on a lens no one on the expedition
  had.
- **"Every one of her 28 men survived"** is true of *Endurance*'s own crew.
  The expedition's Ross Sea party, on the other side of Antarctica, lost three,
  which is why the caption says *her* men.
- **106 years**, not 107: 21 November 1915 to 5 March 2022.
- **The wreck is as the 2022 survey found her**: upright on soft mud, lying
  roughly north–south, masts down, the wheel and taffrail standing, "ENDURANCE"
  arced across the stern under the taffrail with a five-pointed star below it
  between flourishes, white and orange anemones and stalked sea lilies on her.
  She is Historic Site and Monument No. 93 under the Antarctic Treaty.

What is *not* from the record: the camera positions and moves; the exact
stations of the boats and the number of kennels; the shape of every floe and
ridge; the time the fall takes (a hull sinks at a few metres a second, so
three kilometres took her a quarter of an hour or more — here it takes thirty
seconds, though the light at each depth is the light at that depth); and the
bioluminescence, which is what animals at those depths do when something
touches them, not something anyone saw.

## The camera

A pixel is not a colour, it is a question about light: what arrives on this
patch of the gate, through this lens, while the shutter is open. So each
frame's exposure is cut into slices through the half of the frame interval the
shutter is open, the scene is set up once per slice, and every sample of a
pixel is dealt a different slice and a different point on the aperture. Motion
blur and depth of field are not effects applied afterwards; they are what the
samples average to. A pixel's samples are a low-discrepancy set rotated by a
hash of the pixel, so neighbouring pixels do not see the same instant through
the same corner of the lens.

The sampler takes two passes. One sample goes through every pixel; then a
pixel gets the rest only if its log-luminance differs from a neighbour's by
more than about ten per cent. Flat snow in focus gets one sample and the
rigging in front of it gets all of them.

Rigging is too thin to march — a shroud is two pixels wide from forty metres —
so ropes are drawn as anti-aliased lines with a depth test, widened by their
own circle of confusion, their brightness spread so a rope out of focus
carries the same light as one in focus.

Highlights roll off logarithmically before the bloom sees them, the way a
negative saturates: a lamp is a hundred thousand times brighter than the snow
it lights, and without that it flares a quarter of the frame white.

## The sky

Sun and moon positions come from the low-precision formulas of the
*Astronomical Almanac*, with the moon's parallax corrected for the observer,
which moves it by up to a degree. The moon's brightness follows its phase angle
and its disc is lit from the sun's direction, maria and all.

The air is single-scattering Rayleigh and Mie with ozone absorption, which is
what turns the twilight band blue rather than grey, integrated along each view
ray with the Earth's shadow taken into account. It is tabulated once per frame
over a 512×256 map of directions — square root of elevation, so the rows crowd
toward the horizon where the colour changes — and rebuilt only when a light
has moved by a tenth of a degree. The same integral, taken over the
hemisphere, is the ambient light on everything. Stars have their physical flux
rather than a look.

## The ice

The pack is a height field: Voronoi floes 140 m across, jammed together. Each
seam between two floes is, at random, a refrozen lead, a crack, or a pressure
ridge — a sail of rubble blocks, each a tilted slab on a grid, higher where the
pressure is working. Fields of rafted blocks lie out on the floes. Snow buries
the small blocks and fills the troughs as the winter goes on, and the wind
carves it into ribs. The ice heaps against the hull wherever it meets it.

A ray a kilometre out cannot afford that function, so the pack is baked around
each shot into a clipmap: twelve levels of 1024², each twice the size of the
last, from 1 cm texels to 20 m. Every 16×16 block keeps its highest point and
steepest slope, and coarser tiers of 64×64 and 256×256 keep their highest
point, so a ray a metre above the snow crosses open pack in a handful of steps.
Texels with a cliff in them — a block face, the edge of a lead — are flagged,
and near the lens, where bilinear heights would turn a face into a staircase,
the march asks the function itself there, but only once it is down among the
texels' own heights.

## The ship

She is lofted, not modelled: a plan-view waterline and a family of cross
sections, fuller amidships and finer at the ends, swept into a distance field,
with her counter stern built as a swelling ellipse over the sternpost.
Dimensions are hers — 43.9 m, 7.62 m beam, 4.80 m moulded depth — and so is the
layout from photographs and plans: raised fo'c'sle, deckhouses, funnel, three
boats in davits, the wheel aft. The name on the stern is the engine's stroke
font, set on an arc with thick and thin strokes and serifs, raised off the
planking in gilt, over a five-pointed star.

The rig is in sections — lower masts, topmasts, topgallant, bowsprit, funnel —
each a group of tapered spars with its own pivot, so a section can hang,
break, fall or be gone. Standing rigging is a list of ropes between points on
those sections, and a rope whose ends have moved apart by more than three per
cent has parted.

A ray tests only the sections whose bounding boxes it crosses, which halved the
cost of every frame with the ship in it.

## The water

Water is blue for the reason ice is: it absorbs red. Absorption and scattering
are per channel, for clear Antarctic water. Daylight under the surface is
refracted into Snell's window, attenuated to each point's depth, and scattered
into every view ray along its length, in closed form. Through the leads it
comes down in shafts, marched with a Henyey–Greenstein phase function; through
the floes it arrives as a faint blue-green glow over the whole underside of the
pack.

On the way down, the scene is only the few hundred metres of water round her,
but the light in it is the light of the depth on the gauge: red is gone in the
first tens of metres, green within a hundred, blue within a few hundred. Past
that the only light is bioluminescence, sparks where her leading edges touch
the animals in the water, swept back over the hull by the water going past.

At 3,008 m the only light is the vehicle's two lamps, with their beams
scattered in the water by equiangular sampling — samples spread evenly in the
angle a ray subtends at the lamp, which is where the light is — and the
picture white-balanced the way an underwater camera is.

## The sound

The score and the sound are two layers, mixed apart.

The score is a small string section, a solo voice for the theme, a celesta for
the cold, a drone and, under the second act, a low bowed pulse: D minor for the
ice and the sinking, D major for the finding. The theme — four notes that rise
and fall back — is heard under the ship at midwinter, falls as her stern rises,
rises for "every one of her 28 men survived", falls for "no one saw her again",
and comes back in major as her name comes up.

The sound is the story told without notes, and all of it is synthesis: wind
over the pack as noise through a wandering band-pass; the ice grinding as
stick-slip friction, a train of small slips quickening as the load builds and
ringing a resonant filter; timber breaking as a report, a scatter of splinters
and the thump of the spar's weight; the dynamo hum of the stern light, switched
by exactly the flicker the picture shows; bubbles as rising pitched blips;
the vehicle's thrusters; a sonar ping and its echo. Every event the picture
and the sound both have to hit is a cue in `song.h` that both read.

From the cut under the ice to the bottom the sound effects are low-passed — the
camera is under water, the orchestra is not.

## Building and running

From the root of the repository, which is where the program expects to be run:

```powershell
.\build.ps1 05-endurance
```

```powershell
$e = ".\demos\05-endurance\out\endurance.exe"
& $e song                        # the soundtrack, as a WAV
& $e still 50 1920 2             # one frame at t = 50 s, 1920 wide, 2 samples, as a PPM
& $e sheet 5 6 384 1             # the whole film as a contact sheet
& $e clip 56 76 960 1 20         # one stretch, as video with its sound
& $e render 3840 3               # the film, as numbered frames; resumable
& $e encode 3840 16              # frames and soundtrack into the MP4
```

`render` writes each frame to `out/frames` as it finishes and skips frames that
are already there, so a render that is stopped picks up where it left off.
`PROFILE=1` prints where a frame's time went.

`plate` renders the same instant filling the whole 16:9 frame, with no bars
and no captions, for a thumbnail to be composited on (`tools/brand.c`).

Third-party libraries: none. A trained denoiser would have cut the render time
a long way, and its weights would have been an asset in all but name.

## What it costs

17.6 hours for the 2,880 frames at 3840×2160 with two samples a pixel, on a
four-core laptop CPU (an i7-7700HQ, eight threads): 22.0 s a frame on average.

```
WINTER-WIDE   10.6 s    one bake serves the whole shot; the pack is far away
WINTER-SHIP   15.4 s    the ice at the camera's feet, and the rigging
PRESS-NIGHT   36.3 s    the pack re-baked eight times a second as it moves
PRESS-MASTS   30.6 s    blocks close to the lens, marched against the function
SINK-FAR      22.1 s    a 400 mm lens across a mile and a half of pack
SINK-UNDER    31.7 s    shafts of light, marched through the leads
DEEP-FALL     10.8 s
DEEP-DARK     13.1 s    past the light: the hull marched for depth alone
FOUND-WHEEL   35.8 s    two lamps' beams scattered in the water, marine snow
FOUND-STERN   25.5 s
```

Most of the cost is in the ice. A ray that grazes the pack from a camera a
metre and a half up crosses kilometres of it, and before the maps got their
coarser tiers of maxima, a frame of the second act spent three quarters of its
time there. The soundtrack takes about forty seconds, and x264 at its slow
preset another twenty-three minutes.

## Sources

- Shackleton, E. H., *South* (1919). Project Gutenberg:
  https://www.gutenberg.org/ebooks/5199
- UK Antarctic Heritage Trust / Historic England, *Endurance* Conservation
  Management Plan (2023), submitted to ATCM 46:
  https://documents.ats.aq/ATCM46/att/ATCM46_att010_e.pdf
- Endurance22, "Endurance is found" (9 March 2022) and the expedition blog:
  https://endurance22.org/endurance-is-found
- Antarctic Treaty Secretariat, Measure designating HSM 93:
  https://www.ats.aq/devAS/Meetings/Measure/768
- Bergman, J. and Stuart, M., *Records of the Canterbury Museum* 32 (2018),
  67–98, for the drift position around Midwinter's Day.
- Pope, R. M. and Fry, E. S., "Absorption spectrum (380–700 nm) of pure water",
  *Applied Optics* 36 (1997); Morel, A. and Maritorena, S., "Bio-optical
  properties of oceanic waters", *JGR* 106 (2001).
- *Astronomical Almanac*, low-precision formulae for the sun and moon.
