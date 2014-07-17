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
#extension GL_EXT_geometry_shader4 : enable
#extension GL_ARB_gpu_shader5 : enable
#extension GL_ARB_explicit_attrib_location : require

struct VertexData {
	vec4 position;
	vec3 normal;
	vec2 uv;
};

#ifdef VERTEX_SHADER

in vec3 normal;
in vec4 position;

uniform mat4 modelViewMatrix;
uniform mat3 normalMatrix;

out block {
	VertexData v;
} outpt;

void main()
{
	outpt.v.position = modelViewMatrix * position;
	outpt.v.normal = normalize(normalMatrix * normal);
}

#endif  /* VERTEX_SHADER */

/* ***** geometry shader ***** */
#ifdef GEOMETRY_SHADER

#ifndef GLSL_COMPAT_WORKAROUND
layout(lines_adjacency) in;
#ifndef WIREFRAME
layout(triangle_strip, max_vertices = 4) out;
#else
layout(line_strip, max_vertices = 8) out;
#endif
#endif

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform int PrimitiveIdBase;

in block {
	VertexData v;
} inpt[4];

#define INTERP_FACE_VARYING_2(result, fvarOffset, tessCoord)  \
	{ \
		vec2 v[4]; \
		int primOffset = (gl_PrimitiveID + PrimitiveIdBase) * 4; \
		for (int i = 0; i < 4; ++i) { \
			int index = (primOffset + i) * 2 + fvarOffset; \
			v[i] = vec2(texelFetch(FVarDataBuffer, index).s, \
			            texelFetch(FVarDataBuffer, index + 1).s); \
		} \
		result = mix(mix(v[0], v[1], tessCoord.s), \
		             mix(v[3], v[2], tessCoord.s), \
		             tessCoord.t); \
	}

uniform samplerBuffer FVarDataBuffer;

out block {
	VertexData v;
} outpt;

#ifdef FLAT_SHADING
void emit(int index, vec3 normal)
{
	outpt.v.position = inpt[index].v.position;
	outpt.v.normal = normal;

	/* TODO(sergey): Only uniform subdivisions atm. */
	vec2 quadst[4] = vec2[](vec2(0,0), vec2(1,0), vec2(1,1), vec2(0,1));
	vec2 st = quadst[index];

	INTERP_FACE_VARYING_2(outpt.v.uv, 0, st);

	gl_Position = projectionMatrix * inpt[index].v.position;
	EmitVertex();
}

#  ifdef WIREFRAME
void emit_edge(int v0, int v1, vec3 normal)
{
	emit(v0, normal);
	emit(v1, normal);
}
#  endif

#else
void emit(int index)
{
	outpt.v.position = inpt[index].v.position;
	outpt.v.normal = inpt[index].v.normal;

	/* TODO(sergey): Only uniform subdivisions atm. */
	vec2 quadst[4] = vec2[](vec2(0,0), vec2(1,0), vec2(1,1), vec2(0,1));
	vec2 st = quadst[index];

	INTERP_FACE_VARYING_2(outpt.v.uv, 0, st);

	gl_Position = projectionMatrix * inpt[index].v.position;
	EmitVertex();
}

#  ifdef WIREFRAME
void emit_edge(int v0, int v1)
{
	emit(v0);
	emit(v1);
}
#  endif

#endif

void main()
{
	gl_PrimitiveID = gl_PrimitiveIDIn;

#ifdef FLAT_SHADING
	vec3 A = (inpt[0].v.position - inpt[1].v.position).xyz;
	vec3 B = (inpt[3].v.position - inpt[1].v.position).xyz;
	vec3 flat_normal = normalize(cross(B, A));
#  ifndef WIREFRAME
	emit(0, flat_normal);
	emit(1, flat_normal);
	emit(3, flat_normal);
	emit(2, flat_normal);
#  else
	emit_edge(0, 1, flat_normal);
	emit_edge(1, 2, flat_normal);
	emit_edge(2, 3, flat_normal);
	emit_edge(3, 0, flat_normal);
#  endif
#else
#  ifndef WIREFRAME
	emit(0);
	emit(1);
	emit(3);
	emit(2);
#  else
	emit_edge(0, 1);
	emit_edge(1, 2);
	emit_edge(2, 3);
	emit_edge(3, 0);
#  endif
#endif

	EndPrimitive();
}

#endif  /* GEOMETRY_SHADER */

/* ***** Fragment shader ***** */
#ifdef FRAGMENT_SHADER

#define NUM_SOLID_LIGHTS 3

struct LightSource {
	vec4 position;
	vec4 ambient;
	vec4 diffuse;
	vec4 specular;
};

uniform Lighting {
	LightSource lightSource[NUM_SOLID_LIGHTS];
};

uniform vec4 diffuse;
uniform vec4 specular;
uniform float shininess;

#ifdef USE_TEXTURE
uniform sampler2D texture_buffer;
#endif

in block {
	VertexData v;
} inpt;

void main()
{
#ifdef WIREFRAME
	gl_FragColor = diffuse;
#else
	vec3 N = inpt.v.normal;

	if (!gl_FrontFacing)
		N = -N;

	/* Compute diffuse and specular lighting. */
	vec3 L_diffuse = vec3(0.0);
	vec3 L_specular = vec3(0.0);

	/* Assume NUM_SOLID_LIGHTS directional lights. */
	for (int i = 0; i < NUM_SOLID_LIGHTS; i++) {
		vec3 light_direction = lightSource[i].position.xyz;

		/* Diffuse light. */
		vec3 light_diffuse = lightSource[i].diffuse.rgb;
		float diffuse_bsdf = max(dot(N, light_direction), 0.0);
		L_diffuse += light_diffuse * diffuse_bsdf;

		vec4 Plight = lightSource[i].position;
		vec3 l = (Plight.w == 0.0)
			? normalize(Plight.xyz) : normalize(Plight.xyz -
			                                    inpt.v.position.xyz);

		/* Specular light. */
		vec3 light_specular = lightSource[i].specular.rgb;
		vec3 H = normalize(l + vec3(0,0,1));

		float specular_bsdf = pow(max(dot(N, H), 0.0),
		                          shininess);
		L_specular += light_specular * specular_bsdf;
	}

	/* Compute diffuse color. */
	float alpha;
#ifdef USE_TEXTURE
	L_diffuse *= texture2D(texture_buffer, inpt.v.uv).rgb;
#else
	L_diffuse *= diffuse.rgb;
#endif
	alpha = diffuse.a;

	/* Sum lighting. */
	vec3 L = /*gl_FrontLightModelProduct.sceneColor.rgb +*/ L_diffuse;
	L += L_specular * specular.rgb;

	/* Write out fragment color. */
	gl_FragColor = vec4(L, alpha);
#endif
}

#endif  // FRAGMENT_SHADER
