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

BLI_INLINE void gpu_clear_errors()
{
	while (glGetError() != GL_NO_ERROR) {
	}
}

/* Each block contains variables that can be inspected by a
   debugger in the event that an assert is triggered. */

#define GPU_CHECK_BUFFER_LOCK(var) \
    GPU_SAFE_RETURN(GPU_IMMEDIATE->bufferLock != NULL, var);

#define GPU_CHECK_BUFFER_UNLOCK(var) \
    GPU_SAFE_RETURN(GPU_IMMEDIATE->bufferUnlock != NULL, var);

#define GPU_CHECK_CAN_SETUP()         \
    {                                 \
    GLboolean immediateOK;            \
    GLboolean noLockOK;               \
    GLboolean noBeginOK;              \
    GPU_CHECK_BASE(immediateOK);      \
    GPU_CHECK_NO_LOCK(noLockOK)       \
    GPU_CHECK_NO_BEGIN(noBeginOK)     \
    }

#define GPU_CHECK_CAN_LOCK()            \
    {                                   \
    GLboolean immediateOK;              \
    GLboolean noBeginOK;                \
    GLboolean bufferLockOK;             \
    GPU_CHECK_BASE(immediateOK);        \
    GPU_CHECK_NO_BEGIN(noBeginOK)       \
    GPU_CHECK_BUFFER_LOCK(bufferLockOK) \
    }

#define GPU_CHECK_CAN_UNLOCK()              \
    {                                       \
    GLboolean immediateOK;                  \
    GLboolean isLockedOK;                   \
    GLboolean noBeginOK;                    \
    GLboolean bufferUnlockOK;               \
    GPU_CHECK_BASE(immediateOK);            \
    GPU_CHECK_IS_LOCKED(isLockedOK)         \
    GPU_CHECK_NO_BEGIN(noBeginOK)           \
    GPU_CHECK_BUFFER_UNLOCK(bufferUnlockOK) \
    }

#define GPU_SAFE_STMT(var, test, stmt) \
    var = (GLboolean)(test);           \
    BLI_assert((#test, var));          \
    if (var) {                         \
        stmt;                          \
    }

#else

#define gpu_clear_errors() ((void)0)

#define GPU_CHECK_CAN_SETUP()
#define GPU_CHECK_CAN_LOCK()
#define GPU_CHECK_CAN_UNLOCK()

#define GPU_SAFE_STMT(var, test, stmt) { (void)(var); stmt; }

#endif



GLsizei gpu_calc_stride(void);



void gpu_lock_buffer_gl11(void);
void gpu_unlock_buffer_gl11(void);
void gpu_begin_buffer_gl11(void);
void gpu_end_buffer_gl11(void);
void gpu_shutdown_buffer_gl11(GPUimmediate *restrict immediate);



void gpu_lock_buffer_vbo(void);
void gpu_unlock_buffer_vbo(void);
void gpu_begin_buffer_vbo(void);
void gpu_end_buffer_vbo(void);
void gpu_shutdown_buffer_vbo(GPUimmediate *restrict immediate);



#ifdef __cplusplus
}
#endif

#endif /* _GPU_INTERNAL_H_ */
