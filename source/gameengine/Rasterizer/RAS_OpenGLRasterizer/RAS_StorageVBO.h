/*
 * $Id$
 *
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

#ifndef __KX_VERTEXBUFFEROBJECTSTORAGE
#define __KX_VERTEXBUFFEROBJECTSTORAGE

#include "RAS_IStorage.h"
#include "RAS_IRasterizer.h"

#include "RAS_OpenGLRasterizer.h"

class RAS_StorageVBO : public RAS_IStorage
{

public:
	RAS_StorageVBO(RAS_IRasterizer *rasty, int *texco_num, RAS_IRasterizer::TexCoGen *texco, int *attrib_num, RAS_IRasterizer::TexCoGen *attrib);
	virtual ~RAS_StorageVBO();

	virtual bool	Init();
	virtual void	Exit();

	virtual void	IndexPrimitives(RAS_MeshSlot& ms);
	virtual void	IndexPrimitivesMulti(class RAS_MeshSlot& ms);

	virtual void	SetDrawingMode(int drawingmode){m_drawingmode=drawingmode;};

protected:
	int				m_drawingmode;

	int*			m_texco_num;
	int*			m_attrib_num;

	int				m_last_texco_num;
	int				m_last_attrib_num;

	RAS_IRasterizer::TexCoGen*		m_texco;
	RAS_IRasterizer::TexCoGen*		m_attrib;

	RAS_IRasterizer::TexCoGen		m_last_texco[RAS_MAX_TEXCO];
	RAS_IRasterizer::TexCoGen		m_last_attrib[RAS_MAX_ATTRIB];

	RAS_IRasterizer*				m_rasty;

	virtual void	EnableTextures(bool enable);
	virtual void	TexCoordPtr(class RAS_DisplayArray *area);
	virtual void	InitVboSlot(class RAS_DisplayArray* array, class RAS_MeshSlot *ms);

	virtual void	ClearVboSlot(class RAS_VboSlot *slot);


#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:RAS_StorageVA"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif //__KX_VERTEXBUFFEROBJECTSTORAGE