#include "scene.h"
#include "terminal_font.h"

#if defined(MATRIXCODE_USE_MINIMAL_GL_HEADERS)
# include "compat/minigl.h"
#else
# include <GL/gl.h>
#endif

#include <math.h>
#include <stddef.h>
#include <string.h>

#define SCENE_W 640.0F
#define SCENE_H 480.0F
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
    float flicker = 0.88F + 0.08F * sinf((float)t * 11.0F);
    terminal_draw_text("GLOBAL SEARCH", 48.0F, 52.0F, SEARCH_TITLE_PIXEL,
                       0.10F, 0.88F, 0.34F, flicker);
    terminal_draw_text("SEARCHING: MORPHEUS", 48.0F, 104.0F, SEARCH_BODY_PIXEL,
                       0.08F, 0.75F, 0.28F, 0.78F);
    terminal_draw_text("> NETWORK ARCHIVE", 48.0F, 154.0F, SEARCH_BODY_PIXEL,
                       0.06F, 0.62F, 0.23F, 0.62F);
    terminal_draw_text("> NEWS INDEX", 48.0F, 180.0F, SEARCH_BODY_PIXEL,
                       0.06F, 0.62F, 0.23F, 0.62F);
    terminal_draw_text("> INTERNATIONAL RECORDS", 48.0F, 206.0F, SEARCH_BODY_PIXEL,
                       0.06F, 0.62F, 0.23F, 0.62F);
    terminal_draw_text("MATCHES FOUND", 48.0F, 270.0F, SEARCH_BODY_PIXEL,
                       0.10F, 0.82F, 0.31F, 0.78F);
    terminal_draw_text("MORPHEUS / STATUS UNKNOWN", 48.0F, 304.0F, SEARCH_BODY_PIXEL,
                       0.08F, 0.70F, 0.26F, 0.70F);
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

matrix_scene_kind matrix_scene_from_name(const char *name)
{
    if (name && (strcmp(name, "neo-terminal") == 0 || strcmp(name, "neo") == 0))
        return MATRIX_SCENE_NEO_TERMINAL;
    return MATRIX_SCENE_NONE;
}

const char *matrix_scene_name(matrix_scene_kind scene)
{
    return scene == MATRIX_SCENE_NEO_TERMINAL ? "neo-terminal" : "none";
}

double matrix_scene_duration(matrix_scene_kind scene)
{
    return scene == MATRIX_SCENE_NEO_TERMINAL ? NEO_DURATION : 0.0;
}

double matrix_scene_rain_start(matrix_scene_kind scene)
{
    return scene == MATRIX_SCENE_NEO_TERMINAL ? NEO_RAIN_START : 0.0;
}

float matrix_scene_rain_opacity(matrix_scene_kind scene, double elapsed)
{
    double start;
    double end;
    double value;
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
    if (scene == MATRIX_SCENE_NEO_TERMINAL) return render_neo(elapsed, width, height);
    return MATRIX_SCENE_FINISHED;
}

int matrix_scene_self_test(void)
{
    const scene_cue *cue;
    if (matrix_scene_from_name("neo-terminal") != MATRIX_SCENE_NEO_TERMINAL) return 0;
    if (matrix_scene_from_name("bogus") != MATRIX_SCENE_NONE) return 0;
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
