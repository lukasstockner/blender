/*
 * Copyright 2011-2013 Blender Foundation
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

CCL_NAMESPACE_BEGIN

/* Sky texture */

__device float sky_angle_between(float thetav, float phiv, float theta, float phi)
{
	float cospsi = sinf(thetav)*sinf(theta)*cosf(phi - phiv) + cosf(thetav)*cosf(theta);
	return safe_acosf(cospsi);
}

/*
 * "A Practical Analytic Model for Daylight"
 * A. J. Preetham, Peter Shirley, Brian Smits
 */
__device float sky_perez_function(__constant float *lam, float theta, float gamma)
{
	float ctheta = cosf(theta);
	float cgamma = cosf(gamma);

	return (1.0f + lam[0]*expf(lam[1]/ctheta)) * (1.0f + lam[2]*expf(lam[3]*gamma)  + lam[4]*cgamma*cgamma);
}

__device float3 sky_radiance_old(KernelGlobals *kg, float3 dir)
{
	/* convert vector to spherical coordinates */
	float2 spherical = direction_to_spherical(dir);
	float theta = spherical.x;
	float phi = spherical.y;

	/* angle between sun direction and dir */
	float gamma = sky_angle_between(theta, phi, kernel_data.sunsky.theta, kernel_data.sunsky.phi);

	/* clamp theta to horizon */
	theta = min(theta, M_PI_2_F - 0.001f);

	/* compute xyY color space values */
	float x = kernel_data.sunsky.radiance_y * sky_perez_function(kernel_data.sunsky.config_y, theta, gamma);
	float y = kernel_data.sunsky.radiance_z * sky_perez_function(kernel_data.sunsky.config_z, theta, gamma);
	float Y = kernel_data.sunsky.radiance_x * sky_perez_function(kernel_data.sunsky.config_x, theta, gamma);

	/* convert to RGB */
	float3 xyz = xyY_to_xyz(x, y, Y);
	return xyz_to_rgb(xyz.x, xyz.y, xyz.z);
}

/*
 * "An Analytic Model for Full Spectral Sky-Dome Radiance"
 * Lukas Hosek, Alexander Wilkie
 */
__device float sky_radiance_internal(__constant float *configuration, float theta, float gamma)
{
	float ctheta = cosf(theta);
	float cgamma = cosf(gamma);

	float expM = expf(configuration[4] * gamma);
	float rayM = cgamma * cgamma;
	float mieM = (1.0f + rayM) / powf((1.0f + configuration[8]*configuration[8] - 2.0f*configuration[8]*cgamma), 1.5f);
	float zenith = sqrt(ctheta);

	return (1.0f + configuration[0] * expf(configuration[1] / (ctheta + 0.01f))) *
		(configuration[2] + configuration[3] * expM + configuration[5] * rayM + configuration[6] * mieM + configuration[7] * zenith);
}

__device float3 sky_radiance_new(KernelGlobals *kg, float3 dir)
{
	/* convert vector to spherical coordinates */
	float2 spherical = direction_to_spherical(dir);
	float theta = spherical.x;
	float phi = spherical.y;

	/* angle between sun direction and dir */
	float gamma = sky_angle_between(theta, phi, kernel_data.sunsky.theta, kernel_data.sunsky.phi);

	/* clamp theta to horizon */
	theta = min(theta, M_PI_2_F - 0.001f);

	/* compute xyz color space values */
	float x = sky_radiance_internal(kernel_data.sunsky.config_x, theta, gamma) * kernel_data.sunsky.radiance_x;
	float y = sky_radiance_internal(kernel_data.sunsky.config_y, theta, gamma) * kernel_data.sunsky.radiance_y;
	float z = sky_radiance_internal(kernel_data.sunsky.config_z, theta, gamma) * kernel_data.sunsky.radiance_z;

	/* convert to RGB and adjust strength*/
	return xyz_to_rgb(x, y, z) * (M_2PI_F/683);
}

__device void svm_node_tex_sky(KernelGlobals *kg, ShaderData *sd, float *stack, uint dir_offset, uint out_offset, uint sky_model)
{
	float3 dir = stack_load_float3(stack, dir_offset);
	float3 f;

	if(sky_model == 0)
		f = sky_radiance_old(kg, dir);
	else
		f = sky_radiance_new(kg, dir);

	stack_store_float3(stack, out_offset, f);
}

CCL_NAMESPACE_END

