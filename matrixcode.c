#define _POSIX_C_SOURCE 200809L

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#if defined(MATRIXCODE_USE_MINIMAL_GL_HEADERS)
# include "compat/minigl.h"
#else
# include <GL/gl.h>
# include <GL/glx.h>
#endif

#include "glyphs.h"

#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MATRIXCODE_VERSION "0.1.0"
#define MAX_DROPS_PER_COLUMN 2
#define DEFAULT_WINDOW_WIDTH 960
#define DEFAULT_WINDOW_HEIGHT 540

typedef struct rng_state {
    uint64_t state;
} rng_state;

typedef struct options {
    int root_mode;
    int window_mode;
    int have_window_id;
    Window window_id;
    int width;
    int height;
    int fps;
    unsigned int delay_usec;
    int cell_size;
    int density;
    int speed;
    int trail;
    int cycle;
    int glow;
    int contrast;
    uint64_t seed;
    int seed_set;
    unsigned long frames;
    int no_vsync;
    int verbose;
    int self_test;
    const char *screenshot_path;
} options;

typedef struct cell {
    uint16_t glyph;
    uint8_t occupied;
    uint8_t phase;
} cell;

typedef struct rain_column {
    float speed;
    float period;
    float offset[MAX_DROPS_PER_COLUMN];
    float length[MAX_DROPS_PER_COLUMN];
    int last_head[MAX_DROPS_PER_COLUMN];
    int drop_count;
    int active;
    float brightness;
} rain_column;

typedef struct simulation {
    unsigned int columns;
    unsigned int rows;
    float cell_width;
    float cell_height;
    float origin_x;
    float origin_y;
    cell *cells;
    rain_column *column;
    double cycle_accumulator;
    rng_state rng;
} simulation;

typedef struct gl_resources {
    GLuint core_texture;
    GLuint glow_texture;
    matrixcode_atlas atlas;
} gl_resources;

typedef struct app {
    options opts;
    Display *display;
    int screen;
    Window window;
    Window owned_window;
    Colormap colormap;
    XVisualInfo *visual_info;
    GLXContext context;
    int double_buffer;
    Atom wm_delete;
    int width;
    int height;
    gl_resources gl;
    simulation sim;
    unsigned long rendered_frames;
} app;

typedef int (*swap_interval_sgi_proc)(int);
typedef void (*swap_interval_ext_proc)(Display *, GLXDrawable, int);

static volatile sig_atomic_t stop_requested = 0;

static void signal_handler(int signal_number)
{
    (void) signal_number;
    stop_requested = 1;
}

static uint64_t rng_next_u64(rng_state *rng)
{
    uint64_t x = rng->state;
    if (x == 0U) {
        x = UINT64_C(0x9e3779b97f4a7c15);
    }
    x ^= x >> 12U;
    x ^= x << 25U;
    x ^= x >> 27U;
    rng->state = x;
    return x * UINT64_C(2685821657736338717);
}

static unsigned int rng_bounded(rng_state *rng, unsigned int upper)
{
    if (upper == 0U) {
        return 0U;
    }
    return (unsigned int) (rng_next_u64(rng) % upper);
}

static float rng_float(rng_state *rng)
{
    uint64_t value = rng_next_u64(rng) >> 40U;
    return (float) value / 16777216.0F;
}

static uint32_t hash_u32(uint32_t value)
{
    value ^= value >> 16U;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15U;
    value *= UINT32_C(0x846ca68b);
    value ^= value >> 16U;
    return value;
}

static double monotonic_seconds(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0.0;
    }
    return (double) ts.tv_sec + (double) ts.tv_nsec / 1000000000.0;
}

static void sleep_seconds(double seconds)
{
    struct timespec request;
    struct timespec remainder;
    if (seconds <= 0.0) {
        return;
    }
    request.tv_sec = (time_t) seconds;
    request.tv_nsec = (long) ((seconds - (double) request.tv_sec) * 1000000000.0);
    while (nanosleep(&request, &remainder) != 0 && errno == EINTR) {
        request = remainder;
    }
}

static void print_usage(FILE *stream, const char *program)
{
    fprintf(stream,
            "Usage: %s [options]\n"
            "\n"
            "XScreenSaver window options:\n"
            "  -root, --root               draw on the root window\n"
            "  -window, --window           create a test window (default)\n"
            "  -window-id ID               draw into an existing X window\n"
            "  -geometry WxH               test-window size (default 960x540)\n"
            "\n"
            "Visual options:\n"
            "  -fps N                      frame cap, 5..120 (default 30)\n"
            "  -delay USEC                 frame delay; overrides -fps\n"
            "  -cell-size N                glyph height, 10..40 (default 18)\n"
            "  -density N                  active-column percentage, 10..90\n"
            "  -speed N                    fall-speed percentage, 25..250\n"
            "  -trail N                    nominal trail length, 6..40 cells\n"
            "  -cycle N                    glyph-change percentage, 0..300\n"
            "  -glow N                     glow strength, 0..100\n"
            "  -contrast N                 trail contrast, 20..100\n"
            "  -seed N                     deterministic random seed\n"
            "  -no-vsync                   disable GLX swap-interval request\n"
            "\n"
            "Testing options:\n"
            "  -frames N                   exit after N rendered frames\n"
            "  -screenshot FILE.ppm        save the last frame as binary PPM\n"
            "  -self-test                  run non-graphical internal tests\n"
            "  -verbose                    print GL and grid diagnostics\n"
            "  -version                    print version\n"
            "  -help                       show this help\n",
            program);
}

static int parse_long(const char *text, long minimum, long maximum, long *result)
{
    char *end = NULL;

    if (text == NULL || result == NULL) {
        return 0;
    }
    long value;
    errno = 0;
    value = strtol(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' ||
        value < minimum || value > maximum) {
        return 0;
    }
    *result = value;
    return 1;
}

static int parse_u64(const char *text, uint64_t *result)
{
    char *end = NULL;

    if (text == NULL || result == NULL) {
        return 0;
    }
    unsigned long long value;
    errno = 0;
    value = strtoull(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0') {
        return 0;
    }
    *result = (uint64_t) value;
    return 1;
}

static int parse_window_id(const char *text, Window *result)
{
    uint64_t value;
    if (!parse_u64(text, &value)) {
        return 0;
    }
    *result = (Window) value;
    return 1;
}

static int parse_geometry(const char *text, int *width, int *height)
{
    char *separator;
    char left[32];
    long parsed_width;
    long parsed_height;
    size_t left_length;

    if (text == NULL || width == NULL || height == NULL) {
        return 0;
    }
    separator = strchr(text, 'x');
    if (separator == NULL) {
        separator = strchr(text, 'X');
    }
    if (separator == NULL) {
        return 0;
    }
    left_length = (size_t) (separator - text);
    if (left_length == 0U || left_length >= sizeof(left)) {
        return 0;
    }
    memcpy(left, text, left_length);
    left[left_length] = '\0';
    if (!parse_long(left, 160L, 8192L, &parsed_width) ||
        !parse_long(separator + 1, 120L, 8192L, &parsed_height)) {
        return 0;
    }
    *width = (int) parsed_width;
    *height = (int) parsed_height;
    return 1;
}

static void options_defaults(options *opts)
{
    memset(opts, 0, sizeof(*opts));
    opts->window_mode = 1;
    opts->width = DEFAULT_WINDOW_WIDTH;
    opts->height = DEFAULT_WINDOW_HEIGHT;
    opts->fps = 30;
    opts->delay_usec = 33333U;
    opts->cell_size = 18;
    opts->density = 42;
    opts->speed = 100;
    opts->trail = 18;
    opts->cycle = 100;
    opts->glow = 68;
    opts->contrast = 78;
}

static int option_takes_value(const char *argument)
{
    return strcmp(argument, "-window-id") == 0 ||
           strcmp(argument, "--window-id") == 0 ||
           strcmp(argument, "-geometry") == 0 ||
           strcmp(argument, "--geometry") == 0 ||
           strcmp(argument, "-fps") == 0 ||
           strcmp(argument, "--fps") == 0 ||
           strcmp(argument, "-maxfps") == 0 ||
           strcmp(argument, "--maxfps") == 0 ||
           strcmp(argument, "-delay") == 0 ||
           strcmp(argument, "--delay") == 0 ||
           strcmp(argument, "-cell-size") == 0 ||
           strcmp(argument, "--cell-size") == 0 ||
           strcmp(argument, "-density") == 0 ||
           strcmp(argument, "--density") == 0 ||
           strcmp(argument, "-speed") == 0 ||
           strcmp(argument, "--speed") == 0 ||
           strcmp(argument, "-trail") == 0 ||
           strcmp(argument, "--trail") == 0 ||
           strcmp(argument, "-cycle") == 0 ||
           strcmp(argument, "--cycle") == 0 ||
           strcmp(argument, "-glow") == 0 ||
           strcmp(argument, "--glow") == 0 ||
           strcmp(argument, "-contrast") == 0 ||
           strcmp(argument, "--contrast") == 0 ||
           strcmp(argument, "-seed") == 0 ||
           strcmp(argument, "--seed") == 0 ||
           strcmp(argument, "-frames") == 0 ||
           strcmp(argument, "--frames") == 0 ||
           strcmp(argument, "-screenshot") == 0 ||
           strcmp(argument, "--screenshot") == 0;
}

static int parse_options(int argc, char **argv, options *opts)
{
    int i;
    options_defaults(opts);

    for (i = 1; i < argc; i++) {
        const char *argument = argv[i];
        const char *value = NULL;
        long number;

        if (option_takes_value(argument)) {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s requires a value\n", argument);
                return 0;
            }
            value = argv[++i];
        }

        if (strcmp(argument, "-root") == 0 || strcmp(argument, "--root") == 0) {
            opts->root_mode = 1;
            opts->window_mode = 0;
            opts->have_window_id = 0;
        } else if (strcmp(argument, "-window") == 0 ||
                   strcmp(argument, "--window") == 0) {
            opts->root_mode = 0;
            opts->window_mode = 1;
            opts->have_window_id = 0;
        } else if (strcmp(argument, "-window-id") == 0 ||
                   strcmp(argument, "--window-id") == 0) {
            if (!parse_window_id(value, &opts->window_id)) {
                fprintf(stderr, "invalid window ID: %s\n", value);
                return 0;
            }
            opts->root_mode = 0;
            opts->window_mode = 0;
            opts->have_window_id = 1;
        } else if (strcmp(argument, "-geometry") == 0 ||
                   strcmp(argument, "--geometry") == 0) {
            if (!parse_geometry(value, &opts->width, &opts->height)) {
                fprintf(stderr, "invalid geometry: %s\n", value);
                return 0;
            }
        } else if (strcmp(argument, "-fps") == 0 ||
                   strcmp(argument, "--fps") == 0 ||
                   strcmp(argument, "-maxfps") == 0 ||
                   strcmp(argument, "--maxfps") == 0) {
            if (!parse_long(value, 5L, 120L, &number)) {
                fprintf(stderr, "invalid FPS: %s\n", value);
                return 0;
            }
            opts->fps = (int) number;
            opts->delay_usec = (unsigned int) (1000000L / number);
        } else if (strcmp(argument, "-delay") == 0 ||
                   strcmp(argument, "--delay") == 0) {
            if (!parse_long(value, 1000L, 200000L, &number)) {
                fprintf(stderr, "invalid delay: %s\n", value);
                return 0;
            }
            opts->delay_usec = (unsigned int) number;
            opts->fps = (int) (1000000L / number);
        } else if (strcmp(argument, "-cell-size") == 0 ||
                   strcmp(argument, "--cell-size") == 0) {
            if (!parse_long(value, 10L, 40L, &number)) {
                fprintf(stderr, "invalid cell size: %s\n", value);
                return 0;
            }
            opts->cell_size = (int) number;
        } else if (strcmp(argument, "-density") == 0 ||
                   strcmp(argument, "--density") == 0) {
            if (!parse_long(value, 10L, 90L, &number)) {
                fprintf(stderr, "invalid density: %s\n", value);
                return 0;
            }
            opts->density = (int) number;
        } else if (strcmp(argument, "-speed") == 0 ||
                   strcmp(argument, "--speed") == 0) {
            if (!parse_long(value, 25L, 250L, &number)) {
                fprintf(stderr, "invalid speed: %s\n", value);
                return 0;
            }
            opts->speed = (int) number;
        } else if (strcmp(argument, "-trail") == 0 ||
                   strcmp(argument, "--trail") == 0) {
            if (!parse_long(value, 6L, 40L, &number)) {
                fprintf(stderr, "invalid trail length: %s\n", value);
                return 0;
            }
            opts->trail = (int) number;
        } else if (strcmp(argument, "-cycle") == 0 ||
                   strcmp(argument, "--cycle") == 0) {
            if (!parse_long(value, 0L, 300L, &number)) {
                fprintf(stderr, "invalid cycle rate: %s\n", value);
                return 0;
            }
            opts->cycle = (int) number;
        } else if (strcmp(argument, "-glow") == 0 ||
                   strcmp(argument, "--glow") == 0) {
            if (!parse_long(value, 0L, 100L, &number)) {
                fprintf(stderr, "invalid glow strength: %s\n", value);
                return 0;
            }
            opts->glow = (int) number;
        } else if (strcmp(argument, "-contrast") == 0 ||
                   strcmp(argument, "--contrast") == 0) {
            if (!parse_long(value, 20L, 100L, &number)) {
                fprintf(stderr, "invalid contrast: %s\n", value);
                return 0;
            }
            opts->contrast = (int) number;
        } else if (strcmp(argument, "-seed") == 0 ||
                   strcmp(argument, "--seed") == 0) {
            if (!parse_u64(value, &opts->seed)) {
                fprintf(stderr, "invalid seed: %s\n", value);
                return 0;
            }
            opts->seed_set = 1;
        } else if (strcmp(argument, "-frames") == 0 ||
                   strcmp(argument, "--frames") == 0) {
            if (!parse_long(value, 1L, 10000000L, &number)) {
                fprintf(stderr, "invalid frame count: %s\n", value);
                return 0;
            }
            opts->frames = (unsigned long) number;
        } else if (strcmp(argument, "-screenshot") == 0 ||
                   strcmp(argument, "--screenshot") == 0) {
            opts->screenshot_path = value;
        } else if (strcmp(argument, "-no-vsync") == 0 ||
                   strcmp(argument, "--no-vsync") == 0) {
            opts->no_vsync = 1;
        } else if (strcmp(argument, "-vsync") == 0 ||
                   strcmp(argument, "--vsync") == 0) {
            opts->no_vsync = 0;
        } else if (strcmp(argument, "-verbose") == 0 ||
                   strcmp(argument, "--verbose") == 0) {
            opts->verbose = 1;
        } else if (strcmp(argument, "-self-test") == 0 ||
                   strcmp(argument, "--self-test") == 0) {
            opts->self_test = 1;
        } else if (strcmp(argument, "-version") == 0 ||
                   strcmp(argument, "--version") == 0) {
            printf("matrixcode %s\n", MATRIXCODE_VERSION);
            exit(EXIT_SUCCESS);
        } else if (strcmp(argument, "-help") == 0 ||
                   strcmp(argument, "--help") == 0 ||
                   strcmp(argument, "-h") == 0) {
            print_usage(stdout, argv[0]);
            exit(EXIT_SUCCESS);
        } else {
            fprintf(stderr, "unknown option: %s\n", argument);
            return 0;
        }
    }
    return 1;
}

static unsigned int choose_glyph(simulation *sim)
{
    unsigned int total = (unsigned int) matrixcode_base_glyph_count();
    unsigned int base;
    unsigned int roll = rng_bounded(&sim->rng, 100U);
    unsigned int mirror;

    if (roll < 72U) {
        base = rng_bounded(&sim->rng, MATRIXCODE_KANA_COUNT);
    } else if (roll < 88U) {
        base = MATRIXCODE_DIGIT_FIRST +
               rng_bounded(&sim->rng, MATRIXCODE_DIGIT_COUNT);
    } else {
        base = MATRIXCODE_SYMBOL_FIRST +
               rng_bounded(&sim->rng, total - MATRIXCODE_SYMBOL_FIRST);
    }
    mirror = rng_bounded(&sim->rng, 100U) < 44U ? total : 0U;
    return base + mirror;
}

static void simulation_free(simulation *sim)
{
    free(sim->cells);
    free(sim->column);
    sim->cells = NULL;
    sim->column = NULL;
    memset(sim, 0, sizeof(*sim));
}

static int simulation_resize(simulation *sim, const options *opts,
                             int width, int height)
{
    uint64_t saved_rng_state = sim->rng.state;
    unsigned int columns;
    unsigned int rows;
    size_t cell_count;
    unsigned int x;
    unsigned int y;

    simulation_free(sim);
    sim->rng.state = saved_rng_state != 0U ? saved_rng_state : UINT64_C(0x6A09E667F3BCC909);
    sim->cell_height = (float) opts->cell_size;
    sim->cell_width = fmaxf(7.0F, sim->cell_height * 0.64F);
    columns = (unsigned int) fmaxf(1.0F, floorf((float) width / sim->cell_width));
    rows = (unsigned int) fmaxf(1.0F, floorf((float) height / sim->cell_height)) + 2U;
    cell_count = (size_t) columns * rows;

    sim->cells = calloc(cell_count, sizeof(*sim->cells));
    sim->column = calloc(columns, sizeof(*sim->column));
    if (sim->cells == NULL || sim->column == NULL) {
        simulation_free(sim);
        return 0;
    }

    sim->columns = columns;
    sim->rows = rows;
    sim->origin_x = ((float) width - (float) columns * sim->cell_width) * 0.5F;
    sim->origin_y = -sim->cell_height;

    for (y = 0U; y < rows; y++) {
        for (x = 0U; x < columns; x++) {
            cell *current = &sim->cells[(size_t) y * columns + x];
            current->glyph = (uint16_t) choose_glyph(sim);
            current->occupied = (uint8_t) (rng_bounded(&sim->rng, 100U) < 94U);
            current->phase = (uint8_t) rng_bounded(&sim->rng, 256U);
        }
    }

    for (x = 0U; x < columns; x++) {
        rain_column *column = &sim->column[x];
        float speed_scale = (float) opts->speed / 100.0F;
        float length_variation = 0.62F + rng_float(&sim->rng) * 0.85F;
        float nominal_length = (float) opts->trail * length_variation;
        float gap = 13.0F + rng_float(&sim->rng) * 35.0F;
        unsigned int d;

        column->active = rng_bounded(&sim->rng, 100U) < (unsigned int) opts->density;
        column->drop_count = (rng_bounded(&sim->rng, 100U) < 27U) ? 2 : 1;
        column->speed = (4.0F + rng_float(&sim->rng) * 5.5F) * speed_scale;
        column->period = (float) rows + nominal_length + gap;
        column->brightness = 0.72F + rng_float(&sim->rng) * 0.40F;
        column->offset[0] = rng_float(&sim->rng) * column->period;
        column->offset[1] = fmodf(column->offset[0] + column->period *
                                  (0.46F + rng_float(&sim->rng) * 0.10F),
                                  column->period);
        for (d = 0U; d < MAX_DROPS_PER_COLUMN; d++) {
            column->length[d] = fmaxf(5.0F, nominal_length *
                                      (0.80F + rng_float(&sim->rng) * 0.35F));
            column->last_head[d] = -1000000;
        }
    }
    return 1;
}

static float drop_head(const rain_column *column, int drop_index, double time_value)
{
    float phase = fmodf((float) time_value * column->speed +
                        column->offset[drop_index], column->period);
    return phase - column->length[drop_index];
}

static float drop_intensity(float distance, float length)
{
    float normalized;
    float fade;
    if (distance < -0.35F || distance > length) {
        return 0.0F;
    }
    if (distance < 0.65F) {
        return 1.0F - fmaxf(0.0F, distance) * 0.12F;
    }
    normalized = distance / length;
    fade = expf(-distance * 0.115F) * powf(fmaxf(0.0F, 1.0F - normalized), 0.34F);
    return fade;
}

static float cell_intensity(const simulation *sim, unsigned int column_index,
                            unsigned int row_index, double time_value,
                            int *is_head)
{
    const rain_column *column = &sim->column[column_index];
    float best = 0.0F;
    int head = 0;
    int d;

    if (!column->active) {
        if (is_head != NULL) {
            *is_head = 0;
        }
        return 0.0F;
    }

    for (d = 0; d < column->drop_count; d++) {
        float distance = drop_head(column, d, time_value) - (float) row_index;
        float intensity = drop_intensity(distance, column->length[d]);
        if (intensity > best) {
            best = intensity;
            head = distance >= -0.35F && distance < 0.85F;
        }
    }
    if (is_head != NULL) {
        *is_head = head;
    }
    return fminf(1.0F, best * column->brightness);
}

static void mutate_cell(simulation *sim, unsigned int x, unsigned int y,
                        int force_occupied)
{
    cell *target;
    if (x >= sim->columns || y >= sim->rows) {
        return;
    }
    target = &sim->cells[(size_t) y * sim->columns + x];
    target->glyph = (uint16_t) choose_glyph(sim);
    if (force_occupied) {
        target->occupied = 1U;
    } else if (rng_bounded(&sim->rng, 100U) < 8U) {
        target->occupied = (uint8_t) !target->occupied;
    }
    target->phase = (uint8_t) rng_bounded(&sim->rng, 256U);
}

static void simulation_update(simulation *sim, const options *opts,
                              double time_value, double delta)
{
    unsigned int x;
    double mutations_per_second;
    unsigned int random_mutations;

    for (x = 0U; x < sim->columns; x++) {
        rain_column *column = &sim->column[x];
        int d;
        if (!column->active) {
            continue;
        }
        for (d = 0; d < column->drop_count; d++) {
            int head = (int) floorf(drop_head(column, d, time_value));
            int previous = column->last_head[d];
            if (previous < -999999) {
                column->last_head[d] = head;
                previous = head - 1;
            }
            if (head > previous) {
                int row;
                int limit = head - previous;
                if (limit > (int) sim->rows + 8) {
                    limit = 1;
                    previous = head - 1;
                }
                for (row = previous + 1; row <= previous + limit; row++) {
                    if (row >= 0 && row < (int) sim->rows) {
                        mutate_cell(sim, x, (unsigned int) row, 1);
                        if (row > 0 && rng_bounded(&sim->rng, 100U) < 24U) {
                            mutate_cell(sim, x, (unsigned int) row - 1U, 0);
                        }
                    }
                }
            }
            column->last_head[d] = head;
        }
    }

    mutations_per_second = (double) sim->columns * (double) sim->rows *
                           0.026 * (double) opts->cycle / 100.0;
    sim->cycle_accumulator += delta * mutations_per_second;
    random_mutations = (unsigned int) sim->cycle_accumulator;
    sim->cycle_accumulator -= (double) random_mutations;
    while (random_mutations-- > 0U) {
        unsigned int rx = rng_bounded(&sim->rng, sim->columns);
        unsigned int ry = rng_bounded(&sim->rng, sim->rows);
        if (cell_intensity(sim, rx, ry, time_value, NULL) > 0.035F ||
            rng_bounded(&sim->rng, 100U) < 12U) {
            mutate_cell(sim, rx, ry, 0);
        }
    }
}

static void setup_projection(int width, int height)
{
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, (double) width, (double) height, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static int upload_texture(GLuint *texture, const matrixcode_atlas *atlas,
                          const uint8_t *pixels)
{
    glGenTextures(1, texture);
    if (*texture == 0U) {
        return 0;
    }
    glBindTexture(GL_TEXTURE_2D, *texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 (GLsizei) atlas->width, (GLsizei) atlas->height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    return 1;
}

static int gl_resources_init(gl_resources *resources)
{
    memset(resources, 0, sizeof(*resources));
    if (!matrixcode_build_atlas(&resources->atlas)) {
        fprintf(stderr, "could not construct glyph atlas\n");
        return 0;
    }
    if (!upload_texture(&resources->core_texture, &resources->atlas,
                        resources->atlas.core_rgba) ||
        !upload_texture(&resources->glow_texture, &resources->atlas,
                        resources->atlas.glow_rgba)) {
        fprintf(stderr, "could not upload glyph textures\n");
        return 0;
    }
    return 1;
}

static void gl_resources_free(gl_resources *resources)
{
    if (resources->core_texture != 0U) {
        glDeleteTextures(1, &resources->core_texture);
    }
    if (resources->glow_texture != 0U) {
        glDeleteTextures(1, &resources->glow_texture);
    }
    matrixcode_free_atlas(&resources->atlas);
    memset(resources, 0, sizeof(*resources));
}

static void atlas_coordinates(const matrixcode_atlas *atlas,
                              unsigned int glyph,
                              float *u0, float *v0, float *u1, float *v1)
{
    const float padding = 7.0F;
    unsigned int column = glyph % atlas->columns;
    unsigned int row = glyph / atlas->columns;
    float left = (float) column * (float) atlas->cell_width + padding;
    float top = (float) row * (float) atlas->cell_height + padding;
    float right = left + (float) MATRIXCODE_GLYPH_WIDTH * 4.0F;
    float bottom = top + (float) MATRIXCODE_GLYPH_HEIGHT * 4.0F;
    *u0 = left / (float) atlas->width;
    *v0 = top / (float) atlas->height;
    *u1 = right / (float) atlas->width;
    *v1 = bottom / (float) atlas->height;
}

static void emit_glyph_quad(const app *application, unsigned int glyph,
                            float x, float y, float width, float height,
                            float red, float green, float blue, float alpha)
{
    float u0;
    float v0;
    float u1;
    float v1;
    atlas_coordinates(&application->gl.atlas, glyph, &u0, &v0, &u1, &v1);
    glColor4f(red, green, blue, alpha);
    glTexCoord2f(u0, v0);
    glVertex2f(x, y);
    glTexCoord2f(u1, v0);
    glVertex2f(x + width, y);
    glTexCoord2f(u1, v1);
    glVertex2f(x + width, y + height);
    glTexCoord2f(u0, v1);
    glVertex2f(x, y + height);
}

static float smoothstepf(float edge0, float edge1, float value)
{
    float x;
    if (edge0 == edge1) {
        return value < edge0 ? 0.0F : 1.0F;
    }
    x = (value - edge0) / (edge1 - edge0);
    x = fminf(1.0F, fmaxf(0.0F, x));
    return x * x * (3.0F - 2.0F * x);
}

static void render_pass(const app *application, double time_value, int glow_pass)
{
    const simulation *sim = &application->sim;
    const options *opts = &application->opts;
    unsigned int x;
    unsigned int y;
    float contrast_gamma = 0.58F + (100.0F - (float) opts->contrast) * 0.010F;
    uint32_t time_bucket = (uint32_t) floor(time_value * 15.0);

    glBegin(GL_QUADS);
    for (y = 0U; y < sim->rows; y++) {
        for (x = 0U; x < sim->columns; x++) {
            const cell *current = &sim->cells[(size_t) y * sim->columns + x];
            int is_head = 0;
            float intensity = cell_intensity(sim, x, y, time_value, &is_head);
            uint32_t noise_hash;
            float noise;
            float head_mix;
            float red;
            float green;
            float blue;
            float alpha;
            float px;
            float py;
            float draw_width;
            float draw_height;

            if (!current->occupied || intensity < 0.012F) {
                continue;
            }
            noise_hash = hash_u32(((uint32_t) current->phase |
                                   (x << 8U)) ^ (y << 20U) ^ time_bucket);
            noise = 0.89F + (float) (noise_hash & 255U) / 255.0F * 0.15F;
            intensity = powf(fminf(1.0F, intensity * noise), contrast_gamma);
            head_mix = smoothstepf(0.73F, 1.0F, intensity);
            if (is_head) {
                head_mix = fmaxf(head_mix, 0.72F);
            }

            red = 0.008F + head_mix * 0.70F;
            green = 0.46F + intensity * 0.50F + head_mix * 0.15F;
            blue = 0.035F + intensity * 0.10F + head_mix * 0.56F;
            green = fminf(1.0F, green);
            blue = fminf(1.0F, blue);

            px = sim->origin_x + (float) x * sim->cell_width;
            py = sim->origin_y + (float) y * sim->cell_height;
            draw_width = sim->cell_width;
            draw_height = sim->cell_height;

            if (glow_pass) {
                float expansion = 0.34F * sim->cell_height;
                float glow_strength = (float) opts->glow / 100.0F;
                alpha = intensity * (0.24F + head_mix * 0.18F) * glow_strength;
                px -= expansion * 0.5F;
                py -= expansion * 0.5F;
                draw_width += expansion;
                draw_height += expansion;
                emit_glyph_quad(application, current->glyph,
                                px, py, draw_width, draw_height,
                                red * 0.16F, green * 0.90F, blue * 0.35F, alpha);
            } else {
                alpha = fminf(1.0F, intensity * (0.78F + head_mix * 0.42F));
                emit_glyph_quad(application, current->glyph,
                                px, py, draw_width, draw_height,
                                red, green, blue, alpha);
            }
        }
    }
    glEnd();
}

static void render_frame(app *application, double time_value)
{
    glClearColor(0.0F, 0.001F, 0.0F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);

    if (application->opts.glow > 0) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glBindTexture(GL_TEXTURE_2D, application->gl.glow_texture);
        render_pass(application, time_value, 1);
    }

    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glBindTexture(GL_TEXTURE_2D, application->gl.core_texture);
    render_pass(application, time_value, 0);
    glFlush();
}

static int write_ppm(const app *application, const char *path)
{
    FILE *file;
    uint8_t *pixels;
    uint8_t *row;
    size_t row_size;
    int y;

    if (path == NULL) {
        return 1;
    }
    row_size = (size_t) application->width * 4U;
    pixels = malloc(row_size * (size_t) application->height);
    row = malloc((size_t) application->width * 3U);
    if (pixels == NULL || row == NULL) {
        fprintf(stderr, "could not allocate screenshot buffer\n");
        free(pixels);
        free(row);
        return 0;
    }

    glFinish();
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(application->double_buffer ? GL_BACK : GL_FRONT);
    glReadPixels(0, 0, application->width, application->height,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    file = fopen(path, "wb");
    if (file == NULL) {
        fprintf(stderr, "could not open screenshot %s: %s\n",
                path, strerror(errno));
        free(pixels);
        free(row);
        return 0;
    }
    fprintf(file, "P6\n%d %d\n255\n", application->width, application->height);
    for (y = application->height - 1; y >= 0; y--) {
        int x;
        const uint8_t *source = pixels + (size_t) y * row_size;
        for (x = 0; x < application->width; x++) {
            row[(size_t) x * 3U + 0U] = source[(size_t) x * 4U + 0U];
            row[(size_t) x * 3U + 1U] = source[(size_t) x * 4U + 1U];
            row[(size_t) x * 3U + 2U] = source[(size_t) x * 4U + 2U];
        }
        if (fwrite(row, 1U, (size_t) application->width * 3U, file) !=
            (size_t) application->width * 3U) {
            fprintf(stderr, "could not write screenshot %s\n", path);
            fclose(file);
            free(pixels);
            free(row);
            return 0;
        }
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "could not close screenshot %s\n", path);
        free(pixels);
        free(row);
        return 0;
    }
    free(pixels);
    free(row);
    return 1;
}

static XVisualInfo *visual_info_for_window(Display *display, int screen,
                                            Window window)
{
    XWindowAttributes attributes;
    XVisualInfo template;
    XVisualInfo *result;
    int count = 0;
    int use_gl = 0;

    if (!XGetWindowAttributes(display, window, &attributes)) {
        return NULL;
    }
    memset(&template, 0, sizeof(template));
    template.visualid = XVisualIDFromVisual(attributes.visual);
    template.screen = screen;
    result = XGetVisualInfo(display, VisualIDMask | VisualScreenMask,
                            &template, &count);
    if (result == NULL || count < 1) {
        return NULL;
    }
    if (glXGetConfig(display, result, GLX_USE_GL, &use_gl) != 0 || !use_gl) {
        XFree(result);
        return NULL;
    }
    return result;
}

static XVisualInfo *choose_owned_visual(Display *display, int screen,
                                        int *double_buffer)
{
    int attributes_double[] = {
        GLX_RGBA,
        GLX_DOUBLEBUFFER,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        None
    };
    int attributes_single[] = {
        GLX_RGBA,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        None
    };
    XVisualInfo *visual = glXChooseVisual(display, screen, attributes_double);
    if (visual != NULL) {
        *double_buffer = 1;
        return visual;
    }
    visual = glXChooseVisual(display, screen, attributes_single);
    *double_buffer = 0;
    return visual;
}

static int create_owned_window(app *application)
{
    XSetWindowAttributes attributes;
    XSizeHints size_hints;
    XClassHint class_hint;
    char resource_name[] = "matrixcode";
    char resource_class[] = "MatrixCode";

    application->visual_info = choose_owned_visual(application->display,
                                                    application->screen,
                                                    &application->double_buffer);
    if (application->visual_info == NULL) {
        fprintf(stderr, "no suitable GLX visual found\n");
        return 0;
    }

    application->colormap = XCreateColormap(
        application->display,
        RootWindow(application->display, application->screen),
        application->visual_info->visual,
        AllocNone);
    attributes.colormap = application->colormap;
    attributes.background_pixel = 0UL;
    attributes.border_pixel = 0UL;
    attributes.event_mask = ExposureMask | StructureNotifyMask |
                            KeyPressMask | ButtonPressMask;

    application->owned_window = XCreateWindow(
        application->display,
        RootWindow(application->display, application->screen),
        0, 0,
        (unsigned int) application->opts.width,
        (unsigned int) application->opts.height,
        0,
        application->visual_info->depth,
        InputOutput,
        application->visual_info->visual,
        CWBackPixel | CWBorderPixel | CWColormap | CWEventMask,
        &attributes);
    if (application->owned_window == None) {
        fprintf(stderr, "could not create X window\n");
        return 0;
    }
    application->window = application->owned_window;

    XStoreName(application->display, application->window,
               "MatrixCode XScreenSaver preview");
    class_hint.res_name = resource_name;
    class_hint.res_class = resource_class;
    XSetClassHint(application->display, application->window, &class_hint);
    size_hints.flags = PSize;
    size_hints.width = application->opts.width;
    size_hints.height = application->opts.height;
    XSetWMNormalHints(application->display, application->window, &size_hints);
    application->wm_delete = XInternAtom(application->display,
                                          "WM_DELETE_WINDOW", False);
    XSetWMProtocols(application->display, application->window,
                   &application->wm_delete, 1);
    XMapWindow(application->display, application->window);
    XSync(application->display, False);
    return 1;
}

static int select_target_window(app *application)
{
    const char *environment_window;
    Window target = None;
    XWindowAttributes attributes;
    int db = 0;

    environment_window = getenv("XSCREENSAVER_WINDOW");
    if (!application->opts.root_mode && !application->opts.have_window_id &&
        environment_window != NULL && *environment_window != '\0') {
        if (!parse_window_id(environment_window, &target)) {
            fprintf(stderr, "invalid XSCREENSAVER_WINDOW value: %s\n",
                    environment_window);
            return 0;
        }
        application->opts.window_mode = 0;
        application->opts.have_window_id = 1;
        application->opts.window_id = target;
    }

    if (application->opts.window_mode) {
        return create_owned_window(application);
    }
    if (application->opts.root_mode) {
        target = RootWindow(application->display, application->screen);
    } else if (application->opts.have_window_id) {
        target = application->opts.window_id;
    }
    if (target == None || !XGetWindowAttributes(application->display, target,
                                                &attributes)) {
        fprintf(stderr, "could not inspect target X window\n");
        return 0;
    }
    application->window = target;
    application->width = attributes.width;
    application->height = attributes.height;
    application->visual_info = visual_info_for_window(application->display,
                                                       application->screen,
                                                       target);
    if (application->visual_info == NULL) {
        fprintf(stderr, "target window visual is not GLX-capable\n");
        return 0;
    }
    if (glXGetConfig(application->display, application->visual_info,
                     GLX_DOUBLEBUFFER, &db) == 0) {
        application->double_buffer = db != 0;
    }
    XSelectInput(application->display, application->window,
                 ExposureMask | StructureNotifyMask);
    return 1;
}

static void request_swap_interval(app *application)
{
    const char *extensions;
    if (application->opts.no_vsync) {
        return;
    }
    extensions = glXQueryExtensionsString(application->display,
                                          application->screen);
    if (extensions != NULL && strstr(extensions, "GLX_EXT_swap_control") != NULL) {
        swap_interval_ext_proc swap_interval_ext =
            (swap_interval_ext_proc) glXGetProcAddressARB(
                (const GLubyte *) "glXSwapIntervalEXT");
        if (swap_interval_ext != NULL) {
            swap_interval_ext(application->display, application->window, 1);
            return;
        }
    }
    if (extensions != NULL && strstr(extensions, "GLX_SGI_swap_control") != NULL) {
        swap_interval_sgi_proc swap_interval_sgi =
            (swap_interval_sgi_proc) glXGetProcAddressARB(
                (const GLubyte *) "glXSwapIntervalSGI");
        if (swap_interval_sgi != NULL) {
            (void) swap_interval_sgi(1);
        }
    }
}

static int app_init(app *application, const options *opts)
{
    XWindowAttributes attributes;
    const GLubyte *vendor;
    const GLubyte *renderer;
    const GLubyte *version;

    memset(application, 0, sizeof(*application));
    application->opts = *opts;
    application->display = XOpenDisplay(NULL);
    if (application->display == NULL) {
        fprintf(stderr, "could not open X display\n");
        return 0;
    }
    application->screen = DefaultScreen(application->display);
    application->sim.rng.state = opts->seed_set ? opts->seed :
        ((uint64_t) time(NULL) << 32U) ^ (uint64_t) getpid() ^
        UINT64_C(0x6d6174726978636f);

    if (!select_target_window(application)) {
        return 0;
    }
    if (!XGetWindowAttributes(application->display, application->window,
                              &attributes)) {
        fprintf(stderr, "could not read target window geometry\n");
        return 0;
    }
    application->width = attributes.width;
    application->height = attributes.height;

    application->context = glXCreateContext(application->display,
                                             application->visual_info,
                                             NULL, True);
    if (application->context == NULL) {
        fprintf(stderr, "could not create GLX context\n");
        return 0;
    }
    if (!glXMakeCurrent(application->display, application->window,
                        application->context)) {
        fprintf(stderr, "could not make GLX context current\n");
        return 0;
    }
    request_swap_interval(application);
    setup_projection(application->width, application->height);
    if (!gl_resources_init(&application->gl)) {
        return 0;
    }
    if (!simulation_resize(&application->sim, &application->opts,
                           application->width, application->height)) {
        fprintf(stderr, "could not allocate rain simulation\n");
        return 0;
    }

    if (application->opts.verbose) {
        vendor = glGetString(GL_VENDOR);
        renderer = glGetString(GL_RENDERER);
        version = glGetString(GL_VERSION);
        fprintf(stderr, "GL vendor: %s\n", vendor != NULL ? (const char *) vendor : "unknown");
        fprintf(stderr, "GL renderer: %s\n", renderer != NULL ? (const char *) renderer : "unknown");
        fprintf(stderr, "GL version: %s\n", version != NULL ? (const char *) version : "unknown");
        fprintf(stderr, "grid: %u columns x %u rows, %.1fx%.1f cells\n",
                application->sim.columns, application->sim.rows,
                application->sim.cell_width, application->sim.cell_height);
    }
    return 1;
}

static void app_free(app *application)
{
    simulation_free(&application->sim);
    if (application->display != NULL && application->context != NULL) {
        gl_resources_free(&application->gl);
        glXMakeCurrent(application->display, None, NULL);
        glXDestroyContext(application->display, application->context);
        application->context = NULL;
    }
    if (application->display != NULL && application->owned_window != None) {
        XDestroyWindow(application->display, application->owned_window);
    }
    if (application->display != NULL && application->colormap != None) {
        XFreeColormap(application->display, application->colormap);
    }
    if (application->visual_info != NULL) {
        XFree(application->visual_info);
    }
    if (application->display != NULL) {
        XCloseDisplay(application->display);
    }
    memset(application, 0, sizeof(*application));
}

static int process_events(app *application)
{
    while (XPending(application->display) > 0) {
        XEvent event;
        XNextEvent(application->display, &event);
        if (event.type == ConfigureNotify) {
            int width = event.xconfigure.width;
            int height = event.xconfigure.height;
            if (width > 0 && height > 0 &&
                (width != application->width || height != application->height)) {
                application->width = width;
                application->height = height;
                setup_projection(width, height);
                if (!simulation_resize(&application->sim, &application->opts,
                                       width, height)) {
                    return 0;
                }
            }
        } else if (event.type == ClientMessage && application->owned_window != None &&
                   (Atom) event.xclient.data.l[0] == application->wm_delete) {
            return 0;
        } else if ((event.type == KeyPress || event.type == ButtonPress) &&
                   application->owned_window != None) {
            return 0;
        }
    }
    return 1;
}

static int run_app(app *application)
{
    double start = monotonic_seconds();
    double previous = start;
    double next_frame = start;
    double frame_interval = (double) application->opts.delay_usec / 1000000.0;
    int screenshot_written = 0;

    while (!stop_requested) {
        double now;
        double elapsed;
        double delta;

        if (!process_events(application)) {
            break;
        }
        now = monotonic_seconds();
        if (now < next_frame) {
            sleep_seconds(next_frame - now);
            now = monotonic_seconds();
        }
        if (application->opts.seed_set) {
            elapsed = (double) application->rendered_frames * frame_interval;
            delta = frame_interval;
        } else {
            elapsed = now - start;
            delta = now - previous;
            if (delta < 0.0 || delta > 0.25) {
                delta = frame_interval;
            }
        }
        previous = now;

        simulation_update(&application->sim, &application->opts,
                          elapsed, delta);
        render_frame(application, elapsed);
        application->rendered_frames++;

        if (application->opts.frames > 0UL &&
            application->rendered_frames >= application->opts.frames &&
            application->opts.screenshot_path != NULL) {
            screenshot_written = write_ppm(application,
                                            application->opts.screenshot_path);
        }

        if (application->double_buffer) {
            glXSwapBuffers(application->display, application->window);
        } else {
            glFlush();
        }

        if (application->opts.frames > 0UL &&
            application->rendered_frames >= application->opts.frames) {
            return application->opts.screenshot_path == NULL || screenshot_written;
        }
        next_frame += frame_interval;
        if (next_frame < now - frame_interval) {
            next_frame = now + frame_interval;
        }
    }
    return 1;
}

static int simulation_self_test(void)
{
    options opts;
    simulation first;
    simulation second;
    rng_state a;
    rng_state b;
    unsigned int i;
    int ok = 1;

    options_defaults(&opts);
    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));
    first.rng.state = UINT64_C(0x12345678abcdef01);
    second.rng.state = UINT64_C(0x12345678abcdef01);
    a.state = UINT64_C(0x55aa55aa55aa55aa);
    b.state = UINT64_C(0x55aa55aa55aa55aa);

    for (i = 0U; i < 1000U; i++) {
        if (rng_next_u64(&a) != rng_next_u64(&b)) {
            fprintf(stderr, "self-test: RNG is not deterministic\n");
            ok = 0;
            break;
        }
    }
    if (!(drop_intensity(0.0F, 18.0F) > drop_intensity(4.0F, 18.0F) &&
          drop_intensity(4.0F, 18.0F) > drop_intensity(14.0F, 18.0F) &&
          drop_intensity(19.0F, 18.0F) == 0.0F)) {
        fprintf(stderr, "self-test: trail intensity curve is invalid\n");
        ok = 0;
    }
    if (!simulation_resize(&first, &opts, 800, 600) ||
        !simulation_resize(&second, &opts, 800, 600)) {
        fprintf(stderr, "self-test: simulation allocation failed\n");
        simulation_free(&first);
        simulation_free(&second);
        return 0;
    }
    if (first.columns != second.columns || first.rows != second.rows ||
        memcmp(first.cells, second.cells,
               (size_t) first.columns * first.rows * sizeof(*first.cells)) != 0 ||
        memcmp(first.column, second.column,
               (size_t) first.columns * sizeof(*first.column)) != 0) {
        fprintf(stderr, "self-test: seeded simulations differ\n");
        ok = 0;
    }
    for (i = 0U; i < 180U; i++) {
        simulation_update(&first, &opts, (double) i / 30.0, 1.0 / 30.0);
    }
    for (i = 0U; i < first.columns * first.rows; i++) {
        if (first.cells[i].glyph >= matrixcode_total_glyph_count()) {
            fprintf(stderr, "self-test: glyph index escaped atlas\n");
            ok = 0;
            break;
        }
    }
    simulation_free(&first);
    simulation_free(&second);
    return ok;
}

static int run_self_tests(void)
{
    if (!matrixcode_glyphs_self_test()) {
        return 0;
    }
    if (!simulation_self_test()) {
        return 0;
    }
    printf("matrixcode: all self-tests passed\n");
    return 1;
}

int main(int argc, char **argv)
{
    options opts;
    app application;
    int success;

    if (!parse_options(argc, argv, &opts)) {
        print_usage(stderr, argv[0]);
        return EXIT_FAILURE;
    }
    if (opts.self_test) {
        return run_self_tests() ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);

    if (!app_init(&application, &opts)) {
        app_free(&application);
        return EXIT_FAILURE;
    }
    success = run_app(&application);
    app_free(&application);
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
