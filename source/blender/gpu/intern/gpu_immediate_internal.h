/*
* ***** BEGIN GPL LICENSE BLOCK *****
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
* The Original Code is Copyright (C) 2005 Blender Foundation.
* All rights reserved.
*
* The Original Code is: all of this file.
*
* Contributor(s): Jason Wilkins.
*
* ***** END GPL LICENSE BLOCK *****
*/

/** \file blender/gpu/intern/gpu_internal.h
*  \ingroup gpu
*/

#ifndef _GPU_IMMEDIATE_INTERNAL_H_
#define _GPU_IMMEDIATE_INTERNAL_H_

#include "gpu_immediate_inline.h"

#ifdef __cplusplus
extern "C" {
#endif



#if GPU_SAFETY

/* Define some useful, but slow, checks for correct API usage. */

BLI_INLINE void GPU_CLEAR_ERRORS()
{
	while (glGetError() != GL_NO_ERROR) { /* empty */}
}

/* Each block contains variables that can be inspected by a
   debugger in the event that an assert is triggered. */

#define GPU_CHECK_CAN_SETUP()         \
    {                                 \
    GLboolean immediateOK;            \
    GLboolean noLockOK;               \
    GLboolean noBeginOK;              \
    GPU_CHECK_BASE(immediateOK);      \
    GPU_CHECK_NO_LOCK(noLockOK)       \
    GPU_CHECK_NO_BEGIN(noBeginOK)     \
    }

#define GPU_CHECK_CAN_PUSH()                                    \
    {                                                           \
    GLboolean immediateStackOK;                                 \
    GPU_SAFE_RETURN(immediateStack != NULL, immediateStackOK,); \
    }

#define GPU_CHECK_CAN_POP()                                          \
    {                                                                \
    GLboolean immediateOK;                                           \
    GLboolean noLockOK;                                              \
    GLboolean noBeginOK;                                             \
    GPU_SAFE_RETURN(GPU_IMMEDIATE, immediateOK, NULL);               \
    GPU_SAFE_RETURN(GPU_IMMEDIATE->buffer == NULL, noLockOK, NULL);  \
    GPU_SAFE_RETURN(GPU_IMMEDIATE->lockCount == 0, noBeginOK, NULL); \
    }

#define GPU_CHECK_CAN_LOCK()       \
    {                              \
    GLboolean immediateOK;         \
    GLboolean noBeginOK;           \
    GLboolean noLockOK;            \
    GPU_CHECK_BASE(immediateOK);   \
    GPU_CHECK_NO_BEGIN(noBeginOK); \
    GPU_CHECK_NO_LOCK(noLockOK);   \
    }

#define GPU_CHECK_CAN_UNLOCK()      \
    {                               \
    GLboolean immediateOK;          \
    GLboolean isLockedOK;           \
    GLboolean noBeginOK;            \
    GPU_CHECK_BASE(immediateOK);    \
    GPU_CHECK_IS_LOCKED(isLockedOK) \
    GPU_CHECK_NO_BEGIN(noBeginOK)   \
    }

#define GPU_CHECK_CAN_CURRENT()    \
    {                              \
    GLboolean immediateOK;         \
    GLboolean noBeginOK;           \
    GPU_CHECK_BASE(immediateOK);   \
    GPU_CHECK_NO_BEGIN(noBeginOK); \
    }

#define GPU_SAFE_STMT(var, test, stmt) \
    var = (GLboolean)(test);           \
    GPU_ASSERT((#test, var));          \
    if (var) {                         \
        stmt;                          \
    }

#else

#define GPU_CLEAR_ERRORS() ((void)0)

#define GPU_CHECK_CAN_SETUP()
#define GPU_CHECK_CAN_PUSH()
#define GPU_CHECK_CAN_POP()
#define GPU_CHECK_CAN_LOCK()
#define GPU_CHECK_CAN_UNLOCK()
#define GPU_CHECK_CAN_CURRENT()

#define GPU_SAFE_STMT(var, test, stmt) { (void)(var); stmt; }

#endif



GLsizei gpu_calc_stride(void);



void gpu_lock_buffer_gl11(void);
void gpu_unlock_buffer_gl11(void);
void gpu_begin_buffer_gl11(void);
void gpu_end_buffer_gl11(void);
void gpu_shutdown_buffer_gl11(GPUimmediate *restrict immediate);
void gpu_current_color_gl11(void);
void gpu_get_current_color_gl11(GLubyte *restrict v);



void gpu_lock_buffer_vbo(void);
void gpu_unlock_buffer_vbo(void);
void gpu_begin_buffer_vbo(void);
void gpu_end_buffer_vbo(void);
void gpu_shutdown_buffer_vbo(GPUimmediate *restrict immediate);
void gpu_current_color_vbo(void);
void gpu_get_current_color_vbo(GLubyte *restrict v);



#ifdef __cplusplus
}
#endif

#endif /* _GPU_INTERNAL_H_ */
