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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Antony Riakiotakis.
 *
 * ***** END GPL LICENSE BLOCK *****
 */


/* Vertex stream implementation - this will abstract the various methods that will be used
 * to stream vertex data on the GPU. Depending on the undelying implementation,
 * vertex array objects, vertex arrays or vertex buffer objects will be used.
 * A Vertex stream only makes sense for one specific material, since it references
 * attribute bind positions for that material */

/* set one */
typedef struct GPUAttributeSlot {
	int shader_location; /* location of attribute for the */
	int data_type; /* data type, INT, FLOAT, SHORT etc */
} GPUAttributeSlot;

typedef struct GPUVertexStream {
	/* number of attributes in the stream */
	unsigned int num_attribs;

	/* bind buffers to their attribute slots */
	void (*bind)(void);

	/* unbind the buffers from their attribute slots */
	void (*unbind)(void);
} GPUVertexStream;

typedef struct GPUMaterialN {
	void (*updateGLstate)(void);
} GPUMaterialN;

void GPU_create_new_stream (GPUMaterialN *mat)
{

}

