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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Jason Wilkins
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ghost/intern/GHOST_Context.h
 *  \ingroup GHOST
 * Declaration of GHOST_Context class.
 */

#ifndef __GHOST_CONTEXT_H__
#define __GHOST_CONTEXT_H__

#include "GHOST_Types.h"

#define glewGetContext() glewContext
#include <GL/glew.h>
extern "C" GLEWContext* glewContext;

#include <cstdlib> // for NULL



class GHOST_Context
{
public:
	/**
	 * Constructor.
	 */
	GHOST_Context()
		: m_glewContext(NULL)
	{}

	/**
	 * Destructor.
	 */
	virtual ~GHOST_Context()
	{
		delete m_glewContext;
	}

	/**
	 * Swaps front and back buffers of a window.
	 * \return  A boolean success indicator.
	 */
	virtual GHOST_TSuccess swapBuffers() = 0;

	/**
	 * Activates the drawing context of this window.
	 * \return  A boolean success indicator.
	 */
	virtual GHOST_TSuccess activateDrawingContext() = 0;

	/**
	 * Call immediately after new to initialize.  If this fails then immediately delete the object.
	 * \param stereoVisual		Stereo visual for quad buffered stereo.
	 * \param numOfAASamples	Number of samples used for AA (zero if no AA)
	 * \return Indication as to whether initialization has succeeded.
	 */
	virtual GHOST_TSuccess initializeDrawingContext(bool stereoVisual = false, GHOST_TUns16 numOfAASamples = 0) = 0;

	/**
	 * Checks if it is OK for a remove the native display
	 * \return Indication as to whether removal has succeeded.
	 */
	virtual GHOST_TSuccess releaseNativeHandles() = 0;

protected:
	void initContextGLEW();

	void activateGLEW() const
	{
		glewContext = m_glewContext;
	}

private:
	GLEWContext* m_glewContext;
};



GLenum glew_chk(GLenum error, const char* file, int line, const char* text);

#ifndef NDEBUG
#define GLEW_CHK(x) glew_chk((x), __FILE__, __LINE__, #x)
#else
#define GLEW_CHK(x) glew_chk(x)
#endif



class GHOST_PixelFormat {
public:
	enum swap_t { UNKNOWN, COPY, EXCHANGE, UNDEFINED };

	virtual bool   isUsable()     const = 0;
	virtual int    colorBits()    const = 0;
	virtual int    alphaBits()    const = 0;
	virtual int    depthBits()    const = 0;
	virtual int    stencilBits()  const = 0;
	virtual int    samples()      const = 0;
	virtual bool   sRGB()         const = 0;
	virtual swap_t swapMethod()   const = 0;

	int computeWeight() const;

	void print() const;
};



class GHOST_PixelFormatChooser {
public:
	int choosePixelFormat(GHOST_PixelFormatChooser& factory);

protected:
	virtual int                safeChoosePixelFormat() const = 0;
	virtual int                count()                 const = 0;
	virtual GHOST_PixelFormat* get(int i)              const = 0;
};



#endif // __GHOST_CONTEXT_H__
