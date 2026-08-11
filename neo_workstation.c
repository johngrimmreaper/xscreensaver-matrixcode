#include "neo_workstation.h"

#if defined(MATRIXCODE_USE_MINIMAL_GL_HEADERS)
# include "compat/minigl.h"
#else
# include <GL/gl.h>
#endif

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#define NW_W 640.0F
#define NW_H 480.0F

typedef struct nw_color {
    float r, g, b, a;
} nw_color;

typedef struct nw_glyph {
    char ch;
    uint8_t row[7];
} nw_glyph;

static const nw_color NW_DESKTOP      = { 0.055F, 0.075F, 0.058F, 1.0F };
static const nw_color NW_CHROME       = { 0.34F,  0.42F,  0.35F,  1.0F };
static const nw_color NW_CHROME_LIGHT = { 0.62F,  0.73F,  0.67F,  1.0F };
static const nw_color NW_CHROME_DARK  = { 0.10F,  0.15F,  0.11F,  1.0F };
static const nw_color NW_PAPER        = { 0.72F,  0.84F,  0.80F,  1.0F };
static const nw_color NW_PAPER_ALT    = { 0.63F,  0.76F,  0.71F,  1.0F };
static const nw_color NW_INK          = { 0.018F, 0.025F, 0.020F, 1.0F };
static const nw_color NW_BANNER       = { 0.025F, 0.038F, 0.028F, 1.0F };
static const nw_color NW_BANNER_TEXT  = { 0.78F,  0.88F,  0.82F,  1.0F };
static const nw_color NW_SEARCH_BG    = { 0.010F, 0.055F, 0.018F, 0.97F };
static const nw_color NW_SEARCH_EDGE  = { 0.10F,  0.67F,  0.20F,  0.90F };
static const nw_color NW_SEARCH_TEXT  = { 0.24F,  1.00F,  0.34F,  0.96F };
static const nw_color NW_IMAGE_DARK   = { 0.012F, 0.018F, 0.014F, 1.0F };
static const nw_color NW_IMAGE_MID    = { 0.13F,  0.19F,  0.15F,  1.0F };

#define G(ch,a,b,c,d,e,f,g) { ch, { a,b,c,d,e,f,g } }
static const nw_glyph nw_upper[] = {
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
    G('-',0,0,0,31,0,0,0), G('.',0,0,0,0,0,12,12),
    G(':',0,12,12,0,12,12,0), G('/',1,2,2,4,8,8,16)
};
#undef G

static const uint8_t *nw_upper_rows(char ch)
{
    size_t i;
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
    for (i = 0U; i < sizeof(nw_upper) / sizeof(nw_upper[0]); i++)
        if (nw_upper[i].ch == ch) return nw_upper[i].row;
    return NULL;
}

static const uint8_t *nw_search_rows(char ch)
{
    static const uint8_t upper_s[7] = { 15,16,16,14,1,1,30 };
    static const uint8_t lower_e[7] = { 0,0,14,17,31,16,14 };
    static const uint8_t lower_a[7] = { 0,0,14,1,15,17,15 };
    static const uint8_t lower_r[7] = { 0,0,22,25,16,16,16 };
    static const uint8_t lower_c[7] = { 0,0,14,16,16,17,14 };
    static const uint8_t lower_h[7] = { 16,16,22,25,17,17,17 };
    static const uint8_t lower_i[7] = { 4,0,12,4,4,4,14 };
    static const uint8_t lower_n[7] = { 0,0,22,25,17,17,17 };
    static const uint8_t lower_g[7] = { 0,0,15,17,15,1,14 };
    static const uint8_t period[7] = { 0,0,0,0,0,12,12 };

    switch (ch) {
    case 'S': return upper_s;
    case 'e': return lower_e;
    case 'a': return lower_a;
    case 'r': return lower_r;
    case 'c': return lower_c;
    case 'h': return lower_h;
    case 'i': return lower_i;
    case 'n': return lower_n;
    case 'g': return lower_g;
    case '.': return period;
    default: return NULL;
    }
}

static float nw_clamp(float value)
{
    if (value < 0.0F) return 0.0F;
    if (value > 1.0F) return 1.0F;
    return value;
}

static float nw_smooth(float value)
{
    float x = nw_clamp(value);
    return x * x * (3.0F - 2.0F * x);
}

static float nw_stage(double t, double start, double end)
{
    if (t <= start) return 0.0F;
    if (t >= end || end <= start) return 1.0F;
    return nw_smooth((float)((t - start) / (end - start)));
}

static void nw_quad(float x0, float y0, float x1, float y1, nw_color c)
{
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(c.r, c.g, c.b, c.a);
    glBegin(GL_QUADS);
    glVertex2f(x0, y0); glVertex2f(x1, y0);
    glVertex2f(x1, y1); glVertex2f(x0, y1);
    glEnd();
    glEnable(GL_TEXTURE_2D);
}

static void nw_line(float x0, float y0, float x1, float y1, nw_color c)
{
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(c.r, c.g, c.b, c.a);
    glBegin(GL_LINES);
    glVertex2f(x0, y0); glVertex2f(x1, y1);
    glEnd();
    glEnable(GL_TEXTURE_2D);
}

static void nw_bevel(float x0, float y0, float x1, float y1, nw_color face)
{
    nw_quad(x0, y0, x1, y1, face);
    nw_line(x0, y0, x1, y0, NW_CHROME_LIGHT);
    nw_line(x0, y0, x0, y1, NW_CHROME_LIGHT);
    nw_line(x1, y0, x1, y1, NW_CHROME_DARK);
    nw_line(x0, y1, x1, y1, NW_CHROME_DARK);
}

static void nw_ellipse(float cx, float cy, float rx, float ry, nw_color c)
{
    const unsigned int slices = 48U;
    unsigned int i;

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(c.r, c.g, c.b, c.a);
    glBegin(GL_QUADS);
    for (i = 0U; i < slices; i++) {
        float f0 = (float)i / (float)slices;
        float f1 = (float)(i + 1U) / (float)slices;
        float x0 = -rx + 2.0F * rx * f0;
        float x1 = -rx + 2.0F * rx * f1;
        float y0 = ry * sqrtf(fmaxf(0.0F, 1.0F - (x0 * x0) / (rx * rx)));
        float y1 = ry * sqrtf(fmaxf(0.0F, 1.0F - (x1 * x1) / (rx * rx)));
        glVertex2f(cx + x0, cy - y0);
        glVertex2f(cx + x1, cy - y1);
        glVertex2f(cx + x1, cy + y1);
        glVertex2f(cx + x0, cy + y0);
    }
    glEnd();
    glEnable(GL_TEXTURE_2D);
}

static void nw_draw_upper(const char *text, float x, float y, float pixel,
                          nw_color c)
{
    float pen = x;
    size_t n;

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(c.r, c.g, c.b, c.a);
    glBegin(GL_QUADS);
    for (n = 0U; text && text[n] != '\0'; n++) {
        const uint8_t *rows = nw_upper_rows(text[n]);
        unsigned int row;
        unsigned int col;

        if (text[n] == '\n') {
            y += pixel * 9.0F;
            pen = x;
            continue;
        }

        if (rows) {
            for (row = 0U; row < 7U; row++) {
                for (col = 0U; col < 5U; col++) {
                    if ((rows[row] &
                         (uint8_t)(1U << (4U - col))) != 0U) {
                        float px = pen + (float)col * pixel;
                        float py = y + (float)row * pixel;
                        glVertex2f(px, py);
                        glVertex2f(px + pixel, py);
                        glVertex2f(px + pixel, py + pixel);
                        glVertex2f(px, py + pixel);
                    }
                }
            }
        }
        pen += pixel * 6.0F;
    }
    glEnd();
    glEnable(GL_TEXTURE_2D);
}

void neo_workstation_draw_searching(float x, float y, float pixel, float alpha)
{
    static const char text[] = "Searching...";
    float pen = x;
    size_t n;

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glColor4f(NW_SEARCH_TEXT.r, NW_SEARCH_TEXT.g, NW_SEARCH_TEXT.b,
              NW_SEARCH_TEXT.a * alpha);
    glBegin(GL_QUADS);
    for (n = 0U; text[n] != '\0'; n++) {
        const uint8_t *rows = nw_search_rows(text[n]);
        unsigned int row;
        unsigned int col;
        if (rows) {
            for (row = 0U; row < 7U; row++) {
                for (col = 0U; col < 5U; col++) {
                    if ((rows[row] &
                         (uint8_t)(1U << (4U - col))) != 0U) {
                        float px = pen + (float)col * pixel;
                        float py = y + (float)row * pixel;
                        glVertex2f(px, py);
                        glVertex2f(px + pixel, py);
                        glVertex2f(px + pixel, py + pixel);
                        glVertex2f(px, py + pixel);
                    }
                }
            }
        }
        pen += pixel * 6.0F;
    }
    glEnd();
    glEnable(GL_TEXTURE_2D);
}

static void nw_toolbar_icon(float x, float y, unsigned int kind)
{
    nw_color icon = NW_CHROME_DARK;
    switch (kind % 5U) {
    case 0U:
        nw_quad(x + 4.0F, y + 5.0F, x + 7.0F, y + 18.0F, icon);
        nw_quad(x + 9.0F, y + 8.0F, x + 12.0F, y + 18.0F, icon);
        nw_quad(x + 14.0F, y + 3.0F, x + 17.0F, y + 18.0F, icon);
        break;
    case 1U:
        nw_quad(x + 4.0F, y + 4.0F, x + 17.0F, y + 7.0F, icon);
        nw_quad(x + 4.0F, y + 10.0F, x + 17.0F, y + 13.0F, icon);
        nw_quad(x + 4.0F, y + 16.0F, x + 17.0F, y + 19.0F, icon);
        break;
    case 2U:
        nw_quad(x + 9.0F, y + 3.0F, x + 12.0F, y + 19.0F, icon);
        nw_quad(x + 3.0F, y + 9.0F, x + 18.0F, y + 12.0F, icon);
        break;
    case 3U:
        nw_quad(x + 4.0F, y + 5.0F, x + 8.0F, y + 17.0F, icon);
        nw_quad(x + 8.0F, y + 8.0F, x + 12.0F, y + 14.0F, icon);
        nw_quad(x + 12.0F, y + 5.0F, x + 16.0F, y + 17.0F, icon);
        break;
    default:
        nw_quad(x + 5.0F, y + 5.0F, x + 16.0F, y + 16.0F, icon);
        nw_quad(x + 8.0F, y + 8.0F, x + 13.0F, y + 13.0F, NW_CHROME);
        break;
    }
}

static void nw_draw_toolbar(double t)
{
    unsigned int i;
    float download = nw_stage(t, 14.0, 16.0);

    nw_quad(0.0F, 0.0F, NW_W, 58.0F, NW_CHROME_DARK);
    nw_quad(0.0F, 7.0F, NW_W, 39.0F, NW_CHROME);

    for (i = 0U; i < 8U; i++) {
        float x = 12.0F + (float)i * 31.0F;
        nw_bevel(x, 8.0F, x + 27.0F, 35.0F, NW_CHROME);
        nw_toolbar_icon(x + 2.0F, 9.0F, i);
    }

    nw_bevel(274.0F, 8.0F, 423.0F, 35.0F, NW_CHROME);
    nw_quad(285.0F, 15.0F, 407.0F, 29.0F, NW_CHROME_LIGHT);

    nw_bevel(436.0F, 8.0F, 627.0F, 35.0F, NW_CHROME);
    nw_quad(449.0F, 15.0F, 611.0F, 29.0F, NW_PAPER_ALT);

    if (download > 0.0F) {
        nw_quad(370.0F, 42.0F, 565.0F, 57.0F, NW_BANNER);
        nw_quad(370.0F, 42.0F, 370.0F + 33.0F * download,
                57.0F, NW_CHROME_LIGHT);
        nw_draw_upper("DOWNLOAD", 410.0F, 45.0F, 1.25F, NW_BANNER_TEXT);
    }
}

static void nw_body_lines(float x, float y, float width,
                          unsigned int lines, uint32_t seed)
{
    unsigned int i;
    for (i = 0U; i < lines; i++) {
        uint32_t v = seed ^ (i * UINT32_C(2654435761));
        float frac;
        float line_w;
        v ^= v >> 16U;
        v *= UINT32_C(2246822519);
        v ^= v >> 13U;
        frac = 0.58F + (float)(v & UINT32_C(255)) / 255.0F * 0.42F;
        line_w = width * frac;
        nw_quad(x, y + (float)i * 7.4F,
                x + line_w, y + (float)i * 7.4F + 2.0F, NW_INK);
    }
}

static void nw_draw_global_banner(float x, float y, float width)
{
    nw_quad(x, y, x + width, y + 48.0F, NW_BANNER);
    nw_line(x, y, x + width, y, NW_CHROME_LIGHT);
    nw_line(x, y + 48.0F, x + width, y + 48.0F, NW_CHROME_DARK);
    nw_draw_upper("GLOBAL SEARCH", x + 18.0F, y + 11.0F, 3.0F,
                  NW_BANNER_TEXT);
}

static void nw_draw_article(double t)
{
    float appear = nw_stage(t, 3.0, 6.0);
    float shift = nw_stage(t, 10.0, 14.0);
    float x = 32.0F + (1.0F - appear) * 78.0F - shift * 20.0F;
    float y = 176.0F - appear * 18.0F;
    float w = 465.0F;
    float h = 293.0F;

    if (appear <= 0.0F) return;

    nw_quad(x + 7.0F, y + 8.0F, x + w + 7.0F, y + h + 8.0F,
            NW_CHROME_DARK);
    nw_quad(x, y, x + w, y + h, NW_PAPER);

    nw_draw_upper("MORPHEUS", x + 22.0F, y + 72.0F, 2.15F, NW_INK);
    nw_draw_upper("ELUDES", x + 22.0F, y + 93.0F, 2.15F, NW_INK);
    nw_draw_upper("POLICE AT", x + 22.0F, y + 114.0F, 2.15F, NW_INK);
    nw_draw_upper("HEATHROW", x + 22.0F, y + 135.0F, 2.15F, NW_INK);
    nw_draw_upper("AIRPORT", x + 22.0F, y + 156.0F, 2.15F, NW_INK);

    nw_body_lines(x + 201.0F, y + 73.0F, 225.0F, 14U,
                  UINT32_C(0x19990331));
    nw_body_lines(x + 22.0F, y + 190.0F, 168.0F, 11U,
                  UINT32_C(0x4d4f5250));
    nw_body_lines(x + 201.0F, y + 190.0F, 225.0F, 11U,
                  UINT32_C(0x48455553));
}

static void nw_draw_portrait(double t)
{
    float appear = nw_stage(t, 7.0, 11.0);
    float x = 493.0F + (1.0F - appear) * 92.0F;
    float y = 151.0F;
    unsigned int i;

    if (appear <= 0.0F) return;

    nw_quad(x, y, 634.0F, 459.0F, NW_PAPER_ALT);
    nw_quad(x + 7.0F, y + 8.0F, 629.0F, 454.0F, NW_IMAGE_DARK);

    nw_ellipse(x + 79.0F, y + 97.0F, 48.0F, 67.0F, NW_IMAGE_MID);
    nw_ellipse(x + 74.0F, y + 103.0F, 44.0F, 64.0F, NW_IMAGE_DARK);
    nw_quad(x + 104.0F, y + 95.0F, x + 132.0F, y + 110.0F,
            NW_IMAGE_DARK);
    nw_ellipse(x + 74.0F, y + 259.0F, 87.0F, 103.0F, NW_IMAGE_DARK);

    for (i = 0U; i < 14U; i++) {
        float yy = y + 54.0F + (float)i * 19.0F;
        nw_quad(x + 13.0F, yy, 625.0F, yy + 1.2F,
                (nw_color){0.24F,0.34F,0.27F,0.28F});
    }
}

static void nw_draw_arabic_result(double t)
{
    float appear = nw_stage(t, 14.0, 18.0);
    float settle = nw_stage(t, 18.0, 21.0);
    float x = 122.0F;
    float y = 120.0F - (1.0F - appear) * 138.0F + settle * 18.0F;
    float w = 354.0F;

    if (appear <= 0.0F) return;

    nw_quad(x + 6.0F, y + 7.0F, x + w + 6.0F, y + 238.0F,
            NW_CHROME_DARK);
    nw_quad(x, y, x + w, y + 231.0F, NW_PAPER_ALT);

    nw_line(x + 59.0F, y + 42.0F, x + 286.0F, y + 42.0F, NW_INK);
    nw_line(x + 74.0F, y + 72.0F, x + 281.0F, y + 72.0F, NW_INK);
    nw_quad(x + 85.0F, y + 28.0F, x + 94.0F, y + 73.0F, NW_INK);
    nw_quad(x + 132.0F, y + 36.0F, x + 143.0F, y + 72.0F, NW_INK);
    nw_quad(x + 205.0F, y + 24.0F, x + 215.0F, y + 72.0F, NW_INK);
    nw_quad(x + 251.0F, y + 17.0F, x + 260.0F, y + 71.0F, NW_INK);
    nw_ellipse(x + 168.0F, y + 52.0F, 18.0F, 14.0F, NW_INK);
    nw_ellipse(x + 227.0F, y + 49.0F, 13.0F, 10.0F, NW_INK);

    nw_draw_upper("AN-NAHAR", x + 130.0F, y + 84.0F, 1.65F, NW_INK);
    nw_draw_upper("ALL STATES 6 1.50", x + 109.0F, y + 103.0F,
                  0.95F, NW_INK);

    nw_body_lines(x + 18.0F, y + 132.0F, w - 36.0F, 5U,
                  UINT32_C(0x414e4e41));
    nw_body_lines(x + 18.0F, y + 178.0F, w - 36.0F, 5U,
                  UINT32_C(0x48415231));
}

static void nw_draw_search_box(double t)
{
    float pulse = 0.90F + 0.08F * sinf((float)t * 6.0F);
    const float x0 = 137.0F;
    const float y0 = 67.0F;
    const float x1 = 447.0F;
    const float y1 = 111.0F;

    nw_quad(x0 + 6.0F, y0 + 7.0F, x1 + 6.0F, y1 + 7.0F,
            (nw_color){0.0F,0.0F,0.0F,0.42F});
    nw_quad(x0, y0, x1, y1, NW_SEARCH_BG);
    nw_line(x0, y0, x1, y0, NW_SEARCH_EDGE);
    nw_line(x0, y0, x0, y1, NW_SEARCH_EDGE);
    nw_line(x1, y0, x1, y1, NW_CHROME_DARK);
    nw_line(x0, y1, x1, y1, NW_CHROME_DARK);

    neo_workstation_draw_searching(x0 + 18.0F, y0 + 10.0F, 2.45F, pulse);
}

static void nw_draw_scanlines(void)
{
    unsigned int y;
    for (y = 1U; y < 480U; y += 4U) {
        nw_quad(0.0F, (float)y, NW_W, (float)y + 1.0F,
                (nw_color){0.0F,0.0F,0.0F,0.075F});
    }
}

void neo_workstation_render(double elapsed)
{
    float global_appear = nw_stage(elapsed, 0.5, 3.5);
    float banner_y = 126.0F + (1.0F - global_appear) * 34.0F;

    nw_quad(0.0F, 0.0F, NW_W, NW_H, NW_DESKTOP);
    nw_draw_toolbar(elapsed);

    nw_quad(16.0F, 62.0F, 624.0F, 469.0F, NW_CHROME_DARK);
    nw_quad(23.0F, 68.0F, 617.0F, 463.0F, NW_PAPER_ALT);

    if (global_appear > 0.0F)
        nw_draw_global_banner(78.0F, banner_y, 404.0F);

    nw_draw_article(elapsed);
    nw_draw_portrait(elapsed);
    nw_draw_arabic_result(elapsed);
    nw_draw_search_box(elapsed);
    nw_draw_scanlines();
}

int neo_workstation_self_test(void)
{
    if (nw_stage(0.0, 1.0, 2.0) != 0.0F) return 0;
    if (nw_stage(3.0, 1.0, 2.0) != 1.0F) return 0;
    if (nw_upper_rows('G') == NULL) return 0;
    if (nw_search_rows('a') == NULL) return 0;
    if (nw_search_rows('x') != NULL) return 0;
    return 1;
}
