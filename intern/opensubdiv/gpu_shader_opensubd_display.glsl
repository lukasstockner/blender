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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/* ***** Vertex shader ***** */

#version 130

#ifdef VERTEX_SHADER

in vec3 normal;
in vec3 position;

out vec3 varying_normal;
out vec3 varying_position;

void main()
{
	vec4 co = gl_ModelViewMatrix * vec4(position, 1.0);

	varying_normal = normalize(gl_NormalMatrix * normal);
	varying_position = co.xyz;

	gl_Position = gl_ProjectionMatrix * co;

#ifdef GPU_NVIDIA
	/* Setting gl_ClipVertex is necessary to get glClipPlane working on NVIDIA
	 * graphic cards, while on ATI it can cause a software fallback.
	 */
	gl_ClipVertex = gl_ModelViewMatrix * gl_Vertex;
#endif
}

#endif  /* VERTEX_SHADER */

/* ***** Fragment shader ***** */
#ifdef FRAGMENT_SHADER

#define NUM_SOLID_LIGHTS 3

in vec3 varying_normal;
in vec3 varying_position;

void main()
{
	/* Compute normal. */
	vec3 N = varying_normal;

	if (!gl_FrontFacing)
		N = -N;

	/* Compute diffuse and specular lighting. */
	vec3 L_diffuse = vec3(0.0);
	vec3 L_specular = vec3(0.0);

	/* Assume NUM_SOLID_LIGHTS directional lights. */
	for (int i = 0; i < NUM_SOLID_LIGHTS; i++) {
		vec3 light_direction = gl_LightSource[i].position.xyz;

		/* Diffuse light. */
		vec3 light_diffuse = gl_LightSource[i].diffuse.rgb;
		float diffuse_bsdf = max(dot(N, light_direction), 0.0);
		L_diffuse += light_diffuse*diffuse_bsdf;

		/* Specular light. */
		vec3 light_specular = gl_LightSource[i].specular.rgb;
		vec3 H = gl_LightSource[i].halfVector.xyz;

		float specular_bsdf = pow(max(dot(N, H), 0.0),
		                          gl_FrontMaterial.shininess);
		L_specular += light_specular*specular_bsdf;
	}

	/* Compute diffuse color. */
	float alpha;
	L_diffuse *= gl_FrontMaterial.diffuse.rgb;
	alpha = gl_FrontMaterial.diffuse.a;

	/* Sum lighting. */
	vec3 L = gl_FrontLightModelProduct.sceneColor.rgb + L_diffuse;
	L += L_specular*gl_FrontMaterial.specular.rgb;

	/* Write out fragment color. */
	gl_FragColor = vec4(L, alpha);
}

#endif  // FRAGMENT_SHADER
