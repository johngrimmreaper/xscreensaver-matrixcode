#ifndef MATRIXCODE_TERMINAL_FONT_H
#define MATRIXCODE_TERMINAL_FONT_H

void terminal_draw_text(const char *text, float x, float y, float pixel,
                        float red, float green, float blue, float alpha);
float terminal_text_width(const char *text, float pixel);

#endif
