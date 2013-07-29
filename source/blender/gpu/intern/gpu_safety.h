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
* The Original Code is Copyright (C) 2012 Blender Foundation.
* All rights reserved.
*
* The Original Code is: all of this file.
*
* Contributor(s): Jason Wilkins.
*
* ***** END GPL LICENSE BLOCK *****
*/

/** \file blender/gpu/intern/gpu_safety.h
*  \ingroup gpu
*/

#ifndef GPU_SAFETY_H
#define GPU_SAFETY_H

#include "gpu_glew.h"

#include "BLI_utildefines.h"

#include <stdlib.h> /* for abort */



#ifdef __cplusplus
extern "C" {
#endif



#ifndef GPU_SAFETY
#define GPU_SAFETY (!defined(NDEBUG) && WITH_GPU_SAFETY)
#endif



const char* gpuErrorString(GLenum err);

void gpu_check(const char* file, int line, const char* text);



#if GPU_SAFETY

/* Define some useful, but slow, checks for correct API usage. */

#define GPU_ASSERT(test) BLI_assert(test)

/* Bails out of function even if assert or abort are disabled.
   Needs a variable in scope to store results of the test.
   Can be used in functions that return void if third argument is left blank */
#define GPU_SAFE_RETURN(test, var, ret) \
    var = (GLboolean)(test);            \
    GPU_ASSERT(((void)#test, var));     \
    if (!var) {                         \
        return ret;                     \
    }

#if WITH_ASSERT_ABORT
#define GPU_ABORT abort
#else
#define GPU_ABORT() ((void)0)
#endif

#define GPU_CHECK_NO_ERROR() gpu_check(__FILE__, __LINE__, 0)

#define GPU_CHECK(x) (gpu_check(__FILE__, __LINE__, "Before: " #x), (x), gpu_check(__FILE__, __LINE__, "After: " #x))

#else

#define GPU_ASSERT(test)                ((void)0)

#define GPU_SAFE_RETURN(test, var, ret) { (void)var; }

#define GPU_ABORT()                     ((void)0)

#define GPU_CHECK_NO_ERROR()            ((void)0)

#define GPU_CHECK(x) x

#endif /* GPU_SAFETY */



#ifdef __cplusplus
}
#endif

#endif /* GPU_SAFETY_H */
