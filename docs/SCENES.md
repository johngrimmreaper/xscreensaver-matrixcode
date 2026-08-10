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

A settled window resize restarts the entire presentation at scene time zero,
which keeps scene geometry deterministic and consistent across XScreenSaver
monitor windows.
