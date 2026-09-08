# LIFTOFF

Thirty seconds, vertical, of something that actually exists. The two pieces
before this one were abstract on purpose — a fractal owes nobody an
explanation. A rocket does. So every proportion here is a real proportion, the
trajectory comes out of an integration rather than a curve editor, and the
numbers in the corner of the frame are the same numbers steering the camera.

1080×1920, 60 fps, 112 BPM, 14 bars — exactly 30.000 seconds, so the video is
cut to the bar rather than the bar padded to the video.

## Structure

| Bar | Time | Mission | Section | On screen |
|---|---|---|---|---|
| 0 | 0:00 | T−8 → T+0 | HOLD | The count. Floodlights, venting, and ignition three seconds before the drop. |
| 2 | 0:04.3 | T+0 → T+24 | LIFT | Release, on the first kick. A tracking camera on a lens that keeps getting longer. |
| 5 | 0:10.7 | T+24 → T+152 | CLIMB | A mounted camera. Max-Q, the sky going black, the vehicle tipping over. |
| 9 | 0:19.3 | T+152 → T+176 | STAGE | Cut-off. Everything drops out — there is no air. Separation. |
| 12 | 0:25.7 | T+176 → T+536 | ORBIT | The limb, the daylight, three lines and out to black. |

The piece loops: it ends on black and silence at exactly the point it begins,
because a short is watched twice before it is watched once.

## The clock

A launch to orbit takes nine minutes and this is thirty seconds long, so the
clock has to run fast. The way it does is the one structural idea in the piece:

**Mission time advances by a fixed number of seconds per beat, and the rate
changes only where the picture cuts.** One second per beat through the count,
two through the ascent, eight climbing out of the atmosphere, two again for the
separation, and forty-five for the coast to orbit.

Two things fall out of that. The countdown ticks on the beat, because a beat is
a second there. And the scheduled events of the flight land on beats without
being nudged: ignition at T−3 is five beats into the count, release is the
downbeat of bar 2, cut-off is the downbeat of bar 9, the second stage lights on
the downbeat of bar 10, and the burn ends exactly where the music does.

Maximum dynamic pressure is the interesting exception. Nothing schedules it —
it is where the drag curve and the atmosphere happen to cross — so it lands at
T+54.7, which is nowhere in particular. The caption for it does not carry a
timestamp at all; it asks the model when max-Q was and converts that back
through the clock. Change the pitch kick and the caption moves on its own.

## The trajectory

`flight.c` integrates a two-degree-of-freedom launch once at startup, at two
millisecond steps, and everything else samples it. Thrust falls out of mass
flow and ambient pressure; drag falls out of a Mach-dependent coefficient and
an exponential atmosphere; the attitude is a gravity turn after one 6.3° pitch
kick, and the upper stage flies linear tangent steering. Nothing is keyframed.

There are exactly four steering numbers in the file — when the kick starts and
ends, how big it is, and the tangent the upper stage steers to. Everything
below came out of the integration:

```
liftoff        1.43 g
max-Q          31.9 kPa at T+54.7, at 8.5 km
throttle       down to 74% through the transonic region
               and again from T+130, holding 4 g
MECO           T+152   82.0 km   2 447 m/s   57° from vertical
separation     T+155, at 4.5 m/s
SECO           T+536  167.0 km   7 735 m/s
```

Those are ordinary numbers for a 549-tonne two-stage kerosene/oxygen vehicle,
and they are ordinary because the model is, not because they were typed in.

`liftoff profile` prints the whole table. It is the mode this demo could not
have been made without: the trajectory drives every camera, so a shot that
looks wrong is usually a trajectory that is wrong, and reading it as numbers
takes a second where rendering it takes an hour.

## The light

The sun is 5.5° below the horizon, and that one number decides how the whole
piece is lit.

On the pad it means night: six xenon floodlights and a black sky with stars in
it. Three seconds before the release the engines light and become, by a very
wide margin, the brightest thing that has been on screen — and the vehicle, the
tower, the deck and every parcel of smoke are lit by that one source falling
off with the square of the distance, like any other.

Then the vehicle climbs, and the visible horizon drops away underneath it —
roughly √(2h/R), so nine degrees at eighty kilometres. At some altitude it
drops past the sun. For a sun 5.5° down that altitude is 29 km, which the
trajectory reaches ninety-five seconds in, which is two thirds of the way
through the climb. **Nothing animates a sunrise. The vehicle flies into one.**

The sky is single-scattered air: eight samples along the ray, each asked how
dense the air is there and whether the sun can see it, with Rayleigh and Mie
terms and the reddening that comes from the path length to the sun. The same
function draws a black sky at sea level and the blue arc along the limb from
two hundred kilometres, because the only thing that changed between them is the
altitude it was handed.

## The exhaust

Marched as a volume, and the shape is a function of the air rather than of the
section it appears in. A nozzle is cut for one ambient pressure: below it the
jet is squeezed into a narrow column with standing shock diamonds in it, and
above it there is nothing left to squeeze and the plume opens into a bell over
a hundred metres long. So the same code draws a hard white column at the pad
and a faint wide flare at eighty kilometres, and it is `lo_air_density` from
the trajectory that moves it between them. The colour is a blackbody at the
local gas temperature, so the flame is white at the throat and orange at the
tip because it is, not because a gradient was picked.

The smoke does one thing worth pointing at. A parcel hanging at nine hundred
metres is exactly as old as the time since the vehicle passed nine hundred
metres, so the integration is read backwards — `lo_flight_time_of_alt` — to
find out when that was and where the vehicle was when it happened. The column
therefore bends downrange, and spreads and fades with age, because the flight
did and it has.

## The vehicle

62.5 m on a 3.66 m body: a slenderness ratio of seventeen, which is why a
launch vehicle is recognisable from a silhouette alone and why it fits a
vertical frame without being cropped into one.

Cylinders, one truncated cone drawn through an eightfold fold of the angle for
the ring of engines, and an ellipsoid for the fairing — which gives exactly the
ogive profile r = R√(1−u²) with no special case at the tip. Almost none of what
makes it read as hardware is geometry, though. It is in the albedo: weld rings
every 3.05 m because that is what fits on the machine that welds them, a
raceway down one side, a black interstage, and frost on the top two thirds of
the first stage because that is where the oxygen is.

## Building and running

```powershell
..\..\build.ps1 03-liftoff
```

```powershell
out\liftoff.exe profile                     # the trajectory, as a table
out\liftoff.exe song                        # the soundtrack, as a WAV
out\liftoff.exe still 6.5 1080 1920 2       # one frame, as a PPM
out\liftoff.exe sheet 8 2 190 1             # the whole piece as a contact sheet
out\liftoff.exe render 1080 1920 2 1 15     # the film
```

The render writes a fragmented MP4, because that is the shape a file
being appended to one frame at a time wants and the index therefore ends
up at the end of it. `publish.ps1` remuxes it — no re-encode, a few
seconds — to move the index to the front, stamps the file with its own
title, author, licence and source URL, and writes it out under the name it
should be uploaded as:

```powershell
..\..\publish.ps1 03-liftoff
```

## What it costs

At 1080×1920 with two anti-aliasing samples, about 4.7 s a frame averaged over
the piece — which hides a factor of three and a half:

```
HOLD, LIFT     8.4 s    vehicle, tower, deck, exhaust and smoke, all marched
CLIMB          3.7 s
STAGE          3.0 s
ORBIT          2.4 s    sky, and one small object in it
```

So two and a half hours for the 1800 frames with one temporal subsample, and
five with two. Nothing in this piece moves fast enough in frame to need the
second one badly; the shot that comes closest is the release, where the vehicle
crosses about eight pixels a frame.

The two shots on the ground cost what they cost because they are four marched
things at once, and the smoke is most of it — the bounding cylinder around it
covers very nearly the whole frame from a camera fifty metres away, so almost
every pixel walks it. Three quarters of the original cost came off by bounding
each volume to what it can actually occupy and by refusing a noise lookup to
any sample too thin to survive compositing; the rest is the price of the shot.
