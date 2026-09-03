/**
 * gl_loader.h
 *
 * OpenGL 3.3 core entry points, loaded at runtime.
 *
 * No GL headers are included anywhere in this project. The three desktop
 * platforms disagree about which header ships, what it declares and whether the
 * symbols link directly (macOS) or have to come from wglGetProcAddress
 * (Windows), so the types and enums this port needs are spelled out here and
 * every function goes through a pointer. That also keeps the GL surface
 * documented in one place: the list below IS the feature set.
 *
 * Guarded whole-file, because pocket/Makefile and psp/Makefile both glob every
 * .c under src/ and would otherwise compile this for targets with no GL.
 */

#ifndef GL_LOADER_H
#define GL_LOADER_H

#ifdef ECS_ENABLE_GL

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Windows puts the whole API behind __stdcall; everyone else uses the C ABI. */
#if defined(_WIN32)
#  define ECS_GLAPI __stdcall
#else
#  define ECS_GLAPI
#endif

/* ── Types ─────────────────────────────────────────────────── */

typedef unsigned int  GLenum;
typedef unsigned char GLboolean;
typedef unsigned int  GLbitfield;
typedef signed char   GLbyte;
typedef short         GLshort;
typedef int           GLint;
typedef int           GLsizei;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned int  GLuint;
typedef float         GLfloat;
typedef double        GLdouble;
typedef char          GLchar;
typedef void          GLvoid;
typedef ptrdiff_t     GLintptr;
typedef ptrdiff_t     GLsizeiptr;

/* ── Enums ─────────────────────────────────────────────────── */

#define GL_FALSE                          0
#define GL_TRUE                           1
#define GL_NO_ERROR                       0
#define GL_POINTS                         0x0000
#define GL_LINES                          0x0001
#define GL_TRIANGLES                      0x0004
#define GL_TRIANGLE_STRIP                 0x0005
#define GL_NEVER                          0x0200
#define GL_LESS                           0x0201
#define GL_EQUAL                          0x0202
#define GL_LEQUAL                         0x0203
#define GL_GREATER                        0x0204
#define GL_ALWAYS                         0x0207
#define GL_SRC_ALPHA                      0x0302
#define GL_ONE_MINUS_SRC_ALPHA            0x0303
#define GL_DST_COLOR                      0x0306
#define GL_ONE                             1
#define GL_ZERO                            0
#define GL_FRONT                          0x0404
#define GL_BACK                           0x0405
#define GL_CW                             0x0900
#define GL_CCW                            0x0901
#define GL_CULL_FACE                      0x0B44
#define GL_DEPTH_TEST                     0x0B71
#define GL_DEPTH_WRITEMASK                0x0B72
#define GL_BLEND                          0x0BE2
#define GL_SCISSOR_TEST                   0x0C11
#define GL_UNPACK_ALIGNMENT               0x0CF5
#define GL_PACK_ALIGNMENT                 0x0D05
#define GL_TEXTURE_2D                     0x0DE1
#define GL_BYTE                           0x1400
#define GL_UNSIGNED_BYTE                  0x1401
#define GL_SHORT                          0x1402
#define GL_UNSIGNED_SHORT                 0x1403
#define GL_INT                            0x1404
#define GL_UNSIGNED_INT                   0x1405
#define GL_FLOAT                          0x1406
#define GL_DEPTH_COMPONENT                0x1902
#define GL_RED                            0x1903
#define GL_RGB                            0x1907
#define GL_RGBA                           0x1908
#define GL_VENDOR                         0x1F00
#define GL_RENDERER                       0x1F01
#define GL_VERSION                        0x1F02
#define GL_NEAREST                        0x2600
#define GL_LINEAR                         0x2601
#define GL_TEXTURE_MAG_FILTER             0x2800
#define GL_TEXTURE_MIN_FILTER             0x2801
#define GL_TEXTURE_WRAP_S                 0x2802
#define GL_TEXTURE_WRAP_T                 0x2803
#define GL_REPEAT                         0x2901
#define GL_CLAMP_TO_EDGE                  0x812F
#define GL_TEXTURE_3D                     0x806F
#define GL_TEXTURE_WRAP_R                 0x8072
#define GL_RGBA8                          0x8058
#define GL_RGB8                           0x8051
#define GL_R8                             0x8229
#define GL_R8UI                           0x8232
#define GL_R16I                           0x8233
#define GL_R16UI                          0x8234
#define GL_R32F                           0x822E
#define GL_RED_INTEGER                    0x8D94
#define GL_DEPTH_COMPONENT24              0x81A6
#define GL_DEPTH_COMPONENT32F             0x8CAC
#define GL_ARRAY_BUFFER                   0x8892
#define GL_ELEMENT_ARRAY_BUFFER           0x8893
#define GL_STREAM_DRAW                    0x88E0
#define GL_STATIC_DRAW                    0x88E4
#define GL_DYNAMIC_DRAW                   0x88E8
#define GL_FRAGMENT_SHADER                0x8B30
#define GL_VERTEX_SHADER                  0x8B31
#define GL_COMPILE_STATUS                 0x8B81
#define GL_LINK_STATUS                    0x8B82
#define GL_INFO_LOG_LENGTH                0x8B84
#define GL_TEXTURE0                       0x84C0
#define GL_TEXTURE_2D_ARRAY               0x8C1A
#define GL_FRAMEBUFFER                    0x8D40
#define GL_READ_FRAMEBUFFER               0x8CA8
#define GL_DRAW_FRAMEBUFFER               0x8CA9
#define GL_RENDERBUFFER                   0x8D41
#define GL_COLOR_ATTACHMENT0              0x8CE0
#define GL_COLOR_ATTACHMENT1              0x8CE1
#define GL_DEPTH_ATTACHMENT               0x8D00
#define GL_FRAMEBUFFER_COMPLETE           0x8CD5
#define GL_COLOR_BUFFER_BIT               0x00004000
#define GL_DEPTH_BUFFER_BIT               0x00000100
#define GL_MULTISAMPLE                    0x809D
#define GL_FRAMEBUFFER_SRGB               0x8DB9

/* ── Entry points ──────────────────────────────────────────────
 * One list, expanded three ways: pointer typedefs, extern declarations and
 * the loader body. Adding a function means adding one line here.
 */

#define ECS_GL_FUNCTIONS \
    GLF(void,   glEnable,                  (GLenum)) \
    GLF(void,   glDisable,                 (GLenum)) \
    GLF(void,   glClear,                   (GLbitfield)) \
    GLF(void,   glClearColor,              (GLfloat, GLfloat, GLfloat, GLfloat)) \
    GLF(void,   glClearDepth,              (GLdouble)) \
    GLF(void,   glViewport,                (GLint, GLint, GLsizei, GLsizei)) \
    GLF(void,   glScissor,                 (GLint, GLint, GLsizei, GLsizei)) \
    GLF(void,   glDepthFunc,               (GLenum)) \
    GLF(void,   glDepthMask,               (GLboolean)) \
    GLF(void,   glBlendFunc,               (GLenum, GLenum)) \
    GLF(void,   glCullFace,                (GLenum)) \
    GLF(void,   glFrontFace,               (GLenum)) \
    GLF(GLenum, glGetError,                (void)) \
    GLF(const GLubyte *, glGetString,      (GLenum)) \
    GLF(void,   glPixelStorei,             (GLenum, GLint)) \
    GLF(void,   glReadPixels,              (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *)) \
    GLF(void,   glDrawArrays,              (GLenum, GLint, GLsizei)) \
    GLF(void,   glDrawElements,            (GLenum, GLsizei, GLenum, const void *)) \
    GLF(void,   glDrawArraysInstanced,     (GLenum, GLint, GLsizei, GLsizei)) \
    GLF(void,   glDrawElementsInstanced,   (GLenum, GLsizei, GLenum, const void *, GLsizei)) \
    GLF(void,   glGenTextures,             (GLsizei, GLuint *)) \
    GLF(void,   glDeleteTextures,          (GLsizei, const GLuint *)) \
    GLF(void,   glBindTexture,             (GLenum, GLuint)) \
    GLF(void,   glActiveTexture,           (GLenum)) \
    GLF(void,   glTexParameteri,           (GLenum, GLenum, GLint)) \
    GLF(void,   glTexImage2D,              (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *)) \
    GLF(void,   glTexSubImage2D,           (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *)) \
    GLF(void,   glTexImage3D,              (GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *)) \
    GLF(void,   glTexSubImage3D,           (GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, const void *)) \
    GLF(void,   glGenVertexArrays,         (GLsizei, GLuint *)) \
    GLF(void,   glDeleteVertexArrays,      (GLsizei, const GLuint *)) \
    GLF(void,   glBindVertexArray,         (GLuint)) \
    GLF(void,   glGenBuffers,              (GLsizei, GLuint *)) \
    GLF(void,   glDeleteBuffers,           (GLsizei, const GLuint *)) \
    GLF(void,   glBindBuffer,              (GLenum, GLuint)) \
    GLF(void,   glBufferData,              (GLenum, GLsizeiptr, const void *, GLenum)) \
    GLF(void,   glBufferSubData,           (GLenum, GLintptr, GLsizeiptr, const void *)) \
    GLF(void,   glEnableVertexAttribArray, (GLuint)) \
    GLF(void,   glVertexAttribPointer,     (GLuint, GLint, GLenum, GLboolean, GLsizei, const void *)) \
    GLF(void,   glVertexAttribIPointer,    (GLuint, GLint, GLenum, GLsizei, const void *)) \
    GLF(void,   glVertexAttribDivisor,     (GLuint, GLuint)) \
    GLF(GLuint, glCreateShader,            (GLenum)) \
    GLF(void,   glDeleteShader,            (GLuint)) \
    GLF(void,   glShaderSource,            (GLuint, GLsizei, const GLchar *const *, const GLint *)) \
    GLF(void,   glCompileShader,           (GLuint)) \
    GLF(void,   glGetShaderiv,             (GLuint, GLenum, GLint *)) \
    GLF(void,   glGetShaderInfoLog,        (GLuint, GLsizei, GLsizei *, GLchar *)) \
    GLF(GLuint, glCreateProgram,           (void)) \
    GLF(void,   glDeleteProgram,           (GLuint)) \
    GLF(void,   glAttachShader,            (GLuint, GLuint)) \
    GLF(void,   glLinkProgram,             (GLuint)) \
    GLF(void,   glUseProgram,              (GLuint)) \
    GLF(void,   glGetProgramiv,            (GLuint, GLenum, GLint *)) \
    GLF(void,   glGetProgramInfoLog,       (GLuint, GLsizei, GLsizei *, GLchar *)) \
    GLF(GLint,  glGetUniformLocation,      (GLuint, const GLchar *)) \
    GLF(void,   glUniform1i,               (GLint, GLint)) \
    GLF(void,   glUniform1f,               (GLint, GLfloat)) \
    GLF(void,   glUniform2f,               (GLint, GLfloat, GLfloat)) \
    GLF(void,   glUniform3f,               (GLint, GLfloat, GLfloat, GLfloat)) \
    GLF(void,   glUniform4f,               (GLint, GLfloat, GLfloat, GLfloat, GLfloat)) \
    GLF(void,   glUniformMatrix4fv,        (GLint, GLsizei, GLboolean, const GLfloat *)) \
    GLF(void,   glGenFramebuffers,         (GLsizei, GLuint *)) \
    GLF(void,   glDeleteFramebuffers,      (GLsizei, const GLuint *)) \
    GLF(void,   glBindFramebuffer,         (GLenum, GLuint)) \
    GLF(void,   glFramebufferTexture2D,    (GLenum, GLenum, GLenum, GLuint, GLint)) \
    GLF(GLenum, glCheckFramebufferStatus,  (GLenum)) \
    GLF(void,   glDrawBuffers,             (GLsizei, const GLenum *)) \
    GLF(void,   glReadBuffer,              (GLenum)) \
    GLF(void,   glBlitFramebuffer,         (GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum))

#define GLF(ret, name, params) typedef ret (ECS_GLAPI *PFN_##name) params;
ECS_GL_FUNCTIONS
#undef GLF

#define GLF(ret, name, params) extern PFN_##name ecs_##name;
ECS_GL_FUNCTIONS
#undef GLF

/* Call sites read as ordinary GL. The indirection is invisible above this
 * header, which keeps render_gl.c comparable with any GL reference material. */
#define GLF(ret, name, params)
ECS_GL_FUNCTIONS
#undef GLF

#define glEnable                  ecs_glEnable
#define glDisable                 ecs_glDisable
#define glClear                   ecs_glClear
#define glClearColor              ecs_glClearColor
#define glClearDepth              ecs_glClearDepth
#define glViewport                ecs_glViewport
#define glScissor                 ecs_glScissor
#define glDepthFunc               ecs_glDepthFunc
#define glDepthMask               ecs_glDepthMask
#define glBlendFunc               ecs_glBlendFunc
#define glCullFace                ecs_glCullFace
#define glFrontFace               ecs_glFrontFace
#define glGetError                ecs_glGetError
#define glGetString               ecs_glGetString
#define glPixelStorei             ecs_glPixelStorei
#define glReadPixels              ecs_glReadPixels
#define glDrawArrays              ecs_glDrawArrays
#define glDrawElements            ecs_glDrawElements
#define glDrawArraysInstanced     ecs_glDrawArraysInstanced
#define glDrawElementsInstanced   ecs_glDrawElementsInstanced
#define glGenTextures             ecs_glGenTextures
#define glDeleteTextures          ecs_glDeleteTextures
#define glBindTexture             ecs_glBindTexture
#define glActiveTexture           ecs_glActiveTexture
#define glTexParameteri           ecs_glTexParameteri
#define glTexImage2D              ecs_glTexImage2D
#define glTexSubImage2D           ecs_glTexSubImage2D
#define glTexImage3D              ecs_glTexImage3D
#define glTexSubImage3D           ecs_glTexSubImage3D
#define glGenVertexArrays         ecs_glGenVertexArrays
#define glDeleteVertexArrays      ecs_glDeleteVertexArrays
#define glBindVertexArray         ecs_glBindVertexArray
#define glGenBuffers              ecs_glGenBuffers
#define glDeleteBuffers           ecs_glDeleteBuffers
#define glBindBuffer              ecs_glBindBuffer
#define glBufferData              ecs_glBufferData
#define glBufferSubData           ecs_glBufferSubData
#define glEnableVertexAttribArray ecs_glEnableVertexAttribArray
#define glVertexAttribPointer     ecs_glVertexAttribPointer
#define glVertexAttribIPointer    ecs_glVertexAttribIPointer
#define glVertexAttribDivisor     ecs_glVertexAttribDivisor
#define glCreateShader            ecs_glCreateShader
#define glDeleteShader            ecs_glDeleteShader
#define glShaderSource            ecs_glShaderSource
#define glCompileShader           ecs_glCompileShader
#define glGetShaderiv             ecs_glGetShaderiv
#define glGetShaderInfoLog        ecs_glGetShaderInfoLog
#define glCreateProgram           ecs_glCreateProgram
#define glDeleteProgram           ecs_glDeleteProgram
#define glAttachShader            ecs_glAttachShader
#define glLinkProgram             ecs_glLinkProgram
#define glUseProgram              ecs_glUseProgram
#define glGetProgramiv            ecs_glGetProgramiv
#define glGetProgramInfoLog       ecs_glGetProgramInfoLog
#define glGetUniformLocation      ecs_glGetUniformLocation
#define glUniform1i               ecs_glUniform1i
#define glUniform1f               ecs_glUniform1f
#define glUniform2f               ecs_glUniform2f
#define glUniform3f               ecs_glUniform3f
#define glUniform4f               ecs_glUniform4f
#define glUniformMatrix4fv        ecs_glUniformMatrix4fv
#define glGenFramebuffers         ecs_glGenFramebuffers
#define glDeleteFramebuffers      ecs_glDeleteFramebuffers
#define glBindFramebuffer         ecs_glBindFramebuffer
#define glFramebufferTexture2D    ecs_glFramebufferTexture2D
#define glCheckFramebufferStatus  ecs_glCheckFramebufferStatus
#define glDrawBuffers             ecs_glDrawBuffers
#define glReadBuffer              ecs_glReadBuffer
#define glBlitFramebuffer         ecs_glBlitFramebuffer

/**
 * Resolve every entry point above through the platform's GL proc lookup.
 * Returns false and logs the first missing name if the driver is short of
 * 3.3 — the caller then falls back to the software renderer.
 */
bool gl_load(void *(*get_proc)(const char *));

#endif /* ECS_ENABLE_GL */
#endif /* GL_LOADER_H */
