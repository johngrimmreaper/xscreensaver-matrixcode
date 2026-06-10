#ifndef MATRIXCODE_GLYPHS_H
#define MATRIXCODE_GLYPHS_H

#include <stddef.h>
#include <stdint.h>

#define MATRIXCODE_GLYPH_WIDTH 8
#define MATRIXCODE_GLYPH_HEIGHT 12
#define MATRIXCODE_KANA_COUNT 33
#define MATRIXCODE_DIGIT_FIRST 33
#define MATRIXCODE_DIGIT_COUNT 10
#define MATRIXCODE_SYMBOL_FIRST 43

typedef struct matrixcode_atlas {
    unsigned int width;
    unsigned int height;
    unsigned int cell_width;
    unsigned int cell_height;
    unsigned int columns;
    unsigned int glyph_count;
    uint8_t *core_rgba;
    uint8_t *glow_rgba;
} matrixcode_atlas;

size_t matrixcode_base_glyph_count(void);
size_t matrixcode_total_glyph_count(void);
const char *matrixcode_glyph_name(size_t index);
const uint8_t *matrixcode_glyph_rows(size_t index);
int matrixcode_glyphs_self_test(void);
int matrixcode_build_atlas(matrixcode_atlas *atlas);
void matrixcode_free_atlas(matrixcode_atlas *atlas);

#endif
