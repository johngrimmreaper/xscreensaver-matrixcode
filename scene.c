#include "scene.h"
#include "neo_workstation.h"
#include "terminal_font.h"

#if defined(MATRIXCODE_USE_MINIMAL_GL_HEADERS)
# include "compat/minigl.h"
#else
# include <GL/gl.h>
#endif

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define SCENE_W 640.0F
#define SCENE_H 480.0F

/* Experimental trace/tunnel prelude. These timings are calibration values;
 * final-cut screenshots/audio will replace them. */
#define TRACE_DURATION 14.0
#define TRACE_LOCK_START 1.0
#define TRACE_LOCK_END 5.5
#define TRACE_APERTURE_START 5.5
#define TRACE_BLACKOUT_START 7.0
#define TRACE_TUNNEL_START 7.8
#define TRACE_PHOSPHOR_START 10.2
#define TRACE_GLYPH_START 10.9
#define TRACE_SEARCH_START 12.8
#define TRACE_LOCK_ROW 10U

#define NEO_DURATION 76.0
#define NEO_RAIN_START 73.0
#define SEARCH_UI_SCALE 0.85F
#define TERMINAL_MSG_SCALE 0.68F
#define SEARCH_TITLE_PIXEL (3.0F * SEARCH_UI_SCALE)
#define SEARCH_BODY_PIXEL (2.0F * SEARCH_UI_SCALE)
#define TERMINAL_PIXEL (3.0F * TERMINAL_MSG_SCALE)
#define TERMINAL_X 54.0F
#define TERMINAL_Y 112.0F

typedef struct scene_view { float x, y, scale; } scene_view;

typedef enum scene_action_kind {
    SCENE_SHOW_SEARCH = 0,
    SCENE_BLACK_CURSOR,
    SCENE_TYPE_TEXT,
    SCENE_SHOW_TEXT,
    SCENE_BLACK,
    SCENE_RAIN_TRANSITION
} scene_action_kind;

typedef struct scene_cue {
    scene_action_kind kind;
    double start_time;
    double end_time;
    const char *text;
} scene_cue;

static const scene_cue neo_cues[] = {
    { SCENE_SHOW_SEARCH,      0.0, 23.0, NULL },
    { SCENE_BLACK_CURSOR,   23.0, 25.0, NULL },
    { SCENE_TYPE_TEXT,      25.0, 29.0, "Wake up, Neo..." },
    { SCENE_SHOW_TEXT,      29.0, 40.0, "Wake up, Neo..." },
    { SCENE_TYPE_TEXT,      40.0, 47.0, "The Matrix has you..." },
    { SCENE_SHOW_TEXT,      47.0, 55.0, "The Matrix has you..." },
    { SCENE_TYPE_TEXT,      55.0, 66.0, "Follow the white rabbit." },
    { SCENE_SHOW_TEXT,      66.0, 67.0, "Follow the white rabbit." },
    { SCENE_SHOW_TEXT,      67.0, 71.0, "Knock, knock, Neo." },
    { SCENE_BLACK,          71.0, 73.0, NULL },
    { SCENE_RAIN_TRANSITION,73.0, 76.0, NULL }
};

static scene_view view_for(int width, int height)
{
    scene_view v;
    float sx = (float)width / SCENE_W;
    float sy = (float)height / SCENE_H;
    v.scale = sx < sy ? sx : sy;
    v.x = ((float)width - SCENE_W * v.scale) * 0.5F;
    v.y = ((float)height - SCENE_H * v.scale) * 0.5F;
    return v;
}

static void scene_projection(scene_view v)
{
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glOrtho(0.0, SCENE_W, SCENE_H, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glViewport((GLint)lroundf(v.x), (GLint)lroundf(v.y),
               (GLsizei)lroundf(SCENE_W * v.scale), (GLsizei)lroundf(SCENE_H * v.scale));
}

static void scene_projection_end(int width, int height)
{
    glMatrixMode(GL_MODELVIEW); glPopMatrix();
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glViewport(0, 0, width, height);
}

static const scene_cue *cue_at(double t)
{
    size_t i;
    for (i = 0U; i < sizeof(neo_cues) / sizeof(neo_cues[0]); i++) {
        if (t >= neo_cues[i].start_time && t < neo_cues[i].end_time)
            return &neo_cues[i];
    }
    return NULL;
}

static size_t typed_count_window(const char *text, double t, double start, double end)
{
    size_t len = text ? strlen(text) : 0U;
    double fraction;
    if (len == 0U || t <= start) return 0U;
    if (t >= end || end <= start) return len;
    fraction = (t - start) / (end - start);
    if (fraction <= 0.0) return 0U;
    if (fraction >= 1.0) return len;
    return (size_t)floor(fraction * (double)len);
}

static int cursor_visible(double t)
{
    return fmod(t * 2.0, 2.0) < 1.0;
}

static void draw_cursor(float x, float y, float pixel, double t)
{
    float width;
    if (!cursor_visible(t)) return;
    width = fmaxf(1.0F, pixel * 0.55F);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glColor4f(0.24F, 1.0F, 0.42F, 0.92F);
    glBegin(GL_QUADS);
    glVertex2f(x, y); glVertex2f(x + width, y);
    glVertex2f(x + width, y + pixel * 7.0F); glVertex2f(x, y + pixel * 7.0F);
    glEnd();
    glEnable(GL_TEXTURE_2D);
}

static void draw_text_with_cursor(const char *text, size_t count, double t,
                                  float x, float y, float pixel)
{
    char buf[128];
    float cx;
    if (!text) {
        draw_cursor(x, y, pixel, t);
        return;
    }
    if (count >= sizeof(buf)) count = sizeof(buf) - 1U;
    memcpy(buf, text, count);
    buf[count] = '\0';
    terminal_draw_text(buf, x, y, pixel, 0.18F, 1.0F, 0.40F, 0.90F);
    cx = x + terminal_text_width(buf, pixel);
    if (count > 0U) cx += pixel;
    draw_cursor(cx, y, pixel, t);
}

static void draw_search_screen(double t)
{
    neo_workstation_render(t);
}

static float trace_smooth(float x)
{
    if (x <= 0.0F) return 0.0F;
    if (x >= 1.0F) return 1.0F;
    return x * x * (3.0F - 2.0F * x);
}

static uint32_t trace_hash(uint32_t value)
{
    value ^= value >> 16U;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15U;
    value *= UINT32_C(0x846ca68b);
    value ^= value >> 16U;
    return value;
}

static unsigned int trace_locked_count(double t)
{
    double fraction;
    unsigned int value;
    if (t <= TRACE_LOCK_START) return 0U;
    if (t >= TRACE_LOCK_END) return 10U;
    fraction = (t - TRACE_LOCK_START) / (TRACE_LOCK_END - TRACE_LOCK_START);
    value = (unsigned int)floor(fraction * 10.0);
    return value > 10U ? 10U : value;
}

static void trace_draw_char(char ch, float x, float y, float pixel,
                            float red, float green, float blue, float alpha)
{
    char text[2];
    text[0] = ch;
    text[1] = '\0';
    terminal_draw_text(text, x, y, pixel, red, green, blue, alpha);
}

static void draw_trace_labels(double t, unsigned int locked)
{
    float pulse = 0.72F + 0.18F * sinf((float)t * 8.0F);
    terminal_draw_text("TRACE PROGRAM: RUNNING", 26.0F, 26.0F, 1.8F,
                       0.10F, 0.86F, 0.30F, pulse);
    terminal_draw_text("10-DIGIT ORIGIN / ACQUIRE", 26.0F, 50.0F, 1.25F,
                       0.06F, 0.55F, 0.20F, 0.55F);
    if (locked >= 10U) {
        terminal_draw_text("TRACE COMPLETE", 26.0F, 430.0F, 1.55F,
                           0.18F, 1.0F, 0.38F, 0.92F);
        terminal_draw_text("CALL ORIGIN: 312-555-0690", 26.0F, 452.0F, 1.15F,
                           0.08F, 0.72F, 0.25F, 0.70F);
    }
}

static void draw_trace_grid(double t, float zoom)
{
    static const char target[] = "3125550690";
    const float left = 94.0F;
    const float top = 44.0F;
    const float x_step = 46.0F;
    const float y_step = 19.0F;
    /* Focus between cells rather than on a numeral: the aperture is a
     * cinematic transition target, not a literal zero glyph. */
    const float target_x = left + 4.5F * x_step;
    const float target_y = top + (float)TRACE_LOCK_ROW * y_step;
    unsigned int locked = trace_locked_count(t);
    unsigned int tick = (unsigned int)floor(t * 18.0);
    unsigned int col;
    unsigned int row;

    if (zoom <= 1.01F) draw_trace_labels(t, locked);

    for (col = 0U; col < 10U; col++) {
        for (row = 0U; row < 21U; row++) {
            float x = left + (float)col * x_step;
            float y = top + (float)row * y_step;
            float sx = SCENE_W * 0.5F + (x - target_x) * zoom;
            float sy = SCENE_H * 0.5F + (y - target_y) * zoom;
            float pixel = 1.45F * zoom;
            float alpha;
            char digit;
            uint32_t hash;

            if (sx < -160.0F || sx > SCENE_W + 160.0F ||
                sy < -160.0F || sy > SCENE_H + 160.0F)
                continue;

            hash = trace_hash(col * UINT32_C(131) ^
                              row * UINT32_C(977) ^
                              tick * UINT32_C(8191));
            digit = (char)('0' + (char)(hash % 10U));
            alpha = 0.20F +
                    (float)(hash & UINT32_C(255)) / 255.0F * 0.42F;

            if (row == TRACE_LOCK_ROW && col < locked) {
                digit = target[col];
                trace_draw_char(digit, sx, sy, pixel,
                                0.30F, 1.0F, 0.48F, 0.98F);
            } else {
                trace_draw_char(digit, sx, sy, pixel,
                                0.05F, 0.64F, 0.20F, alpha);
            }
        }
    }
}

typedef enum trace_phase {
    TRACE_PHASE_ACQUIRE = 0,
    TRACE_PHASE_APERTURE,
    TRACE_PHASE_BLACKOUT,
    TRACE_PHASE_TUNNEL,
    TRACE_PHASE_PHOSPHOR,
    TRACE_PHASE_GLYPH_REVEAL,
    TRACE_PHASE_SEARCH_UI
} trace_phase;

static trace_phase trace_phase_at(double t)
{
    if (t < TRACE_APERTURE_START) return TRACE_PHASE_ACQUIRE;
    if (t < TRACE_BLACKOUT_START) return TRACE_PHASE_APERTURE;
    if (t < TRACE_TUNNEL_START) return TRACE_PHASE_BLACKOUT;
    if (t < TRACE_PHOSPHOR_START) return TRACE_PHASE_TUNNEL;
    if (t < TRACE_GLYPH_START) return TRACE_PHASE_PHOSPHOR;
    if (t < TRACE_SEARCH_START) return TRACE_PHASE_GLYPH_REVEAL;
    return TRACE_PHASE_SEARCH_UI;
}

static float trace_unit(double value, double start, double end)
{
    double p;
    if (value <= start) return 0.0F;
    if (value >= end || end <= start) return 1.0F;
    p = (value - start) / (end - start);
    return (float)p;
}

static void trace_draw_filled_circle(float cx, float cy, float radius,
                                     float alpha)
{
    const unsigned int slices = 48U;
    unsigned int i;

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0F, 0.0F, 0.0F, alpha);
    glBegin(GL_QUADS);
    for (i = 0U; i < slices; i++) {
        float f0 = (float)i / (float)slices;
        float f1 = (float)(i + 1U) / (float)slices;
        float x0 = -radius + 2.0F * radius * f0;
        float x1 = -radius + 2.0F * radius * f1;
        float yy0 = sqrtf(fmaxf(0.0F, radius * radius - x0 * x0));
        float yy1 = sqrtf(fmaxf(0.0F, radius * radius - x1 * x1));
        glVertex2f(cx + x0, cy - yy0);
        glVertex2f(cx + x1, cy - yy1);
        glVertex2f(cx + x1, cy + yy1);
        glVertex2f(cx + x0, cy + yy0);
    }
    glEnd();
    glEnable(GL_TEXTURE_2D);
}

static void trace_draw_aperture_ring(float cx, float cy, float radius,
                                     float alpha)
{
    const unsigned int segments = 64U;
    unsigned int i;

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glLineWidth(1.5F);
    glColor4f(0.12F, 0.92F, 0.30F, alpha);
    glBegin(GL_LINES);
    for (i = 0U; i < segments; i++) {
        float a0 = (float)i / (float)segments * 6.28318530718F;
        float a1 = (float)(i + 1U) / (float)segments * 6.28318530718F;
        glVertex2f(cx + cosf(a0) * radius, cy + sinf(a0) * radius);
        glVertex2f(cx + cosf(a1) * radius, cy + sinf(a1) * radius);
    }
    glEnd();
    glEnable(GL_TEXTURE_2D);
}

static void draw_trace_aperture(double t)
{
    float p = trace_smooth(trace_unit(t, TRACE_APERTURE_START,
                                     TRACE_BLACKOUT_START));
    float radius = 6.0F + p * 390.0F;
    float darkness = 0.30F + 0.70F * p;
    trace_draw_filled_circle(SCENE_W * 0.5F, SCENE_H * 0.5F,
                             radius, darkness);
    trace_draw_aperture_ring(SCENE_W * 0.5F, SCENE_H * 0.5F,
                             radius, (1.0F - p) * 0.90F);
    trace_draw_aperture_ring(SCENE_W * 0.5F, SCENE_H * 0.5F,
                             radius + 4.0F, (1.0F - p) * 0.28F);
}

static void trace_draw_cell(float x0, float y0, float x1, float y1,
                            float depth, float intensity)
{
    float hot = depth * depth * depth;
    float alpha = intensity * (0.18F + 0.82F * depth);
    glColor4f(0.08F + 0.80F * hot,
              0.50F + 0.50F * depth,
              0.10F + 0.72F * hot,
              alpha);
    glVertex2f(x0, y0);
    glVertex2f(x1, y0);
    glVertex2f(x1, y1);
    glVertex2f(x0, y1);
}

static void draw_trace_tunnel(double t, float intensity)
{
    const float cx = SCENE_W * 0.5F;
    const float cy = SCENE_H * 0.5F;
    const unsigned int layers = 13U;
    const unsigned int horizontal_cells = 13U;
    const unsigned int vertical_cells = 9U;
    float phase = (float)(t - TRACE_TUNNEL_START);
    unsigned int layer;

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glBegin(GL_QUADS);

    for (layer = 0U; layer < layers; layer++) {
        float raw = fmodf((float)layer / (float)layers +
                          phase * 0.43F, 1.0F);
        float depth = raw * raw;
        float half_w = 30.0F + depth * 390.0F;
        float half_h = 20.0F + depth * 285.0F;
        float cell_w = 5.0F + depth * 25.0F;
        float cell_h = 4.0F + depth * 18.0F;
        float wobble_x = sinf(phase * 0.9F + (float)layer) *
                         (1.0F - raw) * 4.0F;
        float wobble_y = cosf(phase * 0.7F + (float)layer * 0.6F) *
                         (1.0F - raw) * 3.0F;
        unsigned int i;

        for (i = 0U; i < horizontal_cells; i++) {
            float u = ((float)i + 0.5F) / (float)horizontal_cells;
            float x = cx + wobble_x + (u * 2.0F - 1.0F) * half_w;
            trace_draw_cell(x - cell_w, cy + wobble_y - half_h - cell_h,
                            x + cell_w, cy + wobble_y - half_h + cell_h,
                            raw, intensity);
            trace_draw_cell(x - cell_w, cy + wobble_y + half_h - cell_h,
                            x + cell_w, cy + wobble_y + half_h + cell_h,
                            raw, intensity);
        }

        for (i = 0U; i < vertical_cells; i++) {
            float v = ((float)i + 0.5F) / (float)vertical_cells;
            float y = cy + wobble_y + (v * 2.0F - 1.0F) * half_h;
            trace_draw_cell(cx + wobble_x - half_w - cell_w,
                            y - cell_h,
                            cx + wobble_x - half_w + cell_w,
                            y + cell_h, raw, intensity);
            trace_draw_cell(cx + wobble_x + half_w - cell_w,
                            y - cell_h,
                            cx + wobble_x + half_w + cell_w,
                            y + cell_h, raw, intensity);
        }
    }
    glEnd();

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0F, 0.0F, 0.0F, 0.90F);
    glBegin(GL_QUADS);
    glVertex2f(cx - 38.0F, cy - 27.0F);
    glVertex2f(cx + 38.0F, cy - 27.0F);
    glVertex2f(cx + 38.0F, cy + 27.0F);
    glVertex2f(cx - 38.0F, cy + 27.0F);
    glEnd();
    glEnable(GL_TEXTURE_2D);
}

static void draw_phosphor_bloom(float alpha)
{
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glColor4f(0.58F, 1.0F, 0.66F, alpha);
    glBegin(GL_QUADS);
    glVertex2f(0.0F, 0.0F);
    glVertex2f(SCENE_W, 0.0F);
    glVertex2f(SCENE_W, SCENE_H);
    glVertex2f(0.0F, SCENE_H);
    glEnd();
    glEnable(GL_TEXTURE_2D);
}

static void draw_trace_raster(float alpha)
{
    unsigned int x;
    unsigned int y;

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glLineWidth(1.0F);
    glColor4f(0.18F, 0.72F, 0.28F, alpha);
    glBegin(GL_LINES);
    for (x = 0U; x <= 640U; x += 4U) {
        glVertex2f((float)x, 0.0F);
        glVertex2f((float)x, SCENE_H);
    }
    for (y = 0U; y <= 480U; y += 3U) {
        glVertex2f(0.0F, (float)y);
        glVertex2f(SCENE_W, (float)y);
    }
    glEnd();
    glEnable(GL_TEXTURE_2D);
}

static void draw_trace_glyph_reveal(double t)
{
    float p = trace_smooth(trace_unit(t, TRACE_GLYPH_START,
                                     TRACE_SEARCH_START));
    float pixel = 2.7F + (1.0F - p) * 47.3F;
    float anchored_x = SCENE_W * 0.5F - 14.5F * pixel;
    float anchored_y = SCENE_H * 0.5F - 4.8F * pixel;
    float move = trace_smooth(fmaxf(0.0F, (p - 0.65F) / 0.35F));
    float final_x = 178.0F;
    float final_y = 82.0F;
    float x = anchored_x * (1.0F - move) + final_x * move;
    float y = anchored_y * (1.0F - move) + final_y * move;

    neo_workstation_draw_searching(x, y, pixel, 0.94F);
    draw_trace_raster((1.0F - p) * 0.20F);
}

static matrix_scene_result render_neo(double t, int width, int height)
{
    const scene_cue *cue = cue_at(t);
    scene_view v = view_for(width, height);
    glClearColor(0.0F, 0.0F, 0.0F, 1.0F); glClear(GL_COLOR_BUFFER_BIT);
    scene_projection(v);
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_LIGHTING);

    if (cue) {
        switch (cue->kind) {
        case SCENE_SHOW_SEARCH:
            draw_search_screen(t);
            break;
        case SCENE_BLACK_CURSOR:
            draw_cursor(TERMINAL_X, TERMINAL_Y, TERMINAL_PIXEL, t);
            break;
        case SCENE_TYPE_TEXT:
            draw_text_with_cursor(cue->text,
                                  typed_count_window(cue->text, t,
                                                     cue->start_time, cue->end_time),
                                  t, TERMINAL_X, TERMINAL_Y, TERMINAL_PIXEL);
            break;
        case SCENE_SHOW_TEXT:
            draw_text_with_cursor(cue->text, strlen(cue->text), t,
                                  TERMINAL_X, TERMINAL_Y, TERMINAL_PIXEL);
            break;
        case SCENE_BLACK:
        case SCENE_RAIN_TRANSITION:
            break;
        }
    }

    scene_projection_end(width, height);
    return t >= NEO_DURATION ? MATRIX_SCENE_FINISHED : MATRIX_SCENE_RUNNING;
}


static matrix_scene_result render_trace(double t, int width, int height)
{
    scene_view view = view_for(width, height);
    trace_phase phase = trace_phase_at(t);

    glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);
    scene_projection(view);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);

    switch (phase) {
    case TRACE_PHASE_ACQUIRE:
        draw_trace_grid(t, 1.0F);
        break;

    case TRACE_PHASE_APERTURE: {
        float p = trace_smooth(trace_unit(t, TRACE_APERTURE_START,
                                         TRACE_BLACKOUT_START));
        float zoom = 1.0F + p * 7.0F;
        draw_trace_grid(t, zoom);
        draw_trace_aperture(t);
        break;
    }

    case TRACE_PHASE_BLACKOUT:
        break;

    case TRACE_PHASE_TUNNEL: {
        float p = trace_unit(t, TRACE_TUNNEL_START, TRACE_PHOSPHOR_START);
        draw_trace_tunnel(t, 0.28F + 0.72F * trace_smooth(p));
        break;
    }

    case TRACE_PHASE_PHOSPHOR: {
        float p = trace_smooth(trace_unit(t, TRACE_PHOSPHOR_START,
                                         TRACE_GLYPH_START));
        draw_trace_tunnel(t, 1.0F);
        draw_phosphor_bloom(0.08F + p * 0.46F);
        break;
    }

    case TRACE_PHASE_GLYPH_REVEAL: {
        float p = trace_unit(t, TRACE_GLYPH_START, TRACE_SEARCH_START);
        draw_trace_tunnel(t, (1.0F - p) * 0.82F);
        draw_phosphor_bloom((1.0F - p) * 0.26F);
        draw_trace_glyph_reveal(t);
        break;
    }

    case TRACE_PHASE_SEARCH_UI:
        draw_search_screen(t - TRACE_SEARCH_START);
        break;
    }

    scene_projection_end(width, height);
    return t >= TRACE_DURATION ?
           MATRIX_SCENE_FINISHED : MATRIX_SCENE_RUNNING;
}

matrix_scene_kind matrix_scene_from_name(const char *name)
{
    if (name && (strcmp(name, "neo-terminal") == 0 || strcmp(name, "neo") == 0))
        return MATRIX_SCENE_NEO_TERMINAL;
    if (name && (strcmp(name, "neo-trace") == 0 || strcmp(name, "trace") == 0))
        return MATRIX_SCENE_NEO_TRACE;
    return MATRIX_SCENE_NONE;
}

const char *matrix_scene_name(matrix_scene_kind scene)
{
    if (scene == MATRIX_SCENE_NEO_TERMINAL) return "neo-terminal";
    if (scene == MATRIX_SCENE_NEO_TRACE) return "neo-trace";
    return "none";
}

double matrix_scene_duration(matrix_scene_kind scene)
{
    if (scene == MATRIX_SCENE_NEO_TERMINAL) return NEO_DURATION;
    if (scene == MATRIX_SCENE_NEO_TRACE) return TRACE_DURATION;
    return 0.0;
}

double matrix_scene_rain_start(matrix_scene_kind scene)
{
    if (scene == MATRIX_SCENE_NEO_TERMINAL) return NEO_RAIN_START;
    if (scene == MATRIX_SCENE_NEO_TRACE) return TRACE_DURATION;
    return 0.0;
}

float matrix_scene_rain_opacity(matrix_scene_kind scene, double elapsed)
{
    double start;
    double end;
    double value;
    if (scene == MATRIX_SCENE_NEO_TRACE)
        return elapsed >= TRACE_DURATION ? 1.0F : 0.0F;
    if (scene != MATRIX_SCENE_NEO_TERMINAL) return 1.0F;
    start = NEO_RAIN_START;
    end = NEO_DURATION;
    if (elapsed <= start) return 0.0F;
    if (elapsed >= end) return 1.0F;
    value = (elapsed - start) / (end - start);
    return (float)value;
}

matrix_scene_result matrix_scene_render(matrix_scene_kind scene, double elapsed,
                                        int width, int height)
{
    if (scene == MATRIX_SCENE_NEO_TERMINAL)
        return render_neo(elapsed, width, height);
    if (scene == MATRIX_SCENE_NEO_TRACE)
        return render_trace(elapsed, width, height);
    return MATRIX_SCENE_FINISHED;
}

int matrix_scene_self_test(void)
{
    const scene_cue *cue;
    if (matrix_scene_from_name("neo-terminal") != MATRIX_SCENE_NEO_TERMINAL) return 0;
    if (matrix_scene_from_name("neo-trace") != MATRIX_SCENE_NEO_TRACE) return 0;
    if (matrix_scene_from_name("trace") != MATRIX_SCENE_NEO_TRACE) return 0;
    if (matrix_scene_from_name("bogus") != MATRIX_SCENE_NONE) return 0;
    if (matrix_scene_duration(MATRIX_SCENE_NEO_TRACE) != TRACE_DURATION) return 0;
    if (trace_locked_count(0.5) != 0U) return 0;
    if (trace_locked_count(6.0) != 10U) return 0;
    if (trace_phase_at(5.4) != TRACE_PHASE_ACQUIRE) return 0;
    if (trace_phase_at(5.6) != TRACE_PHASE_APERTURE) return 0;
    if (trace_phase_at(7.4) != TRACE_PHASE_BLACKOUT) return 0;
    if (trace_phase_at(8.5) != TRACE_PHASE_TUNNEL) return 0;
    if (trace_phase_at(10.5) != TRACE_PHASE_PHOSPHOR) return 0;
    if (trace_phase_at(11.5) != TRACE_PHASE_GLYPH_REVEAL) return 0;
    if (trace_phase_at(13.0) != TRACE_PHASE_SEARCH_UI) return 0;
    if (!neo_workstation_self_test()) return 0;
    if (matrix_scene_duration(MATRIX_SCENE_NEO_TERMINAL) != 76.0) return 0;
    if (matrix_scene_rain_start(MATRIX_SCENE_NEO_TERMINAL) != 73.0) return 0;
    if (matrix_scene_rain_opacity(MATRIX_SCENE_NEO_TERMINAL, 73.0) != 0.0F) return 0;
    if (matrix_scene_rain_opacity(MATRIX_SCENE_NEO_TERMINAL, 76.0) != 1.0F) return 0;

    cue = cue_at(24.0);
    if (!cue || cue->kind != SCENE_BLACK_CURSOR) return 0;
    cue = cue_at(25.0);
    if (!cue || cue->kind != SCENE_TYPE_TEXT || strcmp(cue->text, "Wake up, Neo...") != 0) return 0;
    cue = cue_at(67.0);
    if (!cue || cue->kind != SCENE_SHOW_TEXT || strcmp(cue->text, "Knock, knock, Neo.") != 0) return 0;
    cue = cue_at(74.0);
    if (!cue || cue->kind != SCENE_RAIN_TRANSITION) return 0;

    if (typed_count_window("Wake up, Neo...", 25.0, 25.0, 29.0) != 0U) return 0;
    if (typed_count_window("Wake up, Neo...", 29.0, 25.0, 29.0) != strlen("Wake up, Neo...")) return 0;
    if (typed_count_window("Follow the white rabbit.", 60.5, 55.0, 66.0) == 0U) return 0;
    return 1;
}
