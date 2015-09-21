/*
 * Copyright 2011-2014 Blender Foundation
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
 * limitations under the License
 */

#ifndef __UTIL_DISTORT_H__
#define __UTIL_DISTORT_H__

#ifdef WITH_CYCLES_DISTORTION
#  include "libmv/simple_pipeline/distortion_models.h"
#endif

CCL_NAMESPACE_BEGIN

ccl_device void util_apply_polynomial_distortion(const float2 image,
                                                 const float focal_length,
                                                 const float2 principal_point,
                                                 const float k1,
                                                 const float k2,
                                                 const float k3,
                                                 float2 *result)
{
#if defined(WITH_CYCLES_DISTORTION) && !defined(__KERNEL_GPU__)
	double normalized_x = (image.x - principal_point.x) / focal_length;
	double normalized_y = (image.y - principal_point.y) / focal_length;
	double image_x, image_y;
	libmv::InvertPolynomialDistortionModel(focal_length, focal_length,
	                                       normalized_x, normalized_y,
	                                       k1, k2, k3,
	                                       0.0, 0.0,
	                                       image.x, image.y,
	                                       &image_x, &image_y);
	result->x = image_x;
	result->y = image_y;
#else
	*result = image;
#endif
}

ccl_device void util_invert_polynomial_distortion(const float2 image,
                                                  const float focal_length,
                                                  const float2 principal_point,
                                                  const float k1,
                                                  const float k2,
                                                  const float k3,
                                                  float2 *result)
{
#if defined(WITH_CYCLES_DISTORTION) && !defined(__KERNEL_GPU__)
	double normalized_x, normalized_y;
	libmv::InvertPolynomialDistortionModel(focal_length, focal_length,
	                                       principal_point.x, principal_point.y,
	                                       k1, k2, k3,
	                                       0.0, 0.0,
	                                       image.x, image.y,
	                                       &normalized_x,
	                                       &normalized_y);
	result->x = normalized_x * (double)focal_length + (double)principal_point.x;
	result->y = normalized_y * (double)focal_length + (double)principal_point.y;
#else
	*result = image;
#endif
}

CCL_NAMESPACE_END

#endif  /* __UTIL_DISTORT_H__ */
