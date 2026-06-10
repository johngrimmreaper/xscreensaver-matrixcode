#ifndef MATRIXCODE_MINIGL_H
#define MATRIXCODE_MINIGL_H

/*
 * Minimal OpenGL 1.x / GLX declarations used only by the repository's
 * fallback build path when development headers are unavailable. Normal
 * Debian and Ubuntu builds use <GL/gl.h> and <GL/glx.h> from libgl-dev.
 */

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef void GLvoid;
typedef signed char GLbyte;
typedef short GLshort;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned int GLuint;
typedef float GLfloat;
typedef double GLdouble;
typedef char GLchar;

typedef XID GLXDrawable;
typedef struct __GLXcontextRec *GLXContext;
typedef void (*__GLXextFuncPtr)(void);

#define GL_FALSE                         0
#define GL_TRUE                          1
#define GL_TEXTURE_2D                    0x0DE1
#define GL_BLEND                         0x0BE2
#define GL_DEPTH_TEST                    0x0B71
#define GL_CULL_FACE                     0x0B44
#define GL_LIGHTING                      0x0B50
#define GL_DITHER                        0x0BD0
#define GL_SRC_ALPHA                     0x0302
#define GL_ONE                           1
#define GL_ONE_MINUS_SRC_ALPHA           0x0303
#define GL_COLOR_BUFFER_BIT              0x00004000
#define GL_PROJECTION                    0x1701
#define GL_MODELVIEW                     0x1700
#define GL_QUADS                         0x0007
#define GL_RGBA                          0x1908
#define GL_UNSIGNED_BYTE                 0x1401
#define GL_TEXTURE_MIN_FILTER            0x2801
#define GL_TEXTURE_MAG_FILTER            0x2800
#define GL_TEXTURE_WRAP_S                0x2802
#define GL_TEXTURE_WRAP_T                0x2803
#define GL_LINEAR                        0x2601
#define GL_CLAMP                         0x2900
#define GL_UNPACK_ALIGNMENT              0x0CF5
#define GL_PACK_ALIGNMENT                0x0D05
#define GL_FRONT                         0x0404
#define GL_BACK                          0x0405
#define GL_VENDOR                        0x1F00
#define GL_RENDERER                      0x1F01
#define GL_VERSION                       0x1F02

#define GLX_USE_GL                       1
#define GLX_BUFFER_SIZE                  2
#define GLX_LEVEL                        3
#define GLX_RGBA                         4
#define GLX_DOUBLEBUFFER                 5
#define GLX_STEREO                       6
#define GLX_AUX_BUFFERS                  7
#define GLX_RED_SIZE                     8
#define GLX_GREEN_SIZE                   9
#define GLX_BLUE_SIZE                    10
#define GLX_ALPHA_SIZE                   11
#define GLX_DEPTH_SIZE                   12
#define GLX_STENCIL_SIZE                 13
#define GLX_ACCUM_RED_SIZE               14
#define GLX_ACCUM_GREEN_SIZE             15
#define GLX_ACCUM_BLUE_SIZE              16
#define GLX_ACCUM_ALPHA_SIZE             17

extern XVisualInfo *glXChooseVisual(Display *, int, int *);
extern GLXContext glXCreateContext(Display *, XVisualInfo *, GLXContext, Bool);
extern Bool glXMakeCurrent(Display *, GLXDrawable, GLXContext);
extern void glXSwapBuffers(Display *, GLXDrawable);
extern void glXDestroyContext(Display *, GLXContext);
extern int glXGetConfig(Display *, XVisualInfo *, int, int *);
extern const char *glXQueryExtensionsString(Display *, int);
extern __GLXextFuncPtr glXGetProcAddressARB(const GLubyte *);

extern void glViewport(GLint, GLint, GLsizei, GLsizei);
extern void glMatrixMode(GLenum);
extern void glLoadIdentity(void);
extern void glOrtho(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble);
extern void glDisable(GLenum);
extern void glEnable(GLenum);
extern void glBlendFunc(GLenum, GLenum);
extern void glClearColor(GLfloat, GLfloat, GLfloat, GLfloat);
extern void glClear(GLbitfield);
extern void glGenTextures(GLsizei, GLuint *);
extern void glBindTexture(GLenum, GLuint);
extern void glTexParameteri(GLenum, GLenum, GLint);
extern void glPixelStorei(GLenum, GLint);
extern void glTexImage2D(GLenum, GLint, GLint, GLsizei, GLsizei, GLint,
                         GLenum, GLenum, const GLvoid *);
extern void glDeleteTextures(GLsizei, const GLuint *);
extern void glBegin(GLenum);
extern void glEnd(void);
extern void glColor4f(GLfloat, GLfloat, GLfloat, GLfloat);
extern void glTexCoord2f(GLfloat, GLfloat);
extern void glVertex2f(GLfloat, GLfloat);
extern void glFlush(void);
extern void glFinish(void);
extern void glReadBuffer(GLenum);
extern void glReadPixels(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, GLvoid *);
extern const GLubyte *glGetString(GLenum);

#ifdef __cplusplus
}
#endif

#endif
