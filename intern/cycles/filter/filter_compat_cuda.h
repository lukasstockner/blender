/*
 * Copyright 2011-2017 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __FILTER_COMPAT_CUDA_H__
#define __FILTER_COMPAT_CUDA_H__

#define __KERNEL_GPU__
#define __KERNEL_CUDA__
#define CCL_NAMESPACE_BEGIN
#define CCL_NAMESPACE_END

#include <cuda.h>
#include <cuda_fp16.h>
#include <float.h>

/* Qualifier wrappers for different names on different devices */

#define ccl_device  __device__ __inline__
#  define ccl_device_forceinline  __device__ __forceinline__
#if (__KERNEL_CUDA_VERSION__ == 80) && (__CUDA_ARCH__ < 500)
#  define ccl_device_inline  __device__ __forceinline__
#else
#  define ccl_device_inline  __device__ __inline__
#endif
#define ccl_device_noinline  __device__ __noinline__
#define ccl_global
#define ccl_constant
#define ccl_may_alias
#define ccl_addr_space
#define ccl_restrict __restrict__
#define ccl_align(n) __align__(n)
#define ccl_readonly_ptr const * __restrict__
#define ccl_local __shared__
#define ccl_local_param

#define CCL_MAX_LOCAL_SIZE (CUDA_THREADS_BLOCK_WIDTH*CUDA_THREADS_BLOCK_WIDTH)

/* No assert supported for CUDA */

#define kernel_assert(cond)

/* Types */

#include "util_half.h"
#include "util_types.h"

/* Use fast math functions */

#define cosf(x) __cosf(((float)(x)))
#define sinf(x) __sinf(((float)(x)))
#define powf(x, y) __powf(((float)(x)), ((float)(y)))
#define tanf(x) __tanf(((float)(x)))
#define logf(x) __logf(((float)(x)))
#define expf(x) __expf(((float)(x)))

#endif /* __FILTER_COMPAT_CUDA_H__ */
