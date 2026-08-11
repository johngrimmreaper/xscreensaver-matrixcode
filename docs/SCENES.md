# MatrixCode scene engine

MatrixCode can play deterministic scripted scenes before handing control to the
continuous rain renderer. Scenes are separate from the rain simulation so new
film recreations can be added without adding one-off state to `matrixcode.c`.

## Coordinate model

A scene is authored on a canonical 640x480 raster. The full composition is
scaled uniformly and centered in the actual drawable. This intentionally
differs from rain, whose logical grid adapts to the monitor/window aspect.

The distinction is deliberate:

- **scene:** preserve photographed composition;
- **rain:** preserve apparent glyph scale while filling the available display.

## `neo-trace` prototype

`neo-trace` is a standalone calibration scene for the transition that will
eventually precede `neo-terminal`.

The trace-program opening remains a clean-room MatrixCode invention inspired by
the screenplay's telephone-origin acquisition. The transition that follows is
based more closely on final-film reference frames: an aperture fills the view,
the image drops almost to black, a tiled green phosphor tunnel appears, the
tunnel overexposes, and the dark opening resolves into the counter (enclosed
negative space) of the lowercase `a` in `Searching...`.

The scene recreates the visual grammar rather than embedding film frames or
extracted artwork:

```text
 0.0 ->  5.5  racing trace columns and progressive numeric lock
 5.5 ->  7.0  camera push toward an abstract circular aperture
 7.0 ->  7.8  near-black transition beat
 7.8 -> 10.2  accelerating tiled phosphor tunnel
10.2 -> 10.9  severe green-white phosphor bloom
10.9 -> 12.8  tunnel resolves through the counter of lowercase `a`;
              camera pulls back to reveal `Searching...`
12.8 -> 14.0  current Morpheus-search landing display
```

Calibration checkpoints:

```sh
./matrixcode -window -scene neo-trace -scene-time 5.5
./matrixcode -window -scene neo-trace -scene-time 7.4
./matrixcode -window -scene neo-trace -scene-time 8.5
./matrixcode -window -scene neo-trace -scene-time 10.5
./matrixcode -window -scene neo-trace -scene-time 11.5
./matrixcode -window -scene neo-trace -scene-time 12.8
```

The `Searching...` reveal uses a small scene-local mixed-case bitmap alphabet so
the experimental transition does not change the already-calibrated takeover
message typography. Final timing, tunnel perspective, phosphor intensity and
the search-workstation layout remain subject to calibration against the
reference frames.


## Neo workstation visual language

The search display is intentionally **not** Matrix rain rendered inside a fake
window. It is treated as its own fictional late-1990s workstation environment,
derived from final-film visual references but implemented from clean-room
primitives.

The first workstation pass uses this dedicated palette:

```text
desktop / deep chrome   dark olive-charcoal
window chrome           muted gray-green
highlight edges         pale cyan-green
document paper          pale cyan-gray
normal editorial text   near-black
GLOBAL SEARCH banner    charcoal with pale lettering
Searching... box        very dark green
Searching... text       luminous phosphor green
photo/result imagery    high-contrast monochrome
```

The workstation renderer lives in `neo_workstation.c` rather than `scene.c`.
It owns the toolbar, beveled widgets, document cards, search overlay, clean-room
Morpheus silhouette, newspaper-like body texture and result choreography.

The current search timeline gradually layers:

```text
 0 ->  4 s   workstation chrome + GLOBAL SEARCH
 3 -> 10 s   Heathrow article enters and settles
 7 -> 14 s   monochrome Morpheus result panel enters
14 -> 21 s   international/An-Nahar-inspired result overlays the article
14 -> 23 s   Download activity appears while Searching... remains top-most
```

These durations are deliberately stretched across the existing 23-second
calibration window. Once the final scene timing is locked, the same renderer can
be retimed without changing its visual components.

## `neo-terminal`

The first scene is the computer takeover sequence near the beginning of the
first film. Its timing is expressed in absolute movie-relative seconds from the
first view of Neo's computer already searching for Morpheus.

Current first-pass cue table:

```text
 0.0 -> 23.0  search screen
23.0 -> 25.0  black screen, blinking cursor only
25.0 -> 29.0  type "Wake up, Neo..."
29.0 -> 40.0  hold
40.0 -> 47.0  type "The Matrix has you..."
47.0 -> 55.0  hold
55.0 -> 66.0  type "Follow the white rabbit."
66.0 -> 67.0  hold
67.0 -> 71.0  instantly show "Knock, knock, Neo."
71.0 -> 73.0  black
73.0 -> 76.0  fade in operator1999 rain
```

Typed messages derive their visible character count from the cue's explicit
start and end times rather than from one global typing-speed constant. This
makes each line finish at the measured cue boundary and naturally gives the
three messages different cadences.

The hijack terminal text uses a smaller scale than the search UI. In canonical
640x480 coordinates the 5x7 bitmap cells are about 14 pixels high, leaving the
large black field visible around the message and cursor.

The pre-hijack search display remains a clean-room approximation for now. The
23-second phase is intentionally long enough to establish the computer/search
context before the abrupt blackout. Its article layout, toolbar, floating
"Searching..." treatment and typography will be calibrated separately against
reference frames.

## Fidelity status

The scene engine, scaling, absolute timing model, cursor-only blackout, distinct
typing cadences, instant final message and rain transition are implemented.
The scene is **not yet claimed as pixel-for-pixel film accurate**. Exact glyph
shape, margins, line placement, search-page artwork, phosphor response and
frame timing should continue to be calibrated against user-supplied reference
frames from the film.

No extracted movie frame, official font, promotional glyph asset or third-party
texture is embedded in the repository.

## Runtime behavior

Default:

```sh
./matrixcode -window
```

Skip scenes:

```sh
./matrixcode -window -no-scene
```

Select the first scene explicitly:

```sh
./matrixcode -window -scene neo-terminal
```

For frame calibration, jump directly to a movie-relative scene time:

```sh
./matrixcode -window -scene neo-terminal -scene-time 23
./matrixcode -window -scene neo-terminal -scene-time 40
./matrixcode -window -scene neo-terminal -scene-time 55
./matrixcode -window -scene neo-terminal -scene-time 67
```

`-scene-time` accepts fractional seconds from zero through the selected scene's
duration and then continues playback normally.  It is a calibration/debugging
control rather than part of the normal XScreenSaver presentation.

For an exact deterministic screenshot at a cue boundary:

```sh
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a \
  ./matrixcode -window -geometry 1280x960 \
  -scene neo-terminal -scene-time 55 \
  -seed 19990331 -frames 1 -no-vsync \
  -screenshot neo-55s.ppm
```

This makes frame matching practical without waiting through the first minute of
the scene after every visual adjustment.

A settled window resize restarts the presentation clock.  Normal playback
restarts at scene time zero; calibration playback restarts at the explicit
`-scene-time` offset.  This keeps scene geometry deterministic and consistent
across XScreenSaver monitor windows.
