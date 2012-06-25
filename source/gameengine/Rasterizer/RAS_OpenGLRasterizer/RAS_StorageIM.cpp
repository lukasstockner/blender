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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "RAS_StorageIM.h"

#include <GL/glew.h>
#include "GPU_draw.h"
#include "GPU_extensions.h"
#include "GPU_material.h"

#include "DNA_meshdata_types.h"

#include "BKE_DerivedMesh.h"

#include "GPU_compatibility.h"

RAS_StorageIM::RAS_StorageIM(int *texco_num, RAS_IRasterizer::TexCoGen *texco, int *attrib_num, RAS_IRasterizer::TexCoGen *attrib) :
	m_texco_num(texco_num),
	m_texco(texco),
	m_attrib_num(attrib_num),
	m_attrib(attrib)
{
}
RAS_StorageIM::~RAS_StorageIM()
{
}

bool RAS_StorageIM::Init()
{
	return true;
}
void RAS_StorageIM::Exit()
{
}

void RAS_StorageIM::IndexPrimitives(RAS_MeshSlot& ms)
{
	IndexPrimitivesInternal(ms, false);
}

void RAS_StorageIM::IndexPrimitivesMulti(class RAS_MeshSlot& ms)
{
	IndexPrimitivesInternal(ms, true);
}

void RAS_StorageIM::TexCoord(const RAS_TexVert &tv)
{
	int unit;

	for (unit = 0; unit < *m_texco_num; unit++) {
		switch(m_texco[unit]) {
			case RAS_IRasterizer::RAS_TEXCO_ORCO:
			case RAS_IRasterizer::RAS_TEXCO_GLOB:
				gpuMultiTexCoord3fv(unit, tv.getXYZ());
				break;
			case RAS_IRasterizer::RAS_TEXCO_UV:
				gpuMultiTexCoord2fv(unit, tv.getUV(unit));
				break;
			case RAS_IRasterizer::RAS_TEXCO_NORM:
				gpuMultiTexCoord3fv(unit, tv.getNormal());
				break;
			case RAS_IRasterizer::RAS_TEXTANGENT:
				gpuMultiTexCoord4fv(unit, tv.getTangent());
				break;
			default:
				break;
		}
	}

	if (*m_attrib_num > 0) {
		int uv = 0;

		int index_f  = 0;
		int index_ub = 0;

		for (unit = 0; unit < *m_attrib_num; unit++) {
			switch(m_attrib[unit]) {
				case RAS_IRasterizer::RAS_TEXCO_ORCO:
				case RAS_IRasterizer::RAS_TEXCO_GLOB:
					gpuVertexAttrib3fv(index_f++, tv.getXYZ());
					break;
				case RAS_IRasterizer::RAS_TEXCO_UV:
					gpuVertexAttrib2fv(index_f++, tv.getUV(uv++));
					break;
				case RAS_IRasterizer::RAS_TEXCO_NORM:
					gpuVertexAttrib3fv(index_f++, tv.getNormal());
					break;
				case RAS_IRasterizer::RAS_TEXTANGENT:
					gpuVertexAttrib4fv(index_f++, tv.getTangent());
					break;
				case RAS_IRasterizer::RAS_TEXCO_VCOL:
					gpuVertexAttrib4ubv(index_ub++, tv.getRGBA());
					break;
				default:
					break;
			}
		}
	}
}

void RAS_StorageIM::SetCullFace(bool enable)
{
	if (enable)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);
}

static bool current_wireframe;
static RAS_MaterialBucket *current_bucket;
static RAS_IPolyMaterial *current_polymat;
static RAS_MeshSlot *current_ms;
static RAS_MeshObject *current_mesh;
static int current_blmat_nr;
static GPUVertexAttribs current_gpu_attribs;
static Image *current_image;
static int CheckMaterialDM(int matnr, void *attribs)
{
	// only draw the current material
	if (matnr != current_blmat_nr)
		return 0;
	GPUVertexAttribs *gattribs = (GPUVertexAttribs *)attribs;
	if (gattribs)
		memcpy(gattribs, &current_gpu_attribs, sizeof(GPUVertexAttribs));
	return 1;
}

/*
static int CheckTexfaceDM(void *mcol, int index)
{

	// index is the original face index, retrieve the polygon
	RAS_Polygon* polygon = (index >= 0 && index < current_mesh->NumPolygons()) ?
		current_mesh->GetPolygon(index) : NULL;
	if (polygon && polygon->GetMaterial() == current_bucket) {
		// must handle color.
		if (current_wireframe)
			return 2;
		if (current_ms->m_bObjectColor) {
			MT_Vector4& rgba = current_ms->m_RGBAcolor;
			glColor4d(rgba[0], rgba[1], rgba[2], rgba[3]);
			// don't use mcol
			return 2;
		}
		if (!mcol) {
			// we have to set the color from the material
			unsigned char rgba[4];
			current_polymat->GetMaterialRGBAColor(rgba);
			glColor4ubv((const GLubyte *)rgba);
			return 2;
		}
		return 1;
	}
	return 0;
}
*/

static DMDrawOption CheckTexDM(MTFace *tface, int has_mcol, int matnr)
{

	// index is the original face index, retrieve the polygon
	if (matnr == current_blmat_nr &&
		(tface == NULL || tface->tpage == current_image)) {
		// must handle color.
		if (current_wireframe)
			return DM_DRAW_OPTION_NO_MCOL;
		if (current_ms->m_bObjectColor) {
			MT_Vector4& rgba = current_ms->m_RGBAcolor;
			gpuCurrentColor4d(rgba[0], rgba[1], rgba[2], rgba[3]);
			// don't use mcol
			return DM_DRAW_OPTION_NO_MCOL;
		}
		if (!has_mcol) {
			// we have to set the color from the material
			unsigned char rgba[4];
			current_polymat->GetMaterialRGBAColor(rgba);
			gpuCurrentColor4ubv((const GLubyte *)rgba);
			return DM_DRAW_OPTION_NO_MCOL;
		}
		return DM_DRAW_OPTION_NORMAL;
	}
	return DM_DRAW_OPTION_SKIP;
}

void RAS_StorageIM::IndexPrimitivesInternal(RAS_MeshSlot& ms, bool multi)
{ 
	bool obcolor = ms.m_bObjectColor;
	bool wireframe = m_drawingmode <= RAS_IRasterizer::KX_WIREFRAME;
	MT_Vector4& rgba = ms.m_RGBAcolor;
	RAS_MeshSlot::iterator it;

	if (ms.m_pDerivedMesh) {
		// mesh data is in derived mesh, 
		current_bucket = ms.m_bucket;
		current_polymat = current_bucket->GetPolyMaterial();
		current_ms = &ms;
		current_mesh = ms.m_mesh;
		current_wireframe = wireframe;
		// MCol *mcol = (MCol*)ms.m_pDerivedMesh->getFaceDataArray(ms.m_pDerivedMesh, CD_MCOL); /* UNUSED */

		// handle two-side
		if (current_polymat->GetDrawingMode() & RAS_IRasterizer::KX_BACKCULL)
			this->SetCullFace(true);
		else
			this->SetCullFace(false);

		if (current_polymat->GetFlag() & RAS_BLENDERGLSL) {
			// GetMaterialIndex return the original mface material index, 
			// increment by 1 to match what derived mesh is doing
			current_blmat_nr = current_polymat->GetMaterialIndex()+1;
			// For GLSL we need to retrieve the GPU material attribute
			Material* blmat = current_polymat->GetBlenderMaterial();
			Scene* blscene = current_polymat->GetBlenderScene();
			if (!wireframe && blscene && blmat)
				GPU_material_vertex_attributes(GPU_material_from_blender(blscene, blmat), &current_gpu_attribs);
			else
				memset(&current_gpu_attribs, 0, sizeof(current_gpu_attribs));
			// DM draw can mess up blending mode, restore at the end
			int current_blend_mode = GPU_get_material_alpha_blend();
			ms.m_pDerivedMesh->drawFacesGLSL(ms.m_pDerivedMesh, CheckMaterialDM);
			GPU_set_material_alpha_blend(current_blend_mode);
		} else {
			//ms.m_pDerivedMesh->drawMappedFacesTex(ms.m_pDerivedMesh, CheckTexfaceDM, mcol);
			current_blmat_nr = current_polymat->GetMaterialIndex();
			current_image = current_polymat->GetBlenderImage();
			ms.m_pDerivedMesh->drawFacesTex(ms.m_pDerivedMesh, CheckTexDM, NULL, NULL);
		}
		return;
	}

	{
		static const GLenum tex[16] = {
			GL_TEXTURE0,  GL_TEXTURE1,  GL_TEXTURE2,  GL_TEXTURE3,
			GL_TEXTURE4,  GL_TEXTURE5,  GL_TEXTURE6,  GL_TEXTURE7,
			GL_TEXTURE8,  GL_TEXTURE9,  GL_TEXTURE10, GL_TEXTURE11,
			GL_TEXTURE12, GL_TEXTURE13, GL_TEXTURE14, GL_TEXTURE15,
		};

		GLint texSize[16];

		GLenum attrib_f[16];
		GLint attribSize_f[16];
		GLint count_f = 0;
		GLenum attrib_ub[16];
		GLint attribSize_ub[16];
		GLint count_ub = 0;

		int unit;

		GPU_ASSERT(*m_texco_num  <= 16);
		GPU_ASSERT(*m_attrib_num <= 16);

		gpuImmediateElementSizes(3, 3, 4);

		for (unit = 0; unit < *m_texco_num; unit++) {
			switch(m_texco[unit]) {
				case RAS_IRasterizer::RAS_TEXCO_ORCO:
				case RAS_IRasterizer::RAS_TEXCO_GLOB:
					texSize[unit] = 3;
					break;
				case RAS_IRasterizer::RAS_TEXCO_UV:
					texSize[unit] = 2;
					break;
				case RAS_IRasterizer::RAS_TEXCO_NORM:
					texSize[unit] = 3;
					break;
				case RAS_IRasterizer::RAS_TEXTANGENT:
					texSize[unit] = 4;
					break;
				default:
					break;
			}
		}

		gpuImmediateTextureUnitCount(*m_texco_num);
		gpuImmediateTexCoordSizes(texSize);
		gpuImmediateTextureUnitMap(tex);

		for (unit = 0; unit < *m_attrib_num; unit++) {
			switch(m_attrib[unit]) {
				case RAS_IRasterizer::RAS_TEXCO_ORCO:
				case RAS_IRasterizer::RAS_TEXCO_GLOB:
					attrib_f[count_f] = unit;
					attribSize_f[count_f] = 3;
					count_f++;
					break;
				case RAS_IRasterizer::RAS_TEXCO_UV:
					attrib_f[count_f] = unit;
					attribSize_f[count_f] = 2;
					count_f++;
					break;
				case RAS_IRasterizer::RAS_TEXCO_NORM:
					attrib_f[count_f] = unit;
					attribSize_f[count_f] = 3;
					count_f++;
					break;
				case RAS_IRasterizer::RAS_TEXTANGENT:
					attrib_f[count_f] = unit;
					attribSize_f[count_f] = 4;
					count_f++;
					break;
				case RAS_IRasterizer::RAS_TEXCO_VCOL:
					attrib_ub[count_ub] = unit;
					attribSize_ub[count_ub] = 4;
					count_ub++;
					break;
				default:
					break;
			}
		}

		gpuImmediateFloatAttribCount(count_f);
		gpuImmediateFloatAttribSizes(attribSize_f);
		gpuImmediateFloatAttribIndexMap(attrib_f);

		gpuImmediateUbyteAttribCount(count_ub);
		gpuImmediateUbyteAttribSizes(attribSize_ub);
		gpuImmediateUbyteAttribIndexMap(attrib_ub);
	}

	gpuImmediateLock();

	// iterate over display arrays, each containing an index + vertex array
	for (ms.begin(it); !ms.end(it); ms.next(it)) {
		RAS_TexVert *vertex;
		size_t i, j, numvert;
		
		numvert = it.array->m_type;

		if (it.array->m_type == RAS_DisplayArray::LINE) {
			// line drawing
			gpuBegin(GL_LINES);

			for (i = 0; i < it.totindex; i += 2)
			{
				vertex = &it.vertex[it.index[i]];
				gpuVertex3fv(vertex->getXYZ());

				vertex = &it.vertex[it.index[i+1]];
				gpuVertex3fv(vertex->getXYZ());
			}

			gpuEnd();
		}
		else {
			// triangle and quad drawing
			if (it.array->m_type == RAS_DisplayArray::TRIANGLE)
				gpuBegin(GL_TRIANGLES);
			else
				gpuBegin(GL_QUADS);

			for (i = 0; i < it.totindex; i += numvert)
			{
				if (obcolor)
					gpuColor4d(rgba[0], rgba[1], rgba[2], rgba[3]);

				for (j = 0; j < numvert; j++) {
					vertex = &it.vertex[it.index[i+j]];

					if (!wireframe) {
						if (!obcolor)
							gpuColor4ubv((const GLubyte *)(vertex->getRGBA()));

						gpuNormal3fv(vertex->getNormal());

						if (multi)
							TexCoord(*vertex);
						else
							gpuTexCoord2fv(vertex->getUV(0));
					}

					gpuVertex3fv(vertex->getXYZ());
				}
			}

			gpuEnd();
		}
	}

	gpuImmediateUnlock();
}
