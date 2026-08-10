# Tuning the film look

Use `operator1999` as the baseline and change one family of variables at a time.

## 1. Composition first

```sh
./matrixcode -window -profile operator1999 -aspect 4:3 -columns 80
```

The 80-column setting defines the 4:3 reference density.  At 4:3 it produces
80x60 cells; at 16:9 the default `-aspect auto` extends the same cell size to
about 107x60 so the full window is populated.  Higher resolutions at the same
aspect keep that logical count and enlarge the glyphs.  If you prefer larger
glyphs, try 72 reference columns; for smaller glyphs, try 88 or 96.

## 2. Rain rhythm

Defaults:

```text
density 55
speed    40
trail   18
cycle   100
```

Try these variants:

```sh
# Slightly emptier / calmer
./matrixcode -density 48 -speed 35 -trail 20

# More crowded operator wall
./matrixcode -density 62 -trail 21
```

Avoid driving density to 80–90 unless you specifically want a modern "wall of
code" look.

## 3. CRT calibration

The default CRT values are deliberately modest:

```text
curvature      11
scanlines      32
phosphor-mask  14
vignette       30
persistence    14
overscan        2
```

On a 4K display, scanlines and the mask can tolerate slightly higher values.  On
1080p, large values quickly look like an emulator filter rather than a filmed
CRT.

Compare instantly with:

```sh
./matrixcode -profile operator1999 -no-crt
```

If the CRT layer is the first thing you notice from normal seating distance, it
is probably too strong.

## 4. Glow

Glow is part of the image, but too much is one of the easiest ways to get the
familiar modern neon-green Matrix-clone appearance.  Start at the default 76
and adjust in small steps.

## 5. Your monitor matters

OLED/high-contrast panels exaggerate the black/green separation.  IPS panels
may need slightly more contrast and less glow.  Very high-DPI monitors make the
virtual-raster/scanline treatment especially useful because otherwise the code
looks unnaturally vector-clean.
