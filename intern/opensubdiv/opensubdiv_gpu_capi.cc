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
#include <vector>

#include <opensubdiv/osd/glMesh.h>

#ifdef OPENSUBDIV_HAS_CUDA
#  include <opensubdiv/osd/cudaGLVertexBuffer.h>
#endif
#include <opensubdiv/osd/glDrawRegistry.h>

// **************** Types declaration ****************

using OpenSubdiv::OsdDrawContext;
using OpenSubdiv::OsdGLDrawRegistry;
using OpenSubdiv::OsdGLMeshInterface;

static struct Transform {
	float ModelViewMatrix[16];
	float ProjectionMatrix[16];
	float ModelViewProjectionMatrix[16];
} g_transformData;

extern "C" char datatoc_gpu_shader_opensubd_display_glsl[];

inline void multMatrix(float *d, const float *a, const float *b) {
	for (int i=0; i<4; ++i) {
		for (int j=0; j<4; ++j) {
			d[i*4 + j] =
				a[i*4 + 0] * b[0*4 + j] +
				a[i*4 + 1] * b[1*4 + j] +
				a[i*4 + 2] * b[2*4 + j] +
				a[i*4 + 3] * b[3*4 + j];
		}
	}
}

static void bindProgram(GLuint program)
{
	glUseProgram(program);

	// shader uniform setting
	GLint position = glGetUniformLocation(program, "lightSource[0].position");
	GLint ambient = glGetUniformLocation(program, "lightSource[0].ambient");
	GLint diffuse = glGetUniformLocation(program, "lightSource[0].diffuse");
	GLint specular = glGetUniformLocation(program, "lightSource[0].specular");
	GLint position1 = glGetUniformLocation(program, "lightSource[1].position");
	GLint ambient1 = glGetUniformLocation(program, "lightSource[1].ambient");
	GLint diffuse1 = glGetUniformLocation(program, "lightSource[1].diffuse");
	GLint specular1 = glGetUniformLocation(program, "lightSource[1].specular");

	glUniform4f(position, 0.5, 0.2f, 1.0f, 0.0f);
	glUniform4f(ambient, 0.1f, 0.1f, 0.1f, 1.0f);
	glUniform4f(diffuse, 0.7f, 0.7f, 0.7f, 1.0f);
	glUniform4f(specular, 0.8f, 0.8f, 0.8f, 1.0f);

	glUniform4f(position1, -0.8f, 0.4f, -1.0f, 0.0f);
	glUniform4f(ambient1, 0.0f, 0.0f, 0.0f, 1.0f);
	glUniform4f(diffuse1, 0.5f, 0.5f, 0.5f, 1.0f);
	glUniform4f(specular1, 0.8f, 0.8f, 0.8f, 1.0f);

	GLint otcMatrix = glGetUniformLocation(program, "objectToClipMatrix");
	GLint oteMatrix = glGetUniformLocation(program, "objectToEyeMatrix");

	glUniformMatrix4fv(otcMatrix, 1, false,
	                   g_transformData.ModelViewProjectionMatrix);
	glUniformMatrix4fv(oteMatrix, 1, false,
	                   g_transformData.ModelViewMatrix);
}

static GLuint compileShader(GLenum shaderType,
                            const char *section,
                            const char *define)
{
	const char *sources[4];
	char sdefine[64];
	sprintf(sdefine, "#define %s\n", section);

	sources[0] = "#version 330\n";
	sources[1] = define;
	sources[2] = sdefine;
	sources[3] = datatoc_gpu_shader_opensubd_display_glsl;

	GLuint shader = glCreateShader(shaderType);
	glShaderSource(shader, 4, sources, NULL);
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

static GLuint linkProgram(const char *define)
{
	GLuint vertexShader = compileShader(GL_VERTEX_SHADER, "VERTEX_SHADER", define);
	GLuint geometryShader = compileShader(GL_GEOMETRY_SHADER, "GEOMETRY_SHADER", define);
	GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, "FRAGMENT_SHADER", define);

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
	glGetProgramiv(program, GL_LINK_STATUS, &status );
	if (status == GL_FALSE) {
		GLchar emsg[1024];
		glGetProgramInfoLog(program, sizeof(emsg), 0, emsg);
		fprintf(stderr, "Error linking GLSL program : %s\n", emsg );
		fprintf(stderr, "Defines: %s\n", define);
		exit(1);
	}

	return program;
}

void openSubdiv_osdGLMeshDisplay(OpenSubdiv_GLMesh *gl_mesh)
{
	//static GLuint g_quadLineProgram = 0;
	static GLuint g_quadFillProgram = 0;
	static bool need_init = true;
	OsdGLMeshInterface *mesh = (OsdGLMeshInterface *) gl_mesh->descriptor;

	if (need_init) {
		g_quadFillProgram = linkProgram("#define PRIM_QUAD\n#define GEOMETRY_OUT_FILL\n");
		//g_quadLineProgram = linkProgram("#define PRIM_QUAD\n#define GEOMETRY_OUT_LINE\n");
		need_init = false;
	}

	glGetFloatv(GL_PROJECTION_MATRIX, g_transformData.ProjectionMatrix);
	glGetFloatv(GL_MODELVIEW_MATRIX, g_transformData.ModelViewMatrix);
	multMatrix(g_transformData.ModelViewProjectionMatrix,
			   g_transformData.ModelViewMatrix,
			   g_transformData.ProjectionMatrix);

	using OpenSubdiv::OsdDrawContext;
	using OpenSubdiv::FarPatchTables;

	const OsdDrawContext::PatchArrayVector &patches = mesh->GetDrawContext()->patchArrays;

	for (int i=0; i<(int)patches.size(); ++i) {
		OpenSubdiv::OsdDrawContext::PatchArray const &patch = patches[i];

		bindProgram(g_quadFillProgram);
		glDrawElements(GL_LINES_ADJACENCY, patch.GetNumIndices(),
		               GL_UNSIGNED_INT, NULL);
	}
	glBindVertexArray(0);
	glUseProgram(0);
}
