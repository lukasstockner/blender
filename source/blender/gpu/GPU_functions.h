/* ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Jason Wilkins.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/GPU_functions.h
 *  \ingroup gpu
 */
#ifndef GPU_FUNCTIONS_H
#define GPU_FUNTCIONS_H

#include "intern/gpu_immediate.h" /* XXX: temporary, will re-factor header files later */

#ifndef GPU_FUNC_INTERN
#define GPUFUNC extern
#else
#define GPUFUNC
#endif


#undef GLAPIENTRY /* glew.h was included above, so GLAPIENTRY is defined, but blank */

/***** BEGIN:THIS CODE WAS COPIED DIRECTLY FROM glew.h *****/

#if defined(_WIN32)

/*
 * GLEW does not include <windows.h> to avoid name space pollution.
 * GL needs GLAPI and GLAPIENTRY, GLU needs APIENTRY, CALLBACK, and wchar_t
 * defined properly.
 */
/* <windef.h> */
#ifndef APIENTRY
#define GLEW_APIENTRY_DEFINED
#  if defined(__MINGW32__) || defined(__CYGWIN__)
#    define APIENTRY __stdcall
#  elif (_MSC_VER >= 800) || defined(_STDCALL_SUPPORTED) || defined(__BORLANDC__)
#    define APIENTRY __stdcall
#  else
#    define APIENTRY
#  endif
#endif
#ifndef GLAPI
#  if defined(__MINGW32__) || defined(__CYGWIN__)
#    define GLAPI extern
#  endif
#endif
/* <winnt.h> */
#ifndef CALLBACK
#define GLEW_CALLBACK_DEFINED
#  if defined(__MINGW32__) || defined(__CYGWIN__)
#    define CALLBACK __attribute__ ((__stdcall__))
#  elif (defined(_M_MRX000) || defined(_M_IX86) || defined(_M_ALPHA) || defined(_M_PPC)) && !defined(MIDL_PASS)
#    define CALLBACK __stdcall
#  else
#    define CALLBACK
#  endif
#endif
/* <wingdi.h> and <winnt.h> */
#ifndef WINGDIAPI
#define GLEW_WINGDIAPI_DEFINED
#define WINGDIAPI __declspec(dllimport)
#endif
/* <ctype.h> */
#if (defined(_MSC_VER) || defined(__BORLANDC__)) && !defined(_WCHAR_T_DEFINED)
typedef unsigned short wchar_t;
#  define _WCHAR_T_DEFINED
#endif
/* <stddef.h> */
#if !defined(_W64)
#  if !defined(__midl) && (defined(_X86_) || defined(_M_IX86)) && defined(_MSC_VER) && _MSC_VER >= 1300
#    define _W64 __w64
#  else
#    define _W64
#  endif
#endif
#if !defined(_PTRDIFF_T_DEFINED) && !defined(_PTRDIFF_T_) && !defined(__MINGW64__)
#  ifdef _WIN64
typedef __int64 ptrdiff_t;
#  else
typedef _W64 int ptrdiff_t;
#  endif
#  define _PTRDIFF_T_DEFINED
#  define _PTRDIFF_T_
#endif

#ifndef GLAPI
#  if defined(__MINGW32__) || defined(__CYGWIN__)
#    define GLAPI extern
#  else
#    define GLAPI WINGDIAPI
#  endif
#endif

#ifndef GLAPIENTRY
#define GLAPIENTRY APIENTRY
#endif

/*
 * GLEW_STATIC is defined for static library.
 * GLEW_BUILD  is defined for building the DLL library.
 */

#ifdef GLEW_STATIC
#  define GLEWAPI extern
#else
#  ifdef GLEW_BUILD
#    define GLEWAPI extern __declspec(dllexport)
#  else
#    define GLEWAPI extern __declspec(dllimport)
#  endif
#endif

#else /* _UNIX */

/*
 * Needed for ptrdiff_t in turn needed by VBO.  This is defined by ISO
 * C.  On my system, this amounts to _3 lines_ of included code, all of
 * them pretty much harmless.  If you know of a way of detecting 32 vs
 * 64 _targets_ at compile time you are free to replace this with
 * something that's portable.  For now, _this_ is the portable solution.
 * (mem, 2004-01-04)
 */

#include <stddef.h>

/* SGI MIPSPro doesn't like stdint.h in C++ mode          */
/* ID: 3376260 Solaris 9 has inttypes.h, but not stdint.h */

#if (defined(__sgi) || defined(__sun)) && !defined(__GNUC__)
#include <inttypes.h>
#else
#include <stdint.h>
#endif

#define GLEW_APIENTRY_DEFINED
#define APIENTRY

/*
 * GLEW_STATIC is defined for static library.
 */

#ifdef GLEW_STATIC
#  define GLEWAPI extern
#else
#  if defined(__GNUC__) && __GNUC__>=4
#   define GLEWAPI extern __attribute__ ((visibility("default")))
#  elif defined(__SUNPRO_C) || defined(__SUNPRO_CC)
#   define GLEWAPI extern __global
#  else
#   define GLEWAPI extern
#  endif
#endif

#ifdef WITH_ANDROID
#undef GLEWAPI
#   define GLEWAPI extern __attribute__ ((visibility("default")))
#endif

/* <glu.h> */
#ifndef GLAPI
#define GLAPI extern
#endif
#ifndef GLAPIENTRY
#define GLAPIENTRY
#endif

#endif /* _WIN32 */

/***** END:THIS CODE WAS COPIED DIRECTLY FROM glew.h *****/

#ifdef __cplusplus
extern "C" {
#endif

GPUFUNC GLuint (GLAPIENTRY* gpuCreateShader)(GLuint shaderType);
GPUFUNC void (GLAPIENTRY* gpuAttachShader)(GLuint program, GLuint shader);
GPUFUNC void (GLAPIENTRY* gpuShaderSource)(GLuint shader, GLint count, const GLchar ** string, const GLint * length);
GPUFUNC void (GLAPIENTRY* gpuCompileShader)(GLuint shader);
GPUFUNC void (GLAPIENTRY* gpuGetShaderiv)(GLuint shader, GLuint pname, GLint *params);
GPUFUNC void (GLAPIENTRY* gpuGetShaderInfoLog)(GLuint shader, GLint maxLength, GLint *length, GLchar *infoLog);

GPUFUNC GLuint (GLAPIENTRY* gpuCreateProgram)(void);
GPUFUNC void (GLAPIENTRY* gpuLinkProgram)(GLuint program);
GPUFUNC void (GLAPIENTRY* gpuGetProgramiv)(GLuint shader, GLuint pname, GLint *params);
GPUFUNC void (GLAPIENTRY* gpuGetProgramInfoLog)(GLuint shader, GLint maxLength, GLint *length, GLchar *infoLog);


GPUFUNC void (GLAPIENTRY* gpuUniform1i)(GLint location, GLint v0);
GPUFUNC void (GLAPIENTRY* gpuUniform1f)(GLint location, GLfloat v0);

GPUFUNC void (GLAPIENTRY* gpuUniform1iv)(GLint location, GLint count, const GLint * value);
GPUFUNC void (GLAPIENTRY* gpuUniform2iv)(GLint location, GLint count, const GLint * value);
GPUFUNC void (GLAPIENTRY* gpuUniform3iv)(GLint location, GLint count, const GLint * value);
GPUFUNC void (GLAPIENTRY* gpuUniform4iv)(GLint location, GLint count, const GLint * value);


GPUFUNC void (GLAPIENTRY* gpuUniform1fv)(GLint location, GLint count, const GLfloat * value);
GPUFUNC void (GLAPIENTRY* gpuUniform2fv)(GLint location, GLint count, const GLfloat * value);
GPUFUNC void (GLAPIENTRY* gpuUniform3fv)(GLint location, GLint count, const GLfloat * value);
GPUFUNC void (GLAPIENTRY* gpuUniform4fv)(GLint location, GLint count, const GLfloat * value);
GPUFUNC void (GLAPIENTRY* gpuUniformMatrix3fv)(GLint location, GLint count, GLboolean transpose, const GLfloat * value);
GPUFUNC void (GLAPIENTRY* gpuUniformMatrix4fv)(GLint location, GLint count, GLboolean transpose, const GLfloat * value);

GPUFUNC GLint (GLAPIENTRY* gpuGetAttribLocation)(GLuint program, const GLchar *name);
GPUFUNC void (GLAPIENTRY* gpuBindAttribLocation)(GLuint program, GLuint index, const GLchar * name);
GPUFUNC GLint (GLAPIENTRY* gpuGetUniformLocation)(GLuint program, const GLchar * name);

GPUFUNC void (GLAPIENTRY* gpuVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *  pointer);

GPUFUNC void (GLAPIENTRY* gpuEnableVertexAttribArray)(GLuint index);
GPUFUNC void (GLAPIENTRY* gpuDisableVertexAttribArray)(GLuint index);

GPUFUNC void (GLAPIENTRY* gpuUseProgram)(GLuint program);
GPUFUNC void (GLAPIENTRY* gpuDeleteShader)(GLuint shader);
GPUFUNC void (GLAPIENTRY* gpuDeleteProgram)(GLuint program);



GPUFUNC void (GLAPIENTRY* gpuGenFramebuffers)(GLint m, GLuint * ids);
GPUFUNC void (GLAPIENTRY* gpuBindFramebuffer)(GLuint target, GLuint framebuffer);
GPUFUNC void (GLAPIENTRY* gpuDeleteFramebuffers)(GLint n, const GLuint * framebuffers);

GPUFUNC void (GLAPIENTRY* gpuGenBuffers)(GLsizei  n, GLuint *buffers);
GPUFUNC void (GLAPIENTRY* gpuBindBuffer)(GLenum target, GLuint buffer);
GPUFUNC void (GLAPIENTRY* gpuBufferData)(GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage);
GPUFUNC void (GLAPIENTRY* gpuDeleteBuffers)(GLsizei  n, const GLuint * buffers);


void GPU_func_comp_init(void);


#ifdef __cplusplus
}
#endif


#ifndef GIVE_ME_APIENTRY

/***** BEGIN:THIS CODE WAS COPIED DIRECTLY FROM glew.h *****/

#ifdef GLEW_APIENTRY_DEFINED
#undef GLEW_APIENTRY_DEFINED
#undef APIENTRY
#undef GLAPIENTRY
#define GLAPIENTRY
#endif

#ifdef GLEW_CALLBACK_DEFINED
#undef GLEW_CALLBACK_DEFINED
#undef CALLBACK
#endif

#ifdef GLEW_WINGDIAPI_DEFINED
#undef GLEW_WINGDIAPI_DEFINED
#undef WINGDIAPI
#endif

#undef GLAPI
/* #undef GLEWAPI */

/***** END:THIS CODE WAS COPIED DIRECTLY FROM glew.h *****/

#endif

#endif /* GPU_FUNCTIONS_H */
