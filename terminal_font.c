#include "terminal_font.h"

#if defined(MATRIXCODE_USE_MINIMAL_GL_HEADERS)
# include "compat/minigl.h"
#else
# include <GL/gl.h>
#endif

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct glyph5x7 { char ch; uint8_t row[7]; } glyph5x7;

#define G(ch,a,b,c,d,e,f,g) { ch, { a,b,c,d,e,f,g } }
static const glyph5x7 glyphs[] = {
G('A',14,17,17,31,17,17,17), G('B',30,17,17,30,17,17,30),
G('C',14,17,16,16,16,17,14), G('D',30,17,17,17,17,17,30),
G('E',31,16,16,30,16,16,31), G('F',31,16,16,30,16,16,16),
G('G',14,17,16,23,17,17,14), G('H',17,17,17,31,17,17,17),
G('I',31,4,4,4,4,4,31), G('J',7,2,2,2,18,18,12),
G('K',17,18,20,24,20,18,17), G('L',16,16,16,16,16,16,31),
G('M',17,27,21,21,17,17,17), G('N',17,25,21,19,17,17,17),
G('O',14,17,17,17,17,17,14), G('P',30,17,17,30,16,16,16),
G('Q',14,17,17,17,21,18,13), G('R',30,17,17,30,20,18,17),
G('S',15,16,16,14,1,1,30), G('T',31,4,4,4,4,4,4),
G('U',17,17,17,17,17,17,14), G('V',17,17,17,17,17,10,4),
G('W',17,17,17,21,21,21,10), G('X',17,17,10,4,10,17,17),
G('Y',17,17,10,4,4,4,4), G('Z',31,1,2,4,8,16,31),
G('0',14,17,19,21,25,17,14), G('1',4,12,4,4,4,4,14),
G('2',14,17,1,2,4,8,31), G('3',30,1,1,14,1,1,30),
G('4',2,6,10,18,31,2,2), G('5',31,16,16,30,1,1,30),
G('6',14,16,16,30,17,17,14), G('7',31,1,2,4,8,8,8),
G('8',14,17,17,14,17,17,14), G('9',14,17,17,15,1,1,14),
G('.',0,0,0,0,0,12,12), G(',',0,0,0,0,4,4,8),
G(':',0,12,12,0,12,12,0), G('-',0,0,0,31,0,0,0),
G('_',0,0,0,0,0,0,31), G('/',1,2,2,4,8,8,16),
G('?',14,17,1,2,4,0,4), G('!',4,4,4,4,4,0,4),
G('(',2,4,8,8,8,4,2), G(')',8,4,2,2,2,4,8),
G('[',14,8,8,8,8,8,14), G(']',14,2,2,2,2,2,14),
G('\'',4,4,8,0,0,0,0), G('"',10,10,0,0,0,0,0),
G('>',16,8,4,2,4,8,16), G('<',1,2,4,8,4,2,1)
};
#undef G

static const uint8_t *lookup(char ch)
{
    size_t i;
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
    for (i = 0U; i < sizeof(glyphs) / sizeof(glyphs[0]); i++)
        if (glyphs[i].ch == ch) return glyphs[i].row;
    return NULL;
}

void terminal_draw_text(const char *text, float x, float y, float pixel,
                        float red, float green, float blue, float alpha)
{
    float pen = x;
    size_t n;
    if (!text || pixel <= 0.0F) return;
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glColor4f(red, green, blue, alpha);
    glBegin(GL_QUADS);
    for (n = 0U; text[n] != '\0'; n++) {
        const uint8_t *rows = lookup(text[n]);
        unsigned int row, col;
        if (text[n] == '\n') { y += pixel * 9.0F; pen = x; continue; }
        if (rows) {
            for (row = 0U; row < 7U; row++) for (col = 0U; col < 5U; col++) {
                if ((rows[row] & (uint8_t)(1U << (4U - col))) != 0U) {
                    float px = pen + (float)col * pixel;
                    float py = y + (float)row * pixel;
                    glVertex2f(px, py); glVertex2f(px + pixel, py);
                    glVertex2f(px + pixel, py + pixel); glVertex2f(px, py + pixel);
                }
            }
        }
        pen += pixel * 6.0F;
    }
    glEnd();
    glEnable(GL_TEXTURE_2D);
}

float terminal_text_width(const char *text, float pixel)
{
    size_t len = text ? strlen(text) : 0U;
    return len == 0U ? 0.0F : ((float)len * 6.0F - 1.0F) * pixel;
}
