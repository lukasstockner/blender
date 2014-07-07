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

using OpenSubdiv::OsdGLMeshInterface;

extern "C" char datatoc_gpu_shader_opensubd_display_glsl[];

#ifndef OPENSUBDIV_LEGACY_DRAW
static GLuint compileShader(GLenum shaderType,
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

static GLuint linkProgram(const char *define)
{
	GLuint vertexShader = compileShader(GL_VERTEX_SHADER,
	                                    "VERTEX_SHADER",
	                                    define);
	GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER,
	                                      "FRAGMENT_SHADER",
	                                      define);

	GLuint program = glCreateProgram();

	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);

	glBindAttribLocation(program, 0, "position");
	glBindAttribLocation(program, 1, "normal");

	glLinkProgram(program);

	glDeleteShader(vertexShader);
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

	return program;
}
#endif

void openSubdiv_osdGLMeshDisplay(OpenSubdiv_GLMesh *gl_mesh, int fill_quads)
{
#ifndef OPENSUBDIV_LEGACY_DRAW
	static GLuint flat_fill_program;
	static GLuint smooth_fill_program;
	static GLuint wireframe_program;
	static bool need_init = true;

	if (need_init) {
		flat_fill_program = linkProgram("#define FLAT_SHADING\n");
		smooth_fill_program = linkProgram("#define SMOOTH_SHADING\n");
		wireframe_program = linkProgram("#define WIREFRAME\n");
		need_init = false;
	}
#endif

	OsdGLMeshInterface *mesh = (OsdGLMeshInterface *) gl_mesh->descriptor;

	using OpenSubdiv::OsdDrawContext;
	using OpenSubdiv::FarPatchTables;

	const OsdDrawContext::PatchArrayVector &patches =
		mesh->GetDrawContext()->patchArrays;

#ifndef OPENSUBDIV_LEGACY_DRAW
	if (fill_quads) {
		int model;
		glGetIntegerv(GL_SHADE_MODEL, &model);
		if (model == GL_FLAT) {
			glUseProgram(flat_fill_program);
		}
		else {
			glUseProgram(smooth_fill_program);
		}
	}
	else {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		/* TODO(sergey): For some reason this doesn't work on
		 * my Intel card and gives random color instead of the
		 * one set by glColor().
		 */
		glUseProgram(wireframe_program);
	}
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
			glDrawElements(GL_QUADS,
			               patch.GetNumIndices(),
			               GL_UNSIGNED_INT,
			               NULL);
		}
	}

	/* Restore state. */
	if (!fill_quads) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
	glBindVertexArray(0);
	/* TODO(sergey): Store previously used program and roll back to it? */
	glUseProgram(0);
}
