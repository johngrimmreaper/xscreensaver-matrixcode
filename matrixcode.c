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
#include "scene.h"

#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MATRIXCODE_VERSION "0.2.0-film-rework"
#define DEFAULT_WINDOW_WIDTH 960
#define DEFAULT_WINDOW_HEIGHT 720
#define PI_D 3.14159265358979323846
#define SQRT2_F 1.41421356237F
#define SQRT5_F 2.23606797750F
#define CRT_REFERENCE_WIDTH 640.0F
#define CRT_REFERENCE_HEIGHT 480.0F
#define CRT_REFERENCE_CELL 8.0F
#define OPERATOR_REFERENCE_COLUMNS ((unsigned int)(CRT_REFERENCE_WIDTH / CRT_REFERENCE_CELL))
#define RESIZE_SETTLE_SECONDS 0.12

typedef enum profile_kind {
    PROFILE_OPERATOR_1999 = 0,
    PROFILE_OPENING_1999,
    PROFILE_CLEAN
} profile_kind;

typedef enum aspect_kind {
    ASPECT_AUTO = 0,
    ASPECT_4_3,
    ASPECT_16_9,
    ASPECT_CINEMA
} aspect_kind;

typedef struct rng_state { uint64_t state; } rng_state;

typedef struct options {
    int root_mode;
    int window_mode;
    int have_window_id;
    Window window_id;
    int width;
    int height;
    int fps;
    unsigned int delay_usec;
    unsigned int columns;
    int density;
    int speed;
    int trail;
    int cycle;
    int glow;
    int contrast;
    profile_kind profile;
    aspect_kind aspect;
    int crt;
    int curvature;
    int scanlines;
    int phosphor_mask;
    int vignette;
    int persistence;
    int overscan;
    int no_vsync;
    uint64_t seed;
    int seed_set;
    unsigned long frames;
    int verbose;
    int self_test;
    const char *screenshot_path;
    matrix_scene_kind startup_scene;
    double scene_time;
    int scene_time_set;
} options;

typedef struct cell {
    uint16_t glyph;
    uint8_t occupied;
    float age;
    float cycle_rate;
} cell;

typedef struct rain_column {
    float time_offset;
    float speed_scale;
    float brightness_scale;
} rain_column;

typedef struct content_rect {
    float x;
    float y;
    float width;
    float height;
} content_rect;

typedef struct simulation {
    unsigned int columns;
    unsigned int rows;
    float cell_width;
    float cell_height;
    content_rect content;
    cell *cells;
    rain_column *column;
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
    uint64_t base_seed;
    unsigned long resize_serial;
    int resize_pending;
    int pending_width;
    int pending_height;
    double resize_deadline;
} app;

typedef int (*swap_interval_sgi_proc)(int);
typedef void (*swap_interval_ext_proc)(Display *, GLXDrawable, int);

static volatile sig_atomic_t stop_requested = 0;

static void signal_handler(int sig)
{
    (void)sig;
    stop_requested = 1;
}

static uint64_t rng_next_u64(rng_state *rng)
{
    uint64_t x = rng->state;
    if (x == 0U) x = UINT64_C(0x9e3779b97f4a7c15);
    x ^= x >> 12U;
    x ^= x << 25U;
    x ^= x >> 27U;
    rng->state = x;
    return x * UINT64_C(2685821657736338717);
}

static unsigned int rng_bounded(rng_state *rng, unsigned int upper)
{
    if (upper == 0U) return 0U;
    return (unsigned int)(rng_next_u64(rng) % upper);
}

static float rng_float(rng_state *rng)
{
    uint64_t value = rng_next_u64(rng) >> 40U;
    return (float)value / 16777216.0F;
}

static double monotonic_seconds(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0.0;
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

static void sleep_seconds(double seconds)
{
    struct timespec req, rem;
    if (seconds <= 0.0) return;
    req.tv_sec = (time_t)seconds;
    req.tv_nsec = (long)((seconds - (double)req.tv_sec) * 1000000000.0);
    while (nanosleep(&req, &rem) != 0 && errno == EINTR) req = rem;
}

static int parse_long(const char *text, long minv, long maxv, long *result)
{
    char *end = NULL;
    long value;
    if (!text || !result) return 0;
    errno = 0;
    value = strtol(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' || value < minv || value > maxv) return 0;
    *result = value;
    return 1;
}

static int parse_double(const char *text, double minv, double maxv, double *result)
{
    char *end = NULL;
    double value;
    if (!text || !result) return 0;
    errno = 0;
    value = strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0' || !isfinite(value) ||
        value < minv || value > maxv) return 0;
    *result = value;
    return 1;
}

static int parse_u64(const char *text, uint64_t *result)
{
    char *end = NULL;
    unsigned long long value;
    if (!text || !result || text[0] == '-') return 0;
    errno = 0;
    value = strtoull(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0') return 0;
    *result = (uint64_t)value;
    return 1;
}

static int parse_window_id(const char *text, Window *result)
{
    uint64_t value;
    if (!parse_u64(text, &value)) return 0;
    *result = (Window)value;
    return 1;
}

static int parse_geometry(const char *text, int *width, int *height)
{
    const char *sep;
    char left[32];
    size_t len;
    long w, h;
    if (!text || !width || !height) return 0;
    sep = strchr(text, 'x');
    if (!sep) sep = strchr(text, 'X');
    if (!sep) return 0;
    len = (size_t)(sep - text);
    if (len == 0U || len >= sizeof(left)) return 0;
    memcpy(left, text, len);
    left[len] = '\0';
    if (!parse_long(left, 160L, 8192L, &w) || !parse_long(sep + 1, 120L, 8192L, &h)) return 0;
    *width = (int)w;
    *height = (int)h;
    return 1;
}

static profile_kind profile_from_name(const char *name)
{
    if (name && (strcmp(name, "opening") == 0 || strcmp(name, "opening1999") == 0)) return PROFILE_OPENING_1999;
    if (name && strcmp(name, "clean") == 0) return PROFILE_CLEAN;
    return PROFILE_OPERATOR_1999;
}

static const char *profile_name(profile_kind profile)
{
    if (profile == PROFILE_OPENING_1999) return "opening1999";
    if (profile == PROFILE_CLEAN) return "clean";
    return "operator1999";
}

static void options_profile_defaults(options *opts, profile_kind profile)
{
    memset(opts, 0, sizeof(*opts));
    opts->window_mode = 1;
    opts->width = DEFAULT_WINDOW_WIDTH;
    opts->height = DEFAULT_WINDOW_HEIGHT;
    opts->fps = 30;
    opts->delay_usec = 33333U;
    opts->profile = profile;
    opts->columns = OPERATOR_REFERENCE_COLUMNS;
    opts->density = 55;
    opts->speed = 40;
    opts->trail = 18;
    opts->cycle = 100;
    opts->glow = 76;
    opts->contrast = 82;
    opts->aspect = ASPECT_AUTO;
    opts->crt = 1;
    opts->curvature = 11;
    opts->scanlines = 32;
    opts->phosphor_mask = 14;
    opts->vignette = 30;
    opts->persistence = 14;
    opts->overscan = 2;
    opts->startup_scene = MATRIX_SCENE_NEO_TERMINAL;
    if (profile == PROFILE_OPENING_1999) {
        opts->aspect = ASPECT_CINEMA;
        opts->crt = 0;
        opts->columns = 108U;
        opts->density = 52;
        opts->glow = 72;
        opts->curvature = 0;
        opts->scanlines = 0;
        opts->phosphor_mask = 0;
        opts->vignette = 8;
        opts->persistence = 6;
        opts->overscan = 0;
    } else if (profile == PROFILE_CLEAN) {
        opts->aspect = ASPECT_AUTO;
        opts->crt = 0;
        opts->columns = 92U;
        opts->density = 48;
        opts->speed = 90;
        opts->trail = 15;
        opts->glow = 58;
        opts->curvature = 0;
        opts->scanlines = 0;
        opts->phosphor_mask = 0;
        opts->vignette = 0;
        opts->persistence = 0;
        opts->overscan = 0;
    }
}

static aspect_kind parse_aspect(const char *text)
{
    if (strcmp(text, "4:3") == 0 || strcmp(text, "4/3") == 0) return ASPECT_4_3;
    if (strcmp(text, "16:9") == 0 || strcmp(text, "16/9") == 0) return ASPECT_16_9;
    if (strcmp(text, "2.39:1") == 0 || strcmp(text, "cinema") == 0) return ASPECT_CINEMA;
    return ASPECT_AUTO;
}

static int option_takes_value(const char *arg)
{
    static const char *const names[] = {
        "-profile", "--profile", "-window-id", "--window-id", "-geometry", "--geometry",
        "-fps", "--fps", "-maxfps", "--maxfps", "-delay", "--delay", "-columns", "--columns",
        "-cell-size", "--cell-size", "-density", "--density", "-speed", "--speed", "-trail", "--trail",
        "-cycle", "--cycle", "-glow", "--glow", "-contrast", "--contrast", "-aspect", "--aspect",
        "-curvature", "--curvature", "-scanlines", "--scanlines", "-phosphor-mask", "--phosphor-mask",
        "-vignette", "--vignette", "-persistence", "--persistence", "-overscan", "--overscan",
        "-seed", "--seed", "-frames", "--frames", "-screenshot", "--screenshot", "-scene", "--scene",
        "-scene-time", "--scene-time"
    };
    size_t i;
    for (i = 0U; i < sizeof(names) / sizeof(names[0]); i++) if (strcmp(arg, names[i]) == 0) return 1;
    return 0;
}

static void print_usage(FILE *stream, const char *program)
{
    fprintf(stream,
        "Usage: %s [options]\n\n"
        "Profiles:\n"
        "  -profile operator1999|opening1999|clean  visual preset (default operator1999)\n"
        "\nXScreenSaver/window options:\n"
        "  -root | -window | -window-id ID\n"
        "  -scene neo-terminal | -no-scene startup scene (default neo-terminal)\n"
        "  -scene-time SECONDS           start/seek scene clock for calibration\n"
        "  -geometry WxH                 preview-window size\n"
        "\nFilm geometry and rain:\n"
        "  -columns N                    logical columns, 40..240 (operator default 80)\n"
        "  -aspect auto|4:3|16:9|2.39:1  content framing (operator default auto)\n"
        "  -density N                    lit-cell coverage, 10..90 (default 55)\n"
        "  -speed N                      fall speed percentage, 25..250 (operator default 40)\n"
        "  -trail N                      rain period/length, 6..40 (default 18)\n"
        "  -cycle N                      glyph cycling, 0..300\n"
        "  -glow N                       optical glow, 0..100\n"
        "  -contrast N                   glyph/trail contrast, 20..100\n"
        "\nCRT emulation:\n"
        "  -crt | -no-crt                enable/disable CRT treatment\n"
        "  -curvature N                  barrel curvature, 0..100\n"
        "  -scanlines N                  scanline strength, 0..100\n"
        "  -phosphor-mask N              vertical grille strength, 0..100\n"
        "  -vignette N                   edge darkening, 0..100\n"
        "  -persistence N                phosphor after-image, 0..100\n"
        "  -overscan N                   image inset, 0..10 percent\n"
        "\nTiming/testing:\n"
        "  -fps N | -delay USEC          frame cap\n"
        "  -seed N -frames N             deterministic render\n"
        "  -screenshot FILE.ppm          capture last deterministic frame\n"
        "  -no-vsync -verbose -self-test -version -help\n",
        program);
}

static int parse_options(int argc, char **argv, options *opts)
{
    int i;
    profile_kind profile = PROFILE_OPERATOR_1999;
    for (i = 1; i + 1 < argc; i++) {
        if (strcmp(argv[i], "-profile") == 0 || strcmp(argv[i], "--profile") == 0) profile = profile_from_name(argv[i + 1]);
    }
    options_profile_defaults(opts, profile);
    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];
        const char *value = NULL;
        long number;
        if (option_takes_value(arg)) {
            if (i + 1 >= argc) { fprintf(stderr, "%s requires a value\n", arg); return 0; }
            value = argv[++i];
        }
        if (strcmp(arg, "-profile") == 0 || strcmp(arg, "--profile") == 0) {
            if (strcmp(value, "operator") != 0 && strcmp(value, "operator1999") != 0 &&
                strcmp(value, "opening") != 0 && strcmp(value, "opening1999") != 0 && strcmp(value, "clean") != 0) {
                fprintf(stderr, "invalid profile: %s\n", value); return 0;
            }
        } else if (strcmp(arg, "-scene") == 0 || strcmp(arg, "--scene") == 0) {
            opts->startup_scene = matrix_scene_from_name(value);
            if (opts->startup_scene == MATRIX_SCENE_NONE) { fprintf(stderr, "invalid scene: %s\n", value); return 0; }
        } else if (strcmp(arg, "-no-scene") == 0 || strcmp(arg, "--no-scene") == 0) {
            opts->startup_scene = MATRIX_SCENE_NONE;
        } else if (strcmp(arg, "-scene-time") == 0 || strcmp(arg, "--scene-time") == 0) {
            if (!parse_double(value, 0.0, 86400.0, &opts->scene_time)) { fprintf(stderr, "invalid scene time: %s\n", value); return 0; }
            opts->scene_time_set = 1;
        } else if (strcmp(arg, "-root") == 0 || strcmp(arg, "--root") == 0) {
            opts->root_mode = 1; opts->window_mode = 0; opts->have_window_id = 0;
        } else if (strcmp(arg, "-window") == 0 || strcmp(arg, "--window") == 0) {
            opts->root_mode = 0; opts->window_mode = 1; opts->have_window_id = 0;
        } else if (strcmp(arg, "-window-id") == 0 || strcmp(arg, "--window-id") == 0) {
            if (!parse_window_id(value, &opts->window_id)) { fprintf(stderr, "invalid window ID: %s\n", value); return 0; }
            opts->root_mode = 0; opts->window_mode = 0; opts->have_window_id = 1;
        } else if (strcmp(arg, "-geometry") == 0 || strcmp(arg, "--geometry") == 0) {
            if (!parse_geometry(value, &opts->width, &opts->height)) { fprintf(stderr, "invalid geometry: %s\n", value); return 0; }
        } else if (strcmp(arg, "-fps") == 0 || strcmp(arg, "--fps") == 0 || strcmp(arg, "-maxfps") == 0 || strcmp(arg, "--maxfps") == 0) {
            if (!parse_long(value, 5L, 120L, &number)) return 0;
            opts->fps = (int)number; opts->delay_usec = (unsigned int)(1000000L / number);
        } else if (strcmp(arg, "-delay") == 0 || strcmp(arg, "--delay") == 0) {
            if (!parse_long(value, 1000L, 200000L, &number)) return 0;
            opts->delay_usec = (unsigned int)number; opts->fps = (int)(1000000L / number);
        } else if (strcmp(arg, "-columns") == 0 || strcmp(arg, "--columns") == 0) {
            if (!parse_long(value, 40L, 240L, &number)) return 0;
            opts->columns = (unsigned int)number;
        } else if (strcmp(arg, "-cell-size") == 0 || strcmp(arg, "--cell-size") == 0) {
            if (!parse_long(value, 8L, 40L, &number)) return 0;
            /* Compatibility knob: 18px maps to the 80-column reference grid
             * in a 1440x1080 4:3 aperture. */
            opts->columns = (unsigned int)(1440L / number);
            if (opts->columns < 40U) opts->columns = 40U;
            if (opts->columns > 240U) opts->columns = 240U;
        } else if (strcmp(arg, "-density") == 0 || strcmp(arg, "--density") == 0) {
            if (!parse_long(value, 10L, 90L, &number)) return 0;
            opts->density = (int)number;
        } else if (strcmp(arg, "-speed") == 0 || strcmp(arg, "--speed") == 0) {
            if (!parse_long(value, 25L, 250L, &number)) return 0;
            opts->speed = (int)number;
        } else if (strcmp(arg, "-trail") == 0 || strcmp(arg, "--trail") == 0) {
            if (!parse_long(value, 6L, 40L, &number)) return 0;
            opts->trail = (int)number;
        } else if (strcmp(arg, "-cycle") == 0 || strcmp(arg, "--cycle") == 0) {
            if (!parse_long(value, 0L, 300L, &number)) return 0;
            opts->cycle = (int)number;
        } else if (strcmp(arg, "-glow") == 0 || strcmp(arg, "--glow") == 0) {
            if (!parse_long(value, 0L, 100L, &number)) return 0;
            opts->glow = (int)number;
        } else if (strcmp(arg, "-contrast") == 0 || strcmp(arg, "--contrast") == 0) {
            if (!parse_long(value, 20L, 100L, &number)) return 0;
            opts->contrast = (int)number;
        } else if (strcmp(arg, "-aspect") == 0 || strcmp(arg, "--aspect") == 0) {
            if (strcmp(value, "auto") != 0 && strcmp(value, "4:3") != 0 && strcmp(value, "4/3") != 0 &&
                strcmp(value, "16:9") != 0 && strcmp(value, "16/9") != 0 && strcmp(value, "2.39:1") != 0 && strcmp(value, "cinema") != 0) return 0;
            opts->aspect = parse_aspect(value);
        } else if (strcmp(arg, "-curvature") == 0 || strcmp(arg, "--curvature") == 0) {
            if (!parse_long(value, 0L, 100L, &number)) return 0;
            opts->curvature = (int)number;
        } else if (strcmp(arg, "-scanlines") == 0 || strcmp(arg, "--scanlines") == 0) {
            if (!parse_long(value, 0L, 100L, &number)) return 0;
            opts->scanlines = (int)number;
        } else if (strcmp(arg, "-phosphor-mask") == 0 || strcmp(arg, "--phosphor-mask") == 0) {
            if (!parse_long(value, 0L, 100L, &number)) return 0;
            opts->phosphor_mask = (int)number;
        } else if (strcmp(arg, "-vignette") == 0 || strcmp(arg, "--vignette") == 0) {
            if (!parse_long(value, 0L, 100L, &number)) return 0;
            opts->vignette = (int)number;
        } else if (strcmp(arg, "-persistence") == 0 || strcmp(arg, "--persistence") == 0) {
            if (!parse_long(value, 0L, 100L, &number)) return 0;
            opts->persistence = (int)number;
        } else if (strcmp(arg, "-overscan") == 0 || strcmp(arg, "--overscan") == 0) {
            if (!parse_long(value, 0L, 10L, &number)) return 0;
            opts->overscan = (int)number;
        } else if (strcmp(arg, "-crt") == 0 || strcmp(arg, "--crt") == 0) {
            opts->crt = 1;
        } else if (strcmp(arg, "-no-crt") == 0 || strcmp(arg, "--no-crt") == 0) {
            opts->crt = 0;
        } else if (strcmp(arg, "-no-vsync") == 0 || strcmp(arg, "--no-vsync") == 0) {
            opts->no_vsync = 1;
        } else if (strcmp(arg, "-vsync") == 0 || strcmp(arg, "--vsync") == 0) {
            opts->no_vsync = 0;
        } else if (strcmp(arg, "-seed") == 0 || strcmp(arg, "--seed") == 0) {
            if (!parse_u64(value, &opts->seed)) return 0;
            opts->seed_set = 1;
        } else if (strcmp(arg, "-frames") == 0 || strcmp(arg, "--frames") == 0) {
            if (!parse_long(value, 1L, 10000000L, &number)) return 0;
            opts->frames = (unsigned long)number;
        } else if (strcmp(arg, "-screenshot") == 0 || strcmp(arg, "--screenshot") == 0) {
            opts->screenshot_path = value;
        } else if (strcmp(arg, "-verbose") == 0 || strcmp(arg, "--verbose") == 0) {
            opts->verbose = 1;
        } else if (strcmp(arg, "-self-test") == 0 || strcmp(arg, "--self-test") == 0) {
            opts->self_test = 1;
        } else if (strcmp(arg, "-version") == 0 || strcmp(arg, "--version") == 0) {
            printf("matrixcode %s\n", MATRIXCODE_VERSION); exit(EXIT_SUCCESS);
        } else if (strcmp(arg, "-help") == 0 || strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            print_usage(stdout, argv[0]); exit(EXIT_SUCCESS);
        } else {
            fprintf(stderr, "unknown option: %s\n", arg); return 0;
        }
    }
    if (opts->scene_time_set) {
        double duration;
        if (opts->startup_scene == MATRIX_SCENE_NONE) {
            fprintf(stderr, "-scene-time requires an enabled startup scene\n");
            return 0;
        }
        duration = matrix_scene_duration(opts->startup_scene);
        if (opts->scene_time > duration) {
            fprintf(stderr, "scene time %.3f exceeds %s duration %.3f\n",
                    opts->scene_time, matrix_scene_name(opts->startup_scene), duration);
            return 0;
        }
    }
    return 1;
}

static content_rect choose_content_rect(const options *opts, int width, int height)
{
    content_rect r;
    float target = 0.0F;
    float w = (float)width, h = (float)height;
    if (opts->aspect == ASPECT_4_3) target = 4.0F / 3.0F;
    else if (opts->aspect == ASPECT_16_9) target = 16.0F / 9.0F;
    else if (opts->aspect == ASPECT_CINEMA) target = 2.39F;
    if (target > 0.0F) {
        if (w / h > target) w = h * target; else h = w / target;
    }
    if (opts->crt && opts->overscan > 0) {
        float factor = 1.0F - (float)opts->overscan / 100.0F;
        w *= factor; h *= factor;
    }
    r.x = ((float)width - w) * 0.5F;
    r.y = ((float)height - h) * 0.5F;
    r.width = w; r.height = h;
    return r;
}

static unsigned int choose_glyph(simulation *sim)
{
    unsigned int base_count = (unsigned int)matrixcode_base_glyph_count();
    unsigned int roll = rng_bounded(&sim->rng, 100U);
    unsigned int base;
    if (roll < 74U) base = rng_bounded(&sim->rng, MATRIXCODE_KANA_COUNT);
    else if (roll < 89U) base = MATRIXCODE_DIGIT_FIRST + rng_bounded(&sim->rng, MATRIXCODE_DIGIT_COUNT);
    else base = MATRIXCODE_SYMBOL_FIRST + rng_bounded(&sim->rng, base_count - MATRIXCODE_SYMBOL_FIRST);
    /* Mirroring is common in the film language, but not every symbol is mirrored. */
    if (rng_bounded(&sim->rng, 100U) < 36U) base += base_count;
    return base;
}

static void simulation_free(simulation *sim)
{
    free(sim->cells); free(sim->column); memset(sim, 0, sizeof(*sim));
}

static int simulation_resize(simulation *sim, const options *opts, int width, int height)
{
    uint64_t saved = sim->rng.state;
    content_rect r = choose_content_rect(opts, width, height);
    /* `opts->columns` is the 4:3 reference density.  Derive a reference row
     * count from it, then size square logical cells from the drawable height.
     * Wider windows gain columns instead of stretching the code or leaving
     * pillarboxes; larger monitors keep the same logical density and simply
     * render larger glyphs. */
    unsigned int reference_rows = (unsigned int)lroundf(
        (float)opts->columns * CRT_REFERENCE_HEIGHT / CRT_REFERENCE_WIDTH);
    float target_cell;
    unsigned int columns;
    unsigned int rows;
    if (reference_rows < 24U) reference_rows = 24U;
    target_cell = r.height / (float)reference_rows;
    if (target_cell < 1.0F) target_cell = 1.0F;
    columns = (unsigned int)lroundf(r.width / target_cell);
    rows = reference_rows;
    if (columns < 24U) columns = 24U;
    if (columns > 1024U) columns = 1024U;
    size_t count;
    unsigned int x, y;
    simulation_free(sim);
    sim->rng.state = saved != 0U ? saved : UINT64_C(0x6A09E667F3BCC909);
    sim->columns = columns; sim->rows = rows; sim->content = r;
    sim->cell_width = r.width / (float)columns;
    sim->cell_height = r.height / (float)rows;
    count = (size_t)columns * rows;
    sim->cells = calloc(count, sizeof(*sim->cells));
    sim->column = calloc(columns, sizeof(*sim->column));
    if (!sim->cells || !sim->column) { simulation_free(sim); return 0; }
    for (y = 0U; y < rows; y++) for (x = 0U; x < columns; x++) {
        cell *c = &sim->cells[(size_t)y * columns + x];
        c->glyph = (uint16_t)choose_glyph(sim);
        c->occupied = (uint8_t)(rng_bounded(&sim->rng, 100U) < 97U);
        c->age = rng_float(&sim->rng);
        c->cycle_rate = 0.55F + rng_float(&sim->rng) * 0.90F;
    }
    for (x = 0U; x < columns; x++) {
        sim->column[x].time_offset = rng_float(&sim->rng) * 1000.0F;
        sim->column[x].speed_scale = 0.50F + rng_float(&sim->rng) * 0.50F;
        sim->column[x].brightness_scale = 0.88F + rng_float(&sim->rng) * 0.18F;
    }
    return 1;
}

static float fract_positive(float x) { return x - floorf(x); }
static float wobble(float x) { return x + 0.3F * sinf(SQRT2_F * x) + 0.2F * sinf(SQRT5_F * x); }

static float rain_raw(const simulation *sim, const options *opts, unsigned int x, int y, double time_value)
{
    const rain_column *col = &sim->column[x];
    float fall_speed = 0.60F * ((float)opts->speed / 100.0F) * col->speed_scale;
    float rain_length = fmaxf(0.45F, (float)opts->trail / 12.0F);
    float column_time = col->time_offset + (float)time_value * fall_speed;
    float phase = ((float)(-y) * 0.01F + column_time) / rain_length;
    phase = wobble(phase);
    return 1.0F - fract_positive(phase);
}

static int is_cursor_cell(const simulation *sim, const options *opts, unsigned int x, unsigned int y, double t)
{
    float here = rain_raw(sim, opts, x, (int)y, t);
    float below = rain_raw(sim, opts, x, (int)y + 1, t);
    return here > 0.72F && here > below + 0.45F;
}

static void simulation_update(simulation *sim, const options *opts, double delta)
{
    size_t i, count = (size_t)sim->columns * sim->rows;
    float global_rate = 0.60F * (float)opts->cycle / 100.0F;
    if (global_rate <= 0.0F) return;
    for (i = 0U; i < count; i++) {
        cell *c = &sim->cells[i];
        c->age += (float)delta * global_rate * c->cycle_rate;
        if (c->age >= 1.0F) {
            c->age = fract_positive(c->age);
            c->glyph = (uint16_t)choose_glyph(sim);
            if (rng_bounded(&sim->rng, 100U) < 4U) c->occupied = (uint8_t)!c->occupied;
        }
    }
}

static void setup_projection(int width, int height)
{
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glOrtho(0.0, (double)width, (double)height, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
}

static int upload_texture(GLuint *texture, const matrixcode_atlas *atlas, const uint8_t *pixels, GLint filter)
{
    glGenTextures(1, texture);
    if (*texture == 0U) return 0;
    glBindTexture(GL_TEXTURE_2D, *texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)atlas->width, (GLsizei)atlas->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    return 1;
}

static int gl_resources_init(gl_resources *res)
{
    memset(res, 0, sizeof(*res));
    if (!matrixcode_build_atlas(&res->atlas)) return 0;
    if (!upload_texture(&res->core_texture, &res->atlas, res->atlas.core_rgba, GL_NEAREST) ||
        !upload_texture(&res->glow_texture, &res->atlas, res->atlas.glow_rgba, GL_LINEAR)) return 0;
    return 1;
}

static void gl_resources_free(gl_resources *res)
{
    if (res->core_texture) glDeleteTextures(1, &res->core_texture);
    if (res->glow_texture) glDeleteTextures(1, &res->glow_texture);
    matrixcode_free_atlas(&res->atlas); memset(res, 0, sizeof(*res));
}

static void atlas_coords(const matrixcode_atlas *atlas, unsigned int glyph, float *u0, float *v0, float *u1, float *v1)
{
    const float padding = 7.0F;
    unsigned int col = glyph % atlas->columns, row = glyph / atlas->columns;
    float left = (float)col * (float)atlas->cell_width + padding;
    float top = (float)row * (float)atlas->cell_height + padding;
    float right = left + (float)MATRIXCODE_GLYPH_WIDTH * 4.0F;
    float bottom = top + (float)MATRIXCODE_GLYPH_HEIGHT * 4.0F;
    *u0 = left / (float)atlas->width; *v0 = top / (float)atlas->height;
    *u1 = right / (float)atlas->width; *v1 = bottom / (float)atlas->height;
}

static float smoothstep_local(float a, float b, float x)
{
    float t;
    if (a == b) return x < a ? 0.0F : 1.0F;
    t = (x - a) / (b - a); if (t < 0.0F) t = 0.0F; if (t > 1.0F) t = 1.0F;
    return t * t * (3.0F - 2.0F * t);
}

static float crt_virtual_width(const content_rect *r)
{
    if (r->height <= 0.0F) return CRT_REFERENCE_WIDTH;
    return CRT_REFERENCE_HEIGHT * (r->width / r->height);
}

static void warp_point(const options *opts, const content_rect *r, float x, float y, float *out_x, float *out_y)
{
    float nx;
    float ny;
    if (opts->crt) {
        /* Quantize onto a 640x480 virtual raster before applying tube curvature.
         * This is what makes a 1440p/4K panel read like photographed late-90s video
         * instead of perfectly resolution-independent vector graphics. */
        float virtual_width = crt_virtual_width(r);
        float lx = (x - r->x) / r->width * virtual_width;
        float ly = (y - r->y) / r->height * CRT_REFERENCE_HEIGHT;
        lx = floorf(lx + 0.5F);
        ly = floorf(ly + 0.5F);
        x = r->x + lx / virtual_width * r->width;
        y = r->y + ly / CRT_REFERENCE_HEIGHT * r->height;
    }
    nx = ((x - r->x) / r->width) * 2.0F - 1.0F;
    ny = ((y - r->y) / r->height) * 2.0F - 1.0F;
    float k = opts->crt ? (float)opts->curvature / 100.0F * 0.085F : 0.0F;
    float r2 = nx * nx + ny * ny;
    float scale = 1.0F + k * r2;
    nx *= scale; ny *= scale;
    *out_x = r->x + (nx + 1.0F) * 0.5F * r->width;
    *out_y = r->y + (ny + 1.0F) * 0.5F * r->height;
}

static int rounded_corner_visible(const options *opts, float nx, float ny)
{
    float ax = fabsf(nx), ay = fabsf(ny);
    if (!opts->crt) return 1;
    if (ax <= 0.93F || ay <= 0.93F) return 1;
    {
        float dx = (ax - 0.93F) / 0.07F;
        float dy = (ay - 0.93F) / 0.07F;
        return dx * dx + dy * dy <= 1.0F;
    }
}

static float vignette_factor(const options *opts, float nx, float ny)
{
    float radial, edge, amount;
    if (opts->vignette <= 0) return 1.0F;
    radial = sqrtf(nx * nx + ny * ny) * 0.70710678F;
    edge = smoothstep_local(0.58F, 1.0F, radial);
    amount = (float)opts->vignette / 100.0F * 0.70F;
    return 1.0F - edge * edge * amount;
}

static void emit_warped_quad(const app *a, unsigned int glyph, float x, float y, float w, float h,
                             float red, float green, float blue, float alpha)
{
    float u0, v0, u1, v1;
    float x0, y0, x1, y1, x2, y2, x3, y3;
    atlas_coords(&a->gl.atlas, glyph, &u0, &v0, &u1, &v1);
    warp_point(&a->opts, &a->sim.content, x, y, &x0, &y0);
    warp_point(&a->opts, &a->sim.content, x + w, y, &x1, &y1);
    warp_point(&a->opts, &a->sim.content, x + w, y + h, &x2, &y2);
    warp_point(&a->opts, &a->sim.content, x, y + h, &x3, &y3);
    glColor4f(red, green, blue, alpha);
    glTexCoord2f(u0, v0); glVertex2f(x0, y0);
    glTexCoord2f(u1, v0); glVertex2f(x1, y1);
    glTexCoord2f(u1, v1); glVertex2f(x2, y2);
    glTexCoord2f(u0, v1); glVertex2f(x3, y3);
}

static void render_rain_layer(const app *a, double t, float opacity, int glow_pass, float expansion)
{
    const simulation *sim = &a->sim;
    const options *opts = &a->opts;
    unsigned int x, y;
    float threshold = 1.0F - (float)opts->density / 100.0F;
    float trail_alpha = 0.205F + (float)opts->contrast / 100.0F * 0.145F;
    glBegin(GL_QUADS);
    for (y = 0U; y < sim->rows; y++) for (x = 0U; x < sim->columns; x++) {
        const cell *c = &sim->cells[(size_t)y * sim->columns + x];
        float raw, px, py, draw_w, draw_h, nx, ny, vig, alpha;
        int cursor;
        if (!c->occupied) continue;
        raw = rain_raw(sim, opts, x, (int)y, t);
        cursor = is_cursor_cell(sim, opts, x, y, t);
        if (!cursor && raw <= threshold) continue;
        px = sim->content.x + (float)x * sim->cell_width;
        py = sim->content.y + (float)y * sim->cell_height;
        nx = ((px + sim->cell_width * 0.5F - sim->content.x) / sim->content.width) * 2.0F - 1.0F;
        ny = ((py + sim->cell_height * 0.5F - sim->content.y) / sim->content.height) * 2.0F - 1.0F;
        if (!rounded_corner_visible(opts, nx, ny)) continue;
        vig = vignette_factor(opts, nx, ny) * sim->column[x].brightness_scale;
        /* Matrix operator glyphs read conspicuously tall and narrow.  The
         * original clean-room bitmaps are 8x12, but mapping them into a nearly
         * square quad made the old version look like generic terminal text. */
        draw_h = sim->cell_height * 0.98F;
        draw_w = fminf(sim->cell_width * 0.76F, draw_h / 1.35F);
        px += (sim->cell_width - draw_w) * 0.5F;
        py += (sim->cell_height - draw_h) * 0.5F;
        if (glow_pass) {
            float grow = expansion * sim->cell_height;
            px -= grow * 0.5F; py -= grow * 0.5F; draw_w += grow; draw_h += grow;
            if (cursor) {
                alpha = 0.36F * opacity * ((float)opts->glow / 100.0F) * vig;
                emit_warped_quad(a, c->glyph, px, py, draw_w, draw_h, 0.22F, 1.00F, 0.45F, alpha);
            } else {
                alpha = trail_alpha * 0.52F * opacity * ((float)opts->glow / 100.0F) * vig;
                emit_warped_quad(a, c->glyph, px, py, draw_w, draw_h, 0.04F, 0.88F, 0.25F, alpha);
            }
        } else if (cursor) {
            alpha = 0.98F * opacity * vig;
            emit_warped_quad(a, c->glyph, px, py, draw_w, draw_h, 0.62F, 1.00F, 0.76F, alpha);
        } else {
            alpha = trail_alpha * opacity * vig;
            emit_warped_quad(a, c->glyph, px, py, draw_w, draw_h, 0.12F, 1.00F, 0.46F, alpha);
        }
    }
    glEnd();
}

static void render_crt_overlay(const app *a, float opacity)
{
    const content_rect *r = &a->sim.content;
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (a->opts.scanlines > 0) {
        float alpha = (float)a->opts.scanlines / 100.0F * 0.20F * opacity;
        float spacing = fmaxf(2.0F, r->height / CRT_REFERENCE_HEIGHT * 2.0F);
        float fy;
        glLineWidth(1.0F); glBegin(GL_LINES);
        for (fy = r->y + spacing * 0.5F; fy <= r->y + r->height; fy += spacing) {
            glColor4f(0.0F, 0.0F, 0.0F, alpha);
            glVertex2f(r->x, fy); glVertex2f(r->x + r->width, fy);
        }
        glEnd();
    }
    if (a->opts.phosphor_mask > 0) {
        float alpha = (float)a->opts.phosphor_mask / 100.0F * 0.13F * opacity;
        float spacing = fmaxf(2.0F, r->width / crt_virtual_width(r) * 2.0F);
        float fx;
        glBegin(GL_LINES);
        for (fx = r->x + spacing * 0.5F; fx <= r->x + r->width; fx += spacing) {
            glColor4f(0.0F, 0.0F, 0.0F, alpha);
            glVertex2f(fx, r->y); glVertex2f(fx, r->y + r->height);
        }
        glEnd();
    }
    glEnable(GL_TEXTURE_2D);
}

static void render_frame(app *a, double t, float opacity, int clear_frame)
{
    if (clear_frame) {
        glClearColor(0.0F, 0.0010F, 0.0002F, 1.0F); glClear(GL_COLOR_BUFFER_BIT);
    }
    glLoadIdentity();
    glEnable(GL_TEXTURE_2D); glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_LIGHTING);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    if (a->opts.crt && a->opts.persistence > 0) {
        float p = (float)a->opts.persistence / 100.0F * 0.36F;
        glBindTexture(GL_TEXTURE_2D, a->gl.glow_texture);
        render_rain_layer(a, t - 1.0 / 60.0, p * opacity, 1, 0.58F);
        glBindTexture(GL_TEXTURE_2D, a->gl.core_texture);
        render_rain_layer(a, t - 1.0 / 60.0, p * 0.45F * opacity, 0, 0.0F);
    }
    if (a->opts.glow > 0) {
        glBindTexture(GL_TEXTURE_2D, a->gl.glow_texture);
        render_rain_layer(a, t, 0.38F * opacity, 1, 0.78F);
        render_rain_layer(a, t, 0.70F * opacity, 1, 0.34F);
    }
    glBindTexture(GL_TEXTURE_2D, a->gl.core_texture);
    render_rain_layer(a, t, opacity, 0, 0.0F);
    if (a->opts.crt) render_crt_overlay(a, opacity);
    glFlush();
}

static int write_ppm(const app *a, const char *path)
{
    FILE *file;
    uint8_t *pixels, *row;
    size_t row_size;
    int y;
    if (!path) return 1;
    row_size = (size_t)a->width * 4U;
    pixels = malloc(row_size * (size_t)a->height);
    row = malloc((size_t)a->width * 3U);
    if (!pixels || !row) { free(pixels); free(row); return 0; }
    glFinish(); glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(a->double_buffer ? GL_BACK : GL_FRONT);
    glReadPixels(0, 0, a->width, a->height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    file = fopen(path, "wb");
    if (!file) { free(pixels); free(row); return 0; }
    fprintf(file, "P6\n%d %d\n255\n", a->width, a->height);
    for (y = a->height - 1; y >= 0; y--) {
        int x; const uint8_t *src = pixels + (size_t)y * row_size;
        for (x = 0; x < a->width; x++) {
            row[(size_t)x * 3U] = src[(size_t)x * 4U];
            row[(size_t)x * 3U + 1U] = src[(size_t)x * 4U + 1U];
            row[(size_t)x * 3U + 2U] = src[(size_t)x * 4U + 2U];
        }
        if (fwrite(row, 1U, (size_t)a->width * 3U, file) != (size_t)a->width * 3U) { fclose(file); free(pixels); free(row); return 0; }
    }
    if (fclose(file) != 0) { free(pixels); free(row); return 0; }
    free(pixels); free(row); return 1;
}

static XVisualInfo *visual_info_for_window(Display *display, int screen, Window window)
{
    XWindowAttributes attrs;
    XVisualInfo templ;
    XVisualInfo *result;
    int count = 0, use_gl = 0;
    if (!XGetWindowAttributes(display, window, &attrs)) return NULL;
    memset(&templ, 0, sizeof(templ));
    templ.visualid = XVisualIDFromVisual(attrs.visual); templ.screen = screen;
    result = XGetVisualInfo(display, VisualIDMask | VisualScreenMask, &templ, &count);
    if (!result || count < 1) return result;
    if (glXGetConfig(display, result, GLX_USE_GL, &use_gl) != 0 || !use_gl) { XFree(result); return NULL; }
    return result;
}

static int create_owned_window(app *a)
{
    int attrs[] = { GLX_RGBA, GLX_DOUBLEBUFFER, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8, None };
    XSetWindowAttributes swa;
    a->visual_info = glXChooseVisual(a->display, a->screen, attrs);
    if (!a->visual_info) {
        int fallback[] = { GLX_RGBA, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8, None };
        a->visual_info = glXChooseVisual(a->display, a->screen, fallback);
    }
    if (!a->visual_info) return 0;
    a->colormap = XCreateColormap(a->display, RootWindow(a->display, a->screen), a->visual_info->visual, AllocNone);
    memset(&swa, 0, sizeof(swa)); swa.colormap = a->colormap; swa.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask | ButtonPressMask;
    a->owned_window = XCreateWindow(a->display, RootWindow(a->display, a->screen), 0, 0,
                                    (unsigned int)a->opts.width, (unsigned int)a->opts.height, 0,
                                    a->visual_info->depth, InputOutput, a->visual_info->visual,
                                    CWColormap | CWEventMask, &swa);
    if (a->owned_window == None) return 0;
    a->window = a->owned_window; a->width = a->opts.width; a->height = a->opts.height;
    XStoreName(a->display, a->window, "MatrixCode 1999 film study");
    a->wm_delete = XInternAtom(a->display, "WM_DELETE_WINDOW", False);
    (void)XSetWMProtocols(a->display, a->window, &a->wm_delete, 1);
    XMapWindow(a->display, a->window); XSync(a->display, False);
    return 1;
}

static int select_target_window(app *a)
{
    const char *env_window = getenv("XSCREENSAVER_WINDOW");
    Window target = None;
    XWindowAttributes attrs;
    int db = 0;
    if (!a->opts.root_mode && !a->opts.have_window_id && env_window && *env_window) {
        if (!parse_window_id(env_window, &target)) return 0;
        a->opts.window_mode = 0; a->opts.have_window_id = 1; a->opts.window_id = target;
    }
    if (a->opts.window_mode) return create_owned_window(a);
    if (a->opts.root_mode) target = RootWindow(a->display, a->screen); else if (a->opts.have_window_id) target = a->opts.window_id;
    if (target == None || !XGetWindowAttributes(a->display, target, &attrs)) return 0;
    a->window = target; a->width = attrs.width; a->height = attrs.height;
    a->visual_info = visual_info_for_window(a->display, a->screen, target);
    if (!a->visual_info) { fprintf(stderr, "target window visual is not GLX-capable\n"); return 0; }
    if (glXGetConfig(a->display, a->visual_info, GLX_DOUBLEBUFFER, &db) == 0) a->double_buffer = db != 0;
    XSelectInput(a->display, a->window, ExposureMask | StructureNotifyMask);
    return 1;
}

static uint64_t mix_seed(uint64_t value)
{
    value += UINT64_C(0x9e3779b97f4a7c15);
    value = (value ^ (value >> 30U)) * UINT64_C(0xbf58476d1ce4e5b9);
    value = (value ^ (value >> 27U)) * UINT64_C(0x94d049bb133111eb);
    value ^= value >> 31U;
    return value != 0U ? value : UINT64_C(0x6A09E667F3BCC909);
}

static uint64_t resize_seed(const app *a, int width, int height, unsigned long serial)
{
    uint64_t geometry = ((uint64_t)(unsigned int)width << 32U) | (uint64_t)(unsigned int)height;
    return mix_seed(a->base_seed ^ geometry ^ ((uint64_t)serial * UINT64_C(0xd1342543de82ef95)));
}

static void request_swap_interval(app *a)
{
    const char *ext;
    if (a->opts.no_vsync) return;
    ext = glXQueryExtensionsString(a->display, a->screen);
    if (ext && strstr(ext, "GLX_EXT_swap_control")) {
        swap_interval_ext_proc proc = (swap_interval_ext_proc)glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalEXT");
        if (proc) { proc(a->display, a->window, 1); return; }
    }
    if (ext && strstr(ext, "GLX_SGI_swap_control")) {
        swap_interval_sgi_proc proc = (swap_interval_sgi_proc)glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalSGI");
        if (proc) (void)proc(1);
    }
}

static int app_init(app *a, const options *opts)
{
    XWindowAttributes attrs;
    memset(a, 0, sizeof(*a)); a->opts = *opts;
    a->display = XOpenDisplay(NULL); if (!a->display) { fprintf(stderr, "could not open X display\n"); return 0; }
    a->screen = DefaultScreen(a->display);
    a->base_seed = opts->seed_set ? opts->seed :
        (((uint64_t)time(NULL) << 32U) ^ (uint64_t)getpid() ^ UINT64_C(0x6d6174726978636f));
    a->base_seed = mix_seed(a->base_seed);
    a->sim.rng.state = a->base_seed;
    if (!select_target_window(a)) return 0;
    if (!XGetWindowAttributes(a->display, a->window, &attrs)) return 0;
    a->width = attrs.width; a->height = attrs.height;
    if (a->owned_window != None) {
        int db = 0;
        if (glXGetConfig(a->display, a->visual_info, GLX_DOUBLEBUFFER, &db) == 0) a->double_buffer = db != 0;
    }
    a->context = glXCreateContext(a->display, a->visual_info, NULL, True); if (!a->context) return 0;
    if (!glXMakeCurrent(a->display, a->window, a->context)) return 0;
    request_swap_interval(a); setup_projection(a->width, a->height);
    if (!gl_resources_init(&a->gl)) return 0;
    if (!simulation_resize(&a->sim, &a->opts, a->width, a->height)) return 0;
    if (a->opts.verbose) {
        const GLubyte *vendor = glGetString(GL_VENDOR), *renderer = glGetString(GL_RENDERER), *version = glGetString(GL_VERSION);
        if (a->opts.scene_time_set)
            fprintf(stderr, "scene: %s, start time: %.3fs\n", matrix_scene_name(a->opts.startup_scene), a->opts.scene_time);
        else
            fprintf(stderr, "scene: %s\n", matrix_scene_name(a->opts.startup_scene));
        fprintf(stderr, "profile: %s, grid: %ux%u, cell %.2fx%.2f, content %.0fx%.0f\n",
                profile_name(a->opts.profile), a->sim.columns, a->sim.rows, a->sim.cell_width, a->sim.cell_height,
                a->sim.content.width, a->sim.content.height);
        fprintf(stderr, "GL vendor: %s\nGL renderer: %s\nGL version: %s\n",
                vendor ? (const char *)vendor : "unknown", renderer ? (const char *)renderer : "unknown", version ? (const char *)version : "unknown");
    }
    return 1;
}

static void app_free(app *a)
{
    simulation_free(&a->sim);
    if (a->display && a->context) {
        gl_resources_free(&a->gl); glXMakeCurrent(a->display, None, NULL); glXDestroyContext(a->display, a->context);
    }
    if (a->display && a->owned_window != None) XDestroyWindow(a->display, a->owned_window);
    if (a->display && a->colormap != None) XFreeColormap(a->display, a->colormap);
    if (a->visual_info) XFree(a->visual_info);
    if (a->display) XCloseDisplay(a->display);
    memset(a, 0, sizeof(*a));
}

static int restart_for_resize(app *a, int width, int height)
{
    if (width <= 0 || height <= 0) return 1;
    if (width == a->width && height == a->height) return 1;
    a->width = width;
    a->height = height;
    a->resize_serial++;
    a->sim.rng.state = resize_seed(a, width, height, a->resize_serial);
    setup_projection(width, height);
    if (!simulation_resize(&a->sim, &a->opts, width, height)) return 0;
    if (a->opts.verbose) {
        fprintf(stderr,
                "resize restart %lu: %dx%d, grid %ux%u, cell %.2fx%.2f, content %.0fx%.0f\n",
                a->resize_serial, width, height, a->sim.columns, a->sim.rows,
                a->sim.cell_width, a->sim.cell_height,
                a->sim.content.width, a->sim.content.height);
    }
    return 1;
}

static void schedule_resize(app *a, int width, int height)
{
    if (width <= 0 || height <= 0) return;
    if (!a->resize_pending && width == a->width && height == a->height) return;
    if (a->resize_pending && width == a->pending_width && height == a->pending_height) return;
    a->resize_pending = 1;
    a->pending_width = width;
    a->pending_height = height;
    a->resize_deadline = monotonic_seconds() + RESIZE_SETTLE_SECONDS;
}

static int sync_window_geometry(app *a)
{
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(a->display, a->window, &attrs)) return 0;
    if (attrs.width != a->width || attrs.height != a->height)
        schedule_resize(a, attrs.width, attrs.height);
    return 1;
}

static int process_events(app *a)
{
    while (XPending(a->display) > 0) {
        XEvent ev;
        XNextEvent(a->display, &ev);
        if (ev.type == ConfigureNotify && ev.xconfigure.window == a->window) {
            schedule_resize(a, ev.xconfigure.width, ev.xconfigure.height);
        } else if (ev.type == ClientMessage && a->owned_window != None &&
                   (Atom)ev.xclient.data.l[0] == a->wm_delete) {
            return 0;
        } else if ((ev.type == KeyPress || ev.type == ButtonPress) && a->owned_window != None) {
            return 0;
        }
    }
    return 1;
}

static int apply_settled_resize(app *a, int *did_restart)
{
    *did_restart = 0;
    if (!a->resize_pending) return 1;
    if (monotonic_seconds() < a->resize_deadline) return 1;
    if (!restart_for_resize(a, a->pending_width, a->pending_height)) return 0;
    a->resize_pending = 0;
    *did_restart = 1;
    return 1;
}

static int run_app(app *a)
{
    double start = monotonic_seconds(), previous = start, next_frame = start;
    double frame_interval = (double)a->opts.delay_usec / 1000000.0;
    double scene_offset = a->opts.scene_time_set ? a->opts.scene_time : 0.0;
    int screenshot_written = 0;
    while (!stop_requested) {
        double now, elapsed, delta;
        int restarted = 0;
        if (!process_events(a)) break;
        /* ConfigureNotify is the normal path. Poll periodically as a fallback
         * for reparenting/embedding setups that resize the drawable without a
         * useful configure event reaching the hack. */
        if ((a->rendered_frames % 15UL) == 0UL) {
            if (!sync_window_geometry(a)) break;
        }
        if (!apply_settled_resize(a, &restarted)) break;
        now = monotonic_seconds();
        if (restarted) {
            start = now;
            previous = now;
            next_frame = now;
            a->rendered_frames = 0UL;
            screenshot_written = 0;
        }
        if (now < next_frame) { sleep_seconds(next_frame - now); now = monotonic_seconds(); }
        if (a->opts.seed_set) { elapsed = scene_offset + (double)a->rendered_frames * frame_interval; delta = frame_interval; }
        else { elapsed = scene_offset + now - start; delta = now - previous; if (delta < 0.0 || delta > 0.25) delta = frame_interval; }
        previous = now;
        if (a->opts.startup_scene != MATRIX_SCENE_NONE &&
            elapsed < matrix_scene_duration(a->opts.startup_scene)) {
            double rain_start = matrix_scene_rain_start(a->opts.startup_scene);
            float rain_opacity = matrix_scene_rain_opacity(a->opts.startup_scene, elapsed);
            (void)matrix_scene_render(a->opts.startup_scene, elapsed, a->width, a->height);
            if (rain_opacity > 0.0F) {
                simulation_update(&a->sim, &a->opts, delta);
                render_frame(a, elapsed - rain_start, rain_opacity, 0);
            }
        } else {
            double rain_elapsed = elapsed;
            if (a->opts.startup_scene != MATRIX_SCENE_NONE)
                rain_elapsed -= matrix_scene_rain_start(a->opts.startup_scene);
            simulation_update(&a->sim, &a->opts, delta);
            render_frame(a, rain_elapsed, 1.0F, 1);
        }
        a->rendered_frames++;
        if (a->opts.frames > 0UL && a->rendered_frames >= a->opts.frames && a->opts.screenshot_path)
            screenshot_written = write_ppm(a, a->opts.screenshot_path);
        if (a->double_buffer) glXSwapBuffers(a->display, a->window); else glFlush();
        if (a->opts.frames > 0UL && a->rendered_frames >= a->opts.frames)
            return a->opts.screenshot_path == NULL || screenshot_written;
        next_frame += frame_interval;
        if (next_frame < now - frame_interval) next_frame = now + frame_interval;
    }
    return 1;
}

static int simulation_self_test(void)
{
    options opts; simulation a, b; unsigned int i; int ok = 1;
    options_profile_defaults(&opts, PROFILE_OPERATOR_1999); memset(&a, 0, sizeof(a)); memset(&b, 0, sizeof(b));
    a.rng.state = UINT64_C(0x12345678abcdef01); b.rng.state = UINT64_C(0x12345678abcdef01);
    if (!simulation_resize(&a, &opts, 1920, 1080) || !simulation_resize(&b, &opts, 1920, 1080)) return 0;
    if (a.columns != 107U || a.rows != 60U || fabsf(a.content.width - 1920.0F * 0.98F) > 2.0F ||
        fabsf(a.cell_height - 17.64F) > 0.20F) {
        fprintf(stderr, "self-test: operator geometry unexpected: %ux%u %.1fx%.1f cell %.2f\n",
                a.columns, a.rows, a.content.width, a.content.height, a.cell_height); ok = 0;
    }
    {
        simulation large;
        memset(&large, 0, sizeof(large));
        large.rng.state = UINT64_C(0x12345678abcdef01);
        if (!simulation_resize(&large, &opts, 3840, 2160)) return 0;
        if (large.columns != 107U || large.rows != 60U ||
            large.cell_height < a.cell_height * 1.95F || large.cell_height > a.cell_height * 2.05F) {
            fprintf(stderr, "self-test: resolution scaling unexpected: 1080p %.2f, 4K %.2f\n",
                    a.cell_height, large.cell_height);
            ok = 0;
        }
        simulation_free(&large);
    }
    {
        simulation four_three;
        memset(&four_three, 0, sizeof(four_three));
        four_three.rng.state = UINT64_C(0x12345678abcdef01);
        opts.aspect = ASPECT_4_3;
        if (!simulation_resize(&four_three, &opts, 1920, 1080)) return 0;
        if (four_three.columns != 80U || four_three.rows != 60U) {
            fprintf(stderr, "self-test: explicit 4:3 reference grid unexpected: %ux%u\n",
                    four_three.columns, four_three.rows);
            ok = 0;
        }
        simulation_free(&four_three);
        opts.aspect = ASPECT_AUTO;
    }
    if (memcmp(a.cells, b.cells, (size_t)a.columns * a.rows * sizeof(*a.cells)) != 0 ||
        memcmp(a.column, b.column, (size_t)a.columns * sizeof(*a.column)) != 0) { fprintf(stderr, "self-test: seeded simulations differ\n"); ok = 0; }
    for (i = 0U; i < 120U; i++) simulation_update(&a, &opts, 1.0 / 30.0);
    for (i = 0U; i < a.columns * a.rows; i++) if (a.cells[i].glyph >= matrixcode_total_glyph_count()) { ok = 0; break; }
    {
        float cursor_count = 0.0F;
        for (i = 0U; i < a.columns; i++) {
            unsigned int y;
            for (y = 0U; y < a.rows; y++) if (is_cursor_cell(&a, &opts, i, y, 1.0)) cursor_count += 1.0F;
        }
        if (cursor_count < 8.0F || cursor_count > 90.0F) { fprintf(stderr, "self-test: implausible cursor population %.0f\n", cursor_count); ok = 0; }
    }
    simulation_free(&a); simulation_free(&b); return ok;
}

static int run_self_tests(void)
{
    if (!matrixcode_glyphs_self_test()) return 0;
    if (!matrix_scene_self_test()) return 0;
    if (!simulation_self_test()) return 0;
    printf("matrixcode: all self-tests passed\n"); return 1;
}

int main(int argc, char **argv)
{
    options opts; app application; int success;
    if (!parse_options(argc, argv, &opts)) { print_usage(stderr, argv[0]); return EXIT_FAILURE; }
    if (opts.self_test) return run_self_tests() ? EXIT_SUCCESS : EXIT_FAILURE;
    signal(SIGINT, signal_handler); signal(SIGTERM, signal_handler); signal(SIGHUP, signal_handler);
    if (!app_init(&application, &opts)) { app_free(&application); return EXIT_FAILURE; }
    success = run_app(&application); app_free(&application); return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
