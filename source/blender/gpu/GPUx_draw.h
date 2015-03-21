#ifndef BLENDER_GL_DRAW_PRIMITIVES
#define BLENDER_GL_DRAW_PRIMITIVES

/* Stateless draw functions for lists of primitives.
 * Mike Erwin, Dec 2014 */

#include "GPUx_state.h"
#include "GPUx_vbo.h"
#include "GPUx_element.h"

/* pass ElementList = NULL to draw all vertices from VertexBuffer in order */
void draw_points(const CommonDrawState*, const PointDrawState*, const VertexBuffer*, const ElementList*);
void draw_lines(const CommonDrawState*, const LineDrawState*, const VertexBuffer*, const ElementList*);
void draw_triangles(const CommonDrawState*, const PolygonDrawState*, const VertexBuffer*, const ElementList*);

/* generic version uses ElementList's primitive type */
void draw_primitives(const CommonDrawState*, const void *primitive_state, const VertexBuffer*, const ElementList*);

#endif /* BLENDER_GL_DRAW_PRIMITIVES */
