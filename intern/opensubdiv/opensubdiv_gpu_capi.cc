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

#include <GL/glew.h>
#include <vector>

#include <opensubdiv/osd/glMesh.h>

#include <opensubdiv/osd/cudaGLVertexBuffer.h>
#include <opensubdiv/osd/glDrawRegistry.h>

// **************** Types declaration ****************

using OpenSubdiv::OsdDrawContext;
using OpenSubdiv::OsdGLDrawRegistry;
using OpenSubdiv::OsdGLMeshInterface;

class EffectDrawRegistry :
	public OsdGLDrawRegistry<OsdDrawContext::PatchDescriptor> {
protected:
	virtual ConfigType *_CreateDrawConfig(DescType const &desc,
	                                      SourceConfigType const *sconfig);

	virtual SourceConfigType *_CreateDrawSourceConfig(DescType const &desc);
};

// TODO(sergey): Fine for tests, but ideally need to be stored
// in some sort of object draw context.
static GLuint g_transformUB = 0,
              g_transformBinding = 0,
              g_tessellationUB = 0,
              g_tessellationBinding = 0,
              g_lightingUB = 0,
              g_lightingBinding = 0;

static struct Transform {
	float ModelViewMatrix[16];
	float ProjectionMatrix[16];
	float ModelViewProjectionMatrix[16];
} g_transformData;

static struct Program {
	GLuint program;
	GLuint uniformModelViewProjectionMatrix;
	GLuint attrPosition;
	GLuint attrColor;
} g_defaultProgram;

static EffectDrawRegistry g_effectRegistry;

extern "C" char datatoc_gpu_shader_opensubd_display_glsl[];

static void checkGLErrors(std::string const &where = "")
{
	GLuint err;
	while ((err = glGetError()) != GL_NO_ERROR) {
		std::cerr << "GL error: "
		          << (where.empty() ? "" : where + " ")
		          << err << "\n";
	}
}

static GLuint compileShader(GLenum shaderType, const char *source)
{
	GLuint shader = glCreateShader(shaderType);
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);
	checkGLErrors("compileShader");
	return shader;
}

static bool linkDefaultProgram()
{
#define GLSL_VERSION_DEFINE "#version 400\n"

	static const char *vsSrc =
		GLSL_VERSION_DEFINE
		"in vec3 position;\n"
		"in vec3 color;\n"
		"out vec4 fragColor;\n"
		"uniform mat4 ModelViewProjectionMatrix;\n"
		"void main() {\n"
		"  fragColor = vec4(color, 1);\n"
		"  gl_Position = ModelViewProjectionMatrix * "
		"                  vec4(position, 1);\n"
		"}\n";

	static const char *fsSrc =
		GLSL_VERSION_DEFINE
		"in vec4 fragColor;\n"
		"out vec4 color;\n"
		"void main() {\n"
		"  color = fragColor;\n"
		"}\n";

	GLuint program = glCreateProgram();
	GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vsSrc);
	GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fsSrc);

	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);

	glLinkProgram(program);

	GLint status;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE) {
		GLint infoLogLength;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLength);
		char *infoLog = new char[infoLogLength];
		glGetProgramInfoLog(program, infoLogLength, NULL, infoLog);
		printf("%s\n", infoLog);
		delete[] infoLog;
		exit(1);
	}

	g_defaultProgram.program = program;
	g_defaultProgram.uniformModelViewProjectionMatrix =
		glGetUniformLocation(program, "ModelViewProjectionMatrix");
	g_defaultProgram.attrPosition = glGetAttribLocation(program, "position");
	g_defaultProgram.attrColor = glGetAttribLocation(program, "color");

	return true;
}

EffectDrawRegistry::SourceConfigType*
EffectDrawRegistry::_CreateDrawSourceConfig(DescType const &desc)
{
	SourceConfigType *sconfig = BaseRegistry::_CreateDrawSourceConfig(desc);
	const char *shaderSource = datatoc_gpu_shader_opensubd_display_glsl;

	assert(sconfig);

	const char *glslVersion = "#version 400\n";

	if (desc.GetType() == OpenSubdiv::FarPatchTables::QUADS or
	    desc.GetType() == OpenSubdiv::FarPatchTables::TRIANGLES)
	{
		sconfig->vertexShader.source = shaderSource;
		sconfig->vertexShader.version = glslVersion;
		sconfig->vertexShader.AddDefine("VERTEX_SHADER");
	} else {
		sconfig->geometryShader.AddDefine("SMOOTH_NORMALS");
    }

	sconfig->geometryShader.source = shaderSource;
	sconfig->geometryShader.version = glslVersion;
	sconfig->geometryShader.AddDefine("GEOMETRY_SHADER");

	sconfig->fragmentShader.source = shaderSource;
	sconfig->fragmentShader.version = glslVersion;
	sconfig->fragmentShader.AddDefine("FRAGMENT_SHADER");

	if (desc.GetType() == OpenSubdiv::FarPatchTables::QUADS) {
		// uniform catmark, bilinear
		sconfig->geometryShader.AddDefine("PRIM_QUAD");
		sconfig->fragmentShader.AddDefine("PRIM_QUAD");
		sconfig->commonShader.AddDefine("UNIFORM_SUBDIVISION");
	} else if (desc.GetType() == OpenSubdiv::FarPatchTables::TRIANGLES) {
		// uniform loop
		sconfig->geometryShader.AddDefine("PRIM_TRI");
		sconfig->fragmentShader.AddDefine("PRIM_TRI");
		sconfig->commonShader.AddDefine("LOOP");
		sconfig->commonShader.AddDefine("UNIFORM_SUBDIVISION");
	} else {
		// adaptive
		sconfig->vertexShader.source = shaderSource + sconfig->vertexShader.source;
		sconfig->tessControlShader.source = shaderSource + sconfig->tessControlShader.source;
		sconfig->tessEvalShader.source = shaderSource + sconfig->tessEvalShader.source;

		sconfig->geometryShader.AddDefine("PRIM_TRI");
		sconfig->fragmentShader.AddDefine("PRIM_TRI");
    }

	// TODO(sergey): Currently unsupported, but good to have for the reference.
#if 0
	if (screenSpaceTess) {
		sconfig->commonShader.AddDefine("OSD_ENABLE_SCREENSPACE_TESSELLATION");
	}
	if (fractionalSpacing) {
		sconfig->commonShader.AddDefine("OSD_FRACTIONAL_ODD_SPACING");
	}
	if (patchCull) {
		sconfig->commonShader.AddDefine("OSD_ENABLE_PATCH_CULL");
	}
#endif

	sconfig->commonShader.AddDefine("GEOMETRY_OUT_FILL");

	return sconfig;
}

EffectDrawRegistry::ConfigType *
EffectDrawRegistry::_CreateDrawConfig(DescType const &desc,
                                      SourceConfigType const *sconfig)
{
	ConfigType *config = BaseRegistry::_CreateDrawConfig(desc, sconfig);
	delete sconfig;
	assert(config);

	GLuint uboIndex;

	g_transformBinding = 0;
	uboIndex = glGetUniformBlockIndex(config->program, "Transform");
	if (uboIndex != GL_INVALID_INDEX) {
        glUniformBlockBinding(config->program, uboIndex, g_transformBinding);
	}

	g_tessellationBinding = 1;
	uboIndex = glGetUniformBlockIndex(config->program, "Tessellation");
	if (uboIndex != GL_INVALID_INDEX) {
        glUniformBlockBinding(config->program, uboIndex, g_tessellationBinding);
	}

	g_lightingBinding = 2;
	uboIndex = glGetUniformBlockIndex(config->program, "Lighting");
	if (uboIndex != GL_INVALID_INDEX) {
        glUniformBlockBinding(config->program, uboIndex, g_lightingBinding);
	}

	GLint loc;
	glUseProgram(config->program);
	if ((loc = glGetUniformLocation(config->program, "OsdVertexBuffer")) != -1) {
		glUniform1i(loc, 0); // GL_TEXTURE0
	}
	if ((loc = glGetUniformLocation(config->program, "OsdValenceBuffer")) != -1) {
		glUniform1i(loc, 1); // GL_TEXTURE1
	}
	if ((loc = glGetUniformLocation(config->program, "OsdQuadOffsetBuffer")) != -1) {
		glUniform1i(loc, 2); // GL_TEXTURE2
	}
	if ((loc = glGetUniformLocation(config->program, "OsdPatchParamBuffer")) != -1) {
		glUniform1i(loc, 3); // GL_TEXTURE3
	}
	if ((loc = glGetUniformLocation(config->program, "OsdFVarDataBuffer")) != -1) {
		glUniform1i(loc, 4); // GL_TEXTURE4
	}

	return config;
}

static GLuint bindProgram(OsdGLMeshInterface *mesh,
                          OsdDrawContext::PatchArray const &patch,
                          int level)
{
	EffectDrawRegistry::ConfigType *config =
		g_effectRegistry.GetDrawConfig(patch.GetDescriptor());

	GLuint program = config->program;

	glUseProgram(program);

	if (! g_transformUB) {
		glGenBuffers(1, &g_transformUB);
		glBindBuffer(GL_UNIFORM_BUFFER, g_transformUB);
		glBufferData(GL_UNIFORM_BUFFER,
		             sizeof(g_transformData), NULL, GL_STATIC_DRAW);
	};
	glBindBuffer(GL_UNIFORM_BUFFER, g_transformUB);
	glBufferSubData(GL_UNIFORM_BUFFER,
	                0, sizeof(g_transformData), &g_transformData);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	glBindBufferBase(GL_UNIFORM_BUFFER, g_transformBinding, g_transformUB);

	// Update and bind tessellation state
	struct Tessellation {
		float TessLevel;
	} tessellationData;

	tessellationData.TessLevel = static_cast<float>(1 << level);

	if (! g_tessellationUB) {
		glGenBuffers(1, &g_tessellationUB);
		glBindBuffer(GL_UNIFORM_BUFFER, g_tessellationUB);
		glBufferData(GL_UNIFORM_BUFFER,
		             sizeof(tessellationData), NULL, GL_STATIC_DRAW);
	};
	glBindBuffer(GL_UNIFORM_BUFFER, g_tessellationUB);
	glBufferSubData(GL_UNIFORM_BUFFER,
	                0, sizeof(tessellationData), &tessellationData);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	glBindBufferBase(GL_UNIFORM_BUFFER, g_tessellationBinding, g_tessellationUB);

	// Update and bind lighting state
	struct Lighting {
		struct Light {
			float position[4];
			float ambient[4];
			float diffuse[4];
			float specular[4];
		} lightSource[2];
	} lightingData = {
		{{  { 0.5,  0.2f, 1.0f, 0.0f },
		    { 0.1f, 0.1f, 0.1f, 1.0f },
		    { 0.7f, 0.7f, 0.7f, 1.0f },
		    { 0.8f, 0.8f, 0.8f, 1.0f } },

		 { { -0.8f, 0.4f, -1.0f, 0.0f },
		   {  0.0f, 0.0f,  0.0f, 1.0f },
		   {  0.5f, 0.5f,  0.5f, 1.0f },
		   {  0.8f, 0.8f,  0.8f, 1.0f } }}
	};
	if (! g_lightingUB) {
		glGenBuffers(1, &g_lightingUB);
		glBindBuffer(GL_UNIFORM_BUFFER, g_lightingUB);
		glBufferData(GL_UNIFORM_BUFFER,
		             sizeof(lightingData), NULL, GL_STATIC_DRAW);
	};
	glBindBuffer(GL_UNIFORM_BUFFER, g_lightingUB);
	glBufferSubData(GL_UNIFORM_BUFFER,
	                0, sizeof(lightingData), &lightingData);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	glBindBufferBase(GL_UNIFORM_BUFFER, g_lightingBinding, g_lightingUB);

	if (mesh->GetDrawContext()->GetVertexTextureBuffer()) {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_BUFFER,
		              mesh->GetDrawContext()->GetVertexTextureBuffer());
	}
	if (mesh->GetDrawContext()->GetVertexValenceTextureBuffer()) {
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_BUFFER,
		              mesh->GetDrawContext()->GetVertexValenceTextureBuffer());
	}
	if (mesh->GetDrawContext()->GetQuadOffsetsTextureBuffer()) {
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_BUFFER,
		              mesh->GetDrawContext()->GetQuadOffsetsTextureBuffer());
	}
	if (mesh->GetDrawContext()->GetPatchParamTextureBuffer()) {
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_BUFFER,
		              mesh->GetDrawContext()->GetPatchParamTextureBuffer());
	}
	if (mesh->GetDrawContext()->GetFvarDataTextureBuffer()) {
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_BUFFER,
		              mesh->GetDrawContext()->GetFvarDataTextureBuffer());
	}

	glActiveTexture(GL_TEXTURE0);

	return program;
}

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

void openSubdiv_osdGLMeshDisplay(OpenSubdiv_GLMesh *gl_mesh)
{
	static bool need_init = true;
	OsdGLMeshInterface *mesh = (OsdGLMeshInterface *) gl_mesh->descriptor;

	if (need_init) {
		linkDefaultProgram();
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
		OsdDrawContext::PatchArray const & patch = patches[i];
		OpenSubdiv::OsdDrawContext::PatchDescriptor desc = patch.GetDescriptor();
		OpenSubdiv::FarPatchTables::Type patchType = desc.GetType();

		GLenum primType;
		switch(patchType) {
			case OpenSubdiv::FarPatchTables::QUADS:
				primType = GL_LINES_ADJACENCY;
				break;
			case OpenSubdiv::FarPatchTables::TRIANGLES:
				primType = GL_TRIANGLES;
				break;
			default:
				primType = GL_PATCHES;
				glPatchParameteri(GL_PATCH_VERTICES, desc.GetNumControlVertices());
		}

		GLuint program = bindProgram(mesh, patch, gl_mesh->level);
		GLuint diffuseColor = glGetUniformLocation(program, "diffuseColor");

		glProgramUniform4f(program, diffuseColor, 0.8f, 0.8f, 0.8f, 1);

		GLuint uniformGregoryQuadOffsetBase =
			glGetUniformLocation(program, "GregoryQuadOffsetBase");
		GLuint uniformPrimitiveIdBase =
			glGetUniformLocation(program, "PrimitiveIdBase");

		glProgramUniform1i(program, uniformGregoryQuadOffsetBase,
		                   patch.GetQuadOffsetIndex());
		glProgramUniform1i(program, uniformPrimitiveIdBase,
		                   patch.GetPatchIndex());

		glDrawElements(primType, patch.GetNumIndices(), GL_UNSIGNED_INT,
		               (void *)(patch.GetVertIndex() * sizeof(unsigned int)));
	}
	glBindVertexArray(0);
	glUseProgram(0);
}
