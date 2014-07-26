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
 * The Original Code is: all of this file.
 *
 * Contributor(s): Jason Wilkins
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file glew-mx.c
 *  \ingroup glew-mx
 *
 * Support for GLEW Multimple rendering conteXts (MX)
 * Maintained by Jason Wilkins
 *
 * Different rendering contexts may have different entry points 
 * to extension functions of the same name.  So it can cause
 * problems if, for example, a second context uses a pointer to
 * say, glActiveTextureARB, that was queried from the first context.
 *
 * GLEW has basic support for multiple contexts by enabling GLEW_MX,
 * but it does not provide a full implementation.  This is because
 * there are too many questions about thread safety and memory
 * allocation that are up to the user of GLEW.
 *
 * This implementation is very basic.  It is not thread safe and it
 * uses malloc. For a single context the overhead should be
 * no more than using GLEW without GLEW_MX enabled.
 */

#ifdef GLEW_MX

#include "glew-mx.h"

#include <stdlib.h>

GLEWContext *_mxContext = NULL;

GLEWContext *mxGetContext(void)
{
	return _mxContext;
}

void mxSetContext(GLEWContext *ctx)
{
	_mxContext = ctx;
}

GLEWContext *mxCreateContext(void)
{
	return calloc(1, sizeof(GLEWContext));
}

void mxDestroyContext(GLEWContext *ctx)
{
	if (_mxContext == ctx)
		_mxContext = NULL;

	free(ctx);
}

#endif
