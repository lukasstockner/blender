/*
 * $Id$
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "RAS_StorageVBO.h"

#include "GL/glew.h"

RAS_StorageVBO::RAS_StorageVBO(RAS_IRasterizer *rasty, int *texco_num, RAS_IRasterizer::TexCoGen *texco, int *attrib_num, RAS_IRasterizer::TexCoGen *attrib) :
	m_texco_num(texco_num),
	m_texco(texco),
	m_attrib_num(attrib_num),
	m_attrib(attrib),
	m_last_texco_num(0),
	m_last_attrib_num(0),
	m_rasty(rasty)
{
}

RAS_StorageVBO::~RAS_StorageVBO()
{
}

bool RAS_StorageVBO::Init()
{
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	return true;
}

void RAS_StorageVBO::Exit()
{
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
}

void RAS_StorageVBO::ClearVboSlot(class RAS_VboSlot *slot)
{
	if(!GLEW_ARB_vertex_buffer_object) return;
	if(slot->m_verts) delete slot->m_verts;
	if(slot->m_vertVbo) glDeleteBuffers(1, &slot->m_vertVbo);
	if(slot->m_indexVbo) glDeleteBuffers(1, &slot->m_indexVbo);
	if(slot->m_normalVbo) glDeleteBuffers(1, &slot->m_normalVbo);
	if(slot->m_tangentVbo) glDeleteBuffers(1, &slot->m_tangentVbo);
	if(slot->m_colorVbo) glDeleteBuffers(1, &slot->m_colorVbo);
	if(slot->m_texCoordVbo[0]) glDeleteBuffers(1, &slot->m_texCoordVbo[0]);
	if(slot->m_texCoordVbo[1]) glDeleteBuffers(1, &slot->m_texCoordVbo[1]);

	slot->m_verts = 0;
	slot->m_vertVbo = 0;
	slot->m_indexVbo = 0;
	slot->m_normalVbo = 0;
	slot->m_tangentVbo = 0;
	slot->m_colorVbo = 0;
	slot->m_texCoordVbo[0] = 0;
	slot->m_texCoordVbo[1] = 0;
}

void RAS_StorageVBO::InitVboSlot(class RAS_DisplayArray* array, class RAS_MeshSlot *ms)
{
	if(array->m_vertex.size() == 0 || array->m_index.size() == 0 || !GLEW_ARB_vertex_buffer_object)
		return;

	// clean up any previous data before creating a new slot
	if(array->m_vboSlot) delete array->m_vboSlot;
	array->m_vboSlot = new RAS_VboSlot(m_rasty);

	/* uploading data to vertex buffer objects doesn't allow
	stride so we have to grab the data from RAS_TexVert and
	initialize new arrays which we then use to upload the
	data to the vbos*/
	float *normals = 0;
	float *tangents = 0;
	float *texCoords0 = 0;
	float *texCoords1 = 0;
	unsigned char *colors = 0;
	bool isColors = glIsEnabled(GL_COLOR_ARRAY);
	array->m_vboSlot->m_verts = new float[array->m_vertex.size()*3];
	normals = new float[array->m_vertex.size()*3];
	tangents = new float[array->m_vertex.size()*4];
	texCoords0 = new float[array->m_vertex.size()*2];
	texCoords1 = new float[array->m_vertex.size()*2];
	colors = new unsigned char[array->m_vertex.size()*4];

	// upload the data
	unsigned int num0 = 0;
	unsigned int num1 = 0;
	unsigned int num2 = 0;
	unsigned int vertnum = 0;
	for(vertnum = 0; vertnum < array->m_vertex.size(); vertnum++)
	{
		memcpy(&array->m_vboSlot->m_verts[num0], array->m_vertex[vertnum].getXYZ(), sizeof(float)*3);
		memcpy(&normals[num0], array->m_vertex[vertnum].getNormal(), sizeof(float)*3);
		memcpy(&texCoords0[num1], array->m_vertex[vertnum].getUV1(), sizeof(float)*2);
		memcpy(&texCoords1[num1], array->m_vertex[vertnum].getUV2(), sizeof(float)*2);
		memcpy(&tangents[num2], array->m_vertex[vertnum].getTangent(), sizeof(float)*4);
		memcpy(&colors[num2], array->m_vertex[vertnum].getRGBA(), sizeof(int));
		num0+=3;
		num1+=2;
		num2+=4;
	}

	/* Vertex VBO */
	glGenBuffersARB(1, &array->m_vboSlot->m_vertVbo);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, array->m_vboSlot->m_vertVbo);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(float)*array->m_vertex.size()*3,
		array->m_vboSlot->m_verts, GL_DYNAMIC_DRAW_ARB);

	/* Normal VBO */
	glGenBuffersARB(1, &array->m_vboSlot->m_normalVbo);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, array->m_vboSlot->m_normalVbo);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(float)*array->m_vertex.size()*3,
		normals, GL_DYNAMIC_DRAW_ARB);

	/* Tangent VBO */
	glGenBuffersARB(1, &array->m_vboSlot->m_tangentVbo);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, array->m_vboSlot->m_tangentVbo);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(float)*array->m_vertex.size()*4,
		tangents, GL_STATIC_DRAW_ARB);

	/* Tex Coord VBO 1 */
	glGenBuffersARB(1, &array->m_vboSlot->m_texCoordVbo[0]);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, array->m_vboSlot->m_texCoordVbo[0]);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(float)*array->m_vertex.size()*2,
		texCoords0, GL_STATIC_DRAW_ARB);

	/* Tex Coord VBO 2 */
	glGenBuffersARB(1, &array->m_vboSlot->m_texCoordVbo[1]);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, array->m_vboSlot->m_texCoordVbo[1]);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(float)*array->m_vertex.size()*2,
		texCoords1, GL_STATIC_DRAW_ARB);

	/* Color VBO */
	if(isColors)
	{
		glGenBuffersARB(1, &array->m_vboSlot->m_colorVbo);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, array->m_vboSlot->m_colorVbo);
		glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(unsigned char)*array->m_vertex.size()*4,
			colors, GL_STATIC_DRAW_ARB);
	}

	/* Index VBO */
	glGenBuffersARB(1, &array->m_vboSlot->m_indexVbo);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, array->m_vboSlot->m_indexVbo);
	glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sizeof(unsigned short)*array->m_index.size(),
		&array->m_index[0], GL_STATIC_DRAW_ARB);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);

	// clean up
	delete[] array->m_vboSlot->m_verts;
	array->m_vboSlot->m_verts = 0;
	delete[] normals;
	delete[] texCoords0;
	delete[] texCoords1;
	delete[] colors;
}

void RAS_StorageVBO::IndexPrimitives(RAS_MeshSlot& ms)
{
	static const GLsizei stride = sizeof(RAS_TexVert);
	bool wireframe = m_drawingmode <= RAS_IRasterizer::KX_WIREFRAME;
	RAS_MeshSlot::iterator it;
	GLenum drawmode;

	if(!wireframe)
		EnableTextures(true);

	// use glDrawElements to draw each vertexarray
	for(ms.begin(it); !ms.end(it); ms.next(it)) {
		if(it.totindex == 0)
			continue;

		// drawing mode
		if(it.array->m_type == RAS_DisplayArray::TRIANGLE)
			drawmode = GL_TRIANGLES;
		else if(it.array->m_type == RAS_DisplayArray::QUAD)
			drawmode = GL_QUADS;
		else
			drawmode = GL_LINES;

		// colors
		if (drawmode != GL_LINES && !wireframe) {
			if (ms.m_bObjectColor) {
				const MT_Vector4& rgba = ms.m_RGBAcolor;

				glDisableClientState(GL_COLOR_ARRAY);
				glColor4d(rgba[0], rgba[1], rgba[2], rgba[3]);
			}
			else {
				glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
				glEnableClientState(GL_COLOR_ARRAY);
			}
		}
		else
			glColor4f(0.0f, 0.0f, 0.0f, 1.0f);

		if(!it.array->m_vboSlot)
			InitVboSlot(it.array, &ms);

		if(it.array->m_vboSlot)
		{
			glBindBuffer(GL_ARRAY_BUFFER_ARB, it.array->m_vboSlot->m_vertVbo);
			glVertexPointer(3, GL_FLOAT, 0, 0);
			glBindBuffer(GL_ARRAY_BUFFER_ARB, it.array->m_vboSlot->m_normalVbo);
			glNormalPointer(GL_FLOAT, 0, 0);

			if(!wireframe) {
				glBindBuffer(GL_ARRAY_BUFFER_ARB, it.array->m_vboSlot->m_texCoordVbo[0]);
				glTexCoordPointer(2, GL_FLOAT, 0, 0);
				if(glIsEnabled(GL_COLOR_ARRAY))
				{
					glBindBuffer(GL_ARRAY_BUFFER_ARB, it.array->m_vboSlot->m_colorVbo);
					glColorPointer(4, GL_UNSIGNED_BYTE, 0, 0);
				}
			}

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER_ARB, it.array->m_vboSlot->m_indexVbo);

			// a little clean up...
			glDrawElements(drawmode, it.totindex, GL_UNSIGNED_SHORT, 0);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
			glBindBuffer(GL_ARRAY_BUFFER_ARB, 0);
			glVertexPointer(3, GL_FLOAT, 0, 0);
			glNormalPointer(GL_FLOAT, 0, 0);

			if(!wireframe) {
				glTexCoordPointer(2, GL_FLOAT, 0, 0);
				if(glIsEnabled(GL_COLOR_ARRAY))
					glColorPointer(4, GL_UNSIGNED_BYTE, 0, 0);
			}
		}

		if(!wireframe) {
			glDisableClientState(GL_COLOR_ARRAY);
			EnableTextures(false);
		}
	}
}

void RAS_StorageVBO::IndexPrimitivesMulti(class RAS_MeshSlot& ms)
{
	static const GLsizei stride = sizeof(RAS_TexVert);
	bool wireframe = m_drawingmode <= RAS_IRasterizer::KX_WIREFRAME;
	RAS_MeshSlot::iterator it;
	GLenum drawmode;
	unsigned int unit;

	if(!wireframe)
		EnableTextures(true);

	// use glDrawElements to draw each vertexarray
	for(ms.begin(it); !ms.end(it); ms.next(it)) {
		if(it.totindex == 0)
			continue;

		// drawing mode
		if(it.array->m_type == RAS_DisplayArray::TRIANGLE)
			drawmode = GL_TRIANGLES;
		else if(it.array->m_type == RAS_DisplayArray::QUAD)
			drawmode = GL_QUADS;
		else
			drawmode = GL_LINES;

		// colors
		if (drawmode != GL_LINES && !wireframe) {
			if (ms.m_bObjectColor) {
				const MT_Vector4& rgba = ms.m_RGBAcolor;

				glDisableClientState(GL_COLOR_ARRAY);
				glColor4d(rgba[0], rgba[1], rgba[2], rgba[3]);
			}
			else {
				glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
				glEnableClientState(GL_COLOR_ARRAY);
			}
		}
		else
			glColor4f(0.0f, 0.0f, 0.0f, 1.0f);

		if(!it.array->m_vboSlot) 
			InitVboSlot(it.array, &ms);

		if(it.array->m_vboSlot)
		{
			glBindBuffer(GL_ARRAY_BUFFER_ARB, it.array->m_vboSlot->m_vertVbo);
			glVertexPointer(3, GL_FLOAT, 0, 0);
			glBindBuffer(GL_ARRAY_BUFFER_ARB, it.array->m_vboSlot->m_normalVbo);
			glNormalPointer(GL_FLOAT, 0, 0);

			if(!wireframe) {
				TexCoordPtr(it.array);
				if(glIsEnabled(GL_COLOR_ARRAY))
				{
					glBindBuffer(GL_ARRAY_BUFFER_ARB, it.array->m_vboSlot->m_colorVbo);
					glColorPointer(4, GL_UNSIGNED_BYTE, 0, 0);
				}
			}

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER_ARB, it.array->m_vboSlot->m_indexVbo);

			// a little clean up...
			glDrawElements(drawmode, it.totindex, GL_UNSIGNED_SHORT, 0);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
			glBindBuffer(GL_ARRAY_BUFFER_ARB, 0);
			glVertexPointer(3, GL_FLOAT, 0, 0);
			glNormalPointer(GL_FLOAT, 0, 0);

			if(!wireframe) {
				for(unit=0; unit<*m_texco_num; unit++) {
					glClientActiveTextureARB(GL_TEXTURE0_ARB+unit);
					glBindBuffer(GL_ARRAY_BUFFER_ARB, 0);
					glTexCoordPointer(2, GL_FLOAT, 0, 0);
				}
				if(glIsEnabled(GL_COLOR_ARRAY))
					glColorPointer(4, GL_UNSIGNED_BYTE, 0, 0);
			}

		}

		if(!wireframe) {
			glDisableClientState(GL_COLOR_ARRAY);
			EnableTextures(false);
		}
	}
}

void RAS_StorageVBO::TexCoordPtr(class RAS_DisplayArray *array)
{
	/* note: this function must closely match EnableTextures to enable/disable
	 * the right arrays, otherwise coordinate and attribute pointers from other
	 * materials can still be used and cause crashes */
	int unit;
	static RAS_TexVert *vertex = 0;
	vertex = &array->m_vertex[0];

	if(GLEW_ARB_multitexture)
	{
		for(unit=0; unit<*m_texco_num; unit++)
		{
			glClientActiveTextureARB(GL_TEXTURE0_ARB+unit);
			if(vertex->getFlag() & RAS_TexVert::SECOND_UV && (int)vertex->getUnit() == unit) {
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);

				if(GLEW_ARB_vertex_buffer_object && array->m_vboSlot) {
					glBindBufferARB(GL_ARRAY_BUFFER_ARB, array->m_vboSlot->m_texCoordVbo[1]);
					glTexCoordPointer(2, GL_FLOAT, 0, 0);
				} else
					glTexCoordPointer(2, GL_FLOAT, sizeof(RAS_TexVert), vertex->getUV2());

				continue;
			}
			switch(m_texco[unit])
			{
			case RAS_IRasterizer::RAS_TEXCO_ORCO:
			case RAS_IRasterizer::RAS_TEXCO_GLOB:
				if(GLEW_ARB_vertex_buffer_object && array->m_vboSlot) {
					glBindBufferARB(GL_ARRAY_BUFFER_ARB, array->m_vboSlot->m_vertVbo);
					glTexCoordPointer(3, GL_FLOAT, 0, 0);
				} else
					glTexCoordPointer(3, GL_FLOAT, sizeof(RAS_TexVert), vertex->getXYZ());
				break;
			case RAS_IRasterizer::RAS_TEXCO_UV1:
				if(GLEW_ARB_vertex_buffer_object && array->m_vboSlot) {
					glBindBufferARB(GL_ARRAY_BUFFER_ARB, array->m_vboSlot->m_texCoordVbo[0]);
					glTexCoordPointer(2, GL_FLOAT, 0, 0);
				} else
					glTexCoordPointer(2, GL_FLOAT, sizeof(RAS_TexVert), vertex->getUV1());
				break;
			case RAS_IRasterizer::RAS_TEXCO_NORM:
				if(GLEW_ARB_vertex_buffer_object && array->m_vboSlot) {
					glBindBufferARB(GL_ARRAY_BUFFER_ARB, array->m_vboSlot->m_normalVbo);
					glTexCoordPointer(3, GL_FLOAT, 0, 0);
				} else
					glTexCoordPointer(3, GL_FLOAT, sizeof(RAS_TexVert), vertex->getNormal());
				break;
			case RAS_IRasterizer::RAS_TEXTANGENT:
				if(GLEW_ARB_vertex_buffer_object && array->m_vboSlot) {
					glBindBufferARB(GL_ARRAY_BUFFER_ARB, array->m_vboSlot->m_tangentVbo);
					glTexCoordPointer(4, GL_FLOAT, 0, 0);
				} else
					glTexCoordPointer(4, GL_FLOAT, sizeof(RAS_TexVert), vertex->getTangent());
				break;
			case RAS_IRasterizer::RAS_TEXCO_UV2:
				if(GLEW_ARB_vertex_buffer_object && array->m_vboSlot) {
					glBindBufferARB(GL_ARRAY_BUFFER_ARB, array->m_vboSlot->m_texCoordVbo[1]);
					glTexCoordPointer(2, GL_FLOAT, 0, 0);
				} else
					glTexCoordPointer(2, GL_FLOAT, sizeof(RAS_TexVert), vertex->getUV2());
				break;
			default:
				break;
			}
		}

		glClientActiveTextureARB(GL_TEXTURE0_ARB);
	}

	if(GLEW_ARB_vertex_program) {
		for(unit=0; unit<*m_attrib_num; unit++) {
			switch(m_attrib[unit]) {
			case RAS_IRasterizer::RAS_TEXCO_ORCO:
			case RAS_IRasterizer::RAS_TEXCO_GLOB:
				if(GLEW_ARB_vertex_buffer_object && array->m_vboSlot) {
					glBindBufferARB(GL_ARRAY_BUFFER_ARB, array->m_vboSlot->m_vertVbo);
					glVertexAttribPointerARB(unit, 3, GL_FLOAT, GL_FALSE, 0, 0);
				} else
					glVertexAttribPointerARB(unit, 3, GL_FLOAT, GL_FALSE, sizeof(RAS_TexVert), vertex->getXYZ());
				break;
			case RAS_IRasterizer::RAS_TEXCO_UV1:
				if(GLEW_ARB_vertex_buffer_object && array->m_vboSlot) {
					glBindBufferARB(GL_ARRAY_BUFFER_ARB, array->m_vboSlot->m_texCoordVbo[0]);
					glVertexAttribPointerARB(unit, 2, GL_FLOAT, GL_FALSE, 0, 0);
				} else
					glVertexAttribPointerARB(unit, 2, GL_FLOAT, GL_FALSE, sizeof(RAS_TexVert), vertex->getUV1());
				break;
			case RAS_IRasterizer::RAS_TEXCO_NORM:
				if(GLEW_ARB_vertex_buffer_object && array->m_vboSlot) {
					glBindBufferARB(GL_ARRAY_BUFFER_ARB, array->m_vboSlot->m_normalVbo);
					glVertexAttribPointerARB(unit, 3, GL_FLOAT, GL_FALSE, 0, 0);
				} else
					glVertexAttribPointerARB(unit, 3, GL_FLOAT, GL_FALSE, sizeof(RAS_TexVert), vertex->getNormal());
				break;
			case RAS_IRasterizer::RAS_TEXTANGENT:
				if(GLEW_ARB_vertex_buffer_object && array->m_vboSlot) {
					glBindBufferARB(GL_ARRAY_BUFFER_ARB, array->m_vboSlot->m_tangentVbo);
					glVertexAttribPointerARB(unit, 4, GL_FLOAT, GL_FALSE, 0, 0);
				} else
					glVertexAttribPointerARB(unit, 4, GL_FLOAT, GL_FALSE, sizeof(RAS_TexVert), vertex->getTangent());
				break;
			case RAS_IRasterizer::RAS_TEXCO_UV2:
				if(GLEW_ARB_vertex_buffer_object && array->m_vboSlot) {
					glBindBufferARB(GL_ARRAY_BUFFER_ARB, array->m_vboSlot->m_texCoordVbo[1]);
					glVertexAttribPointerARB(unit, 2, GL_FLOAT, GL_FALSE, 0, 0);
				} else
					glVertexAttribPointerARB(unit, 2, GL_FLOAT, GL_FALSE, sizeof(RAS_TexVert), vertex->getUV2());
				break;
			case RAS_IRasterizer::RAS_TEXCO_VCOL:
				if(GLEW_ARB_vertex_buffer_object && array->m_vboSlot) {
					glBindBufferARB(GL_ARRAY_BUFFER_ARB, array->m_vboSlot->m_colorVbo);
					glVertexAttribPointerARB(unit, 4, GL_UNSIGNED_BYTE, GL_FALSE, 0, 0);
				} else
					glVertexAttribPointerARB(unit, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(RAS_TexVert), vertex->getRGBA());
				break;
			default:
				break;
			}
		}
	}
}

void RAS_StorageVBO::EnableTextures(bool enable)
{
	RAS_IRasterizer::TexCoGen *texco, *attrib;
	int unit, texco_num, attrib_num;

	/* we cache last texcoords and attribs to ensure we disable the ones that
	 * were actually last set */
	if(enable) {
		texco = m_texco;
		texco_num = *m_texco_num;
		attrib = m_attrib;
		attrib_num = *m_attrib_num;
		
		memcpy(m_last_texco, m_texco, sizeof(RAS_IRasterizer::TexCoGen)*(*m_texco_num));
		m_last_texco_num = *m_texco_num;
		memcpy(m_last_attrib, m_attrib, sizeof(RAS_IRasterizer::TexCoGen)*(*m_attrib_num));
		m_last_attrib_num = *m_attrib_num;
	}
	else {
		texco = m_last_texco;
		texco_num = m_last_texco_num;
		attrib = m_last_attrib;
		attrib_num = m_last_attrib_num;
	}

	if(GLEW_ARB_multitexture) {
		for(unit=0; unit<texco_num; unit++) {
			glClientActiveTextureARB(GL_TEXTURE0_ARB+unit);

			switch(texco[unit])
			{
			case RAS_IRasterizer::RAS_TEXCO_ORCO:
			case RAS_IRasterizer::RAS_TEXCO_GLOB:
			case RAS_IRasterizer::RAS_TEXCO_UV1:
			case RAS_IRasterizer::RAS_TEXCO_NORM:
			case RAS_IRasterizer::RAS_TEXTANGENT:
			case RAS_IRasterizer::RAS_TEXCO_UV2:
				if(enable) glEnableClientState(GL_TEXTURE_COORD_ARRAY);
				else glDisableClientState(GL_TEXTURE_COORD_ARRAY);
				break;
			default:
				glDisableClientState(GL_TEXTURE_COORD_ARRAY);
				break;
			}
		}

		glClientActiveTextureARB(GL_TEXTURE0_ARB);
	}
	else {
		if(texco_num) {
			if(enable) glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			else glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		}
	}

	if(GLEW_ARB_vertex_program) {
		for(unit=0; unit<attrib_num; unit++) {
			switch(attrib[unit]) {
			case RAS_IRasterizer::RAS_TEXCO_ORCO:
			case RAS_IRasterizer::RAS_TEXCO_GLOB:
			case RAS_IRasterizer::RAS_TEXCO_UV1:
			case RAS_IRasterizer::RAS_TEXCO_NORM:
			case RAS_IRasterizer::RAS_TEXTANGENT:
			case RAS_IRasterizer::RAS_TEXCO_UV2:
			case RAS_IRasterizer::RAS_TEXCO_VCOL:
				if(enable) glEnableVertexAttribArrayARB(unit);
				else glDisableVertexAttribArrayARB(unit);
				break;
			default:
				glDisableVertexAttribArrayARB(unit);
				break;
			}
		}
	}

	if(!enable) {
		m_last_texco_num = 0;
		m_last_attrib_num = 0;
	}
}

