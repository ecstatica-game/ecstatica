/**
 * gl_loader.c
 *
 * Definitions and the one-shot resolve for the entry points in gl_loader.h.
 */

#include "gl_loader.h"

#ifdef ECS_ENABLE_GL

#include "types.h"

/* The #defines in the header would rewrite the pointer definitions below into
 * themselves. Nothing here calls GL, so drop them for this file only. */
#define GLF(ret, name, params) PFN_##name ecs_##name = 0;
#undef glEnable
#undef glDisable
#undef glClear
#undef glClearColor
#undef glClearDepth
#undef glViewport
#undef glScissor
#undef glDepthFunc
#undef glDepthMask
#undef glBlendFunc
#undef glCullFace
#undef glFrontFace
#undef glGetError
#undef glGetString
#undef glPixelStorei
#undef glReadPixels
#undef glDrawArrays
#undef glDrawElements
#undef glDrawArraysInstanced
#undef glDrawElementsInstanced
#undef glGenTextures
#undef glDeleteTextures
#undef glBindTexture
#undef glActiveTexture
#undef glTexParameteri
#undef glTexImage2D
#undef glTexSubImage2D
#undef glTexImage3D
#undef glTexSubImage3D
#undef glGenVertexArrays
#undef glDeleteVertexArrays
#undef glBindVertexArray
#undef glGenBuffers
#undef glDeleteBuffers
#undef glBindBuffer
#undef glBufferData
#undef glBufferSubData
#undef glEnableVertexAttribArray
#undef glVertexAttribPointer
#undef glVertexAttribIPointer
#undef glVertexAttribDivisor
#undef glCreateShader
#undef glDeleteShader
#undef glShaderSource
#undef glCompileShader
#undef glGetShaderiv
#undef glGetShaderInfoLog
#undef glCreateProgram
#undef glDeleteProgram
#undef glAttachShader
#undef glLinkProgram
#undef glUseProgram
#undef glGetProgramiv
#undef glGetProgramInfoLog
#undef glGetUniformLocation
#undef glUniform1i
#undef glUniform1f
#undef glUniform2f
#undef glUniform3f
#undef glUniform4f
#undef glUniformMatrix4fv
#undef glGenFramebuffers
#undef glDeleteFramebuffers
#undef glBindFramebuffer
#undef glFramebufferTexture2D
#undef glCheckFramebufferStatus
#undef glDrawBuffers
#undef glReadBuffer
#undef glBlitFramebuffer
ECS_GL_FUNCTIONS
#undef GLF

bool gl_load(void *(*get_proc)(const char *)) {
    if (!get_proc) return false;

    bool ok = true;

#define GLF(ret, name, params)                                      \
    ecs_##name = (PFN_##name)get_proc(#name);                       \
    if (!ecs_##name) {                                              \
        DBG_LOG(1, "[GL] missing entry point: %s\n", #name);        \
        ok = false;                                                 \
    }
    ECS_GL_FUNCTIONS
#undef GLF

    return ok;
}

#endif /* ECS_ENABLE_GL */
