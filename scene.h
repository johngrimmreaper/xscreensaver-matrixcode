#ifndef MATRIXCODE_SCENE_H
#define MATRIXCODE_SCENE_H

typedef enum matrix_scene_kind {
    MATRIX_SCENE_NONE = 0,
    MATRIX_SCENE_NEO_TERMINAL,
    MATRIX_SCENE_NEO_TRACE
} matrix_scene_kind;

typedef enum matrix_scene_result {
    MATRIX_SCENE_RUNNING = 0,
    MATRIX_SCENE_FINISHED
} matrix_scene_result;

matrix_scene_kind matrix_scene_from_name(const char *name);
const char *matrix_scene_name(matrix_scene_kind scene);
double matrix_scene_duration(matrix_scene_kind scene);
double matrix_scene_rain_start(matrix_scene_kind scene);
float matrix_scene_rain_opacity(matrix_scene_kind scene, double elapsed);
matrix_scene_result matrix_scene_render(matrix_scene_kind scene, double elapsed,
                                        int width, int height);
int matrix_scene_self_test(void);

#endif
