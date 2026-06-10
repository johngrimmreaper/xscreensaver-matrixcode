# Research notes

The implementation was informed by public descriptions of how the original
code rain was designed and by close comparison with established recreations.

## Findings that affected the code

1. Production designer Simon Whiteley has described the source imagery as
   Japanese characters derived from cookbooks and manually adapted into the
   visual language of the film. This argues for a curated, irregular glyph set
   rather than normal Japanese text rendered from a font.
2. Accounts of the title sequence describe an earlier 3D/tumbling direction
   being rejected in favor of a more traditional, older and Japanese-feeling
   presentation. MatrixCode therefore remains resolutely 2D.
3. Rezmason's detailed reconstruction notes identify a particularly important
   behavior: characters remain on a grid while waves of illumination move down
   columns. They also note multiple drops, different speeds, pale leading
   symbols and a bloom/tone-map stage. MatrixCode adopts those *behaviors* but
   not that project's official-derived glyph assets or WebGL implementation.
4. Existing XScreenSaver conventions require a hack executable in the
   libexec directory and an XML settings description. MatrixCode supports root,
   owned-window and embedded-window operation as a standalone package.

## Clean-room boundary

No bitmap, vector glyph, font, shader, source file, or extracted asset from a
film, official promotional application, or another Matrix rain implementation
is present in this repository. The visual rules above were independently
implemented in C, and every glyph bitmap was drawn specifically for this
project.

## Public references

- WIRED, “The Matrix Code Came From Sushi Recipes” (2017):
  https://www.wired.com/story/the-matrix-code-sushi-recipe/
- befores & afters, interview material on the title design:
  https://beforesandafters.com/2019/03/31/the-matrix-code-how-the-iconic-opening-title-was-created/
- Rezmason/matrix design notes (MIT-licensed implementation):
  https://github.com/Rezmason/matrix
- XScreenSaver source and hacking documentation:
  https://www.jwz.org/xscreensaver/
- The user's standalone Mystify package used as a packaging-layout reference:
  https://github.com/johngrimmreaper/xscreensaver-mystify
