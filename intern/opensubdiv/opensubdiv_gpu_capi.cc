/*
 * Adapted from OpenSubdiv with this license:
 *
 *   Copyright 2013 Pixar
 *
 *   Licensed under the Apache License, Version 2.0 (the "Apache License")
 *   with the following modification; you may not use this file except in
 *   compliance with the Apache License and the following modification to it:
 *   Section 6. Trademarks. is deleted and replaced with:
 *
 *   6. Trademarks. This License does not grant permission to use the trade
 *      names, trademarks, service marks, or product names of the Licensor
 *      and its affiliates, except as required to comply with Section 4(c) of
 *      the License and to reproduce the content of the NOTICE file.
 *
 *   You may obtain a copy of the Apache License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the Apache License with the above modification is
 *   distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 *   KIND, either express or implied. See the Apache License for the specific
 *   language governing permissions and limitations under the Apache License.
 *
 * Modifications Copyright 2014, Blender Foundation.
 *
 * Contributor(s): Sergey Sharybin
 *
 */

#include "opensubdiv_capi.h"

#ifdef _MSC_VER
#  include "iso646.h"
#endif

#include <GL/glew.h>

#include <opensubdiv/osd/glMesh.h>

#ifdef OPENSUBDIV_HAS_CUDA
#  include <opensubdiv/osd/cudaGLVertexBuffer.h>
#endif

#include <opensubdiv/osd/cpuGLVertexBuffer.h>
#include <opensubdiv/osd/cpuComputeContext.h>
#include <opensubdiv/osd/cpuComputeController.h>

#include "opensubdiv_partitioned.h"

using OpenSubdiv::OsdGLMeshInterface;
using OpenSubdiv::PartitionedGLMeshInterface;

extern "C" char datatoc_gpu_shader_opensubd_display_glsl[];

#ifndef OPENSUBDIV_LEGACY_DRAW

#define NUM_SOLID_LIGHTS 3
typedef struct Light {
	float position[4];
	float ambient[4];
	float diffuse[4];
	float specular[4];
} Light;

typedef struct Lighting {
	Light lights[NUM_SOLID_LIGHTS];
} Lighting;

typedef struct Transform {
	float projection_matrix[16];
	float model_view_matrix[16];
	float normal_matrix[9];
} Transform;

static GLuint g_flat_fill_program = 0;
static GLuint g_smooth_fill_program = 0;
static GLuint g_wireframe_program = 0;

static GLuint g_lighting_ub = 0;
static Lighting g_lighting_data;
static Transform g_transform;

/* TODO(sergey): This is actually duplicated code from BLI. */
namespace {
void copy_m3_m3(float m1[3][3], float m2[3][3])
{
	/* destination comes first: */
	memcpy(&m1[0], &m2[0], 9 * sizeof(float));
}

void copy_m3_m4(float m1[3][3], float m2[4][4])
{
	m1[0][0] = m2[0][0];
	m1[0][1] = m2[0][1];
	m1[0][2] = m2[0][2];

	m1[1][0] = m2[1][0];
	m1[1][1] = m2[1][1];
	m1[1][2] = m2[1][2];

	m1[2][0] = m2[2][0];
	m1[2][1] = m2[2][1];
	m1[2][2] = m2[2][2];
}

void adjoint_m3_m3(float m1[3][3], float m[3][3])
{
	m1[0][0] = m[1][1] * m[2][2] - m[1][2] * m[2][1];
	m1[0][1] = -m[0][1] * m[2][2] + m[0][2] * m[2][1];
	m1[0][2] = m[0][1] * m[1][2] - m[0][2] * m[1][1];

	m1[1][0] = -m[1][0] * m[2][2] + m[1][2] * m[2][0];
	m1[1][1] = m[0][0] * m[2][2] - m[0][2] * m[2][0];
	m1[1][2] = -m[0][0] * m[1][2] + m[0][2] * m[1][0];

	m1[2][0] = m[1][0] * m[2][1] - m[1][1] * m[2][0];
	m1[2][1] = -m[0][0] * m[2][1] + m[0][1] * m[2][0];
	m1[2][2] = m[0][0] * m[1][1] - m[0][1] * m[1][0];
}

float determinant_m3_array(float m[3][3])
{
	return (m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
	        m[1][0] * (m[0][1] * m[2][2] - m[0][2] * m[2][1]) +
	        m[2][0] * (m[0][1] * m[1][2] - m[0][2] * m[1][1]));
}

bool invert_m3_m3(float m1[3][3], float m2[3][3])
{
	float det;
	int a, b;
	bool success;

	/* calc adjoint */
	adjoint_m3_m3(m1, m2);

	/* then determinant old matrix! */
	det = determinant_m3_array(m2);

	success = (det != 0.0f);

	if (det != 0.0f) {
		det = 1.0f / det;
		for (a = 0; a < 3; a++) {
			for (b = 0; b < 3; b++) {
				m1[a][b] *= det;
			}
		}
	}

	return success;
}

bool invert_m3(float m[3][3])
{
	float tmp[3][3];
	bool success;

	success = invert_m3_m3(tmp, m);
	copy_m3_m3(m, tmp);

	return success;
}

void transpose_m3(float mat[3][3])
{
	float t;

	t = mat[0][1];
	mat[0][1] = mat[1][0];
	mat[1][0] = t;
	t = mat[0][2];
	mat[0][2] = mat[2][0];
	mat[2][0] = t;
	t = mat[1][2];
	mat[1][2] = mat[2][1];
	mat[2][1] = t;
}

GLuint compileShader(GLenum shaderType,
                     const char *section,
                     const char *define)
{
	const char *sources[3];
	char sdefine[64];
	sprintf(sdefine, "#define %s\n", section);

	sources[0] = define;
	sources[1] = sdefine;
	sources[2] = datatoc_gpu_shader_opensubd_display_glsl;

	GLuint shader = glCreateShader(shaderType);
	glShaderSource(shader, 3, sources, NULL);
	glCompileShader(shader);

	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		GLchar emsg[1024];
		glGetShaderInfoLog(shader, sizeof(emsg), 0, emsg);
		fprintf(stderr, "Error compiling GLSL shader (%s): %s\n", section, emsg);
		fprintf(stderr, "Section: %s\n", sdefine);
		fprintf(stderr, "Defines: %s\n", define);
		fprintf(stderr, "Source: %s\n", sources[2]);
		exit(1);
	}

	return shader;
}

GLuint linkProgram(const char *define)
{
	GLuint vertexShader = compileShader(GL_VERTEX_SHADER,
	                                    "VERTEX_SHADER",
	                                    define);
	GLuint geometryShader = compileShader(GL_GEOMETRY_SHADER,
	                                      "GEOMETRY_SHADER",
	                                      define);
	GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER,
	                                      "FRAGMENT_SHADER",
	                                      define);

	GLuint program = glCreateProgram();

	glAttachShader(program, vertexShader);
	glAttachShader(program, geometryShader);
	glAttachShader(program, fragmentShader);

	glBindAttribLocation(program, 0, "position");
	glBindAttribLocation(program, 1, "normal");

	glLinkProgram(program);

	glDeleteShader(vertexShader);
	glDeleteShader(geometryShader);
	glDeleteShader(fragmentShader);

	GLint status;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE) {
		GLchar emsg[1024];
		glGetProgramInfoLog(program, sizeof(emsg), 0, emsg);
		fprintf(stderr, "Error linking GLSL program : %s\n", emsg);
		fprintf(stderr, "Defines: %s\n", define);
		exit(1);
	}

	glUniformBlockBinding(program,
	                      glGetUniformBlockIndex(program, "Lighting"),
	                      0);

#if 0  /* Used for textured view */
	glProgramUniform1i(program,
	                   glGetUniformLocation(program, "texture_buffer"),
	                   0);  /* GL_TEXTURE0 */
#endif

	glProgramUniform1i(program,
	                   glGetUniformLocation(program, "FVarDataBuffer"),
	                   4);  /* GL_TEXTURE4 */

	return program;
}

void bindProgram(PartitionedGLMeshInterface *mesh,
                 int program)
{
	glUseProgram(program);

	/* Matricies */
	glUniformMatrix4fv(glGetUniformLocation(program, "modelViewMatrix"),
	                   1, false,
	                   g_transform.model_view_matrix);
	glUniformMatrix4fv(glGetUniformLocation(program, "projectionMatrix"),
	                   1, false,
	                   g_transform.projection_matrix);
	glUniformMatrix3fv(glGetUniformLocation(program, "normalMatrix"),
	                   1, false,
	                   g_transform.normal_matrix);

	/* Ligthing */
	glBindBuffer(GL_UNIFORM_BUFFER, g_lighting_ub);
	glBufferSubData(GL_UNIFORM_BUFFER,
	                0, sizeof(g_lighting_data), &g_lighting_data);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_lighting_ub);

	/* Color */
	GLboolean use_lighting;
	glGetBooleanv(GL_LIGHTING, &use_lighting);

	if (use_lighting) {
		float color[4];
		glGetMaterialfv(GL_FRONT, GL_DIFFUSE, color);
		glUniform4fv(glGetUniformLocation(program, "diffuse"), 1, color);

		glGetMaterialfv(GL_FRONT, GL_SPECULAR, color);
		glUniform4fv(glGetUniformLocation(program, "specular"), 1, color);

		glGetMaterialfv(GL_FRONT, GL_SHININESS, color);
		glUniform1f(glGetUniformLocation(program, "shininess"), color[0]);
	}
	else {
		float color[4];
		glGetFloatv(GL_CURRENT_COLOR, color);
		glUniform4fv(glGetUniformLocation(program, "diffuse"), 1, color);
	}

	/* Face-fertex data */
	if (mesh->GetDrawContext()->GetFvarDataTextureBuffer()) {
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_BUFFER,
		              mesh->GetDrawContext()->GetFvarDataTextureBuffer());
	}
}

}  /* namespace */
#endif

void openSubdiv_osdGLDisplayInit(void)
{
#ifndef OPENSUBDIV_LEGACY_DRAW
	static bool need_init = true;
	if (need_init) {
		g_flat_fill_program = linkProgram("#define FLAT_SHADING\n");
		g_smooth_fill_program = linkProgram("#define SMOOTH_SHADING\n");
		g_wireframe_program = linkProgram("#define WIREFRAME\n");

		/* We start with totally emoty lighting setup. */
		memset(&g_lighting_data, 0, sizeof(g_lighting_data));

		glGenBuffers(1, &g_lighting_ub);
		glBindBuffer(GL_UNIFORM_BUFFER, g_lighting_ub);
		glBufferData(GL_UNIFORM_BUFFER,
		             sizeof(g_lighting_data), NULL, GL_STATIC_DRAW);

		need_init = false;
	}
#endif
}

void openSubdiv_osdGLDisplayDeinit(void)
{
#ifndef OPENSUBDIV_LEGACY_DRAW
	if (g_lighting_ub != 0) {
		glDeleteBuffers(1, &g_lighting_ub);
	}
	if (g_flat_fill_program) {
		glDeleteProgram(g_flat_fill_program);
	}
	if (g_smooth_fill_program) {
		glDeleteProgram(g_flat_fill_program);
	}
	if (g_wireframe_program) {
		glDeleteProgram(g_wireframe_program);
	}
#endif
}

void openSubdiv_osdGLMeshDisplayPrepare(void)
{
#ifndef OPENSUBDIV_LEGACY_DRAW
	/* Update transformation matricies. */
	glGetFloatv(GL_PROJECTION_MATRIX, g_transform.projection_matrix);
	glGetFloatv(GL_MODELVIEW_MATRIX, g_transform.model_view_matrix);

	copy_m3_m4((float (*)[3])g_transform.normal_matrix,
	           (float (*)[4])g_transform.model_view_matrix);
	invert_m3((float (*)[3])g_transform.normal_matrix);
	transpose_m3((float (*)[3])g_transform.normal_matrix);

	/* Update OpenGL lights positions, colors etc. */
	for (int i = 0; i < NUM_SOLID_LIGHTS; ++i) {
		glGetLightfv(GL_LIGHT0 + i,
		             GL_POSITION,
		             g_lighting_data.lights[i].position);
		glGetLightfv(GL_LIGHT0 + i,
		             GL_AMBIENT,
		             g_lighting_data.lights[i].ambient);
		glGetLightfv(GL_LIGHT0 + i,
		             GL_DIFFUSE,
		             g_lighting_data.lights[i].diffuse);
		glGetLightfv(GL_LIGHT0 + i,
		             GL_SPECULAR,
		             g_lighting_data.lights[i].specular);
	}
#endif
}

void openSubdiv_osdGLMeshDisplay(OpenSubdiv_GLMesh *gl_mesh,
                                 int fill_quads,
                                 int partition)
{
	openSubdiv_osdGLDisplayInit();

	using OpenSubdiv::OsdDrawContext;
	using OpenSubdiv::FarPatchTables;

	PartitionedGLMeshInterface *mesh =
		(PartitionedGLMeshInterface *)(gl_mesh->descriptor);

	OsdDrawContext::PatchArrayVector const &patches =
	        partition >= 0
	            ? mesh->GetPatchArrays(partition)
	            : mesh->GetDrawContext()->patchArrays;

#ifndef OPENSUBDIV_LEGACY_DRAW
	GLuint program = g_smooth_fill_program;
	if (fill_quads) {
		int model;
		glGetIntegerv(GL_SHADE_MODEL, &model);
		if (model == GL_FLAT) {
			program = g_flat_fill_program;
		}
	}
	else {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		program = g_wireframe_program;
	}

	bindProgram(mesh, program);
#else
	if (!fill_quads) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}
#endif

	for (int i = 0; i < (int)patches.size(); ++i) {
		OpenSubdiv::OsdDrawContext::PatchArray const &patch = patches[i];
		OpenSubdiv::OsdDrawContext::PatchDescriptor desc = patch.GetDescriptor();
		OpenSubdiv::FarPatchTables::Type patchType = desc.GetType();

		if (patchType == OpenSubdiv::FarPatchTables::QUADS) {
			int mode = GL_QUADS;
#ifndef OPENSUBDIV_LEGACY_DRAW
			glUniform1i(glGetUniformLocation(program, "PrimitiveIdBase"),
			            patch.GetPatchIndex());
			mode = GL_LINES_ADJACENCY;
#endif
			glDrawElements(mode,
			               patch.GetNumIndices(),
			               GL_UNSIGNED_INT,
			               (void *)(patch.GetVertIndex() *
			                        sizeof(unsigned int)));
		}
	}

	/* Restore state. */
	if (!fill_quads) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
	glBindVertexArray(0);
#ifndef OPENSUBDIV_LEGACY_DRAW
	glActiveTexture(GL_TEXTURE0);
	/* TODO(sergey): Store previously used program and roll back to it? */
	glUseProgram(0);
#endif
}
